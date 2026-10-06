/*
 * CRASH-14 for 6.0.0: unknown type codes.
 *
 * 6.0.0 has no generic swmm_getCount(objType) / swmm_getIndex(objType, id);
 * counts and lookups are per object type (swmm_node_count, swmm_node_index,
 * ...), so an object-type code cannot index past an array. The one engine
 * function that takes an element-type code with an index is
 * swmm_forcing_clear(engine, type, idx). The test checks that the typed
 * lookups give the right answers for the deck (2 nodes, 1 link) and that
 * swmm_forcing_clear refuses type codes 18, 50, 99 and -1 with an error and
 * no memory error.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_forcing.h"

int main(void)
{
    static const int badType[] = { 18, 50, 99, -1 };
    SWMM_Engine e = swmm_engine_create();
    int i, rc, bad = 0;
    double t = 0.0;

    rc = swmm_engine_open(e, "CRASH-14_model.inp", "CRASH-14_6.rpt", "CRASH-14_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 0);
    if (!rc) rc = swmm_engine_step(e, &t);
    if (rc)
    {
        printf("FAIL: could not start the run (error %d)\n", rc);
        return 1;
    }

    printf("swmm_node_count          = %d (deck has 2)\n", swmm_node_count(e));
    printf("swmm_link_count          = %d (deck has 1)\n", swmm_link_count(e));
    printf("swmm_node_index(\"O1\")    = %d (expected 1)\n", swmm_node_index(e, "O1"));
    if (swmm_node_count(e) != 2 || swmm_link_count(e) != 1 || swmm_node_index(e, "O1") != 1)
        bad++;

    for (i = 0; i < 4; i++)
    {
        rc = swmm_forcing_clear(e, badType[i], 0);
        printf("swmm_forcing_clear(type %3d) = %d\n", badType[i], rc);
        if (rc == 0) bad++;
    }
    swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);

    if (bad)
    {
        printf("FAIL: %d checks accepted an unknown type or gave a wrong count/index\n", bad);
        return 1;
    }
    printf("PASS: unknown type codes are refused and typed counts/lookups are right\n");
    return 0;
}
