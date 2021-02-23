#include <mapper/mapper.h>
#include <stdio.h>
#include <math.h>
#include <lo/lo.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

void handler(mpr_sig sig, mpr_sig_evt evt, mpr_id id, int len, mpr_type type,
             const void *val, mpr_time t)
{
    if (val)
    {
        // const char *name = mpr_obj_get_prop_as_str(sig, MPR_PROP_NAME, NULL);
        printf("ID: %d\tValue: %d\n", (int)id, *(int *)val);
        printf("Instances: %d\n\n", mpr_sig_get_num_inst(sig, MPR_STATUS_ACTIVE));
    }
    // received++;
}

int main(int argc, char **argv)
{
    int debug_test = 1; //! Remove when done

    printf("Begin Test\n\n");

    /* Create a source and a destination for signals to be added to */
    mpr_dev src = mpr_dev_new("src", 0);
    mpr_dev dst = mpr_dev_new("dst", 0);
    int ni = 5;
    mpr_sig sig_out = mpr_sig_new(src, MPR_DIR_OUT, "out-sig", 1, MPR_INT32, "none", 0, 0, &ni, 0, 0);
    mpr_sig sig_in = mpr_sig_new(dst, MPR_DIR_IN, "in-sig", 1, MPR_INT32, "none", 0, 0, &ni, handler, MPR_SIG_UPDATE);

    while (!mpr_dev_get_is_ready(src) && !mpr_dev_get_is_ready(dst))
    {
        mpr_dev_poll(src, 0);
        mpr_dev_poll(dst, 0);
    }

    //TODO: Remove debug statements.
    if (debug_test)
        printf("Devs and Sigs are Ready\n");

    mpr_map map = mpr_map_new(1, &sig_out, 1, &sig_in);

    mpr_obj_push(map);

    // printf("Push Maps\n");

    while (!mpr_map_get_is_ready(map))
    {
        mpr_dev_poll(src, 10);
        mpr_dev_poll(dst, 10);
    }

    //TODO: Remove debug statements.
    if (debug_test)
        printf("Map is Ready\n");

    // char *names[] = {"Thumb", "Index--", "Middle", "Ring", "Pinky---"}; // Five elements
    char *names[] = {"A", "BB", "CCC", "DDDD", "EEEEE"};                           // Five elements
    char *names2[] = {"AAAAAA", "BBBBBBB", "CCCCCCCC", "DDDDDDDDD", "EEEEEEEEEE"}; // Five elements

    int num_inst = sizeof(names) / sizeof(names[0]); // 5

    /* Reserve Named Instances on both ends of the map */

    mpr_sig_reserve_named_inst(sig_out, names, num_inst, 0);
    mpr_sig_reserve_named_inst(sig_in, names2, num_inst, 0);

    printf("\n----- GO! -----\n");

    int next_id = 1;

    while (1)
    {
        mpr_dev_poll(src, 500);
        mpr_dev_poll(dst, 500);

        // mpr_sig_set_named_inst_value(sig_out, names[count], 1, MPR_INT32, &count);

        // count++;
        // if (count > 4)
        // {
        //     count = 0;
        // }

        mpr_sig_set_value(sig_out, next_id, 1, MPR_INT32, &next_id);

        next_id++;

        if (next_id > 5)
        {
            next_id = 1;
        }
    }

    printf("\nEnd Test\n");

    return 0;
}
