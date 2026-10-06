/*
 * CRASH-05 for 6.0.0: an expression with a 300-digit constant or a
 * 300-character name must be read without a memory error. The 6.0.0
 * tokenizers build numbers and names in std::string, so they are not
 * expected to overflow.
 *
 * Same decks as CRASH-05_test.c:
 *   CRASH-05_long-number.inp  [TREATMENT] J1 TP C = 0.5*111...1 (300 digits)
 *   CRASH-05_long-name.inp    [CONTROLS]  EXPRESSION E1 = AAA...A + 1
 *
 * Correct behaviour: opening and initializing each deck returns (with or
 * without an input error) and AddressSanitizer reports nothing.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"

static const char *decks[] = {"CRASH-05_long-number.inp", "CRASH-05_long-name.inp"};

int main(void)
{
    int i, rc;

    setvbuf(stdout, NULL, _IONBF, 0);   /* keep the rows printed before a crash */
    printf("Deck                        open/init  Message\n");
    for (i = 0; i < 2; i++)
    {
        const char *msg = NULL;
        SWMM_Engine e = swmm_engine_create();
        rc = swmm_engine_open(e, decks[i], "CRASH-05_6.rpt", "CRASH-05_6.out", NULL);
        if (!rc) rc = swmm_engine_initialize(e);
        if (rc) msg = swmm_get_last_error_msg(e);
        printf("%-27s %9d  %s\n", decks[i], rc, rc ? (msg ? msg : "") : "(accepted)");
        swmm_engine_close(e);
        swmm_engine_destroy(e);
    }
    printf("PASS: both decks are read without a memory error\n");
    return 0;
}
