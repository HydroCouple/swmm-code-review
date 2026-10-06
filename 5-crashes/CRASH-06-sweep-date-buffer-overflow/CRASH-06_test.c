/*
 * CRASH-06: a SWEEP_START/SWEEP_END value of 24 or more characters overflows
 * the 25-byte strDate buffer in project_readOption().
 *
 *     sstrncpy(strDate, s2, 24);           // fills strDate[0..24]
 *     sstrcat(strDate, "/1947", 25);
 *
 * sstrcat() checks "offset == destsize - 1" only after writing a character.
 * When dest already holds destsize - 1 characters, the first write moves
 * offset past that value, the check never matches, and the rest of the
 * suffix and its terminator are written beyond the buffer (a stack buffer
 * overflow; AddressSanitizer stops the run, verdict CRASH).
 *
 * Correct behaviour: swmm_open() returns; the value is not a month/day, so
 * the report should show ERROR 213 (invalid date/time) and swmm_open()
 * return 200 (input errors). The test passes when swmm_open() returns at
 * all, error or not, and prints what it returned.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

int main(void)
{
    char line[256], msg[256] = "";
    FILE *f;
    int err = swmm_open("CRASH-06_long-sweep-date.inp", "CRASH-06.rpt",
                        "CRASH-06.out");
    swmm_close();

    /* the input error, if any, is in the report */
    f = fopen("CRASH-06.rpt", "r");
    while (f && fgets(line, sizeof line, f))
        if (strstr(line, "ERROR 213")) { strcpy(msg, line); break; }
    if (f) fclose(f);

    printf("swmm_open() returned %d\n", err);
    if (msg[0]) printf("Report: %s", msg);
    if (msg[0])
        printf("PASS: the 25-character SWEEP_START value is rejected as an "
               "invalid date (ERROR 213) without overrunning strDate\n");
    else
        printf("PASS: swmm_open() returned (error %d) without overrunning "
               "strDate\n", err);
    return 0;
}
