/*
 * IO-07: a ';' inside a quoted token is taken as the start of a comment.
 *
 * getTokens() (input.c) cuts the line at the first ';' before it looks for
 * quoted tokens, so the file name in
 *     TS1  FILE  "IO-07_flow;v2.dat"
 * becomes "IO-07_flow" and the series file cannot be opened. The function's
 * own notes say "Text between quotes is treated as a single token".
 *
 * Correct behaviour: the quoted name is read whole, TS1 is read from
 * IO-07_flow;v2.dat (2 cfs from 0:00 to 6:00), and the lateral inflow to J1
 * at 3:00 is 2.0 cfs (tolerance 1e-6 cfs; the series is constant).
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    double t = 0.0, q = -1.0;
    int err, j1 = -1;

    err = swmm_open("IO-07_quoted-semicolon.inp", "IO-07.rpt", "IO-07.out");
    if (!err) err = swmm_start(0);
    if (!err) j1 = swmm_getIndex(swmm_NODE, "J1");
    while (!err)
    {
        err = swmm_step(&t);
        if (!(t > 0.0)) break;
        if (q < 0.0 && t >= 3.0 / 24.0) q = swmm_getValue(swmm_NODE_LATFLOW, j1);
    }
    swmm_end();
    swmm_close();

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
