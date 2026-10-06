/*
 * API-03 for 6.0.0: do the error codes the API returns have a message?
 *
 * The legacy test checks that each toolkit error code has message text. Here
 * the same is checked for the codes 6.0.0's C API returns when it is misused:
 * a lifecycle call in the wrong state, a NULL handle, an object index out of
 * range and a missing input file. For each, swmm_error_message(code) must be
 * a real message (not empty and not its "Unknown error" fallback), and for
 * an engine-level refusal swmm_get_last_error/_msg must report the same code
 * with a non-empty message. Each check uses its own engine handle.
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

static int bad = 0, n = 0;

static void check(const char *what, int rc, SWMM_Engine e)
{
    const char *m = swmm_error_message(rc);
    int ok = rc != 0 && m && *m && strcmp(m, "Unknown error") != 0;
    printf("%-40s rc %3d  swmm_error_message: \"%s\"\n", what, rc, m ? m : "(null)");
    if (e)
    {
        const char *last = swmm_get_last_error_msg(e);
        int lastCode = swmm_get_last_error(e);
        printf("%-40s        swmm_get_last_error %d: \"%s\"\n", "", lastCode, last ? last : "(null)");
        if (lastCode != rc || !last || !*last) ok = 0;
    }
    n++;
    if (!ok) bad++;
}

int main(void)
{
    SWMM_Engine e;
    double t = 0.0, d = 0.0;
    int rc;

    /* lifecycle: step on an engine that has not been opened */
    e = swmm_engine_create();
    rc = swmm_engine_step(e, &t);
    check("swmm_engine_step before open", rc, e);
    swmm_engine_destroy(e);

    /* NULL handle */
    rc = swmm_engine_initialize(NULL);
    check("swmm_engine_initialize(NULL)", rc, NULL);

    /* index out of range on an open model */
    e = swmm_engine_create();
    rc = swmm_engine_open(e, "API-03_model.inp", "API-03_6.rpt", "API-03_6.out", NULL);
    if (rc)
    {
        printf("FAIL: could not open the deck (error %d)\n", rc);
        return 1;
    }
    rc = swmm_node_get_depth(e, 999, &d);
    check("swmm_node_get_depth(index 999)", rc, NULL);
    swmm_engine_close(e);
    swmm_engine_destroy(e);

    /* missing input file */
    e = swmm_engine_create();
    rc = swmm_engine_open(e, "API-03_missing.inp", "API-03_m.rpt", "API-03_m.out", NULL);
    check("swmm_engine_open(missing file)", rc, NULL);
    swmm_engine_destroy(e);

    if (bad)
    {
        printf("FAIL: %d of %d error codes returned by the API have no message\n", bad, n);
        return 1;
    }
    printf("PASS: every error code checked (%d) has a message\n", n);
    return 0;
}
