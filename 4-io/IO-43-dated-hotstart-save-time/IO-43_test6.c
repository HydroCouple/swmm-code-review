/*
 * IO-43 for 6.0.0: SAVE HOTSTART <file> <date> <time>.
 *
 * 6.0.0 parses the date and time (FilesHandler.cpp) but its only hot start
 * save is the one at the end of the run, which skips every row with a date.
 * IO-43_dated-save.inp asks for IO-43_0300.hsf at 01/01/2020 03:00:00 of a
 * 00:00-06:00 run; 6.0.0 writes no file and gives no warning.
 *
 * Correct behaviour, as in IO-43_test.c: the file exists and holds the state
 * after the first routing step that reaches 03:00 (J1 depth and C1 flow
 * closest to that step's, and within 0.001, far above float rounding).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

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
    double t = 0.0, d, best = 1e30;
    float rec[12];
    int rc, n = 0, i, ib = -1, i0300 = -1, j1, c1, nwarn;
    long size;
    char hdr[39], tbuf[24];
    FILE *f;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "IO-43_dated-save.inp", "IO-436.rpt", "IO-436.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 0);
    j1 = swmm_node_index(e, "J1");
    c1 = swmm_link_index(e, "C1");
    while (!rc && n < MAXSTEPS)
    {
        rc = swmm_engine_step(e, &t);
        if (rc || t <= 0) break;
        tDay[n] = t;
        swmm_node_get_depth(e, j1, &yJ1[n]);
        swmm_link_get_flow(e, c1, &qC1[n]);
        if (i0300 < 0 && t >= 0.125 - 1.0e-9) i0300 = n;
        n++;
    }
    if (!rc) rc = swmm_engine_end(e);
    nwarn = swmm_get_warning_count(e);
    for (i = 0; i < nwarn; i++) printf("warning: %s\n", swmm_get_warning_at(e, i));
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc || i0300 < 0)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
        return 1;
    }
    printf("%-34s %10s %10s\n", "", "J1 depth", "C1 flow");
    printf("%-34s %10s %10s\n", "", "(ft)", "(cfs)");
    hms(tDay[i0300], tbuf);
    printf("run at %-27s %10.4f %10.4f\n", tbuf, yJ1[i0300], qC1[i0300]);
    printf("%-34s %10.4f %10.4f\n", "run at 06:00:00.0 (end)", yJ1[n-1], qC1[n-1]);

    f = fopen("IO-43_0300.hsf", "rb");
    if (!f)
    {
        printf("FAIL: IO-43_0300.hsf, asked for at 03:00:00, was not written\n");
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
    printf("%-34s %10.4f %10.4f\n", "IO-43_0300.hsf", rec[0], rec[6]);

    for (i = 0; i < n; i++)
    {
        d = fabs(yJ1[i] - rec[0]) + fabs(qC1[i] - rec[6]);
        if (d < best) { best = d; ib = i; }
    }
    hms(tDay[ib], tbuf);
    printf("The file holds the state of the run at %s\n", tbuf);
    if (ib != i0300 || best > 0.001)
    {
        printf("FAIL: the hot start file asked for at 03:00:00 holds the state "
               "at %s (C1 %.4f cfs instead of %.4f cfs)\n", tbuf, rec[6],
               qC1[i0300]);
        return 1;
    }
    printf("PASS: the hot start file holds the state at the requested 03:00:00\n");
    return 0;
}
