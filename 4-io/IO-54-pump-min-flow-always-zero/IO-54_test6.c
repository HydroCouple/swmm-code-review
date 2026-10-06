/*
 * IO-54 for 6.0.0: the Pumping Summary's "Min Flow" is always 0.00.
 *
 * Same check as IO-54_test.c. DefaultReportPlugin prints a literal 0.0 in
 * the Min Flow column, and the engine keeps no minimum pump flow.
 *
 * The deck has one ideal pump that passes its wet well's inflow: 1 cfs for
 * 3 hours, a ramp to 3 cfs over the 4th hour, then 3 cfs. The test steps the
 * run, records the smallest pump flow of the steps in which the pump ran
 * (flow above 0.001 cfs), writes the report and reads P1's row of the
 * Pumping Summary back.
 *
 * Correct behaviour: the reported Min Flow equals the smallest flow the pump
 * delivered while running (1.00 cfs). The tolerance of 0.01 cfs is the
 * rounding of the %9.2f column; the defect prints 0.00, 1 cfs away.
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

int main(void)
{
    char line[512], name[64];
    double t = 0.0, q, qmin = 1.0e30, qmax = 0.0;
    double pct, rptMin = -1.0, rptAvg = -1.0, rptMax = -1.0;
    int rc, p1, starts, inTable = 0, found = 0;
    FILE *f;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "IO-54_two-rates.inp", "IO-54_6.rpt", "IO-54_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    p1 = swmm_link_index(e, "P1");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        if (swmm_link_get_flow(e, p1, &q) == 0 && q > 0.001)
        {
            if (q < qmin) qmin = q;
            if (q > qmax) qmax = q;
        }
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc) { printf("FAIL: the run stopped with error %d\n", rc); return 1; }

    f = fopen("IO-54_6.rpt", "r");
    if (!f) { printf("FAIL: cannot read IO-54_6.rpt\n"); return 1; }
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Pumping Summary")) inTable = 1;
        if (inTable && sscanf(line, "%63s %lf %d %lf %lf %lf", name, &pct,
                              &starts, &rptMin, &rptAvg, &rptMax) == 6
            && strcmp(name, "P1") == 0)
        {
            found = 1;
            break;
        }
    }
    fclose(f);
    if (!found) { printf("FAIL: no P1 row in the Pumping Summary\n"); return 1; }

    printf("                       Min Flow   Max Flow  (CFS)\n");
    printf("pump flow while on   %9.2f  %9.2f\n", qmin, qmax);
    printf("Pumping Summary      %9.2f  %9.2f   (Avg Flow %.2f, %.2f %% utilized)\n",
           rptMin, rptMax, rptAvg, pct);

    if (rptMin < qmin - 0.01 || rptMin > qmin + 0.01)
    {
        printf("FAIL: Pumping Summary Min Flow is %.2f cfs, but the pump never ran "
               "below %.2f cfs\n", rptMin, qmin);
        return 1;
    }
    printf("PASS: Pumping Summary Min Flow (%.2f cfs) is the smallest flow the "
           "pump delivered while running\n", rptMin);
    return 0;
}
