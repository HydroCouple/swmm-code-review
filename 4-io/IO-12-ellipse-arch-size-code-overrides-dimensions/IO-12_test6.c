/*
 * IO-12 for 6.0.0 (same decks and checks as IO-12_test.c). XSection.cpp has
 * the same size-code logic as legacy xsect_setParams().
 *
 * IO-12: for HORIZ_ELLIPSE, VERT_ELLIPSE and ARCH, a zero Geom2 makes Geom1
 * the size code (the SWMM 5.0 form), even when Geom3 holds a size code, and a
 * size code that is not a whole number is truncated without a message.
 *
 * The manual ([XSECTIONS], Table D-2): Geom1 = full height, Geom2 = max.
 * width, Geom3 = size code of a standard pipe from Appendix A12/A13, "leave
 * blank (or 0) if the pipe has custom dimensions". The standard dimensions
 * come from those tables (xsect.dat):
 *   ellipse code 4: 24 x 38 in -> 2.00 ft x 3.17 ft, full area 5.10 ft2
 *   arch code 12:   62 x 102 in -> 5.17 ft high, 8.50 ft wide, full area 34.60 ft2
 *
 * Correct behaviour:
 *   IO-12_size-codes.inp       runs; C1 HORIZ_ELLIPSE 2 0 4, C2 VERT_ELLIPSE
 *                              3.17 0 4 and C3 ARCH 5.17 0 12 (height and size
 *                              code given, width left to the code) get the
 *                              standard sizes of codes 4, 4 and 12, and so does
 *                              C4 HORIZ_ELLIPSE 4 0 0 (the SWMM 5.0 form).
 *   IO-12_height-as-code.inp   is rejected: HORIZ_ELLIPSE 1.2 0 0 0 (metres) has
 *                              no width, and 1.2 is not a size code either.
 *   IO-12_fractional-code.inp  is rejected: ARCH 4.5 7.0 12.5 0 has a size code
 *                              that does not exist.
 * Values are read from the report's Cross Section Summary (2 decimals), so
 * the tolerance is 0.01; the wrong results are a rejected deck or a pipe of a
 * different size.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

/* depth, area, width of C1..Cnmax from the Cross Section Summary */
static int readXsects(const char *rpt, double v[4][3], int nmax)
{
    FILE *f = fopen(rpt, "r");
    char line[512], name[64], shape[64];
    double d, a, r, w;
    int in = 0, found = 0, k;
    if (!f) return 0;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Cross Section Summary")) { in = 1; continue; }
        if (!in) continue;
        if (sscanf(line, "%63s %63s %lf %lf %lf %lf", name, shape, &d, &a, &r, &w) == 6
            && name[0] == 'C' && (k = name[1] - '1') >= 0 && k < nmax && name[2] == 0)
        {
            v[k][0] = d; v[k][1] = a; v[k][2] = w;
            if (++found == nmax) break;
        }
    }
    fclose(f);
    return found;
}

static int run(const char *inp, const char *rpt, const char *out)
{
    return swmm_engine_run(inp, rpt, out, NULL);
}

int main(void)
{
    static const char *xs[4] = {"HORIZ_ELLIPSE 2 0 4", "VERT_ELLIPSE 3.17 0 4",
                                "ARCH 5.17 0 12", "HORIZ_ELLIPSE 4 0 0"};
    static const double expect[4][3] = {{2.00, 5.10, 3.17}, {3.17, 5.10, 2.00},
                                        {5.17, 34.60, 8.50}, {2.00, 5.10, 3.17}};
    double v[4][3] = {{0}};
    int err, k, n, nbad = 0;
    char bad[512] = "";

    /* 1. standard sizes given by size code */
    err = run("IO-12_size-codes.inp", "IO-12_size-codes.rpt", "IO-12_size-codes.out");
    n = readXsects("IO-12_size-codes.rpt", v, 4);
    printf("IO-12_size-codes.inp: error %d\n", err);
    printf("Link  Xsection                Depth    Area   Width   expected\n");
    for (k = 0; k < 4; k++)
    {
        int ok = n == 4 && fabs(v[k][0] - expect[k][0]) <= 0.01
                 && fabs(v[k][1] - expect[k][1]) <= 0.01 && fabs(v[k][2] - expect[k][2]) <= 0.01;
        if (n == 4)
            printf("C%d    %-22s %7.2f %7.2f %7.2f   %.2f %.2f %.2f%s\n", k + 1, xs[k],
                   v[k][0], v[k][1], v[k][2], expect[k][0], expect[k][1], expect[k][2],
                   ok ? "" : "   <-- wrong");
        if (!ok && n == 4)
        {
            char m[64];
            snprintf(m, sizeof m, "%sC%d %s has the wrong size", nbad ? ", " : "", k + 1, xs[k]);
            strncat(bad, m, sizeof bad - strlen(bad) - 1);
            nbad++;
        }
    }
    if (err || n < 4)
    {
        printf("      (deck rejected, no cross sections reported)\n");
        snprintf(bad + strlen(bad), sizeof bad - strlen(bad),
                 "%sIO-12_size-codes.inp rejected with error %d", nbad ? ", " : "", err);
        nbad++;
    }

    /* 2. and 3. invalid size codes must be rejected */
    {
        static const char *deck[2] = {"IO-12_height-as-code", "IO-12_fractional-code"};
        static const char *what[2] = {"HORIZ_ELLIPSE 1.2 0 0 0 (m)", "ARCH 4.5 7.0 12.5 0"};
        static const char *unit[2] = {"m", "ft"};
        int i;
        for (i = 0; i < 2; i++)
        {
            char inp[64], rpt[64], out[64];
            double w[4][3] = {{0}};
            snprintf(inp, sizeof inp, "%s.inp", deck[i]);
            snprintf(rpt, sizeof rpt, "%s.rpt", deck[i]);
            snprintf(out, sizeof out, "%s.out", deck[i]);
            err = run(inp, rpt, out);
            if (err)
            {
                printf("%s: %s rejected with error %d\n", inp, what[i], err);
                continue;
            }
            readXsects(rpt, w, 1);
            printf("%s: %s accepted, C1 built as depth %.2f %s, width %.2f %s\n",
                   inp, what[i], w[0][0], unit[i], w[0][2], unit[i]);
            snprintf(bad + strlen(bad), sizeof bad - strlen(bad),
                     "%s%s accepted as a %.2f %s high pipe", nbad ? ", " : "", what[i],
                     w[0][0], unit[i]);
            nbad++;
        }
    }

    if (nbad)
    {
        printf("FAIL: %s\n", bad);
        return 1;
    }
    printf("PASS: size codes in Geom3 and in the 5.0 form give the standard sizes, "
           "and invalid size codes are rejected\n");
    return 0;
}
