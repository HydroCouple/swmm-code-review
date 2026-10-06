/*
 * IO-03 for 6.0.0: negative and NaN rainfall values are used as rainfall.
 *
 * Same decks and check as IO-03_test.c:
 *   series-negative  time series 0.5, -2.0, 0.5, 0.0 in/hr
 *   file-negative    user-prepared rain file 0.5, -1, -1, 0.5 in/hr
 *   file-nan         user-prepared rain file 0.5, nan, 0.5 in/hr
 * The valid records of every deck add up to 1.000 in. Correct: no negative or
 * non-finite rainfall on the subcatchment at any step, Total Precipitation
 * 1.000 in and a runoff continuity error near zero (or the deck is rejected
 * with an error code). swmm_subcatch_get_rainfall returns in/hr here and
 * swmm_subcatch_get_runoff cfs (project units).
 *
 * Tolerances: 0.01 in on the precipitation total (the wrong values are
 * -1.000 in and nan) and 1 % on the continuity error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_subcatchments.h"

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
        double t = 0.0, r, q, minRain = 0.0, precip, contErr;
        int rc, nBad = 0;
        SWMM_Engine e;

        sprintf(inp, "IO-03_%s.inp", deck[d]);
        sprintf(rpt, "IO-03_%s6.rpt", deck[d]);
        sprintf(out, "IO-03_%s6.out", deck[d]);
        e = swmm_engine_create();
        rc = swmm_engine_open(e, inp, rpt, out, NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 1);
        if (rc)
        {
            /* the deck was refused at input time: acceptable */
            printf("%-16s  rejected with error %d\n", deck[d], rc);
            swmm_engine_close(e);
            swmm_engine_destroy(e);
            continue;
        }
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (t <= 0.0) break;
            r = 0.0;
            q = 0.0;
            swmm_subcatch_get_rainfall(e, 0, &r);
            swmm_subcatch_get_runoff(e, 0, &q);
            if (!isfinite(r) || !isfinite(q)) nBad++;
            else if (r < minRain) minRain = r;
        }
        if (!rc) rc = swmm_engine_end(e);
        if (!rc) rc = swmm_engine_report(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        readReport(rpt, &precip, &contErr);

        printf("%-16s  %8.3f   %6d       %10.3f     %10.3f\n",
               deck[d], minRain, nBad, precip, contErr);
        if (rc || minRain < 0.0 || nBad > 0 || !(fabs(precip - 1.0) <= 0.01)
            || !(fabs(contErr) <= 1.0))
        {
            char s[160];
            ok = 0;
            sprintf(s, "%s%s: min rain %.3f in/hr, %d non-finite, precipitation %.3f in, "
                    "continuity %.3f %%", failMsg[0] ? "; " : "", deck[d], minRain,
                    nBad, precip, contErr);
            if (rc) sprintf(s + strlen(s), ", run error %d", rc);
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
