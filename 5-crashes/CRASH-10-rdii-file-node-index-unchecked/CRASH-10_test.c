/*
 * CRASH-10: the node indexes in a binary RDII interface file are used to
 * index Node[] without a range check.
 *
 * CRASH-10_use-rdii.inp reads its RDII inflows from CRASH-10.rdii
 * ([FILES] USE RDII). The binary format stores, after the "SWMM5-RDII" stamp,
 * the time step, the number of RDII nodes and their node INDEXES, then
 * records of (date, flow per node). An index is only meaningful for the model
 * that wrote the file. The test writes four files with one RDII node each:
 *     index 0       J1, the model's RDII node (control: must run)
 *     index 2       one past the last node (the model has 2 nodes)
 *     index 100000  far out of range
 *     index -1      negative
 * Correct behaviour: the control runs and J1 receives the file's 1.0 cfs;
 * every other file is rejected with ERROR 345 (invalid format for RDII
 * interface file). Unpatched, readRdiiFileHeader() reads Node[2] past the end
 * of the array (AddressSanitizer: heap-buffer-overflow) and Node[100000] and
 * Node[-1] outside it (a segmentation fault in a normal build).
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* write a binary RDII file with one node index and one 1.0 cfs record */
static int writeRdiiFile(int index)
{
    int   step = 900, n = 1;
    double date = 43831.0;             /* 01/01/2020 00:00 */
    float q = 1.0f;
    FILE* f = fopen("CRASH-10.rdii", "wb");
    if (!f) return 0;
    fwrite("SWMM5-RDII", 1, strlen("SWMM5-RDII"), f);
    fwrite(&step, sizeof(int), 1, f);
    fwrite(&n, sizeof(int), 1, f);
    fwrite(&index, sizeof(int), 1, f);
    fwrite(&date, sizeof(double), 1, f);
    fwrite(&q, sizeof(float), 1, f);
    fclose(f);
    return 1;
}

/* run the deck; return the error code and J1's largest lateral inflow */
static int run(double* qMax)
{
    double t = 0.0, q;
    int err, j;

    *qMax = 0.0;
    err = swmm_open("CRASH-10_use-rdii.inp", "CRASH-10.rpt", "CRASH-10.out");
    if (!err) err = swmm_start(1);
    j = swmm_getIndex(swmm_NODE, "J1");
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        q = swmm_getValue(swmm_NODE_LATFLOW, j);
        if (q > *qMax) *qMax = q;
    }
    swmm_end();
    swmm_close();
    return err;
}

int main(void)
{
    int indexes[4] = {0, 2, 100000, -1};
    double qMax;
    int i, err, nBad = 0;

    setvbuf(stdout, NULL, _IONBF, 0);   /* keep the table if a run crashes */
    printf("node index in file  error  max RDII inflow at J1 (cfs)\n");
    for (i = 0; i < 4; i++)
    {
        if (!writeRdiiFile(indexes[i])) { printf("FAIL: cannot write CRASH-10.rdii\n"); return 1; }
        printf("%18d  ", indexes[i]);
        err = run(&qMax);
        printf("%5d  %.3f\n", err, qMax);
        if (i == 0 && (err != 0 || qMax < 0.999)) nBad++;
        if (i > 0 && err != 345) nBad++;
    }
    if (nBad)
    {
        printf("FAIL: %d of 4 RDII files handled wrongly (expected: index 0 runs "
               "with 1.0 cfs, the others give error 345)\n", nBad);
        return 1;
    }
    printf("PASS: out-of-range node indexes in an RDII file are rejected with "
           "error 345\n");
    return 0;
}
