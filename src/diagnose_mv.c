/*****************************************************************************/
/*  diagnose_mv.c -- part of drvec (VEC estimation, Mauricio 2006).
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

/*  Diagnosis multivariante de residuos: portmanteau de Hosking y Jarque-Bera
 *  multivariante.
 *
 *  PROCEDENCIA (2026-08-18): COPIA de drtran/src/diagnose.c -- hosking_test
 *  (:812), jarque_bera_multivariate (:906) y multivariate_diagnostics (:958) --
 *  sin cambios.
 *
 *  POR QUE COPIA Y NO UNA PROPIA.  drvec no hacia ninguna diagnosis: main.h
 *  declaraba estas tres funciones por herencia del header de drvarma pero no
 *  existian en el proyecto, y el .out no decia nada de los residuos.  La primera
 *  version de esto fue un portmanteau escrito aqui, y estaba mal planteado: la
 *  diagnosis tiene que ser LA MISMA que la del resto de la suite, para que el
 *  mismo residuo se lea igual en drvarma, en drtran y aqui.  Un estadistico
 *  propio, aunque sea correcto, obliga a comparar peras con manzanas.
 *
 *  No arrastra nada nuevo: chisq, matrix_inverse, matrix_transpose y
 *  matrix_multiply ya estan en nlatools.c de drvec.
 *
 *  Lo que drvec ANADE por su cuenta -- la tabla de correlaciones cruzadas R(k),
 *  que es lo que dice si quedan efectos cruzados y de que ORDEN -- vive en
 *  drvec.c y esta marcado como añadido, no como parte de esto.
 *
 *  Ver docs/PLAN_BETA.md y docs/DEVELOPMENT_RECORD.md.
 */

#include "main.h"
#include <math.h>

/*---------------------------------------------------------------------------*/
void hosking_test(real **res, int nobs, int m, int s,
                         real *Q, real *pval) {
    int r, i, j, k;
    real ***C;   /* C[r][1..m][1..m] para r = 0..s */
    real **C0inv, **Cr, **Crt, **tmp1, **tmp2, **tmp3;
    real tr;

    C = (real ***)malloc((s+1) * sizeof(real**));
    for (r = 0; r <= s; r++) {
        C[r] = matrix(1, m, 1, m);
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++)
                C[r][i][j] = 0.0;
    }

    /* Calcular medias de las series residuales */
    real *mean = vector(1, m);
    for (j = 1; j <= m; j++) {
        mean[j] = 0.0;
        for (i = 1; i <= nobs; i++) mean[j] += res[i][j];
        mean[j] /= nobs;
    }

    /* Calcular matrices de autocovarianza C[r] */
    for (r = 0; r <= s; r++) {
        int T = nobs - r;
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++) {
                real sum = 0.0;
                for (k = 1; k <= T; k++)
                    sum += (res[k][i] - mean[i]) * (res[k+r][j] - mean[j]);
                C[r][i][j] = sum / nobs;
            }
        }
    }

    /* Invertir C[0] */
    C0inv = matrix(1, m, 1, m);
    matrix_inverse(C[0], C0inv, m);

    /* Acumular Q */
    *Q = 0.0;
    Cr   = matrix(1, m, 1, m);
    Crt  = matrix(1, m, 1, m);
    tmp1 = matrix(1, m, 1, m);
    tmp2 = matrix(1, m, 1, m);
    tmp3 = matrix(1, m, 1, m);

    for (r = 1; r <= s; r++) {
        /* Cr = C[r] */
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++)
                Cr[i][j] = C[r][i][j];

        /* Crt = Cr' */
        matrix_transpose(Cr, Crt, m, m);

        /* tmp1 = Crt * C0inv */
        matrix_multiply(Crt, C0inv, tmp1, m, m, m);

        /* tmp2 = tmp1 * Cr */
        matrix_multiply(tmp1, Cr, tmp2, m, m, m);

        /* tmp3 = tmp2 * C0inv */
        matrix_multiply(tmp2, C0inv, tmp3, m, m, m);

        /* traza de tmp3 */
        tr = 0.0;
        for (i = 1; i <= m; i++) tr += tmp3[i][i];
        *Q += tr;
    }
    *Q *= nobs;

    /* Grados de libertad = m^2 * s */
    int df = m * m * s;
    *pval = 1.0 - chisq(*Q, df);

    /* Liberar memoria */
    free_matrix(tmp3, 1, m, 1, m);
    free_matrix(tmp2, 1, m, 1, m);
    free_matrix(tmp1, 1, m, 1, m);
    free_matrix(Crt, 1, m, 1, m);
    free_matrix(Cr, 1, m, 1, m);
    free_matrix(C0inv, 1, m, 1, m);
    for (r = 0; r <= s; r++)
        free_matrix(C[r], 1, m, 1, m);
    free(C);
    free_vector(mean, 1, m);
}

/*---------------------------------------------------------------------------*/
void jarque_bera_multivariate(real **res, int nobs, int m,
                                      real *JB, real *pval) {
    int i, j;
    real *skew = vector(1, m);
    real *kurt = vector(1, m);
    real *mean = vector(1, m);
    real *sd   = vector(1, m);

    /* Calcular estadísticos univariantes */
    for (j = 1; j <= m; j++) {
        mean[j] = 0.0;
        for (i = 1; i <= nobs; i++) mean[j] += res[i][j];
        mean[j] /= nobs;

        real sum2 = 0.0, sum3 = 0.0, sum4 = 0.0;
        for (i = 1; i <= nobs; i++) {
            real dev = res[i][j] - mean[j];
            sum2 += dev * dev;
            sum3 += dev * dev * dev;
            sum4 += dev * dev * dev * dev;
        }
        sd[j] = sqrt(sum2 / nobs);
        if (sd[j] > 1e-12) {
            skew[j] = (sum3 / nobs) / (sd[j] * sd[j] * sd[j]);
            kurt[j] = (sum4 / nobs) / (sd[j] * sd[j] * sd[j] * sd[j]) - 3.0;
        } else {
            skew[j] = 0.0;
            kurt[j] = 0.0;
        }
    }

    /* Suma de JB univariantes */
    *JB = 0.0;
    for (j = 1; j <= m; j++) {
        real jb_i = nobs * (skew[j] * skew[j] / 6.0 + kurt[j] * kurt[j] / 24.0);
        *JB += jb_i;
    }

    /* Grados de libertad = 2*m */
    int df = 2 * m;
    *pval = 1.0 - chisq(*JB, df);

    free_vector(sd, 1, m);
    free_vector(mean, 1, m);
    free_vector(kurt, 1, m);
    free_vector(skew, 1, m);
}

/*---------------------------------------------------------------------------*/
void multivariate_diagnostics(real **res, int nobs, int m, FILE *outputv) {
    int s = (int) sqrt((double)nobs);  /* número de rezagos para Hosking (puede ajustarse) */
    if (s < 1) s = 1;
    if (s > nobs - 2) s = nobs - 2;

    real Q, pQ, JB, pJB;

    hosking_test(res, nobs, m, s, &Q, &pQ);
    jarque_bera_multivariate(res, nobs, m, &JB, &pJB);

    fprintf(outputv, "\n\n");
    fprintf(outputv, "=============================================================\n");
    fprintf(outputv, "           MULTIVARIATE RESIDUAL DIAGNOSTICS                \n");
    fprintf(outputv, "=============================================================\n");

    fprintf(outputv, "\nHosking's Multivariate Portmanteau Test (lag %d):\n", s);
    fprintf(outputv, "  Q(%d) = %.4f, p-value = %.4f\n", m*m*s, Q, pQ);
    if (pQ < 0.05)
        fprintf(outputv, "  *** REJECT H0: residuals are not white noise.\n");
    else
        fprintf(outputv, "  Cannot reject H0: residuals appear white noise.\n");

    fprintf(outputv, "\nMultivariate Jarque-Bera Test (normality):\n");
    fprintf(outputv, "  JB(%d) = %.4f, p-value = %.4f\n", 2*m, JB, pJB);
    if (pJB < 0.05)
        fprintf(outputv, "  *** REJECT H0: residuals are not normally distributed.\n");
    else
        fprintf(outputv, "  Cannot reject H0: residuals appear normal.\n");

    fprintf(outputv, "=============================================================\n");
}
