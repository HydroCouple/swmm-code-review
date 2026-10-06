/*
 * IO-56 for 6.0.0: same check as IO-56_test.c. 6.0.0 clears the normal-flow
 * and inlet-control flags after counting them at every step, so it is
 * expected to pass unpatched.
 *
 * The deck has a junction fed with 4 cfs for the first hour of a 6-hour
 * run. A small low-level pipe (C2) drains it, and a steep 1.5-ft overflow
 * pipe (C1) leaves it 0.5 ft above its floor. C1 runs while the inflow
 * lasts; once the junction has drained below the overflow, C1 is dry for
 * the rest of the run.
 *
 * The test runs the deck, writes the report and reads C1's row of the Flow
 * Classification Summary.
 *
 * Correct behaviour: the normal-flow limitation applies only to flowing
 * conduits (dwflow.c tests it only in the SUBCRITICAL and SUPCRITICAL flow
 * classes), so Norm Ltd and Inlet Ctrl cannot exceed the fraction of time
 * the conduit is wet, 1 - Dry - Up Dry - Down Dry. The tolerance of 0.02
 * covers the rounding of the four %4.2f columns; the defect exceeds it by
 * the whole dry fraction (0.83).
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"

int main(void)
{
    char line[512], name[64];
    double len, fc[7], normLtd = -1.0, inletCtrl = -1.0, wet;
    int err, inTable = 0, found = 0;
    FILE *f;

    err = swmm_engine_run("IO-56_overflow.inp", "IO-56_6.rpt", "IO-56_6.out", NULL);
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    f = fopen("IO-56_6.rpt", "r");
    if (!f) { printf("FAIL: cannot read IO-56_6.rpt\n"); return 1; }
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Flow Classification Summary")) inTable = 1;
        if (inTable && sscanf(line, "%63s %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                              name, &len, &fc[0], &fc[1], &fc[2], &fc[3], &fc[4],
                              &fc[5], &fc[6], &normLtd, &inletCtrl) == 11
            && strcmp(name, "C1") == 0)
        {
            found = 1;
            break;
        }
    }
    fclose(f);
    if (!found) { printf("FAIL: no C1 row in the Flow Classification Summary\n"); return 1; }

    wet = 1.0 - fc[0] - fc[1] - fc[2];
    printf("C1 fraction of time:  Dry %.2f  Up Dry %.2f  Down Dry %.2f  "
           "Sub %.2f  Sup %.2f  Up Crit %.2f  Down Crit %.2f\n",
           fc[0], fc[1], fc[2], fc[3], fc[4], fc[5], fc[6]);
    printf("wet (1 - dry classes)  %.2f\n", wet);
    printf("Norm Ltd               %.2f\n", normLtd);
    printf("Inlet Ctrl             %.2f\n", inletCtrl);

    if (normLtd > wet + 0.02 || inletCtrl > wet + 0.02)
    {
        printf("FAIL: C1 is reported normal-flow limited %.2f of the time "
               "but was wet only %.2f of the time\n", normLtd, wet);
        return 1;
    }
    printf("PASS: Norm Ltd (%.2f) and Inlet Ctrl (%.2f) do not exceed the time "
           "C1 was wet (%.2f)\n", normLtd, inletCtrl, wet);
    return 0;
}
