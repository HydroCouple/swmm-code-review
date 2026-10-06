/*
 * CON-11: in a treatment expression, a pollutant name stands for the node's
 * concentration before treatment in a concentration-type (C =) equation and
 * for the inflow concentration in a removal-type (R =) equation (reference
 * manual Vol. III, section 5.4.2). getVariableValue() instead picks between
 * the two by the treatment type of the REFERENCED pollutant, and a pollutant
 * with no equation counts as removal-type (calloc'd treatType 0 = REMOVAL).
 *
 * Storage unit SU1 (1000 ft2, 4 ft deep at the start, no outflow) holds
 * TSS = 100 mg/L and TP = 10 mg/L.
 *   CON-11_ctype-untreated-ref.inp  TP C = 0.05*TSS, TSS untreated, no inflow
 *   CON-11_ctype-rtype-ref.inp      TP C = 0.05*TSS, TSS R = 0.5, no inflow
 *       TSS is the pond concentration 100, so TP = MIN(10, 0.05*100) = 5.
 *   CON-11_rtype-ctype-ref.inp      TP R = 0.005*TSS, TSS C = TSS, and 1 cfs
 *       of inflow carrying TP = 10 mg/L and no TSS. TSS is the inflow
 *       concentration 0, so TP's removal is 0 and TP stays at 10.
 *
 * Correct behaviour: TP in SU1 at the end of the 1-hour run is 5, 5 and
 * 10 mg/L. Tolerance 0.1 mg/L; the bug gives 0, 0 and about 7.4.
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

static const struct { const char *deck, *tss, *tp; double expect; } cases[] = {
    {"CON-11_ctype-untreated-ref.inp", "(none)",  "C = 0.05*TSS",  5.0},
    {"CON-11_ctype-rtype-ref.inp",     "R = 0.5", "C = 0.05*TSS",  5.0},
    {"CON-11_rtype-ctype-ref.inp",     "C = TSS", "R = 0.005*TSS", 10.0},
};

int main(void)
{
    int i, nbad = 0;

    printf("Deck                            TSS eqn  TP eqn         TP expected  TP computed\n");
    for (i = 0; i < 3; i++)
    {
        double t = 0.0, tp = -1.0;
        int err, su1;
        err = swmm_open(cases[i].deck, "CON-11.rpt", "CON-11.out");
        if (!err) err = swmm_start(1);
        su1 = swmm_getIndex(swmm_NODE, "SU1");
        while (!err)
        {
            err = swmm_step(&t);
            if (t <= 0.0) break;
        }
        swmm_end();
        swmm_close();
        if (!err) tp = last_qual("CON-11.out", su1, 1);
        if (err || !(fabs(tp - cases[i].expect) < 0.1)) nbad++;
        printf("%-31s %-8s %-14s %8.2f   %8.2f%s\n", cases[i].deck, cases[i].tss,
               cases[i].tp, cases[i].expect, tp,
               err ? "  <-- run failed" :
               !(fabs(tp - cases[i].expect) < 0.1) ? "  <-- wrong" : "");
    }
    if (nbad)
    {
        printf("FAIL: TP in SU1 is wrong in %d of 3 decks: TSS in TP's equation is read "
               "by TSS's treatment type, not TP's\n", nbad);
        return 1;
    }
    printf("PASS: TSS in TP's equation is the pond concentration in a C = equation and "
           "the inflow concentration in an R = equation\n");
    return 0;
}
