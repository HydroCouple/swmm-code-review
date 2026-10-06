/*
 * IO-05 for 6.0.0: input lines longer than 1024 characters.
 *
 * The legacy reader splits such a line in two and parses the second part as
 * a line of its own (see IO-05_test.c). 6.0.0's InputReader reads whole lines
 * with std::getline(), so it has no line limit and cannot split one.
 *
 * Correct behaviour: a line is never split. IO-05_long-comment.inp (a
 * 1151-character comment line) must open with its 2 nodes and 1 link and
 * run. IO-05_long-data-line.inp (a 1098-character [TIMESERIES] line) must
 * either be refused with ERROR 201, as the manual describes, or be read as
 * one line; 6.0.0 does the latter, so it must open without an error.
 */
#include <stdio.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_nodes.h"
#include "openswmm/engine/openswmm_links.h"

int main(void)
{
    double t = 0.0;
    int err, nNodes = -1, nLinks = -1, bad1, bad2;
    SWMM_Engine e = swmm_engine_create();

    /* 1. long comment line */
    err = swmm_engine_open(e, "IO-05_long-comment.inp", "IO-05a_6.rpt", "IO-05a_6.out", NULL);
    if (!err)
    {
        nNodes = swmm_node_count(e);
        nLinks = swmm_link_count(e);
        err = swmm_engine_initialize(e);
        if (!err) err = swmm_engine_start(e, 0);
        while (!err)
        {
            err = swmm_engine_step(e, &t);
            if (!(t > 0.0)) break;
        }
        if (!err) err = swmm_engine_end(e);
    }
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    bad1 = err != 0 || nNodes != 2 || nLinks != 1;
    printf("long comment line:   error code %d, %d nodes, %d links\n", err, nNodes, nLinks);

    /* 2. long data line: read whole, so the deck opens */
    e = swmm_engine_create();
    err = swmm_engine_open(e, "IO-05_long-data-line.inp", "IO-05b_6.rpt", "IO-05b_6.out", NULL);
    printf("long data line:      error code %d  %s\n", err,
           err ? swmm_get_last_error_msg(e) : "");
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    bad2 = err != 0;

    if (bad1 || bad2)
    {
        printf("FAIL: a line longer than 1024 characters is not read as one line\n");
        return 1;
    }
    printf("PASS: lines longer than 1024 characters are read whole (comment ignored, "
           "data line opens)\n");
    return 0;
}
