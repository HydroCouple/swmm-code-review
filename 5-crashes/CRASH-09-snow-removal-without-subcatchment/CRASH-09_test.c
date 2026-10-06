/*
 * CRASH-09: [SNOWPACKS] REMOVAL with Fsub > 0 but no receiving subcatchment
 * indexes Subcatch[-1].
 *
 * The receiving subcatchment is the optional last field of a REMOVAL line.
 * When it is left out, snow_readMeltParams() stores toSubcatch = -1, nothing
 * checks it, and snow_plowSnow() reads Subcatch[-1].snowpack as soon as the
 * plowable pack is deeper than Dplow. Under AddressSanitizer the run stops
 * there with a heap-buffer-overflow (verdict CRASH); a plain build reads
 * whatever lies before the array and may write snow through it.
 *
 * CRASH-09_fsub-no-subcatch.inp: 1 in of snow on S1 (10 ac, all plowable),
 * REMOVAL Fsub = 1.0 with no subcatchment named, 60 degF for 6 h, no rain or
 * losses. Correct behaviour: the run either rejects the input with an error
 * code, or completes with the snow kept (it has nowhere to go) and a runoff
 * continuity error within 1% (all snow melts and runs off; about -0.6% here
 * from the time stepping).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

/* "inches" column of a Runoff Quantity Continuity line, or -1 */
static double rpt_depth(const char *rpt, const char *label)
{
    char line[256];
    double vol, depth = -1.0;
    FILE *f = fopen(rpt, "r");
    if (!f) return -1.0;
    while (fgets(line, sizeof line, f))
    {
        char *p = strstr(line, label);
        if (!p) continue;
        p += strlen(label);
        while (*p == ' ' || *p == '.') p++;
        if (sscanf(p, "%lf %lf", &vol, &depth) != 2) depth = -1.0;
        break;
    }
    fclose(f);
    return depth;
}

int main(void)
{
    double elapsed = 0.0;
    float runoffErr = 0.0f, flowErr, qualErr;
    int err, steps = 0;

    err = swmm_open("CRASH-09_fsub-no-subcatch.inp", "CRASH-09.rpt", "CRASH-09.out");
    if (!err) err = swmm_start(1);
    if (err)
    {
        swmm_close();
        printf("PASS: the input is rejected with error %d instead of "
               "indexing Subcatch[-1]\n", err);
        return 0;
    }
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        steps++;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    if (!err) err = swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d after %d steps\n", err, steps);
        return 1;
    }

    printf("Runoff steps run: %d\n", steps);
    printf("Initial snow cover  %6.3f in\n",
           rpt_depth("CRASH-09.rpt", "Initial Snow Cover"));
    printf("Surface runoff      %6.3f in\n",
           rpt_depth("CRASH-09.rpt", "Surface Runoff"));
    printf("Final snow cover    %6.3f in\n",
           rpt_depth("CRASH-09.rpt", "Final Snow Cover"));
    printf("Runoff continuity error %.3f %%\n", runoffErr);

    /* the run with the snow kept gives about -0.6%; 1% leaves margin */
    if (fabs(runoffErr) > 1.0)
    {
        printf("FAIL: the run completed but lost or created snow: runoff "
               "continuity error %.3f%%\n", runoffErr);
        return 1;
    }
    printf("PASS: the run completed, the snow with no receiving subcatchment "
           "stayed on S1 (runoff continuity error %.3f%%)\n", runoffErr);
    return 0;
}
