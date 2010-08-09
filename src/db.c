
#include <string.h>

#include "mapper_internal.h"

/*! Initialization of the global list of local devices. */
mapper_db_device g_db_registered_devices = NULL;

/*! Update information about a given device record based on message
 *  parameters. */
static void update_device_record_params(mapper_db_device reg,
                                        const char *name,
                                        mapper_message_t *params)
{
    lo_arg **a_host = mapper_msg_get_param(params, AT_IP);
    const char *t_host = mapper_msg_get_type(params, AT_IP);

    lo_arg **a_port = mapper_msg_get_param(params, AT_PORT);
    const char *t_port = mapper_msg_get_type(params, AT_PORT);

    lo_arg **a_canAlias = mapper_msg_get_param(params, AT_CANALIAS);
    const char *t_canAlias = mapper_msg_get_type(params, AT_CANALIAS);

    if (!reg->name || strcmp(reg->name, name)) {
        reg->name = (char*) realloc(reg->name, strlen(name));
        strcpy(reg->name, name);
    }

    if (a_host && t_host[0]=='s') {
        reg->host = (char*) realloc(reg->host, strlen(&(*a_host)->s));
        strcpy(reg->host, &(*a_host)->s);
    }

    if (a_port && t_port[0]=='i')
        reg->port = (*a_port)->i;

    if (a_canAlias && t_canAlias[0]=='s')
        reg->canAlias = strcmp("no", &(*a_canAlias)->s)!=0;
}

int mapper_db_add_or_update_params(const char *name,
                                   mapper_message_t *params)
{
    mapper_db_device reg = mapper_db_find_device_by_name(name);
    int rc = 0;

    if (!reg) {
        reg = (mapper_db_device)malloc(sizeof(*reg));
        memset(reg, 0, sizeof(*reg));
        rc = 1;

        reg->next = g_db_registered_devices;
        g_db_registered_devices = reg;
    }

    if (reg)
        update_device_record_params(reg, name, params);

    return rc;
}

mapper_db_device mapper_db_find_device_by_name(const char *name)
{
    mapper_db_device reg = g_db_registered_devices;
    while (reg) {
        if (strcmp(reg->name, name)==0)
            return reg;
        reg = reg->next;
    }
    return 0;
}

void mapper_db_dump()
{
    mapper_db_device reg = g_db_registered_devices;
    trace("Registered devices:\n");
    while (reg) {
        trace("  name=%s, host=%s, port=%d, canAlias=%d\n",
              reg->name, reg->host,
              reg->port, reg->canAlias);
        reg = reg->next;
    }
}
