/*
 * IO-51 for 6.0.0: with [REPORT] CONTINUITY NO, a large NEGATIVE continuity
 * error does not force the continuity table into the report.
 *
 * DefaultReportPlugin writes the Flow Routing Continuity table when
 * CONTINUITY is YES or when routing_error() * 100 > 10, as legacy
 * massbal_report() does. The error is signed, so a large negative error
 * (more water leaves than came in) never forces the table.
 *
 * The deck: J1 gets 1 cfs of dry weather flow and an external inflow of
 * -2 cfs (a withdrawal); only half of it can be met, and the run ends with a
 * flow routing continuity error of about -100 %. The deck sets
 * CONTINUITY NO.
 *
 * Correct behaviour: the table is in the report exactly when |error| > 10 %.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_massbalance.h"

#define RPT "IO-516.rpt"

int main(void)
{
    double t = 0.0, err = 0.0, pct;
    int table = 0;
    char line[256];
    FILE *f;
    SWMM_Engine e = swmm_engine_create();
    int rc = swmm_engine_open(e, "IO-51_withdrawal.inp", RPT, "IO-516.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_get_routing_continuity_error(e, &err);   /* a fraction */
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
        return 1;
    }
    pct = 100.0 * err;

    f = fopen(RPT, "r");
    if (!f) { printf("FAIL: cannot open %s\n", RPT); return 1; }
    while (fgets(line, sizeof line, f))
        if (strstr(line, "Flow Routing Continuity")) table = 1;
    fclose(f);

    printf("[REPORT] CONTINUITY              NO\n");
    printf("flow routing continuity error   %.3f %% (swmm_get_routing_continuity_error)\n", pct);
    printf("Flow Routing Continuity table   %s\n", table ? "written" : "not written");

    if ((fabs(pct) > 10.0) != table)
    {
        printf("FAIL: the flow routing continuity error is %.3f %% (|error| %s 10 %%) "
               "but the table is %s\n", pct, fabs(pct) > 10.0 ? ">" : "<=",
               table ? "written" : "not written");
        return 1;
    }
    printf("PASS: the table is %s for a continuity error of %.3f %%\n",
           table ? "written" : "left out", pct);
    return 0;
}
