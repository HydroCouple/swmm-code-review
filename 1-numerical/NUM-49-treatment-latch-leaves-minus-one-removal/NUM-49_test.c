/*
 * NUM-49: after a cyclic treatment latches the node's error flag, pollutants
 * evaluated later keep the "not computed" removal R = -1, and the apply pass
 * uses it: a concentration-type equation sets C = (1 - (-1)) * C = 2 * C.
 *
 * Junction J1 takes 1 cfs at TN = 10 mg/L and TSS = 10 mg/L. TN's removal is
 * written in terms of itself (R = 0.5*R_TN, a cycle); TSS has the independent
 * equation C = 0.9*TSS and is listed after TN.
 *
 * Correct behaviour: a cyclic dependency is ERROR 161 in the manual, so a run
 * that stops with error 161 is correct (that is CON-12's fix). If the run goes
 * on, treatment can only remove mass: TSS at J1 and at the outfall must not
 * exceed the 10 mg/L it arrives with, and quality continuity must hold.
 * Tolerances: TSS <= 10.5 mg/L (the bug gives 20) and |quality continuity
 * error| < 1 % (the bug gives -100 %). swmm_getMassBalErr() reports the
 * largest error over all pollutants.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

/* pollutant k (node result 6 + k) at node n in the last reporting period */
static double last_qual(const char *out, int n, int k)
{
    SMO_Handle h = NULL;
    float *vals = NULL;
    int len = 0, nper = 0;
    double c = -1.0;
    SMO_init(&h);
    if (SMO_open(h, out) == 0 && SMO_getTimes(h, SMO_numPeriods, &nper) == 0 &&
        SMO_getNodeResult(h, nper - 1, n, &vals, &len) == 0 && len > 6 + k)
        c = vals[6 + k];
    SMO_free((void **)&vals);
    SMO_close(&h);
    return c;
}

int main(void)
{
    double t = 0.0, tssJ1, tssO1;
    float runoffErr = 0.0f, flowErr = 0.0f, qualErr = 0.0f;
    int err, j1, o1;

    err = swmm_open("NUM-49_cyclic-then-ctype.inp", "NUM-49.rpt", "NUM-49.out");
    if (!err) err = swmm_start(1);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    o1 = swmm_getIndex(swmm_NODE, "O1");
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
    }
    swmm_end();
    swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_close();

    if (err == 161)
    {
        printf("Run stopped with error 161 (cyclic dependency in treatment functions)\n");
        printf("PASS: the cyclic treatment is reported as ERROR 161, so no concentration "
               "is built from an unset removal\n");
        return 0;
    }
    if (err)
    {
        printf("FAIL: the run stopped with unexpected error %d\n", err);
        return 1;
    }

    tssJ1 = last_qual("NUM-49.out", j1, 1);
    tssO1 = last_qual("NUM-49.out", o1, 1);
    printf("TSS inflow concentration      10.00 mg/L\n");
    printf("TSS at J1 (end of run)       %6.2f mg/L\n", tssJ1);
    printf("TSS at outfall O1            %6.2f mg/L\n", tssO1);
    printf("Quality continuity error     %6.2f %%\n", qualErr);

    if (!(tssJ1 <= 10.5) || !(tssO1 <= 10.5) || !(fabs(qualErr) < 1.0))
    {
        printf("FAIL: treatment raised TSS from 10 to %.2f mg/L (removal R = -1 applied) "
               "and created mass: quality continuity error %.2f %%\n", tssO1, qualErr);
        return 1;
    }
    printf("PASS: TSS is not raised above its inflow concentration and quality "
           "continuity holds\n");
    return 0;
}
