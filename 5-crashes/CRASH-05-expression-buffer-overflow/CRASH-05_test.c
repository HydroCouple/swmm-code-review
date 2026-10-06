/*
 * CRASH-05: the expression lexer (mathexpr.c) builds each number and each
 * name one character at a time with strcat() into fixed 255-byte buffers,
 * with no length check: getNumber() into the local `char sNumber[255]`,
 * getToken() into the global `static char Token[255]`. Expression lines can
 * be up to 1024 characters, so a long constant or name writes past the end.
 *
 *   CRASH-05_long-number.inp  [TREATMENT] J1 TP C = 0.5*111...1 (300 digits)
 *   CRASH-05_long-name.inp    [CONTROLS]  EXPRESSION E1 = AAA...A + 1
 *                             (300-character name)
 *
 * Correct behaviour: reading the input does not write out of bounds. Under
 * AddressSanitizer the unfixed engine stops at the first strcat() past the
 * end (stack-buffer-overflow, global-buffer-overflow). With the fix,
 * swmm_open() returns, here with ERROR 233 (invalid math expression)
 * because a constant or name that long cannot be read.
 *
 * Each deck is opened in a child process, so that the sanitizer report of
 * the first deck does not hide the second.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
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

static const char *decks[] = {"CRASH-05_long-number.inp", "CRASH-05_long-name.inp"};

int main(void)
{
    int i, err, status, nbad = 0;
    char msg[256];

    setvbuf(stdout, NULL, _IONBF, 0);   /* keep the rows printed before a crash */
    printf("Deck                        swmm_open  Report\n");
    for (i = 0; i < 2; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            err = swmm_open(decks[i], "CRASH-05.rpt", "CRASH-05.out");
            swmm_close();
            msg[0] = '\0';
            if (err) first_input_error("CRASH-05.rpt", msg, sizeof(msg));
            printf("%-27s %9d  %s\n", decks[i], err, err ? msg : "(accepted)");
            _exit(0);
        }
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            nbad++;
            printf("%-27s  <-- memory error while reading the expression\n", decks[i]);
        }
    }
    if (nbad)
    {
        printf("FAIL: %d of 2 decks write past a 255-byte buffer while reading an "
               "expression\n", nbad);
        return 1;
    }
    printf("PASS: both decks are read without a memory error\n");
    return 0;
}
