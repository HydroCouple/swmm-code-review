/*
 * IO-46 for 6.0.0: groundwater inflow in a run that reads a runoff interface
 * file.
 *
 * Same decks and checks as IO-46_test.c: J1's largest lateral inflow and the
 * routing continuity's "Groundwater Inflow" volume of the USE RUNOFF run must
 * equal those of the SAVE RUNOFF run that wrote the file (within 1 %).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

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
    double t = 0.0, q = 0.0;
    int j1, rc;
    SWMM_Engine e = swmm_engine_create();
    *qmax = 0.0;
    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (rc || t <= 0) break;
        swmm_node_get_lateral_inflow(e, j1, &q);
        *qmax = fmax(*qmax, q);
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    *vgw = rpt_value(rpt, "Groundwater Inflow");
    return rc;
}

int main(void)
{
    double qs, vs, qu, vu;
    int rc;

    rc = run("IO-46_save.inp", "IO-466_save.rpt", "IO-466_save.out", &qs, &vs);
    if (!rc) rc = run("IO-46_use.inp", "IO-466_use.rpt", "IO-466_use.out", &qu, &vu);
    if (rc || isnan(vs) || isnan(vu))
    {
        printf("FAIL: a run stopped with error %d or wrote no routing continuity table\n", rc);
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
