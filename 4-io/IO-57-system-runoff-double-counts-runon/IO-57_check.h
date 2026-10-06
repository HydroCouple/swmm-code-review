/* IO-57: .out checks shared by IO-57_test.c and IO-57_test6.c.
 * The including file defines OUT and run(). */

int main(void)
{
    int head[7], rec[6], ns, nn, nl, np, nsv, nnv, nlv, p, j, err;
    int badRunoff = 0, badInflow, shown = 0;
    double volSys = 0.0, volLat = 0.0;
    long bpp, base;
    float *v;
    FILE *f;

    err = run();
    if (err) { printf("FAIL: run stopped with error %d\n", err); return 1; }

    f = fopen(OUT, "rb");
    fread(head, 4, 7, f);                 /* magic, version, units, ns, nn, nl, npol */
    fseek(f, -24L, SEEK_END);
    fread(rec, 4, 6, f);                  /* ..., results pos, periods, error, magic */
    ns = head[3]; nn = head[4]; nl = head[5]; np = rec[3];
    nsv = 8 + head[6]; nnv = 6 + head[6]; nlv = 5 + head[6];
    bpp = 8 + 4L * (ns * nsv + nn * nnv + nl * nlv + 15);
    v = (float *)malloc(bpp);

    printf("Time   S1 runoff  S2 runoff  Sys runoff  Sum node lat. inflow  Sys lateral inflow (cfs)\n");
    for (p = 0; p < np; p++)
    {
        double s1, s2, sysRunoff, sysInflow, latSum = 0.0;
        int b1;
        base = rec[2] + p * bpp + 8;
        fseek(f, base, SEEK_SET);
        fread(v, 4, (bpp - 8) / 4, f);
        s1 = v[0 * nsv + 4];                              /* SUBCATCH_RUNOFF */
        s2 = v[1 * nsv + 4];
        for (j = 0; j < nn; j++) latSum += v[ns * nsv + j * nnv + 3];   /* NODE_LATFLOW */
        sysRunoff = v[ns * nsv + nn * nnv + nl * nlv + 4];               /* SYS_RUNOFF */
        sysInflow = v[ns * nsv + nn * nnv + nl * nlv + 9];               /* SYS_INFLOW */
        b1 = fabs(sysRunoff - s2) > 0.001 * s2 + 0.001;
        badRunoff += b1;
        volSys += sysInflow * 300.0 / 43560.0;          /* 5-min periods, ac-ft */
        volLat += latSum * 300.0 / 43560.0;
        if ((p + 1) % 6 == 0 && shown < 6)       /* every 30 min */
        {
            printf("%2d:%02d  %9.3f  %9.3f  %10.3f  %20.3f  %18.3f%s\n",
                   (p + 1) * 5 / 60, (p + 1) * 5 % 60, s1, s2, sysRunoff, latSum,
                   sysInflow, b1 ? "  <-- wrong" : "");
            shown++;
        }
    }
    fclose(f);
    free(v);
    badInflow = fabs(volSys - volLat) > 0.03 * volLat;
    printf("Volume over all periods: system lateral inflow %.3f ac-ft, "
           "node lateral inflows %.3f ac-ft (rain: 1 in on 10 ac = 0.833)\n", volSys, volLat);

    if (badRunoff || badInflow)
    {
        printf("FAIL: in %d of %d periods system runoff is not S2's runoff; system lateral "
               "inflow volume %.3f ac-ft against %.3f ac-ft entering the nodes\n",
               badRunoff, np, volSys, volLat);
        return 1;
    }
    printf("PASS: system runoff and lateral inflow count each subcatchment's water once "
           "in all %d periods\n", np);
    return 0;
}
