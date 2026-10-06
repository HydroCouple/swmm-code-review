/*
 * CRASH-13: setting a node or link pollutant mass flux between swmm_open and
 * swmm_start (5.3.0's swmm_setValueExpanded with
 * swmm_NODE_POLLUTANT_LATMASS_FLUX / swmm_LINK_POLLUTANT_LATMASS_FLUX).
 *
 * The pre-start branch of setLinkValue() writes through a TLink pointer that
 * is only set in the running branch (NULL here), and the pre-start branch of
 * setNodeValue() writes Node[i].apiExtQualMassFlux[pollutantIndex] without
 * checking pollutantIndex. A valid node value is accepted (return code 0) and
 * then zeroed by swmm_start (qualrout_init), so the run never sees it.
 *
 * Correct behaviour for each call: no crash, and either the call is refused
 * with a non-zero code or the run uses the value (the pollutant concentration
 * at the object becomes > 0; the deck has no other source of P1 or P2, so any
 * positive concentration means the flux was applied).
 *
 * Each case runs in a child process so that one crash does not hide the
 * others; the sanitizer report of a crashing child is printed as it happens.
 * Child exit codes: 0 = correct, 3 = accepted but ignored, anything else
 * (sanitizer exit or signal) = crashed.
 *
 * 5.2.4 has no expanded setters and no pollutant flux, so it is not affected.
 */
#include <stdio.h>
#include <stdlib.h>
#include "swmm5.h"

#ifdef OPENSWMM_LEGACY_SOLVER_H_
#include <unistd.h>
#include <sys/wait.h>

/* Run one case: set the flux before swmm_start, then run and record the
 * largest concentration of that pollutant at the object. */
static int runCase(int objType, const char *objName, int subIndex, int pollutantIndex,
                   double flux, const char *label)
{
    double elapsed = 0.0, cMax = 0.0, c;
    int err, rc, idx, prop, cprop;

    err = swmm_open("CRASH-13_two-pollutants.inp", "CRASH-13.rpt", "CRASH-13.out");
    if (err) { printf("open failed: %d\n", err); return 4; }
    idx = swmm_getIndex(objType == swmm_NODE ? swmm_NODE : swmm_LINK, objName);
    prop  = objType == swmm_NODE ? swmm_NODE_POLLUTANT_LATMASS_FLUX
                                 : swmm_LINK_POLLUTANT_LATMASS_FLUX;
    cprop = objType == swmm_NODE ? swmm_NODE_POLLUTANT_CONCENTRATION
                                 : swmm_LINK_POLLUTANT_CONCENTRATION;

    rc = swmm_setValueExpanded(objType, prop, idx, subIndex, pollutantIndex, flux);

    err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        /* an out-of-range pollutant has no concentration to read */
        if (pollutantIndex >= 0 && pollutantIndex < 2)
        {
            c = swmm_getValueExpanded(objType, cprop, idx, 0, pollutantIndex);
            if (c > cMax) cMax = c;
        }
    }
    swmm_end();
    swmm_close();

    printf("%-46s %10d   %12.4f   %s\n", label, rc, cMax,
           rc != 0 ? "refused" : (cMax > 0.0 ? "used by the run" : "ACCEPTED BUT IGNORED"));
    return (rc != 0 || cMax > 0.0) ? 0 : 3;
}
#endif

int main(void)
{
#ifdef OPENSWMM_LEGACY_SOLVER_H_
    struct { int objType; const char *name; int sub, pol; const char *label; } cases[] = {
        { swmm_NODE, "J1", 0, 0, "NODE_POLLUTANT_LATMASS_FLUX(J1, P1 = 0)" },
        { swmm_NODE, "J1", 0, 2, "NODE_POLLUTANT_LATMASS_FLUX(J1, pollutant 2)" },
        { swmm_LINK, "C1", 0, 1, "LINK_POLLUTANT_LATMASS_FLUX(C1, P2 = 1)" },
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    int i, status, bad = 0, crashed = 0;
    pid_t pid;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Flux 5.0 set with swmm_setValueExpanded before swmm_start (2 pollutants)\n");
    printf("%-46s %10s   %12s   %s\n", "call", "rc", "max conc", "result");
    for (i = 0; i < n; i++)
    {
        pid = fork();
        if (pid == 0)
            _exit(runCase(cases[i].objType, cases[i].name, cases[i].sub, cases[i].pol,
                          5.0, cases[i].label));
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) continue;
        bad++;
        if (WIFEXITED(status) && WEXITSTATUS(status) == 3) continue;
        crashed++;
        if (WIFSIGNALED(status))
            printf("%-46s CRASHED (signal %d)\n", cases[i].label, WTERMSIG(status));
        else
            printf("%-46s CRASHED (exit code %d)\n", cases[i].label, WEXITSTATUS(status));
    }
    if (bad)
    {
        printf("FAIL: %d of %d pre-start flux calls are wrong (%d crashed, %d accepted but "
               "ignored)\n", bad, n, crashed, bad - crashed);
        return 1;
    }
    printf("PASS: every pre-start pollutant flux call is refused with an error code or used "
           "by the run, without a crash\n");
    return 0;
#else
    printf("5.2.4 has no swmm_setValueExpanded and no pollutant mass flux API\n");
    printf("PASS: not affected (the API does not exist in 5.2.4)\n");
    return 0;
#endif
}
