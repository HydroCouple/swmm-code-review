/*
 * IO-50 for 6.0.0: a continuity error computed from NaN totals is reported
 * as 0.000 %.
 *
 * 6.0.0 copies the legacy three-branch error formula (|in - out| < tol,
 * in > 0, out > 0, otherwise 0) into DefaultReportPlugin and the C API, so a
 * NaN ledger reads as a perfect balance there too.
 *
 * Run A sets a NaN lateral inflow at J1 for one routing step, as the legacy
 * test does. 6.0.0 checks heads and link flows for divergence after every
 * step, so this run is expected to stop with an error, which is correct
 * behaviour (nothing is reported as balanced).
 *
 * Run B sets a NaN TSS mass flux at J1 for one routing step with
 * swmm_node_set_quality_mass_flux(). The hydraulics stay finite, so the
 * divergence check does not fire, but the TSS ledger becomes NaN.
 *
 * Correct behaviour: a continuity table with NaN rows must not show a
 * numeric continuity error, in the report or from the C API getters.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_pollutants.h"
#include "openswmm/engine/openswmm_massbalance.h"

/* Reads the continuity table that follows the line containing `title`:
 * counts the rows that print nan and returns the Continuity Error value. */
static int readTable(const char *rpt, const char *title, int *nanRows, double *pct)
{
    char line[256];
    FILE *f = fopen(rpt, "r");
    *nanRows = 0;
    if (!f) return 0;
    while (fgets(line, sizeof line, f) && !strstr(line, title)) ;
    printf("%s table of %s:\n", title, rpt);
    while (fgets(line, sizeof line, f))
    {
        char *dots = strstr(line, "..");
        if (!dots) continue;
        printf("%s", line);
        if (strstr(line, "Continuity Error"))
        {
            while (*dots == '.') dots++;
            *pct = strtod(dots, NULL);
            fclose(f);
            return 1;
        }
        if (strstr(line, "nan")) (*nanRows)++;
    }
    fclose(f);
    return 0;
}

/* Runs the deck with a NaN set at J1 during step 10.
 * mode 0: lateral inflow, mode 1: TSS mass flux. Returns the run's error. */
static int run(int mode, const char *rpt, const char *out,
               double *flowErr, double *qualErr)
{
    double t = 0.0;
    int step = 0, j1, tss;
    SWMM_Engine e = swmm_engine_create();
    int rc = swmm_engine_open(e, "IO-50_nan-inflow.inp", rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    tss = swmm_pollutant_index(e, "TSS");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (rc || t <= 0) break;
        step++;
        if (mode == 0)
        {
            if (step == 10) swmm_node_set_lateral_inflow(e, j1, NAN);
            if (step == 11) swmm_node_set_lateral_inflow(e, j1, 0.0);
        }
        else
        {
            if (step == 10) swmm_node_set_quality_mass_flux(e, j1, tss, NAN);
            if (step == 11) swmm_node_set_quality_mass_flux(e, j1, tss, 0.0);
        }
    }
    if (rc)
    {
        const char *msg = swmm_get_last_error_msg(e);
        printf("run stopped at step %d with error %d: %.160s\n", step, rc, msg ? msg : "");
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_get_routing_continuity_error(e, flowErr);
    swmm_get_quality_continuity_error(e, tss, qualErr);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

/* Returns 1 if the table is consistent: NaN rows give a NaN error. */
static int check(const char *what, int nanRows, double rptErr, double apiErr)
{
    printf("  nan rows: %d, error in report: %g %%, from the C API: %g (fraction)\n\n",
           nanRows, rptErr, apiErr);
    if (nanRows > 0 && (!isnan(rptErr) || !isnan(apiErr)))
    {
        printf("FAIL: %s has %d nan rows but its continuity error reads "
               "%.3f %% in the report and %.3f %% from the C API\n",
               what, nanRows, rptErr, 100.0 * apiErr);
        return 0;
    }
    if (nanRows == 0 && (isnan(rptErr) || isnan(apiErr)))
    {
        printf("FAIL: %s is finite but its continuity error is NaN\n", what);
        return 0;
    }
    return 1;
}

int main(void)
{
    double flowErr = 0.0, qualErr = 0.0, flowPct = 0.0, qualPct = 0.0;
    int rc, flowNan = 0, qualNan = 0;

    /* --- run A: NaN lateral inflow */
    printf("Run A: NaN lateral inflow at J1 for one step\n");
    rc = run(0, "IO-50a6.rpt", "IO-50a6.out", &flowErr, &qualErr);
    if (rc)
        printf("  stopped with an error: nothing is reported as balanced\n\n");
    else
    {
        if (!readTable("IO-50a6.rpt", "Flow Routing Continuity", &flowNan, &flowPct))
        {
            printf("FAIL: no Flow Routing Continuity table in IO-50a6.rpt\n");
            return 1;
        }
        if (!check("Flow Routing Continuity (run A)", flowNan, flowPct, flowErr)) return 1;
    }

    /* --- run B: NaN TSS mass flux */
    printf("Run B: NaN TSS mass flux at J1 for one step\n");
    rc = run(1, "IO-50b6.rpt", "IO-50b6.out", &flowErr, &qualErr);
    if (rc)
    {
        printf("  stopped with an error: nothing is reported as balanced\n\n");
        printf("PASS: both NaN inputs stop the run with an error\n");
        return 0;
    }
    if (!readTable("IO-50b6.rpt", "Quality Routing Continuity", &qualNan, &qualPct))
    {
        printf("FAIL: no Quality Routing Continuity table in IO-50b6.rpt\n");
        return 1;
    }
    if (!check("Quality Routing Continuity (run B)", qualNan, qualPct, qualErr)) return 1;
    if (qualNan == 0)
        printf("PASS: the quality ledger is finite (no NaN reached it)\n");
    else
        printf("PASS: the quality continuity error with %d nan rows is reported "
               "as nan, not as a number\n", qualNan);
    return 0;
}
