/*
 * NUM-49 for 6.0.0: a pollutant evaluated after a cyclic treatment latch must
 * not get a removal of -1 (which doubles a concentration-type result).
 *
 * 6.0.0 copies legacy's node-wide latch, but its apply pass skips every
 * removal <= 0 (QualityRouting.cpp), so the unset R = -1 is never applied.
 *
 * Correct behaviour, as in NUM-49_test.c: either the run stops with ERROR 161
 * (cyclic dependency in treatment functions), or TSS at J1 and at the outfall
 * stays at or below its 10 mg/L inflow concentration (<= 10.5 mg/L; the
 * legacy bug gives 20) and the TSS quality continuity error is small
 * (|error| < 1 %). swmm_get_quality_continuity_error() returns a fraction.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_massbalance.h"

int main(void)
{
    double t = 0.0, tssJ1 = -1.0, tssO1 = -1.0, qualErr = 0.0;
    int rc, j1 = -1, o1 = -1;
    const char *msg;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "NUM-49_cyclic-then-ctype.inp", "NUM-49_6.rpt", "NUM-49_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    if (!rc) { j1 = swmm_node_index(e, "J1"); o1 = swmm_node_index(e, "O1"); }
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    if (!rc)
    {
        swmm_node_get_quality(e, j1, 1, &tssJ1);
        swmm_node_get_quality(e, o1, 1, &tssO1);
        rc = swmm_engine_end(e);
        swmm_get_quality_continuity_error(e, 1, &qualErr);
        qualErr *= 100.0;
        if (!rc) rc = swmm_engine_report(e);
    }
    msg = swmm_get_last_error_msg(e);
    if (rc && msg && strstr(msg, "161"))
    {
        printf("Run stopped with: %s\n", msg);
        printf("PASS: the cyclic treatment is reported as ERROR 161, so no concentration "
               "is built from an unset removal\n");
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        return 0;
    }
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run stopped with unexpected error %d (%s)\n", rc, msg ? msg : "");
        return 1;
    }

    printf("TSS inflow concentration      10.00 mg/L\n");
    printf("TSS at J1 (end of run)       %6.2f mg/L\n", tssJ1);
    printf("TSS at outfall O1            %6.2f mg/L\n", tssO1);
    printf("TSS quality continuity error %6.2f %%\n", qualErr);

    if (!(tssJ1 <= 10.5) || !(tssO1 <= 10.5) || !(fabs(qualErr) < 1.0))
    {
        printf("FAIL: treatment raised TSS from 10 to %.2f mg/L (removal R = -1 applied) "
               "and created mass: quality continuity error %.2f %%\n", tssO1, qualErr);
        return 1;
    }
    printf("PASS: TSS is not raised above its inflow concentration and quality "
           "continuity holds\n");
    return 0;
}
