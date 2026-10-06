/*
 * IO-34: rule actions give links settings outside their valid range.
 *
 * A pump's setting is a speed (0 = off, no upper limit); an orifice's, weir's
 * or outlet's setting is the fraction open, 0 to 1. SWMM rejects a numeric
 * regulator setting outside 0..1 (ERROR 211) and clamps a PID's output, but
 * it does not check a numeric pump setting, nor the values a CURVE or
 * TIMESERIES action produces.
 *
 * Correct behaviour, checked here:
 *  1. IO-34_modulated.inp: outlet OL1 follows a series that goes 1, 3, -0.5
 *     and pump P1 one that goes 1, -1. At every step OL1's setting must lie in
 *     0..1 and P1's must be >= 0, and neither link may carry negative flow
 *     (both point downstream; nothing drives water back up).
 *  2. IO-34_pump-minus-one.inp ("PUMP P1 SETTING = -1") must be rejected with
 *     ERROR 211, as IO-34_outlet-three.inp ("OUTLET OL1 SETTING = 3") is.
 * With the bug OL1 runs at setting 3 (three times its rating) and then -0.5
 * (reverse flow), P1 pumps backwards, and the pump rule is accepted.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* returns 1 if the report file contains the text */
static int rptHas(const char *rpt, const char *text)
{
    char line[512];
    int found = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return 0;
    while (!found && fgets(line, sizeof line, f)) found = strstr(line, text) != NULL;
    fclose(f);
    return found;
}

/* opens a deck that should be rejected; returns 1 if it was, with ERROR 211 */
static int rejected(const char *inp, const char *rpt, const char *out, int *code)
{
    *code = swmm_open(inp, rpt, out);
    swmm_close();
    return *code != 0 && rptHas(rpt, "ERROR 211");
}

int main(void)
{
    double t = 0.0, sample[3] = {0.5, 1.5, 2.5}, so, sp, qo, qp;
    double soMin = 1e9, soMax = -1e9, spMin = 1e9, qoMin = 1e9, qpMin = 1e9;
    int err, OL, P, k = 0, ok = 1, rejP, rejO, codeP, codeO;

    err = swmm_open("IO-34_modulated.inp", "IO-34_mod.rpt", "IO-34_mod.out");
    if (!err) err = swmm_start(0);
    OL = swmm_getIndex(swmm_LINK, "OL1");
    P  = swmm_getIndex(swmm_LINK, "P1");
    printf("  time   OL1 setting  OL1 flow (cfs)   P1 setting  P1 flow (cfs)\n");
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        so = swmm_getValue(swmm_LINK_SETTING, OL);
        sp = swmm_getValue(swmm_LINK_SETTING, P);
        qo = swmm_getValue(swmm_LINK_FLOW, OL);
        qp = swmm_getValue(swmm_LINK_FLOW, P);
        if (so < soMin) soMin = so;
        if (so > soMax) soMax = so;
        if (sp < spMin) spMin = sp;
        if (qo < qoMin) qoMin = qo;
        if (qp < qpMin) qpMin = qp;
        if (k < 3 && t * 24.0 >= sample[k] - 1e-9)
        {
            printf("  %3.1f h     %6.2f      %8.3f        %6.2f      %8.3f\n",
                   sample[k], so, qo, sp, qp);
            k++;
        }
    }
    swmm_end();
    swmm_close();
    if (err)
    {
        printf("FAIL: the modulated run stopped with error %d\n", err);
        return 1;
    }
    printf("  over the run: OL1 setting %.2f to %.2f, P1 setting >= %.2f, "
           "OL1 flow >= %.3f, P1 flow >= %.3f\n", soMin, soMax, spMin, qoMin, qpMin);
    if (soMin < 0.0 || soMax > 1.0 || spMin < 0.0 || qoMin < -1e-6 || qpMin < -1e-6) ok = 0;

    rejP = rejected("IO-34_pump-minus-one.inp", "IO-34_pump.rpt", "IO-34_pump.out", &codeP);
    rejO = rejected("IO-34_outlet-three.inp", "IO-34_outlet.rpt", "IO-34_outlet.out", &codeO);
    printf("  PUMP P1 SETTING = -1:   %s (error code %d)\n",
           rejP ? "rejected with ERROR 211" : "accepted", codeP);
    printf("  OUTLET OL1 SETTING = 3: %s (error code %d)\n",
           rejO ? "rejected with ERROR 211" : "accepted", codeO);
    if (!rejP || !rejO) ok = 0;

    if (!ok)
    {
        printf("FAIL: settings outside the valid range are used:");
        if (soMax > 1.0 || soMin < 0.0) printf(" OL1 at %.2f..%.2f;", soMin, soMax);
        if (spMin < 0.0) printf(" P1 at %.2f;", spMin);
        if (qoMin < -1e-6 || qpMin < -1e-6) printf(" reverse flow (OL1 %.3f, P1 %.3f cfs);", qoMin, qpMin);
        if (!rejP) printf(" PUMP SETTING = -1 accepted;");
        if (!rejO) printf(" OUTLET SETTING = 3 accepted;");
        printf("\n");
        return 1;
    }
    printf("PASS: modulated settings stay in range and out-of-range numeric settings are rejected\n");
    return 0;
}
