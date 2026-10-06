/*
 * CRASH-04: the object-count pass of the input reader reads a curve's type
 * token with strtok(), without the NULL check and the quote handling that the
 * read pass (getTokens) has.
 *
 * 1. CRASH-04_quoted-shape.inp: a shape curve whose type is written "Shape".
 *    The count pass does not see a shape curve, the Shape array is allocated
 *    with no elements, and project_validate() writes past it. Correct: the
 *    model opens and runs, and conduit C1 (CUSTOM, 2 ft, shape SH1 = a closed
 *    2 x 2 ft square: A = 4 ft2, R = 0.5 ft) has the Manning full flow
 *    1.486/0.013 * 4 * 0.5^(2/3) * sqrt(1/400) = 14.40 cfs.
 * 2. CRASH-04_curve-id-only.inp: a [CURVES] line with only the curve name.
 *    strtok() returns NULL for the type and findmatch()/match() dereference
 *    it. Correct: swmm_open() fails with ERROR 203 (too few items), which is
 *    what table_readCurve() reports for such a line.
 *
 * Tolerance: 0.1 % on the full flow. The custom-shape tables are exact for a
 * constant width, and the conduit slope differs from 1/400 by 2e-6.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define QFULL 14.4019   /* cfs, computed in the header comment */

static int reportHas(const char *rpt, const char *text)
{
    char line[256];
    int found = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return 0;
    while (!found && fgets(line, sizeof line, f))
        if (strstr(line, text))
        {
            found = 1;
            printf("    report: %s", line + strspn(line, " "));
        }
    fclose(f);
    return found;
}

int main(void)
{
    int err, ok1, ok2, c1;
    double qFull = 0.0, elapsed = 0.0;

    printf("CRASH-04_quoted-shape.inp:\n");
    fflush(stdout);
    err = swmm_open("CRASH-04_quoted-shape.inp", "CRASH-04_shape.rpt", "CRASH-04_shape.out");
    printf("    swmm_open returned %d\n", err);
    if (!err)
    {
        c1 = swmm_getIndex(swmm_LINK, "C1");
        qFull = swmm_getValue(swmm_LINK_FULLFLOW, c1);
        err = swmm_start(1);
        while (!err)
        {
            err = swmm_step(&elapsed);
            if (elapsed <= 0.0) break;
        }
        swmm_end();
        printf("    C1 full flow %.4f cfs (expected %.4f), run returned %d\n", qFull, QFULL, err);
    }
    swmm_close();
    ok2 = (err == 0) && fabs(qFull - QFULL) <= 0.001 * QFULL;

    printf("CRASH-04_curve-id-only.inp:\n");
    fflush(stdout);
    err = swmm_open("CRASH-04_curve-id-only.inp", "CRASH-04_idonly.rpt", "CRASH-04_idonly.out");
    swmm_close();
    printf("    swmm_open returned %d\n", err);
    ok1 = (err != 0) && reportHas("CRASH-04_idonly.rpt", "ERROR 203");

    if (!ok1 || !ok2)
    {
        printf("FAIL: %s%s%s\n",
               ok1 ? "" : "the name-only curve line is not reported as ERROR 203",
               (!ok1 && !ok2) ? "; " : "",
               ok2 ? "" : "the quoted Shape curve does not give the expected section");
        return 1;
    }
    printf("PASS: the name-only curve line gives ERROR 203 and the quoted Shape curve "
           "builds a 2 x 2 ft section (full flow %.2f cfs)\n", qFull);
    return 0;
}
