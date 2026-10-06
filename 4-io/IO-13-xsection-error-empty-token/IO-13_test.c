/*
 * IO-13: invalid cross-section geometry is reported as "ERROR 211: invalid
 * number" with an empty token, so the message does not say what is wrong.
 *
 * IO-13_bad-xsections.inp has three conduits, each with geometry that
 * xsect_setParams() refuses for a different reason:
 *   C1 FILLED_CIRCULAR 2 3        sediment depth >= diameter
 *   C2 CIRCULAR        0          zero diameter
 *   C3 HORIZ_ELLIPSE   2 3.17 99  size code 99 (there are 23)
 *
 * Correct behaviour: the deck is rejected, and each error message itself
 * (the report line that starts with "ERROR") names the conduit it is about,
 * as the other input error messages name the offending item. The report also
 * echoes the input line below the message in 5.2.4/5.3.0; that echo is not
 * counted here, because it does not say which value is wrong and 6.0.0 does
 * not print it.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "swmm5.h"

#define NLINKS 3

/* does the text contain the link name as a whole word? */
static int names(const char *text, const char *id)
{
    const char *p = text;
    size_t n = strlen(id);
    while ((p = strstr(p, id)) != NULL)
    {
        int before = p == text || !isalnum((unsigned char)p[-1]);
        int after = !isalnum((unsigned char)p[n]) && p[n] != '_';
        if (before && after) return 1;
        p += n;
    }
    return 0;
}

int main(void)
{
    static const char *ids[NLINKS] = {"C1", "C2", "C3"};
    int named[NLINKS] = {0};
    int err, k, nerr = 0, nbad = 0;
    char line[512], bad[128] = "";
    FILE *f;

    err = swmm_open("IO-13_bad-xsections.inp", "IO-13.rpt", "IO-13.out");
    swmm_close();
    printf("swmm_open returned %d; error messages in the report:\n", err);

    f = fopen("IO-13.rpt", "r");
    while (f && fgets(line, sizeof line, f))
    {
        char *p = strstr(line, "ERROR");
        if (!p) continue;
        nerr++;
        printf("  %s", p);
        for (k = 0; k < NLINKS; k++)
            if (names(p, ids[k])) named[k] = 1;
    }
    if (f) fclose(f);

    for (k = 0; k < NLINKS; k++)
        if (!named[k])
        {
            snprintf(bad + strlen(bad), sizeof bad - strlen(bad), "%s%s", nbad ? ", " : "", ids[k]);
            nbad++;
        }
    if (!err)
    {
        printf("FAIL: the deck with three invalid cross sections was accepted\n");
        return 1;
    }
    if (nbad)
    {
        printf("FAIL: %d error messages, but none names %s\n", nerr, bad);
        return 1;
    }
    printf("PASS: each invalid cross section is reported with the name of its conduit\n");
    return 0;
}
