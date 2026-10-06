/*
 * IO-36: a relative [FILES] USE/SAVE RDII file name is resolved against the
 * working directory, while every other file name in the input is resolved
 * against the folder of the input file.
 *
 * The decks are in the sub-folder model/ and the test runs them from the
 * folder above, as a GUI, a batch script or an API host would:
 *   model/IO-36_use-rdii.inp   USE RDII "IO-36_in.rdii". The test first writes
 *                              model/IO-36_in.rdii (binary format, node J1,
 *                              1.0 cfs). Correct: the run finds it and J1
 *                              receives 1.0 cfs.
 *   model/IO-36_save-rdii.inp  SAVE RDII "IO-36_out.rdii" and
 *                              SAVE OUTFLOWS "IO-36_outflows.txt". Correct:
 *                              both files are written in model/, like every
 *                              other relative file name in an input file.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

static int exists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f) fclose(f);
    return f != NULL;
}

/* write a binary RDII file: node index 0 (J1), one 1.0 cfs record */
static int writeRdiiFile(const char* path)
{
    int   step = 900, n = 1, index = 0;
    double date = 43831.0;             /* 01/01/2020 00:00 */
    float q = 1.0f;
    FILE* f = fopen(path, "wb");
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

/* run a deck; return the error code and J1's largest lateral inflow */
static int run(const char* inp, double* qMax)
{
    double t = 0.0, q;
    int err, j;

    *qMax = 0.0;
    err = swmm_open(inp, "IO-36.rpt", "IO-36.out");
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
    double qMax;
    int err, ok = 1;
    int outModel, outCwd, rdiiModel, rdiiCwd;

    remove("IO-36_out.rdii");
    remove("model/IO-36_out.rdii");
    remove("model/IO-36_outflows.txt");
    if (!writeRdiiFile("model/IO-36_in.rdii"))
    {
        printf("FAIL: cannot write model/IO-36_in.rdii\n");
        return 1;
    }

    /* --- USE RDII */
    err = run("model/IO-36_use-rdii.inp", &qMax);
    printf("USE RDII  \"IO-36_in.rdii\":  error %d, max RDII inflow at J1 %.3f cfs\n",
           err, qMax);
    if (err != 0 || qMax < 0.999) ok = 0;

    /* --- SAVE RDII and SAVE OUTFLOWS */
    err = run("model/IO-36_save-rdii.inp", &qMax);
    outModel  = exists("model/IO-36_outflows.txt");
    outCwd    = exists("IO-36_outflows.txt");
    rdiiModel = exists("model/IO-36_out.rdii");
    rdiiCwd   = exists("IO-36_out.rdii");
    printf("SAVE run: error %d\n", err);
    printf("  SAVE OUTFLOWS \"IO-36_outflows.txt\" written to: %s\n",
           outModel ? "model/ (next to the .inp)" : outCwd ? "working directory" : "nowhere");
    printf("  SAVE RDII     \"IO-36_out.rdii\"     written to: %s\n",
           rdiiModel ? "model/ (next to the .inp)" : rdiiCwd ? "working directory" : "nowhere");
    if (err != 0 || !rdiiModel || rdiiCwd) ok = 0;

    if (!ok)
    {
        printf("FAIL: the RDII file name is not resolved against the input "
               "file's folder\n");
        return 1;
    }
    printf("PASS: USE and SAVE RDII resolve a relative name against the input "
           "file's folder\n");
    return 0;
}
