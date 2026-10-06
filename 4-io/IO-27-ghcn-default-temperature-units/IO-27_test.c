/*
 * IO-27: a GHCN-Daily climate file used without the Units token is not read
 * in the documented default units.
 *
 * The input reference ([TEMPERATURE], FILE line) says the optional Units
 * token is "C10 for tenths of a degree C (the default), C for degrees C or F
 * for degrees F". Older GHCN-Daily files, and every GHCN file SWMM 5.1 read,
 * give temperatures in tenths of a degree C.
 *
 * The climate file here has TMAX 250 and TMIN 150 every day, i.e. 25 C and
 * 15 C. The decks give no Units token. Correct behaviour (the documented
 * default C10): the hourly air temperature stays between 15 and 25 C, that is
 * 59 to 77 F in the US-unit deck. The test reads the air temperature the run
 * writes to the binary output each hour (deg F for US, deg C for SI units)
 * and requires every value to lie within 0.5 degrees of that range. The buggy
 * engine reads the numbers as deg F (US) or deg C (SI): 150 to 250 degrees.
 */
#include <stdio.h>
#include "swmm5.h"
#include "swmm_output.h"

/* runs one deck; returns 1 if all hourly temperatures lie in [lo, hi] */
static int check(const char *inp, const char *rpt, const char *out,
                 double lo, double hi, const char *unit)
{
    double elapsed = 0.0, tmin = 1e9, tmax = -1e9;
    int err, nper = 0, k, len = 0;
    SMO_Handle h = NULL;
    float *vals = NULL;

    err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_close();
    if (err)
    {
        printf("%s: run stopped with error %d\n", inp, err);
        return 0;
    }

    SMO_init(&h);
    if (SMO_open(h, out) != 0 || SMO_getTimes(h, SMO_numPeriods, &nper) != 0 || nper < 1)
    {
        printf("%s: cannot read %s\n", inp, out);
        return 0;
    }
    /* index 0 of the system results is the air temperature in user units */
    for (k = 0; k < nper; k++)
    {
        if (SMO_getSystemResult(h, k, 0, &vals, &len) != 0 || len < 1) { SMO_close(&h); return 0; }
        if (vals[0] < tmin) tmin = vals[0];
        if (vals[0] > tmax) tmax = vals[0];
        SMO_free((void **)&vals);
    }
    SMO_close(&h);

    printf("  %-22s hourly air temperature %7.2f to %7.2f %s  (expected %.0f to %.0f %s)\n",
           inp, tmin, tmax, unit, lo, hi, unit);
    return tmin >= lo - 0.5 && tmax <= hi + 0.5;
}

int main(void)
{
    int okUS, okSI;
    printf("GHCN file with TMAX 250, TMIN 150 (tenths of deg C), no Units token\n");
    okUS = check("IO-27_us-units.inp", "IO-27_us.rpt", "IO-27_us.out", 59.0, 77.0, "F");
    okSI = check("IO-27_si-units.inp", "IO-27_si.rpt", "IO-27_si.out", 15.0, 25.0, "C");

    if (!okUS || !okSI)
    {
        printf("FAIL: without a Units token the GHCN temperatures are not read in tenths of "
               "a degree C, the documented default (%s)\n",
               (!okUS && !okSI) ? "US and SI decks" : (!okUS ? "US deck" : "SI deck"));
        return 1;
    }
    printf("PASS: without a Units token the GHCN temperatures are read in tenths of a degree C\n");
    return 0;
}
