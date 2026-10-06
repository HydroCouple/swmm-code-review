/*
 * NUM-53: with RULE_STEP > 0 the rules are told the routing step, not the
 * rule interval.
 *
 * Rules run once per RULE_STEP, but controls_evaluate() receives the routing
 * step as its time step. That step sets the width of the "=" window for time
 * premises and the dt of a PID action.
 *
 * Correct behaviour, checked here with RULE_STEP 5 min:
 *  1. NUM-53_time-eq.inp: rule TEQ (SIMULATION TIME = 1.05, i.e. 01:03) sets
 *     OR1 to 0.2 and rule CEQ (CLOCKTIME = 02:03:00) sets it to 0.5. Each must
 *     act once, at the rule evaluation that covers its time: within one rule
 *     step (plus the routing step in which the new setting is read) of 01:03
 *     and 02:03. With the bug the "=" window is +-15 s around 01:03 and 02:03,
 *     no evaluation (at :00 and :05) falls in it, and neither rule ever acts.
 *  2. NUM-53_pid-rt10.inp / NUM-53_pid-rt60.inp: PID 0.002 0.1 0 with a
 *     constant relative error of -1, routing steps 10 s and 60 s. The integral
 *     term is kp*e/ki = -0.02 per minute of controller time, so between 12 min
 *     and 22 min (two 5-minute rule intervals) the setting must drop by 0.2,
 *     for both routing steps. Tolerance 5 % (0.01). It also covers the extra
 *     kp*e = -0.002 per evaluation that NUM-52's error reset adds while that
 *     bug is unfixed (-0.204 in all). With the bug each evaluation integrates
 *     over one routing step only: -0.011 (10 s) and -0.044 (60 s).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

/* minutes at which OR1's setting first equals each value (-1 = never) */
static int runTimeEq(double *t02, double *t05)
{
    double t = 0.0, s;
    int err, L;
    *t02 = *t05 = -1.0;
    err = swmm_open("NUM-53_time-eq.inp", "NUM-53_time-eq.rpt", "NUM-53_time-eq.out");
    if (!err) err = swmm_start(0);
    L = swmm_getIndex(swmm_LINK, "OR1");
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        s = swmm_getValue(swmm_LINK_SETTING, L);
        if (*t02 < 0.0 && fabs(s - 0.2) < 1e-9) *t02 = t * 1440.0;
        if (*t05 < 0.0 && fabs(s - 0.5) < 1e-9) *t05 = t * 1440.0;
    }
    swmm_end();
    swmm_close();
    return err;
}

/* OR1's setting at 12 and 22 min */
static int runPid(const char *inp, const char *rpt, const char *out, double *s12, double *s22)
{
    double t = 0.0, s;
    int err, L;
    *s12 = *s22 = -1.0;
    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(0);
    L = swmm_getIndex(swmm_LINK, "OR1");
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        s = swmm_getValue(swmm_LINK_SETTING, L);
        if (*s12 < 0.0 && t * 1440.0 >= 12.0 - 1e-6) *s12 = s;
        if (*s22 < 0.0 && t * 1440.0 >= 22.0 - 1e-6) *s22 = s;
    }
    swmm_end();
    swmm_close();
    return err;
}

int main(void)
{
    double t02, t05, a12, a22, b12, b22, dA, dB, expD = 0.002 * (-1.0) * 10.0 / 0.1;
    int e1, e2, e3, okT, okP;

    e1 = runTimeEq(&t02, &t05);
    e2 = runPid("NUM-53_pid-rt10.inp", "NUM-53_pid-rt10.rpt", "NUM-53_pid-rt10.out", &a12, &a22);
    e3 = runPid("NUM-53_pid-rt60.inp", "NUM-53_pid-rt60.rpt", "NUM-53_pid-rt60.out", &b12, &b22);
    if (e1 || e2 || e3)
    {
        printf("FAIL: a run stopped with an error (%d, %d, %d)\n", e1, e2, e3);
        return 1;
    }

    printf("RULE_STEP 5 min, ROUTING_STEP 30 s\n");
    printf("  TIME = 1.05 (01:03):        OR1 -> 0.2 at %s", t02 < 0 ? "never" : "");
    if (t02 >= 0) printf("%.1f min", t02);
    printf("   (due by 63 + 5 + 0.5 min)\n");
    printf("  CLOCKTIME = 02:03:00:       OR1 -> 0.5 at %s", t05 < 0 ? "never" : "");
    if (t05 >= 0) printf("%.1f min", t05);
    printf("   (due by 123 + 5 + 0.5 min)\n");
    dA = a22 - a12;
    dB = b22 - b12;
    printf("PID 0.002 0.1 0, e = -1, RULE_STEP 5 min    setting at 12 min   22 min   change\n");
    printf("  ROUTING_STEP 10 s                        %8.4f       %8.4f  %8.4f\n", a12, a22, dA);
    printf("  ROUTING_STEP 60 s                        %8.4f       %8.4f  %8.4f\n", b12, b22, dB);
    printf("  expected change kp*e*(10 min)/ki                                %8.4f\n", expD);

    okT = t02 >= 58.0 && t02 <= 68.5 && t05 >= 118.0 && t05 <= 128.5;
    okP = fabs(dA - expD) <= 0.05 * fabs(expD) && fabs(dB - expD) <= 0.05 * fabs(expD);
    if (!okT || !okP)
    {
        printf("FAIL:");
        if (!okT)
            printf(" the time premises with '=' did not act within one rule step"
                   " (OR1 -> 0.2 at %.1f min, -> 0.5 at %.1f min; -1 = never);", t02, t05);
        if (!okP)
            printf(" the PID integral over 10 min is %.4f (10 s steps) and %.4f (60 s steps)"
                   " instead of %.4f;", dA, dB, expD);
        printf("\n");
        return 1;
    }
    printf("PASS: '=' time premises act once per rule step and the PID integrates over "
           "the rule interval, independent of the routing step\n");
    return 0;
}
