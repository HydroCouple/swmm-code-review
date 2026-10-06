/*
 * CRASH-02 for 6.0.0: the same two decks as CRASH-02_test.c.
 *
 * 6.0.0 applies [XSECTIONS] after all link sections, whatever their order in
 * the file, and never touches conduit data for a regulator. Correct
 * behaviour, as in CRASH-02_test.c: the orifice row with an undefined node is
 * reported as ERROR 209, and the weir model with [XSECTIONS] above [WEIRS]
 * runs and passes the 5 cfs inflow at the end (1 % tolerance).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

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
    int rc, ok1, ok2, w1;
    double t = 0.0, q = 0.0;
    SWMM_Engine e = swmm_engine_create();

    printf("CRASH-02_orifice-bad-node.inp:\n");
    rc = swmm_engine_open(e, "CRASH-02_orifice-bad-node.inp", "CRASH-02_badnode6.rpt",
                          "CRASH-02_badnode6.out", NULL);
    printf("    swmm_engine_open returned %d\n", rc);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    ok1 = (rc != 0) && reportHas("CRASH-02_badnode6.rpt", "ERROR 209: undefined object O1x");

    printf("CRASH-02_xsect-before-weir.inp:\n");
    e = swmm_engine_create();
    rc = swmm_engine_open(e, "CRASH-02_xsect-before-weir.inp", "CRASH-02_weir6.rpt",
                          "CRASH-02_weir6.out", NULL);
    printf("    swmm_engine_open returned %d\n", rc);
    if (!rc)
    {
        w1 = swmm_link_index(e, "W1");
        rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 1);
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (t <= 0) break;
            swmm_link_get_flow(e, w1, &q);
        }
        if (!rc) rc = swmm_engine_end(e);
        printf("    W1 flow at the end %.4f cfs (inflow 5 cfs), run returned %d\n", q, rc);
    }
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    ok2 = (rc == 0) && fabs(q - 5.0) <= 0.05;

    if (!ok1 || !ok2)
    {
        printf("FAIL: %s%s%s\n",
               ok1 ? "" : "the orifice with an undefined node is not reported as ERROR 209",
               (!ok1 && !ok2) ? "; " : "",
               ok2 ? "" : "the model with [XSECTIONS] above [WEIRS] does not run correctly");
        return 1;
    }
    printf("PASS: the bad orifice row gives ERROR 209 and the weir model runs with "
           "[XSECTIONS] first\n");
    return 0;
}
