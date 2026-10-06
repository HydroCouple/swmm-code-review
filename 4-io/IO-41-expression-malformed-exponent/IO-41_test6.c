/*
 * IO-41 for 6.0.0: a malformed number in an expression must be rejected.
 * The 6.0.0 tokenizers collect the characters of a number and convert them
 * with std::stod, which reads the longest valid prefix and ignores the rest
 * ("2.5E+" -> 2.5) and throws std::invalid_argument when there is no valid
 * prefix ("."), which nothing catches.
 *
 * Same decks as IO-41_test.c, each with one control-rule expression:
 *   IO-41_exponent.inp   EXPRESSION E1 = 2.5E+
 *   IO-41_point.inp      EXPRESSION E1 = 2 + .
 *
 * Correct behaviour: opening each deck fails with an input error.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"

static const struct { const char *deck, *expr; } cases[] = {
    {"IO-41_exponent.inp", "E1 = 2.5E+"},
    {"IO-41_point.inp",    "E1 = 2 + ."},
};

int main(void)
{
    int i, nbad = 0;

    setvbuf(stdout, NULL, _IONBF, 0);   /* keep the rows printed before a crash */
    printf("Deck                 Expression     Error  Message\n");
    for (i = 0; i < 2; i++)
    {
        const char *msg = NULL;
        int rc;
        SWMM_Engine e = swmm_engine_create();
        rc = swmm_engine_open(e, cases[i].deck, "IO-41_6.rpt", "IO-41_6.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (rc) msg = swmm_get_last_error_msg(e);
        if (!rc) nbad++;
        printf("%-20s %-14s %5d  %s\n", cases[i].deck, cases[i].expr, rc,
               rc ? (msg ? msg : "") : "accepted  <-- malformed number not rejected");
        swmm_engine_close(e);
        swmm_engine_destroy(e);
    }
    if (nbad)
    {
        printf("FAIL: %d of 2 expressions with a malformed number are accepted\n", nbad);
        return 1;
    }
    printf("PASS: both malformed numbers are rejected with an input error\n");
    return 0;
}
