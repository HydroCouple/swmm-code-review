/*
 * CON-20: a vegetative swale in a Horton (or Curve Number) subcatchment
 * infiltrates at the pervious area's infiltration rate times the pervious
 * fraction of the subcatchment, instead of at the pervious area's rate.
 *
 * Reference Manual Vol. III, 6.2.8: the swale's infiltration rate f1 "is
 * the same value computed for the pervious area of the subcatchment by
 * SWMM's runoff module". Each deck has a 10-acre subcatchment with a
 * constant Horton capacity of 1 in/hr (MaxRate = MinRate = 1) and a
 * 20000 ft2 swale that takes all the impervious runoff. 3 in/hr of rain
 * falls for 2 hours, so the pervious area infiltrates at 1 in/hr and the
 * swale is full at 01:00. The swale's surface infiltration rate in its LID
 * report file at 01:00 must then be 1.0 in/hr, whatever the imperviousness:
 *
 *     50 % impervious, 90 % impervious, and 100 % impervious (no pervious
 *     area: SWMM then evaluates the infiltration model directly)
 *
 * Without the fix the rate is 1.0 x (1 - imperviousness): 0.5 and 0.1 in/hr.
 * The tolerance of 0.05 in/hr is far below that error and above the
 * 3-decimal rounding of the report file.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "swmm5.h"

/* surface infiltration, storage exfiltration and surface level on the
   LID report file line for the given time; returns 0 if not found */
static int lidLine(const char* fname, const char* time,
                   double* infil, double* exfil, double* level)
{
    char line[1024];
    char* tok[16];
    int n, found = 0;
    FILE* f = fopen(fname, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        if (!strstr(line, time) || line[3] != '/') continue;
        n = 0;
        tok[n] = strtok(line, " \t\r\n");
        while (tok[n] && n < 15) tok[++n] = strtok(NULL, " \t\r\n");
        /* date, time, elapsed, inflow, evap, surface infil, pave perc,
           soil perc, storage exfil, surface runoff, drain, surface level */
        if (n >= 12)
        {
            *infil = atof(tok[5]);
            *exfil = atof(tok[8]);
            *level = atof(tok[11]);
            found = 1;
        }
        break;
    }
    fclose(f);
    return found;
}

static int run(const char* inp, const char* rpt, const char* out)
{
    double elapsed = 0.0;
    int err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    return err;
}

int main(void)
{
    const int imperv[3] = {50, 90, 100};
    const double expected = 1.0;   /* in/hr, Horton capacity of the soil */
    char inp[64], rpt[64], out[64], lid[64], msg[256] = "";
    double infil, exfil, level;
    int k, err, bad = 0;

    printf("Swale at 01:00, pervious area infiltrating at %.3f in/hr\n", expected);
    printf("%%Imperv  swale infil (in/hr)  exfil (in/hr)  level (in)\n");
    for (k = 0; k < 3; k++)
    {
        sprintf(inp, "CON-20_imperv%d.inp", imperv[k]);
        sprintf(rpt, "CON-20_imperv%d.rpt", imperv[k]);
        sprintf(out, "CON-20_imperv%d.out", imperv[k]);
        sprintf(lid, "CON-20_lid%d.txt", imperv[k]);
        err = run(inp, rpt, out);
        if (err || !lidLine(lid, "01:00:00", &infil, &exfil, &level))
        {
            printf("%7d  run error %d or no LID report line\n", imperv[k], err);
            if (!bad) sprintf(msg, "%d %% impervious: run error %d or no "
                              "LID report line at 01:00", imperv[k], err);
            bad = 1;
            continue;
        }
        printf("%7d  %19.3f  %13.3f  %10.3f\n", imperv[k], infil, exfil, level);
        if (fabs(infil - expected) > 0.05 && !bad)
        {
            sprintf(msg, "at %d %% impervious the swale infiltrates %.3f in/hr "
                    "while the pervious area infiltrates %.3f in/hr",
                    imperv[k], infil, expected);
            bad = 1;
        }
    }
    if (bad)
    {
        printf("FAIL: %s\n", msg);
        return 1;
    }
    printf("PASS: the swale infiltrates at the pervious area's rate at every "
           "imperviousness\n");
    return 0;
}
