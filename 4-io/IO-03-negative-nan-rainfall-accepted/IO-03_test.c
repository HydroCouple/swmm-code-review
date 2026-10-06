/*
 * IO-03: negative and NaN rainfall values are used as rainfall.
 *
 * Three decks, each with one subcatchment (10 ac, 50 % impervious, Horton)
 * on a gage with hourly intensities over a 6-hour run:
 *   series-negative  time series 0.5, -2.0, 0.5, 0.0 in/hr
 *   file-negative    user-prepared rain file 0.5, -1, -1, 0.5 in/hr
 *                    (-1 used as a missing-data flag)
 *   file-nan         user-prepared rain file 0.5, nan, 0.5 in/hr
 *
 * Rainfall cannot be negative or undefined. The valid records of every deck
 * add up to 0.5 + 0.5 = 1.000 in, so the correct result is: no negative or
 * non-finite rainfall on the subcatchment at any step, Total Precipitation
 * 1.000 in, and a runoff continuity error near zero. (Rejecting the deck at
 * input time with an error code is also acceptable.)
 *
 * Tolerances: 0.01 in on the precipitation total (the wrong values are
 * -1.000 in and nan) and 1 % on the continuity error (normal runs of these
 * decks give about 0.1 %; the wrong ones give -206 %).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

static void readReport(const char *rpt, double *precip, double *contErr)
{
    char line[256];
    int inRunoff = 0;
    FILE *f = fopen(rpt, "r");
    *precip = NAN;
    *contErr = NAN;
    if (!f) return;
    while (fgets(line, sizeof(line), f))
    {
        char *p;
        if (strstr(line, "Runoff Quantity Continuity")) inRunoff = 1;
        if (!inRunoff) continue;
        if ((p = strstr(line, "Total Precipitation ......")) != NULL)
        {
            /* second column is inches; strtod also reads "nan" */
            char s[2][32];
            if (sscanf(p + 26, "%31s %31s", s[0], s[1]) == 2)
                *precip = strtod(s[1], NULL);
        }
        if ((p = strstr(line, "Continuity Error (%) .....")) != NULL)
        {
            char s[32];
            if (sscanf(p + 26, "%31s", s) == 1) *contErr = strtod(s, NULL);
            break;
        }
    }
    fclose(f);
}

int main(void)
{
    const char *deck[3] = {"series-negative", "file-negative", "file-nan"};
    int d, ok = 1;
    char failMsg[512] = "";

    printf("Deck              min rain   non-finite   Total Precip   Continuity\n");
    printf("                  (in/hr)    values       (in)           Error (%%)\n");
    for (d = 0; d < 3; d++)
    {
        char inp[64], rpt[64], out[64];
        double elapsed = 0.0, r, q, minRain = 0.0, precip, contErr;
        int err, nBad = 0;

        sprintf(inp, "IO-03_%s.inp", deck[d]);
        sprintf(rpt, "IO-03_%s.rpt", deck[d]);
        sprintf(out, "IO-03_%s.out", deck[d]);
        err = swmm_open(inp, rpt, out);
        if (!err) err = swmm_start(1);
        if (err)
        {
            /* the deck was refused at input time: acceptable */
            printf("%-16s  rejected with error %d\n", deck[d], err);
            swmm_close();
            continue;
        }
        while (!err)
        {
            err = swmm_step(&elapsed);
            if (elapsed <= 0.0) break;
            r = swmm_getValue(swmm_SUBCATCH_RAINFALL, 0);
            q = swmm_getValue(swmm_SUBCATCH_RUNOFF, 0);
            if (!isfinite(r) || !isfinite(q)) nBad++;
            else if (r < minRain) minRain = r;
        }
        swmm_end();
        swmm_report();
        swmm_close();
        readReport(rpt, &precip, &contErr);

        printf("%-16s  %8.3f   %6d       %10.3f     %10.3f\n",
               deck[d], minRain, nBad, precip, contErr);
        if (err || minRain < 0.0 || nBad > 0 || !(fabs(precip - 1.0) <= 0.01)
            || !(fabs(contErr) <= 1.0))
        {
            char s[160];
            ok = 0;
            sprintf(s, "%s%s: min rain %.3f in/hr, %d non-finite, precipitation %.3f in, "
                    "continuity %.3f %%", failMsg[0] ? "; " : "", deck[d], minRain,
                    nBad, precip, contErr);
            if (err) sprintf(s + strlen(s), ", run error %d", err);
            strncat(failMsg, s, sizeof(failMsg) - strlen(failMsg) - 1);
        }
    }
    printf("expected             >= 0        0            1.000     within +-1\n");

    if (!ok)
    {
        printf("FAIL: invalid rainfall values are used as rain (%s)\n", failMsg);
        return 1;
    }
    printf("PASS: negative and NaN records carry no rain (1.000 in from the valid "
           "records, continuity closes)\n");
    return 0;
}
