/*
 * IO-42 for 6.0.0: the same four hot start files as IO-42_test.c.
 *
 * 6.0.0 reads legacy .hsf files in HotStartManager::apply_legacy_routing().
 * It does check each read, but it does not check that the file ends where the
 * model's records end, and on a short read it returns without an error text.
 *
 * Correct behaviour: the matching file is applied (C1 starts with the flow it
 * had at the end of the save run; 0.001 cfs is far above float rounding of a
 * 6 cfs flow), and the truncated, too-short and too-long files are refused
 * with an error whose message says what is wrong.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "openswmm/engine/openswmm_engine.h"
#include "openswmm/engine/openswmm_links.h"
#include "openswmm/engine/openswmm_massbalance.h"

static int save_run(const char *inp, const char *rpt, const char *out, double *q)
{
    double t = 0.0;
    SWMM_Engine e = swmm_engine_create();
    int rc = swmm_engine_open(e, inp, rpt, out, NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 1);
    while (!rc)
    {
        rc = swmm_engine_step(e, &t);
        if (!rc) swmm_link_get_flow(e, swmm_link_index(e, "C1"), q);
        if (t <= 0) break;
    }
    if (!rc) rc = swmm_engine_end(e);
    if (!rc) rc = swmm_engine_report(e);
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

static long copy_file(const char *src, const char *dst, long n)
{
    char buf[4096];
    long total = 0;
    size_t k;
    FILE *fi = fopen(src, "rb"), *fo;
    if (!fi) return -1;
    fo = fopen(dst, "wb");
    if (!fo) { fclose(fi); return -1; }
    while ((k = fread(buf, 1, sizeof(buf), fi)) > 0)
    {
        if (n >= 0 && total + (long)k > n) k = (size_t)(n - total);
        fwrite(buf, 1, k, fo);
        total += (long)k;
        if (n >= 0 && total >= n) break;
    }
    fclose(fi);
    fclose(fo);
    return total;
}

/* start inp from IO-42_use.hsf; read C1's initial state, run the 10 minutes
   and return C1's largest flow and the flow routing continuity error (%), or
   the error text */
static int use_run(const char *inp, double *q, double *y, double *s,
                   double *qmax, double *ce, char *msg)
{
    SWMM_Engine e = swmm_engine_create();
    int rc, c1;
    double t = 0.0;
    msg[0] = '\0';
    *q = *y = *s = *ce = *qmax = NAN;
    rc = swmm_engine_open(e, inp, "IO-426_use.rpt", "IO-426_use.out", NULL);
    if (!rc) rc = swmm_engine_initialize(e);
    if (!rc) rc = swmm_engine_start(e, 0);
    if (!rc)
    {
        c1 = swmm_link_index(e, "C1");
        swmm_link_get_flow(e, c1, q);
        swmm_link_get_depth(e, c1, y);
        swmm_link_get_control_setting(e, c1, s);
        *qmax = fabs(*q);
        while (!swmm_engine_step(e, &t))
        {
            double qt = 0.0;
            swmm_link_get_flow(e, c1, &qt);
            *qmax = fmax(*qmax, fabs(qt));
            if (t <= 0) break;
        }
        if (!swmm_engine_end(e) && !swmm_get_routing_continuity_error(e, ce))
            *ce *= 100.0;   /* the API returns a fraction */
    }
    else
    {
        const char *m = swmm_get_last_error_msg(e);
        snprintf(msg, 200, "%s", m ? m : "");
    }
    swmm_engine_close(e);
    swmm_engine_destroy(e);
    return rc;
}

int main(void)
{
    struct { const char *name, *src, *inp; long trim; } cases[] = {
        { "matching file (control)",     "IO-42_junction.hsf", "IO-42_use.inp",          0 },
        { "truncated (no link records)", "IO-42_junction.hsf", "IO-42_use.inp",         24 },
        { "junction file, storage model","IO-42_junction.hsf", "IO-42_use_storage.inp",  0 },
        { "storage file, junction model","IO-42_storage.hsf",   "IO-42_use.inp",          0 },
    };
    double qsave = NAN, q, y, s, qmax, ce, dummy;
    long size, bytes;
    int i, rc, accepted = 0, nomsg = 0, okctl = 0;
    char msg[256];
    FILE *f;

    rc = save_run("IO-42_save.inp", "IO-426_save.rpt", "IO-426_save.out", &qsave);
    if (!rc) rc = save_run("IO-42_save_storage.inp", "IO-426_save_storage.rpt",
                           "IO-426_save_storage.out", &dummy);
    if (rc)
    {
        printf("FAIL: a save run stopped with error %d\n", rc);
        return 1;
    }
    printf("C1 flow at the end of the save run: %.4f cfs\n\n", qsave);
    printf("%-30s %6s %6s %8s %8s %8s %8s %12s  %s\n", "hot start file",
           "bytes", "error", "C1 flow", "C1 depth", "C1 sett", "C1 max",
           "flow CE (%)", "message");
    printf("%-30s %6s %6s %8s %8s %8s %8s\n", "", "", "", "(cfs)", "(ft)",
           "", "(cfs)");

    for (i = 0; i < 4; i++)
    {
        f = fopen(cases[i].src, "rb");
        if (!f) { printf("FAIL: %s was not written\n", cases[i].src); return 1; }
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fclose(f);
        bytes = copy_file(cases[i].src, "IO-42_use.hsf", size - cases[i].trim);
        rc = use_run(cases[i].inp, &q, &y, &s, &qmax, &ce, msg);
        printf("%-30s %6ld %6d %8.4f %8.4f %8.4f %8.4f %12.3f  %s\n",
               cases[i].name, bytes, rc, q, y, s, qmax, ce, msg);
        if (i == 0) okctl = (rc == 0 && fabs(q - qsave) < 0.001);
        else if (rc == 0) accepted++;
        /* a refusal must say why: more than the "USE HOTSTART: " prefix */
        else if (strlen(msg) <= strlen("USE HOTSTART: ")) nomsg++;
    }

    if (!okctl)
    {
        printf("FAIL: the run from the matching hot start file did not start "
               "with C1 at %.4f cfs\n", qsave);
        return 1;
    }
    if (accepted || nomsg)
    {
        printf("FAIL: of 3 hot start files that do not match the model, "
               "applied without an error: %d, refused with an empty message: "
               "%d\n", accepted, nomsg);
        return 1;
    }
    printf("PASS: the matching hot start file is applied; the truncated and "
           "mismatched files are refused with an error message\n");
    return 0;
}
