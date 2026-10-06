/*
 * CRASH-16 for 6.0.0: saved results read with an out-of-range object index.
 *
 * 6.0.0 has no swmm_getSavedValue; saved results are read from the binary
 * output file through the separate reader in openswmm_output.h. The test runs
 * the same deck, opens its .out file and asks for a node, a link and a
 * subcatchment series (and a node's attributes) with index = count and -1.
 * Correct behaviour: those calls return an error (-1) without touching memory
 * outside the reader's arrays, and an index in range returns the saved result
 * (J1's depth at the first period, 0:15, is > 0).
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_output.h"

int main(void)
{
    SWMM_Output out;
    float series[4], attr[64];
    int rc, i, bad = 0, nNode, nLink, nSub, nPer, count = 0;

    rc = swmm_engine_run("CRASH-16_model.inp", "CRASH-16_6.rpt", "CRASH-16_6.out", NULL);
    if (rc)
    {
        printf("FAIL: the run failed (error %d)\n", rc);
        return 1;
    }
    out = swmm_output_open("CRASH-16_6.out");
    if (!out)
    {
        printf("FAIL: cannot open the output file\n");
        return 1;
    }
    nNode = swmm_output_get_node_count(out);
    nLink = swmm_output_get_link_count(out);
    nSub  = swmm_output_get_subcatch_count(out);
    nPer  = swmm_output_get_period_count(out);
    printf("output file: %d subcatchments, %d nodes, %d links, %d periods\n", nSub, nNode, nLink, nPer);

    rc = swmm_output_get_node_series(out, 0, SWMM_OUT_NODE_DEPTH, 0, 0, series);
    printf("depth of node 0 (%s) at the first period (0:15) = %.4f ft (rc %d)\n",
           swmm_output_get_node_id(out, 0), series[0], rc);
    if (rc != 0 || !(series[0] > 0.0f)) bad++;

    for (i = 0; i < 2; i++)
    {
        int n = i ? -1 : nNode, l = i ? -1 : nLink, s = i ? -1 : nSub;
        rc = swmm_output_get_node_series(out, n, SWMM_OUT_NODE_DEPTH, 0, 0, series);
        printf("node series,    index %2d -> rc %d\n", n, rc);
        if (rc == 0) bad++;
        rc = swmm_output_get_link_series(out, l, SWMM_OUT_LINK_FLOW, 0, 0, series);
        printf("link series,    index %2d -> rc %d\n", l, rc);
        if (rc == 0) bad++;
        rc = swmm_output_get_subcatch_series(out, s, SWMM_OUT_SUBCATCH_RUNOFF, 0, 0, series);
        printf("subcatch series, index %2d -> rc %d\n", s, rc);
        if (rc == 0) bad++;
        rc = swmm_output_get_node_attribute(out, n, 0, attr, &count);
        printf("node attribute, index %2d -> rc %d\n", n, rc);
        if (rc == 0) bad++;
    }
    swmm_output_close(out);

    if (bad)
    {
        printf("FAIL: %d reads with an out-of-range index succeeded (or the in-range read failed)\n", bad);
        return 1;
    }
    printf("PASS: out-of-range indices are refused and in-range ones return the saved result\n");
    return 0;
}
