/*
 * CRASH-10 for 6.0.0: the node indexes in a binary RDII interface file are
 * used to index Node[] without a range check in 5.2.4 and 5.3.0. 6.0.0
 * range-checks them (RdiiInterface.cpp); this is the same check as
 * CRASH-10_test.c through the 6.0.0 C API.
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
 * every other file is rejected (6.0.0 reports ERROR 345 and returns an
 * error code from its API).
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

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
    double t = 0.0, q = 0.0;
    int err, j = -1;
    SWMM_Engine e = swmm_engine_create();

    *qMax = 0.0;
    err = swmm_engine_open(e, "CRASH-10_use-rdii.inp", "CRASH-10_6.rpt", "CRASH-10_6.out", NULL);
    if (!err) err = swmm_engine_initialize(e);
    if (!err) err = swmm_engine_start(e, 1);
    if (!err) j = swmm_node_index(e, "J1");
    while (!err)
    {
        err = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        swmm_node_get_lateral_inflow(e, j, &q);     /* project units: cfs */
        if (q > *qMax) *qMax = q;
    }
    if (!err) err = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
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
        if (i > 0 && err == 0) nBad++;
    }
    if (nBad)
    {
        printf("FAIL: %d of 4 RDII files handled wrongly (expected: index 0 runs "
               "with 1.0 cfs, the others are rejected)\n", nBad);
        return 1;
    }
    printf("PASS: out-of-range node indexes in an RDII file are rejected\n");
    return 0;
}
