/*
 * CRASH-01 for 6.0.0: datetime::timeDiff() converts an out-of-range double to long.
 *
 * 6.0.0 keeps the legacy algorithm but returns a long, so the NO_DATE case of
 * the legacy test (-6.3e10 s) fits on 64-bit Linux and macOS. It still
 * overflows on Windows, where long is 32 bits, and on every platform for date
 * differences beyond about 2.9e14 years, which the public API accepts:
 * swmm_datetime_time_diff(1e15, 0, &out).
 *
 * Correct behaviour: no undefined conversion. A difference that does not fit
 * in a long is clamped to LONG_MAX / LONG_MIN (callers only test the sign and
 * compare with a step length).
 */
#include <stdio.h>
#include <limits.h>
#include "openswmm/engine/openswmm_datetime.h"

int main(void)
{
    long secs = 0;
    double d2020 = 0.0;
    int ok = 1;

    /* the legacy test's case: NO_DATE (-693594) against 1 Jan 2020 */
    swmm_datetime_encode_date(2020, 1, 1, &d2020);
    swmm_datetime_time_diff(-693594.0, d2020, &secs);
    printf("NO_DATE - 2020-01-01      = %ld s\n", secs);
    if (secs >= 0) ok = 0;

    /* a difference far outside the range of a 64-bit long */
    swmm_datetime_time_diff(1.0e15, 0.0, &secs);
    printf("1e15 days - day 0         = %ld s (exact value 8.64e19 s)\n", secs);
    if (secs != LONG_MAX) ok = 0;

    swmm_datetime_time_diff(-1.0e15, 0.0, &secs);
    printf("-1e15 days - day 0        = %ld s\n", secs);
    if (secs != LONG_MIN) ok = 0;

    if (!ok)
    {
        printf("FAIL: an out-of-range time difference is not clamped\n");
        return 1;
    }
    printf("PASS: out-of-range time differences are clamped, with no undefined conversion\n");
    return 0;
}
