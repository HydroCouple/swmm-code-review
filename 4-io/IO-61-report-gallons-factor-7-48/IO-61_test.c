/*
 * IO-61: the summary tables convert ft3 to 10^6 US gallons with 7.48 gal/ft3,
 * the continuity tables with MGDperCFS / SECperDAY = 7.48056 gal/ft3.
 *
 * statsrpt_writeReport() sets Vcf = 7.48 / 1.0e6 for every volume in the
 * summary tables (runoff, node inflow, flooding, outfall loading, pumping);
 * report_writeFlowError() uses MGDperCFS / SECperDAY. The same water
 * therefore prints 0.0074 % smaller in the summary tables, which shows at
 * three decimals once a volume exceeds about 14 million gallons.
 *
 * The deck sends a constant 100 cfs to outfall O1 for 10 days (about 646
 * million gallons). With one outfall, the Outfall Loading Summary's Total
 * Volume and the Flow Routing Continuity's External Outflow are the same
 * water.
 *
 * Correct behaviour: the two agree within 0.005 million gallons. Both are
 * printed to 0.001 and accumulated in different places, so a few units in
 * the last digit are allowed; the 7.48 factor makes them differ by 0.048.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "swmm5.h"

#define RPT "IO-61.rpt"

/* Finds the line after `title` whose first token is `key` and returns the
 * number in column `col` (0 = first number after the label). */
static int readValue(const char *title, const char *key, int col, double *x)
{
    char line[512];
    int inTable = 0;
    FILE *f = fopen(RPT, "r");
    if (!f) return 0;
    while (fgets(line, sizeof line, f))
    {
        char *p;
        int k;
        if (strstr(line, title)) inTable = 1;
        if (!inTable || !(p = strstr(line, key))) continue;
        if (strncmp(line + strspn(line, " "), key, strlen(key)) != 0) continue;
        p += strlen(key);
        while (*p == ' ' || *p == '.') p++;
        for (k = 0; k <= col; k++)
        {
            *x = strtod(p, &p);
        }
        printf("  %s", line);
        fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}

int main(void)
{
    double elapsed = 0.0, outfallVol = 0.0, extOut = 0.0, af = 0.0;
    int err;

    err = swmm_open("IO-61_ten-days-100cfs.inp", RPT, "IO-61.out");
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

    printf("From %s:\n", RPT);
    if (!readValue("Flow Routing Continuity", "External Outflow", 1, &extOut) ||
        !readValue("Flow Routing Continuity", "External Outflow", 0, &af) ||
        !readValue("Outfall Loading Summary", "O1", 3, &outfallVol))
    {
        printf("FAIL: a table is missing from %s\n", RPT);
        return 1;
    }
    printf("\nvolume leaving through O1            10^6 gal\n");
    printf("  Flow Routing Continuity          %10.3f   (%.3f acre-ft)\n", extOut, af);
    printf("  Outfall Loading Summary          %10.3f\n", outfallVol);
    printf("  difference                       %10.3f\n", outfallVol - extOut);
    printf("  ratio                            %.7f   (7.48056 / 7.48 = %.7f)\n",
           extOut / outfallVol, 0.64632 / 86400.0 / 7.48e-6);

    if (fabs(outfallVol - extOut) > 0.005)
    {
        printf("FAIL: the same volume prints as %.3f million gallons in the Outfall "
               "Loading Summary and %.3f in Flow Routing Continuity\n", outfallVol, extOut);
        return 1;
    }
    printf("PASS: both tables print the outfall volume as %.3f million gallons "
           "(within rounding)\n", extOut);
    return 0;
}
