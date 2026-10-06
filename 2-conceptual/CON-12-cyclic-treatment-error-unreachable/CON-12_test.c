/*
 * CON-12: ERROR 161 (cyclic dependency in treatment functions) is never
 * reported. getRemoval() latches ErrCode = 1 when it meets a removal that is
 * still being evaluated, but treatmnt_treat() tests ErrCode against
 * ERR_CYCLIC_TREATMENT (161), so the error branch is dead and the run goes on
 * with the cyclic removals silently set to 0.
 *
 * Two decks, both with 10 mg/L of every pollutant entering junction J1:
 *   CON-12_self-reference.inp  TN    R = 0.5*R_TN
 *   CON-12_mutual.inp          BOD5  R = 0.5*R_TN,  TN  R = 0.5*R_BOD5
 * The second is the manual's own example for ERROR 161; the first is the
 * shortest cycle, and getRemoval() handles it the same way.
 *
 * Correct behaviour (manual, Appendix A, ERROR 161): the run stops with error
 * code 161.
 */
#include <stdio.h>
#include "swmm5.h"

static const char *decks[] = {"CON-12_self-reference.inp", "CON-12_mutual.inp"};

int main(void)
{
    int i, err, nbad = 0;
    char msg[256];

    printf("Deck                         Error code  Message\n");
    for (i = 0; i < 2; i++)
    {
        double t = 0.0;
        msg[0] = '\0';
        err = swmm_open(decks[i], "CON-12.rpt", "CON-12.out");
        if (!err) err = swmm_start(1);
        while (!err)
        {
            err = swmm_step(&t);
            if (t <= 0.0) break;
        }
        swmm_end();
        swmm_getError(msg, sizeof(msg));
        swmm_close();
        if (err != 161) nbad++;
        printf("%-28s %10d  %s%s\n", decks[i], err, err ? msg : "(run completed)",
               err != 161 ? "  <-- no ERROR 161" : "");
    }
    if (nbad)
    {
        printf("FAIL: %d of 2 decks with a cyclic treatment dependency run without ERROR 161\n",
               nbad);
        return 1;
    }
    printf("PASS: both cyclic treatment dependencies stop the run with ERROR 161\n");
    return 0;
}
