/*
 * IO-29 for 6.0.0 (same deck and checks as IO-29_test.c). 6.0.0's
 * handle_transects() reads the X1 line with the same item positions.
 *
 * IO-29: an X1 line written as the manual documents it is misread.
 *
 * The manual ([TRANSECTS]) gives the X1 line as
 *     X1  Name  Nsta  Xleft  Xright  0  0  0  Lfactor  Wfactor  Eoffset   (11 items)
 * The parser reads Lfactor, Wfactor and Eoffset from items 8-10, i.e. the
 * layout with two placeholder zeros that the GUI writes:
 *     X1  Name  Nsta  Xleft  Xright  0  0  Lfactor  Wfactor  Eoffset      (10 items)
 *
 * IO-29_x1-layouts.inp defines the same channel twice, with a meander factor
 * Lfactor = 2.0 and no width factor or elevation offset: T1 (conduit C1) with
 * the 10-item line and T2 (conduit C2) with the 11-item line of the manual.
 * The GR data is a trapezoid 5 ft deep, 20 ft wide at the bottom and 40 ft at
 * the top, so for both conduits:
 *     full depth 5 ft, full area (20 + 40)/2 * 5 = 150 ft2, max width 40 ft,
 * and because the two transects are identical (the meander factor changes the
 * channel roughness, not the shape), C2's hydraulic radius and full flow must
 * equal C1's.
 *
 * The values are read from the report's Cross Section Summary (2 decimals),
 * so the tolerance is 0.01. Misreading the manual's line doubles the width
 * (Lfactor lands in Wfactor), which is a difference of 150 ft2 in area.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

/* C1/C2 from the Cross Section Summary: depth, area, hyd. radius, width, full flow */
static int readXsects(const char *rpt, double v[2][5])
{
    FILE *f = fopen(rpt, "r");
    char line[512], name[64], shape[64];
    double d, a, r, w, q;
    int nb, in = 0, found = 0;
    if (!f) return 0;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Cross Section Summary")) { in = 1; continue; }
        if (!in) continue;
        if (sscanf(line, "%63s %63s %lf %lf %lf %lf %d %lf", name, shape, &d, &a, &r, &w, &nb, &q) == 8)
        {
            int k = strcmp(name, "C1") == 0 ? 0 : strcmp(name, "C2") == 0 ? 1 : -1;
            if (k < 0) continue;
            v[k][0] = d; v[k][1] = a; v[k][2] = r; v[k][3] = w; v[k][4] = q;
            if (++found == 2) break;
        }
    }
    fclose(f);
    return found;
}

int main(void)
{
    double v[2][5] = {{0}};
    int k, err, n, ok = 1;
    const char *layout[2] = {"10 items (GUI) ", "11 items (manual)"};

    err = swmm_engine_run("IO-29_x1-layouts.inp", "IO-29.rpt", "IO-29.out", NULL);
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    n = readXsects("IO-29.rpt", v);
    if (n < 2)
    {
        printf("FAIL: C1/C2 not found in the Cross Section Summary\n");
        return 1;
    }

    printf("Link  X1 line              Depth    Area  HydRad   Width  FullFlow\n");
    printf("                            (ft)   (ft2)    (ft)    (ft)     (cfs)\n");
    for (k = 0; k < 2; k++)
        printf("C%d    %-18s %7.2f %7.2f %7.2f %7.2f %9.2f\n", k + 1, layout[k],
               v[k][0], v[k][1], v[k][2], v[k][3], v[k][4]);
    printf("expected (both)          %7.2f %7.2f %7s %7.2f   (C2 = C1)\n", 5.0, 150.0, "", 40.0);

    for (k = 0; k < 2; k++)
        if (fabs(v[k][0] - 5.0) > 0.01 || fabs(v[k][1] - 150.0) > 0.01
            || fabs(v[k][3] - 40.0) > 0.01) ok = 0;
    if (fabs(v[1][2] - v[0][2]) > 0.01 || fabs(v[1][4] - v[0][4]) > 0.01) ok = 0;

    if (!ok)
    {
        printf("FAIL: the X1 line written as documented gives full area %.2f ft2 and width %.2f ft "
               "(expected 150.00 and 40.00) and full flow %.2f cfs instead of %.2f\n",
               v[1][1], v[1][3], v[1][4], v[0][4]);
        return 1;
    }
    printf("PASS: both X1 layouts give the same transect (area 150 ft2, width 40 ft, "
           "full flow %.2f cfs)\n", v[0][4]);
    return 0;
}
