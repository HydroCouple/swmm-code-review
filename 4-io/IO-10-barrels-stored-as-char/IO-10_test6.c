/*
 * IO-10 for 6.0.0 (same decks and check as IO-10_test.c). 6.0.0 stores the
 * barrel count as an int (LinkSubtypes.hpp), so it is expected to pass.
 *
 * IO-10: the number of barrels is stored in a char, so values above 127 wrap.
 *
 * link_readXsectParams() reads the Barrels item with atoi() and stores it with
 * (char)i in TConduit.barrels, which is declared char. 300 becomes 44; 200
 * becomes -56 where char is signed (x86) and is then rejected by
 * link_validate() as "invalid number of barrels", but stays 200 where char
 * is unsigned (ARM).
 *
 * IO-10_300-barrels.inp and IO-10_200-barrels.inp each have one conduit of
 * 1 ft pipes on a 0.1 % slope carrying 100 cfs. One such pipe flows about
 * 1.1 cfs full, so 300 or 200 barrels carry 100 cfs part full.
 *
 * Correct behaviour: both decks run and the report's Cross Section Summary
 * shows the number of barrels that was entered. The test also prints the
 * total full-flow capacity (barrels x full flow per barrel) and the depth at
 * the inlet junction J1 at the end of the run, to show the effect of a wrong
 * count.
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

/* barrels and per-barrel full flow of C1 from the Cross Section Summary */
static int readC1(const char *rpt, int *barrels, double *qfull)
{
    FILE *f = fopen(rpt, "r");
    char line[512], name[64], shape[64];
    double d, a, r, w, q;
    int nb, in = 0, found = 0;
    if (!f) return 0;
    while (!found && fgets(line, sizeof line, f))
    {
        if (strstr(line, "Cross Section Summary")) { in = 1; continue; }
        if (in && sscanf(line, "%63s %63s %lf %lf %lf %lf %d %lf", name, shape, &d, &a, &r, &w,
                         &nb, &q) == 8 && strcmp(name, "C1") == 0)
        {
            *barrels = nb; *qfull = q; found = 1;
        }
    }
    fclose(f);
    return found;
}

static int run(const char *inp, const char *rpt, const char *out, double *yend)
{
    SWMM_Engine e = swmm_engine_create();
    double t = 0.0, y;
    int j1, rc = swmm_engine_open(e, inp, rpt, out, NULL);
    *yend = 0.0;
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    j1 = rc ? -1 : swmm_node_index(e, "J1");
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        if (swmm_node_get_depth(e, j1, &y) == 0) *yend = y;
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

int main(void)
{
    static const int entered[2] = {300, 200};
    int i, nbad = 0;
    char bad[256] = "";

    printf("Barrels   error  Barrels in  Full flow per  Total capacity  J1 depth\n");
    printf("entered          report      barrel (cfs)   (cfs)           at end (ft)\n");
    for (i = 0; i < 2; i++)
    {
        char inp[64], rpt[64], out[64];
        double qfull = 0.0, yend;
        int err, nb = 0, found;
        snprintf(inp, sizeof inp, "IO-10_%d-barrels.inp", entered[i]);
        snprintf(rpt, sizeof rpt, "IO-10_%d-barrels.rpt", entered[i]);
        snprintf(out, sizeof out, "IO-10_%d-barrels.out", entered[i]);
        err = run(inp, rpt, out, &yend);
        found = readC1(rpt, &nb, &qfull);
        if (err || !found)
            printf("%7d   %5d  (rejected)\n", entered[i], err);
        else
            printf("%7d   %5d  %10d  %13.2f  %14.1f  %9.2f\n", entered[i], err, nb, qfull,
                   nb * qfull, yend);
        if (err || !found || nb != entered[i])
        {
            char m[96];
            if (err || !found)
                snprintf(m, sizeof m, "%s%d barrels rejected (error %d)", nbad ? ", " : "",
                         entered[i], err);
            else
                snprintf(m, sizeof m, "%s%d barrels became %d", nbad ? ", " : "", entered[i], nb);
            strncat(bad, m, sizeof bad - strlen(bad) - 1);
            nbad++;
        }
    }
    if (nbad)
    {
        printf("FAIL: %s\n", bad);
        return 1;
    }
    printf("PASS: the conduits have the number of barrels that was entered\n");
    return 0;
}
