/*
 * IO-30: a control-rule VARIABLE or EXPRESSION name resolves to an earlier
 * one whose name is a prefix of it.
 *
 * IO-30_prefix-names.inp defines, in [CONTROLS]:
 *     VARIABLE D1 = NODE SU1 DEPTH        (1 ft)
 *     VARIABLE D10 = NODE SU2 DEPTH       (3 ft)
 *     EXPRESSION H = D1                   (1)
 *     EXPRESSION H2 = 5 * D1              (5)
 *     EXPRESSION G = D10                  (3)
 * and three rules: IF D10 > 2 (sets OR1), IF H2 > 2 (sets OR2) and
 * IF G > 2 (sets OR3), each to SETTING = 0.5.
 *
 * Correct behaviour: a name means the variable or expression with exactly
 * that name (case-insensitive, as SWMM IDs are), so all three premises are
 * true and OR1, OR2 and OR3 are at 0.5 after the first step. With the bug
 * "D10" resolves to D1 (1 ft) in the premise and in G's formula, and "H2" to
 * H (1), so all three premises are false and the orifices stay at 1.0.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

int main(void)
{
    const char *ids[3] = {"OR1", "OR2", "OR3"};
    const char *what[3] = {"IF D10 > 2  (D10 = 3)", "IF H2 > 2   (H2 = 5) ", "IF G > 2    (G = D10 = 3)"};
    double t = 0.0, s[3] = {-1, -1, -1};
    int err, L[3], i, steps = 0, ok = 1;

    err = swmm_open("IO-30_prefix-names.inp", "IO-30.rpt", "IO-30.out");
    if (!err) err = swmm_start(0);
    for (i = 0; i < 3; i++) L[i] = swmm_getIndex(swmm_LINK, (char *)ids[i]);
    while (!err)
    {
        err = swmm_step(&t);
        if (t <= 0.0) break;
        if (++steps == 2)
            for (i = 0; i < 3; i++) s[i] = swmm_getValue(swmm_LINK_SETTING, L[i]);
    }
    swmm_end();
    swmm_close();
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }

    printf("  rule premise                 link  setting (expected 0.5)\n");
    for (i = 0; i < 3; i++)
    {
        printf("  %-28s %s   %4.2f\n", what[i], ids[i], s[i]);
        if (fabs(s[i] - 0.5) > 1e-9) ok = 0;
    }
    if (!ok)
    {
        printf("FAIL: names resolve by prefix: D10 is read as D1 and H2 as H "
               "(OR1 %.2f, OR2 %.2f, OR3 %.2f instead of 0.50)\n", s[0], s[1], s[2]);
        return 1;
    }
    printf("PASS: each variable and expression name resolves to the object with exactly that name\n");
    return 0;
}
