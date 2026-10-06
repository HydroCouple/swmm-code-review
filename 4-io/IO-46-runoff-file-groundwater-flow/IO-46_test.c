/*
 * IO-46: groundwater flow read from a runoff interface file is treated as a
 * rate per unit area, so it is multiplied by the subcatchment area again.
 *
 * IO-46_save.inp is a 2-day groundwater recession: a 10-acre subcatchment
 * with no rain whose aquifer drains to junction J1. It saves its runoff
 * results with SAVE RUNOFF. IO-46_use.inp is the same model reading that file
 * with USE RUNOFF.
 *
 * Correct behaviour: a run that reads a runoff interface file gets the runoff
 * and groundwater inflows of the run that wrote it, so J1's largest lateral
 * inflow and the routing continuity's "Groundwater Inflow" volume are the
 * same in both runs. Tolerance 1 %: the file stores single-precision values,
 * while the defect is a factor of about 200,000.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

/* first number after the dot leader of the report line containing label */
static double rpt_value(const char *rpt, const char *label)
{
    char line[512], *p;
    double v = NAN;
    FILE *f = fopen(rpt, "r");
    if (!f) return NAN;
    while (fgets(line, sizeof(line), f))
    {
        if (!(p = strstr(line, label))) continue;
        p += strlen(label);
        while (*p == ' ' || *p == '.') p++;
        sscanf(p, "%lf", &v);
        break;
    }
    fclose(f);
    return v;
}

static int run(const char *inp, const char *rpt, const char *out,
               double *qmax, double *vgw)
{
    double elapsed = 0.0;
    int j1, err = swmm_open(inp, rpt, out);
    *qmax = 0.0;
    if (!err) err = swmm_start(1);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (!err) *qmax = fmax(*qmax, swmm_getValue(swmm_NODE_LATFLOW, j1));
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    *vgw = rpt_value(rpt, "Groundwater Inflow");
    return err;
}

int main(void)
{
    double qs, vs, qu, vu;
    int err;

    err = run("IO-46_save.inp", "IO-46_save.rpt", "IO-46_save.out", &qs, &vs);
    if (!err) err = run("IO-46_use.inp", "IO-46_use.rpt", "IO-46_use.out", &qu, &vu);
    if (err || isnan(vs) || isnan(vu))
    {
        printf("FAIL: a run stopped with error %d or wrote no routing continuity table\n", err);
        return 1;
    }
    printf("%-34s %16s %22s\n", "", "J1 max lateral", "Groundwater Inflow");
    printf("%-34s %16s %22s\n", "", "inflow (cfs)", "(ac-ft)");
    printf("%-34s %16.4f %22.3f\n", "SAVE RUNOFF run (computes GW)", qs, vs);
    printf("%-34s %16.4f %22.3f\n", "USE RUNOFF run (reads the file)", qu, vu);

    if (fabs(qu - qs) > 0.01 * qs || fabs(vu - vs) > 0.01 * vs)
    {
        printf("FAIL: the USE RUNOFF run routes %.3f ac-ft of groundwater inflow "
               "(peak %.4f cfs) instead of %.3f ac-ft (peak %.4f cfs)\n",
               vu, qu, vs, qs);
        return 1;
    }
    printf("PASS: the USE RUNOFF run routes the groundwater inflow of the run "
           "that wrote the file\n");
    return 0;
}
