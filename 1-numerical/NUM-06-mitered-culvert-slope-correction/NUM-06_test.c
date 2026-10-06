/*
 * NUM-06: the slope correction for mitered culvert inlets (codes 5, 37, 46)
 * is 7*S instead of 0.7*S.
 *
 * Deck: C1 is a 3 ft corrugated metal pipe culvert, code 5 (mitered to
 * conform to slope), 100 ft long on a 5% slope, carrying a steady 60 cfs from
 * J1. Its outlet node J2 drains to a free outfall, and the barrel's full-flow
 * capacity (about 80 cfs) exceeds 60 cfs, so the culvert is inlet controlled.
 *
 * Correct behaviour: SWMM Hydraulics Reference Manual eq. 7-46 (FHWA HDS-5,
 * submerged inlet), with Scf = +0.7 for mitered inlets (-0.5 for all others):
 *     HW/D = c*(Q/(A*sqrt(D)))^2 + Y + 0.7*S,  c = 0.0463, Y = 0.75 (code 5)
 * Q/(A*sqrt(D)) = 60/(7.069*sqrt(3)) = 4.90 > 4, so the submerged form
 * applies, and HW = 5.69 ft. With the 7*S correction the engine gives
 * HW/D larger by 6.3*S = 0.315, i.e. 0.95 ft more headwater.
 *
 * The test reads J1's depth at the end of the 2-hour run (steady state) and
 * requires it to be within 0.15 ft of the HDS-5 headwater.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    const double Q = 60.0, D = 3.0, c = 0.0463, Y = 0.75;
    const double S = 5.0 / sqrt(100.0 * 100.0 - 5.0 * 5.0);  /* 5 ft drop over 100 ft */
    const double AD = 3.14159265358979 * D * D / 4.0 * sqrt(D);
    const double hw07 = D * (c * (Q / AD) * (Q / AD) + Y + 0.7 * S);
    const double hw7  = D * (c * (Q / AD) * (Q / AD) + Y + 7.0 * S);
    double elapsed = 0.0, hw = 0.0, q = 0.0;
    int err, j1, c1;

    err = swmm_open("NUM-06_mitered.inp", "NUM-06.rpt", "NUM-06.out");
    if (!err) err = swmm_start(1);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    c1 = swmm_getIndex(swmm_LINK, "C1");
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        hw = swmm_getValue(swmm_NODE_DEPTH, j1);
        q  = swmm_getValue(swmm_LINK_FLOW, c1);
    }
    swmm_end();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    printf("Q/(A*sqrt(D))                          %7.3f (> 4: submerged inlet)\n", Q / AD);
    printf("HDS-5 headwater, Scf*S = +0.7*S        %7.3f ft\n", hw07);
    printf("same with +7*S                         %7.3f ft\n", hw7);
    printf("SWMM: C1 flow                          %7.2f cfs\n", q);
    printf("SWMM: J1 headwater (steady)            %7.3f ft\n", hw);

    /* 0.15 ft: the 7*S correction adds 0.95 ft */
    if (fabs(hw - hw07) > 0.15)
    {
        printf("FAIL: the mitered culvert's headwater is %.3f ft, HDS-5 gives %.3f ft "
               "(%+.3f ft; a 7*S slope correction gives %.3f ft)\n",
               hw, hw07, hw - hw07, hw7);
        return 1;
    }
    printf("PASS: the mitered culvert's headwater %.3f ft matches HDS-5 (%.3f ft)\n",
           hw, hw07);
    return 0;
}
