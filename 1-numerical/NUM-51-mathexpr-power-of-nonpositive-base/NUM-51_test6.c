/*
 * NUM-51 for 6.0.0: the expression evaluators use plain std::pow, so integer
 * powers of negative numbers are right, but a fractional power of a negative
 * number is NaN, and nothing turns it into a finite value. In a treatment
 * equation the NaN becomes the node's concentration.
 *
 * Same deck and expected values as NUM-51_test.c: at junction J1,
 *   TP   C = (TSS - 13)^2        = 9
 *   COD  C = 30 + (TSS - 12)^3   = 22
 *   BOD  C = 5 + (TSS - 14)^0.5  = 5 ((-4)^0.5 evaluates to 0, as sqrt() of
 *        a negative number does in both engines)
 * Tolerance 0.1 mg/L; 6.0.0 gives BOD = NaN.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

int main(void)
{
    static const char *name[] = {"TP", "COD", "BOD"};
    static const char *eqn[]  = {"(TSS - 13)^2", "30 + (TSS - 12)^3", "5 + (TSS - 14)^0.5"};
    static const double expect[] = {9.0, 22.0, 5.0};
    double t = 0.0, c[3] = {-1.0, -1.0, -1.0};
    int i, rc, j1 = -1, nbad = 0;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "NUM-51_power.inp", "NUM-51_6.rpt", "NUM-51_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    if (!rc) j1 = swmm_node_index(e, "J1");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
    }
    for (i = 0; i < 3 && !rc; i++) rc = swmm_node_get_quality(e, j1, 1 + i, &c[i]);
    if (!rc) rc = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
        return 1;
    }

    printf("Pollutant  Equation at J1          Expected  Computed (mg/L)\n");
    for (i = 0; i < 3; i++)
    {
        int bad = !(fabs(c[i] - expect[i]) < 0.1);
        nbad += bad;
        printf("%-10s C = %-20s %8.2f  %8.2f%s\n", name[i], eqn[i], expect[i], c[i],
               bad ? "  <-- wrong" : "");
    }
    if (nbad)
    {
        printf("FAIL: a power of a negative number is wrong: TP = %.2f (expected 9), "
               "COD = %.2f (expected 22), BOD = %.2f (expected 5)\n", c[0], c[1], c[2]);
        return 1;
    }
    printf("PASS: integer powers of negative numbers are evaluated, and a fractional "
           "power of a negative number gives 0, not NaN\n");
    return 0;
}
