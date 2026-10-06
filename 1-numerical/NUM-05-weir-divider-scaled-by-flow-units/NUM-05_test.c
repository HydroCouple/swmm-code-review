/*
 * NUM-05: a WEIR divider's Cd*Ht^1.5 is divided by UCF(FLOW).
 *
 * Every deck holds the same physical system: 3 cfs into a WEIR divider with
 * Ht = 1 ft and Cd = 3.33 (cfs per ft^1.5), or Ht = 0.3048 m and Cd = 0.5604
 * (cms per m^1.5) in the SI decks, written in each of the six flow units.
 * Kinematic wave routing, steady inflow, so after 3 hours the split is exact.
 *
 * Correct behaviour (SWMM Reference Manual Vol. II, Sec. 4: c_W in ft and
 * sec units; weir links use the same CFS/CMS convention): Qmax = Cd*Ht^1.5
 * is a flow in cfs (US) or cms (SI) whatever the flow units, and the diverted
 * flow is Qmax * f^1.5 with f = (Qin - Qmin)/(Qmax - Qmin). With Qmin = 0
 * that is 94.9% of the inflow in every deck.
 *
 * The test also runs NUM-05_invalid.inp (CFS, Qmin 4 cfs > Qmax 3.33 cfs,
 * inflow 5 cfs), which the input manual says must be rejected with ERROR 137.
 *
 * Tolerance: the diverted fraction must match the weir equation within 0.01.
 * Kinematic-wave steady state matches it to about 1e-6; the unit error gives
 * fractions of 0.76 (MGD), 0.11 (MLD), 0.05 (GPM) or 0.03 (LPS), or no run.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

typedef struct {
    const char *name;   /* deck suffix */
    double qmin;        /* Qmin in cfs */
    double cd, ht;      /* deck values */
    int    si;          /* 1 = SI deck: Cd*Ht^1.5 is in cms */
} Case;

#define CMS_PER_CFS 0.028317  /* M3perFT3, as weir links convert CMS */

static int run_case(const Case *c, double *frac, int *err)
{
    char inp[64], rpt[64], out[64];
    double t = 0.0, qm = 0.0, qd = 0.0;
    int lm, ld;

    sprintf(inp, "NUM-05_%s.inp", c->name);
    sprintf(rpt, "NUM-05_%s.rpt", c->name);
    sprintf(out, "NUM-05_%s.out", c->name);
    *err = swmm_open(inp, rpt, out);
    if (!*err) *err = swmm_start(0);
    while (!*err)
    {
        *err = swmm_step(&t);
        if (t <= 0.0) break;
        lm = swmm_getIndex(swmm_LINK, "CM");
        ld = swmm_getIndex(swmm_LINK, "CD");
        qm = swmm_getValue(swmm_LINK_FLOW, lm);
        qd = swmm_getValue(swmm_LINK_FLOW, ld);
    }
    swmm_end();
    swmm_close();
    *frac = qd / (qm + qd);
    return *err;
}

int main(void)
{
    Case cases[] = {
        {"cfs", 0.0, 3.33, 1.0, 0},   {"gpm", 0.0, 3.33, 1.0, 0},
        {"mgd", 0.0, 3.33, 1.0, 0},   {"cms", 0.0, 0.5604, 0.3048, 1},
        {"lps", 0.0, 0.5604, 0.3048, 1}, {"mld", 0.0, 0.5604, 0.3048, 1},
        {"gpm_qmin", 1.0, 3.33, 1.0, 0}
    };
    int n = sizeof(cases) / sizeof(cases[0]), i, err, bad = 0;
    double qin = 3.0, frac, expect, qmax, f;
    Case invalid = {"invalid", 4.0, 3.33, 1.0, 0};

    printf("Deck        Qmax(cfs)  expected  diverted  error\n");
    for (i = 0; i < n; i++)
    {
        const Case *c = &cases[i];
        qmax = c->cd * pow(c->ht, 1.5) / (c->si ? CMS_PER_CFS : 1.0);
        f = (qin - c->qmin) / (qmax - c->qmin);
        expect = qmax * pow(f, 1.5) / qin;
        run_case(c, &frac, &err);
        printf("%-10s  %8.4f  %8.4f  %8.4f  %5d\n", c->name, qmax, expect,
               err ? NAN : frac, err);
        if (err || !(fabs(frac - expect) < 0.01)) bad++;
    }

    run_case(&invalid, &frac, &err);
    printf("invalid: Qmin 4 cfs > Qmax 3.33 cfs, inflow 5 cfs -> error %d "
           "(expected 137)\n", err);

    if (bad || err != 137)
    {
        printf("FAIL: %d of %d decks do not divert the weir-equation fraction "
               "of the inflow%s\n", bad, n,
               err != 137 ? "; the invalid divider was not rejected" : "");
        return 1;
    }
    printf("PASS: the same WEIR divider diverts the same fraction in every "
           "flow unit, and an invalid one is rejected\n");
    return 0;
}
