/*
 * IO-47: the groundwater table read back from a runoff interface file is
 * mirrored about the aquifer bottom.
 *
 * IO-47_save.inp is a 2-day groundwater recession (water table from 8 ft,
 * aquifer bottom 0 ft, overridden to 1 ft for subcatchment S1) that saves its
 * runoff results with SAVE RUNOFF; IO-47_use.inp reads them with USE RUNOFF.
 * The test reads S1's groundwater elevation (ft) and upper-zone soil moisture
 * series from both binary output files.
 *
 * Correct behaviour: a run that reads a runoff interface file reports the
 * groundwater state of the run that wrote it, so both series agree (within
 * 0.01 ft and 0.001; the file stores single precision), and the water table
 * stays between S1's aquifer bottom (1 ft) and its surface (10 ft).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

#define EBOT  1.0
#define ESURF 10.0

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

/* subcatchment 0's series of attribute attr; returns the number of periods */
static int series(const char *out, SMO_subcatchAttribute attr, float **v)
{
    SMO_Handle h = NULL;
    int n = 0, len = 0;
    *v = NULL;
    if (SMO_init(&h) || SMO_open(h, out)) return 0;
    if (!SMO_getTimes(h, SMO_numPeriods, &n) && n > 0)
        SMO_getSubcatchSeries(h, 0, attr, 0, n, v, &len);   /* end is exclusive */
    SMO_close(&h);
    return *v ? len : 0;
}

int main(void)
{
    float *es = NULL, *eu = NULL, *ms = NULL, *mu = NULL;
    double de = 0, dm = 0, emin = 1e30, emax = -1e30;
    int err, n, i, ok;

    err = run("IO-47_save.inp", "IO-47_save.rpt", "IO-47_save.out");
    if (!err) err = run("IO-47_use.inp", "IO-47_use.rpt", "IO-47_use.out");
    if (err)
    {
        printf("FAIL: a run stopped with error %d\n", err);
        return 1;
    }
    n = series("IO-47_save.out", SMO_gwtable_elev, &es);
    if (n == 0 || series("IO-47_use.out", SMO_gwtable_elev, &eu) != n ||
        series("IO-47_save.out", SMO_soil_moisture, &ms) != n ||
        series("IO-47_use.out", SMO_soil_moisture, &mu) != n)
    {
        printf("FAIL: could not read the groundwater series\n");
        return 1;
    }

    printf("hour   S1 groundwater elevation (ft)   S1 soil moisture\n");
    printf("         SAVE run      USE run          SAVE run   USE run\n");
    for (i = 0; i < n; i++)
    {
        if (i == 0 || i % 12 == 11)
            printf("%4d   %10.3f   %10.3f         %8.4f  %8.4f\n",
                   i + 1, es[i], eu[i], ms[i], mu[i]);
        de = fmax(de, fabs(eu[i] - es[i]));
        dm = fmax(dm, fabs(mu[i] - ms[i]));
        emin = fmin(emin, eu[i]);
        emax = fmax(emax, eu[i]);
    }
    printf("USE run water table between %.3f and %.3f ft (aquifer bottom %.0f ft, "
           "surface %.0f ft)\n", emin, emax, EBOT, ESURF);

    ok = de <= 0.01 && dm <= 0.001 && emin >= EBOT - 0.01 && emax <= ESURF + 0.01;
    SMO_free((void **)&es); SMO_free((void **)&eu);
    SMO_free((void **)&ms); SMO_free((void **)&mu);
    if (!ok)
    {
        printf("FAIL: the USE RUNOFF run's groundwater state differs from the run "
               "that wrote the file by up to %.3f ft (elevation) and %.4f "
               "(moisture)\n", de, dm);
        return 1;
    }
    printf("PASS: the USE RUNOFF run reports the groundwater state of the run "
           "that wrote the file\n");
    return 0;
}
