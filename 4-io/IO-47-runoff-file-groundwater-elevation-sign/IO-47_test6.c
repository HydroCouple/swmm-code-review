/*
 * IO-47 for 6.0.0: the groundwater state read back from a runoff interface
 * file.
 *
 * Same decks and checks as IO-47_test.c: S1's groundwater elevation and soil
 * moisture in the USE RUNOFF run must match the SAVE RUNOFF run that wrote
 * the file (within 0.01 ft and 0.001), and the water table must stay between
 * S1's aquifer bottom (1 ft) and its surface (10 ft). The series are read with
 * 6.0.0's output reader.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_output.h"

#define EBOT  1.0
#define ESURF 10.0
#define MAXP  200

static int series(const char *out, int var, float *v)
{
    float buf[8];
    int n, i;
    SWMM_Output h = swmm_output_open(out);
    if (!h) return 0;
    n = swmm_output_get_period_count(h);
    if (n > MAXP) n = MAXP;
    for (i = 0; i < n; i++)
    {
        if (swmm_output_get_subcatch_result(h, i, var, buf)) { n = 0; break; }
        v[i] = buf[0];
    }
    swmm_output_close(h);
    return n;
}

int main(void)
{
    static float es[MAXP], eu[MAXP], ms[MAXP], mu[MAXP];
    double de = 0, dm = 0, emin = 1e30, emax = -1e30;
    int rc, n, i;

    rc = swmm_engine_run("IO-47_save.inp", "IO-476_save.rpt", "IO-476_save.out", NULL);
    if (!rc) rc = swmm_engine_run("IO-47_use.inp", "IO-476_use.rpt", "IO-476_use.out", NULL);
    if (rc)
    {
        printf("FAIL: a run stopped with error %d\n", rc);
        return 1;
    }
    n = series("IO-476_save.out", SWMM_OUT_SUBCATCH_GW_ELEV, es);
    if (n == 0 || series("IO-476_use.out", SWMM_OUT_SUBCATCH_GW_ELEV, eu) != n ||
        series("IO-476_save.out", SWMM_OUT_SUBCATCH_SOIL_MOIST, ms) != n ||
        series("IO-476_use.out", SWMM_OUT_SUBCATCH_SOIL_MOIST, mu) != n)
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

    if (de > 0.01 || dm > 0.001 || emin < EBOT - 0.01 || emax > ESURF + 0.01)
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
