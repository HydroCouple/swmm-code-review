/*
 * NUM-05 for 6.0.0: a WEIR divider's Cd*Ht^1.5 is divided by the flow-unit
 * factor (Divider.cpp), and no ERROR 137 check exists.
 *
 * Same decks and same rule as NUM-05_test.c: Qmax = Cd*Ht^1.5 is a flow in
 * cfs (US) or cms (SI) whatever the flow units, the diverted flow is
 * Qmax * f^1.5 with f = (Qin - Qmin)/(Qmax - Qmin), 94.9% of the inflow
 * with Qmin = 0. NUM-05_invalid.inp (Qmin 4 cfs > Qmax 3.33 cfs, inflow
 * 5 cfs) must be rejected (ERROR 137 in the input manual), not run.
 *
 * Tolerance: the diverted fraction must match within 0.01 (see NUM-05_test.c).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

typedef struct {
    const char *name;
    double qmin, cd, ht;
    int si;
} Case;

#define CMS_PER_CFS 0.028317  /* M3perFT3, as weir links convert CMS */

static int run_case(const Case *c, double *frac)
{
    char inp[64], rpt[64], out[64];
    double t = 0.0, qm = 0.0, qd = 0.0;
    int rc;
    SWMM_Engine e = swmm_engine_create();

    sprintf(inp, "NUM-05_%s.inp", c->name);
    sprintf(rpt, "NUM-05_%s6.rpt", c->name);
    sprintf(out, "NUM-05_%s6.out", c->name);
    rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0) break;
        swmm_link_get_flow(e, swmm_link_index(e, "CM"), &qm);
        swmm_link_get_flow(e, swmm_link_index(e, "CD"), &qd);
    }
    if (rc) printf("  %s: rc %d, %s\n", c->name, rc, swmm_get_last_error_msg(e));
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    *frac = qd / (qm + qd);
    return rc;
}

int main(void)
{
    Case cases[] = {
        {"cfs", 0.0, 3.33, 1.0, 0},   {"gpm", 0.0, 3.33, 1.0, 0},
        {"mgd", 0.0, 3.33, 1.0, 0},   {"cms", 0.0, 0.5604, 0.3048, 1},
        {"lps", 0.0, 0.5604, 0.3048, 1}, {"mld", 0.0, 0.5604, 0.3048, 1},
        {"gpm_qmin", 1.0, 3.33, 1.0, 0}
    };
    int n = sizeof(cases) / sizeof(cases[0]), i, rc, bad = 0;
    double qin = 3.0, frac, expect, qmax, f;
    Case invalid = {"invalid", 4.0, 3.33, 1.0, 0};

    printf("Deck        Qmax(cfs)  expected  diverted     rc\n");
    for (i = 0; i < n; i++)
    {
        const Case *c = &cases[i];
        qmax = c->cd * pow(c->ht, 1.5) / (c->si ? CMS_PER_CFS : 1.0);
        f = (qin - c->qmin) / (qmax - c->qmin);
        expect = qmax * pow(f, 1.5) / qin;
        rc = run_case(c, &frac);
        printf("%-10s  %8.4f  %8.4f  %8.4f  %5d\n", c->name, qmax, expect, frac, rc);
        if (rc || !(fabs(frac - expect) < 0.01)) bad++;
    }

    rc = run_case(&invalid, &frac);
    if (rc) printf("invalid: Qmin 4 cfs > Qmax 3.33 cfs, inflow 5 cfs -> "
                   "rejected, rc %d\n", rc);
    else    printf("invalid: Qmin 4 cfs > Qmax 3.33 cfs, inflow 5 cfs -> "
                   "ran, diverted fraction %.4f (expected: rejected)\n", frac);

    if (bad || rc == 0)
    {
        printf("FAIL: %d of %d decks do not divert the weir-equation fraction "
               "of the inflow%s\n", bad, n,
               rc == 0 ? "; the invalid divider was run instead of rejected" : "");
        return 1;
    }
    printf("PASS: the same WEIR divider diverts the same fraction in every "
           "flow unit, and an invalid one is rejected\n");
    return 0;
}
