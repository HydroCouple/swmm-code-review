/*
 * API-16: the output reader accepts out-of-range indices, periods and
 * attributes and returns data from the wrong place with error 0.
 *
 * The deck writes an output file with 1 subcatchment, 4 nodes (J1 J2 J3 O1),
 * 3 links (C1 C2 C3), no pollutants and 24 reporting periods. Each probe below
 * passes one argument that is out of range:
 *   - an element index equal to the element count (valid: 0 .. count-1);
 *     the series and result getters test "index > count";
 *   - an end period past the last period (series use an exclusive end, so
 *     endPeriod may be at most Nperiods); only "endPeriod <= startPeriod"
 *     is tested;
 *   - an attribute index equal to the number of variables per element (the
 *     first pollutant when there are no pollutants; 15 for the system, whose
 *     variables are 0 .. 14 although the enum stops at SMO_evap_rate = 13,
 *     14 being PET); never tested.
 *
 * Correct behaviour (header: "Error code 0 on success, -1 on failure or error
 * code"; messages.txt: 421 invalid parameter code, 422 reporting period index
 * out of range, 423 element index out of range): every probe returns a
 * non-zero error code. Three valid calls at the edges of the ranges must
 * still succeed. The reference rows at the end show where the bad reads
 * actually came from.
 */
#include <stdio.h>
#include "swmm5.h"
#include "swmm_output.h"

static int ok = 1;

static void row(const char *call, int err, float *v, int n, int expect_ok)
{
    int k;
    printf("  %-44s %5d  ", call, err);
    if (err == 0)
    {
        printf("len %2d:", n);
        for (k = 0; k < n && k < 5; k++) printf(" %.4g", v[k]);
        SMO_free((void **)&v);
    }
    printf("\n");
    if (expect_ok ? err != 0 : err == 0) ok = 0;
}

int main(void)
{
    SMO_Handle h = NULL;
    int err, n, nper = 0, *cnt = NULL, ns, nn, nl;
    double elapsed = 0.0;
    float *v;
    char call[128];

    /* run the deck to write API-16.out */
    err = swmm_open("API-16_reader.inp", "API-16.rpt", "API-16.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the model run stopped with error %d\n", err);
        return 1;
    }

    SMO_init(&h);
    err = SMO_open(h, "API-16.out");
    if (!err) err = SMO_getTimes(h, SMO_numPeriods, &nper);
    if (!err) err = SMO_getProjectSize(h, &cnt, &n);
    if (err)
    {
        printf("FAIL: could not open API-16.out (error %d)\n", err);
        return 1;
    }
    ns = cnt[0]; nn = cnt[1]; nl = cnt[2];
    printf("API-16.out: %d subcatchment, %d nodes, %d links, %d pollutants, %d periods\n\n",
           ns, nn, nl, cnt[4], nper);
    SMO_free((void **)&cnt);

    printf("  %-44s %5s  %s\n", "call (out-of-range argument)", "error", "data returned");

    /* element index == element count */
    v = NULL; err = SMO_getSubcatchSeries(h, ns, SMO_runoff_rate, 0, 4, &v, &n);
    sprintf(call, "getSubcatchSeries(index %d, runoff, 0, 4)", ns); row(call, err, v, n, 0);
    v = NULL; err = SMO_getNodeSeries(h, nn, SMO_invert_depth, 0, 4, &v, &n);
    sprintf(call, "getNodeSeries(index %d, depth, 0, 4)", nn); row(call, err, v, n, 0);
    v = NULL; err = SMO_getLinkSeries(h, nl, SMO_flow_rate_link, 0, 4, &v, &n);
    sprintf(call, "getLinkSeries(index %d, flow, 0, 4)", nl); row(call, err, v, n, 0);
    v = NULL; err = SMO_getSubcatchResult(h, 0, ns, &v, &n);
    sprintf(call, "getSubcatchResult(period 0, index %d)", ns); row(call, err, v, n, 0);
    v = NULL; err = SMO_getNodeResult(h, 0, nn, &v, &n);
    sprintf(call, "getNodeResult(period 0, index %d)", nn); row(call, err, v, n, 0);
    v = NULL; err = SMO_getLinkResult(h, 0, nl, &v, &n);
    sprintf(call, "getLinkResult(period 0, index %d)", nl); row(call, err, v, n, 0);

    /* end period past the last period */
    v = NULL; err = SMO_getSubcatchSeries(h, 0, SMO_runoff_rate, nper - 2, nper + 3, &v, &n);
    sprintf(call, "getSubcatchSeries(0, runoff, %d, %d)", nper - 2, nper + 3); row(call, err, v, n, 0);
    v = NULL; err = SMO_getNodeSeries(h, 0, SMO_invert_depth, nper - 2, nper + 3, &v, &n);
    sprintf(call, "getNodeSeries(0, depth, %d, %d)", nper - 2, nper + 3); row(call, err, v, n, 0);
    v = NULL; err = SMO_getLinkSeries(h, 0, SMO_flow_rate_link, nper - 2, nper + 3, &v, &n);
    sprintf(call, "getLinkSeries(0, flow, %d, %d)", nper - 2, nper + 3); row(call, err, v, n, 0);
    v = NULL; err = SMO_getSystemSeries(h, SMO_runoff_flow, nper - 2, nper + 3, &v, &n);
    sprintf(call, "getSystemSeries(runoff, %d, %d)", nper - 2, nper + 3); row(call, err, v, n, 0);

    /* attribute index == number of variables (first pollutant, none defined) */
    v = NULL; err = SMO_getSubcatchSeries(h, 0, SMO_pollutant_conc_subcatch, 0, 4, &v, &n);
    row("getSubcatchSeries(0, pollutant 0, 0, 4)", err, v, n, 0);
    v = NULL; err = SMO_getNodeSeries(h, 0, SMO_pollutant_conc_node, 0, 4, &v, &n);
    row("getNodeSeries(0, pollutant 0, 0, 4)", err, v, n, 0);
    v = NULL; err = SMO_getLinkSeries(h, 0, SMO_pollutant_conc_link, 0, 4, &v, &n);
    row("getLinkSeries(0, pollutant 0, 0, 4)", err, v, n, 0);
    v = NULL; err = SMO_getSystemSeries(h, (SMO_systemAttribute)15, 0, 4, &v, &n);
    row("getSystemSeries(attribute 15, 0, 4)", err, v, n, 0);
    v = NULL; err = SMO_getSubcatchAttribute(h, 1, SMO_pollutant_conc_subcatch, &v, &n);
    row("getSubcatchAttribute(period 1, pollutant 0)", err, v, n, 0);
    v = NULL; err = SMO_getNodeAttribute(h, 1, SMO_pollutant_conc_node, &v, &n);
    row("getNodeAttribute(period 1, pollutant 0)", err, v, n, 0);
    v = NULL; err = SMO_getLinkAttribute(h, 1, SMO_pollutant_conc_link, &v, &n);
    row("getLinkAttribute(period 1, pollutant 0)", err, v, n, 0);
#ifdef OPENSWMM_LEGACY_OUTPUT_H_
    /* 5.3.0 only: 5.2.4's SMO_getSystemAttribute() returns the address of a
       local variable (a separate defect, fixed in 5.3.0), which AddressSanitizer
       stops on before the range check matters */
    v = NULL; err = SMO_getSystemAttribute(h, 1, (SMO_systemAttribute)15, &v, &n);
    row("getSystemAttribute(period 1, attribute 15)", err, v, n, 0);
#endif

    /* valid calls at the edges of the ranges: must succeed */
    printf("\n  %-44s %5s  %s\n", "valid call (edge of range)", "error", "data returned");
    v = NULL; err = SMO_getNodeSeries(h, nn - 1, SMO_invert_depth, 0, nper, &v, &n);
    sprintf(call, "getNodeSeries(index %d, depth, 0, %d)", nn - 1, nper); row(call, err, v, n, 1);
    v = NULL; err = SMO_getLinkResult(h, 0, nl - 1, &v, &n);
    sprintf(call, "getLinkResult(period 0, index %d)", nl - 1); row(call, err, v, n, 1);
    v = NULL; err = SMO_getSystemSeries(h, (SMO_systemAttribute)14, 0, 4, &v, &n);
    row("getSystemSeries(attribute 14, 0, 4)", err, v, n, 1);

    /* where the bad reads land */
    printf("\n  %-44s %5s  %s\n", "reference (valid call)", "error", "data returned");
    v = NULL; err = SMO_getNodeSeries(h, 0, SMO_total_inflow, 0, 4, &v, &n);
    row("getNodeSeries(0, total inflow, 0, 4) [J1]", err, v, n, 1);
    v = NULL; err = SMO_getLinkSeries(h, 0, SMO_flow_rate_link, 0, 4, &v, &n);
    row("getLinkSeries(0, flow, 0, 4)         [C1]", err, v, n, 1);
    v = NULL; err = SMO_getSystemSeries(h, SMO_air_temp, 0, 4, &v, &n);
    row("getSystemSeries(air temp, 0, 4)", err, v, n, 1);
    v = NULL; err = SMO_getNodeResult(h, 0, 0, &v, &n);
    row("getNodeResult(period 0, index 0)     [J1]", err, v, n, 1);
    v = NULL; err = SMO_getLinkResult(h, 0, 0, &v, &n);
    row("getLinkResult(period 0, index 0)     [C1]", err, v, n, 1);
    v = NULL; err = SMO_getSystemResult(h, 0, 0, &v, &n);
    row("getSystemResult(period 0)", err, v, n, 1);
    v = NULL; err = SMO_getNodeSeries(h, 0, SMO_invert_depth, nper - 2, nper, &v, &n);
    sprintf(call, "getNodeSeries(0, depth, %d, %d)      [J1]", nper - 2, nper); row(call, err, v, n, 1);
    v = NULL; err = SMO_getNodeSeries(h, 0, SMO_invert_depth, 0, 4, &v, &n);
    row("getNodeSeries(0, depth, 0, 4)        [J1]", err, v, n, 1);
    v = NULL; err = SMO_getNodeSeries(h, 1, SMO_invert_depth, 0, 4, &v, &n);
    row("getNodeSeries(1, depth, 0, 4)        [J2]", err, v, n, 1);
    v = NULL; err = SMO_getLinkSeries(h, 1, SMO_flow_rate_link, 0, 4, &v, &n);
    row("getLinkSeries(1, flow, 0, 4)         [C2]", err, v, n, 1);
    v = NULL; err = SMO_getNodeAttribute(h, 1, SMO_invert_depth, &v, &n);
    row("getNodeAttribute(period 1, depth)", err, v, n, 1);

    SMO_close(&h);

    if (!ok)
    {
        printf("FAIL: out-of-range arguments returned error 0 and data from elsewhere in the file\n");
        return 1;
    }
    printf("PASS: every out-of-range index, end period and attribute returned an error code\n");
    return 0;
}
