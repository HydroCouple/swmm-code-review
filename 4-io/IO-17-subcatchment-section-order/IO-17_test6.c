/*
 * IO-17 for 6.0.0: the same check as IO-17_test.c through the 6.0.0 API.
 *
 * The input reference says the sections of an input file may appear in any
 * order. The four decks hold the same one-subcatchment model (10 ac, 50 %
 * impervious, 25 % of it without depression storage, impervious runoff 50 %
 * routed over the pervious area, Horton infiltration, and [ADJUSTMENTS]
 * patterns that halve S1's pervious n, depression storage and infiltration
 * rate), with the sections in different orders:
 *   IO-17_normal-order.inp        every section below [SUBCATCHMENTS]
 *   IO-17_subareas-first.inp      [SUBAREAS] above [SUBCATCHMENTS]
 *   IO-17_infiltration-first.inp  [INFILTRATION] above [SUBCATCHMENTS]
 *   IO-17_adjustments-first.inp   [ADJUSTMENTS] above [SUBCATCHMENTS] and
 *                                 [PATTERNS] at the end (6.0.0's writer order)
 * 6.0.0's [SUBAREAS], [INFILTRATION] and [ADJUSTMENTS] handlers skip a row
 * whose subcatchment (or pattern) has not been read yet, without an error.
 *
 * Correct: all four decks give the same Runoff Quantity Continuity. The test
 * runs each deck and compares the infiltration, surface runoff and final
 * storage (inches, from the report) with the reference order.
 *
 * Tolerance: 0.002 in. The report prints 3 decimals and the decks do the same
 * arithmetic when the order has no effect; the order bugs change the numbers
 * by 0.1 in or more.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

#define NVARS 3
static const char *labels[NVARS] = {"Infiltration Loss", "Surface Runoff", "Final Storage"};

/* reads the depth column (last number, inches) of the runoff continuity table */
static int readContinuity(const char *rpt, double v[NVARS])
{
    char line[256];
    int i, found = 0, inTable = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Runoff Quantity Continuity")) inTable = 1;
        else if (inTable && strstr(line, "Continuity Error")) break;
        if (!inTable) continue;
        for (i = 0; i < NVARS; i++)
        {
            char *p = strstr(line, labels[i]);
            if (p)
            {
                double a, b;
                p = strstr(p, ". ");
                while (p && *p == '.') p++;
                if (p && sscanf(p, "%lf %lf", &a, &b) == 2) { v[i] = b; found++; }
            }
        }
    }
    fclose(f);
    return found == NVARS;
}

int main(void)
{
    const char *decks[4] = {"normal-order", "subareas-first", "infiltration-first",
                            "adjustments-first"};
    double v[4][NVARS];
    char inp[64], rpt[64], out[64];
    int d, i, err, nbad = 0;
    double worstDiff = 0.0;
    char badList[128] = "";

    for (d = 0; d < 4; d++)
    {
        snprintf(inp, sizeof inp, "IO-17_%s.inp", decks[d]);
        snprintf(rpt, sizeof rpt, "IO-17_%s6.rpt", decks[d]);
        snprintf(out, sizeof out, "IO-17_%s6.out", decks[d]);
        err = swmm_engine_run(inp, rpt, out, NULL);
        if (err || !readContinuity(rpt, v[d]))
        {
            printf("FAIL: %s did not run (error %d) or its report has no continuity table\n",
                   inp, err);
            return 1;
        }
    }

    printf("Runoff continuity of S1 (inches):\n");
    printf("  %-20s %13s %13s %13s\n", "deck", "infiltration", "runoff", "final storage");
    for (d = 0; d < 4; d++)
    {
        int differs = 0;
        printf("  %-20s %13.3f %13.3f %13.3f\n", decks[d], v[d][0], v[d][1], v[d][2]);
        for (i = 0; i < NVARS; i++)
        {
            double diff = fabs(v[d][i] - v[0][i]);
            if (diff > 0.002) differs = 1;
            if (diff > worstDiff) worstDiff = diff;
        }
        if (differs)
        {
            if (nbad++) strcat(badList, ", ");
            strcat(badList, decks[d]);
        }
    }
    printf("  (correct: every row equals normal-order)\n");

    if (nbad)
    {
        printf("FAIL: the results depend on the section order: %s differ%s from "
               "normal-order by up to %.3f in\n", badList, nbad > 1 ? "" : "s", worstDiff);
        return 1;
    }
    printf("PASS: the subcatchment gives the same results whatever the section order\n");
    return 0;
}
