/*
 * IO-45: evaporation read back from a runoff interface file is 24 times too
 * large.
 *
 * IO-45_save.inp computes runoff for one impervious subcatchment (0.5 in of
 * rain, 0.2 in of depression storage, constant potential evaporation of
 * 0.2 in/day) and saves it with SAVE RUNOFF. IO-45_use.inp is the same model
 * reading that file with USE RUNOFF. The test reads the subcatchment's
 * evaporation series (in/day) from both binary output files.
 *
 * Correct behaviour: a run that reads a runoff interface file reports the
 * runoff results of the run that wrote it, so both series are equal (within
 * 1 %, far above float rounding); and evaporation never exceeds the potential
 * rate of 0.2 in/day (plus 0.1 % for float rounding).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

#define PET 0.2     /* in/day, [EVAPORATION] CONSTANT */

static int run(const char *inp, const char *rpt, const char *out)
{
    double elapsed = 0.0;
    int err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_close();
    return err;
}

/* subcatchment 0's evaporation series; returns the number of periods */
static int evap_series(const char *out, float **v)
{
    SMO_Handle h = NULL;
    int n = 0, len = 0;
    *v = NULL;
    if (SMO_init(&h) || SMO_open(h, out)) return 0;
    if (!SMO_getTimes(h, SMO_numPeriods, &n) && n > 0)
        SMO_getSubcatchSeries(h, 0, SMO_evap_loss, 0, n, v, &len);  /* end is exclusive */
    SMO_close(&h);
    return *v ? len : 0;
}

int main(void)
{
    float *es = NULL, *eu = NULL;
    double maxs = 0, maxu = 0, sums = 0, sumu = 0, maxdiff = 0;
    int err, ns, nu, i, ok;

    err = run("IO-45_save.inp", "IO-45_save.rpt", "IO-45_save.out");
    if (!err) err = run("IO-45_use.inp", "IO-45_use.rpt", "IO-45_use.out");
    if (err)
    {
        printf("FAIL: a run stopped with error %d\n", err);
        return 1;
    }
    ns = evap_series("IO-45_save.out", &es);
    nu = evap_series("IO-45_use.out", &eu);
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
        sums += es[i] / 24.0;           /* hourly periods: in/day -> in */
        sumu += eu[i] / 24.0;
        maxdiff = fmax(maxdiff, fabs(eu[i] - es[i]));
    }
    printf("maximum  %13.4f   %14.4f   (potential %.1f in/day)\n", maxs, maxu, PET);
    printf("total    %13.4f   %14.4f   in (sum of the hourly values)\n", sums, sumu);

    ok = maxdiff <= 0.01 * maxs && maxu <= PET * 1.001 && maxs <= PET * 1.001;
    SMO_free((void **)&es);
    SMO_free((void **)&eu);
    if (!ok)
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
