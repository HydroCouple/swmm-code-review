/*
 * IO-40: the expression lexer (mathexpr.c, getOperand) reads a '-' that is
 * directly followed by a digit as the sign of a number whenever the previous
 * lexeme code is <= 6 or '^'. That set is meant to be "an operator or '('",
 * but code 2 is ')'. So in "(TSS)-1" the "-1" becomes the number -1, the
 * parser finds a number where it expects an operator, and the expression is
 * rejected with ERROR 233 (invalid math expression). "(TSS) - 1" parses.
 *
 * Junction J1 takes 1 cfs with TSS = 10, TP = 20 and COD = 40 mg/L.
 *   IO-40_paren-minus.inp         TP C = (TSS)-1,   COD C = (TSS+2)-3
 *   IO-40_paren-minus-spaced.inp  TP C = (TSS) - 1, COD C = (TSS+2) - 3
 *
 * Correct behaviour: both decks run, and TP = COD = 9 mg/L at J1.
 * Tolerance 0.1 mg/L.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "swmm5.h"
#include "swmm_output.h"

/* pollutant k (node result 6 + k) at node n in the last reporting period */
static double last_qual(const char *out, int n, int k)
{
    SMO_Handle h = NULL;
    float *vals = NULL;
    int len = 0, nper = 0;
    double c = -1.0;
    SMO_init(&h);
    if (SMO_open(h, out) == 0 && SMO_getTimes(h, SMO_numPeriods, &nper) == 0 &&
        SMO_getNodeResult(h, nper - 1, n, &vals, &len) == 0 && len > 6 + k)
        c = vals[6 + k];
    SMO_free((void **)&vals);
    SMO_close(&h);
    return c;
}

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

static const struct { const char *deck, *tp, *cod; } cases[] = {
    {"IO-40_paren-minus.inp",        "(TSS)-1",   "(TSS+2)-3"},
    {"IO-40_paren-minus-spaced.inp", "(TSS) - 1", "(TSS+2) - 3"},
};

int main(void)
{
    int i, nbad = 0;
    char msg[256];

    printf("Deck                          TP eqn          COD eqn             Error  TP     COD  (expected 9, 9)\n");
    for (i = 0; i < 2; i++)
    {
        double t = 0.0, tp = -1.0, cod = -1.0;
        int err, j1;
        msg[0] = '\0';
        err = swmm_open(cases[i].deck, "IO-40.rpt", "IO-40.out");
        if (!err) err = swmm_start(1);
        j1 = swmm_getIndex(swmm_NODE, "J1");
        while (!err)
        {
            err = swmm_step(&t);
            if (t <= 0.0) break;
        }
        swmm_end();
        swmm_close();
        if (err) first_input_error("IO-40.rpt", msg, sizeof(msg));
        if (!err)
        {
            tp = last_qual("IO-40.out", j1, 1);
            cod = last_qual("IO-40.out", j1, 2);
        }
        if (err || !(fabs(tp - 9.0) < 0.1) || !(fabs(cod - 9.0) < 0.1))
        {
            nbad++;
            if (err) printf("%-29s C = %-11s C = %-15s %5d  rejected: %s\n", cases[i].deck,
                            cases[i].tp, cases[i].cod, err, msg);
            else     printf("%-29s C = %-11s C = %-15s %5d  %5.2f  %5.2f  <-- wrong\n",
                            cases[i].deck, cases[i].tp, cases[i].cod, err, tp, cod);
        }
        else printf("%-29s C = %-11s C = %-15s %5d  %5.2f  %5.2f\n", cases[i].deck,
                    cases[i].tp, cases[i].cod, err, tp, cod);
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
