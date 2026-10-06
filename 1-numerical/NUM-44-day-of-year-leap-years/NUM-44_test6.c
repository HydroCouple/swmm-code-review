/*
 * NUM-44 for 6.0.0: the same check as NUM-44_test.c through the 6.0.0 C API.
 * OptionsHandler.cpp (SWEEP_START/END) and Controls.cpp (DAYOFYEAR) convert
 * month/day with a non-leap year, as legacy does, and compare with
 * datetime::dayOfYear() of the current date.
 *
 * NUM-44: month/day dates for SWEEP_START/SWEEP_END and the DAYOFYEAR rule
 * premise are turned into day numbers with the non-leap year 1947 and then
 * compared with the day of year of the current date. In a leap year every
 * date after Feb 28 is matched one day early, and Dec 31 (day 366) is never
 * matched.
 *
 * Four decks, a leap year (2020) and a non-leap year (2021) each:
 *   *-mar.inp  02/28 00:30 to 03/01 12:00, sweeping season 01/01 to 03/01
 *   *-dec.inp  12/30 00:30 to 12/31 12:00, default season (01/01 to 12/31)
 * Rules: R1 DAYOFYEAR >= 03/01 sets OR1 to 0.5, R2 DAYOFYEAR = 12/31 sets
 * OR2 to 0.5 (1.0 otherwise). TSS builds up at 1 lb/ac/day on 1 ac and is
 * swept daily (100% removal) inside the season; no rain.
 *
 * The calendar gives, in both years:
 *   mar: OR1 goes to 0.5 on 03/01; OR2 never does; sweeps on 03/01 (and on
 *        02/29 in 2020), removing all buildup up to the last sweep:
 *        2 days = 2 lb in 2020, 1 day = 1 lb in 2021
 *   dec: OR2 is 0.5 on 12/31 only; one sweep on 12/31 removes 1 lb
 * Removal is checked to +-0.25 lb: the step alignment adds at most ~0.1 lb
 * (1-2 hourly steps of buildup), and one missing or extra day is 1 lb.
 * Settings are classed by the date at the start of the step, when the rules
 * were evaluated.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

/* SWMM DateTime (days since 12/30/1899) -> "MM/DD/YYYY" */
static void date_str(double d, char *s)
{
    long z = (long)floor(d) - 25569 + 719468;   /* days since 0000-03-01 */
    long era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    long day = doy - (153 * mp + 2) / 5 + 1;
    long mon = mp < 10 ? mp + 3 : mp - 9;
    long yr = yoe + era * 400 + (mon <= 2);
    sprintf(s, "%02ld/%02ld/%04ld", mon, day, yr);
}

/* value of "Sweeping Removal" in the report, or -1 */
static double sweep_removal(const char *rpt)
{
    char line[256];
    double v = -1.0;
    FILE *f = fopen(rpt, "r");
    if (!f) return -1.0;
    while (fgets(line, sizeof line, f))
    {
        char *p = strstr(line, "Sweeping Removal");
        if (!p) continue;
        p += strlen("Sweeping Removal");
        while (*p == ' ' || *p == '.') p++;
        if (sscanf(p, "%lf", &v) != 1) v = -1.0;
        break;
    }
    fclose(f);
    return v;
}

typedef struct
{
    const char *name, *inp, *rpt, *out;
    int mar;                   /* 1 = 02/28..03/01 deck, 0 = 12/30..12/31 */
    const char *or1Day;        /* expected first day OR1 = 0.5 (mar decks) */
    const char *or2Day;        /* expected only day OR2 = 0.5 ("never") */
    double removal;            /* expected sweeping removal (lb) */
} Case;

int main(void)
{
    Case c[4] = {
        {"2020 mar", "NUM-44_2020-mar.inp", "NUM-44_2020-mar_6.rpt",
         "NUM-44_2020-mar_6.out", 1, "03/01/2020", "never", 2.0},
        {"2021 mar", "NUM-44_2021-mar.inp", "NUM-44_2021-mar_6.rpt",
         "NUM-44_2021-mar_6.out", 1, "03/01/2021", "never", 1.0},
        {"2020 dec", "NUM-44_2020-dec.inp", "NUM-44_2020-dec_6.rpt",
         "NUM-44_2020-dec_6.out", 0, "-", "12/31/2020", 1.0},
        {"2021 dec", "NUM-44_2021-dec.inp", "NUM-44_2021-dec_6.rpt",
         "NUM-44_2021-dec_6.out", 0, "-", "12/31/2021", 1.0}};
    int i, ok = 1;
    char bad[512] = "";

    printf("          OR1 = 0.5 from    OR2 = 0.5 on            Sweeping removal (lb)\n");
    printf("Case      seen    expected  seen        expected    seen     expected\n");
    for (i = 0; i < 4; i++)
    {
        double elapsed = 0.0, before, removal;
        char or1[16] = "never", or2a[16] = "never", or2b[16] = "never", d[16];
        char or2seen[40];
        double s1 = 0.0, s2 = 0.0, start = 0.0, secs = 0.0;
        int k1, k2, err, good;
        SWMM_Engine e = swmm_engine_create();

        err = swmm_engine_open(e, c[i].inp, c[i].rpt, c[i].out, NULL);
        if (!err) err = swmm_engine_initialize(e);
        if (!err) err = swmm_engine_start(e, 1);
        k1 = swmm_link_index(e, "OR1");
        k2 = swmm_link_index(e, "OR2");
        if (!err) err = swmm_get_start_time(e, &start);  /* DateTime */
        before = start;
        while (!err)
        {
            err = swmm_engine_step(e, &elapsed);
            if (err || elapsed <= 0.0) break;
            date_str(before, d);                 /* date the rules saw */
            swmm_link_get_target_setting(e, k1, &s1);
            swmm_link_get_target_setting(e, k2, &s2);
            if (s1 == 0.5 && strcmp(or1, "never") == 0) strcpy(or1, d);
            if (s2 == 0.5)
            {
                if (strcmp(or2a, "never") == 0) strcpy(or2a, d);
                strcpy(or2b, d);
            }
            swmm_get_current_time(e, &secs);     /* seconds since start */
            before = start + secs / 86400.0;
        }
        if (!err) err = swmm_engine_end(e);
        if (!err) err = swmm_engine_report(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        if (err)
        {
            printf("FAIL: %s stopped with error %d\n", c[i].inp, err);
            return 1;
        }
        removal = sweep_removal(c[i].rpt);

        if (strcmp(or2a, or2b) == 0) sprintf(or2seen, "%.5s", or2a);
        else sprintf(or2seen, "%.5s-%.5s", or2a, or2b);
        printf("%-8s  %-5.5s   %-5.5s     %-11s %-5.5s       %6.3f   %6.3f\n",
               c[i].name, c[i].mar ? or1 : "-", c[i].mar ? c[i].or1Day : "-",
               or2seen, c[i].or2Day, removal, c[i].removal);

        good = 1;
        if (c[i].mar && strcmp(or1, c[i].or1Day) != 0)
        {
            good = 0;
            sprintf(bad + strlen(bad), "; %s: DAYOFYEAR >= 03/01 fired on %s",
                    c[i].name, or1);
        }
        if (strcmp(or2a, c[i].or2Day) != 0 || strcmp(or2b, c[i].or2Day) != 0)
        {
            good = 0;
            sprintf(bad + strlen(bad), "; %s: DAYOFYEAR = 12/31 held on %s",
                    c[i].name, or2seen);
        }
        if (fabs(removal - c[i].removal) > 0.25)
        {
            good = 0;
            sprintf(bad + strlen(bad), "; %s: sweeping removed %.3f lb, "
                    "not %.1f", c[i].name, removal, c[i].removal);
        }
        if (!good) ok = 0;
    }
    if (!ok)
    {
        printf("FAIL: month/day dates are matched on the wrong day%s\n", bad);
        return 1;
    }
    printf("PASS: DAYOFYEAR 03/01 and 12/31 and the sweeping season match the "
           "same calendar days in the leap and the non-leap year\n");
    return 0;
}
