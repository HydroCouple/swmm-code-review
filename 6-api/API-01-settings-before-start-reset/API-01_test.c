/*
 * API-01: forcing values set between swmm_open and swmm_start are accepted
 * and then wiped by swmm_start.
 *
 * swmm_setValue (and in 5.3.0 swmm_setValueExpanded) accepts a gage rainfall,
 * a node lateral inflow and a link setting (5.3.0 also a subcatchment's API
 * rainfall, snowfall and PET) before swmm_start, and 5.3.0 returns 0 for them.
 * swmm_start then runs project_init(), whose *_initState() functions reset
 * every one of these fields, so the run ignores the value.
 *
 * Correct behaviour: a value the toolkit accepts is used by the run, or the
 * call is refused with an error code so the caller knows. Each case opens the
 * deck (its gage series is dry), sets one value, runs 2 hours and records what
 * the engine reports during the run:
 *   - gage rainfall 1 in/hr          -> S1 rainfall should be 1 in/hr
 *   - J2 lateral inflow 2 cfs        -> J2 lateral inflow should be 2 cfs
 *   - OR1 setting 0.25               -> OR1 setting should stay 0.25
 *   - S1 API rainfall 1 in/hr (5.3.0)-> S1 rainfall should be 1 in/hr
 *   - S1 API snowfall 0.5 in/hr      -> the property should read 0.5
 *   - S1 API PET 0.2 in/day          -> the property should read 0.2
 * A value counts as used when what the run reports is within 10% of it; the
 * reset values are 0 (rain, inflow, snowfall), 1 (setting) and -1 (PET, "not
 * set"), so the margin is wide. 5.2.4's swmm_setValue returns void, so there
 * the value must be used.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

#ifdef OPENSWMM_LEGACY_SOLVER_H_
#define V530 1          /* 5.3.0: swmm_setValue returns int, expanded API exists */
#define NCASES 9
#else
#define NCASES 3
#endif

static const char *label[9] = {
    "swmm_setValue GAGE_RAINFALL(G1)",
    "swmm_setValue NODE_LATFLOW(J2)",
    "swmm_setValue LINK_SETTING(OR1)",
    "setValueExpanded GAGE_RAINFALL(G1)",
    "setValueExpanded NODE_LATFLOW(J2)",
    "setValueExpanded LINK_SETTING(OR1)",
    "setValueExpanded SUBCATCH_API_RAINFALL(S1)",
    "setValueExpanded SUBCATCH_API_SNOWFALL(S1)",
    "setValueExpanded SUBCATCH_API_PET(S1)"
};
static const double setTo[9] = { 1.0, 2.0, 0.25, 1.0, 2.0, 0.25, 1.0, 0.5, 0.2 };
static const char *seen[9] = {
    "max S1 rainfall (in/hr)", "max J2 lateral inflow (cfs)", "max OR1 setting",
    "max S1 rainfall (in/hr)", "max J2 lateral inflow (cfs)", "max OR1 setting",
    "max S1 rainfall (in/hr)", "max S1 API snowfall (in/hr)", "max S1 API PET (in/day)"
};

/* the value the run reports for case k at the current step */
static double observe(int k, int s1, int j2, int or1)
{
    switch (k)
    {
    case 0: case 3: case 6: return swmm_getValue(swmm_SUBCATCH_RAINFALL, s1);
    case 1: case 4:         return swmm_getValue(swmm_NODE_LATFLOW, j2);
    case 2: case 5:         return swmm_getValue(swmm_LINK_SETTING, or1);
#ifdef V530
    case 7: return swmm_getValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_API_SNOWFALL, s1, 0, 0);
    case 8: return swmm_getValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_API_PET, s1, 0, 0);
#endif
    }
    return 0.0;
}

/* sets the value for case k before swmm_start; returns the call's code
   (always 0 in 5.2.4, where swmm_setValue returns void) */
static int setBeforeStart(int k, int g1, int s1, int j2, int or1)
{
    switch (k)
    {
#ifdef V530
    case 0: return swmm_setValue(swmm_GAGE_RAINFALL, g1, setTo[k]);
    case 1: return swmm_setValue(swmm_NODE_LATFLOW, j2, setTo[k]);
    case 2: return swmm_setValue(swmm_LINK_SETTING, or1, setTo[k]);
    case 3: return swmm_setValueExpanded(swmm_GAGE, swmm_GAGE_RAINFALL, g1, 0, 0, setTo[k]);
    case 4: return swmm_setValueExpanded(swmm_NODE, swmm_NODE_LATFLOW, j2, 0, 0, setTo[k]);
    case 5: return swmm_setValueExpanded(swmm_LINK, swmm_LINK_SETTING, or1, 0, 0, setTo[k]);
    case 6: return swmm_setValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_API_RAINFALL, s1, 0, 0, setTo[k]);
    case 7: return swmm_setValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_API_SNOWFALL, s1, 0, 0, setTo[k]);
    case 8: return swmm_setValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_API_PET, s1, 0, 0, setTo[k]);
#else
    case 0: swmm_setValue(swmm_GAGE_RAINFALL, g1, setTo[k]); return 0;
    case 1: swmm_setValue(swmm_NODE_LATFLOW, j2, setTo[k]); return 0;
    case 2: swmm_setValue(swmm_LINK_SETTING, or1, setTo[k]); return 0;
#endif
    }
    return 0;
}

int main(void)
{
    int k, bad = 0;

    printf("%-44s %6s %9s  %-28s %8s  %s\n",
           "value set before swmm_start", "set", "rc", "what the run reports", "", "verdict");
    for (k = 0; k < NCASES; k++)
    {
        double elapsed = 0.0, v, vmax = -1.0e30;
        int g1, s1, j2, or1, rc, err;
        const char *verdict;

        err = swmm_open("API-01_forcing.inp", "API-01.rpt", "API-01.out");
        if (err)
        {
            printf("FAIL: swmm_open returned %d\n", err);
            return 1;
        }
        g1  = swmm_getIndex(swmm_GAGE, "G1");
        s1  = swmm_getIndex(swmm_SUBCATCH, "S1");
        j2  = swmm_getIndex(swmm_NODE, "J2");
        or1 = swmm_getIndex(swmm_LINK, "OR1");

        rc = setBeforeStart(k, g1, s1, j2, or1);
        err = swmm_start(0);
        while (!err)
        {
            err = swmm_step(&elapsed);
            if (elapsed <= 0.0) break;
            v = observe(k, s1, j2, or1);
            if (v > vmax) vmax = v;
        }
        swmm_end();
        swmm_close();
        if (err)
        {
            printf("FAIL: case %d: the run stopped with error %d\n", k, err);
            return 1;
        }

        if (rc != 0) verdict = "refused";
        else if (fabs(vmax - setTo[k]) <= 0.1 * setTo[k]) verdict = "used";
        else { verdict = "ACCEPTED BUT IGNORED"; bad++; }
        printf("%-44s %6.2f %9d  %-28s %8.3f  %s\n",
               label[k], setTo[k], rc, seen[k], vmax, verdict);
    }

    if (bad)
    {
        printf("FAIL: %d of %d values set before swmm_start were accepted "
               "(rc 0) and then reset by swmm_start\n", bad, NCASES);
        return 1;
    }
    printf("PASS: every value set before swmm_start is either used by the run "
           "or refused with an error code\n");
    return 0;
}
