/*
 * IO-34 for 6.0.0: rule actions give links settings outside their valid range.
 *
 * Same decks and checks as IO-34_test.c. 6.0.0 also accepts a numeric outlet
 * setting of 3, which 5.2.4 and 5.3.0 reject.
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
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

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
    SWMM_Engine e = swmm_engine_create();
    *code = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!*code) *code = swmm_engine_initialize(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return *code != 0 && rptHas(rpt, "ERROR 211");
}

int main(void)
{
    double t = 0.0, sample[3] = {0.5, 1.5, 2.5}, so = 0, sp = 0, qo = 0, qp = 0;
    double soMin = 1e9, soMax = -1e9, spMin = 1e9, qoMin = 1e9, qpMin = 1e9;
    int err, OL, P, k = 0, ok = 1, rejP, rejO, codeP, codeO;

    SWMM_Engine e = swmm_engine_create();
    err = swmm_engine_open(e, "IO-34_modulated.inp", "IO-34_mod6.rpt", "IO-34_mod6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 0);
    OL = swmm_link_index(e, "OL1");
    P  = swmm_link_index(e, "P1");
    printf("  time   OL1 setting  OL1 flow (cfs)   P1 setting  P1 flow (cfs)\n");
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        swmm_link_get_control_setting(e, OL, &so);
        swmm_link_get_control_setting(e, P, &sp);
        swmm_link_get_flow(e, OL, &qo);
        swmm_link_get_flow(e, P, &qp);
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
    if (!err) err = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err)
    {
        printf("FAIL: the modulated run stopped with error %d\n", err);
        return 1;
    }
    printf("  over the run: OL1 setting %.2f to %.2f, P1 setting >= %.2f, "
           "OL1 flow >= %.3f, P1 flow >= %.3f\n", soMin, soMax, spMin, qoMin, qpMin);
    if (soMin < 0.0 || soMax > 1.0 || spMin < 0.0 || qoMin < -1e-6 || qpMin < -1e-6) ok = 0;

    rejP = rejected("IO-34_pump-minus-one.inp", "IO-34_pump6.rpt", "IO-34_pump6.out", &codeP);
    rejO = rejected("IO-34_outlet-three.inp", "IO-34_outlet6.rpt", "IO-34_outlet6.out", &codeO);
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
