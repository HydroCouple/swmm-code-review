/*
 * IO-43: SAVE HOTSTART <file> <date> <time> writes the file at the wrong time.
 *
 * 5.3.0 lets [FILES] give a date and time after a SAVE HOTSTART file name; the
 * file is to hold the state of the run at that moment. IO-43_dated-save.inp
 * runs from 00:00 to 06:00 with an inflow ramping from 0 to 12 cfs and asks for
 * IO-43_0300.hsf at 01/01/2020 03:00:00.
 *
 * The test records J1's depth and C1's flow after every routing step, then
 * decodes the file (a version-4 hot start file: 15-byte stamp, 6 ints, then
 * per node depth and lateral inflow, per link flow, depth and setting, all as
 * floats in ft and cfs) and finds the step whose state it holds.
 *
 * Correct behaviour: the file holds the state after the first routing step
 * that reaches 03:00 (03:00:00.5, as the run's 5-second steps start with a
 * 0.5-second one). The match is by the smallest difference in J1 depth plus
 * C1 flow; it must also be within 0.001, far above float rounding of values
 * near 1 ft and 6 cfs.
 *
 * 5.2.4 has no dated saves: its manual's format is SAVE HOTSTART Fname, it
 * ignores the extra tokens and writes the file at the end of the run. For
 * 5.2.4 the test checks that documented behaviour.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define MAXSTEPS 5000

static double tDay[MAXSTEPS], yJ1[MAXSTEPS], qC1[MAXSTEPS];

static void hms(double days, char *buf)
{
    long ds = (long)floor(days * 864000.0 + 0.5);    /* tenths of a second */
    long s = ds / 10;
    sprintf(buf, "%02ld:%02ld:%02ld.%ld", s / 3600, (s / 60) % 60, s % 60, ds % 10);
}

int main(void)
{
    double elapsed = 0.0, d, best = 1e30;
    float rec[12];
    int err, n = 0, i, ib = -1, i0300 = -1, iwant, j1, c1;
    long size;
    char hdr[39], tbuf[24];
    FILE *f;

    err = swmm_open("IO-43_dated-save.inp", "IO-43.rpt", "IO-43.out");
    if (!err) err = swmm_start(0);
    j1 = swmm_getIndex(swmm_NODE, "J1");
    c1 = swmm_getIndex(swmm_LINK, "C1");
    while (!err && n < MAXSTEPS)
    {
        err = swmm_step(&elapsed);
        if (err) break;
        /* the step that reaches the end time returns elapsed = 0 */
        tDay[n] = elapsed > 0.0 ? elapsed : 0.25;
        yJ1[n] = swmm_getValue(swmm_NODE_DEPTH, j1);
        qC1[n] = swmm_getValue(swmm_LINK_FLOW, c1);
        if (i0300 < 0 && tDay[n] >= 0.125 - 1.0e-9) i0300 = n;
        n++;
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    swmm_close();
    if (err || i0300 < 0)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    /* read J1 (node 0) and C1 (link 0) from the hot start file */
    f = fopen("IO-43_0300.hsf", "rb");
    if (!f)
    {
        printf("FAIL: IO-43_0300.hsf was not written\n");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fread(hdr, 1, 39, f) != 39 || fread(rec, sizeof(float), 12, f) != 12)
    {
        fclose(f);
        printf("FAIL: IO-43_0300.hsf holds only %ld bytes, not the 87 of a "
               "complete file\n", size);
        return 1;
    }
    fclose(f);

    /* find the step whose state the file holds */
    for (i = 0; i < n; i++)
    {
        d = fabs(yJ1[i] - rec[0]) + fabs(qC1[i] - rec[6]);
        if (d < best) { best = d; ib = i; }
    }
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    iwant = i0300;                      /* 5.3.0: first step at the date */
#else
    iwant = n - 1;                      /* 5.2.4: the end of the run */
#endif
    printf("%-34s %10s %10s\n", "", "J1 depth", "C1 flow");
    printf("%-34s %10s %10s\n", "", "(ft)", "(cfs)");
    hms(tDay[i0300], tbuf);
    printf("run at %-27s %10.4f %10.4f\n", tbuf, yJ1[i0300], qC1[i0300]);
    printf("%-34s %10.4f %10.4f\n", "run at 06:00:00.0 (end)", yJ1[n-1], qC1[n-1]);
    printf("%-34s %10.4f %10.4f\n", "IO-43_0300.hsf", rec[0], rec[6]);
    hms(tDay[ib], tbuf);
    printf("The file holds the state of the run at %s\n", tbuf);

    if (ib != iwant || best > 0.001)
    {
        char wbuf[24];
        hms(tDay[iwant], wbuf);
        printf("FAIL: the hot start file should hold the state at %s; it "
               "holds the state at %s (C1 %.4f cfs instead of %.4f cfs)\n",
               wbuf, tbuf, rec[6], qC1[iwant]);
        return 1;
    }
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    printf("PASS: the hot start file holds the state at the requested 03:00:00\n");
#else
    printf("PASS: 5.2.4 has no dated saves; it ignores the date and saves at "
           "the end of the run, as its manual documents\n");
#endif
    return 0;
}
