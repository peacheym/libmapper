#include <mapper/mapper.h>
#include <stdio.h>
#include <math.h>
#include <lo/lo.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

/**\
 * 
 * 
 * NOTE: This test is a BAREBONES tests and does not yet cover full functionality.
 * 
 * TODO: Update this test to ensure full coverage.
 * 
 * 
*/

int main(int argc, char **argv)
{
    printf("Beginning Test...\n");

    mpr_obj parent = (mpr_obj)mpr_dev_new("test-parent", 0);
    
    mpr_obj child = mpr_obj_add_child(parent, "test-child", 0);
    mpr_obj child2 = mpr_obj_add_child(parent, "test-another-child", 0);

    mpr_obj_print(child, 0); // To remove unused variable error.
    mpr_obj_print(child2, 0); // To remove unused variable error.

    printf("Ending Test!\n");
}
