/*
 * CRASH-19 for 6.0.0: output reader calls with a NULL handle.
 *
 * The legacy SMO_* getters test the handle for NULL and then dereference it
 * anyway. In 6.0.0, swmm_output_open() returns NULL when it cannot open a
 * file, and every swmm_output_* function tests the handle first
 * (openswmm_output_impl.cpp: CHECK_READER / if (!handle)).
 *
 * Correct behaviour: with a NULL handle every getter returns -1 (or NULL for
 * the ID getters) and swmm_output_close(NULL) returns; nothing crashes.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_output.h"

static int ok = 1;

static void show(const char *name, int rc)
{
    printf("  %-36s %5d\n", name, rc);
    fflush(stdout);
    if (rc != -1) ok = 0;
}

static void show_id(const char *name, const char *id)
{
    printf("  %-36s %5s\n", name, id ? id : "NULL");
    fflush(stdout);
    if (id != NULL) ok = 0;
}

int main(void)
{
    float v[64];
    double d = 0.0;
    int n = 0, i = 0;
    SWMM_Output h = swmm_output_open("CRASH-19_does_not_exist.out");

    printf("swmm_output_open(missing file) returned %s\n\n", h ? "a handle" : "NULL");
    if (h != NULL)
    {
        printf("FAIL: could not set up the test\n");
        return 1;
    }
    printf("  call with the NULL handle            return\n");
    show("swmm_output_get_version", swmm_output_get_version(h));
    show("swmm_output_get_flow_units", swmm_output_get_flow_units(h));
    show("swmm_output_get_subcatch_count", swmm_output_get_subcatch_count(h));
    show("swmm_output_get_node_count", swmm_output_get_node_count(h));
    show("swmm_output_get_link_count", swmm_output_get_link_count(h));
    show("swmm_output_get_pollut_count", swmm_output_get_pollut_count(h));
    show("swmm_output_get_period_count", swmm_output_get_period_count(h));
    show("swmm_output_get_start_date", swmm_output_get_start_date(h, &d));
    show("swmm_output_get_report_step", swmm_output_get_report_step(h));
    show_id("swmm_output_get_subcatch_id", swmm_output_get_subcatch_id(h, 0));
    show_id("swmm_output_get_node_id", swmm_output_get_node_id(h, 0));
    show_id("swmm_output_get_link_id", swmm_output_get_link_id(h, 0));
    show_id("swmm_output_get_pollut_id", swmm_output_get_pollut_id(h, 0));
    show("swmm_output_get_subcatch_result", swmm_output_get_subcatch_result(h, 0, 0, v));
    show("swmm_output_get_node_result", swmm_output_get_node_result(h, 0, 0, v));
    show("swmm_output_get_link_result", swmm_output_get_link_result(h, 0, 0, v));
    show("swmm_output_get_system_result", swmm_output_get_system_result(h, 0, 0, v));
    show("swmm_output_get_subcatch_series", swmm_output_get_subcatch_series(h, 0, 0, 0, 1, v));
    show("swmm_output_get_node_series", swmm_output_get_node_series(h, 0, 0, 0, 1, v));
    show("swmm_output_get_link_series", swmm_output_get_link_series(h, 0, 0, 0, 1, v));
    show("swmm_output_get_system_series", swmm_output_get_system_series(h, 0, 0, 1, v));
    show("swmm_output_get_subcatch_attribute", swmm_output_get_subcatch_attribute(h, 0, 0, v, &n));
    show("swmm_output_get_node_attribute", swmm_output_get_node_attribute(h, 0, 0, v, &n));
    show("swmm_output_get_link_attribute", swmm_output_get_link_attribute(h, 0, 0, v, &n));
    show("swmm_output_get_period_time", swmm_output_get_period_time(h, 0, &d));
    show("swmm_output_get_error_code", swmm_output_get_error_code(h));
    show("swmm_output_refresh", swmm_output_refresh(h, &i));
    show("swmm_output_is_live", swmm_output_is_live(h));
    swmm_output_close(h);
    printf("  %-36s %5s\n", "swmm_output_close", "(void)");

    if (!ok)
    {
        printf("FAIL: a call with a NULL handle did not return -1 / NULL\n");
        return 1;
    }
    printf("PASS: every output reader call with a NULL handle returned -1 (or NULL) without crashing\n");
    return 0;
}
