/*
 * IO-07 for 6.0.0: a ';' inside a quoted token.
 *
 * Legacy getTokens() cuts a line at the first ';', even inside quotes (see
 * IO-07_test.c). 6.0.0's Tokenizer::strip_comment() tracks quotes.
 *
 * Correct behaviour: TS1 is read from "IO-07_flow;v2.dat" (2 cfs from 0:00 to
 * 6:00) and the lateral inflow to J1 at 3:00 is 2.0 cfs (tolerance 1e-6).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

int main(void)
{
    double t = 0.0, q = -1.0, v = 0.0;
    int err, j1 = -1;
    SWMM_Engine e = swmm_engine_create();

    err = swmm_engine_open(e, "IO-07_quoted-semicolon.inp", "IO-07_6.rpt", "IO-07_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 0);
    if (!err) j1 = swmm_node_index(e, "J1");
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (!(t > 0.0)) break;
        if (q < 0.0 && t >= 3.0 / 24.0)
        {
            swmm_node_get_lateral_inflow(e, j1, &v);
            q = v;
        }
    }
    if (!err) err = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);

    printf("error code %d, J1 inflow at 3:00 = %.4f cfs (expected 2.0)\n", err, q);
    if (err || fabs(q - 2.0) > 1e-6)
    {
        printf("FAIL: the quoted file name \"IO-07_flow;v2.dat\" is cut at the ';' "
               "(error %d)\n", err);
        return 1;
    }
    printf("PASS: a ';' inside a quoted file name is part of the name; TS1 is read "
           "from IO-07_flow;v2.dat\n");
    return 0;
}
