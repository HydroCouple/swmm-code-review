/*
 * NUM-50: under STEADY routing, a pollutant mass flux added to a conduit
 * through the 5.3.0 API is diluted by the conduit's volume, so most of the
 * mass booked as external inflow never leaves the conduit.
 *
 * Under STEADY routing a conduit has no mixing volume: its concentration is
 * what it passes on, and the mass it delivers per step is c * q * dt. The
 * 5.3.0 code turns the flux F into a concentration increment
 * F * dt / (V + q * dt), the dynamic-wave mixing formula, and books all of F.
 * Only the fraction q * dt / (V + q * dt) leaves.
 *
 * The deck sends 1 cfs of clean water through one conduit; the test sets a
 * flux of 10 on C1 after the first routing step and runs 2 hours. Correct:
 * mass conservation, i.e. the mass booked as External Inflow leaves as
 * External Outflow (quality continuity error near 0). The check allows 5%:
 * steady-flow quality routing itself leaves 1-2% in transit at the end of a
 * run (see CRASH-12's deck in 5.2.4), while the bug loses about 60%.
 * The check uses only the report's own ledger, so it does not depend on the
 * unit of the flux (API-08).
 *
 * The link flux API exists only in 5.3.0; on 5.2.4 the test reports that.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

/* read "External Inflow" / "External Outflow" from the Quality Routing
   Continuity table of the report */
static double lastNumber(const char *line)
{
    const char *p = line + strlen(line);
    while (p > line && (p[-1] == '\n' || p[-1] == '\r' || p[-1] == ' ')) p--;
    while (p > line && p[-1] != ' ') p--;
    return atof(p);
}

static int readQualLedger(const char *rpt, double *in, double *out)
{
    char line[256];
    int inQual = 0, found = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Quality Routing Continuity")) inQual = 1;
        if (!inQual) continue;
        if (strstr(line, "External Inflow"))  { *in = lastNumber(line);  found++; }
        if (strstr(line, "External Outflow")) { *out = lastNumber(line); found++; }
        if (strstr(line, "Continuity Error")) break;
    }
    fclose(f);
    return found == 2;
}

int main(void)
{
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    double elapsed = 0.0, cLink = 0.0, cOut = 0.0, q = 0.0, extIn = 0.0, extOut = 0.0;
    float runoffErr = 0, flowErr = 0, qualErr = 0;
    int err, c1, o1, step = 0;

    err = swmm_open("NUM-50_steady-conduit.inp", "NUM-50.rpt", "NUM-50.out");
    if (!err) err = swmm_start(1);
    c1 = swmm_getIndex(swmm_LINK, "C1");
    o1 = swmm_getIndex(swmm_NODE, "O1");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        if (++step == 1)
            err = swmm_setValueExpanded(swmm_LINK, swmm_LINK_POLLUTANT_LATMASS_FLUX,
                                        c1, 0, 0, 10.0);
        q = swmm_getValue(swmm_LINK_FLOW, c1);
        cLink = swmm_getValueExpanded(swmm_LINK, swmm_LINK_POLLUTANT_CONCENTRATION, c1, 0, 0);
        cOut = swmm_getValueExpanded(swmm_NODE, swmm_NODE_POLLUTANT_CONCENTRATION, o1, 0, 0);
    }
    swmm_end();
    swmm_getMassBalErr(&runoffErr, &flowErr, &qualErr);
    swmm_report();
    swmm_close();

    if (err || !readQualLedger("NUM-50.rpt", &extIn, &extOut))
    {
        printf("FAIL: the run stopped with error %d or the report has no quality ledger\n", err);
        return 1;
    }
    printf("Final step: C1 flow %.4f cfs, C1 conc %.5f, O1 conc %.5f (mg/L)\n", q, cLink, cOut);
    printf("Quality ledger: External Inflow %.3f lb, External Outflow %.3f lb, "
           "delivered %.1f %%, continuity error %.3f %%\n",
           extIn, extOut, 100.0 * extOut / extIn, qualErr);
    if (fabs(qualErr) > 5.0)
    {
        printf("FAIL: only %.1f %% of the API mass flux booked on C1 leaves the conduit "
               "(continuity error %.3f %%)\n", 100.0 * extOut / extIn, qualErr);
        return 1;
    }
    printf("PASS: the mass flux added to C1 under STEADY routing leaves the conduit "
           "(continuity error %.3f %%)\n", qualErr);
    return 0;
#else
    printf("PASS: not affected, 5.2.4 has no link pollutant flux API\n");
    return 0;
#endif
}
