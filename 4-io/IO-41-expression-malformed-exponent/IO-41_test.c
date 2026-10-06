/*
 * IO-41: the expression lexer (mathexpr.c, getNumber) accepts a malformed
 * number and reads it as 0 without flagging a parse error:
 *   - an exponent marker with no digits ("2.5E+", "5e"): getNumber sets a
 *     local errflag and returns 0, but never sets the parser's Err flag;
 *   - a decimal point with no digits ("."): atof(".") = 0.
 * The expression is accepted and the run uses 0 for that constant.
 *
 * Two decks, each with one control-rule expression:
 *   IO-41_exponent.inp   EXPRESSION E1 = 2.5E+
 *   IO-41_point.inp      EXPRESSION E1 = 2 + .
 *
 * Correct behaviour: a malformed number makes the expression invalid, as
 * "1.5.2" already does, so swmm_open() fails and the report shows
 * ERROR 233 (invalid math expression).
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

/* first line of the report that names an input error other than 200 */
static void first_input_error(const char *rpt, char *msg, int n)
{
    char line[512];
    FILE *f = fopen(rpt, "r");
    msg[0] = '\0';
    if (!f) return;
    while (fgets(line, sizeof(line), f))
    {
        char *p = strstr(line, "ERROR ");
        if (p && strncmp(p, "ERROR 200", 9) != 0)
        {
            p[strcspn(p, "\r\n")] = '\0';
            snprintf(msg, n, "%s", p);
            break;
        }
    }
    fclose(f);
}

static const struct { const char *deck, *expr; } cases[] = {
    {"IO-41_exponent.inp", "E1 = 2.5E+"},
    {"IO-41_point.inp",    "E1 = 2 + ."},
};

int main(void)
{
    int i, nbad = 0;
    char msg[256];

    printf("Deck                 Expression     Error  Report\n");
    for (i = 0; i < 2; i++)
    {
        int err = swmm_open(cases[i].deck, "IO-41.rpt", "IO-41.out");
        swmm_close();
        msg[0] = '\0';
        if (err) first_input_error("IO-41.rpt", msg, sizeof(msg));
        if (!err || strstr(msg, "ERROR 233") == NULL)
        {
            nbad++;
            printf("%-20s %-14s %5d  %s\n", cases[i].deck, cases[i].expr, err,
                   err ? msg : "accepted  <-- malformed number read as 0");
        }
        else printf("%-20s %-14s %5d  %s\n", cases[i].deck, cases[i].expr, err, msg);
    }
    if (nbad)
    {
        printf("FAIL: %d of 2 expressions with a malformed number are accepted\n", nbad);
        return 1;
    }
    printf("PASS: both malformed numbers are rejected with ERROR 233\n");
    return 0;
}
