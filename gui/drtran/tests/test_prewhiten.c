/* test_prewhiten.c -- la CCF preblanqueada del GUI contra la del motor.
 *
 * lib/prewhiten sale de dentro de drtran.c, asi que la pregunta no es si el
 * codigo parece el mismo: es si los NUMEROS son los mismos. El oraculo es la
 * salida del propio motor,
 *
 *   ./bin/drtran -p tests/cases/ES_CPI_airline.pre tests/cases/WTI_ar1.pre
 *
 * que imprime en su .out la banda, la tabla de r(k) significativos y el
 * veredicto de exogeneidad. Aqui se exigen esos numeros al digito escrito.
 *
 * Y una comprobacion que no es de igualdad, sino de sentido: la CCF SIN
 * preblanquear tiene que dar otra cosa. Si diera lo mismo, preblanquear no
 * estaria haciendo nada y la prueba no distinguiria nada.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "main.h"
#include "fue_pre_reader.h"
#include "prewhiten.h"

real macheps = 2.220446049250313e-16;
FILE *outputv;

static int fallos = 0;

static void check(const char *que, double esperado, double visto, double tol)
{
    int ok = fabs(esperado - visto) <= tol;

    printf("  %-34s  esperado %9.4f   visto %9.4f   %s\n",
           que, esperado, visto, ok ? "ok" : "FALLA");
    if (!ok) fallos++;
}

int main(int argc, char **argv)
{
    struct Tusmodel tm_sal, tm_ent;
    struct Tseries  ts_sal, ts_ent;
    real          **dm_sal = NULL, **dm_ent = NULL;
    double          ccf[200], nu[200];
    real          **res = NULL, Q = 0, p = 0;
    char            why[512] = "";
    int             n = 0, nlags = 24, k, neg = 0;
    double          banda;

    outputv = stderr;

    if (argc < 3) {
        fprintf(stderr, "uso: test_prewhiten <salida.pre> <entrada.pre>\n");
        return 2;
    }

    if (read_fue_pre(argv[1], &tm_sal, &ts_sal, &dm_sal) != 0 ||
        read_fue_pre(argv[2], &tm_ent, &ts_ent, &dm_ent) != 0) {
        fprintf(stderr, "no pude leer los .pre\n");
        return 1;
    }

    if (prewhiten_ccf(&tm_ent, &ts_ent, dm_ent,
                      &tm_sal, &ts_sal, dm_sal,
                      nlags, ccf, nu, &n, &res, why, sizeof why) != 0) {
        fprintf(stderr, "prewhiten_ccf: %s\n", why);
        return 1;
    }
    if (why[0]) printf("aviso: %s\n\n", why);

    banda = 2.0 / sqrt((double) n);

    printf("La CCF preblanqueada, contra lo que imprime el motor con -p\n\n");
    check("observaciones estacionarias", 203.0,     (double) n,     0.5);
    check("banda 2/sqrt(N)",               0.14037, banda,          5e-5);
    check("r(0)",                          0.3542,  ccf[nlags + 0], 5e-4);
    check("r(1)",                          0.2167,  ccf[nlags + 1], 5e-4);
    check("r(12)",                        -0.3282,  ccf[nlags + 12], 5e-4);
    check("r(13)",                        -0.1797,  ccf[nlags + 13], 5e-4);
    check("nu(0)",                         0.0151,  nu[nlags + 0],  5e-5);
    check("nu(12)",                       -0.0140,  nu[nlags + 12], 5e-5);

    /* Los negativos: el motor dice "0 significant out of 24". */
    for (k = 1; k <= nlags; k++)
        if (fabs(ccf[nlags - k]) > banda) neg++;
    check("retardos negativos fuera de banda", 0.0, (double) neg, 0.5);
    check("r(-6), el mayor de los negativos", -0.107, ccf[nlags - 6], 5e-4);

    /* El estadistico, con la rutina del motor sobre los preblanqueados. */
    hosking_test(res, n, 2, nlags, &Q, &p);
    printf("\n  Hosking sobre los preblanqueados: P(%d) = %.1f  (p = %.4f)\n",
           4 * nlags, (double) Q, (double) p);

    /* --- que preblanquear haga algo ---------------------------------------
     * La CCF CRUDA de las dos series, sin filtrar. Si saliera parecida a la
     * preblanqueada, la prueba de arriba no estaria probando nada.        */
    {
    int    nn = ts_sal.nobs < ts_ent.nobs ? ts_sal.nobs : ts_ent.nobs;
    real  *c  = vector(1, nlags + 1);
    real   ma = Mean(ts_ent.data, nn), sa = Stdev(ts_ent.data, nn);
    real   mb = Mean(ts_sal.data, nn), sb = Stdev(ts_sal.data, nn);
    real   Qc = 0, pc = 0;
    real **rc = matrix(1, nn, 1, 2);
    int    t, fuera = 0;
    double bandac = 2.0 / sqrt((double) nn);

    Ccf(ts_ent.data, ts_sal.data, nn, nlags, c, ma, mb, sa, sb);
    for (k = 0; k <= nlags; k++) if (fabs(c[k + 1]) > bandac) fuera++;

    for (t = 1; t <= nn; t++) { rc[t][1] = ts_sal.data[t]; rc[t][2] = ts_ent.data[t]; }
    hosking_test(rc, nn, 2, nlags, &Qc, &pc);

    printf("\n  CCF CRUDA, sin preblanquear: r(0) = %.4f, %d de %d retardos "
           "positivos\n  fuera de banda, P(%d) = %.1f\n",
           (double) c[1], fuera, nlags + 1, 4 * nlags, (double) Qc);

    if (fabs((double) c[1] - ccf[nlags]) < 0.05) {
        printf("  FALLA: la cruda y la preblanqueada dan casi lo mismo en "
               "k=0; preblanquear no esta haciendo nada\n");
        fallos++;
    } else
        printf("  ok: la cruda dice otra cosa, que es justo por lo que no "
               "sirve para identificar\n");

    free_matrix(rc, 1, nn, 1, 2);
    free_vector(c, 1, nlags + 1);
    }

    free_matrix(res, 1, n, 1, 2);

    printf("\n%d fallos\n", fallos);
    return fallos ? 1 : 0;
}
