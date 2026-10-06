/*
 * CON-10: a negative lateral inflow (a withdrawal) removes water from a node
 * but no pollutant mass, so the node's concentration is inflated.
 *
 * Quality routing mixes the mass of all positive lateral sources with the
 * node's hydraulic inflow, which contains the NET lateral flow. A withdrawal
 * therefore shrinks the dilution flow without taking any mass out, and only
 * when the net lateral flow is negative does removeOutflows() book mass
 * leaving with it. Under KINWAVE the net negative case is worse: the mixing
 * flow is the link inflow minus the withdrawal while the withdrawal is still
 * booked at the result, so mass is created.
 *
 * Correct behaviour: every source in the three decks carries 100 mg/L TSS
 * and nothing reacts, so a withdrawal (water leaving at the node's mixed
 * concentration) cannot change any concentration: J1 and the outfall must
 * end at 100 mg/L, and the quality continuity error must be small.
 * Tolerances: |C - 100| < 1 mg/L (the bug gives 200 mg/L) and
 * |quality continuity error| < 1 % (the bug gives NaN in the KW case).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

static const char *decks[] = {"CON-10_withdrawal-dw.inp", "CON-10_withdrawal-kw.inp",
                              "CON-10_net-withdrawal-kw.inp"};

/* TSS (node result 6, the first pollutant) at node n in the last period */
static double last_tss(const char *out, int n)
{
    SMO_Handle h = NULL;
    float *vals = NULL;
    int len = 0, nper = 0;
    double c = -1.0;
    SMO_init(&h);
    if (SMO_open(h, out) == 0 && SMO_getTimes(h, SMO_numPeriods, &nper) == 0 &&
        SMO_getNodeResult(h, nper - 1, n, &vals, &len) == 0 && len > 6)
        c = vals[6];
    SMO_free((void **)&vals);
    SMO_close(&h);
    return c;
}

int main(void)
{
    int i, err, nbad = 0;
    printf("Deck                            Routing     J1 TSS   O1 TSS  Quality continuity\n");
    printf("                                            (mg/L)   (mg/L)  error (%%)\n");
    for (i = 0; i < 3; i++)
    {
        double t = 0.0, cJ1, cO1;
        float runoffErr = 0.0f, flowErr = 0.0f, qualErr = 0.0f;
        int j1, o1, bad;
        err = swmm_open(decks[i], "CON-10.rpt", "CON-10.out");
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
        cJ1 = last_tss("CON-10.out", j1);
        cO1 = last_tss("CON-10.out", o1);
        bad = err || !(fabs(cJ1 - 100.0) < 1.0) || !(fabs(cO1 - 100.0) < 1.0) ||
              !(fabs(qualErr) < 1.0);
        if (bad) nbad++;
        printf("%-30s  %-8s  %7.2f  %7.2f  %18.3f%s\n", decks[i], i == 0 ? "DYNWAVE" : "KINWAVE",
               cJ1, cO1, qualErr, bad ? "  <-- wrong" : "");
    }
    if (nbad)
    {
        printf("FAIL: in %d of 3 decks the withdrawal leaves its pollutant behind "
               "(concentration above every source's 100 mg/L, or no mass balance)\n", nbad);
        return 1;
    }
    printf("PASS: withdrawals leave at the node's concentration; all nodes stay at 100 mg/L\n");
    return 0;
}
