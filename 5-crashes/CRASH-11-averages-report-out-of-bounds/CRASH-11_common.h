/* CRASH-11: checks shared by CRASH-11_test.c and CRASH-11_test6.c.
 * The including file defines ids[] and run(inp, rpt, out). */

/* "Reported Max Depth" (last column of the Node Depth Summary) for ids[] */
static int rpt_max(const char *rpt, double v[3])
{
    char line[512];
    int i, n = 0, in = 0;
    FILE *f = fopen(rpt, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "Node Depth Summary")) { in = 1; continue; }
        if (in && strstr(line, "Summary")) break;
        if (!in) continue;
        for (i = 0; i < 3; i++)
        {
            char key[16], *last, *p;
            sprintf(key, "  %s ", ids[i]);
            if (strncmp(line, key, strlen(key)) != 0) continue;
            last = NULL;
            for (p = strtok(line, " \t\r\n"); p; p = strtok(NULL, " \t\r\n")) last = p;
            v[i] = last ? atof(last) : -1.0;
            n++;
            break;
        }
    }
    fclose(f);
    return n;
}

/* maximum depth of the first three reported nodes over all periods of the .out */
static int out_max(const char *out, double v[3])
{
    int head[7], rec[6], p, i;
    long bpp, base;
    FILE *f = fopen(out, "rb");
    if (!f) return 0;
    fread(head, 4, 7, f);                 /* magic, version, units, ns, nn, nl, npol */
    fseek(f, -24L, SEEK_END);
    fread(rec, 4, 6, f);                  /* ..., results pos, periods, error, magic */
    bpp = 8 + 4L * (head[3] * (8 + head[6]) + head[4] * (6 + head[6]) +
                    head[5] * (5 + head[6]) + 15);
    for (i = 0; i < 3; i++) v[i] = 0.0;
    for (p = 0; p < rec[3]; p++)
        for (i = 0; i < 3 && i < head[4]; i++)
        {
            float z;
            base = rec[2] + p * bpp + 8 + 4L * head[3] * (8 + head[6]);
            fseek(f, base + 4L * i * (6 + head[6]), SEEK_SET);
            fread(&z, 4, 1, f);
            if (z > v[i]) v[i] = z;
        }
    fclose(f);
    return rec[3];
}

static int off(double a, double ref) { return fabs(a - ref) > 0.01 + 0.03 * ref; }

int main(void)
{
    double ref[3], all[3] = {-1, -1, -1}, sub[3] = {-1, -1, -1};
    int i, err1, err2, bad = 0;

    setvbuf(stdout, NULL, _IONBF, 0);     /* keep the output if the run aborts */
    err1 = run("CRASH-11_si-all.inp", RPT1, OUT1);
    if (err1 || out_max(OUT1, ref) < 1 || rpt_max(RPT1, all) < 3)
    {
        printf("FAIL: run 1 (all nodes) did not complete (error %d)\n", err1);
        return 1;
    }
    printf("1. all nodes reported\n");
    printf("   Node  Reported Max Depth  Max depth in .out (m)\n");
    for (i = 0; i < 3; i++)
    {
        int b = off(all[i], ref[i]);
        bad += b;
        printf("   %-4s  %18.2f  %21.3f%s\n", ids[i], all[i], ref[i], b ? "  <-- wrong" : "");
    }

    printf("2. only J3 reported (NODES J3)\n");
    err2 = run("CRASH-11_si-subset.inp", RPT2, OUT2);
    if (err2 || rpt_max(RPT2, sub) < 3)
    {
        printf("FAIL: run 2 (node subset) did not complete (error %d)\n", err2);
        return 1;
    }
    printf("   Node  Reported Max Depth  Max depth in run 1 .out (m)\n");
    for (i = 0; i < 3; i++)
    {
        int b = off(sub[i], ref[i]);
        bad += b;
        printf("   %-4s  %18.2f  %27.3f%s\n", ids[i], sub[i], ref[i], b ? "  <-- wrong" : "");
    }

    if (bad)
    {
        printf("FAIL: %d of 6 Reported Max Depths differ from the maximum saved depth "
               "(J1: %.2f and %.2f m, should be %.2f m)\n", bad, all[0], sub[0], ref[0]);
        return 1;
    }
    printf("PASS: Reported Max Depth is the maximum saved depth, with all nodes "
           "and with a node subset\n");
    return 0;
}
