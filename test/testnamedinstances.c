#include <mapper/mapper.h>
#include <stdio.h>
#include <math.h>
#include <lo/lo.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

int main(int argc, char **argv)
{
    printf("Begin Test\n\n");

    mpr_dev dev = mpr_dev_new("test-named-instances", 0);
    mpr_sig sig = mpr_sig_new(dev, MPR_DIR_ANY, "test sig", 1, MPR_INT32, "none", -1, 1, NULL, NULL, NULL);

    char* names[]={"Thumb", "Index", "Middle", "Ring", "Pinky"}; // Five elements
    int num_inst = sizeof(names)/sizeof(names[0]);

    mpr_sig_reserve_named_inst(sig, names, num_inst, NULL, NULL);

    printf("\nEnd Test\n");

    return 0;
}
