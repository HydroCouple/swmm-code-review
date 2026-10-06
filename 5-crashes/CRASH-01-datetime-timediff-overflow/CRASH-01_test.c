/*
 * CRASH-01: datetime_timeDiff() converts an out-of-range double to int.
 *
 * The deck's only rain series ends at 0:45 of a 3-hour run. From then on the
 * gage's next rain date is NO_DATE (1 Jan 0001), and at every runoff step
 * runoff_getTimeStep() asks datetime_timeDiff() for the seconds between that
 * date and the current date: about -6.3e10 s. The function stores
 * (int)floor(...) of that value, which is undefined behaviour in C.
 *
 * This test only runs the model. The defect is the conversion itself, which
 * UndefinedBehaviorSanitizer reports as "runtime error: -6.31e+10 is outside
 * the range of representable values of type 'int'" (tools/run-test.sh turns
 * that report into the CRASH verdict). Without a sanitizer the run completes,
 * so a plain build prints PASS either way; see the README.
 */
#include <stdio.h>
#include "swmm5.h"

int main(void)
{
    double elapsed = 0.0;
    int err, steps = 0;

    err = swmm_open("CRASH-01_rain-ends-early.inp", "CRASH-01.rpt", "CRASH-01.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
        steps++;
    }
    swmm_end();
    swmm_close();

    printf("Routing steps run: %d, error code: %d\n", steps, err);
    if (err)
    {
        printf("FAIL: the run stopped with error %d\n", err);
        return 1;
    }
    printf("PASS: the run finished; datetime_timeDiff() did not overflow "
           "(no sanitizer report above)\n");
    return 0;
}
