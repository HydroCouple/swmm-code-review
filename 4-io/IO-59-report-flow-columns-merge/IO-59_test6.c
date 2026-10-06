/*
 * IO-59 for 6.0.0: same check as IO-59_test.c. DefaultReportPlugin writes
 * the Node Inflow Summary with the same back-to-back %9.2f fields.
 *
 * The deck (GPM units) sends a constant 150,000 GPM (334 cfs) through one
 * junction and one 6-ft pipe to an outfall. The test runs it, writes the
 * report and reads the Node Inflow Summary rows of the junction J1 and the
 * outfall O1 as whitespace-separated fields: name, type, maximum lateral
 * inflow, maximum total inflow, days, hr:min.
 *
 * Correct behaviour: the fields can be read back. J1's maximum lateral and
 * total inflow are the 150,000 GPM it receives; O1 has no lateral inflow and
 * a maximum total inflow within 1 % of 150,000 GPM (the pipe passes the
 * steady inflow; its start-up surge is 0.2 %). When the columns run
 * together, the two numbers are read as one and the next field as ".00".
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"

int main(void)
{
    const char *ids[2] = { "J1", "O1" };
    const double wantLat[2] = { 150000.0, 0.0 };
    char line[512], name[64], type[64], rows[2][512] = { "", "" };
    double lat, tot;
    int err, k, n, d, h, m, inTable = 0, ok = 1;
    FILE *f;

    err = swmm_engine_run("IO-59_trunk-gpm.inp", "IO-59_6.rpt", "IO-59_6.out", NULL);
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }

    f = fopen("IO-59_6.rpt", "r");
    if (!f) { printf("FAIL: cannot read IO-59_6.rpt\n"); return 1; }
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Node Inflow Summary")) inTable = 1;
        else if (strstr(line, "Summary")) inTable = 0;
        if (!inTable || sscanf(line, "%63s", name) != 1) continue;
        line[strcspn(line, "\r\n")] = '\0';
        for (k = 0; k < 2; k++)
            if (strcmp(name, ids[k]) == 0 && !rows[k][0]) strcpy(rows[k], line);
    }
    fclose(f);

    printf("Node Inflow Summary rows:\n");
    for (k = 0; k < 2; k++) printf("%.66s\n", rows[k]);
    printf("\n      read as:  max lateral   max total   (GPM)\n");
    for (k = 0; k < 2; k++)
    {
        lat = tot = -1.0;
        n = sscanf(rows[k], "%63s %63s %lf %lf %d %d:%d", name, type, &lat, &tot, &d, &h, &m);
        printf("  %-4s        %12.2f  %12.2f   (%d of 7 fields)\n", ids[k], lat, tot, n);
        if (n != 7 || lat < wantLat[k] - 0.01 || lat > wantLat[k] + 0.01 ||
            tot < 148500.0 || tot > 151500.0)
            ok = 0;
    }
    if (!ok)
    {
        printf("FAIL: the maximum lateral and total inflow columns of the Node Inflow "
               "Summary run together for flows of 150,000 GPM\n");
        return 1;
    }
    printf("PASS: the Node Inflow Summary columns stay separate and read back as "
           "150,000 GPM\n");
    return 0;
}
