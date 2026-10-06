/*
 * BND-04 for 6.0.0 (C API): pollutant mass carried into
 * the network by reverse flow from an outfall is not booked.
 *
 * Storage SU1 receives 1 cfs at 10 mg/L for 2 h (about 4.5 lb of P1) and
 * drains to outfall O1. The outfall stage then rises to 3 ft and about
 * 55,000 ft3 flows back into SU1. With no inflow of its own, O1 keeps its
 * last concentration (10 mg/L) and the reverse flow carries it into SU1;
 * the engine books that water as external inflow but not its mass.
 *
 * Correct behaviour: pollutant mass is conserved, so the quality routing
 * continuity error in the report is close to 0. Tolerance 1 %; the bug gives
 * about -760 %. The test prints the report's quality continuity lines.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_massbalance.h"

/* reads a number from the "Quality Routing Continuity" table of a report */
static double rptValue(const char *rpt, const char *label)
{
    char line[256];
    int inTable = 0;
    double x = NAN;
    FILE *f = fopen(rpt, "r");
    if (!f) return NAN;
    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "Quality Routing Continuity")) inTable = 1;
        if (inTable && strstr(line, label))
        {
            char *p = strstr(line, label) + strlen(label);
            while (*p == ' ' || *p == '.') p++;
            x = strtod(p, NULL);
            break;
        }
    }
    fclose(f);
    return x;
}

int main(void)
{
    double ext, out, fin, pct, eQual = NAN, eFlow = NAN;
    int err;
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0;

    err = swmm_engine_open(e, "BND-04_pond-backflow.inp", "BND-04_6.rpt", "BND-04_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_get_quality_continuity_error(e, 0, &eQual);   /* fraction */
    swmm_get_routing_continuity_error(e, &eFlow);       /* fraction */
    eQual *= 100.0;
    eFlow *= 100.0;
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    ext = rptValue("BND-04_6.rpt", "External Inflow");
    out = rptValue("BND-04_6.rpt", "External Outflow");
    fin = rptValue("BND-04_6.rpt", "Final Stored Mass");
    pct = rptValue("BND-04_6.rpt", "Continuity Error (%)");
    printf("Quality routing continuity (P1, lb)\n");
    printf("  External Inflow    %10.3f\n", ext);
    printf("  External Outflow   %10.3f\n", out);
    printf("  Final Stored Mass  %10.3f\n", fin);
    printf("  Continuity Error   %10.3f %%   (swmm_get_quality_continuity_error: %.3f %%)\n", pct, eQual);
    printf("Flow routing continuity error %.3f %%\n", eFlow);

    if (!(fabs(eQual) <= 1.0))
    {
        printf("FAIL: pollutant mass is not conserved: %.3f lb booked in, %.3f lb out, %.3f lb "
               "stored at the end (continuity error %.3f %%)\n", ext, out, fin, eQual);
        return 1;
    }
    printf("PASS: the mass carried by the outfall's reverse flow is booked "
           "(quality continuity error %.3f %%)\n", eQual);
    return 0;
}
