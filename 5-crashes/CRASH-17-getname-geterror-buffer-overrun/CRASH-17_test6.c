/*
 * CRASH-17 for 6.0.0: string getters given a buffer shorter than the text.
 *
 * 6.0.0 returns IDs and error messages as const char* (swmm_node_id,
 * swmm_get_last_error_msg), so the legacy pattern does not occur there. Its
 * getters that do fill a caller buffer take the buffer size and document
 * truncation to size - 1 characters. The test calls two of them with exactly
 * sized heap buffers that are too short for the text:
 *   - swmm_node_get_ids_bulk(e, buf, 2, 2) for the IDs "J1" and "O1" must
 *     give "J" and "O" in a 4-byte buffer;
 *   - swmm_gage_get_timeseries(e, G1, buf, 2) for the series "RAIN" must give
 *     "R" in a 2-byte buffer.
 * AddressSanitizer would report any byte written past the buffers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_gages.h"

int main(void)
{
    SWMM_Engine e = swmm_engine_create();
    char *ids = malloc(4), *ts = malloc(2);
    int rc, bad = 0;

    rc = swmm_engine_open(e, "CRASH-17_model.inp", "CRASH-17_6.rpt", "CRASH-17_6.out", NULL);
    if (rc)
    {
        printf("FAIL: could not open the deck (error %d)\n", rc);
        return 1;
    }

    rc = swmm_node_get_ids_bulk(e, ids, 2, 2);
    printf("swmm_node_get_ids_bulk(stride 2) rc %d: \"%s\", \"%s\"\n", rc, ids, ids + 2);
    if (rc != 0 || strcmp(ids, "J") != 0 || strcmp(ids + 2, "O") != 0) bad++;

    rc = swmm_gage_get_timeseries(e, swmm_gage_index(e, "G1"), ts, 2);
    printf("swmm_gage_get_timeseries(buflen 2) rc %d: \"%s\"\n", rc, ts);
    if (rc != 0 || strcmp(ts, "R") != 0) bad++;

    free(ids);
    free(ts);
    swmm_engine_close(e);
    swmm_engine_destroy(e);

    if (bad)
    {
        printf("FAIL: %d getters did not truncate to the buffer size\n", bad);
        return 1;
    }
    printf("PASS: both getters stay within the buffer size they are given\n");
    return 0;
}
