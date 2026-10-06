/*
 * NUM-52: a PID controller with a constant error keeps moving its link.
 *
 * In every deck node SU1 is an isolated storage unit at a fixed depth, so the
 * relative error the controller sees, e = (SetPoint - depth)/SetPoint, is the
 * same at every evaluation. Rules are evaluated at every routing step.
 *
 * SWMM's PID is the velocity (incremental) form, with e(-1) = 0:
 *     setting(k) = setting(k-1) + kp*[(e(k) - e(k-1)) + e(k)*dt/ki + kd*(...)/dt]
 *
 * Correct behaviour, from that equation with a constant error e:
 *  1. NUM-52_p-rs30.inp / NUM-52_p-rs60.inp: PID 0.1 0 0 (proportional only),
 *     e = -1. The first evaluation moves OR1 from 1.0 to 1 + 0.1*(-1) = 0.9;
 *     after that e(k) - e(k-1) = 0 and the setting stays 0.9, for a 30 s and a
 *     60 s routing step alike. Checked at every step to within 0.001; the bug
 *     closes the orifice (setting 0) within 10 evaluations.
 *  2. NUM-52_pi-small-error.inp: PID 0.1 10 0, e = -0.01. The integral term
 *     moves the setting at kp*e/ki = -0.0001 per minute, independent of the
 *     step. Checked as the slope between 10 and 20 min, to within 20 %. The
 *     bug gives -0.0021 per minute (21 times too fast); removing only the
 *     error reset, but keeping the 0.0001 dead band on each update, gives 0
 *     (every integral increment, 0.00005, is discarded and the controller
 *     never removes the error).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#define NMARK 5
static const double marks[NMARK] = {1, 2, 5, 10, 20};   /* minutes */

typedef struct { double at[NMARK]; double minS, maxDevP; int err; } Run;

/* runs a deck; maxDevP = largest |setting - 0.9| seen at any step */
static Run runDeck(const char *inp, const char *rpt, const char *out)
{
    Run r = {{0}, 1.0, 0.0, 0};
    double t = 0.0, s;
    int nMark = 0, L;

    r.err = swmm_open(inp, rpt, out);
    if (!r.err) r.err = swmm_start(0);
    L = swmm_getIndex(swmm_LINK, "OR1");
    while (!r.err)
    {
        r.err = swmm_step(&t);
        if (t <= 0.0) break;
        s = swmm_getValue(swmm_LINK_SETTING, L);
        if (s < r.minS) r.minS = s;
        if (fabs(s - 0.9) > r.maxDevP) r.maxDevP = fabs(s - 0.9);
        while (nMark < NMARK && t * 1440.0 + 1e-6 >= marks[nMark]) r.at[nMark++] = s;
    }
    swmm_end();
    swmm_close();
    return r;
}

static void show(const char *label, Run r)
{
    int i;
    printf("%-26s", label);
    for (i = 0; i < NMARK; i++) printf(" %8.5f", r.at[i]);
    printf(" %8.5f\n", r.minS);
}

int main(void)
{
    Run p30, p60, pi;
    double slope, expSlope = 0.1 * (-0.01) / 10.0;   /* kp*e/ki per minute */
    int okP, okPI;

    p30 = runDeck("NUM-52_p-rs30.inp", "NUM-52_p-rs30.rpt", "NUM-52_p-rs30.out");
    p60 = runDeck("NUM-52_p-rs60.inp", "NUM-52_p-rs60.rpt", "NUM-52_p-rs60.out");
    pi  = runDeck("NUM-52_pi-small-error.inp", "NUM-52_pi.rpt", "NUM-52_pi.out");
    if (p30.err || p60.err || pi.err)
    {
        printf("FAIL: a run stopped with an error (%d, %d, %d)\n", p30.err, p60.err, pi.err);
        return 1;
    }

    printf("OR1 setting at                1 min    2 min    5 min   10 min   20 min      min\n");
    show("P 0.1, e=-1, step 30 s", p30);
    show("P 0.1, e=-1, step 60 s", p60);
    show("PI 0.1 10, e=-0.01, 30 s", pi);
    slope = (pi.at[4] - pi.at[3]) / 10.0;
    printf("P runs: largest |setting - 0.9| at any step: %.5f (30 s), %.5f (60 s)\n",
           p30.maxDevP, p60.maxDevP);
    printf("PI run: slope 10-20 min %.6f per min, expected kp*e/ki = %.6f per min\n",
           slope, expSlope);

    okP  = p30.maxDevP < 0.001 && p60.maxDevP < 0.001;
    okPI = fabs(slope - expSlope) < 0.2 * fabs(expSlope);
    if (!okP || !okPI)
    {
        printf("FAIL:");
        if (!okP)
            printf(" the P-only controller drives OR1 to %.3f (30 s) and %.3f (60 s)"
                   " instead of holding 0.9;", p30.minS, p60.minS);
        if (!okPI)
            printf(" the PI controller moves %.6f per min instead of %.6f;", slope, expSlope);
        printf("\n");
        return 1;
    }
    printf("PASS: with a constant error the P controller holds 0.9 and the PI controller "
           "integrates at kp*e/ki, for both routing steps\n");
    return 0;
}
