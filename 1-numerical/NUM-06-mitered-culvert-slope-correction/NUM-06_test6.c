/*
 * NUM-06 for 6.0.0: culvert::getInflow() (Culvert.cpp) uses the legacy
 * slope correction, 7*S instead of 0.7*S, for mitered inlets (codes 5, 37, 46).
 *
 * Same deck and check as NUM-06_test.c: a 3 ft CMP culvert, code 5, on a 5%
 * slope carrying 60 cfs must have the HDS-5 submerged inlet-control headwater
 * HW = D*(c*(Q/(A*sqrt(D)))^2 + Y + 0.7*S) = 5.69 ft (SWMM Hydraulics
 * Reference Manual eq. 7-46, Scf = 0.7 for mitered inlets), within 0.15 ft.
 * The 7*S correction adds 0.95 ft.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

int main(void)
{
    const double Q = 60.0, D = 3.0, c = 0.0463, Y = 0.75;
    const double S = 5.0 / sqrt(100.0 * 100.0 - 5.0 * 5.0);  /* 5 ft drop over 100 ft */
    const double AD = 3.14159265358979 * D * D / 4.0 * sqrt(D);
    const double hw07 = D * (c * (Q / AD) * (Q / AD) + Y + 0.7 * S);
    const double hw7  = D * (c * (Q / AD) * (Q / AD) + Y + 7.0 * S);
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, hw = 0.0, q = 0.0;
    int err, j1, c1;

    err = swmm_engine_open(e, "NUM-06_mitered.inp", "NUM-06_6.rpt", "NUM-06_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    j1 = swmm_node_index(e, "J1");
    c1 = swmm_link_index(e, "C1");
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0) break;
        swmm_node_get_depth(e, j1, &hw);     /* ft (CFS model) */
        swmm_link_get_flow(e, c1, &q);       /* cfs */
    }
    if (!err) err = swmm_engine_end(e);
    if (!err) err = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
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
