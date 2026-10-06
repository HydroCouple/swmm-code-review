/*
 * API-17 for 6.0.0: node and link type codes.
 *
 * In 5.3.0 the output reader's property getters return the integer type code
 * of a node or link as a float bit pattern (OUTFALL 1 -> 1.4e-45). 6.0.0's
 * output reader skips the property block and has no property getter; the
 * type codes are offered by the engine API as integers
 * (swmm_node_get_type / swmm_link_get_type, SWMM_NodeType / SWMM_LinkType).
 *
 * Correct behaviour: every node and link of the deck reports its type code:
 * J1, J2 = 0, O1 = 1, SU1 = 2, D1 = 3; C1-C3 = 0, P1 = 1, OR1 = 2, W1 = 3, OL1 = 4.
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

static int expected_type(int is_node, const char *id)
{
    if (is_node)
    {
        if (id[0] == 'J') return SWMM_NODE_JUNCTION;
        if (id[0] == 'O') return SWMM_NODE_OUTFALL;
        if (strncmp(id, "SU", 2) == 0) return SWMM_NODE_STORAGE;
        if (id[0] == 'D') return SWMM_NODE_DIVIDER;
    }
    else
    {
        if (id[0] == 'C') return SWMM_LINK_CONDUIT;
        if (id[0] == 'P') return SWMM_LINK_PUMP;
        if (strncmp(id, "OR", 2) == 0) return SWMM_LINK_ORIFICE;
        if (id[0] == 'W') return SWMM_LINK_WEIR;
        if (strncmp(id, "OL", 2) == 0) return SWMM_LINK_OUTLET;
    }
    return -1;
}

int main(void)
{
    SWMM_Engine e = swmm_engine_create();
    int rc, j, n, type, ok = 1;
    const char *id;

    rc = swmm_engine_open(e, "API-17_types.inp", "API-17_6.rpt", "API-17_6.out", NULL);
    if (rc)
    {
        printf("FAIL: swmm_engine_open returned %d\n", rc);
        return 1;
    }

    printf("  %-4s %8s %8s\n", "ID", "expected", "type");
    n = swmm_node_count(e);
    for (j = 0; j < n; j++)
    {
        type = -1;
        id = swmm_node_id(e, j);
        swmm_node_get_type(e, j, &type);
        printf("  %-4s %8d %8d\n", id, expected_type(1, id), type);
        if (type != expected_type(1, id)) ok = 0;
    }
    n = swmm_link_count(e);
    for (j = 0; j < n; j++)
    {
        type = -1;
        id = swmm_link_id(e, j);
        swmm_link_get_type(e, j, &type);
        printf("  %-4s %8d %8d\n", id, expected_type(0, id), type);
        if (type != expected_type(0, id)) ok = 0;
    }
    swmm_engine_close(e);
    swmm_engine_destroy(e);

    if (!ok)
    {
        printf("FAIL: a node or link type code is wrong\n");
        return 1;
    }
    printf("PASS: every node and link reports its integer type code\n");
    return 0;
}
