/*
 * CRASH-18 for 6.0.0: system results for an out-of-range period.
 *
 * The legacy SMO_getSystemResult() reads into an unallocated buffer when the
 * period is out of range. 6.0.0's reader (OutputReader::get_system_result,
 * behind swmm_output_get_system_result) checks the period before seeking and
 * writes into a caller-owned value.
 *
 * Correct behaviour: periods Nperiods and -1 return -1 and leave the caller's
 * value untouched; a valid period returns 0.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_output.h"

static int probe(SWMM_Output out, int period, int expect_ok)
{
    float value = -999.0f;   /* sentinel: must survive an invalid call */
    int rc = swmm_output_get_system_result(out, period, SWMM_OUT_SYS_RAINFALL, &value);
    printf("%8d  %5d  %g\n", period, rc, value);
    if (expect_ok) return rc == 0;
    return rc == -1 && value == -999.0f;
}

int main(void)
{
    int err, nper, ok = 1;
    SWMM_Output out;

    err = swmm_engine_run("CRASH-18_reader.inp", "CRASH-18_6.rpt", "CRASH-18_6.out", NULL);
    if (err)
    {
        printf("FAIL: the model run stopped with error %d\n", err);
        return 1;
    }
    out = swmm_output_open("CRASH-18_6.out");
    if (!out)
    {
        printf("FAIL: swmm_output_open failed\n");
        return 1;
    }
    nper = swmm_output_get_period_count(out);
    printf("Reporting periods in CRASH-18_6.out: %d\n\n", nper);
    printf("  period  rc     rainfall\n");

    ok &= probe(out, 0, 1);
    ok &= probe(out, nper - 1, 1);
    ok &= probe(out, nper, 0);
    ok &= probe(out, -1, 0);
    swmm_output_close(out);

    if (!ok)
    {
        printf("FAIL: an out-of-range period was not rejected\n");
        return 1;
    }
    printf("PASS: swmm_output_get_system_result() rejects periods %d and -1 "
           "and leaves the caller's value untouched\n", nper);
    return 0;
}
