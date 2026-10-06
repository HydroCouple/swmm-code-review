/*
 * API-03: 5.3.0 has no message text for its API error codes.
 *
 * 5.3.0 renumbered the toolkit's API errors from 501..509 (5.2.4) to
 * ERR_API_NOT_OPEN = -999901 .. ERR_API_IS_RUNNING = -999912, but error.txt,
 * the table error_getMsg() looks codes up in, still lists 501..509. Every API
 * error therefore has an empty message: swmm_getErrorFromCode() returns "",
 * and swmm_getError() returns the code with an empty message (it only looks
 * up the text for codes > 0).
 *
 * Correct behaviour: each error code the toolkit returns has a message.
 *   1. swmm_start with no project open is refused with the "not open" code,
 *      which both versions also store as the current error; swmm_getError
 *      must then return that code and a non-empty message (5.2.4 and 5.3.0).
 *   2. swmm_getErrorFromCode must give a non-empty message for each of the
 *      12 API error codes declared in openswmm_solver.h (5.3.0 only; the
 *      function does not exist in 5.2.4).
 * Buffers are larger than MAXMSG + 1 so that CRASH-17 (one byte written past
 * the length passed) cannot interfere.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

#ifdef OPENSWMM_LEGACY_SOLVER_H_
#define V530 1
#endif

static const char *trim(const char *s)
{
    while (*s == '\n' || *s == ' ') s++;
    return s;
}

int main(void)
{
    char msg[1100];
    int rc, code, bad = 0, n = 0;

    /* 1. a refused call stored as the current error */
    rc = swmm_start(0);
    memset(msg, 0, sizeof msg);
    code = swmm_getError(msg, 1024);
    printf("swmm_start with no project open returned %d\n", rc);
    printf("swmm_getError returned %d, message \"%s\"\n", code, trim(msg));
    n++;
    if (rc == 0 || code != rc || strlen(trim(msg)) == 0) bad++;

#ifdef V530
    /* 2. every API error code */
    printf("\n%9s  %s\n", "code", "swmm_getErrorFromCode message");
    for (code = ERR_API_NOT_OPEN; code >= ERR_API_IS_RUNNING; code--)
    {
        char *p = msg;
        memset(msg, 0, sizeof msg);
        swmm_getErrorFromCode(code, &p);
        printf("%9d  \"%s\"\n", code, trim(msg));
        n++;
        if (strlen(trim(msg)) == 0) bad++;
    }
#endif

    if (bad)
    {
        printf("FAIL: %d of %d API error codes have an empty message\n", bad, n);
        return 1;
    }
    printf("PASS: every API error code checked (%d) has a message\n", n);
    return 0;
}
