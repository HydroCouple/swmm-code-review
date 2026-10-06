/*
 * CRASH-02: an [XSECTIONS] row for a link whose own row has not been stored
 * writes Conduit[0].barrels; with no conduits that is a heap overflow.
 *
 * link_readXsectParams() tests Link[j].type == CONDUIT before setting the
 * conduit's barrels. Link[] is zero-filled, so a link whose own row comes
 * later in the file, or was rejected, has type 0 (CONDUIT) and sub-index 0.
 * With no conduits the Conduit array has no elements.
 *
 * 1. CRASH-02_orifice-bad-node.inp (standard section order): the orifice row
 *    names an undefined node. Correct: swmm_open() fails and the report
 *    gives "ERROR 209: undefined object O1x".
 * 2. CRASH-02_xsect-before-weir.inp: [XSECTIONS] above [WEIRS], which the
 *    input format allows. Correct: the model runs, and at the end of the run
 *    (steady state) the weir passes the 5 cfs inflow.
 *
 * Tolerance: 1 % on the weir flow; the inflow has been constant for 2 hours.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

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
    int err, ok1, ok2, w1;
    double elapsed = 0.0, q = 0.0;

    printf("CRASH-02_orifice-bad-node.inp:\n");
    fflush(stdout);
    err = swmm_open("CRASH-02_orifice-bad-node.inp", "CRASH-02_badnode.rpt", "CRASH-02_badnode.out");
    swmm_close();
    printf("    swmm_open returned %d\n", err);
    ok1 = (err != 0) && reportHas("CRASH-02_badnode.rpt", "ERROR 209: undefined object O1x");

    printf("CRASH-02_xsect-before-weir.inp:\n");
    fflush(stdout);
    err = swmm_open("CRASH-02_xsect-before-weir.inp", "CRASH-02_weir.rpt", "CRASH-02_weir.out");
    printf("    swmm_open returned %d\n", err);
    if (!err)
    {
        w1 = swmm_getIndex(swmm_LINK, "W1");
        err = swmm_start(1);
        while (!err)
        {
            err = swmm_step(&elapsed);
            if (elapsed <= 0.0) break;
            q = swmm_getValue(swmm_LINK_FLOW, w1);
        }
        swmm_end();
        printf("    W1 flow at the end %.4f cfs (inflow 5 cfs), run returned %d\n", q, err);
    }
    swmm_close();
    ok2 = (err == 0) && fabs(q - 5.0) <= 0.05;

    if (!ok1 || !ok2)
    {
        printf("FAIL: %s%s%s\n",
               ok1 ? "" : "the orifice with an undefined node is not reported as ERROR 209",
               (!ok1 && !ok2) ? "; " : "",
               ok2 ? "" : "the model with [XSECTIONS] above [WEIRS] does not run correctly");
        return 1;
    }
    printf("PASS: no overflow; the bad orifice row gives ERROR 209 and the weir model "
           "runs with [XSECTIONS] first\n");
    return 0;
}
