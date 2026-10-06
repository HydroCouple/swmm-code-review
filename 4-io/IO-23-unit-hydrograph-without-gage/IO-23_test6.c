/*
 * IO-23 for 6.0.0: an RDII unit hydrograph group with no rain gage line is
 * accepted. Same check as IO-23_test.c through the 6.0.0 C API.
 *
 * The input reference requires, for each unit hydrograph group, "one line to
 * specify its rain gage followed by" its UH lines. Both decks leave that line
 * out of [HYDROGRAPHS] while node J1 receives RDII from the group:
 *   IO-23_no-gage-line.inp  the project has one gage, G1, that nothing uses
 *   IO-23_no-gages.inp      the project has no rain gages at all
 * Correct behaviour: the run stops with an input error. For information the
 * test prints the RDII volume at J1 of a run that is accepted; had the group
 * been on G1 it would be R x P x A = 0.1 x 1 in x 100 ac = 36,300 ft3.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

#define EXPECTED_FT3 (0.1 * (1.0 / 12.0) * 100.0 * 43560.0)

/* run a deck; return the error code and the RDII volume (ft3) at J1 */
static int run(const char* inp, double* vol)
{
    double t = 0.0, tOld = 0.0, q = 0.0;
    int err, j = -1;
    SWMM_Engine e = swmm_engine_create();

    *vol = 0.0;
    err = swmm_engine_open(e, inp, "IO-23_6.rpt", "IO-23_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    if (!err) j = swmm_node_index(e, "J1");
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        swmm_node_get_lateral_inflow(e, j, &q);     /* project units: cfs */
        *vol += q * (t - tOld) * 86400.0;
        tOld = t;
    }
    if (!err) err = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return err;
}

int main(void)
{
    const char* decks[2] = {"IO-23_no-gage-line.inp", "IO-23_no-gages.inp"};
    double vol;
    int i, err, nBad = 0;

    setvbuf(stdout, NULL, _IONBF, 0);   /* keep the table if a run crashes */
    printf("deck                     error  RDII volume (ft3)\n");
    for (i = 0; i < 2; i++)
    {
        err = run(decks[i], &vol);
        if (err)
            printf("%-24s %5d  (rejected)\n", decks[i], err);
        else
        {
            printf("%-24s %5d  %12.1f  (%.1f x R*P*A for gage G1)\n",
                   decks[i], err, vol, vol / EXPECTED_FT3);
            nBad++;
        }
    }
    if (nBad)
    {
        printf("FAIL: %d of 2 decks with a gageless unit hydrograph group ran "
               "without an error\n", nBad);
        return 1;
    }
    printf("PASS: a unit hydrograph group without a rain gage is an input error\n");
    return 0;
}
