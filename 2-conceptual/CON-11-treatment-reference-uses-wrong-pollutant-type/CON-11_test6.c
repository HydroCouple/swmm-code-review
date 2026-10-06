/*
 * CON-11 for 6.0.0: in a treatment expression, a pollutant name stands for the
 * node's concentration before treatment in a concentration-type (C =)
 * equation and for the inflow concentration in a removal-type (R =) equation
 * (reference manual Vol. III, section 5.4.2). 6.0.0 copies legacy's rule of
 * choosing by the REFERENCED pollutant's treatment type (QualityRouting.cpp,
 * the cpollut array), except that an untreated pollutant reads as the node
 * concentration.
 *
 * Same decks and expected values as CON-11_test.c: TP in storage unit SU1
 * at the end of the run is 5, 5 and 10 mg/L. Tolerance 0.1 mg/L; the bug
 * gives 5, 0 and about 7.4.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

static const struct { const char *deck, *tss, *tp; double expect; } cases[] = {
    {"CON-11_ctype-untreated-ref.inp", "(none)",  "C = 0.05*TSS",  5.0},
    {"CON-11_ctype-rtype-ref.inp",     "R = 0.5", "C = 0.05*TSS",  5.0},
    {"CON-11_rtype-ctype-ref.inp",     "C = TSS", "R = 0.005*TSS", 10.0},
};

int main(void)
{
    int i, nbad = 0;

    printf("Deck                            TSS eqn  TP eqn         TP expected  TP computed\n");
    for (i = 0; i < 3; i++)
    {
        double t = 0.0, tp = -1.0;
        int rc, su1 = -1;
        SWMM_Engine e = swmm_engine_create();
        rc = swmm_engine_open(e, cases[i].deck, "CON-11_6.rpt", "CON-11_6.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 1);
        if (!rc) su1 = swmm_node_index(e, "SU1");
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (t <= 0.0) break;
        }
        if (!rc) rc = swmm_node_get_quality(e, su1, 1, &tp);
        if (!rc) rc = swmm_engine_end(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        if (rc || !(fabs(tp - cases[i].expect) < 0.1)) nbad++;
        printf("%-31s %-8s %-14s %8.2f   %8.2f%s\n", cases[i].deck, cases[i].tss,
               cases[i].tp, cases[i].expect, tp,
               rc ? "  <-- run failed" :
               !(fabs(tp - cases[i].expect) < 0.1) ? "  <-- wrong" : "");
    }
    if (nbad)
    {
        printf("FAIL: TP in SU1 is wrong in %d of 3 decks: TSS in TP's equation is read "
               "by TSS's treatment type, not TP's\n", nbad);
        return 1;
    }
    printf("PASS: TSS in TP's equation is the pond concentration in a C = equation and "
           "the inflow concentration in an R = equation\n");
    return 0;
}
