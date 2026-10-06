/*
 * IO-28: a transect is only finished (geometry tables built, roughness set)
 * when an NC line or a known section header follows it.
 *
 * Each deck has two IRREGULAR conduits, C1 on transect T1 and C2 on T2. Both
 * transects are trapezoids with a channel bottom at elevation 100 and banks
 * rising 5 ft to elevation 105, with the same Manning's n:
 *     T1: bottom 20 ft, top 40 ft -> depth 5 ft, area (20+40)/2*5 = 150 ft2, width 40 ft
 *     T2: bottom 40 ft, top 60 ft -> depth 5 ft, area (40+60)/2*5 = 250 ft2, width 60 ft
 * All four decks are valid input per the manual:
 *   IO-28_one-nc.inp          one NC line, then X1/GR for T1 and X1/GR for T2
 *                             ("the NC line is only needed when a transect has
 *                             different Manning's n values than the previous one")
 *   IO-28_transects-last.inp  [TRANSECTS] is the last section of the file
 *                             ("sections can appear in any arbitrary order")
 *   IO-28_unknown-section.inp [TRANSECTS] is followed by a section the engine
 *                             does not know. 5.3.0 skips unknown sections with
 *                             a warning; 5.2.4 rejects them with ERROR 205,
 *                             which is its documented rule and not this defect,
 *                             so that case is skipped when ERROR 205 is reported.
 *   IO-28_two-sections.inp    T1 in one [TRANSECTS] section, T2 in a second
 *                             [TRANSECTS] section at the end of the file
 *
 * Correct behaviour: every deck runs, and the Cross Section Summary of the
 * report shows the values above for C1 and C2. The values are printed with
 * two decimals, so a tolerance of 0.01 is used; the unfinished transect makes
 * the whole run fail with ERROR 113/119 or gives a conduit the other
 * transect's shape, so there is no near miss.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define NDECKS 4
static const char *Decks[NDECKS] = {
    "IO-28_one-nc", "IO-28_transects-last", "IO-28_unknown-section", "IO-28_two-sections"};
/* expected full depth, full area, max width of C1 (transect T1) and C2 (T2) */
static const double Expect[2][3] = {{5.0, 150.0, 40.0}, {5.0, 250.0, 60.0}};

/* find C1/C2 in the report's Cross Section Summary; returns how many were found */
static int readXsects(const char *rpt, double v[2][3])
{
    FILE *f = fopen(rpt, "r");
    char line[512], name[64], shape[64];
    double d, a, r, w;
    int in = 0, found = 0;
    if (!f) return 0;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Cross Section Summary")) { in = 1; continue; }
        if (!in) continue;
        if (sscanf(line, "%63s %63s %lf %lf %lf %lf", name, shape, &d, &a, &r, &w) == 6)
        {
            int k = strcmp(name, "C1") == 0 ? 0 : strcmp(name, "C2") == 0 ? 1 : -1;
            if (k >= 0) { v[k][0] = d; v[k][1] = a; v[k][2] = w; found++; }
            if (found == 2) break;
        }
    }
    fclose(f);
    return found;
}

/* print the report's ERROR lines, indented */
static void printErrors(const char *rpt)
{
    FILE *f = fopen(rpt, "r");
    char line[512], *p;
    if (!f) return;
    while (fgets(line, sizeof line, f))
    {
        if ((p = strstr(line, "ERROR")) == NULL) continue;
        printf("%-24s        %s", "", p);
    }
    fclose(f);
}

static int rptHas(const char *rpt, const char *text)
{
    FILE *f = fopen(rpt, "r");
    char line[512];
    int has = 0;
    if (!f) return 0;
    while (!has && fgets(line, sizeof line, f)) has = strstr(line, text) != NULL;
    fclose(f);
    return has;
}

int main(void)
{
    int i, k, nbad = 0;
    char inp[128], rpt[128], out[128], bad[512] = "";

    printf("%-24s %5s  %-4s %7s %7s %7s\n", "Deck", "Error", "Link", "Depth", "Area", "Width");
    printf("%-24s %5s  %-4s %7s %7s %7s\n", "", "", "", "(ft)", "(ft2)", "(ft)");
    for (i = 0; i < NDECKS; i++)
    {
        double v[2][3] = {{0}};
        int err, n, ok = 1;
        snprintf(inp, sizeof inp, "%s.inp", Decks[i]);
        snprintf(rpt, sizeof rpt, "%s.rpt", Decks[i]);
        snprintf(out, sizeof out, "%s.out", Decks[i]);
        err = swmm_open(inp, rpt, out);
        if (!err) err = swmm_start(1);
        while (!err)
        {
            double t = 0.0;
            err = swmm_step(&t);
            if (t <= 0.0) break;
        }
        swmm_end();
        if (!err) err = swmm_report();
        swmm_close();
        if (err && i == 2 && rptHas(rpt, "ERROR 205"))
        {
            printf("%-24s %5d  (unknown sections are rejected by this version: skipped)\n",
                   Decks[i] + 6, err);
            continue;
        }
        n = readXsects(rpt, v);
        if (n < 2)
        {
            printf("%-24s %5d  (run rejected; the report says:)\n", Decks[i] + 6, err);
            printErrors(rpt);
        }
        else for (k = 0; k < 2; k++)
        {
            printf("%-24s %5d  C%d   %7.2f %7.2f %7.2f%s\n", k ? "" : Decks[i] + 6, err, k + 1,
                   v[k][0], v[k][1], v[k][2],
                   fabs(v[k][1] - Expect[k][1]) > 0.01 ? "   <-- wrong shape" : "");
            if (fabs(v[k][0] - Expect[k][0]) > 0.01 || fabs(v[k][1] - Expect[k][1]) > 0.01
                || fabs(v[k][2] - Expect[k][2]) > 0.01) ok = 0;
        }
        if (err || n < 2 || !ok)
        {
            char msg[128];
            if (err)
                snprintf(msg, sizeof msg, "%s%s (error %d%s)", nbad ? ", " : "", Decks[i] + 6, err,
                         rptHas(rpt, "ERROR 119") ? ", ERROR 113/119 in the report" : "");
            else
                snprintf(msg, sizeof msg, "%s%s (runs, but a conduit has the wrong shape)",
                         nbad ? ", " : "", Decks[i] + 6);
            strncat(bad, msg, sizeof bad - strlen(bad) - 1);
            nbad++;
        }
    }
    if (nbad)
    {
        printf("FAIL: %d of %d valid decks rejected or with a wrong transect: %s\n",
               nbad, NDECKS, bad);
        return 1;
    }
    printf("PASS: every transect is built with its own shape, whatever line or section "
           "follows it\n");
    return 0;
}
