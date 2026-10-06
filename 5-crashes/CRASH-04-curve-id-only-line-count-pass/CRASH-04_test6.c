/*
 * CRASH-04 for 6.0.0: the same two decks as CRASH-04_test.c.
 *
 * 6.0.0 has no separate count pass, so neither deck crashes. Correct
 * behaviour, as for the fixed 5.3.0:
 * 1. the shape curve with the quoted type "Shape" builds the 2 x 2 ft closed
 *    square of conduit C1: full area 4 ft2, full hydraulic radius 0.5 ft,
 *    and the model runs;
 * 2. the [CURVES] line with only the curve name is rejected with ERROR 203
 *    (too few items), as legacy table_readCurve() intends.
 * Tolerance 0.1 %: the custom-shape tables are exact for a constant width.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_xsect.h"

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
    int rc, ok1, ok2, c1;
    double t = 0.0, yFull = 0, aFull = 0, rFull = 0, wMax = 0, sFull = 0, aMax = 0;
    SWMM_XSect xs = NULL;
    SWMM_Engine e = swmm_engine_create();

    printf("CRASH-04_quoted-shape.inp:\n");
    rc = swmm_engine_open(e, "CRASH-04_quoted-shape.inp", "CRASH-04_shape6.rpt",
                          "CRASH-04_shape6.out", NULL);
    printf("    swmm_engine_open returned %d\n", rc);
    if (!rc)
    {
        c1 = swmm_link_index(e, "C1");
        if (swmm_link_create_xsect(e, c1, &xs) == 0)
        {
            swmm_xsect_full_properties(xs, &yFull, &aFull, &rFull, &wMax, &sFull, &aMax);
            swmm_xsect_free(xs);
        }
        rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 1);
        while (!rc) { rc = swmm_engine_step(e, &t); if (t <= 0) break; }
        if (!rc) rc = swmm_engine_end(e);
        printf("    C1 full area %.4f ft2 (expected 4), full hydraulic radius %.4f ft "
               "(expected 0.5), run returned %d\n", aFull, rFull, rc);
    }
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    ok2 = (rc == 0) && fabs(aFull - 4.0) <= 0.004 && fabs(rFull - 0.5) <= 0.0005;

    printf("CRASH-04_curve-id-only.inp:\n");
    e = swmm_engine_create();
    rc = swmm_engine_open(e, "CRASH-04_curve-id-only.inp", "CRASH-04_idonly6.rpt",
                          "CRASH-04_idonly6.out", NULL);
    printf("    swmm_engine_open returned %d\n", rc);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    ok1 = (rc != 0) && reportHas("CRASH-04_idonly6.rpt", "ERROR 203");

    if (!ok1 || !ok2)
    {
        printf("FAIL: %s%s%s\n",
               ok1 ? "" : "the name-only curve line is not reported as ERROR 203",
               (!ok1 && !ok2) ? "; " : "",
               ok2 ? "" : "the quoted Shape curve does not give the expected section");
        return 1;
    }
    printf("PASS: the name-only curve line gives ERROR 203 and the quoted Shape curve "
           "builds the 2 x 2 ft section\n");
    return 0;
}
