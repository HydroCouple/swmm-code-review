/*
 * IO-42: the hot start reader does not notice when the file runs out early
 * or holds more records than the model needs.
 *
 * Two 3-hour runs save a hot start file: IO-42_save.inp (J2 a junction,
 * 87 bytes) and IO-42_save_storage.inp (J2 a storage unit, which adds one
 * float, 91 bytes). The test then continues the run from 03:00 for 10
 * minutes (IO-42_use.inp / IO-42_use_storage.inp) from four files:
 *
 *   matching       the junction model's own file (control)
 *   truncated      the same file without its last 24 bytes (the link records)
 *   junction file into the storage model   (4 bytes too short for it)
 *   storage file into the junction model   (4 bytes too long for it)
 *
 * The node and link counts in the header match in every case. Only the first
 * file holds the records the model needs, so the correct behaviour is to start
 * from it and to refuse the other three with an error (SWMM has 335 "error in
 * reading from hot start interface file" and 333 "incompatible data found in
 * hot start interface file" for this). The check on the control run is that
 * C1 starts with the flow it had at the end of the save run (to float
 * precision; 0.001 cfs is far above float rounding of a 6 cfs flow).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

/* run a save deck to the end; return C1's flow at the end of the run */
static int save_run(const char *inp, const char *rpt, const char *out, double *q)
{
    double elapsed = 0.0;
    int err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        /* the step that reaches the end time returns elapsed = 0 */
        if (!err) *q = swmm_getValue(swmm_LINK_FLOW, swmm_getIndex(swmm_LINK, "C1"));
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_close();
    return err;
}

/* copy the first n bytes of src to dst (n < 0: all); return bytes written */
static long copy_file(const char *src, const char *dst, long n)
{
    char buf[4096];
    long total = 0;
    size_t k;
    FILE *fi = fopen(src, "rb"), *fo;
    if (!fi) return -1;
    fo = fopen(dst, "wb");
    if (!fo) { fclose(fi); return -1; }
    while ((k = fread(buf, 1, sizeof(buf), fi)) > 0)
    {
        if (n >= 0 && total + (long)k > n) k = (size_t)(n - total);
        fwrite(buf, 1, k, fo);
        total += (long)k;
        if (n >= 0 && total >= n) break;
    }
    fclose(fi);
    fclose(fo);
    return total;
}

/* start inp from the hot start file IO-42_use.hsf; read C1's initial state,
   run the 10 minutes and return C1's largest flow and the flow routing
   continuity error (%) */
static int use_run(const char *inp, double *q, double *y, double *s,
                   double *qmax, double *ce, char *msg)
{
    int err, c1;
    double elapsed = 0.0;
    float e1 = 0, e2 = 0, e3 = 0;
    msg[0] = '\0';
    *q = *y = *s = *ce = *qmax = NAN;
    err = swmm_open(inp, "IO-42_use.rpt", "IO-42_use.out");
    if (!err) err = swmm_start(0);
    if (!err)
    {
        c1 = swmm_getIndex(swmm_LINK, "C1");
        *q = swmm_getValue(swmm_LINK_FLOW, c1);
        *y = swmm_getValue(swmm_LINK_DEPTH, c1);
        *s = swmm_getValue(swmm_LINK_SETTING, c1);
        *qmax = fabs(*q);
        while (!swmm_step(&elapsed))
        {
            *qmax = fmax(*qmax, fabs(swmm_getValue(swmm_LINK_FLOW, c1)));
            if (elapsed <= 0.0) break;
        }
    }
    else swmm_getError(msg, 200);
    swmm_end();
    if (!err && !swmm_getMassBalErr(&e1, &e2, &e3)) *ce = e2;
    swmm_close();
    return err;
}

int main(void)
{
    struct { const char *name, *src, *inp; long trim; } cases[] = {
        { "matching file (control)",     "IO-42_junction.hsf", "IO-42_use.inp",          0 },
        { "truncated (no link records)", "IO-42_junction.hsf", "IO-42_use.inp",         24 },
        { "junction file, storage model","IO-42_junction.hsf", "IO-42_use_storage.inp",  0 },
        { "storage file, junction model","IO-42_storage.hsf",  "IO-42_use.inp",          0 },
    };
    double qsave = NAN, q, y, s, qmax, ce;
    long size, bytes;
    int i, err, bad = 0, okctl = 0;
    char msg[256];
    FILE *f;

    err = save_run("IO-42_save.inp", "IO-42_save.rpt", "IO-42_save.out", &qsave);
    if (!err) { double dummy; err = save_run("IO-42_save_storage.inp",
                "IO-42_save_storage.rpt", "IO-42_save_storage.out", &dummy); }
    if (err)
    {
        printf("FAIL: a save run stopped with error %d\n", err);
        return 1;
    }
    printf("C1 flow at the end of the save run: %.4f cfs\n\n", qsave);
    printf("%-30s %6s %6s %8s %8s %8s %8s %12s  %s\n", "hot start file",
           "bytes", "error", "C1 flow", "C1 depth", "C1 sett", "C1 max",
           "flow CE (%)", "message");
    printf("%-30s %6s %6s %8s %8s %8s %8s\n", "", "", "", "(cfs)", "(ft)",
           "", "(cfs)");

    for (i = 0; i < 4; i++)
    {
        f = fopen(cases[i].src, "rb");
        if (!f) { printf("FAIL: %s was not written\n", cases[i].src); return 1; }
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fclose(f);
        bytes = copy_file(cases[i].src, "IO-42_use.hsf", size - cases[i].trim);
        err = use_run(cases[i].inp, &q, &y, &s, &qmax, &ce, msg);
        {   /* first line of the message, without the leading blank line */
            char *m = msg; while (*m == '\n' || *m == ' ') m++;
            m[strcspn(m, "\n")] = '\0';
            printf("%-30s %6ld %6d %8.4f %8.4f %8.4f %8.4f %12.3f  %s\n",
                   cases[i].name, bytes, err, q, y, s, qmax, ce, m);
        }
        if (i == 0) okctl = (err == 0 && fabs(q - qsave) < 0.001);
        else if (err == 0) bad++;
    }

    if (!okctl)
    {
        printf("FAIL: the run from the matching hot start file did not start "
               "with C1 at %.4f cfs\n", qsave);
        return 1;
    }
    if (bad)
    {
        printf("FAIL: %d of 3 hot start files that do not match the model were "
               "applied without an error\n", bad);
        return 1;
    }
    printf("PASS: the matching hot start file is applied; the truncated and "
           "mismatched files are refused with an error\n");
    return 0;
}
