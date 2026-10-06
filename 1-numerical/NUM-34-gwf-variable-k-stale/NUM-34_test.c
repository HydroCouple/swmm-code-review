/*
 * NUM-34: the K variable of a [GWF] expression is not the hydraulic
 * conductivity of the aquifer being computed.
 *
 * S1 has a wet aquifer (theta 0.45, field capacity 0.30) and no custom
 * expression. S2 has a dry aquifer (theta 0.20, below field capacity) and
 * "[GWF] S2 DEEP K". There is no rain and no evaporation, so S2's moisture
 * content stays at 0.20 and its K is constant:
 *
 *     K = Ks exp(-(phi - theta) HCO) = 1.0 exp(-(0.5 - 0.2) x 10) = 0.0498 in/hr
 *
 * (input reference [GWF]: K = unsaturated hydraulic conductivity; Vol I
 * eq. 5-19). Deep percolation over 24 h is K x 24 h = 1.195 in, or
 * 0.996 ac-ft on S2's 10 ac. S1 has no deep loss, so the report's Deep
 * Percolation is S2's alone.
 *
 * With the bug S2 uses the conductivity last computed for S1 (13.568 ac-ft),
 * or 0 when no wetter aquifer was computed before it. The tolerance of
 * 0.01 ac-ft (1 %) is above the report's 3-decimal rounding and far below
 * either error.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

/* first number after the dotted leader of the report line containing label;
   col = 0 for the first column (acre-feet), 1 for the second (inches) */
static int rptValue(const char* rpt, const char* label, int col, double* v)
{
    char line[512];
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        char* p = strstr(line, label);
        double a, b;
        int n;
        if (!p) continue;
        p += strlen(label);
        p += strspn(p, " .");
        n = sscanf(p, "%lf %lf", &a, &b);
        fclose(f);
        if (n < col + 1) return 0;
        *v = col ? b : a;
        return 1;
    }
    fclose(f);
    return 0;
}

int main(void)
{
    const double Ks = 1.0, phi = 0.5, theta = 0.2, hco = 10.0, hours = 24.0, acres = 10.0;
    double K = Ks * exp(-(phi - theta) * hco);          /* in/hr */
    double expected = K * hours / 12.0 * acres;          /* ac-ft */
    double elapsed = 0.0, deep = 0.0, deepIn = 0.0;
    int err;

    err = swmm_open("NUM-34_dry-aquifer-deep-k.inp", "NUM-34.rpt", "NUM-34.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    if (!rptValue("NUM-34.rpt", "Deep Percolation", 0, &deep) ||
        !rptValue("NUM-34.rpt", "Deep Percolation", 1, &deepIn))
    {
        printf("FAIL: could not read Deep Percolation from NUM-34.rpt\n");
        return 1;
    }

    printf("S2 K(theta) = %.4f in/hr\n", K);
    printf("Deep percolation, expected  %8.3f ac-ft\n", expected);
    printf("Deep percolation, reported  %8.3f ac-ft (%.3f in over both subcatchments)\n",
           deep, deepIn);

    if (fabs(deep - expected) > 0.01)
    {
        printf("FAIL: DEEP = K on S2 percolates %.3f ac-ft instead of %.3f ac-ft "
               "(K of S2's own moisture content, %.4f in/hr)\n", deep, expected, K);
        return 1;
    }
    printf("PASS: DEEP = K percolates %.3f ac-ft, K(theta) of S2's aquifer\n", deep);
    return 0;
}
