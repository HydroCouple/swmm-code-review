/*
 * CRASH-19: the output reader dereferences a NULL handle after testing for it.
 *
 * Almost every SMO_* getter starts with
 *     if (p_data == NULL) errorcode = -1; else ...
 * and ends with
 *     return set_error(p_data->error_handle, errorcode);
 * so a NULL handle is dereferenced in the return statement. SMO_getUnits()
 * reads p_data->Npolluts before its NULL test, and SMO_clearError() has none.
 *
 * Scenario: SMO_close(&h) sets h to NULL (it takes the handle's address for
 * that purpose), and the program then asks the closed handle for data. The
 * same happens to any caller whose SMO_init() failed and left h NULL.
 *
 * Correct behaviour (header: "Error code 0 on success, -1 on failure or error
 * code"): every call with a NULL handle returns a non-zero error code without
 * touching memory; SMO_clearError() simply returns. The three functions that
 * already do this (SMO_getVersion, SMO_getFlowUnits, SMO_checkError) are
 * probed first, so the table shows them before the first crash.
 */
#include <stdio.h>
#include "swmm5.h"
#include "swmm_output.h"

static int ok = 1;

static void show(const char *name, int rc)
{
    printf("  %-26s %5d\n", name, rc);
    fflush(stdout);
    if (rc == 0) ok = 0;
}

int main(void)
{
    SMO_Handle h = NULL;
    int err, n = 0, i = 0, *iv = NULL;
    double elapsed = 0.0, date = 0.0;
    float *fv = NULL;
    char *s = NULL;

    /* run the deck to write CRASH-19.out */
    err = swmm_open("CRASH-19_reader.inp", "CRASH-19.rpt", "CRASH-19.out");
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

    /* open the file, read something, close it */
    SMO_init(&h);
    err = SMO_open(h, "CRASH-19.out");
    if (!err) err = SMO_getStartDate(h, &date);
    SMO_close(&h);
    printf("SMO_open/getStartDate: error %d, start date %.1f; "
           "after SMO_close the handle is %s\n\n", err, date, h ? "not NULL" : "NULL");
    if (err || h != NULL)
    {
        printf("FAIL: could not set up the test\n");
        return 1;
    }

    printf("  call with the NULL handle  return\n");
    /* already guarded: these return -1 */
    show("SMO_getVersion", SMO_getVersion(h, &i));
    show("SMO_getFlowUnits", SMO_getFlowUnits(h, &i));
    show("SMO_checkError", SMO_checkError(h, &s));
    /* the rest dereference the NULL handle */
    show("SMO_getStartDate", SMO_getStartDate(h, &date));
    show("SMO_getTimes", SMO_getTimes(h, SMO_numPeriods, &i));
    show("SMO_getProjectSize", SMO_getProjectSize(h, &iv, &n));
    show("SMO_getUnits", SMO_getUnits(h, &iv, &n));
    show("SMO_getPollutantUnits", SMO_getPollutantUnits(h, &iv, &n));
    show("SMO_getElementName", SMO_getElementName(h, SMO_node, 0, &s, &n));
    show("SMO_getSubcatchSeries", SMO_getSubcatchSeries(h, 0, SMO_rainfall_subcatch, 0, 2, &fv, &n));
    show("SMO_getNodeSeries", SMO_getNodeSeries(h, 0, SMO_invert_depth, 0, 2, &fv, &n));
    show("SMO_getLinkSeries", SMO_getLinkSeries(h, 0, SMO_flow_rate_link, 0, 2, &fv, &n));
    show("SMO_getSystemSeries", SMO_getSystemSeries(h, SMO_air_temp, 0, 2, &fv, &n));
    show("SMO_getSubcatchAttribute", SMO_getSubcatchAttribute(h, 0, SMO_rainfall_subcatch, &fv, &n));
    show("SMO_getNodeAttribute", SMO_getNodeAttribute(h, 0, SMO_invert_depth, &fv, &n));
    show("SMO_getLinkAttribute", SMO_getLinkAttribute(h, 0, SMO_flow_rate_link, &fv, &n));
    show("SMO_getSystemAttribute", SMO_getSystemAttribute(h, 0, SMO_air_temp, &fv, &n));
    show("SMO_getSubcatchResult", SMO_getSubcatchResult(h, 0, 0, &fv, &n));
    show("SMO_getNodeResult", SMO_getNodeResult(h, 0, 0, &fv, &n));
    show("SMO_getLinkResult", SMO_getLinkResult(h, 0, 0, &fv, &n));
    show("SMO_getSystemResult", SMO_getSystemResult(h, 0, 0, &fv, &n));
#ifdef OPENSWMM_LEGACY_OUTPUT_H_
    /* functions added in 5.3.0 */
    show("SMO_getNumVars", SMO_getNumVars(h, SMO_node, &i));
    show("SMO_getVarCode", SMO_getVarCode(h, SMO_node, 0, &i));
    show("SMO_getVarCodes", SMO_getVarCodes(h, SMO_node, &iv, &n));
    show("SMO_getNumProperties", SMO_getNumProperties(h, SMO_node, &i));
    show("SMO_getPropertyCode", SMO_getPropertyCode(h, SMO_node, 0, &i));
    show("SMO_getPropertyCodes", SMO_getPropertyCodes(h, SMO_node, &iv, &n));
    {
        float x = 0.0f;
        show("SMO_getPropertyValue", SMO_getPropertyValue(h, SMO_node, 0, 0, &x));
    }
    show("SMO_getPropertyValues", SMO_getPropertyValues(h, SMO_node, 0, &fv, &n));
#endif
    SMO_clearError(h);
    printf("  %-26s %5s\n", "SMO_clearError", "(void)");

    if (!ok)
    {
        printf("FAIL: a call with a NULL handle returned 0\n");
        return 1;
    }
    printf("PASS: every reader call with a NULL handle returned an error code without crashing\n");
    return 0;
}
