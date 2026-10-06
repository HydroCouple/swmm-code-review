/*
 * NUM-51: the expression evaluator (mathexpr.c) computes x^y as 0 whenever
 * x <= 0: `if (r2 <= 0.0) r2 = 0.0; else r2 = pow(r2, r1);`. The guard is
 * meant for a fractional power of a negative number, which has no real
 * value, but it also zeroes integer powers: (-3)^2 = 0 and (-2)^3 = 0.
 *
 * Junction J1 takes 1 cfs with TSS = 10, TP = 20, COD = 40, BOD = 20 mg/L
 * and has the concentration-type treatment equations
 *   TP   C = (TSS - 13)^2        expected (-3)^2 = 9
 *   COD  C = 30 + (TSS - 12)^3   expected 30 + (-2)^3 = 22
 *   BOD  C = 5 + (TSS - 14)^0.5  (-4)^0.5 has no real value; the evaluator
 *        returns 0 for it, as it does for sqrt() of a negative number, so 5
 *
 * Correct behaviour: '^' is standard exponentiation (input reference,
 * [TREATMENT]), so J1 shows TP = 9, COD = 22 and a finite BOD = 5 mg/L.
 * Tolerance 0.1 mg/L; the bug gives TP = 0 and COD = 30.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

/* node results in the last reporting period (pollutant k is entry 6 + k) */
static int last_node_result(const char *out, int n, float **vals, int *len)
{
    SMO_Handle h = NULL;
    int nper = 0, err;
    SMO_init(&h);
    err = SMO_open(h, out);
    if (!err) err = SMO_getTimes(h, SMO_numPeriods, &nper);
    if (!err) err = SMO_getNodeResult(h, nper - 1, n, vals, len);
    SMO_close(&h);
    return err;
}

int main(void)
{
    static const char *name[] = {"TP", "COD", "BOD"};
    static const char *eqn[]  = {"(TSS - 13)^2", "30 + (TSS - 12)^3", "5 + (TSS - 14)^0.5"};
    static const double expect[] = {9.0, 22.0, 5.0};
    double t = 0.0, c[3] = {-1.0, -1.0, -1.0};
    float *vals = NULL;
    int i, len = 0, err, j1, nbad = 0;

    err = swmm_open("NUM-51_power.inp", "NUM-51.rpt", "NUM-51.out");
    if (!err) err = swmm_start(1);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
    }
    swmm_end();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    if (last_node_result("NUM-51.out", j1, &vals, &len) == 0 && len >= 10)
        for (i = 0; i < 3; i++) c[i] = vals[7 + i];
    SMO_free((void **)&vals);

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
