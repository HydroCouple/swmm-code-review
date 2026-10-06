/*
 * API-01 for 6.0.0: forcing values set before swmm_engine_start.
 *
 * Same check as the legacy test, through 6.0.0's runtime setters and its
 * forcing API: after swmm_engine_initialize and before swmm_engine_start, set
 * a gage rainfall, a node lateral inflow, a link setting and a subcatchment
 * rainfall, then run the deck (its gage series is dry) and record what the
 * engine reports during the run.
 *
 * Correct behaviour: a value the API accepts (rc 0) is used by the run, or
 * the call is refused with an error code. A value counts as used when the
 * largest value reported during the run is within 10% of it; the values the
 * run falls back to are 0 (rain, inflow) and 1 (setting).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_gages.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_subcatchments.h"
#include "openswmm/engine/openswmm_forcing.h"

#define NCASES 9

static const char *label[NCASES] = {
    "swmm_gage_set_rainfall(G1)",
    "swmm_forcing_gage_rainfall(G1)",
    "swmm_node_set_lateral_inflow(J2)",
    "swmm_forcing_node_lat_inflow(J2)",
    "swmm_link_set_control_setting(OR1)",
    "swmm_link_set_target_setting(OR1)",
    "swmm_forcing_link_setting(OR1)",
    "swmm_subcatch_set_rainfall(S1)",
    "swmm_forcing_subcatch_rainfall(S1)"
};
static const double setTo[NCASES] = { 1.0, 1.0, 2.0, 2.0, 0.25, 0.25, 0.25, 1.0, 1.0 };
static const char *seen[NCASES] = {
    "max S1 rainfall (in/hr)", "max S1 rainfall (in/hr)",
    "max J2 lateral inflow (cfs)", "max J2 lateral inflow (cfs)",
    "max OR1 setting", "max OR1 setting", "max OR1 setting",
    "max S1 rainfall (in/hr)", "max S1 rainfall (in/hr)"
};

static int setBeforeStart(SWMM_Engine e, int k, int g1, int s1, int j2, int or1)
{
    const int O = SWMM_FORCING_OVERRIDE, P = SWMM_FORCING_PERSIST;
    switch (k)
    {
    case 0: return swmm_gage_set_rainfall(e, g1, setTo[k]);
    case 1: return swmm_forcing_gage_rainfall(e, g1, setTo[k], O, P);
    case 2: return swmm_node_set_lateral_inflow(e, j2, setTo[k]);
    case 3: return swmm_forcing_node_lat_inflow(e, j2, setTo[k], O, P);
    case 4: return swmm_link_set_control_setting(e, or1, setTo[k]);
    case 5: return swmm_link_set_target_setting(e, or1, setTo[k]);
    case 6: return swmm_forcing_link_setting(e, or1, setTo[k], O, P);
    case 7: return swmm_subcatch_set_rainfall(e, s1, setTo[k]);
    case 8: return swmm_forcing_subcatch_rainfall(e, s1, setTo[k], O, P);
    }
    return 0;
}

static double observe(SWMM_Engine e, int k, int s1, int j2, int or1)
{
    double v = 0.0;
    if (k == 2 || k == 3)      swmm_node_get_lateral_inflow(e, j2, &v);
    else if (k >= 4 && k <= 6) swmm_link_get_control_setting(e, or1, &v);
    else                       swmm_subcatch_get_rainfall(e, s1, &v);
    return v;
}

int main(void)
{
    int k, bad = 0;

    printf("%-36s %6s %4s  %-28s %8s  %s\n",
           "value set before swmm_engine_start", "set", "rc", "what the run reports", "", "verdict");
    for (k = 0; k < NCASES; k++)
    {
        SWMM_Engine e = swmm_engine_create();
        double t = 0.0, v, vmax = -1.0e30;
        int g1, s1, j2, or1, rc, err;
        const char *verdict;

        err = swmm_engine_open(e, "API-01_forcing.inp", "API-01_6.rpt", "API-01_6.out", NULL);
        if (!err) err = swmm_engine_initialize(e);
        if (err)
        {
            printf("FAIL: could not open the deck (error %d)\n", err);
            return 1;
        }
        g1  = swmm_gage_index(e, "G1");
        s1  = swmm_subcatch_index(e, "S1");
        j2  = swmm_node_index(e, "J2");
        or1 = swmm_link_index(e, "OR1");

        rc = setBeforeStart(e, k, g1, s1, j2, or1);
        err = swmm_engine_start(e, 0);
        while (!err)
        {
            err = swmm_engine_step(e, &t);
            if (t <= 0.0) break;
            v = observe(e, k, s1, j2, or1);
            if (v > vmax) vmax = v;
        }
        if (!err) err = swmm_engine_end(e);
        swmm_engine_close(e);
        swmm_engine_destroy(e);
        if (err)
        {
            printf("FAIL: case %d: the run stopped with error %d\n", k, err);
            return 1;
        }

        if (rc != 0) verdict = "refused";
        else if (fabs(vmax - setTo[k]) <= 0.1 * setTo[k]) verdict = "used";
        else { verdict = "ACCEPTED BUT IGNORED"; bad++; }
        printf("%-36s %6.2f %4d  %-28s %8.3f  %s\n",
               label[k], setTo[k], rc, seen[k], vmax, verdict);
    }

    if (bad)
    {
        printf("FAIL: %d of %d values set before swmm_engine_start were accepted "
               "and then not used\n", bad, NCASES);
        return 1;
    }
    printf("PASS: every value set before swmm_engine_start is either used by the "
           "run or refused with an error code\n");
    return 0;
}
