/* probe_ladder_key.c -- BUG-60: ladder_key() (src/drvec.c:5310) loses the
 * data once a drift is held.
 *
 * ladder_key is static in drvec.c, so this probe carries a verbatim copy of
 * its arithmetic (same terms, same order of additions) for p = 2, q = 0,
 * case 3, no diagonal flags, no -xsys.  It reads mink-muskrat in levels
 * (datasets/mauricio/mink_muskrat.inp), computes the key, changes ONE
 * observation by 1e-3 and computes it again: with a held drift the two keys
 * are equal, without one they differ.
 *
 *   gcc -o pk probe_ladder_key.c && ./pk ../../../datasets/mauricio/mink_muskrat.inp
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define T 62
#define M 2
static double raw[T + 1][M + 1];

static double key(int held)            /* drvec.c ladder_key(), case 3 */
{
    double k = 2 * 1e6 + 0 * 1e4 + 3 * 1e3 + 0 * 100 + 0 * 10 + 0
             + T * 1e-3 + 0 * 0.5;
    int t, i;
    for (i = 1; i <= M; i++)
        if (held & (1 << (i - 1))) k += 1e12 * (double) (1 << (i - 1));
    for (t = 1; t <= T; t++)
        for (i = 1; i <= M; i++) k += raw[t][i] * (1e-7 * (t % 97 + i));
    return k;
}

int main(int argc, char **argv)
{
    char line[512];
    int t = 0, hdr = 0;
    FILE *f = fopen(argc > 1 ? argv[1] : "mink_muskrat.inp", "r");
    if (!f) { perror("data"); return 2; }
    while (fgets(line, sizeof line, f) && t < T) {
        if (line[0] == '*') continue;
        if (hdr < 4) { hdr++; continue; }   /* layout, dims, names, refactor */
        if (sscanf(line, "%lf %lf", &raw[t + 1][1], &raw[t + 1][2]) == 2) t++;
    }
    fclose(f);
    if (t != T) { fprintf(stderr, "read %d observations, not %d\n", t, T); return 2; }

    double a0 = key(0), a1 = key(1);
    raw[31][2] += 1e-3;                    /* one observation, a material change */
    double b0 = key(0), b1 = key(1);
    printf("  drift free : key %.10f -> %.10f  %s\n", a0, b0, a0 == b0 ? "SAME" : "differ");
    printf("  drift held : key %.10f -> %.10f  %s\n", a1, b1, a1 == b1 ? "SAME" : "differ");
    return a1 == b1;                       /* 1 while the defect is open */
}
