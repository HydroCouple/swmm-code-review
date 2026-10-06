/*
 * CON-19: the surface vegetation fraction of a vegetative swale and of a
 * permeable pavement counts in the stored volume but not in the depth
 * update, so a surface with VegFrac > 0 loses track of part of its water.
 *
 * The surface layer's water fills only the fraction phi = 1 - VegFrac of
 * the space above the surface (Reference Manual Vol. III, Eqs. 6-53/6-54
 * for the swale; phi1 dd1/dt = ... for the other LIDs). Both decks have
 * no evaporation and no infiltration, so at the end of the run the water
 * held in the unit must equal rain - surface runoff.
 *
 * 1. CON-19_swale.inp: a 43560 ft2 swale (the whole subcatchment), top
 *    width 20 ft (length 2178 ft), 12 in deep, side slopes 5:1 (bottom
 *    width 10 ft), VegFrac 0.5; 2.0 in of rain in the hour the run lasts.
 *    The water held at the final depth d (ft, from the LID report file) is
 *    phi * L * d * (Wb + s*d) / A.
 * 2. CON-19_pavement.inp: a 1-acre permeable pavement with 3 in of surface
 *    storage (VegFrac 0.5) and no outflow path but the pavement, which takes
 *    0.5 in/hr; 2.0 in of rain in 1 hour, the run ends at 1:30 with water
 *    ponded. Nothing leaves the unit, so final storage must be 2.0 in and
 *    the runoff continuity error 0.
 *
 * Without the fix the depth rises 1/phi times too slowly while the stored
 * volume is booked with phi: the swale holds about 1 in less than the
 * 1.93 in it received and the pavement reports 1.37 in of 2.0 in. The
 * tolerances (0.05 in, 1 %) are far below these errors and above the
 * 3-decimal rounding of the reports and the integration error of the
 * swale's depth (about 0.01 in at VegFrac = 0).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "swmm5.h"

/* number in column col (0 = acre-feet, 1 = inches) of the report line
   containing label; 0 if the line is absent */
static double rptValue(const char* rpt, const char* label, int col)
{
    char line[512];
    double v = 0.0;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0.0;
    while (fgets(line, sizeof(line), f))
    {
        char* p = strstr(line, label);
        double a, b;
        int n;
        if (!p) continue;
        p += strlen(label);
        p += strspn(p, " .");
        n = sscanf(p, "%lf %lf", &a, &b);
        if (n > col) v = col ? b : a;
        break;
    }
    fclose(f);
    return v;
}

/* surface level (in) on the last line of an LID report file; -1 if none */
static double lastSurfaceLevel(const char* fname)
{
    char line[1024], last[1024] = "";
    char* tok;
    int i;
    double v = -1.0;
    FILE* f = fopen(fname, "r");
    if (!f) return -1.0;
    while (fgets(line, sizeof(line), f))
        if (line[0] == ' ' && line[3] == '/' && line[6] == '/') strcpy(last, line);
    fclose(f);
    /* date, time, then 13 numbers; the surface level is the 10th */
    tok = strtok(last, " \t\r\n");
    for (i = 0; tok && i < 11; i++) tok = strtok(NULL, " \t\r\n");
    if (tok) v = atof(tok);
    return v;
}

static int run(const char* inp, const char* rpt, const char* out)
{
    double elapsed = 0.0;
    int err = swmm_open(inp, rpt, out);
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    return err;
}

int main(void)
{
    /* swale geometry (ft) from the deck */
    const double A = 43560.0, W1 = 20.0, D1 = 1.0, sx = 5.0, phi = 0.5;
    const double L = A / W1, Wb = W1 - 2.0 * sx * D1;
    double rain, runoff, level, d, held, sRain, pRain, pRunoff, pStore, pErr;
    int err, bad = 0;
    char msg[256] = "";

    err = run("CON-19_swale.inp", "CON-19_swale.rpt", "CON-19_swale.out");
    if (err)
    {
        printf("FAIL: the swale run stopped with error %d\n", err);
        return 1;
    }
    sRain  = rptValue("CON-19_swale.rpt", "Total Precipitation", 1);
    runoff = rptValue("CON-19_swale.rpt", "Surface Runoff", 1);
    level  = lastSurfaceLevel("CON-19_lid.txt");
    rain   = sRain;
    d      = level / 12.0;
    held   = phi * L * d * (Wb + sx * d) / A * 12.0;

    err = run("CON-19_pavement.inp", "CON-19_pavement.rpt", "CON-19_pavement.out");
    if (err)
    {
        printf("FAIL: the pavement run stopped with error %d\n", err);
        return 1;
    }
    pRain   = rptValue("CON-19_pavement.rpt", "Total Precipitation", 1);
    pRunoff = rptValue("CON-19_pavement.rpt", "Surface Runoff", 1);
    pStore  = rptValue("CON-19_pavement.rpt", "Final Storage", 1);
    pErr    = rptValue("CON-19_pavement.rpt", "Continuity Error (%)", 0);

    printf("Swale, VegFrac 0.5 (in)\n");
    printf("  rain %.3f  surface runoff %.3f  -> must hold %.3f\n",
           rain, runoff, rain - runoff);
    printf("  final water level %.3f  -> holds %.3f\n", level, held);
    printf("Permeable pavement, VegFrac 0.5 (in)\n");
    printf("  rain %.3f  surface runoff %.3f  -> must hold %.3f\n",
           pRain, pRunoff, pRain - pRunoff);
    printf("  final storage %.3f  runoff continuity error %.3f %%\n",
           pStore, pErr);

    if (rain < 1.9 || level < 0.0 || fabs(held - (rain - runoff)) > 0.05)
    {
        sprintf(msg, "the swale received %.3f in net but its final level of "
                "%.3f in holds %.3f in", rain - runoff, level, held);
        bad = 1;
    }
    else if (pRain < 1.9 || fabs(pStore - (pRain - pRunoff)) > 0.05 ||
             fabs(pErr) > 1.0)
    {
        sprintf(msg, "the pavement received %.3f in but stores %.3f in; "
                "continuity error %.3f %%", pRain - pRunoff, pStore, pErr);
        bad = 1;
    }
    if (bad)
    {
        printf("FAIL: %s\n", msg);
        return 1;
    }
    printf("PASS: with VegFrac 0.5 the water held by the swale and the "
           "pavement equals what they received\n");
    return 0;
}
