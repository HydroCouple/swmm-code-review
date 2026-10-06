/*
 * IO-62: in SI units the Street Flow Summary has no "Count" label over the
 * inlet-count column.
 *
 * The deck (CMS units) has one street with 2 grate inlets. The test runs it,
 * reads the report back, finds the column-header row of the inlet table (the
 * one with "Design" and "Location") and the street's row, and checks that the
 * header has a "Count" label whose last letter sits over the inlet count
 * (2), as it does in US units.
 */
#include <stdio.h>
#include <string.h>
#include "swmm5.h"

int main(void)
{
    char line[512], header[512] = "", row[512] = "";
    char *p, *q;
    FILE *f;
    int err, col;

    double elapsed = 0.0;
    err = swmm_open("IO-62_si-street.inp", "IO-62.rpt", "IO-62.out");
    if (!err) err = swmm_start(1);
    while (!err)
    {
        err = swmm_step(&elapsed);
        if (elapsed <= 0.0) break;
    }
    swmm_end();
    if (!err) err = swmm_report();
    swmm_close();
    if (err) { printf("FAIL: the run stopped with error %d\n", err); return 1; }
    f = fopen("IO-62.rpt", "r");
    if (!f) { printf("FAIL: cannot read IO-62.rpt\n"); return 1; }
    while (fgets(line, sizeof line, f))
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (strstr(line, "Design") && strstr(line, "Location") && !header[0]) strcpy(header, line);
        if (strstr(line, "ON-GRADE") && !row[0]) strcpy(row, line);
    }
    fclose(f);

    /* print the part of both lines from the design name to the first Pcnt column */
    q = strstr(header, "Design");
    col = q ? (int)(q - header) : 0;
    printf("header: ...%.42s\n", header + col);
    printf("row:    ...%.42s\n", (int)strlen(row) > col ? row + col : row);
    p = strstr(header, "Count");
    if (!p)
    {
        printf("FAIL: the SI inlet table header has no \"Count\" label for the inlet count column\n");
        return 1;
    }
    col = (int)(p - header) + 4;
    if (col >= (int)strlen(row) || row[col] != '2')
    {
        printf("FAIL: the \"Count\" label is not over the inlet count\n");
        return 1;
    }
    printf("PASS: the SI inlet table labels the inlet count column \"Count\"\n");
    return 0;
}
