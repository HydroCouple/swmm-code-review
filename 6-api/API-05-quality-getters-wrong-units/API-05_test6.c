/*
 * API-05 for 6.0.0: units of the ponded-quality API.
 *
 * 6.0.0 has no ponded-concentration property and no link load getter; its
 * counterpart is swmm_subcatch_get_ponded_quality(), documented as the
 * pollutant MASS in the ponded water. On the legacy test's deck (10 ac fully
 * impervious, rain at 10 mg/L, depression storage 0.10 in, no evaporation)
 * the ponded water is rain only, so mass / (10 mg/L x 28.3168 L/ft3 x area)
 * is the ponded depth:
 *   - 0 before the rain (0:01),
 *   - above the 0.10 in depression storage while it runs off (0:45),
 *   - back to 0.10 in once runoff has stopped (end of run, 4 h; the excess
 *     still draining is about 0.002 in), checked within 0.095-0.11 in.
 * A mass in any other unit (mg/L x ft3, or per acre) would be off by a
 * factor of 28 or more.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_subcatchments.h"

int main(void)
{
    const double LperFT3 = 28.316846592, areaFt2 = 10.0 * 43560.0, cRain = 10.0;
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, m, mDry = -1.0, m45 = 0.0, mEnd = 0.0, c45 = 0.0;
    double dDry, d45, dEnd;
    int s1, step = 0, got45 = 0;
    int rc = swmm_engine_open(e, "API-05_rain-quality.inp", "API-05_6.rpt", "API-05_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    s1 = swmm_subcatch_index(e, "S1");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        step++;
        m = 0.0;
        swmm_subcatch_get_ponded_quality(e, s1, 0, &m);
        if (step == 1) mDry = m;
        if (!got45 && t * 1440.0 >= 45.0 - 1e-6)
        {
            m45 = m;
            swmm_subcatch_get_quality(e, s1, 0, &c45);
            got45 = 1;
        }
        mEnd = m;
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);

    /* implied ponded depth in inches */
    dDry = mDry / (cRain * LperFT3 * areaFt2) * 12.0;
    d45  = m45  / (cRain * LperFT3 * areaFt2) * 12.0;
    dEnd = mEnd / (cRain * LperFT3 * areaFt2) * 12.0;
    printf("6.0.0 has no ponded-concentration property and no link load getter;\n"
           "swmm_subcatch_get_ponded_quality() returns the ponded P1 mass\n");
    printf("%-10s %16s %22s\n", "time", "P1 mass (mg)", "implied depth (in)");
    printf("%-10s %16.6g %22.5f\n", "0:01", mDry, dDry);
    printf("%-10s %16.6g %22.5f   (runoff conc. %.4f mg/L)\n", "0:45", m45, d45, c45);
    printf("%-10s %16.6g %22.5f   (depression storage 0.10 in)\n", "4:00", mEnd, dEnd);
    if (rc || fabs(mDry) > 1e-9 || !(d45 > 0.10 && d45 < 0.5) || !(dEnd > 0.095 && dEnd < 0.11))
    {
        printf("FAIL: the ponded quality is not the pollutant mass in mg (error code %d)\n", rc);
        return 1;
    }
    printf("PASS: the ponded quality getter returns the ponded mass in mg "
           "(implied depth %.4f in at the end vs 0.10 in of depression storage)\n", dEnd);
    return 0;
}
