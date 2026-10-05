#include <stdio.h>
#include <stdlib.h>
#include "unit_root_tests.h"
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "r"); int n; double *x;
    while (fscanf(f, "%d", &n) == 1) {
        x = malloc(n * sizeof(double));
        for (int i = 0; i < n; i++) fscanf(f, "%lf", &x[i]);
        double st, p, cr[3], ks, kp; int lag, nobs, kl;
        int ra = adf_test(x, n, &st, &p, &lag, &nobs, cr);
        kpss_test(x, n, &ks, &kp, &kl);
        printf("%d %.17g %.17g %d %d %.17g %.17g %.17g %d\n", ra, st, p, lag, nobs, cr[1], ks, kp, kl);
        free(x);
    }
    return 0;
}
