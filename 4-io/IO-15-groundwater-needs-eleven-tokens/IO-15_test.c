/*
 * IO-15: a [GROUNDWATER] row with the 10 required fields is rejected with
 * ERROR 203 (too few items); the reader asks for 11.
 *
 * The input reference gives the format as
 *     Subcat Aquifer Node Esurf A1 B1 A2 B2 A3 Dsw (Egwt Ebot Egw Umc)
 * and says of Egwt: "Leave blank (or enter *) to use the elevation of the
 * receiving node's invert". So
 *     IO-15_ten-fields.inp   S1 AQ J1 10 0.05 1.5 0 0 0 0
 * must be accepted and must give the same result as
 *     IO-15_star.inp         S1 AQ J1 10 0.05 1.5 0 0 0 0 *
 * The test runs both decks and compares the Groundwater Flow volume of the
 * report's Groundwater Continuity table (the aquifer drains, so it is > 0).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

/* first number after the dotted leader of the first line containing label
   after the line containing section */
static int rptValue(const char* rpt, const char* section, const char* label,
                    double* v)
{
    char line[512];
    int inSection = 0;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        char* p;
        if (!inSection)
        {
            if (strstr(line, section)) inSection = 1;
            continue;
        }
        p = strstr(line, label);
        if (!p) continue;
        p += strlen(label);
        p += strspn(p, " .");
        fclose(f);
        return sscanf(p, "%lf", v) == 1;
    }
    fclose(f);
    return 0;
}

/* returns the error code of the run; *gw = Groundwater Flow (ac-ft) */
static int runDeck(const char* inp, const char* rpt, const char* out,
                   double* gw)
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
    if (!err && !rptValue(rpt, "Groundwater Continuity", "Groundwater Flow", gw))
        err = -1;
    return err;
}

int main(void)
{
    double gw10 = 0.0, gwStar = 0.0;
    int err10, errStar;

    err10 = runDeck("IO-15_ten-fields.inp", "IO-15_ten-fields.rpt",
                    "IO-15_ten-fields.out", &gw10);
    errStar = runDeck("IO-15_star.inp", "IO-15_star.rpt", "IO-15_star.out",
                      &gwStar);

    printf("[GROUNDWATER] row                    Result\n");
    if (err10) printf("10 fields                            error %d\n", err10);
    else printf("10 fields                            GW flow %.3f ac-ft\n", gw10);
    if (errStar) printf("10 fields + *                        error %d\n", errStar);
    else printf("10 fields + *                        GW flow %.3f ac-ft\n", gwStar);

    if (errStar || gwStar <= 0.0)
    {
        printf("FAIL: the reference deck with * did not run (error %d)\n",
               errStar);
        return 1;
    }
    if (err10)
    {
        printf("FAIL: a [GROUNDWATER] row with the 10 required fields is "
               "rejected (error %d; the report says ERROR 203)\n", err10);
        return 1;
    }
    if (fabs(gw10 - gwStar) > 0.0005)
    {
        printf("FAIL: leaving out Egwt gives %.3f ac-ft of GW flow, "
               "* gives %.3f\n", gw10, gwStar);
        return 1;
    }
    printf("PASS: a 10-field [GROUNDWATER] row is accepted and equals the "
           "row with * (%.3f ac-ft)\n", gw10);
    return 0;
}
