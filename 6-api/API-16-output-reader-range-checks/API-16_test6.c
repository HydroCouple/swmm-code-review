/*
 * API-16 for 6.0.0: out-of-range indices, periods and variables.
 *
 * The legacy SMO_* getters accept an element index equal to the element
 * count, an end period past the last period and any attribute index, and
 * return data from elsewhere in the file with error 0. 6.0.0's OutputReader
 * range-checks every argument (OutputReader.cpp, get_*_result / get_*_series /
 * get_*_attribute). Note the different conventions: in 6.0.0 a series' end
 * period is inclusive, "result" means one variable for all elements and
 * "attribute" means all variables of one element.
 *
 * Correct behaviour: every probe with an out-of-range argument returns -1;
 * the valid calls at the edges of the ranges return 0.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_output.h"

static int ok = 1;

static void row(const char *call, int rc, int expect_ok)
{
    printf("  %-50s %3d\n", call, rc);
    if (expect_ok ? rc != 0 : rc != -1) ok = 0;
}

int main(void)
{
    float v[256];
    int err, n = 0, nper, ns, nn, nl;
    char call[128];
    SWMM_Output out;

    err = swmm_engine_run("API-16_reader.inp", "API-16_6.rpt", "API-16_6.out", NULL);
    if (err)
    {
        printf("FAIL: the model run stopped with error %d\n", err);
        return 1;
    }
    out = swmm_output_open("API-16_6.out");
    if (!out)
    {
        printf("FAIL: swmm_output_open failed\n");
        return 1;
    }
    nper = swmm_output_get_period_count(out);
    ns = swmm_output_get_subcatch_count(out);
    nn = swmm_output_get_node_count(out);
    nl = swmm_output_get_link_count(out);
    printf("API-16_6.out: %d subcatchment, %d nodes, %d links, %d pollutants, %d periods\n\n",
           ns, nn, nl, swmm_output_get_pollut_count(out), nper);
    printf("  %-50s %3s\n", "call (out-of-range argument)", "rc");

    /* element index == element count */
    sprintf(call, "get_subcatch_series(index %d, runoff, 0, 3)", ns);
    row(call, swmm_output_get_subcatch_series(out, ns, SWMM_OUT_SUBCATCH_RUNOFF, 0, 3, v), 0);
    sprintf(call, "get_node_series(index %d, depth, 0, 3)", nn);
    row(call, swmm_output_get_node_series(out, nn, SWMM_OUT_NODE_DEPTH, 0, 3, v), 0);
    sprintf(call, "get_link_series(index %d, flow, 0, 3)", nl);
    row(call, swmm_output_get_link_series(out, nl, SWMM_OUT_LINK_FLOW, 0, 3, v), 0);
    sprintf(call, "get_subcatch_attribute(index %d, period 0)", ns);
    row(call, swmm_output_get_subcatch_attribute(out, ns, 0, v, &n), 0);
    sprintf(call, "get_node_attribute(index %d, period 0)", nn);
    row(call, swmm_output_get_node_attribute(out, nn, 0, v, &n), 0);
    sprintf(call, "get_link_attribute(index %d, period 0)", nl);
    row(call, swmm_output_get_link_attribute(out, nl, 0, v, &n), 0);

    /* end period past the last period (inclusive end: last valid is nper-1) */
    sprintf(call, "get_subcatch_series(0, runoff, %d, %d)", nper - 2, nper + 2);
    row(call, swmm_output_get_subcatch_series(out, 0, SWMM_OUT_SUBCATCH_RUNOFF, nper - 2, nper + 2, v), 0);
    sprintf(call, "get_node_series(0, depth, %d, %d)", nper - 2, nper + 2);
    row(call, swmm_output_get_node_series(out, 0, SWMM_OUT_NODE_DEPTH, nper - 2, nper + 2, v), 0);
    sprintf(call, "get_link_series(0, flow, %d, %d)", nper - 2, nper + 2);
    row(call, swmm_output_get_link_series(out, 0, SWMM_OUT_LINK_FLOW, nper - 2, nper + 2, v), 0);
    sprintf(call, "get_system_series(runoff, %d, %d)", nper - 2, nper + 2);
    row(call, swmm_output_get_system_series(out, SWMM_OUT_SYS_RUNOFF, nper - 2, nper + 2, v), 0);

    /* variable index == number of variables (first pollutant, none defined) */
    row("get_subcatch_series(0, pollutant 0, 0, 3)",
        swmm_output_get_subcatch_series(out, 0, SWMM_OUT_SUBCATCH_POLLUT_BASE, 0, 3, v), 0);
    row("get_node_series(0, pollutant 0, 0, 3)",
        swmm_output_get_node_series(out, 0, SWMM_OUT_NODE_POLLUT_BASE, 0, 3, v), 0);
    row("get_link_series(0, pollutant 0, 0, 3)",
        swmm_output_get_link_series(out, 0, SWMM_OUT_LINK_POLLUT_BASE, 0, 3, v), 0);
    row("get_system_series(variable 15, 0, 3)",
        swmm_output_get_system_series(out, SWMM_OUT_SYS_PET + 1, 0, 3, v), 0);
    row("get_subcatch_result(period 1, pollutant 0)",
        swmm_output_get_subcatch_result(out, 1, SWMM_OUT_SUBCATCH_POLLUT_BASE, v), 0);
    row("get_node_result(period 1, pollutant 0)",
        swmm_output_get_node_result(out, 1, SWMM_OUT_NODE_POLLUT_BASE, v), 0);
    row("get_link_result(period 1, pollutant 0)",
        swmm_output_get_link_result(out, 1, SWMM_OUT_LINK_POLLUT_BASE, v), 0);
    row("get_system_result(period 1, variable 15)",
        swmm_output_get_system_result(out, 1, SWMM_OUT_SYS_PET + 1, v), 0);

    printf("\n  %-50s %3s\n", "valid call (edge of range)", "rc");
    sprintf(call, "get_node_series(index %d, depth, 0, %d)", nn - 1, nper - 1);
    row(call, swmm_output_get_node_series(out, nn - 1, SWMM_OUT_NODE_DEPTH, 0, nper - 1, v), 1);
    sprintf(call, "get_link_attribute(index %d, period 0)", nl - 1);
    row(call, swmm_output_get_link_attribute(out, nl - 1, 0, v, &n), 1);
    row("get_system_result(period 1, PET)",
        swmm_output_get_system_result(out, 1, SWMM_OUT_SYS_PET, v), 1);

    swmm_output_close(out);

    if (!ok)
    {
        printf("FAIL: an out-of-range argument was accepted\n");
        return 1;
    }
    printf("PASS: every out-of-range index, end period and variable returned -1\n");
    return 0;
}
