/*
 * IO-51: with [REPORT] CONTINUITY NO, a large NEGATIVE continuity error does
 * not force the continuity table into the report.
 *
 * massbal_report() writes each continuity table when CONTINUITY is YES or
 * when the error exceeds 10 % (MAX_FLOW_BALANCE_ERR, MAX_RUNOFF_BALANCE_ERR):
 *     if ( massbal_getFlowError() > MAX_FLOW_BALANCE_ERR || ... )
 * The errors are signed: positive when water is lost, negative when more
 * leaves (or is stored) than came in. Only the positive side is tested.
 *
 * The deck: J1 gets 1 cfs of dry weather flow and an external inflow of
 * -2 cfs (a withdrawal). Only 1 cfs is there to withdraw, but the full 2 cfs
 * is booked as outflow, so the run ends with a flow routing continuity error
 * of about -100 %. The deck sets CONTINUITY NO.
 *
 * Correct behaviour (the rule the code implements): the Flow Routing
 * Continuity table is in the report exactly when |error| > 10 %.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define RPT "IO-51.rpt"

int main(void)
{
    double elapsed = 0.0;
    int err, table = 0;
    float runoffErr = 0.0f, flowErr = 0.0f, qualErr = 0.0f;
    char line[256];
    FILE *f;

    err = swmm_open("IO-51_withdrawal.inp", RPT, "IO-51.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    f = fopen(RPT, "r");
    if (!f) { printf("FAIL: cannot open %s\n", RPT); return 1; }
    while (fgets(line, sizeof line, f))
        if (strstr(line, "Flow Routing Continuity")) table = 1;
    fclose(f);

    printf("[REPORT] CONTINUITY              NO\n");
    printf("flow routing continuity error   %.3f %% (swmm_getMassBalErr)\n", flowErr);
    printf("Flow Routing Continuity table   %s\n", table ? "written" : "not written");

    if ((fabs(flowErr) > 10.0) != table)
    {
        printf("FAIL: the flow routing continuity error is %.3f %% (|error| %s 10 %%) "
               "but the table is %s\n", flowErr, fabs(flowErr) > 10.0 ? ">" : "<=",
               table ? "written" : "not written");
        return 1;
    }
    printf("PASS: the table is %s for a continuity error of %.3f %%\n",
           table ? "written" : "left out", flowErr);
    return 0;
}
