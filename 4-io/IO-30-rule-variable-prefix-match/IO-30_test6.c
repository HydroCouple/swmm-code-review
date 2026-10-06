/*
 * IO-30 for 6.0.0: a control-rule VARIABLE or EXPRESSION name resolves to an
 * earlier one whose name is a prefix of it.
 *
 * Same deck and check as IO-30_test.c. 6.0.0 resolves names with a
 * case-insensitive exact comparison (ieq() for variables, a case-insensitive
 * map for expressions; Controls.cpp notes that legacy's prefix match is "a
 * quirk we deliberately do not copy"), so this test is expected to pass
 * unpatched.
 *
 * Correct behaviour: IF D10 > 2, IF H2 > 2 and IF G > 2 (G = D10) are all
 * true, so OR1, OR2 and OR3 are at 0.5 after the first step.
 */
#include <stdio.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"

int main(void)
{
    const char *ids[3] = {"OR1", "OR2", "OR3"};
    const char *what[3] = {"IF D10 > 2  (D10 = 3)", "IF H2 > 2   (H2 = 5) ", "IF G > 2    (G = D10 = 3)"};
    double t = 0.0, s[3] = {-1, -1, -1};
    int rc, L[3] = {-1, -1, -1}, i, steps = 0, ok = 1;
    SWMM_Engine e = swmm_engine_create();

    rc = swmm_engine_open(e, "IO-30_prefix-names.inp", "IO-30_6.rpt", "IO-30_6.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 0);
    if (!rc) for (i = 0; i < 3; i++) L[i] = swmm_link_index(e, ids[i]);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (t <= 0.0) break;
        if (++steps == 2)
            for (i = 0; i < 3; i++) swmm_link_get_control_setting(e, L[i], &s[i]);
    }
    if (!rc) rc = swmm_engine_end(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    if (rc)
    {
        printf("FAIL: the run stopped with error %d\n", rc);
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
