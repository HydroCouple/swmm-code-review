/*
 * API-17: SMO_getPropertyValue()/SMO_getPropertyValues() return the node and
 * link type codes as the bit pattern of an integer read into a float.
 *
 * The output file's property block stores, for each node, the type code as a
 * 4-byte integer followed by invert and maximum depth as 4-byte reals, and for
 * each link the type code (integer) followed by inlet offset, outlet offset,
 * full depth and length (reals) (legacy engine output.c, output_open()).
 * The property getters added in 5.3.0 fread every slot into a float, so the
 * type code comes back as a denormal: OUTFALL (1) as 1.4e-45.
 *
 * The deck has one node of each type (J1, J2 junctions = 0, O1 outfall = 1,
 * SU1 storage = 2, D1 divider = 3) and one link of each type (C1-C3 conduits
 * = 0, P1 pump = 1, OR1 orifice = 2, W1 weir = 3, OL1 outlet = 4).
 *
 * Correct behaviour: property 0 of every node and link, from either getter,
 * equals the element's type code. The other properties are printed for
 * reference and are not checked.
 *
 * 5.2.4's output library has no property getters, so there is nothing to test
 * there.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"
#include "swmm_output.h"

#ifdef OPENSWMM_LEGACY_OUTPUT_H_
static int expected_type(SMO_elementType type, const char *id)
{
    if (type == SMO_node)
    {
        if (id[0] == 'J') return 0;                    /* JUNCTION */
        if (id[0] == 'O') return 1;                    /* OUTFALL  */
        if (strncmp(id, "SU", 2) == 0) return 2;       /* STORAGE  */
        if (id[0] == 'D') return 3;                    /* DIVIDER  */
    }
    else
    {
        if (id[0] == 'C') return 0;                    /* CONDUIT  */
        if (id[0] == 'P') return 1;                    /* PUMP     */
        if (strncmp(id, "OR", 2) == 0) return 2;       /* ORIFICE  */
        if (id[0] == 'W') return 3;                    /* WEIR     */
        if (strncmp(id, "OL", 2) == 0) return 4;       /* OUTLET   */
    }
    return -1;
}

static int check(SMO_Handle h, SMO_elementType type, int count, const char *label)
{
    int j, k, n, len, nprop = 0, code = -1, ok = 1;
    float one, *all;
    char *id;

    SMO_getNumProperties(h, type, &nprop);
    SMO_getPropertyCode(h, type, 0, &code);
    printf("%s: %d properties, property 0 has code %d\n", label, nprop, code);
    printf("  %-4s %8s %14s %14s   %s\n", "ID", "expected", "getPropValue", "getPropValues",
           "other properties");
    for (j = 0; j < count; j++)
    {
        id = NULL;
        SMO_getElementName(h, type, j, &id, &n);
        one = -1.0f;
        SMO_getPropertyValue(h, type, 0, j, &one);
        all = NULL;
        SMO_getPropertyValues(h, type, j, &all, &len);
        printf("  %-4s %8d %14.6g %14.6g  ", id, expected_type(type, id), one, all ? all[0] : -1.0f);
        for (k = 1; all && k < len; k++) printf(" %g", all[k]);
        printf("\n");
        if (one != (float)expected_type(type, id) || !all || all[0] != one) ok = 0;
        SMO_free((void **)&all);
        SMO_free((void **)&id);
    }
    printf("\n");
    return ok;
}
#endif

int main(void)
{
#ifdef OPENSWMM_LEGACY_OUTPUT_H_
    SMO_Handle h = NULL;
    int err, n, ok = 1, *cnt = NULL;
    double elapsed = 0.0;

    /* run the deck to write API-17.out */
    err = swmm_open("API-17_types.inp", "API-17.rpt", "API-17.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    if (!err) err = swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    if (err)
    {
        printf("FAIL: the model run stopped with error %d\n", err);
        return 1;
    }

    SMO_init(&h);
    err = SMO_open(h, "API-17.out");
    if (!err) err = SMO_getProjectSize(h, &cnt, &n);
    if (err)
    {
        printf("FAIL: could not open API-17.out (error %d)\n", err);
        return 1;
    }
    ok &= check(h, SMO_node, cnt[1], "Nodes");
    ok &= check(h, SMO_link, cnt[2], "Links");
    SMO_free((void **)&cnt);
    SMO_close(&h);

    if (!ok)
    {
        printf("FAIL: property 0 does not give the node/link type code "
               "(an integer's bits read as a float)\n");
        return 1;
    }
    printf("PASS: property 0 of every node and link is its type code\n");
    return 0;
#else
    printf("PASS: 5.2.4's output library has no property getters "
           "(SMO_getPropertyValue was added in 5.3.0), so nothing is misread\n");
    return 0;
#endif
}
