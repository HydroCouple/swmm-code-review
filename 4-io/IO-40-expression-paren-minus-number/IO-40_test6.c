/*
 * IO-40 for 6.0.0: a minus sign directly after ')' must be a subtraction, so
 * "(TSS)-1" parses like "(TSS) - 1". The 6.0.0 tokenizers decide between a
 * unary and a binary minus from the previous token, and ')' gives a binary
 * minus, so 6.0.0 is expected to pass.
 *
 * Same decks and expected values as IO-40_test.c: both decks run, and TP =
 * COD = 9 mg/L at junction J1 (tolerance 0.1 mg/L).
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"

static const struct { const char *deck, *tp, *cod; } cases[] = {
    {"IO-40_paren-minus.inp",        "(TSS)-1",   "(TSS+2)-3"},
    {"IO-40_paren-minus-spaced.inp", "(TSS) - 1", "(TSS+2) - 3"},
};

int main(void)
{
    int i, nbad = 0;

    printf("Deck                          TP eqn          COD eqn             Error  TP     COD  (expected 9, 9)\n");
    for (i = 0; i < 2; i++)
    {
        double t = 0.0, tp = -1.0, cod = -1.0;
        int rc, j1 = -1;
        const char *msg = "";
        SWMM_Engine e = swmm_engine_create();
        rc = swmm_engine_open(e, cases[i].deck, "IO-40_6.rpt", "IO-40_6.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (!rc) rc = swmm_engine_start(e, 1);
        if (!rc) j1 = swmm_node_index(e, "J1");
        while (!rc)
        {
            rc = swmm_engine_step(e, &t);
            if (t <= 0.0) break;
        }
        if (!rc) rc = swmm_node_get_quality(e, j1, 1, &tp);
        if (!rc) rc = swmm_node_get_quality(e, j1, 2, &cod);
        if (!rc) rc = swmm_engine_end(e);
        if (rc && swmm_get_last_error_msg(e)) msg = swmm_get_last_error_msg(e);
        if (rc || !(fabs(tp - 9.0) < 0.1) || !(fabs(cod - 9.0) < 0.1))
        {
            nbad++;
            if (rc) printf("%-29s C = %-11s C = %-15s %5d  rejected: %s\n", cases[i].deck,
                           cases[i].tp, cases[i].cod, rc, msg);
            else    printf("%-29s C = %-11s C = %-15s %5d  %5.2f  %5.2f  <-- wrong\n",
                           cases[i].deck, cases[i].tp, cases[i].cod, rc, tp, cod);
        }
        else printf("%-29s C = %-11s C = %-15s %5d  %5.2f  %5.2f\n", cases[i].deck,
                    cases[i].tp, cases[i].cod, rc, tp, cod);
        swmm_engine_close(e);
        swmm_engine_destroy(e);
    }
    if (nbad)
    {
        printf("FAIL: %d of 2 decks with a valid '-' after ')' is rejected or gives a wrong "
               "result\n", nbad);
        return 1;
    }
    printf("PASS: a minus sign after ')' is read as a subtraction, with or without a space\n");
    return 0;
}
