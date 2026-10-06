/*
 * IO-48: the routing interface file (SAVE OUTFLOWS) is stamped with the
 * report time but holds the outfall state at the end of the routing step
 * that passed it.
 *
 * output_saveResults() writes node and link results interpolated to the
 * report time between OldRoutingTime and NewRoutingTime, then calls
 * iface_saveOutletResults(), which writes Node.inflow and Node.newQual, the
 * values at NewRoutingTime, under the report date. When a report time falls
 * inside a routing step the interface hydrograph is shifted early by up to
 * one step.
 *
 * Correct behaviour: a row of the interface file stamped T holds the outfall
 * inflow and concentration at T, which is what the same run reports for T in
 * its .out file. IO-48_ramp.inp ramps flow and TSS into a KINWAVE network
 * with 40-s routing steps and 1-minute reports; the test compares every O1
 * row of IO-48_iface.txt with the .out values for O1 at the same time.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

/* ---- reading the binary results file (format of SWMM 5 .out files) ---- */
static int OutNs, OutNn, OutNl, OutNsv, OutNnv, OutNlv, OutNsys, OutNper;
static long OutPos;
static FILE *OutFile;

static int rd_int(long pos)
{
    int v = 0;
    fseek(OutFile, pos, SEEK_SET);
    if (fread(&v, 4, 1, OutFile) != 1) v = 0;
    return v;
}

static int out_open(const char *path)
{
    int hdr[7], ep[6], n;
    long p;
    OutFile = fopen(path, "rb");
    if (!OutFile) return 0;
    if (fread(hdr, 4, 7, OutFile) != 7) return 0;
    OutNs = hdr[3]; OutNn = hdr[4]; OutNl = hdr[5];
    fseek(OutFile, -24L, SEEK_END);
    if (fread(ep, 4, 6, OutFile) != 6) return 0;
    OutPos = ep[2]; OutNper = ep[3];
    p = ep[1];                                         /* input properties */
    n = rd_int(p); p += 4 + 4L * n + 4L * OutNs * n;
    n = rd_int(p); p += 4 + 4L * n + 4L * OutNn * n;
    n = rd_int(p); p += 4 + 4L * n + 4L * OutNl * n;
    OutNsv = rd_int(p);  p += 4 + 4L * OutNsv;         /* reported variables */
    OutNnv = rd_int(p);  p += 4 + 4L * OutNnv;
    OutNlv = rd_int(p);  p += 4 + 4L * OutNlv;
    OutNsys = rd_int(p);
    return OutNper > 0;
}

/* node variable v (4 = total inflow, 6 = first pollutant) of node n in period k */
static double out_node(int k, int n, int v)
{
    float x = 0.0f;
    long rec = 8 + 4L * (OutNs * OutNsv + OutNn * OutNnv + OutNl * OutNlv + OutNsys);
    fseek(OutFile, OutPos + k * rec + 8 + 4L * (OutNs * OutNsv + n * OutNnv + v), SEEK_SET);
    if (fread(&x, 4, 1, OutFile) != 1) x = -1.0f;
    return x;
}

/* ---- compare the interface file's O1 rows with the .out file ---- */
static int compare(const char *ifaceName, int o1)
{
    char line[256], id[64];
    int yr, mo, dy, hr, mi, se, nrows = 0, nbad = 0, inData = 0;
    double q, c, worstQ = 0.0;
    FILE *f = fopen(ifaceName, "r");
    if (!f || !out_open("IO-48.out"))
    {
        printf("FAIL: cannot read the interface file or the results file\n");
        return 1;
    }
    printf("Time   Interface file      Results file (.out)\n");
    printf("       Flow (cfs)  TSS     Flow (cfs)  TSS (mg/L)\n");
    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "Node ", 5) == 0) { inData = 1; continue; }
        if (!inData) continue;
        if (sscanf(line, "%63s %d %d %d %d %d %d %lf %lf", id, &yr, &mo, &dy, &hr, &mi, &se,
                   &q, &c) != 9 || strcmp(id, "O1") != 0) continue;
        {
            int k = (hr * 3600 + mi * 60 + se) / 60 - 1;   /* .out period of this time */
            double qo, co;
            int bad;
            if (k < 0 || k >= OutNper) continue;            /* 00:00 has no .out period */
            qo = out_node(k, o1, 4);
            co = out_node(k, o1, 6);
            /* tolerance: the .out holds 4-byte floats and the interface file
               prints 6 decimals; the shift from a missing interpolation is
               a whole fraction of a 40-s step of a 1 cfs/min ramp (> 0.1 cfs) */
            bad = fabs(q - qo) > 1.0e-3 + 1.0e-5 * fabs(qo) || fabs(c - co) > 1.0e-2 + 1.0e-5 * fabs(co);
            nrows++;
            if (bad) nbad++;
            if (fabs(q - qo) > fabs(worstQ)) worstQ = q - qo;
            if (mi <= 8 || mi == 30)
                printf("%02d:%02d  %10.4f  %7.3f  %10.4f  %10.3f%s\n", hr, mi, q, c, qo, co,
                       bad ? "  <-- differs" : "");
        }
    }
    fclose(f);
    fclose(OutFile);
    if (nrows == 0)
    {
        printf("FAIL: no outfall rows found in the interface file\n");
        return 1;
    }
    if (nbad)
    {
        printf("FAIL: %d of %d interface-file rows do not hold the outfall state at their "
               "time stamp (largest flow difference %+.4f cfs)\n", nbad, nrows, worstQ);
        return 1;
    }
    printf("PASS: all %d interface-file rows match the outfall results at their time stamp\n",
           nrows);
    return 0;
}

int main(void)
{
    double t = 0.0;
    int err, o1;
    err = swmm_open("IO-48_ramp.inp", "IO-48.rpt", "IO-48.out");
    if (!err) err = swmm_start(1);
    o1 = swmm_getIndex(swmm_NODE, "O1");
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
    }
    swmm_end();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    return compare("IO-48_iface.txt", o1);
}
