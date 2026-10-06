/*
 * NUM-20: standard arch size code 31 has the full hydraulic radius of code 30.
 *
 * Codes 30, 31 and 32 are corrugated-steel pipe arches (3 x 1 in corrugation)
 * of rise x span 31 x 40, 36 x 46 and 41 x 53 in, with full areas 7.0, 9.4 and
 * 12.3 ft2 (Appendix A13 of the input manual / Table E-1 of the reference
 * manual). They are the same shape at three scales: span/rise is 1.29, 1.28,
 * 1.29 and A/(rise*span) is 0.813, 0.817, 0.815. For geometrically similar
 * sections the hydraulic radius scales with size, so Rfull/rise must be the
 * same for all three.
 *
 * The toolkit gives the full-flow capacity Qfull = 1.486/n * A * R^(2/3) * S^0.5;
 * the test backs out Rfull from it with the tabulated area and compares
 * Rfull/rise of code 31 with the mean of its neighbours. Tolerance 3 %: the
 * neighbours agree to 0.1 %, the defect is 14 % low.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    const char *ids[3] = {"A30", "A31", "A32"};
    const double rise[3] = {31.0, 36.0, 41.0}, span[3] = {40.0, 46.0, 53.0};
    const double afull[3] = {7.0, 9.4, 12.3}, n = 0.024;
    double qfull[3], slope[3], rfull[3], ratio[3], ref, dev;
    int i, k, err;

    err = swmm_open("NUM-20_arch-codes.inp", "NUM-20.rpt", "NUM-20.out");
    if (!err) err = swmm_start(0);
    for (i = 0; !err && i < 3; i++)
    {
        k = swmm_getIndex(swmm_LINK, ids[i]);
        qfull[i] = swmm_getValue(swmm_LINK_FULLFLOW, k);
        slope[i] = swmm_getValue(swmm_LINK_SLOPE, k);
        rfull[i] = pow(qfull[i] * n / (1.486 * afull[i] * sqrt(slope[i])), 1.5);
        ratio[i] = rfull[i] / (rise[i] / 12.0);
    }
    swmm_end();
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    printf("Code  Rise x span (in)  Afull (ft2)  A/(rise*span)  Qfull (cfs)  Rfull (ft)  Rfull/rise\n");
    for (i = 0; i < 3; i++)
        printf("%4d  %6.0f x %-6.0f   %8.1f      %8.3f    %9.2f   %9.3f   %9.3f\n",
               30 + i, rise[i], span[i], afull[i], afull[i] / (rise[i] * span[i] / 144.0),
               qfull[i], rfull[i], ratio[i]);

    ref = 0.5 * (ratio[0] + ratio[2]);
    dev = ratio[1] / ref - 1.0;
    printf("Code 31: Rfull/rise %.3f vs %.3f for codes 30 and 32 (%+.1f %%); "
           "Rfull %.3f ft, %.3f ft by the same ratio\n", ratio[1], ref, 100.0 * dev,
           rfull[1], ref * rise[1] / 12.0);
    if (fabs(dev) > 0.03)
    {
        printf("FAIL: code 31 has Rfull = %.3f ft, %.0f %% below the %.3f ft its shape implies; "
               "Qfull %.2f cfs\n", rfull[1], -100.0 * dev, ref * rise[1] / 12.0, qfull[1]);
        return 1;
    }
    printf("PASS: codes 30-32 have the same Rfull/rise, as similar shapes must\n");
    return 0;
}
