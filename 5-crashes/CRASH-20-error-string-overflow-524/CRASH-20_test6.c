/*
 * CRASH-20 for 6.0.0: the same check through the 6.0.0 API. 6.0.0 builds its
 * error messages in std::string, so it is expected to pass. The legacy
 * description follows.
 *
 * error_setInpError() does strcpy(ErrString, s) with "char ErrString[256]".
 * Every input reader passes it the token that caused the error, and an input
 * line, hence a token, can be up to 1023 characters long. A token of 256
 * characters or more writes past the end of ErrString into the globals that
 * follow it. 5.3.0 sizes ErrString to MAXMSG (1024).
 *
 * CRASH-20_long-name.inp has a conduit whose From node is a misspelt,
 * 300-character name. Correct: the open fails with an input error and the
 * report shows ERROR 209 with the whole name, without a memory error. Under
 * AddressSanitizer 5.2.4 stops with "global-buffer-overflow ... WRITE of
 * size 301" in error_setInpError.
 */
#include <stdio.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"

int main(void)
{
    char line[2048], name[301];
    int i, err, found = 0, full = 0;
    FILE *f;

    /* the 300-character node name used in the deck */
    name[0] = 'N';
    for (i = 1; i < 300; i++) name[i] = (char)('0' + (i - 1) % 10);
    name[300] = 0;

    {
        SWMM_Engine e = swmm_engine_create();
        err = swmm_engine_open(e, "CRASH-20_long-name.inp", "CRASH-206.rpt", "CRASH-206.out", NULL);
        swmm_engine_close(e);
        swmm_engine_destroy(e);
    }

    f = fopen("CRASH-206.rpt", "r");
    if (f)
    {
        while (fgets(line, sizeof line, f))
        {
            if (strstr(line, "ERROR 209"))
            {
                found = 1;
                if (strstr(line, name)) full = 1;
                break;
            }
        }
        fclose(f);
    }
    printf("swmm_engine_open returned %d; ERROR 209 in the report: %s; with the whole 300-character name: %s\n",
           err, found ? "yes" : "no", full ? "yes" : "no");

    if (err == 0 || !found || !full)
    {
        printf("FAIL: the undefined 300-character node name is not reported as ERROR 209 in full\n");
        return 1;
    }
    printf("PASS: the input error is reported with the whole name and no memory error\n");
    return 0;
}
