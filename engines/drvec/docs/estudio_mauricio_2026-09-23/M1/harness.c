/* Read-only instrumentation of drvec: includes drvec.c unchanged, renames its
 * main, and calls build_y2_levels + vec_shootx + elf on a given x[].        */
#include <float.h>
#define main drvec_main
#include "drvec.c"
#undef main

int main(int argc, char **argv)
{
    /* harness data.txt x.txt p q r case marow levels */
    FILE *fd = fopen(argv[1], "r"), *fx = fopen(argv[2], "r");
    int p = atoi(argv[3]), q = atoi(argv[4]), r = atoi(argv[5]);
    int cs = atoi(argv[6]), mr = atoi(argv[7]), lev = atoi(argv[8]);
    int n, M, t, i, j, k, ifault = 0, npar;
    macheps = DBL_EPSILON;
    outputv = stdout;
    if (fscanf(fd, "%d %d", &n, &M) != 2) return 1;
    rawmat = matrix(1, n, 1, M);
    for (t = 1; t <= n; t++) for (j = 1; j <= M; j++)
        if (fscanf(fd, "%lf", &rawmat[t][j]) != 1) return 2;
    nser = M; nobs_raw = n;
    global_p = p; global_q = q; global_r = r; global_case = cs;
    global_marow = mr; global_levels = lev;
    build_y2_levels();
    npar = calc_nparametrs();
    real *x = vector(1, npar);
    for (i = 1; i <= npar; i++) if (fscanf(fx, "%lf", &x[i]) != 1) { fprintf(stderr, "x short at %d of %d\n", i, npar); return 3; }
    printf("npar %d nobs %d\n", npar, nobs);
    struct Tvarma v;
    vec_shootx(x, &v, &ifault, 1, 0);
    printf("ifault %d\n", ifault);
    for (k = 1; k <= p; k++) for (i = 1; i <= M; i++) for (j = 1; j <= M; j++)
        printf("phi %d %d %d %.17g\n", k, i, j, v.phi[k][i][j]);
    for (k = 1; k <= q; k++) for (i = 1; i <= M; i++) for (j = 1; j <= M; j++)
        printf("theta %d %d %d %.17g\n", k, i, j, v.theta[k][i][j]);
    for (i = 1; i <= M; i++) for (j = 1; j <= M; j++)
        printf("qq %d %d %.17g\n", i, j, v.qq[i][j]);
    for (i = 1; i <= M; i++) printf("mu %d %.17g\n", i, v.mu[i]);
    for (t = 1; t <= v.n; t++) for (i = 1; i <= M; i++)
        printf("w %d %d %.17g\n", t, i, v.w[t][i]);
    {
        const real LOG2PI = 1.837877066;
        real f1, f2, le; int ife = 0;
        real tol[2] = { -1.0e-3, 1.0e-3 };
        for (int it = 0; it < 2; it++) {
            elf(v.m, v.n, v.p, v.q, v.mu, v.phi, v.theta, v.qq, v.w, 1.0, tol[it],
                TRUE, v.a, &f1, &f2, &le, &ife);
            real ll = -0.5 * v.m * v.n * (LOG2PI - log((real) v.m) - log((real) v.n) + 1.0)
                      - 0.5 * v.n * (v.m * log(f1) + log(f2));
            printf("elf xitol %g ifault %d logelf_sigma1 %.12f conc %.12f s2 %.12g\n",
                   tol[it], ife, le, ll, f1 / (v.m * v.n));
        }
        for (t = 1; t <= 3; t++) for (i = 1; i <= M; i++)
            printf("a %d %d %.17g\n", t, i, v.a[t][i]);
    }
    printf("granger_sv %.12g\n", granger_sv);
    return 0;
}
