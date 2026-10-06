/*
 * IO-08: a UTF-8 byte order mark hides the first section of the input file.
 *
 * Both decks start with the bytes EF BB BF (UTF-8 with BOM, as written by
 * Notepad and other Windows editors). The reader recognises a section header
 * only when the line's first token starts with '[', so "\xEF\xBB\xBF[INFLOWS]"
 * is not a header: the object-count pass skips that section and the data pass
 * reads its lines as [TITLE] text.
 *
 * Correct behaviour: a BOM is an encoding mark, not text, so each deck is read
 * as if it were not there:
 *  - IO-08_bom-inflows-first.inp ([INFLOWS] first): J1 gets its 5 cfs inflow;
 *    the lateral inflow at 3:00 is 5.0 cfs.
 *  - IO-08_bom-options-first.inp ([OPTIONS] first): the run uses CMS
 *    (swmm_FLOWUNITS = 3), starts on 06/01/2020 (day 43983) and J1's
 *    lateral inflow at 3:00 is 0.15 cms.
 */
#include <stdio.h>
#include <math.h>
#include "swmm5.h"

/* run a deck; return the swmm_open/run error and J1's lateral inflow at 3:00 */
static int runDeck(const char *inp, double *q3, double *units, double *start)
{
    double t = 0.0;
    int err, j1;
    *q3 = -1.0; *units = -1.0; *start = -1.0;
    err = swmm_open(inp, "IO-08.rpt", "IO-08.out");
    if (!err) err = swmm_start(0);
    if (!err)
    {
        *units = swmm_getValue(swmm_FLOWUNITS, 0);
        *start = swmm_getValue(swmm_STARTDATE, 0);
        j1 = swmm_getIndex(swmm_NODE, "J1");
        while (!err)
        {
            err = swmm_step(&t);
            if (!(t > 0.0)) break;
            if (*q3 < 0.0 && t >= 3.0 / 24.0) *q3 = swmm_getValue(swmm_NODE_LATFLOW, j1);
        }
        swmm_end();
    }
    swmm_close();
    return err;
}

int main(void)
{
    double q, units, start;
    int err, bad1, bad2;

    err = runDeck("IO-08_bom-inflows-first.inp", &q, &units, &start);
    bad1 = err || fabs(q - 5.0) > 1e-6;
    printf("[INFLOWS] first: error %3d, J1 inflow at 3:00 = %.4f cfs (expected 5.0)\n", err, q);

    err = runDeck("IO-08_bom-options-first.inp", &q, &units, &start);
    bad2 = err || units != 3.0 || fabs(start - 43983.0) > 1e-6 || fabs(q - 0.15) > 1e-6;
    printf("[OPTIONS] first: error %3d, flow units %.0f (expected 3 = CMS), start day %.0f "
           "(expected 43983), J1 inflow at 3:00 = %.4f (expected 0.15)\n", err, units, start, q);

    if (bad1 || bad2)
    {
        printf("FAIL: the first section of a file saved with a UTF-8 byte order mark is "
               "not read%s%s\n", bad1 ? " ([INFLOWS] lost)" : "",
               bad2 ? " ([OPTIONS] lost)" : "");
        return 1;
    }
    printf("PASS: files with a UTF-8 byte order mark are read like files without one\n");
    return 0;
}
