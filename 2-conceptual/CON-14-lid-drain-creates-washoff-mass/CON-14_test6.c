/*
 * CON-14 for 6.0.0: when a LID underdrain releases more water than the subcatchment
 * generates in a step, the surplus is exported with the current washoff
 * concentration while the BMP removal term only ever grows, so pollutant mass
 * is created.
 *
 * S1 (10 ac, 50 % impervious) has a 1 ac bio-retention cell with an underdrain
 * and no seepage. Washoff is an EMC of 100 mg/L under a 1.25 in storm. The cell
 * takes all (CON-14_full-capture.inp) or half (CON-14_half-capture.inp) of the
 * impervious runoff. Nothing is swept, deposited or left on the surface, so the
 * pollutant generated on S1 must equal what the LID removes plus what reaches
 * the drainage network:
 *
 *     buildup + wet deposition = sweeping + infiltration + BMP removal
 *                                + remaining buildup + wet weather inflow
 *
 * The first five terms come from the Runoff Quality Continuity table, the last
 * from the Quality Routing Continuity table (the load that reached J1). With the
 * bug the BMP term also counts the mass of water that the cell later releases
 * through its drain, so removal + delivery exceeds the generated mass by 8 %
 * (half capture) to 42 % (full capture). The limit of 1 % is 8 times smaller
 * than the smallest of these errors; a correct run closes within 0.1 %.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"

/* first number after the dotted leader of the first line containing label
   that follows the first line containing section */
static int rptValue(const char* rpt, const char* section, const char* label, double* v)
{
    char line[512];
    int inSection = 0;
    FILE* f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        char* p;
        if (!inSection) { inSection = strstr(line, section) != NULL; continue; }
        p = strstr(line, label);
        if (!p) continue;
        p += strlen(label);
        p += strspn(p, " .");
        fclose(f);
        return sscanf(p, "%lf", v) == 1;
    }
    fclose(f);
    return 0;
}

int main(void)
{
    const char* name[2] = {"full-capture", "half-capture"};
    const char* rq = "Runoff Quality Continuity";
    int k, bad = 0;
    char msg[256] = "";

    printf("LID takes        generated  BMP      delivered  balance\n");
    printf("                  (lbs)     removal  to J1      error (%%)\n");
    printf("                            (lbs)    (lbs)\n");
    for (k = 0; k < 2; k++)
    {
        char inp[64], rpt[64], out[64];
        SWMM_Engine e;
        double elapsed = 0.0, init = 0, build = 0, dep = 0, sweep = 0, infil = 0,
               bmp = 0, remain = 0, wwi = 0, in, err;
        int rc;

        sprintf(inp, "CON-14_%s.inp", name[k]);
        sprintf(rpt, "CON-14_%s6.rpt", name[k]);
        sprintf(out, "CON-14_%s6.out", name[k]);
        e = swmm_engine_create();
        rc = swmm_engine_open(e, inp, rpt, out, NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 1);
        while (!rc)
        {
            rc = swmm_engine_step(e, &elapsed);
            if (elapsed <= 0.0) break;
        }
        if (!rc) rc = swmm_engine_end(e);
        if (!rc) rc = swmm_engine_report(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        if (rc ||
            !rptValue(rpt, rq, "Initial Buildup", &init) ||
            !rptValue(rpt, rq, "Surface Buildup", &build) ||
            !rptValue(rpt, rq, "Wet Deposition", &dep) ||
            !rptValue(rpt, rq, "Sweeping Removal", &sweep) ||
            !rptValue(rpt, rq, "Infiltration Loss", &infil) ||
            !rptValue(rpt, rq, "BMP Removal", &bmp) ||
            !rptValue(rpt, rq, "Remaining Buildup", &remain) ||
            !rptValue(rpt, "Quality Routing Continuity", "Wet Weather Inflow", &wwi))
        {
            printf("FAIL: the run of %s did not complete (error %d)\n", inp, rc);
            return 1;
        }
        in = init + build + dep;
        err = 100.0 * (in - (sweep + infil + bmp + remain + wwi)) / in;
        printf("%-15s %8.3f  %8.3f  %8.3f  %8.3f\n",
               k == 0 ? "all imperv." : "half imperv.", in, bmp, wwi, err);
        if (fabs(err) > 1.0)
        {
            bad++;
            sprintf(msg + strlen(msg), "%s%s %.3f lbs generated, %.3f removed + %.3f delivered (%.1f %%)",
                    bad > 1 ? "; " : " ", name[k], in, bmp + infil, wwi, err);
        }
    }
    if (bad)
    {
        printf("FAIL: removal + delivery does not equal the generated pollutant mass:%s\n", msg);
        return 1;
    }
    printf("PASS: generated mass = LID removal + mass delivered to the network within 1 %%\n");
    return 0;
}
