/*
 * NUM-01: a conduit with no water gets a NaN pollutant concentration.
 *
 * findLinkQual() (KINWAVE/DYNWAVE) and findSFLinkQual() (STEADY) end with a
 * block, added in 5.3.0 for the API link pollutant flux, that converts the
 * flux to a concentration with  flux * tStep / (v1 + qIn * tStep)  and books
 * it back as a mass with  * (v1 + qIn * tStep).  For a conduit that holds no
 * water and receives none (v1 = 0, qIn = 0) the default zero flux gives
 * 0/0 = NaN, which is written into the link's concentration, its load total
 * and the External Inflow term of the quality mass balance.
 *
 * Correct behaviour: a dry conduit with no inflow holds no pollutant. Every
 * pollutant value in the binary output must be finite, and the Quality
 * Routing Continuity table must hold numbers that balance: the error
 * recomputed from its own lines, 100 * (in - out) / in, within 1 %
 * (the decks give -0.2 % to 0 % when correct; the bug gives NaN).
 *
 * Decks: a minimal KINWAVE and STEADY network whose conduits are dry for the
 * first 2 hours (C3 for the whole run), and EPA's Example1.inp unchanged.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

static const char *decks[] = {"NUM-01_dry-branch-kw.inp", "NUM-01_dry-branch-sf.inp",
                              "NUM-01_example1.inp"};

/* Count non-finite numbers among all computed results of a binary .out file
   (subcatchment, node, link and system variables of every reporting period). */
static long out_nonfinite(const char *path, long *total)
{
    FILE *f = fopen(path, "rb");
    int trailer[6];
    long size, per, k, nfl, bad = 0;
    *total = 0;
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, size - 24, SEEK_SET);
    if (fread(trailer, 4, 6, f) != 6 || trailer[3] <= 0) { fclose(f); return -1; }
    /* trailer: id pos, props pos, results pos, periods, error, magic;
       each period = 8-byte date + nfl 4-byte floats */
    nfl = ((size - 24 - trailer[2]) / trailer[3] - 8) / 4;
    fseek(f, trailer[2], SEEK_SET);
    for (per = 0; per < trailer[3]; per++)
    {
        double date;
        float v;
        if (fread(&date, 8, 1, f) != 1) break;
        for (k = 0; k < nfl; k++)
        {
            if (fread(&v, 4, 1, f) != 1) break;
            if (!isfinite(v)) bad++;
            (*total)++;
        }
    }
    fclose(f);
    return bad;
}

/* First-pollutant value of a line of the Quality Routing Continuity table */
static double qual_line(const char *rpt, const char *label)
{
    FILE *f = fopen(rpt, "r");
    char line[512];
    int inTable = 0;
    double v = 0.0;
    if (!f) return NAN;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Quality Routing Continuity")) inTable = 1;
        if (inTable && strstr(line, label))
        {
            char *p = strstr(line, "..");   /* skip the dot leader */
            if (p) { while (*p == '.') p++; sscanf(p, "%lf", &v); }
            break;
        }
    }
    fclose(f);
    return v;
}

int main(void)
{
    int i, nbad = 0;
    printf("Deck                        NaN values   Wet weather  External  External  Final    Error from  Reported\n");
    printf("                            in .out      inflow       outflow   inflow    stored   the table   error\n");
    printf("                                         (lb, first pollutant)                     (%%)         (%%)\n");
    for (i = 0; i < 3; i++)
    {
        double t = 0.0, in, out, err, rep, wwIn, exOut, exIn, fin;
        long nonfin, total;
        int rc, bad;

        rc = swmm_open(decks[i], "NUM-01.rpt", "NUM-01.out");
        if (!rc) rc = swmm_start(1);
        while (!rc)
        {
            rc = swmm_step(&t);
            if (t <= 0.0) break;
        }
        swmm_end();
        swmm_report();
        swmm_close();

        nonfin = out_nonfinite("NUM-01.out", &total);
        exIn = qual_line("NUM-01.rpt", "External Inflow");
        wwIn = qual_line("NUM-01.rpt", "Wet Weather Inflow");
        exOut = qual_line("NUM-01.rpt", "External Outflow");
        fin  = qual_line("NUM-01.rpt", "Final Stored Mass");
        in  = qual_line("NUM-01.rpt", "Dry Weather Inflow") + wwIn +
              qual_line("NUM-01.rpt", "Groundwater Inflow") + qual_line("NUM-01.rpt", "RDII Inflow") +
              exIn + qual_line("NUM-01.rpt", "Initial Stored Mass");
        out = exOut + qual_line("NUM-01.rpt", "Flooding Loss") +
              qual_line("NUM-01.rpt", "Exfiltration Loss") + qual_line("NUM-01.rpt", "Mass Reacted") + fin;
        err = 100.0 * (in - out) / in;
        rep = qual_line("NUM-01.rpt", "Continuity Error");

        bad = rc || nonfin != 0 || !(fabs(err) < 1.0);
        if (bad) nbad++;
        printf("%-26s  %4ld/%-6ld  %10.3f  %8.3f  %8.3f  %7.3f  %10.3f  %8.3f%s\n", decks[i], nonfin,
               total, wwIn, exOut, exIn, fin, err, rep, bad ? "  <-- wrong" : "");
        if (rc) printf("    run stopped with error %d\n", rc);
    }
    if (nbad)
    {
        printf("FAIL: in %d of 3 decks dry conduits get NaN concentrations and the quality "
               "mass balance holds NaN (while the reported continuity error looks fine)\n", nbad);
        return 1;
    }
    printf("PASS: all output values are finite and the quality mass balance closes in every deck\n");
    return 0;
}
