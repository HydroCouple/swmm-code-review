/*
 * API-06 for 6.0.0: per-area buildup through the API.
 *
 * 6.0.0 has no runtime external-buildup property. Its per-area buildup
 * setter is swmm_subcatch_set_initial_loading() (the [LOADINGS] value, mass
 * per unit area). On the legacy test's two decks (10 ac, one land use, no
 * buildup function, dry day, DRY_STEP 1 h and 5 min) a loading of 1.0 set
 * before the run must read back as 1.0 and give the same 10 lb (1.0 lb/ac x
 * 10 ac) of Initial Buildup in the report for both time steps.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_subcatchments.h"

/* "Initial Buildup ..........        10.000" from the Runoff Quality Continuity */
static double reportInitialBuildup(const char *rpt)
{
    char line[512];
    double x = -1.0;
    FILE *f = fopen(rpt, "r");
    if (!f) return -1.0;
    while (fgets(line, sizeof(line), f))
    {
        char *p = strstr(line, "Initial Buildup");
        if (p)
        {
            /* skip the dot leader to the first number */
            p += strlen("Initial Buildup");
            while (*p == ' ' || *p == '.') p++;
            sscanf(p, "%lf", &x);
            break;
        }
    }
    fclose(f);
    return x;
}

static int runDeck(const char *inp, const char *rpt, const char *out, double *back)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0;
    int s1, rc = swmm_engine_open(e, inp, rpt, out, NULL);
    s1 = swmm_subcatch_index(e, "S1");
    if (!rc) rc = swmm_subcatch_set_initial_loading(e, s1, 0, 1.0);
    if (!rc) rc = swmm_subcatch_get_initial_loading(e, s1, 0, back);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

int main(void)
{
    double back1 = 0, back5 = 0, ib1, ib5;
    int rc1 = runDeck("API-06_dry-step-1h.inp", "API-06_6_1h.rpt", "API-06_6_1h.out", &back1);
    int rc5 = runDeck("API-06_dry-step-5min.inp", "API-06_6_5min.rpt", "API-06_6_5min.out", &back5);
    ib1 = reportInitialBuildup("API-06_6_1h.rpt");
    ib5 = reportInitialBuildup("API-06_6_5min.rpt");

    printf("6.0.0 has no runtime external buildup; swmm_subcatch_set_initial_loading(S1, P1, 1.0)\n");
    printf("%-10s %10s %26s %10s\n", "DRY_STEP", "read back", "report Initial Buildup", "expected");
    printf("%-10s %10.4f %23.3f lb %10s\n", "1 h", back1, ib1, "10 lb");
    printf("%-10s %10.4f %23.3f lb %10s\n", "5 min", back5, ib5, "10 lb");
    if (rc1 || rc5 || fabs(back1 - 1.0) > 1e-9 || fabs(back5 - 1.0) > 1e-9 ||
        fabs(ib1 - 10.0) > 0.5 || fabs(ib5 - 10.0) > 0.5)
    {
        printf("FAIL: the per-area buildup API is not symmetric or depends on the time step "
               "(error codes %d, %d)\n", rc1, rc5);
        return 1;
    }
    printf("PASS: a per-area buildup set through the API reads back as set and gives "
           "1.0 lb/ac x 10 ac = 10 lb whatever the time step\n");
    return 0;
}
