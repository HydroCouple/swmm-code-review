/*
 * NUM-35 for 6.0.0: an aquifer's effective upper-zone ET fraction above 1 makes the
 * lower-zone ET negative, which puts water into the saturated zone.
 *
 * ETu is the fraction of the evaporation potential used by the upper zone;
 * the rest, (1 - ETu), is available to the lower zone. Lower-zone ET is a loss
 * and can never be negative. Two decks, 10 days of June, evaporation
 * 0.2 in/day:
 *
 *   NUM-35_monthly-pattern.inp  ETu = 0.5 with a June pattern factor of 3.0
 *                               (valid input). Lower Zone ET must be >= 0.
 *   NUM-35_etu-1.5.inp          ETu = 1.5. Either the input is rejected as an
 *                               invalid aquifer parameter (as 6.0.0 does) or
 *                               Lower Zone ET must be >= 0.
 *
 * 6.0.0 rejects ETu = 1.5 (ERROR 109) but reports Lower Zone ET = -0.362 in
 * for the pattern deck. Vol I sec. 5.3.3 constrains lower-zone ET to be
 * non-negative. The tolerance, -0.0005 in, is the rounding of the report's
 * 3 decimals.
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"

/* first number after the dotted leader of the report line containing label;
   col = 0 for the first column (acre-feet), 1 for the second (inches) */
static int rptValue(const char* rpt, const char* label, int col, double* v)
{
    char line[512];
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        char* p = strstr(line, label);
        double a, b;
        int n;
        if (!p) continue;
        p += strlen(label);
        p += strspn(p, " .");
        n = sscanf(p, "%lf %lf", &a, &b);
        fclose(f);
        if (n < col + 1) return 0;
        *v = col ? b : a;
        return 1;
    }
    fclose(f);
    return 0;
}

/* returns -1 if the report could not be read, else the open/run error code */
static int runDeck(const char* inp, const char* rpt, const char* out,
                   double* upper, double* lower)
{
    double t = 0.0;
    SWMM_Engine e = swmm_engine_create();
    int err = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (err) return err;
    if (!rptValue(rpt, "Upper Zone ET", 1, upper) ||
        !rptValue(rpt, "Lower Zone ET", 1, lower)) return -1;
    return 0;
}

int main(void)
{
    double up1 = 0, lo1 = 0, up2 = 0, lo2 = 0;
    int err1, err2, bad1, bad2;

    err1 = runDeck("NUM-35_monthly-pattern.inp", "NUM-35_pat6.rpt", "NUM-35_pat6.out", &up1, &lo1);
    err2 = runDeck("NUM-35_etu-1.5.inp", "NUM-35_etu6.rpt", "NUM-35_etu6.out", &up2, &lo2);
    if (err1 == -1 || err2 == -1)
    {
        printf("FAIL: could not read the groundwater continuity table\n");
        return 1;
    }

    printf("Deck                         Upper Zone ET  Lower Zone ET\n");
    printf("                                      (in)           (in)\n");
    if (err1) printf("ETu 0.5 x June factor 3.0    stopped with error %d\n", err1);
    else printf("ETu 0.5 x June factor 3.0    %13.3f  %13.3f\n", up1, lo1);
    if (err2) printf("ETu 1.5                      rejected, error %d\n", err2);
    else printf("ETu 1.5                      %13.3f  %13.3f\n", up2, lo2);

    bad1 = lo1 < -0.0005;
    bad2 = err2 == 0 && lo2 < -0.0005;
    if (err1)
    {
        printf("FAIL: the valid deck with an ETu pattern stopped with error %d\n", err1);
        return 1;
    }
    if (bad1 || bad2)
    {
        printf("FAIL: negative lower-zone ET adds water to the aquifer:");
        if (bad1) printf(" %.3f in with the ETu pattern", lo1);
        if (bad1 && bad2) printf(",");
        if (bad2) printf(" %.3f in with ETu 1.5 (accepted as input)", lo2);
        printf("\n");
        return 1;
    }
    printf("PASS: lower-zone ET is never negative (%.3f in with the ETu pattern; "
           "ETu 1.5 %s)\n", lo1, err2 ? "rejected as input" : "runs without negative ET");
    return 0;
}
