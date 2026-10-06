/*
 * IO-44 for 6.0.0: Modified Horton's cumulative infiltration across a restart.
 *
 * Same model and checks as IO-44_test.c. 6.0.0 cannot restart this model from
 * a legacy .hsf file written by itself: its SAVE HOTSTART writes no
 * subcatchment records, and its USE HOTSTART skips subcatchment state anyway.
 * Its own hot start format (swmm_hotstart_save / swmm_hotstart_apply) carries
 * the infiltration state, so this test uses it: the day-1 run saves
 * IO-44_day1_v6.hsf after its last step, and IO-44_day2_v6.inp (day 2, no
 * [FILES] section) applies it before starting.
 *
 * The outfall is named OF1, not O1, so that the native file is 196 bytes, a
 * multiple of 4: for other lengths swmm_hotstart_open() reads the trailing
 * CRC through a misaligned uint32_t pointer (HotStartManager.cpp:336), which
 * the sanitizer build reports as undefined behaviour. That is a separate
 * defect and has nothing to do with the infiltration state.
 *
 * Correct behaviour: day 2 of the restarted run infiltrates and runs off the
 * same depths as day 2 of the continuous run, and the two days stay within
 * Fmax = 1 in (tolerance 0.01 in; see IO-44_test.c).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_hotstart.h"

#define FMAX 1.0
#define TOL  0.01

static double rpt_value(const char *rpt, const char *label)
{
    char line[512], *p, *last = NULL;
    double v = NAN;
    FILE *f = fopen(rpt, "r");
    if (!f) return NAN;
    while (fgets(line, sizeof(line), f))
    {
        if (!strstr(line, label)) continue;
        for (p = strtok(line, " \t\r\n"); p; p = strtok(NULL, " \t\r\n")) last = p;
        if (last) sscanf(last, "%lf", &v);
        break;
    }
    fclose(f);
    return v;
}

/* run inp; apply hot start file use_hs before the start, save save_hs after
   the step that reaches end_day */
static int run(const char *inp, const char *rpt, const char *out,
               const char *use_hs, const char *save_hs, double end_day,
               double *infil, double *runoff)
{
    double t = 0.0;
    SWMM_Engine e = swmm_engine_create();
    SWMM_HotStart hs = NULL;
    int rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc && use_hs)
    {
        rc = swmm_hotstart_open(use_hs, &hs);
        if (!rc) rc = swmm_hotstart_apply(e, hs);
        swmm_hotstart_close(hs);
    }
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (rc || t <= 0) break;
        if (save_hs && t >= end_day - 1.0e-9) rc = swmm_hotstart_save(e, save_hs);
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    if (rc) printf("%s: error %d %s\n", inp, rc, swmm_get_last_error_msg(e));
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    *infil = rpt_value(rpt, "Infiltration Loss");
    *runoff = rpt_value(rpt, "Surface Runoff");
    return rc;
}

int main(void)
{
    double ci, cr, i1, r1, i2, r2;
    int rc;

    rc = run("IO-44_continuous.inp", "IO-446_continuous.rpt", "IO-446_continuous.out",
             NULL, NULL, 0.0, &ci, &cr);
    if (!rc) rc = run("IO-44_day1.inp", "IO-446_day1.rpt", "IO-446_day1.out",
                      NULL, "IO-44_day1_v6.hsf", 1.0, &i1, &r1);
    if (!rc) rc = run("IO-44_day2_v6.inp", "IO-446_day2.rpt", "IO-446_day2.out",
                      "IO-44_day1_v6.hsf", NULL, 0.0, &i2, &r2);
    if (rc || isnan(ci) || isnan(i1) || isnan(i2))
    {
        printf("FAIL: a run stopped with error %d or wrote no runoff continuity table\n", rc);
        return 1;
    }

    printf("%-40s %12s %12s\n", "", "infiltration", "runoff");
    printf("%-40s %12s %12s\n", "", "(in)", "(in)");
    printf("%-40s %12.3f %12.3f\n", "continuous run, both days", ci, cr);
    printf("%-40s %12.3f %12.3f\n", "day-1 run (saves the hot start file)", i1, r1);
    printf("%-40s %12.3f %12.3f\n", "day 2 of the continuous run", ci - i1, cr - r1);
    printf("%-40s %12.3f %12.3f\n", "day-2 run from the hot start file", i2, r2);
    printf("%-40s %12.3f\n", "day-1 run + day-2 run", i1 + i2);

    if (fabs(i2 - (ci - i1)) > TOL || fabs(r2 - (cr - r1)) > TOL || i1 + i2 > FMAX + TOL)
    {
        printf("FAIL: the restarted day 2 infiltrates %.3f in and runs off %.3f in "
               "instead of %.3f in and %.3f in; the two days infiltrate %.3f in "
               "with Fmax = %.1f in\n", i2, r2, ci - i1, cr - r1, i1 + i2, FMAX);
        return 1;
    }
    printf("PASS: the run restarted from the hot start file reproduces day 2 of "
           "the continuous run, within Fmax\n");
    return 0;
}
