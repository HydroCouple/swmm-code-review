/*
 * NUM-01 for 6.0.0: a conduit with no water gets a NaN pollutant concentration.
 *
 * In 5.3.0 the link API pollutant-flux block of findLinkQual() and
 * findSFLinkQual() computes flux * tStep / (v1 + qIn * tStep), which is
 * 0/0 = NaN for a conduit that holds and receives no water. 6.0.0 has no
 * link-level flux (only swmm_node_set_quality_mass_flux), so this test runs
 * the same three decks and is expected to pass unpatched.
 *
 * Correct behaviour, as in NUM-01_test.c: every pollutant value in the
 * binary output is finite, and the Quality Routing Continuity table balances:
 * the error recomputed from its own lines is within 1 %.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

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

        SWMM_Engine e = swmm_engine_create();
        rc = swmm_engine_open(e, decks[i], "NUM-01.rpt", "NUM-01.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 1);
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (t <= 0.0) break;
        }
        if (!rc) rc = swmm_engine_end(e);
        if (!rc) rc = swmm_engine_report(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);

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
