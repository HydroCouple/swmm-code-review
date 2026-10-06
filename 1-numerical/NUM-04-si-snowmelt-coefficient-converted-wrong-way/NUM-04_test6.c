/*
 * NUM-04 for 6.0.0: SI snow melt coefficients are multiplied by 1.8 instead of
 * divided (SWMMEngine.cpp copies legacy setMeltParams()).
 *
 * Same decks and check as NUM-04_test.c: a 25.4 mm (1 in) pack, 10 degC
 * (18 degF) above its base temperature, no rain, 1 hour. The degree-day
 * equation gives 0.4572 mm/hr/degC x 10 degC x 1 h = 4.572 mm (0.180 in).
 * The pack water equivalent left on the pervious surface is read with
 * swmm_subcatch_get_snow_state() (user depth units: mm SI, in US) after the
 * last step. Tolerance 5%; the faulty SI conversion melts 14.81 mm.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_subcatchments.h"

/* pervious-surface SWE after the run, in user depth units, or -1 */
static double run(const char *inp, const char *rpt, const char *out)
{
    double t = 0.0, swe = -1.0;
    int err, k;
    SWMM_Engine e = swmm_engine_create();

    err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    k = swmm_subcatch_index(e, "S1");
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (err || t <= 0) break;
    }
    if (!err) err = swmm_subcatch_get_snow_state(e, k, 2, &swe, NULL, NULL, NULL);
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err)
    {
        printf("run of %s stopped with error %d\n", inp, err);
        return -1.0;
    }
    return swe;
}

int main(void)
{
    const double expected_mm = 4.572;
    double si_left, us_left, si_melt, us_melt;
    int ok;

    si_left = run("NUM-04_si.inp", "NUM-04_si6.rpt", "NUM-04_si6.out");
    us_left = run("NUM-04_us.inp", "NUM-04_us6.rpt", "NUM-04_us6.out");
    if (si_left < 0.0 || us_left < 0.0)
    {
        printf("FAIL: a run did not complete\n");
        return 1;
    }
    si_melt = 25.4 - si_left;             /* mm */
    us_melt = (1.0 - us_left) * 25.4;     /* in -> mm */

    printf("Melt of a 25.4 mm pack in 1 h at 10 degC (18 degF) above base\n");
    printf("Units  Pack left        Melt (mm)  Expected (mm)  Ratio\n");
    printf("SI     %7.3f mm      %8.3f   %8.3f      %6.3f\n",
           si_left, si_melt, expected_mm, si_melt / expected_mm);
    printf("US     %7.3f in      %8.3f   %8.3f      %6.3f\n",
           us_left, us_melt, expected_mm, us_melt / expected_mm);

    ok = si_melt > 0.95 * expected_mm && si_melt < 1.05 * expected_mm
      && us_melt > 0.95 * expected_mm && us_melt < 1.05 * expected_mm;
    if (!ok)
    {
        printf("FAIL: the pack melted %.3f mm in SI units and %.3f mm in US "
               "units; the degree-day equation gives %.3f mm in both\n",
               si_melt, us_melt, expected_mm);
        return 1;
    }
    printf("PASS: both unit systems melt %.3f mm, as the degree-day equation "
           "gives\n", expected_mm);
    return 0;
}
