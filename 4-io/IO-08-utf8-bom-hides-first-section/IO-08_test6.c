/*
 * IO-08 for 6.0.0: a UTF-8 byte order mark hides the first section.
 *
 * InputReader recognises a header only when the trimmed line starts with '[';
 * with a BOM the first line starts with "\xEF\xBB\xBF", so the first section
 * header is not seen and its lines, which come before any known header, are
 * dropped.
 *
 * Correct behaviour, as in IO-08_test.c: each deck is read as if the BOM were
 * not there. [INFLOWS] first: J1's lateral inflow at 3:00 is 5.0 cfs.
 * [OPTIONS] first: FLOW_UNITS is CMS, the start date is 06/01/2020 (day 43983)
 * and J1's lateral inflow at 3:00 is 0.15 cms.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_model.h"

static int runDeck(const char *inp, double *q3, char *units, double *start)
{
    double t = 0.0, v = 0.0;
    int err, j1;
    SWMM_Engine e = swmm_engine_create();
    *q3 = -1.0; *start = -1.0; strcpy(units, "?");
    err = swmm_engine_open(e, inp, "IO-08_6.rpt", "IO-08_6.out", NULL);
    if (!err)
    {
        swmm_options_get(e, "FLOW_UNITS", units, 16);
        swmm_options_get_start_date(e, start);
        err = swmm_engine_initialize(e);
    }
    if (!err) err = swmm_engine_start(e, 0);
    j1 = swmm_node_index(e, "J1");
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (!(t > 0.0)) break;
        if (*q3 < 0.0 && t >= 3.0 / 24.0)
        {
            swmm_node_get_lateral_inflow(e, j1, &v);
            *q3 = v;
        }
    }
    if (!err) err = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return err;
}

int main(void)
{
    double q, start;
    char units[16];
    int err, bad1, bad2;

    err = runDeck("IO-08_bom-inflows-first.inp", &q, units, &start);
    bad1 = err || fabs(q - 5.0) > 1e-6;
    printf("[INFLOWS] first: error %3d, J1 inflow at 3:00 = %.4f cfs (expected 5.0)\n", err, q);

    err = runDeck("IO-08_bom-options-first.inp", &q, units, &start);
    bad2 = err || strcmp(units, "CMS") != 0 || fabs(start - 43983.0) > 1e-6
           || fabs(q - 0.15) > 1e-6;
    printf("[OPTIONS] first: error %3d, flow units %s (expected CMS), start day %.0f "
           "(expected 43983), J1 inflow at 3:00 = %.4f (expected 0.15)\n", err, units, start, q);

    if (bad1 || bad2)
    {
        printf("FAIL: the first section of a file saved with a UTF-8 byte order mark is "
               "not read%s%s\n", bad1 ? " ([INFLOWS] lost)" : "",
               bad2 ? " ([OPTIONS] lost)" : "");
        return 1;
    }
    printf("PASS: files with a UTF-8 byte order mark are read like files without one\n");
    return 0;
}
