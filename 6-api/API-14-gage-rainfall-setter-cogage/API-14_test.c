/*
 * API-14: swmm_setValue(swmm_GAGE_RAINFALL) is ignored for a gage that shares
 * its rain time series with an earlier gage.
 *
 * Gages G1 and G2 read the same series TS1 (0.5 in/hr for 3 hours); G3 reads
 * TS3, a copy of TS1 under another name. Subcatchments S1, S2 and S3 are
 * identical (10 ac, impervious, no depression storage) and drain G1, G2 and
 * G3. Before every step the test sets the rainfall of G2 and G3 to 1.0 in/hr
 * through the API and leaves G1 alone. A gage's API rainfall replaces its own
 * data, so for the 3-hour run:
 *   S1 receives 0.5 in/hr (1.50 in), S2 and S3 receive 1.0 in/hr (3.00 in);
 *   S2 and S3 have the same runoff;
 *   the rainfall written to the .out for S2 and S3 is 1.0 in/hr at every
 *   reporting period.
 * Total Precip is read from the report's Subcatchment Runoff Summary (two
 * decimals; tolerance 0.02 in against a wrong value of 1.50 in), peak runoff
 * from swmm_getValue (S2 and S3 must agree within 0.1 %; the wrong S2 peak is
 * about half of S3's), the reported rainfall from the .out (tolerance 0.001
 * in/hr against a wrong value of 0.5).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"
#include "swmm_output.h"

#define INP "API-14_shared-series.inp"
#define RPT "API-14.rpt"
#define OUT "API-14.out"

static int readTotalPrecip(double precip[3])
{
    char line[512], name[32];
    double p;
    int found = 0, inTable = 0;
    FILE *f = fopen(RPT, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f) && found < 3)
    {
        if (strstr(line, "Subcatchment Runoff Summary")) inTable = 1;
        if (!inTable) continue;
        if (sscanf(line, " %31s %lf", name, &p) == 2 && name[0] == 'S'
            && name[1] >= '1' && name[1] <= '3' && name[2] == '\0')
        {
            precip[name[1] - '1'] = p;
            found++;
        }
    }
    fclose(f);
    return found == 3;
}

int main(void)
{
    const double expPrecip[3] = {1.5, 3.0, 3.0};
    double t = 0.0, q, peak[3] = {0.0, 0.0, 0.0}, precip[3] = {-1.0, -1.0, -1.0};
    double rptMin[3] = {1e10, 1e10, 1e10}, rptMax[3] = {-1e10, -1e10, -1e10};
    int err, i, g2, g3, per, nper = 0, ok = 1;
    SMO_Handle h = NULL;

    err = swmm_open(INP, RPT, OUT);
    if (!err) err = swmm_start(1);
    g2 = swmm_getIndex(swmm_GAGE, "G2");
    g3 = swmm_getIndex(swmm_GAGE, "G3");
    while (!err)
    {
        swmm_setValue(swmm_GAGE_RAINFALL, g2, 1.0);
        swmm_setValue(swmm_GAGE_RAINFALL, g3, 1.0);
        err = swmm_step(&t);
        if (t <= 0.0) break;
        for (i = 0; i < 3; i++)
        {
            q = swmm_getValue(swmm_SUBCATCH_RUNOFF, i);
            if (q > peak[i]) peak[i] = q;
        }
    }
    swmm_end();
    swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    if (!readTotalPrecip(precip))
    {
        printf("FAIL: could not read the Subcatchment Runoff Summary\n");
        return 1;
    }

    /* rainfall written to the output file for each subcatchment */
    SMO_init(&h);
    if (SMO_open(h, OUT) || SMO_getTimes(h, SMO_numPeriods, &nper) || nper <= 0)
    {
        printf("FAIL: could not read %s\n", OUT);
        return 1;
    }
    for (i = 0; i < 3; i++)
    {
        float *v = NULL;
        int len = 0;
        if (SMO_getSubcatchSeries(h, i, SMO_rainfall_subcatch, 0, nper - 1, &v, &len) == 0)
        {
            /* the last period (3:00) is past the end of the series */
            for (per = 0; per < len - 1; per++)
            {
                if (v[per] < rptMin[i]) rptMin[i] = v[per];
                if (v[per] > rptMax[i]) rptMax[i] = v[per];
            }
        }
        SMO_free((void **)&v);
    }
    SMO_close(&h);

    printf("Subcatch  gage  API rain   Total Precip  expected   peak runoff   .out rainfall\n");
    printf("                (in/hr)    (in)          (in)       (cfs)         min - max (in/hr)\n");
    for (i = 0; i < 3; i++)
    {
        printf("S%d        G%d    %-8s   %8.2f      %6.2f   %10.3f     %.3f - %.3f\n",
               i + 1, i + 1, i ? "1.0" : "-", precip[i], expPrecip[i], peak[i],
               rptMin[i], rptMax[i]);
        if (fabs(precip[i] - expPrecip[i]) > 0.02) ok = 0;
    }
    if (fabs(peak[1] - peak[2]) > 0.001 * peak[2]) ok = 0;
    for (i = 1; i < 3; i++)
        if (fabs(rptMin[i] - 1.0) > 0.001 || fabs(rptMax[i] - 1.0) > 0.001) ok = 0;

    if (!ok)
    {
        printf("FAIL: the API rainfall is not used: S2 got %.2f in (peak %.3f cfs, "
               "reported %.3f in/hr), S3 %.2f in (peak %.3f cfs, reported %.3f in/hr); "
               "both should get 3.00 in at 1.000 in/hr\n",
               precip[1], peak[1], rptMax[1], precip[2], peak[2], rptMax[2]);
        return 1;
    }
    printf("PASS: G2 and G3 both use their API rainfall (3.00 in, same runoff); "
           "G1 keeps its series (1.50 in)\n");
    return 0;
}
