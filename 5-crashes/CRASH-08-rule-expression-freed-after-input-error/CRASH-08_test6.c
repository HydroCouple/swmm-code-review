/*
 * CRASH-08 for 6.0.0: closing a project that has a control-rule EXPRESSION
 * after an input error found while objects were counted.
 *
 * 6.0.0 keeps expressions in a std::vector inside ControlEngine, so there is
 * no count/array mismatch to crash on; this test is expected to pass
 * unpatched. It uses only the duplicate-ID deck: 6.0.0 does not reject
 * FLOW_UNITS BOGUS at all (a separate input-validation issue), so the
 * bad-option deck tells nothing about this crash.
 *
 * Correct behaviour: opening/initializing CRASH-08_expr-duplicate-id.inp
 * fails with ERROR 207 (duplicate ID J1) in the report, and closing and
 * destroying the engine returns cleanly.
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"

static int rptHas(const char *rpt, const char *text)
{
    char line[512];
    int found = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return 0;
    while (!found && fgets(line, sizeof line, f)) found = strstr(line, text) != NULL;
    fclose(f);
    return found;
}

int main(void)
{
    int rc, ok;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "CRASH-08_expr-duplicate-id.inp", "CRASH-08_dup6.rpt",
                          "CRASH-08_dup6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    printf("CRASH-08_expr-duplicate-id.inp  open/initialize returned %d; ", rc);
    fflush(stdout);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    ok = rc != 0 && rptHas("CRASH-08_dup6.rpt", "ERROR 207");
    printf("engine closed; report %s \"ERROR 207\"\n", ok ? "has" : "does NOT have");
    if (!ok)
    {
        printf("FAIL: the duplicate ID was not reported\n");
        return 1;
    }
    printf("PASS: the input error is reported and the engine closes cleanly\n");
    return 0;
}
