/*
 * IO-58: the Node Inflow Summary's "Total Inflow Volume" includes the
 * volume the node held at the start of the run.
 *
 * massbal_open() seeds the per-node inflow accumulator with the node's
 * initial volume (NodeInflow[j] = Node[j].newVolume) so that the per-node
 * continuity check balances against the final volume booked in NodeOutflow.
 * writeNodeFlows() prints that same accumulator as "Total Inflow Volume".
 * The manual defines the column as "lateral inflow plus inflow from
 * connecting links" (Chapter 4, Node Inflow table).
 *
 * The deck: storage unit SU1 (1000 ft2) starts 5 ft deep (5000 ft3 stored),
 * receives 1 cfs of lateral inflow for 2 hours (7200 ft3) and has no
 * inflowing links. So its total inflow volume must equal its lateral inflow
 * volume.
 *
 * Correct behaviour: Total Inflow Volume = Lateral Inflow Volume for SU1,
 * within 2 % (both are printed with 3 significant digits and are
 * accumulated slightly differently; the initial volume adds 69 %).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define RPT "IO-58.rpt"

/* Reads the lateral and total inflow volumes of `node` from the Node Inflow
 * Summary. Returns 1 if found. */
static int readNodeInflow(const char *rpt, const char *node, double *lat, double *tot)
{
    char line[512], name[64], type[32], tm[16];
    double maxLat, maxTot;
    int days, inTable = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Node Inflow Summary")) inTable = 1;
        if (!inTable) continue;
        if (sscanf(line, "%63s %31s %lf %lf %d %15s %lf %lf", name, type,
                   &maxLat, &maxTot, &days, tm, lat, tot) == 8 &&
            strcmp(name, node) == 0)
        {
            printf("Node Inflow Summary row:\n%s", line);
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int main(void)
{
    double elapsed = 0.0, lat = 0.0, tot = 0.0, initVol;
    int err;

    err = swmm_open("IO-58_storage-initial-volume.inp", RPT, "IO-58.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    if (!err) swmm_report();
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    if (!readNodeInflow(RPT, "SU1", &lat, &tot))
    {
        printf("FAIL: no row for SU1 in the Node Inflow Summary of %s\n", RPT);
        return 1;
    }
    initVol = 5000.0 * 7.48052 / 1.0e6;   /* 5 ft x 1000 ft2, in 10^6 gal */
    printf("\nSU1 (no inflowing links)       10^6 gal\n");
    printf("  lateral inflow volume        %.4f\n", lat);
    printf("  total inflow volume          %.4f\n", tot);
    printf("  initial stored volume        %.4f  (5000 ft3)\n", initVol);
    printf("  total - lateral              %.4f\n", tot - lat);

    if (lat <= 0.0 || fabs(tot - lat) > 0.02 * lat)
    {
        printf("FAIL: SU1 has no inflowing links, but its Total Inflow Volume "
               "(%.4f) exceeds its Lateral Inflow Volume (%.4f) by %.4f, "
               "its initial stored volume\n", tot, lat, tot - lat);
        return 1;
    }
    printf("PASS: SU1's Total Inflow Volume equals its Lateral Inflow Volume\n");
    return 0;
}
