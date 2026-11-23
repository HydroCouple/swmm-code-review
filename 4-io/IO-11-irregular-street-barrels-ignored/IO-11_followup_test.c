/* PR #193 follow-up checks. Run with "guard" or "street"; see README.md.
 * Compile against the submitted 5.3.0 PR library with ASan/UBSan enabled.
 * The guard case must also be run with PRs #191 and #192 combined.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

static int hasInputError(void)
{
    FILE *f = fopen("IO-11_followup.rpt", "r");
    char line[512];
    int found = 0;
    if (!f) return 0;
    while (fgets(line, sizeof line, f))
        if (strstr(line, "ERROR 209: undefined object O1x")) found = 1;
    fclose(f);
    return found;
}

static int streetBarrels(void)
{
    FILE *f = fopen("IO-11_followup.rpt", "r");
    char line[512], name[64], shape[64];
    double depth, area, radius, width, flow;
    int in = 0, barrels, count[2] = {0, 0};
    if (!f) return 1;
    while (fgets(line, sizeof line, f))
    {
        if (strstr(line, "Cross Section Summary")) { in = 1; continue; }
        if (in && sscanf(line, "%63s %63s %lf %lf %lf %lf %d %lf", name,
                         shape, &depth, &area, &radius, &width, &barrels, &flow) == 8)
        {
            if (!strcmp(name, "C1")) count[0] = barrels;
            if (!strcmp(name, "C2")) count[1] = barrels;
        }
        if (count[0] && count[1]) break;
    }
    fclose(f);
    printf("STREET barrels: C1=%d (expected 2), C2=%d (expected 1)\n",
           count[0], count[1]);
    return count[0] != 2 || count[1] != 1;
}

int main(int argc, char **argv)
{
    int guard, err, failed;
    double elapsed = 0.0;
    if (argc != 2 || (strcmp(argv[1], "guard") && strcmp(argv[1], "street")))
    {
        fprintf(stderr, "usage: IO-11_followup_test guard|street\n");
        return 2;
    }
    guard = !strcmp(argv[1], "guard");
    err = swmm_open(guard ? "IO-11_orifice-bad-node.inp" : "IO-11_street-barrels.inp",
                    "IO-11_followup.rpt", "IO-11_followup.out");
    if (guard)
    {
        swmm_close();
        failed = !err || !hasInputError();
        printf("%s: rejected link reports ERROR 209 without a sanitizer error\n",
               failed ? "FAIL" : "PASS");
        return failed;
    }
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    if (err) { printf("FAIL: STREET run error %d\n", err); return 1; }
    failed = streetBarrels();
    printf("%s: STREET retains the entered barrel counts\n", failed ? "FAIL" : "PASS");
    return failed;
}
