/*
 * CRASH-18: SMO_getSystemResult() reads into a NULL (5.3.0) or uninitialised
 * (5.2.4) pointer when the period index is out of range.
 *
 * The function sets error 422 for a period outside 0..Nperiods-1, but the
 * block that seeks and freads the system values has lost its "else", so it
 * runs anyway, with the result buffer never allocated. For a period past the
 * end the seek lands beyond the end of file and fread reads nothing; for
 * period -1 the seek lands inside the file and fread writes SysVars floats
 * through the bad pointer.
 *
 * Correct behaviour (header: "Error code 0 on success, -1 on failure or error
 * code"; error 422 = "reporting period index out of range"): an invalid period
 * returns 422 without touching memory and without handing back a pointer.
 * A valid period still returns the system values.
 */
#include <stdio.h>
#include "swmm5.h"
#include "swmm_output.h"

static int probe(SMO_Handle h, int period)
{
    float *v = NULL;
    int n = -1;
    int err = SMO_getSystemResult(h, period, 0, &v, &n);
    printf("%8d  %5d  %8d  %s\n", period, err, n, v ? "non-NULL" : "NULL");
    fflush(stdout);
    if (err == 0)
    {
        SMO_free((void **)&v);
        return 1;
    }
    /* an invalid period must give 422 and no buffer */
    return err == 422 && v == NULL;
}

int main(void)
{
    SMO_Handle h = NULL;
    int err, nper = 0, ok = 1;
    double elapsed = 0.0;

    /* run the deck to write CRASH-18.out */
    err = swmm_open("CRASH-18_reader.inp", "CRASH-18.rpt", "CRASH-18.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the model run stopped with error %d\n", err);
        return 1;
    }
    SMO_init(&h);
    err = SMO_open(h, "CRASH-18.out");
    if (err)
    {
        printf("FAIL: SMO_open returned %d\n", err);
        return 1;
    }
    SMO_getTimes(h, SMO_numPeriods, &nper);
    printf("Reporting periods in CRASH-18.out: %d\n\n", nper);
    printf("  period  error  length  buffer\n");

    ok &= probe(h, 0);         /* valid: the first period */
    ok &= probe(h, nper - 1);  /* valid: the last period */
    ok &= probe(h, nper);      /* one past the end: seek lands beyond EOF */
    ok &= probe(h, -1);        /* before the first period: seek lands inside the file */

    SMO_close(&h);

    if (!ok)
    {
        printf("FAIL: an out-of-range period did not return error 422 with no buffer\n");
        return 1;
    }
    printf("PASS: SMO_getSystemResult() returns error 422 for periods %d and -1 "
           "without reading into an unallocated buffer\n", nper);
    return 0;
}
