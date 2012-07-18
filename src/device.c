
#include <lo/lo.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>

#include "mapper_internal.h"
#include "types_internal.h"
#include "config.h"
#include <mapper/mapper.h>

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

/*! Internal function to get the current time. */
static double get_current_time()
{
#ifdef HAVE_GETTIMEOFDAY
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double) tv.tv_sec + tv.tv_usec / 1000000.0;
#else
#error No timing method known on this platform.
#endif
}

//! Allocate and initialize a mapper device.
mapper_device mdev_new(const char *name_prefix, int port,
                       mapper_admin admin)
{
    mapper_device md =
        (mapper_device) calloc(1, sizeof(struct _mapper_device));
    md->name_prefix = strdup(name_prefix);

    if (admin) {
        md->admin = admin;
        md->own_admin = 0;
    }
    else {
        md->admin = mapper_admin_new(0, 0, 0);
        md->own_admin = 1;
    }

    if (!md->admin) {
        mdev_free(md);
        return NULL;
    }

    mapper_admin_add_device(md->admin, md, name_prefix, port);

    md->routers = 0;
    md->instance_id_map = 0;
    md->extra = table_new();
    return md;
}

//! Free resources used by a mapper device.
void mdev_free(mapper_device md)
{
    int i;
    if (md) {
        if (md->admin && md->own_admin)
            mapper_admin_free(md->admin);
        for (i = 0; i < md->n_inputs; i++)
            msig_free(md->inputs[i]);
        if (md->inputs)
            free(md->inputs);
        for (i = 0; i < md->n_outputs; i++)
            msig_free(md->outputs[i]);
        if (md->outputs)
            free(md->outputs);
        while (md->instance_id_map)
            mdev_remove_instance_id_map(md, md->instance_id_map->local);
        while (md->routers)
            mdev_remove_router(md, md->routers);
        if (md->extra)
            table_free(md->extra, 1);
        free(md);
    }
}

#ifdef __GNUC__
// when gcc inlines this with O2 or O3, it causes a crash. bug?
__attribute__ ((noinline))
#endif
static void grow_ptr_array(void **array, int length, int *size)
{
    if (*size < length && !*size)
        (*size)++;
    while (*size < length)
        (*size) *= 2;
    *array = realloc(*array, sizeof(void *) * (*size));
}

static void mdev_increment_version(mapper_device md)
{
    md->version ++;
    if (md->admin->registered) {
        md->flags |= FLAGS_ATTRIBS_CHANGED;
    }
}

static int handler_signal(const char *path, const char *types,
                          lo_arg **argv, int argc, lo_message msg,
                          void *user_data)
{
    mapper_signal sig = (mapper_signal) user_data;
    mapper_device md = sig->device;
    if (!md) {
        trace("error, sig->device==0\n");
        return 0;
    }

    // Default to updating first instance
    if (!sig || !sig->instances)
        return 0;
    mapper_signal_instance si = sig->instances;

    if (types[0] == LO_NIL) {
        si->history.position = -1;
    }
    else {
        /* This is cheating a bit since we know that the arguments pointed
         * to by argv are layed out sequentially in memory.  It's not
         * clear if liblo's semantics guarantee it, but known to be true
         * on all platforms. */
        si->history.position = (si->history.position + 1)
                                % si->history.size;
        memcpy(msig_history_value_pointer(si->history),
               argv[0], msig_vector_bytes(sig));
    }

    if (sig->handler)
        sig->handler(sig, si->id_map->local, &sig->props,
                     &si->history.timetag[si->history.position], types[0] == LO_NIL ? 0 :
                     si->history.value + msig_vector_bytes(sig) * si->history.position);

    return 0;
}

static int handler_signal_instance(const char *path, const char *types,
                                   lo_arg **argv, int argc, lo_message msg,
                                   void *user_data)
{
    mapper_signal sig = (mapper_signal) user_data;
    mapper_device md = sig->device;

    if (!md) {
        trace("error, sig->device==0\n");
        return 0;
    }
    if (argc < 3)
        return 0;

    int group_id = argv[0]->i32;
    int instance_id = argv[1]->i32;
    int is_new = (types[2] == LO_TRUE);

    mapper_signal_instance si = 0;

    // Don't activate instance just to release it again
    if (types[2] == LO_NIL || types[2] == LO_FALSE) {
        if (!msig_find_instance_with_id_map(sig, group_id, instance_id))
            return 0;
    }

    si = msig_get_instance_with_id_map(sig, group_id, instance_id, is_new);
    if (!si && is_new) {
        if (sig->instance_overflow_handler) {
            sig->instance_overflow_handler(sig, group_id, instance_id);
            // try again
            si = msig_get_instance_with_id_map(sig, group_id, instance_id, is_new);
        }
    }
    if (!si) {
        trace("no instances available for group=%ld, id=%ld\n",
              (long)group_id, (long)instance_id);
        return 0;
    }

    if (types[2] == LO_TRUE) {
        if (sig->instance_management_handler)
            sig->instance_management_handler(sig, si->id_map->local, IN_NEW);
        return 0;
    }
    else if (types[2] == LO_FALSE) {
        if (sig->instance_management_handler)
            sig->instance_management_handler(sig, si->id_map->local, IN_REQUEST_KILL);
        return 0;
    }

    if (types[2] != LO_NIL) {
        /* This is cheating a bit since we know that the arguments pointed
         * to by argv are layed out sequentially in memory.  It's not
         * clear if liblo's semantics guarantee it, but known to be true
         * on all platforms. */
        si->history.position = (si->history.position + 1) % si->history.size;
        memcpy(msig_history_value_pointer(si->history), argv[2], msig_vector_bytes(sig));
    }

    if (sig->handler) {
        sig->handler(sig, si->id_map->local, &sig->props,
                     &si->history.timetag[si->history.position], types[2] == LO_NIL ? 0 :
                     si->history.value + msig_vector_bytes(sig) * si->history.position);
    }
    if (types[2] == LO_NIL) {
        msig_release_instance_internal(si);
    }
    return 0;
}

static int handler_query(const char *path, const char *types,
                         lo_arg **argv, int argc, lo_message msg,
                         void *user_data)
{
    mapper_signal sig = (mapper_signal) user_data;
    mapper_device md = sig->device;

    if (!md) {
        trace("error, sig->device==0\n");
        return 0;
    }

    if (!argc)
        return 0;
    else if (types[0] != 's' && types[0] != 'S')
        return 0;

    int i;
    lo_message m;

    mapper_signal_instance si = sig->instances;
    if (!si) {
        // If there are no active instances, send null response
        m = lo_message_new();
        lo_message_add_nil(m);
        lo_send_message(lo_message_get_source(msg), &argv[0]->s, m);
        lo_message_free(m);
    }
    while (si) {
        m = lo_message_new();
        if (!m)
            return 0;
        if (si->signal->props.num_instances > 1)
            lo_message_add_int32(m, (long)si->id_map->local);
        if (si->history.position != -1) {
            if (si->history.type == 'f') {
                float *v = msig_history_value_pointer(si->history);
                for (i = 0; i < si->history.length; i++)
                    lo_message_add_float(m, v[i]);
            }
            else if (si->history.type == 'i') {
                int *v = msig_history_value_pointer(si->history);
                for (i = 0; i < si->history.length; i++)
                    lo_message_add_int32(m, v[i]);
            }
            else if (si->history.type == 'd') {
                double *v = msig_history_value_pointer(si->history);
                for (i = 0; i < si->history.length; i++)
                    lo_message_add_double(m, v[i]);
            }
        }
        else {
            lo_message_add_nil(m);
        }
        lo_send_message(lo_message_get_source(msg), &argv[0]->s, m);
        lo_message_free(m);
        si = si->next;
    }
    return 0;
}

static int handler_query_response(const char *path, const char *types,
                                  lo_arg **argv, int argc, lo_message msg,
                                  void *user_data)
{
    // TODO: integrate instances into queries
    mapper_signal sig = (mapper_signal) user_data;
    mapper_device md = sig->device;

    if (!md) {
        trace("error, sig->device==0\n");
        return 0;
    }
    if (types[0] == LO_NIL && sig->handler) {
        sig->handler(sig, 0, &sig->props, 0, 0);
        return 0;
    }

    mapper_signal_value_t value[sig->props.length];
    int i;
    if (sig->props.type == 'i') {
        for (i = 0; i < sig->props.length && i < argc; i++) {
            if (types[i] == LO_INT32)
                value[i].i32 = argv[i]->i32;
            else if (types[i] == LO_FLOAT)
                value[i].i32 = (int)argv[i]->f;
        }
    }
    else if (sig->props.type == 'f') {
        for (i = 0; i < sig->props.length && i < argc; i++) {
            if (types[i] == LO_INT32)
                value[i].f = (float)argv[i]->i32;
            else if (types[i] == LO_FLOAT)
                value[i].f = argv[i]->f;
        }
    }

    if (sig->handler)
        sig->handler(sig, 0, &sig->props, 0, value);

    return 0;
}

// Add an input signal to a mapper device.
mapper_signal mdev_add_input(mapper_device md, const char *name, int length,
                             char type, const char *unit,
                             void *minimum, void *maximum,
                             mapper_signal_handler *handler, void *user_data)
{
    if (mdev_get_input_by_name(md, name, 0))
        return 0;
    char *type_string = 0, *signal_get = 0;
    mapper_signal sig = msig_new(name, length, type, 0, unit, minimum,
                                 maximum, handler, user_data);
    if (!sig)
        return 0;
    md->n_inputs++;
    grow_ptr_array((void **) &md->inputs, md->n_inputs,
                   &md->n_alloc_inputs);

    mdev_increment_version(md);

    md->inputs[md->n_inputs - 1] = sig;
    sig->device = md;
    if (md->admin->name)
        sig->props.device_name = md->admin->name;

    if (!md->server)
        mdev_start_server(md);
    else {
        type_string = (char*) realloc(type_string, sig->props.length + 3);
        type_string[0] = type_string[1] = 'i';
        memset(type_string + 2, sig->props.type, sig->props.length);
        type_string[sig->props.length + 2] = 0;
        lo_server_add_method(md->server,
                             sig->props.name,
                             type_string + 2,
                             handler_signal, (void *) (sig));
        lo_server_add_method(md->server,
                             sig->props.name,
                             "N",
                             handler_signal, (void *) (sig));
        lo_server_add_method(md->server,
                             sig->props.name,
                             type_string,
                             handler_signal_instance, (void *) (sig));
        lo_server_add_method(md->server,
                             sig->props.name,
                             "iiT",
                             handler_signal_instance, (void *) (sig));
        lo_server_add_method(md->server,
                             sig->props.name,
                             "iiF",
                             handler_signal_instance, (void *) (sig));
        lo_server_add_method(md->server,
                             sig->props.name,
                             "iiN",
                             handler_signal_instance, (void *) (sig));
        int len = strlen(sig->props.name) + 5;
        signal_get = (char*) realloc(signal_get, len);
        snprintf(signal_get, len, "%s%s", sig->props.name, "/get");
        lo_server_add_method(md->server,
                             signal_get,
                             "s",
                             handler_query, (void *) (sig));
        free(type_string);
        free(signal_get);
    }

    return sig;
}

// Add an output signal to a mapper device.
mapper_signal mdev_add_output(mapper_device md, const char *name, int length,
                              char type, const char *unit, void *minimum, void *maximum)
{
    if (mdev_get_output_by_name(md, name, 0))
        return 0;
    mapper_signal sig = msig_new(name, length, type, 1, unit, minimum,
                                 maximum, 0, 0);
    if (!sig)
        return 0;
    md->n_outputs++;
    grow_ptr_array((void **) &md->outputs, md->n_outputs,
                   &md->n_alloc_outputs);

    mdev_increment_version(md);

    md->outputs[md->n_outputs - 1] = sig;
    sig->device = md;
    if (md->admin->name)
        sig->props.device_name = md->admin->name;
    return sig;
}

void mdev_add_signal_query_response_callback(mapper_device md, mapper_signal sig)
{
    if (!sig->props.is_output)
        return;
    char *path = 0;
    int len;
    if (!md->server)
        mdev_start_server(md);
    else {
        len = (int) strlen(sig->props.name) + 5;
        path = (char*) realloc(path, len);
        snprintf(path, len, "%s%s", sig->props.name, "/got");
        lo_server_add_method(md->server,
                             path,
                             NULL,
                             handler_query_response, (void *) (sig));
        free(path);
        md->n_query_inputs ++;
    }
}

void mdev_remove_signal_query_response_callback(mapper_device md, mapper_signal sig)
{
    char *path = 0;
    int len, i;
    if (!md || !sig)
        return;
    for (i=0; i<md->n_outputs; i++) {
        if (md->outputs[i] == sig)
            break;
    }
    if (i==md->n_outputs)
        return;
    len = (int) strlen(sig->props.name) + 5;
    path = (char*) realloc(path, len);
    snprintf(path, len, "%s%s", sig->props.name, "/got");
    lo_server_del_method(md->server, path, NULL);
    md->n_query_inputs --;
}

void mdev_remove_input(mapper_device md, mapper_signal sig)
{
    int i, n;
    for (i=0; i<md->n_inputs; i++) {
        if (md->inputs[i] == sig)
            break;
    }
    if (i==md->n_inputs)
        return;

    for (n=i; n<(md->n_inputs-1); n++) {
        md->inputs[n] = md->inputs[n+1];
    }
    if (md->server) {
        char *type_string = (char*) malloc(sig->props.length + 3);
        type_string[0] = type_string[1] = 'i';
        memset(type_string + 2, sig->props.type, sig->props.length);
        type_string[sig->props.length + 2] = 0;
        lo_server_del_method(md->server, sig->props.name, type_string);
        lo_server_del_method(md->server, sig->props.name, type_string + 2);
        lo_server_del_method(md->server, sig->props.name, "N");
        lo_server_del_method(md->server, sig->props.name, "iiT");
        lo_server_del_method(md->server, sig->props.name, "iiN");
        free(type_string);
        int len = strlen(sig->props.name) + 5;
        char *signal_get = (char*) malloc(len);
        strncpy(signal_get, sig->props.name, len);
        strncat(signal_get, "/get", len);
        lo_server_del_method(md->server, signal_get, NULL);
        free(signal_get);
    }
    md->n_inputs --;
    mdev_increment_version(md);
    msig_free(sig);
}

void mdev_remove_output(mapper_device md, mapper_signal sig)
{
    int i, n;
    for (i=0; i<md->n_outputs; i++) {
        if (md->outputs[i] == sig)
            break;
    }
    if (i==md->n_outputs)
        return;

    for (n=i; n<(md->n_outputs-1); n++) {
        md->outputs[n] = md->outputs[n+1];
    }
    if (sig->handler && md->server) {
        int len = strlen(sig->props.name) + 5;
        char *path = (char*) malloc(len);
        strncpy(path, sig->props.name, len);
        strncat(path, "/got", len);
        lo_server_del_method(md->server, path, NULL);
        free(path);
    }
    md->n_outputs --;
    mdev_increment_version(md);
    msig_free(sig);
}

int mdev_num_inputs(mapper_device md)
{
    return md->n_inputs;
}

int mdev_num_outputs(mapper_device md)
{
    return md->n_outputs;
}

int mdev_num_links(mapper_device md)
{
    return md->n_links;
}

int mdev_num_connections(mapper_device md)
{
    return md->n_connections;
}

mapper_signal *mdev_get_inputs(mapper_device md)
{
    return md->inputs;
}

mapper_signal *mdev_get_outputs(mapper_device md)
{
    return md->outputs;
}

mapper_signal mdev_get_input_by_name(mapper_device md, const char *name,
                                     int *index)
{
    int i;
    int slash = name[0]=='/' ? 1 : 0;
    for (i=0; i<md->n_inputs; i++)
    {
        if (strcmp(md->inputs[i]->props.name + 1,
                   name + slash)==0)
        {
            if (index)
                *index = i;
            return md->inputs[i];
        }
    }
    return 0;
}

mapper_signal mdev_get_output_by_name(mapper_device md, const char *name,
                                      int *index)
{
    int i;
    int slash = name[0]=='/' ? 1 : 0;
    for (i=0; i<md->n_outputs; i++)
    {
        if (strcmp(md->outputs[i]->props.name + 1,
                   name + slash)==0)
        {
            if (index)
                *index = i;
            return md->outputs[i];
        }
    }
    return 0;
}

mapper_signal mdev_get_input_by_index(mapper_device md, int index)
{
    if (index >= 0 && index < md->n_inputs)
        return md->inputs[index];
    return 0;
}

mapper_signal mdev_get_output_by_index(mapper_device md, int index)
{
    if (index >= 0 && index < md->n_outputs)
        return md->outputs[index];
    return 0;
}

int mdev_poll(mapper_device md, int block_ms)
{
    int admin_count = mapper_admin_poll(md->admin);
    int count = 0;

    if (md->server) {

        /* If a timeout is specified, loop until the time is up. */
        if (block_ms)
        {
            double then = get_current_time();
            int left_ms = block_ms;
            while (left_ms > 0)
            {
                if (lo_server_recv_noblock(md->server, left_ms))
                    count++;
                double elapsed = get_current_time() - then;
                left_ms = block_ms - (int)(elapsed*1000);
            }
        }

        /* When done, or if non-blocking, check for remaining messages
         * up to a proportion of the number of input
         * signals. Arbitrarily choosing 1 for now, since we don't
         * support "combining" multiple incoming streams, so there's
         * no point.  Perhaps if this is supported in the future it
         * can be a heuristic based on a recent number of messages per
         * channel per poll. */
        while (count < (md->n_inputs + md->n_query_inputs)*1
               && lo_server_recv_noblock(md->server, 0))
            count++;
    }
    else if (block_ms) {
#ifdef WIN32
        Sleep(block_ms);
#else
        usleep(block_ms * 1000);
#endif
    }

    return admin_count + count;
}

int mdev_route_query(mapper_device md, mapper_signal sig)
{
    int count = 0;
    mapper_router r = md->routers;
    while (r) {
        count += mapper_router_send_query(r, sig);
        r = r->next;
    }
    return count;
}

void mdev_add_router(mapper_device md, mapper_router rt)
{
    mapper_router *r = &md->routers;
    rt->next = *r;
    *r = rt;
    md->n_links++;
}

void mdev_remove_router(mapper_device md, mapper_router rt)
{
    // first remove connections
    mapper_signal_connection sc = rt->connections;
    while (sc) {
        mapper_connection c = sc->connection, temp;
        while (c) {
            temp = c->next;
            mapper_router_remove_connection(rt, c);
            c = temp;
        }
        sc = sc->next;
    }

    int i;
    for (i=0; i<rt->props.num_scopes; i++) {
        // For each scope in this router...
        mapper_router temp = md->routers;
        int safe = 1;
        while (temp) {
            if (mapper_router_in_scope(temp, rt->props.scope_hashes[i])) {
                safe = 1;
                break;
            }
            temp = temp->next;
        }
        if (!safe) {
            /* scope is not used by any other routers, safe to clear
             * corresponding instances in instance id map. */
            mapper_instance_id_map id_map = md->instance_id_map;
            while (id_map) {
                if (id_map->group == rt->props.scope_hashes[i]) {
                    mdev_remove_instance_id_map(md, id_map->local);
                }
                id_map = id_map->next;
            }
        }
        free(rt->props.scope_names[i]);
    }
    free(rt->props.scope_names);
    free(rt->props.scope_hashes);

    // remove router
    mapper_router *r = &md->routers;
    while (*r) {
        if (*r == rt) {
            *r = rt->next;
            free(rt);
            break;
        }
        r = &(*r)->next;
    }

    md->n_links--;
}

mapper_instance_id_map mdev_add_instance_id_map(mapper_device device, int local_id,
                                                int group_id, int remote_id)
{
    mapper_instance_id_map id_map;
    id_map = (mapper_instance_id_map)calloc(1, sizeof(struct _mapper_instance_id_map));
    id_map->local = local_id;
    id_map->group = group_id;
    id_map->remote = remote_id;
    id_map->next = device->instance_id_map;
    device->instance_id_map = id_map;
    return id_map;
}

void mdev_remove_instance_id_map(mapper_device device, int local_id)
{
    mapper_instance_id_map temp, *id_map = &device->instance_id_map;
    while (*id_map) {
        if ((*id_map)->local == local_id) {
            temp = *id_map;
            *id_map = (*id_map)->next;
            free(temp);
            break;
        }
        id_map = &(*id_map)->next;
    }
}

mapper_instance_id_map mdev_set_instance_id_map(mapper_device device, int local_id,
                                                int group_id, int remote_id)
{
    mapper_instance_id_map id_map = device->instance_id_map;
    while (id_map) {
        if (id_map->local == local_id) {
            id_map->group = group_id;
            id_map->remote = remote_id;
            return id_map;
        }
        id_map = id_map->next;
    }

    // map not found, create it
    return mdev_add_instance_id_map(device, local_id, group_id, remote_id);
}

mapper_instance_id_map mdev_find_instance_id_map_by_local(mapper_device device,
                                                          int local_id)
{
    mapper_instance_id_map id_map = device->instance_id_map;

    while (id_map) {
        if (id_map->local == local_id)
            return id_map;
        id_map = id_map->next;
    }
    return 0;
}

mapper_instance_id_map mdev_find_instance_id_map_by_remote(mapper_device device,
                                                           int group_id, int remote_id)
{
    mapper_instance_id_map id_map = device->instance_id_map;
    while (id_map) {
        if ((id_map->group == group_id) && (id_map->remote == remote_id))
            return id_map;
        id_map = id_map->next;
    }
    return 0;
}

/* Note: any call to liblo where get_liblo_error will be called
 * afterwards must lock this mutex, otherwise there is a race
 * condition on receiving this information.  Could be fixed by the
 * liblo error handler having a user context pointer. */
static int liblo_error_num = 0;
static void liblo_error_handler(int num, const char *msg, const char *path)
{
    liblo_error_num = num;
    if (num == LO_NOPORT) {
        trace("liblo could not start a server because port unavailable\n");
    } else
        fprintf(stderr, "[libmapper] liblo server error %d in path %s: %s\n",
               num, path, msg);
}

void mdev_start_server(mapper_device md)
{
    if (!md->server) {
        int i;
        char port[16], *pport = port, *type = 0, *path = 0;

        if (md->admin->port)
            sprintf(port, "%d", md->admin->port);
        else
            pport = 0;

        while (!(md->server = lo_server_new(pport, liblo_error_handler))) {
            pport = 0;
        }

        md->admin->port = lo_server_get_port(md->server);
        trace("bound to port %i\n", md->admin->port);

        for (i = 0; i < md->n_inputs; i++) {
            type = (char*) realloc(type, md->inputs[i]->props.length + 3);
            type[0] = type[1] = 'i';
            memset(type + 2, md->inputs[i]->props.type,
                   md->inputs[i]->props.length);
            type[md->inputs[i]->props.length + 2] = 0;
            lo_server_add_method(md->server,
                                 md->inputs[i]->props.name,
                                 type + 2,
                                 handler_signal, (void *) (md->inputs[i]));
            lo_server_add_method(md->server,
                                 md->inputs[i]->props.name,
                                 "N",
                                 handler_signal, (void *) (md->inputs[i]));
            lo_server_add_method(md->server,
                                 md->inputs[i]->props.name,
                                 type,
                                 handler_signal_instance, (void *) (md->inputs[i]));
            lo_server_add_method(md->server,
                                 md->inputs[i]->props.name,
                                 "iiT",
                                 handler_signal_instance, (void *) (md->inputs[i]));
            lo_server_add_method(md->server,
                                 md->inputs[i]->props.name,
                                 "iiF",
                                 handler_signal_instance, (void *) (md->inputs[i]));
            lo_server_add_method(md->server,
                                 md->inputs[i]->props.name,
                                 "iiN",
                                 handler_signal_instance, (void *) (md->inputs[i]));
            int len = (int) strlen(md->inputs[i]->props.name) + 5;
            path = (char*) realloc(path, len);
            snprintf(path, len, "%s%s", md->inputs[i]->props.name, "/get");
            lo_server_add_method(md->server,
                                 path,
                                 "s",
                                 handler_query, (void *) (md->inputs[i]));
        }
        for (i = 0; i < md->n_outputs; i++) {
            if (!md->outputs[i]->handler)
                continue;
            int len = (int) strlen(md->outputs[i]->props.name) + 5;
            path = (char*) realloc(path, len);
            snprintf(path, len, "%s%s", md->outputs[i]->props.name, "/got");
            lo_server_add_method(md->server,
                                 path,
                                 NULL,
                                 handler_query_response, (void *) (md->outputs[i]));
            md->n_query_inputs ++;
        }
        free(type);
        free(path);
    }
}

const char *mdev_name(mapper_device md)
{
    /* Hand this off to the admin struct, where the name may be
     * cached. However: manually checking ordinal.locked here so that
     * we can safely trace bad usage when mapper_admin_full_name is
     * called inappropriately. */
    if (md->admin->registered)
        return mapper_admin_name(md->admin);
    else
        return 0;
}

unsigned int mdev_id(mapper_device md)
{
    if (md->admin->registered)
        return md->admin->name_hash;
    else
        return 0;
}

unsigned int mdev_port(mapper_device md)
{
    if (md->admin->registered)
        return md->admin->port;
    else
        return 0;
}

const struct in_addr *mdev_ip4(mapper_device md)
{
    if (md->admin->registered)
        return &md->admin->interface_ip;
    else
        return 0;
}

const char *mdev_interface(mapper_device md)
{
    return md->admin->interface_name;
}

unsigned int mdev_ordinal(mapper_device md)
{
    if (md->admin->registered)
        return md->admin->ordinal.value;
    else
        return 0;
}

int mdev_ready(mapper_device device)
{
    if (!device)
        return 0;

    return device->admin->registered;
}

void mdev_set_property(mapper_device dev, const char *property,
                       lo_type type, lo_arg *value)
{
    mapper_table_add_or_update_osc_value(dev->extra,
                                         property, type, value);
}

void mdev_remove_property(mapper_device dev, const char *property)
{
    table_remove_key(dev->extra, property, 1);
}

void mdev_set_timetag(mapper_device dev, mapper_timetag_t timetag)
{
    // To be implemented.
}

void mdev_set_queue_size(mapper_signal sig, int queue_size)
{
    // To be implemented.
}
