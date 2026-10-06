/*
 * NUM-52 for 6.0.0: a PID controller with a constant error keeps moving its link.
 *
 * Same decks and checks as NUM-52_test.c. Node SU1 sits at a fixed depth, so
 * the controller's relative error is constant.
 *
 * Correct behaviour (velocity-form PID, e(-1) = 0):
 *  1. PID 0.1 0 0 with e = -1 moves OR1 once to 0.9 and holds it, for a 30 s
 *     and a 60 s routing step (checked at every step to within 0.001; the bug
 *     closes OR1 within 10 evaluations).
 *  2. PID 0.1 10 0 with e = -0.01 moves OR1 at kp*e/ki = -0.0001 per minute
 *     (slope between 10 and 20 min, to within 20 %; the bug gives -0.0021,
 *     and a fix that keeps the 0.0001 dead band on each update gives 0).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

#define NMARK 5
static const double marks[NMARK] = {1, 2, 5, 10, 20};   /* minutes */

typedef struct { double at[NMARK]; double minS, maxDevP; int err; } Run;

static Run runDeck(const char *inp, const char *rpt, const char *out)
{
    Run r = {{0}, 1.0, 0.0, 0};
    double t = 0.0, s = 0.0;
    int nMark = 0, L = -1;
    SWMM_Engine e = swmm_engine_create();

    r.err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!r.err) r.err = swmm_engine_initialize(e);
    if (!r.err) r.err = swmm_engine_start(e, 0);
    if (!r.err) L = swmm_link_index(e, "OR1");
    while (!r.err)
    {
        r.err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        swmm_link_get_control_setting(e, L, &s);
        if (s < r.minS) r.minS = s;
        if (fabs(s - 0.9) > r.maxDevP) r.maxDevP = fabs(s - 0.9);
        while (nMark < NMARK && t * 1440.0 + 1e-6 >= marks[nMark]) r.at[nMark++] = s;
    }
    if (!r.err) r.err = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
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

    p30 = runDeck("NUM-52_p-rs30.inp", "NUM-52_p-rs30_6.rpt", "NUM-52_p-rs30_6.out");
    p60 = runDeck("NUM-52_p-rs60.inp", "NUM-52_p-rs60_6.rpt", "NUM-52_p-rs60_6.out");
    pi  = runDeck("NUM-52_pi-small-error.inp", "NUM-52_pi_6.rpt", "NUM-52_pi_6.out");
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
