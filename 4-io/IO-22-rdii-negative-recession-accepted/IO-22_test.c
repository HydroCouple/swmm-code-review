/*
 * IO-22: a negative RDII recession limb ratio K is accepted without a message.
 *
 * The input reference defines K as the ratio of the duration of the unit
 * hydrograph's recession limb to its time to peak T, makes the base time
 * T*(1+K), and states that the area under each UH is 1 in. A negative K
 * cannot satisfy that: the base time is shorter than the time to peak (or
 * zero or negative), and SWMM cuts the triangle off on its rising limb.
 *
 * The test runs IO-22_negative-k.inp (R = 0.1, T = 1 h, 1.0 in of rain on
 * 100 ac) with K = -0.5, -1 and -2 (it rewrites the K token into
 * IO-22_k.inp) and with K = +0.5 as a control.
 * Correct behaviour: each negative K is rejected with an input error; the
 * control runs and gives R x P x A = 36,300 ft3 of RDII at J1 (within 1%;
 * the UH convolution of a 1-hour record on a 15-min step is exact to far
 * better than that). Unpatched, K = -0.5 runs and gives half that volume,
 * K = -1 and -2 run and give none.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define EXPECTED_FT3 (0.1 * (1.0 / 12.0) * 100.0 * 43560.0)

/* copy the template deck, replacing K on the UH1 parameter line */
static int makeDeck(const char* k)
{
    char line[512];
    FILE* fi = fopen("IO-22_negative-k.inp", "r");
    FILE* fo = fopen("IO-22_k.inp", "w");
    if (!fi || !fo) { if (fi) fclose(fi); if (fo) fclose(fo); return 0; }
    while (fgets(line, sizeof(line), fi))
    {
        if (strncmp(line, "UH1  ALL  SHORT", 15) == 0)
            fprintf(fo, "UH1  ALL  SHORT  0.1  1  %s\n", k);
        else fputs(line, fo);
    }
    fclose(fi);
    fclose(fo);
    return 1;
}

/* run the deck; return the error code and the RDII volume (ft3) at J1 */
static int run(double* vol)
{
    double t = 0.0, tOld = 0.0;
    int err, j;

    *vol = 0.0;
    err = swmm_open("IO-22_k.inp", "IO-22.rpt", "IO-22.out");
    if (!err) err = swmm_start(1);
    j = swmm_getIndex(swmm_NODE, "J1");
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        *vol += swmm_getValue(swmm_NODE_LATFLOW, j) * (t - tOld) * 86400.0;
        tOld = t;
    }
    swmm_end();
    swmm_close();
    return err;
}

int main(void)
{
    const char* ks[4] = {"-0.5", "-1", "-2", "0.5"};
    double vol;
    int i, err, nBad = 0;

    setvbuf(stdout, NULL, _IONBF, 0);   /* keep the table if a run crashes */
    printf("K       error  RDII volume (ft3)  ratio to R*P*A\n");
    for (i = 0; i < 4; i++)
    {
        if (!makeDeck(ks[i])) { printf("FAIL: cannot write IO-22_k.inp\n"); return 1; }
        err = run(&vol);
        if (err)
            printf("%-6s  %5d  (rejected)\n", ks[i], err);
        else
            printf("%-6s  %5d  %12.1f       %.3f\n", ks[i], err, vol, vol / EXPECTED_FT3);
        if (i < 3 && err == 0) nBad++;
        if (i == 3 && (err != 0 || fabs(vol / EXPECTED_FT3 - 1.0) > 0.01)) nBad++;
    }
    if (nBad)
    {
        printf("FAIL: %d of 4 cases wrong: a negative K must be rejected, "
               "K = 0.5 must run and give R*P*A\n", nBad);
        return 1;
    }
    printf("PASS: negative K is rejected with an input error; K = 0.5 gives R*P*A\n");
    return 0;
}
