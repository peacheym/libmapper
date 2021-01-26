#include <mapper/mapper.h>
#include <stdio.h>
#include <math.h>
#include <lo/lo.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

mpr_dev parent_dev = 0;
mpr_sig parent_sig = 0;

int main(int argc, char **argv)
{
    printf("Beginning Test...\n\n");

    parent_dev = mpr_dev_new("Testparent_dev", 0);
    printf("!- Added Parent Device\n");
    
    // parent_sig = mpr_sig_new(parent_dev, MPR_DIR_ANY, "test sig", 1, MPR_INT32, "ts", 0, 100, 1, NULL, NULL);
    mpr_obj child_obj = mpr_obj_add_child((mpr_obj)parent_dev);
    mpr_dev child = mpr_dev_new_from_parent((mpr_obj)child_obj, "Test Child", 0);
    printf("!- Added Child Dev\n");

    // mpr_obj child_obj2 = mpr_obj_add_child((mpr_obj)parent_dev);
    // mpr_dev child2 = mpr_dev_new_from_parent(child_obj2, "Test Child", 0);

    // mpr_obj child_obj3 = mpr_obj_add_child((mpr_obj)parent_dev);
    // mpr_dev child3 = mpr_dev_new_from_parent(child_obj3, "Test Child", 0);

    printf("\nChild Types:\n");
    mpr_obj_list_child_types((mpr_obj)parent_dev);

    printf("\nEnding Test\n");
}
