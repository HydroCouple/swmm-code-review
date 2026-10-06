/*
 * IO-45 for 6.0.0: evaporation read back from a runoff interface file.
 *
 * Same decks and checks as IO-45_test.c: the USE RUNOFF run must report the
 * subcatchment evaporation (in/day) of the SAVE RUNOFF run that wrote the file
 * (within 1 %), and never more than the potential 0.2 in/day (plus 0.1 %).
 * The series are read with 6.0.0's output reader.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_output.h"

#define PET 0.2     /* in/day, [EVAPORATION] CONSTANT */
#define MAXP 200

/* subcatchment 0's evaporation series; returns the number of periods */
static int evap_series(const char *out, float *v)
{
    float buf[8];
    int n, i;
    SWMM_Output h = swmm_output_open(out);
    if (!h) return 0;
    n = swmm_output_get_period_count(h);
    if (n > MAXP) n = MAXP;
    for (i = 0; i < n; i++)
    {
        if (swmm_output_get_subcatch_result(h, i, SWMM_OUT_SUBCATCH_EVAP, buf)) { n = 0; break; }
        v[i] = buf[0];
    }
    swmm_output_close(h);
    return n;
}

int main(void)
{
    static float es[MAXP], eu[MAXP];
    double maxs = 0, maxu = 0, sums = 0, sumu = 0, maxdiff = 0;
    int rc, ns, nu, i;

    rc = swmm_engine_run("IO-45_save.inp", "IO-456_save.rpt", "IO-456_save.out", NULL);
    if (!rc) rc = swmm_engine_run("IO-45_use.inp", "IO-456_use.rpt", "IO-456_use.out", NULL);
    if (rc)
    {
        printf("FAIL: a run stopped with error %d\n", rc);
        return 1;
    }
    ns = evap_series("IO-456_save.out", es);
    nu = evap_series("IO-456_use.out", eu);
    if (ns == 0 || ns != nu)
    {
        printf("FAIL: could not read the evaporation series (%d and %d periods)\n", ns, nu);
        return 1;
    }

    printf("hour   S1 evaporation (in/day)\n");
    printf("       SAVE RUNOFF run   USE RUNOFF run\n");
    for (i = 0; i < ns; i++)
    {
        if (i < 3 || i % 6 == 5) printf("%4d   %15.4f   %14.4f\n", i + 1, es[i], eu[i]);
        maxs = fmax(maxs, es[i]);
        maxu = fmax(maxu, eu[i]);
        sums += es[i] / 24.0;
        sumu += eu[i] / 24.0;
        maxdiff = fmax(maxdiff, fabs(eu[i] - es[i]));
    }
    printf("maximum  %13.4f   %14.4f   (potential %.1f in/day)\n", maxs, maxu, PET);
    printf("total    %13.4f   %14.4f   in (sum of the hourly values)\n", sums, sumu);

    if (maxdiff > 0.01 * maxs || maxu > PET * 1.001 || maxs > PET * 1.001)
    {
        printf("FAIL: the USE RUNOFF run reports evaporation up to %.4f in/day "
               "(%.1f times the run that wrote the file, potential %.1f in/day)\n",
               maxu, maxs > 0 ? maxu / maxs : 0.0, PET);
        return 1;
    }
    printf("PASS: the USE RUNOFF run reports the evaporation of the run that wrote "
           "the file, within the potential rate\n");
    return 0;
}
