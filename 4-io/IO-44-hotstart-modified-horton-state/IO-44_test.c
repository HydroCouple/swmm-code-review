/*
 * IO-44: a hot start file does not carry Modified Horton's cumulative
 * infiltration Fmh, so a restarted run can infiltrate Fmax all over again.
 *
 * One pervious acre with MODIFIED_HORTON infiltration (f0 3 in/hr, fmin
 * 0.5 in/hr, decay 4/hr, drying time 1000 days, Fmax 1.0 in) gets a 0.8 in
 * storm at the start of each of two days. It is run three ways:
 *
 *   IO-44_continuous.inp  both days in one run
 *   IO-44_day1.inp        day 1, saving IO-44_day1.hsf at the end
 *   IO-44_day2.inp        day 2, starting from IO-44_day1.hsf
 *
 * Infiltration and runoff depths (in) come from the runoff continuity table of
 * each report. Day 1 of the continuous run has the same input as the day-1
 * run, so its day 2 is the continuous total minus the day-1 run.
 *
 * Correct behaviour: a run restarted from a hot start file reproduces the
 * continuous run, so day 2 infiltrates and runs off the same depths in both;
 * and the infiltration over both days stays within Fmax = 1 in (Fmax is the
 * "maximum infiltration volume possible"). The tolerance, 0.01 in, covers the
 * 3-decimal report and the 0.003 in that 1000-day dry-weather recovery
 * restores between the storms; the defect is 0.6 in.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define FMAX 1.0
#define TOL  0.01

/* last number on the first report line that contains label (inches column) */
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

static int run(const char *inp, const char *rpt, const char *out,
               double *infil, double *runoff)
{
    double elapsed = 0.0;
    int err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    *infil = rpt_value(rpt, "Infiltration Loss");
    *runoff = rpt_value(rpt, "Surface Runoff");
    return err;
}

int main(void)
{
    double ci, cr, i1, r1, i2, r2;
    int err;

    err = run("IO-44_continuous.inp", "IO-44_continuous.rpt", "IO-44_continuous.out", &ci, &cr);
    if (!err) err = run("IO-44_day1.inp", "IO-44_day1.rpt", "IO-44_day1.out", &i1, &r1);
    if (!err) err = run("IO-44_day2.inp", "IO-44_day2.rpt", "IO-44_day2.out", &i2, &r2);
    if (err || isnan(ci) || isnan(i1) || isnan(i2))
    {
        printf("FAIL: a run stopped with error %d or wrote no runoff continuity table\n", err);
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
