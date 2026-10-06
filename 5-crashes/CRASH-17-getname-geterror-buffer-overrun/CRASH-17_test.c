/*
 * CRASH-17: swmm_getError and swmm_getName write one byte past the buffer.
 *
 * Both functions document their length argument as the size of the caller's
 * buffer ("msgLen = maximum size of errMsg", "size = size of the name
 * array") and pass it to sstrncpy(dest, src, n), which copies up to n
 * characters and then writes the terminating null: n + 1 bytes. Whenever the
 * text is at least as long as the buffer, the null lands one byte past it.
 *
 * Correct behaviour: at most `size` bytes are written, i.e. the text is cut to
 * size - 1 characters plus its null.
 *   - swmm_getError(buf, 16) after opening a file that does not exist
 *     (error 303, whose text is longer than 15 characters) must leave a
 *     15-character string in the 16-byte buffer;
 *   - swmm_getName(swmm_NODE, J1, buf, 2) must leave "J" in the 2-byte buffer.
 * Each buffer is malloc'd with exactly that size, so AddressSanitizer reports
 * the extra byte. Each check runs in a child process, so a memory error in
 * the first does not hide the second.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "swmm5.h"

static int checkGetError(void)
{
    const int size = 16;
    char *buf = malloc(size);
    int code, ok;

    swmm_open("CRASH-17_missing.inp", "CRASH-17a.rpt", "CRASH-17a.out");
    code = swmm_getError(buf, size);
    printf("  swmm_getError(buf, %d) returned %d, text \"%s\" (%d chars)\n",
           size, code, buf, (int)strlen(buf));
    ok = code != 0 && (int)strlen(buf) == size - 1;
    free(buf);
    swmm_close();
    return ok ? 0 : 2;
}

static int checkGetName(void)
{
    const int size = 2;
    char *buf = malloc(size);
    int j1, ok;

    if (swmm_open("CRASH-17_model.inp", "CRASH-17b.rpt", "CRASH-17b.out")) return 3;
    j1 = swmm_getIndex(swmm_NODE, "J1");
    swmm_getName(swmm_NODE, j1, buf, size);
    printf("  swmm_getName(swmm_NODE, J1, buf, %d) gave \"%s\"\n", size, buf);
    ok = strcmp(buf, "J") == 0;
    free(buf);
    swmm_close();
    return ok ? 0 : 2;
}

int main(void)
{
    static const char *name[2] = { "swmm_getError, 16-byte buffer", "swmm_getName, 2-byte buffer" };
    int k, bad = 0;

    for (k = 0; k < 2; k++)
    {
        int status = 0;
        pid_t pid;

        printf("%s:\n", name[k]);
        fflush(stdout);
        pid = fork();
        if (pid == 0)
        {
            int rc = k == 0 ? checkGetError() : checkGetName();
            fflush(stdout);
            _exit(rc);
        }
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            printf("  ok\n");
        else
        {
            bad++;
            if (WIFEXITED(status) && WEXITSTATUS(status) == 2)
                printf("  wrong text\n");
            else if (WIFEXITED(status))
                printf("  stopped with exit code %d (memory error)\n", WEXITSTATUS(status));
            else
                printf("  stopped by signal %d\n", WTERMSIG(status));
        }
        fflush(stdout);
    }

    if (bad)
    {
        printf("FAIL: %d of 2 calls wrote past the buffer size they were given "
               "or did not truncate to size - 1 characters\n", bad);
        return 1;
    }
    printf("PASS: both calls stay within the buffer size they are given\n");
    return 0;
}
