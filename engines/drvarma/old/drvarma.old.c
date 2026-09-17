/*****************************************************************************/
/*  DRVARMA.C                                                                */
/*  Exact maximum likelihood estimation of VARMA(p,q) models.                */
/*  Based on DRV.C by José Alberto Mauricio (1995).                          */
/*  Refactored for general VARMA estimation.                                 */
/*  Copyright (C) 2025.                                                      */
/*****************************************************************************/

#include "main.h"                   /* Header file (prototype declarations).  */
#include <ctype.h>
#include <math.h>
real macheps;                      /* Machine epsilon: global variable.      */
FILE *outputv;                     /* Output file: global variable.          */

/*****************************************************************************/

real **datamat;                    /* These variables interface with (*cast) */
int  nser, nobs;                   /* CAUTION: do not declare here variables */
                                   /* whose names are equal to the  names of */
                                   /* variables declared in main or (*cast)! */

/*****************************************************************************/

/*****************************************************************************/
/*  CONFIGURACIÓN GLOBAL DEL MODELO                                          */
/*****************************************************************************/

int  global_p = 1;                  /* Orden AR por defecto                   */
int  global_q = 1;                  /* Orden MA por defecto                   */
int  global_include_mean = 0;       /* Include Mean                           */
int global_diagonal_ar = 0;         /*  0=matriz AR completa, 1=diagonal      */
int global_diagonal_ma = 0;         /*  0=matriz MA completa, 1=diagonal      */
int  global_diagonal_cov = 0;       /* 0=matriz completa, 1=diagonal          */
//int global_structural = 0;   // 0 = no mostrar forma estructural, 1 = mostrar */
/*****************************************************************************/

/*****************************************************************************/
/*  Cálculo del número de parámetros                                         */
/*****************************************************************************/
int calcular_nparametros(void) {
    int npar = 0;
    int m = nser;

    if (global_include_mean) npar += m;

    int n_ar = global_diagonal_ar ? m : m * m;
    npar += n_ar * global_p;

    int n_ma = global_diagonal_ma ? m : m * m;
    npar += n_ma * global_q;

    int n_cov = global_diagonal_cov ? m : m * (m + 1) / 2;
    npar += n_cov;

    return npar;
}

/*****************************************************************************/
/*  PREESTIMACIÓN DE PARÁMETROS                                             */
/*  Calcula valores iniciales para el vector x a partir de los datos        */
/*****************************************************************************/

void preestimar_parametros(real *x, int npar) {
    int m = nser;
    int T = nobs - global_p;
    int i, j, k, t, idx = 1;
    real *mean_est = NULL;

    /* 1. Medias muestrales */
    if (global_include_mean) {
        mean_est = vector(1, m);
        for (j = 1; j <= m; j++) {
            real sum = 0.0;
            for (t = 1; t <= nobs; t++) sum += datamat[t][j];
            mean_est[j] = sum / nobs;
            x[idx++] = mean_est[j];
        }
    }

    /* 2. Centrar datos si hay medias */
    real **datac = matrix(1, nobs, 1, m);
    for (t = 1; t <= nobs; t++)
        for (j = 1; j <= m; j++)
            datac[t][j] = datamat[t][j] - (global_include_mean ? mean_est[j] : 0.0);

    /* 3. Construir matriz de regresores Z (sin constante) */
    real **Z = matrix(1, T, 1, m * global_p);
    real **y = matrix(1, T, 1, m);
    for (t = global_p + 1; t <= nobs; t++) {
        int row = t - global_p;
        int col = 1;
        for (k = 1; k <= global_p; k++) {
            for (j = 1; j <= m; j++) {
                Z[row][col++] = datac[t - k][j];
            }
        }
        for (j = 1; j <= m; j++) y[row][j] = datac[t][j];
    }

    /* 4. Estimar VAR por MCO (ecuación por ecuación) */
    real **ZtZ = matrix(1, m * global_p, 1, m * global_p);
    for (i = 1; i <= m * global_p; i++) {
        for (j = 1; j <= m * global_p; j++) {
            real sum = 0.0;
            for (t = 1; t <= T; t++) sum += Z[t][i] * Z[t][j];
            ZtZ[i][j] = sum;
        }
    }
    int *indx = ivector(1, m * global_p);
    ludcp(ZtZ, m * global_p, indx);

    real **coef = matrix(1, m, 1, m * global_p);
    for (int eq = 1; eq <= m; eq++) {
        real *Zty = vector(1, m * global_p);
        for (i = 1; i <= m * global_p; i++) {
            Zty[i] = 0.0;
            for (t = 1; t <= T; t++) Zty[i] += Z[t][i] * y[t][eq];
        }
        lusol(ZtZ, Zty, m * global_p, indx);
        for (i = 1; i <= m * global_p; i++) coef[eq][i] = Zty[i];
        free_vector(Zty, 1, m * global_p);
    }

    /* 5. Guardar coeficientes AR en x */
    int n_ar_est = (global_diagonal_ar ? m : m * m) * global_p;
    for (k = 1; k <= global_p; k++) {
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++) {
                if (global_diagonal_ar && i != j) {
                    /* no se guarda (queda 0 por defecto) */
                } else {
                    int pos = (k - 1) * m * m + (i - 1) * m + j; // 1‑based
                    int coef_idx = (k - 1) * m + j;
                    x[idx + pos - 1] = coef[i][coef_idx];
                }
            }
        }
    }
    idx += n_ar_est;

    /* 6. Calcular residuos del VAR */
    real **res = matrix(1, nobs, 1, m);
    for (t = 1; t <= nobs; t++) {
        if (t <= global_p) {
            for (j = 1; j <= m; j++) res[t][j] = datac[t][j];
        } else {
            for (i = 1; i <= m; i++) {
                real pred = 0.0;
                int col = 1;
                for (k = 1; k <= global_p; k++) {
                    for (j = 1; j <= m; j++) {
                        pred += coef[i][col] * datac[t - k][j];
                        col++;
                    }
                }
                res[t][i] = datac[t][i] - pred;
            }
        }
    }

    /* 7. Matriz de covarianza de los residuos */
    real **cov_res = matrix(1, m, 1, m);
    for (i = 1; i <= m; i++) {
        for (j = 1; j <= i; j++) {
            real sum = 0.0;
            int cnt = 0;
            for (t = global_p + 1; t <= nobs; t++) {
                sum += res[t][i] * res[t][j];
                cnt++;
            }
            cov_res[i][j] = cov_res[j][i] = sum / cnt;
        }
    }

    /* 8. Guardar covarianza (triangular inferior) */
    if (global_diagonal_cov) {
        for (i = 1; i <= m; i++) x[idx++] = cov_res[i][i];
    } else {
        for (i = 1; i <= m; i++)
            for (j = 1; j <= i; j++)
                x[idx++] = cov_res[i][j];
    }

    /* 9. Inicializar MA a cero (ya están en 0 por defecto, pero avanzamos índice) */
    int n_ma_est = (global_diagonal_ma ? m : m * m) * global_q;
    for (i = 0; i < n_ma_est; i++) x[idx++] = 0.0;

    /* Liberar memoria */
    free_matrix(datac, 1, nobs, 1, m);
    free_matrix(Z, 1, T, 1, m * global_p);
    free_matrix(y, 1, T, 1, m);
    free_matrix(ZtZ, 1, m * global_p, 1, m * global_p);
    free_ivector(indx, 1, m * global_p);
    free_matrix(coef, 1, m, 1, m * global_p);
    free_matrix(res, 1, nobs, 1, m);
    free_matrix(cov_res, 1, m, 1, m);
    if (global_include_mean) free_vector(mean_est, 1, m);
}




/* ------------------------------------------------------------------------- */
/*  main function                                                            */
/* ------------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
    STRING inputf, outputf;
    FILE *inputv;
    real *x, *dev, **cov, gradtol, steptol;
    real *a, *wr, *wi, *wmod;
    int npar, maxits, nrits, ifault, i, j, k, met, t, h, lags;
    struct Tvarma varma1;
    struct Tseries series1, series2, series3;
    real *corr1, *corr2, *totcorr, *a1, *a2;
    real std1, std2, std3;

    /* --------------------------------------------------------------------- */
    /*  Command line argument parsing                                        */
    /* --------------------------------------------------------------------- */
    if (argc < 2) {
        printf("\n");
        printf("DRVARMA - Exact Maximum Likelihood Estimation of VARMA models\n");
        printf("Usage: drvarma datafile [p] [q] [options] [method]\n");
        printf("  datafile : base name of input file (without extension)\n");
        printf("  p, q     : AR and MA orders (integers, default p=1 q=1)\n");
        printf("  options  : mean, diagar, diagma, diagcov (or diagonal)\n");
        printf("  method   : 1 (exact) or 2 (approximate), default=1\n");
        printf("\nExamples:\n");
        printf("  drvarma mydata                # VARMA(1,1) no mean, full matrices\n");
        printf("  drvarma mydata 2 1 mean       # VARMA(2,1) with mean\n");
        printf("  drvarma mydata 2 1 diagcov    # VARMA(2,1) with diagonal covariance\n");
        printf("  drvarma mydata 1 1 diagar diagma 2   # diagonal AR and MA, method 2\n");
        exit(1);
    }

    inputf = NEW_STR(80);
    outputf = NEW_STR(80);
    strcpy(inputf, argv[1]);
    strcpy(outputf, argv[1]);
    strcat(inputf, ".inp");
    strcat(outputf, ".out");

    /* Default values */
    global_p = 1;
    global_q = 1;
    global_include_mean = 0;
    global_diagonal_ar = 0;
    global_diagonal_ma = 0;
    global_diagonal_cov = 0;
    met = 1;

    /* Parse remaining arguments */
    int nums_found = 0;
    for (i = 2; i < argc; i++) {
        /* Check if it's a number (p or q) and we haven't collected two yet */
        if (isdigit(argv[i][0]) && nums_found < 2) {
            int val = atoi(argv[i]);
            if (nums_found == 0) global_p = val;
            else global_q = val;
            nums_found++;
        }
        /* Options */
        else if (strcmp(argv[i], "mean") == 0) {
            global_include_mean = 1;
        }
        else if (strcmp(argv[i], "diagar") == 0) {
            global_diagonal_ar = 1;
        }
        else if (strcmp(argv[i], "diagma") == 0) {
            global_diagonal_ma = 1;
        }
        else if (strcmp(argv[i], "diagcov") == 0 || strcmp(argv[i], "diagonal") == 0) {
            global_diagonal_cov = 1;
        }
        else if (strcmp(argv[i], "1") == 0 || strcmp(argv[i], "2") == 0) {
            met = atoi(argv[i]);
        }
        else {
            printf("Unknown option: %s\n", argv[i]);
            printf("Valid options: mean, diagar, diagma, diagcov (or diagonal), 1, 2\n");
            exit(1);
        }
    }

    /* --------------------------------------------------------------------- */
    /*  Open input data file                                                 */
    /* --------------------------------------------------------------------- */
    inputv = fopen(inputf, "r");
    if (!inputv) {
        printf("ERROR: Cannot open input file %s\n", inputf);
        exit(1);
    }
    fscanf(inputv, "%d", &nser);
    fscanf(inputv, "%d", &nobs);
    datamat = matrix(1, nobs, 1, nser);
    for (i = 1; i <= nobs; i++) {
        for (j = 1; j <= nser; j++) {
            fscanf(inputv, "%lf", &datamat[i][j]);
        }
        fscanf(inputv, "\n");
    }
    fclose(inputv);

    /* --------------------------------------------------------------------- */
    /*  Open output file                                                     */
    /* --------------------------------------------------------------------- */
    outputv = fopen(outputf, "w");
    if (!outputv) {
        printf("ERROR: Cannot create output file %s\n", outputf);
        exit(1);
    }

    /* --------------------------------------------------------------------- */
    /*  Write header                                                         */
    /* --------------------------------------------------------------------- */
    fprintf(outputv, "DRVARMA - Exact Maximum Likelihood Estimation of VARMA models\n");
    fprintf(outputv, "=============================================================\n");
    fprintf(outputv, "Input file   : %s\n", inputf);
    fprintf(outputv, "Output file  : %s\n", outputf);
    fprintf(outputv, "Series       : %d\n", nser);
    fprintf(outputv, "Observations : %d\n", nobs);
    fprintf(outputv, "Model        : VARMA(%d,%d)\n", global_p, global_q);
    fprintf(outputv, "Mean         : %s\n", global_include_mean ? "included" : "zero");
    fprintf(outputv, "AR matrices  : %s\n", global_diagonal_ar ? "diagonal" : "full");
    fprintf(outputv, "MA matrices  : %s\n", global_diagonal_ma ? "diagonal" : "full");
    fprintf(outputv, "Covariance   : %s\n", global_diagonal_cov ? "diagonal" : "full");
    fprintf(outputv, "Method       : %s\n\n", met == 1 ? "Exact" : "Approximate");

    /* --------------------------------------------------------------------- */
    /*  Compute number of parameters and allocate                            */
    /* --------------------------------------------------------------------- */
    npar = calcular_nparametros();
    fprintf(outputv, "Number of parameters: %d\n\n", npar);
    printf("Number of parameters: %d\n", npar);



    x   = vector(1, npar);
    dev = vector(1, npar);
    cov = matrix(1, npar, 1, npar);

    /* --------------------------------------------------------------------- */
    /*  Preliminary estimation                                               */
    /* --------------------------------------------------------------------- */
    preestimar_parametros(x, npar);

    /* --------------------------------------------------------------------- */
    /*  Optimization settings                                                */
    /* --------------------------------------------------------------------- */
    maxits  = 500;
    nrits   = 10;
    gradtol = 1.0e-7;
    steptol = 1.0e-7;
    ifault  = 0;
    macheps = cmacheps();

    varma1.m = nser;
    varma1.n = nobs;
    varma1.p = global_p;
    varma1.q = global_q;
    varma1.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;

    /* --------------------------------------------------------------------- */
    /*  Estimate the model                                                   */
    /* --------------------------------------------------------------------- */
    shootx(x, &varma1, &ifault, 1, 0);            /* allocate memory */
    est(&shootx, npar, x, dev, cov, maxits, nrits, gradtol, steptol,
        varma1.xitol, varma1.a, &varma1.sigma2, &varma1.logelf, &ifault);

    if (ifault) {
        printf("Estimation finished with fault code %d\n", ifault);
        fprintf(outputv, "WARNING: Estimation finished with fault code %d\n", ifault);
        switch (ifault) {
            case 1: fprintf(outputv, "  Q matrix not positive definite.\n"); break;
            case 2: fprintf(outputv, "  AR unit root detected.\n"); break;
            case 3: fprintf(outputv, "  AR strictly non-stationary.\n"); break;
            case 4: fprintf(outputv, "  MA strictly non-invertible.\n"); break;
            case 5: fprintf(outputv, "  Unknown numerical problem.\n"); break;
            case 6: fprintf(outputv, "  See shootx().\n"); break;
        }
    }

    /* Update structure with final estimates */
    shootx(x, &varma1, &ifault, 0, 0);

    /* --------------------------------------------------------------------- */
    /*  Print estimation results                                             */
    /* --------------------------------------------------------------------- */
    fprintf(outputv, "\nESTIMATION RESULTS\n");
    fprintf(outputv, "==================\n");
    fprintf(outputv, "Log-likelihood: %15.8f\n", varma1.logelf);
    fprintf(outputv, "sigma2        : %15.8f\n", varma1.sigma2);

    print_parameters(x, dev, cov, npar, nser);
    print_matrices(&varma1);
    print_roots(&varma1);

    /* --------------------------------------------------------------------- */
    /*  Residual analysis (adapted from original main.c)                     */
    /* --------------------------------------------------------------------- */
    fprintf(outputv, "\nRESIDUAL ANALYSIS\n");
    fprintf(outputv, "=================\n");

    /* Compute sample statistics for residuals (already in varma1.a) */
    fprintf(outputv, "\nSample means and standard deviations of residuals:\n");
    for (i = 1; i <= varma1.m; i++) {
        dev[i] = 0.0;
        for (k = 1; k <= varma1.n; k++)
            dev[i] += varma1.a[k][i];
        dev[i] /= varma1.n;

        for (j = 1; j <= i; j++) {
            cov[i][j] = 0.0;
            for (k = 1; k <= varma1.n; k++)
                cov[i][j] += (varma1.a[k][i] - dev[i]) *
                             (varma1.a[k][j] - dev[j]);
            cov[i][j] /= varma1.n;
        }
    }
    for (i = 1; i <= varma1.m; i++) {
        fprintf(outputv, "  %15.8f", dev[i]);
        fprintf(outputv, " (%15.8f)", sqrt(cov[i][i] / varma1.n));
        fprintf(outputv, "  sd: %15.8f\n", sqrt(cov[i][i]));
    }

    fprintf(outputv, "\nSample covariance matrix of residuals (symmetric):\n");
    for (i = 1; i <= varma1.m; i++) {
        for (j = 1; j <= i; j++)
            fprintf(outputv, "  %15.8f", cov[i][j]);
        fprintf(outputv, "\n");
    }
    fprintf(outputv, "\n");

    /* Individual residual series analysis */
    a = vector(1, varma1.n);
    for (i = 1; i <= varma1.m; i++) {
        fprintf(outputv, "Analysis of residuals a[%2d]\n\n", i);
        for (j = 1; j <= varma1.n; j++)
            a[j] = varma1.a[j][i];

        /* Set series properties: start at observation 1, annual frequency */
        series1.name = "residuals";
        series1.nobs = varma1.n;
        series1.freq = 1;
        series1.begtime = 1;
        series1.begyear = 1;      /* observation numbers as years */
        series1.endtime = 1;
        series1.endyear = 1;      /* will be updated by File_StatSer */
        series1.mean = Mean(a, varma1.n);
        std1 = Stdev(a, varma1.n);
        series1.var = std1 * std1;
        series1.max = MaxVal(a, varma1.n);
        series1.min = MinVal(a, varma1.n);
        series1.skew = Skew(a, varma1.n);
        series1.kurt = Kurt(a, varma1.n);
        series1.data = a;

        File_StatSer(&series1);
        File_PlotSer(&series1);
        File_HistSer(&series1);
        File_CorrSer(&series1, 0);
    }
    free_vector(a, 1, varma1.n);

    /* Cross-correlation functions between residuals */
    a1 = vector(1, varma1.n);
    a2 = vector(1, varma1.n);
    for (i = 1; i < varma1.m; i++) {
        for (t = 1; t <= varma1.n; t++)
            a1[t] = varma1.a[t][i];

        series2.name = "residuals";
        series2.nobs = varma1.n;
        series2.freq = 1;
        series2.begtime = 1;
        series2.begyear = 1;
        series2.endtime = 1;
        series2.endyear = 1;
        series2.mean = Mean(a1, varma1.n);
        std2 = Stdev(a1, varma1.n);
        series2.var = std2 * std2;
        series2.max = MaxVal(a1, varma1.n);
        series2.min = MinVal(a1, varma1.n);
        series2.skew = Skew(a1, varma1.n);
        series2.kurt = Kurt(a1, varma1.n);
        series2.data = a1;

        for (j = i + 1; j <= varma1.m; j++) {
            for (t = 1; t <= varma1.n; t++)
                a2[t] = varma1.a[t][j];

            series3.name = "residuals";
            series3.nobs = varma1.n;
            series3.freq = 1;
            series3.begtime = 1;
            series3.begyear = 1;
            series3.endtime = 1;
            series3.endyear = 1;
            series3.mean = Mean(a2, varma1.n);
            std3 = Stdev(a2, varma1.n);
            series3.var = std3 * std3;
            series3.max = MaxVal(a2, varma1.n);
            series3.min = MinVal(a2, varma1.n);
            series3.skew = Skew(a2, varma1.n);
            series3.kurt = Kurt(a2, varma1.n);
            series3.data = a2;

            fprintf(outputv, "CROSS CORRELATION FUNCTION a[%1d] - a[%1d]\n", j, i);
            fprintf(outputv, "      a[%1d] --> a[%1d] IF k > 0\n", j, i);
            fprintf(outputv, "      a[%1d] --> a[%1d] IF k < 0\n", i, j);

            lags = 9;  /* fixed number of lags, could be made adaptive */
            corr1 = vector(1, lags + 1);
            corr2 = vector(1, lags + 1);
            totcorr = vector(1, 2 * lags + 1);

            ccf(&series2, &series3, lags, corr1);
            ccf(&series3, &series2, lags, corr2);

            for (h = 1; h <= lags + 1; h++)
                totcorr[h] = corr1[lags + 2 - h];
            for (h = 2; h <= lags + 1; h++)
                totcorr[lags + h] = corr2[h];

            PlotCCF(totcorr, lags, &series2);
            fprintf(outputv, "Q(%2d) = %6.3f     k >= 0      Q(%2d) = %6.3f      k > 0\n",
                    lags + 1, ChiTestC(corr2, lags + 1, varma1.n),
                    lags, ChiTestC(corr2, lags + 1, varma1.n) -
                          (corr2[1] * corr2[1] * (varma1.n + 2)));
            fprintf(outputv, "Q(%2d) = %6.3f     k <= 0      Q(%2d) = %6.3f      k < 0\n",
                    lags + 1, ChiTestC(corr1, lags + 1, varma1.n),
                    lags, ChiTestC(corr1, lags + 1, varma1.n) -
                          (corr1[1] * corr1[1] * (varma1.n + 2)));
            fprintf(outputv, "\n\n");

            free_vector(corr1, 1, lags + 1);
            free_vector(corr2, 1, lags + 1);
            free_vector(totcorr, 1, 2 * lags + 1);
        }
    }
    free_vector(a2, 1, varma1.n);
    free_vector(a1, 1, varma1.n);

    /* Write residuals in a format suitable for future use (optional) */
    fprintf(outputv, "\nResiduals (for each series):\n");
    for (i = 1; i <= varma1.m; i++) {
        fprintf(outputv, "\na[%2d]\n", i);
        for (j = 1; j <= varma1.n; j++)
            fprintf(outputv, "  %15.8f\n", varma1.a[j][i]);
    }

    /* Write final parameter vector for potential restart */
    fprintf(outputv, "\nEstimated parameters (for restart):\n");
    for (i = 1; i <= npar; i++)
        fprintf(outputv, "x[%2d] = %15.8f;\n", i, x[i]);

    /* --------------------------------------------------------------------- */
    /*  Cleanup and exit                                                     */
    /* --------------------------------------------------------------------- */
    shootx(x, &varma1, &ifault, 0, 1);            /* deallocate VARMA structure */
    free_matrix(cov, 1, npar, 1, npar);
    free_vector(dev, 1, npar);
    free_vector(x, 1, npar);
    free_matrix(datamat, 1, nobs, 1, nser);
    FREE_STR(outputf);
    FREE_STR(inputf);
    fclose(outputv);
    printf("Estimation completed. Results written to %s\n", outputf);
    return 0;
}


/*****************************************************************************/
/*  shootx: mapea vector de parámetros a estructura VARMA                    */
/*****************************************************************************/
void shootx(real *x, struct Tvarma *armax, int *ifaultx, int firstx, int lastx)
{
    int i, j, k, t, idx = 1;
    int m = armax->m;
    int p = armax->p;
    int q = armax->q;
    real **mtmp0, **mtmp1, **mtmp2, **mtmp3, **mtmp4, *vtmp0;
    int *index;

    real *mu1;
    real ***phi1;
    real ***theta1;
    real **qq1;
    int m1, p1, q1;

    *ifaultx = 0;

    /*************************************************************************/
    /* [1]: Asignación de memoria (firstx == 1)                              */
    /*************************************************************************/
    if (firstx) {
        armax->mu    = vector(1, m);
        armax->phi   = tensor(0, p, 1, m, 1, m);
        armax->theta = tensor(0, q, 1, m, 1, m);
        armax->qq    = matrix(1, m, 1, m);
        armax->w     = matrix(1, armax->n, 1, m);
        armax->a     = matrix(1, armax->n, 1, m);

        for (i = 1; i <= m; i++) {
            armax->mu[i] = 0.0;
            for (j = 1; j <= m; j++) {
                for (k = 0; k <= p; k++) armax->phi[k][i][j] = 0.0;
                for (k = 0; k <= q; k++) armax->theta[k][i][j] = 0.0;
                armax->qq[i][j] = 0.0;
            }
        }
        for (i = 1; i <= m; i++) {
            armax->phi[0][i][i] = 1.0;
            armax->theta[0][i][i] = 1.0;
        }
        for (t = 1; t <= armax->n; t++)
            for (j = 1; j <= m; j++)
                armax->w[t][j] = datamat[t][j];
        return;
    }

    /*************************************************************************/
    /* [2]: Construcción del modelo no normalizado a partir de x            */
    /*************************************************************************/
    m1 = m;
    p1 = p;
    q1 = q;

    mu1    = vector(1, m1);
    phi1   = tensor(0, p1, 1, m1, 1, m1);
    theta1 = tensor(0, q1, 1, m1, 1, m1);
    qq1    = matrix(1, m1, 1, m1);

    for (i = 1; i <= m1; i++) {
        mu1[i] = 0.0;
        for (j = 1; j <= m1; j++) {
            for (k = 0; k <= p1; k++) phi1[k][i][j] = 0.0;
            for (k = 0; k <= q1; k++) theta1[k][i][j] = 0.0;
            qq1[i][j] = 0.0;
        }
        phi1[0][i][i]   = 1.0;
        theta1[0][i][i] = 1.0;
    }

    /* --- Leer medias (si se incluyen) --- */
    if (global_include_mean) {
        for (i = 1; i <= m; i++) mu1[i] = x[idx++];
    } else {
        for (i = 1; i <= m; i++) mu1[i] = 0.0;
    }

    /* --- Leer matrices AR --- */
    for (k = 1; k <= p; k++) {
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++) {
                if (global_diagonal_ar && i != j)
                    phi1[k][i][j] = 0.0;
                else
                    phi1[k][i][j] = x[idx++];
            }
        }
    }

    /* --- Leer matrices MA --- */
    for (k = 1; k <= q; k++) {
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++) {
                if (global_diagonal_ma && i != j)
                    theta1[k][i][j] = 0.0;
                else
                    theta1[k][i][j] = x[idx++];
            }
        }
    }

    /* --- Leer matriz de covarianza (triangular inferior) --- */
    if (global_diagonal_cov) {
        for (i = 1; i <= m; i++) qq1[i][i] = x[idx++];
    } else {
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= i; j++) {
                real val = x[idx++];
                qq1[i][j] = val;
                qq1[j][i] = val;
            }
        }
    }

    if (idx - 1 != calcular_nparametros()) {
        printf("Error in shootx: Incorrect number of parameters.\n");
        exit(1);
    }

    /*************************************************************************/
    /* [3]: Normalización (código original de shootx)                       */
    /*************************************************************************/
    mtmp1 = matrix(1, m1, 1, m1);
    mtmp2 = matrix(1, m1, 1, m1);
    mtmp0 = matrix(1, m1, 1, m1);
    vtmp0 = vector(1, m1);
    index = ivector(1, m1);

    for (i = 1; i <= m1; i++)
        for (j = 1; j <= m1; j++) mtmp0[i][j] = phi1[0][i][j];
    ludcp(mtmp0, m1, index);
    for (j = 1; j <= m1; j++) {
        for (i = 1; i <= m1; i++) vtmp0[i] = 0.0;
        vtmp0[j] = 1.0;
        lusol(mtmp0, vtmp0, m1, index);
        for (i = 1; i <= m1; i++) mtmp1[i][j] = vtmp0[i];
    }

    for (i = 1; i <= m1; i++)
        for (j = 1; j <= m1; j++) mtmp0[i][j] = theta1[0][i][j];
    ludcp(mtmp0, m1, index);
    for (j = 1; j <= m1; j++) {
        for (i = 1; i <= m1; i++) vtmp0[i] = 0.0;
        vtmp0[j] = 1.0;
        lusol(mtmp0, vtmp0, m1, index);
        for (i = 1; i <= m1; i++) mtmp2[i][j] = vtmp0[i];
    }

    free_ivector(index, 1, m1);
    free_vector(vtmp0, 1, m1);
    free_matrix(mtmp0, 1, m1, 1, m1);

    /* AR normalizado */
    for (k = 1; k <= p1; k++)
        for (i = 1; i <= m1; i++)
            for (j = 1; j <= m1; j++) {
                armax->phi[k][i][j] = 0.0;
                for (int k1 = 1; k1 <= m1; k1++)
                    armax->phi[k][i][j] += mtmp1[i][k1] * phi1[k][k1][j];
            }

    mtmp3 = matrix(1, m1, 1, m1);
    mtmp4 = matrix(1, m1, 1, m1);

    /* Theta1[0]^{-1} * phi1[0] */
    for (i = 1; i <= m1; i++)
        for (j = 1; j <= m1; j++) {
            mtmp3[i][j] = 0.0;
            for (int k1 = 1; k1 <= m1; k1++)
                mtmp3[i][j] += mtmp2[i][k1] * phi1[0][k1][j];
        }

    /* MA normalizado */
    for (k = 1; k <= q1; k++) {
        for (i = 1; i <= m1; i++)
            for (j = 1; j <= m1; j++) {
                mtmp4[i][j] = 0.0;
                for (int k1 = 1; k1 <= m1; k1++)
                    mtmp4[i][j] += mtmp1[i][k1] * theta1[k][k1][j];
            }
        for (i = 1; i <= m1; i++)
            for (j = 1; j <= m1; j++) {
                armax->theta[k][i][j] = 0.0;
                for (int k1 = 1; k1 <= m1; k1++)
                    armax->theta[k][i][j] += mtmp4[i][k1] * mtmp3[k1][j];
            }
    }

    /* phi1[0]^{-1} * theta1[0] */
    for (i = 1; i <= m1; i++)
        for (j = 1; j <= m1; j++) {
            mtmp3[i][j] = 0.0;
            for (int k1 = 1; k1 <= m1; k1++)
                mtmp3[i][j] += mtmp1[i][k1] * theta1[0][k1][j];
        }

    /* Hacer simétrica qq1 */
    for (i = 1; i <= m1; i++)
        for (j = i; j <= m1; j++)
            qq1[i][j] = qq1[j][i];

    /* Covarianza normalizada */
    for (i = 1; i <= m1; i++)
        for (j = 1; j <= m1; j++) {
            mtmp4[i][j] = 0.0;
            for (int k1 = 1; k1 <= m1; k1++)
                mtmp4[i][j] += mtmp3[i][k1] * qq1[k1][j];
        }
    for (i = 1; i <= m1; i++)
        for (j = 1; j <= m1; j++) {
            armax->qq[i][j] = 0.0;
            for (int k1 = 1; k1 <= m1; k1++)
                armax->qq[i][j] += mtmp4[i][k1] * mtmp3[j][k1];
        }

    /* Media */
    for (i = 1; i <= m1; i++)
        armax->mu[i] = mu1[i];

    free_matrix(mtmp4, 1, m1, 1, m1);
    free_matrix(mtmp3, 1, m1, 1, m1);
    free_matrix(mtmp2, 1, m1, 1, m1);
    free_matrix(mtmp1, 1, m1, 1, m1);
    free_matrix(qq1, 1, m1, 1, m1);
    free_tensor(theta1, 0, q1, 1, m1, 1, m1);
    free_tensor(phi1, 0, p1, 1, m1, 1, m1);
    free_vector(mu1, 1, m1);

    /*************************************************************************/
    /* [4]: Liberar memoria si lastx == 1                                   */
    /*************************************************************************/
    if (lastx == 1) {
        free_matrix(armax->a, 1, armax->n, 1, armax->m);
        free_matrix(armax->w, 1, armax->n, 1, armax->m);
        free_matrix(armax->qq, 1, armax->m, 1, armax->m);
        free_tensor(armax->theta, 0, armax->q, 1, armax->m, 1, armax->m);
        free_tensor(armax->phi, 0, armax->p, 1, armax->m, 1, armax->m);
        free_vector(armax->mu, 1, armax->m);
    }
}

/*****************************************************************************/
/*****************************************************************************/

void ObsToDate( int beg_per, int beg_sub, int obs_no, int freq,
                int *per, int *sub )
{
   div_t cad;

   if ( obs_no + beg_sub - 1 <= freq )
      {
      *per = beg_per;
      *sub = beg_sub + obs_no - 1;
      }
   else
      {
      cad = div( obs_no - (freq - beg_sub + 1), freq );
      if ( cad.rem > 0 )
         {
         *per = beg_per + cad.quot + 1;
         *sub = cad.rem;
         }
      else
         {
         *per = beg_per + cad.quot;
         *sub = freq;
         }
      }
}

/*****************************************************************************/

void DateToObs( int beg_per, int beg_sub, int per, int sub, int freq,
                int *obs_no )
{
   int srest, pcad, sad;

   srest = freq - beg_sub + 1;
   if ( sub == freq )
      {
      pcad = per - beg_per;
      *obs_no = srest + freq * pcad;
      }
   else
      {
      pcad = per - beg_per - 1;
      sad  = sub;
      *obs_no = srest + freq * pcad + sad;
      }
}

/*****************************************************************************/

real Mean( real *data, int nobs )

{
   int  i;
   real sum;

   sum = 0.0;
   for ( i = 1; i <= nobs; i++ ) sum += data[i];
   return( sum / nobs );
}

/*****************************************************************************/

real Stdev( real *data, int nobs )

{
   int  i;
   real sum, ave;

   ave = Mean( data, nobs );
   sum = 0.0;
   for ( i = 1; i <= nobs; i++ )
       sum += (data[i] - ave) * (data[i] - ave);
   return( sqrt( sum / nobs ) );
}

/*****************************************************************************/

int MaxVal( real *data, int nobs )

{
   int  i, max;
   real maximum;

   max     = 1;
   maximum = data[1];

   for ( i = 2; i <= nobs; i++ )
       if ( data[i] >= maximum )
          {
          maximum = data[i];
          max     = i;
          }
   return( max );
}

/*****************************************************************************/

int MinVal( real *data, int nobs )

{
   int  i, min;
   real minimum;

   min     = 1;
   minimum = data[1];

   for ( i = 2; i <= nobs; i++ )
       if ( data[i] <= minimum )
          {
          minimum = data[i];
          min     = i;
          }
   return( min );
}

/*****************************************************************************/

real Skew( real *data, int nobs )

{
   int  i;
   real sum, ave, std;

   ave = Mean( data, nobs );
   std = Stdev( data, nobs );
   if ( std < 1.0e-20 )
      return( 0.0 );
   else
      {
      sum = 0.0;
      for ( i = 1; i <= nobs; i++ )
          sum += ( (data[i]-ave) * (data[i]-ave) * (data[i]-ave) ) /
                 (std * std * std);
      return( sum / nobs );
      }
}

/*****************************************************************************/

real Kurt( real *data, int nobs )

{
   int  i;
   real sum, ave, std;

   ave = Mean( data, nobs );
   std = Stdev( data, nobs );
   if ( std < 1.0e-20 )
      return( 0.0 );
   else
      {
      sum = 0.0;
      for ( i = 1; i <= nobs; i++ )
          sum += ((data[i]-ave) * (data[i]-ave) * (data[i]-ave) * (data[i]-ave)) /
                 ( std * std * std * std );
      return( sum / nobs - 3.0 );
      }
}

/*****************************************************************************/

void Acf( struct Tseries *ser, int lags, real *corr )

{
   int  i, j;
   real rtmp1, rtmp2;

   for ( i = 1; i <= lags; i++ ) corr[i] = 0.0;

   rtmp1 = ser->mean;
   rtmp2 = ser->var;

   for ( j = 1; j <= lags; j++ ) for ( i = 1; i <= ser->nobs-j; i++ )
       corr[j] += (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
}

/*****************************************************************************/

void Pacf( int lags, real *pcorr )

{
   int  i, j;
   real sum1, sum2, **MatPacf, *corr;

   MatPacf = matrix( 1, lags, 1, lags );
   corr    = vector( 1, lags );

   for ( i = 1; i <= lags; i++ )
       {
       for ( j = 1; j <= lags; j++ )
           MatPacf[i][j] = 0.0;
       corr[i] = pcorr[i];            /* Note that pcorr is input as the acf */
       }                              /* and output as the pacf.             */

   MatPacf[1][1] = corr[1];
   for ( i = 2; i <= lags; i++ )
       {
       sum1 = 0.0;
       sum2 = 0.0;
       for ( j = 1; j <= i-1; j++ )
           {
           sum1 += MatPacf[i-1][j] * corr[i-j];
           sum2 += MatPacf[i-1][j] * corr[j];
           }
       MatPacf[i][i] = (corr[i]-sum1) / (1.0-sum2);
       for ( j = 1; j <= i-1; j++ )
           MatPacf[i][j] = MatPacf[i-1][j] - MatPacf[i][i] * MatPacf[i-1][i-j];
       }
   for ( i = 1; i <= lags; i++ ) pcorr[i] = MatPacf[i][i];

   free_vector( corr, 1, lags );
   free_matrix( MatPacf, 1, lags, 1, lags );
}

/*****************************************************************************/

real ChiTest( real *corr, int lags, int nobs )

{
   int  i;
   real chisqr;

   chisqr = 0.0;
   for ( i = 1; i <= lags; i++ )
       chisqr += (corr[i] * corr[i]) / (nobs-i);
   chisqr *= nobs;
   chisqr *= (nobs + 2);
   return( chisqr );
}

/*****************************************************************************/

void File_StatSer( struct Tseries *ser )

{
   int  Maxy, Maxt, Miny, Mint, Aper, Asub;
   real tmp;

/* Compute sample statistics for time series serk:                           */

   ObsToDate( ser->begyear, ser->begtime, ser->nobs, ser->freq, &Aper, &Asub );
   ser->endtime = Asub;
   ser->endyear = Aper;
   ser->mean = Mean( ser->data, ser->nobs );
   tmp = Stdev( ser->data, ser->nobs );
   ser->var  = tmp * tmp;
   ser->skew = Skew( ser->data, ser->nobs );
   ser->kurt = Kurt( ser->data, ser->nobs );
   ser->max  = MaxVal( ser->data, ser->nobs );
   ser->min  = MinVal( ser->data, ser->nobs );

/* Write to output file:                                                     */

// fprintf( outputv, "%s", ser->name );
   fprintf( outputv, "Unconditional residuals " );
   fprintf( outputv, "(seasonal period: %d)\n", ser->freq );
   fprintf( outputv, "%d observations: ", ser->nobs );
   if ( ser->freq > 1 )
      fprintf( outputv, "from %d/%d to %d/%d\n",
               ser->begtime, ser->begyear, ser->endtime, ser->endyear );
   else
      fprintf( outputv, "from %d to %d\n", ser->begyear, ser->endyear );
   fprintf( outputv, "\n" );

   fprintf( outputv, "                  Mean: %18.6f\n", ser->mean );
   fprintf( outputv, "Standard error of mean: %18.6f\n", tmp / sqrt( ser->nobs ) );
   fprintf( outputv, "              Variance: %18.6f\n", tmp * tmp );
   fprintf( outputv, "    Standard deviation: %18.6f\n", tmp );
   fprintf( outputv, "              Skewness: %18.6f\n", ser->skew );
   fprintf( outputv, "              Kurtosis: %18.6f\n", ser->kurt );

   ObsToDate( ser->begyear, ser->begtime, ser->max, ser->freq, &Maxy, &Maxt );
   ObsToDate( ser->begyear, ser->begtime, ser->min, ser->freq, &Miny, &Mint );

   if ( ser->freq > 1 )
      {
      fprintf( outputv, "               Minimum: %18.6f at %2d/%d (observation %3d)\n",
               ser->data[ser->min], Mint, Miny, ser->min );
      fprintf( outputv, "               Maximum: %18.6f at %2d/%d (observation %3d)\n",
               ser->data[ser->max], Maxt, Maxy, ser->max );
      }
   else
      {
      fprintf( outputv, "               Minimum: %18.6f at %d (observation %3d)\n",
               ser->data[ser->min], Miny, ser->min );
      fprintf( outputv, "               Maximum: %18.6f at %d (observation %3d)\n",
               ser->data[ser->max], Maxy, ser->max );
      }
   fprintf( outputv, "\n" );
}

/*****************************************************************************/

void File_PlotSer( struct Tseries *ser )

{
   int  i, Aper, Asub, itmp1, itmp2, round_local( real );
   real BandPos1, BandPos2, Pos, AbsMax, HorInc, rtmp1, rtmp2, rtmp3, rtmp4;
   STRING Guions, Marcas, Tmpstr;

   Guions = NEW_STR( 80 );
   Marcas = NEW_STR( 80 );
   Tmpstr = NEW_STR( 80 );

   strcpy( Guions, "-------------+-------------------------+-------------------------+--------------" );
   strcpy( Marcas, "                                       0                          " );

   Aper = 0;
   Asub = 0;

/* Maximum value to plot (if < 2.0, then force 3.0):                         */

   itmp1 = ser->max;
   itmp2 = ser->min;
   rtmp1 = ser->data[itmp1];
   rtmp2 = ser->data[itmp2];
   rtmp3 = ser->mean;
   rtmp4 = sqrt( ser->var );

   AbsMax = fabs( (rtmp1 - rtmp3) / rtmp4 );
   if ( fabs( (rtmp2 - rtmp3) / rtmp4 ) > AbsMax )
      AbsMax = fabs( (rtmp2 - rtmp3) / rtmp4 );
   if ( AbsMax <= 2.0 ) AbsMax = 3.0;

   if ( AbsMax > 8.0 )
      {
      fprintf( outputv, "Warning: at least one observation above 8 sigmas\n" );
      goto p1;
      }

/* The value of each character + positions of � and 2� bands:                */

   HorInc   = 25.0 / AbsMax;
   BandPos1 = HorInc;
   BandPos2 = 2.0 * HorInc;

   for ( i = 1; i <= 8; i++ ) if ( AbsMax >= i )
       {
       sprintf( Tmpstr, "%d", i);
//       itoa( i, Tmpstr, 10 );
       Guions[39 - round_local( i * HorInc )]     = '+';
       Marcas[39 - round_local( i * HorInc )]     = Tmpstr[0];
       Marcas[39 - round_local( i * HorInc ) - 1] = '-';
       Guions[39 + round_local( i * HorInc )]     = '+';
       Marcas[39 + round_local( i * HorInc )]     = Tmpstr[0];
       Marcas[39 + round_local( i * HorInc ) - 1] = '+';
       }

   fprintf( outputv, "Standardized time series plot " );
   fprintf( outputv, "(original values on right-side column):\n" );
   fprintf( outputv, "\n" );
   fprintf( outputv, "%s\n", Marcas );
   fprintf( outputv, "%s\n", Guions );

/* Standardized time series plot:                                            */

   for ( i = 1; i <= ser->nobs; i++ )
       {
       fprintf( outputv, "%4d", i );
       ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper, &Asub );
       if ( ser->freq == 1 )
          fprintf( outputv, "%7d ", Aper );
       else
          fprintf( outputv, "%3d/%4d", Asub, Aper );
       Tmpstr[0] = '\0';
       while ( strlen( Tmpstr ) <= 54 ) strcat( Tmpstr, " " );
       if ( (ser->freq != 1) && (Asub == ser->freq) )
          {
          Tmpstr[1]  = '+';
          Tmpstr[53] = '+';
          }
       else
          {
          Tmpstr[1]  = '|';
          Tmpstr[53] = '|';
          }
       if ( fabs( (ser->data[i] - rtmp3) / rtmp4 ) >= 2.0 )
          {
          Tmpstr[0]  = '>';
          Tmpstr[54] = '>';
          }
       Pos = (ser->data[i] - rtmp3) / rtmp4 * HorInc;
       Tmpstr[27 + round_local( Pos )] = '*';
       if ( Tmpstr[27] == ' ' )
          Tmpstr[27] = '|';
       if ( Tmpstr[27 + round_local( BandPos1 )] == ' ' )
          Tmpstr[27 + round_local( BandPos1 )] = ':';
       if ( Tmpstr[27 - round_local( BandPos1 )] == ' ' )
          Tmpstr[27 - round_local( BandPos1 )] = ':';
       if ( Tmpstr[27 + round_local( BandPos2 )] == ' ' )
          Tmpstr[27 + round_local( BandPos2 )] = ':';
       if ( Tmpstr[27 - round_local( BandPos2 )] == ' ' )
          Tmpstr[27 - round_local( BandPos2 )] = ':';
       fprintf( outputv, "%s", Tmpstr );
       fprintf( outputv, "%13.10f\n", ser->data[i] );
       }
   fprintf( outputv, "%s\n", Guions );
   fprintf( outputv, "%s\n", Marcas );
   fprintf( outputv, "\n" );

/* Table of outliers:                                                        */

   fprintf( outputv, "                 +------------------------------------------+\n" );
   fprintf( outputv, "                 |       Table of standardized values       |\n" );
   fprintf( outputv, "                 |       greater than or equal to 2.0       |\n" );
   fprintf( outputv, "                 +------------------------------------------+\n" );
   fprintf( outputv, "                 |                                          |\n" );
   fprintf( outputv, "                 | Observation    Date   Standardized value |\n" );
   fprintf( outputv, "                 |                                          |\n" );

   for ( i = 1; i <= ser->nobs; i++ )
       if ( fabs( (ser->data[i] - rtmp3) / rtmp4 ) >= 2.0 )
          {
          fprintf( outputv, "                 |" );
          fprintf( outputv, "%7d", i );
          ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper, &Asub );
          if ( ser->freq == 1 )
             fprintf( outputv, "%13d ", Aper );
          else
             fprintf( outputv, "%9d/%4d", Asub, Aper );
          fprintf( outputv, "%13.2f", (ser->data[i]-rtmp3)/rtmp4 );
          fprintf( outputv, "        |\n" );
          }
   fprintf( outputv, "                 +------------------------------------------+\n" );
   fprintf( outputv, "\n" );

p1:FREE_STR( Tmpstr );
   FREE_STR( Marcas );
   FREE_STR( Guions );
}

/*****************************************************************************/

void File_HistSer( struct Tseries *ser )

{
   const int NumFil = 17;
   const int NumCol = 64;

   int  itmp1, itmp2, Atip1, Atip2, NumCat, i, j, nphor, fmax, *freqs, *chk;
   real fmax1, rtmp1, rtmp2, xmax, num, ObsPerFil, *breakk;
   STRING *shist, *aux, base1, base2, no, yes, s1, s2;

/* Allocate workspace and initialize:                                        */

   freqs  = ivector( 1, 50 );
   chk    = ivector( 1, 50 );
   breakk = vector( 1, 50 );
   shist  = (STRING *)malloc( (size_t)(NumFil) * sizeof( STRING ) );
   aux    = (STRING *)malloc( (size_t)(NumFil) * sizeof( STRING ) );
   for ( i = 0; i <= NumFil-1; i++ )
       {
       shist[i] = NEW_STR( NumCol );
       aux[i]   = NEW_STR( NumCol );
       }
   base1 = NEW_STR( 80 );
   base2 = NEW_STR( 80 );
   no    = NEW_STR( 80 );
   yes   = NEW_STR( 80 );
   s1    = NEW_STR( 80 );
   s2    = NEW_STR( 80 );

   Atip1 = 0;
   Atip2 = 0;

/* Find maximum absolute value of standardized series:                       */

   itmp1 = ser->max;
   itmp2 = ser->min;
   rtmp1 = ser->mean;
   rtmp2 = sqrt( ser->var );

   xmax = fabs( (ser->data[itmp1]-rtmp1) / rtmp2 );
   if ( fabs( (ser->data[itmp2]-rtmp1) / rtmp2 ) > xmax )
      xmax = fabs( (ser->data[itmp2]-rtmp1) / rtmp2 );

/* Set maximum absolute value to either 4.0 or 8.0:                          */

   if ( xmax > 8.0 )
      {
      fprintf( outputv, "Warning: at least one observation above 8 sigmas\n" );
      goto h1;
      }
   xmax = ( xmax <= 4.0 ) ? 4.0 : 8.0;

/* Number of horizontal characters per category:                             */

   if ( xmax == 4.0 )
      {
      nphor = 4;
      strcpy( no, "    " );
      strcpy( yes, "...." );
      strcpy( base1, "        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+" );
      strcpy( base2, "       -4      -3      -2      -1       0      +1      +2      +3      +4" );
      }
   else
      {
      nphor = 2;
      strcpy( no, "  " );
      strcpy( yes, ".." );
      strcpy( base1, "        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+" );
      strcpy( base2, "       -8  -7  -6  -5  -4  -3  -2  -1   0  +1  +2  +3  +4  +5  +6  +7  +8" );
      }

/* Create vector of "breakpoints":                                           */

   for ( i = 1; i <= 50; i++ ) breakk[i] = 0.0;
   NumCat = 1;
   breakk[NumCat] = -xmax + 0.5;              /* Note that "bandwidth" = 0.5 */
   while ( breakk[NumCat] < xmax )
      {
      NumCat += 1;
      breakk[NumCat] = breakk[NumCat-1] + 0.5;
      }

/* Create vector of frequencies and set number of outliers:                  */

   for ( i = 1; i <= 50; i++ ) freqs[i] = 0;

   for ( i = 1; i <= ser->nobs; i++ )
       {
       num = (ser->data[i]-rtmp1) / rtmp2;
       if ( num <= breakk[1] )
          freqs[1] += 1;
       else
          for ( j = 2; j <= NumCat; j++ )
              if ( (num > breakk[j-1]) && (num <= breakk[j]) ) freqs[j] += 1;
       if ( fabs( num ) >= 2.0 )
          Atip2 += 1;
       if ( fabs( num ) >= 1.0 )
          Atip1 += 1;
       }

/* Maximum frequency = maximum to draw vertically (fills NumFil-1 rows):     */

   fmax = freqs[1];
   for ( i = 2; i <= NumCat; i++ )
       if ( freqs[i] > fmax ) fmax = freqs[i];

/* Number of observations represented by one row of dots:                    */

   fmax1 = fmax;
   ObsPerFil =  ( fmax1 / 16 );

/* Fill the NumFil rows that make up the histogram:                          */

   for ( j = 2; j <= NumFil; j++ )
       {
       for ( i = 1; i <= NumCat; i++ )
           if ( freqs[i] > ObsPerFil * (NumFil-j) )
              strcat( shist[j-1], yes );
           else
              strcat( shist[j-1], no );
       strcpy( s2, shist[j-1] );
       COPY_STR( s2, 0, strlen( s2 )-1, shist[j-1] );
       strcat( shist[j-1], "|" );
       }

   for ( i = 1; i <= 50; i++ ) chk[i] = 0;

   for ( j = 2; j <= NumFil; j++ )
       for ( i = 1; i <= NumCat; i++ )
           if ( (freqs[i] > ObsPerFil * (NumFil-j)) && (chk[i] == 0) )
              {
              if ( nphor == 2 )
                 {
		   sprintf(s1, "%d", freqs[i]);
//                 itoa( freqs[i], s1, 10 );
                 if ( strlen( s1 ) == 1 ) strcat( s1, " " );
                 }
              else
                 {
		   sprintf(s1, "%d", freqs[i]);
//                 itoa( freqs[i], s1, 10 );
                 if ( strlen( s1 ) == 2 )
                    {
                    strcpy( s2, " " );
                    strcat( s1, s2 );
                    strcat( s2, s1 );
                    strcpy( s1, s2 );
                    }
                 else if ( strlen( s1 ) == 1 )
                    {
                    strcpy( s2, " " );
                    strcat( s1, s2 );
                    strcpy( s2, "  " );
                    strcat( s2, s1 );
                    strcpy( s1, s2 );
                    }
                 else if ( strlen( s1 ) == 3 )
                    {
                    strcpy( s2, " " );
                    strcat( s1, s2 );
                    }
                 }
              strcat( aux[j-2], s1 );
              chk[i] = 1;
              }
           else
              strcat( aux[j-2], no );

   strcpy( shist[0], aux[0] );
   shist[0][NumCol-1] = '|';
   shist[0][NumCol]   = '\0';

   for ( j = 2; j <= NumFil-1; j++ )
       for ( i = 1; i <= strlen( aux[j-1] ); i++ )
           if ( aux[j-1][i-1] != ' ' ) shist[j-1][i-1] = aux[j-1][i-1];

/* Write to output file:                                                     */

   fprintf( outputv, "Standardized time series histogram:\n" );
   fprintf( outputv, "\n" );
/*
   for ( i = 1; i <= NumCat; i++ )
       {
       fprintf( outputv, " Breakpoint[%2d] = %5.1f", i, breakk[i] );
       fprintf( outputv, " Frequency[%2d] = %4d\n", i, freqs[i] );
       }
   fprintf( outputv, "\n" );
*/
   fprintf( outputv, "%s\n", base2 );
   fprintf( outputv, "%s\n", base1 );
   for ( i = 1; i <= NumFil; i++ )
       fprintf( outputv, "        |%s\n", shist[i-1] );
   fprintf( outputv, "%s\n", base1 );
   fprintf( outputv, "%s\n", base2 );
   fprintf( outputv, "\n" );

   fprintf( outputv, "%16d values outside (-1,+1): %5.2f %% (31.74 %% expected)\n",
            Atip1, (Atip1 * 100.0) / ser->nobs );
   fprintf( outputv, "%16d values outside (-2,+2): %5.2f %% ( 4.56 %% expected)\n",
            Atip2, (Atip2 * 100.0) / ser->nobs );

   fprintf( outputv, "\n" );

h1:FREE_STR( s2 );
   FREE_STR( s1 );
   FREE_STR( yes );
   FREE_STR( no );
   FREE_STR( base2 );
   FREE_STR( base1 );
   for ( i = NumFil-1; i >= 0; i-- )
       {
       FREE_STR( aux[i] );
       FREE_STR( shist[i] );
       }
   free( (FREE_ARG)aux );
   free( (FREE_ARG)shist );
   free_vector( breakk, 1, 50 );
   free_ivector( chk, 1, 50 );
   free_ivector( freqs, 1, 50 );
}

/*****************************************************************************/

void File_CorrSer( struct Tseries *ser, int npar )

{
   real *corr;
   int  lags;
   void PlotCor( real *, int, int, struct Tseries *, int );

   if ( ser->nobs < 3 * (ser->freq + 1) )
      lags = ser->nobs - ser->freq / 2;
   else
 //     lags = 3 * (ser->freq + 1);
     lags = 3 * (ser->freq + 2);

   corr = vector( 1, lags );

   Acf( ser, lags, corr );
   PlotCor( corr, lags, 1, ser, npar );
   Pacf( lags, corr );
   PlotCor( corr, lags, 0, ser, npar );

   free_vector( corr, 1, lags );
}

/*****************************************************************************/

void PlotCor( real *corr, int lags, int isacf, struct Tseries *ser, int npar )

{
   int  nobs, freq, i, j, posi, symbol, round_local( real );
   real pos, HorInc;
   STRING Guions, Marcas, TmpStr;

   nobs = ser->nobs;
   freq = ser->freq;

   Guions = NEW_STR( 80 );
   Marcas = NEW_STR( 80 );
   TmpStr = NEW_STR( 80 );

   strcpy( Guions, "-------------+-------------------------+-------------------------+--------------" );
   HorInc = 25.0;
   if ( isacf )
      {
      strcpy( Marcas, "            -1                         0                         1  L-B Q  DF" );
      fprintf( outputv, "Autocorrelation function (acf " );
      }
   else
      {
      strcpy( Marcas, "            -1                         0                         1" );
      fprintf( outputv, "Partial autocorrelation function (pacf " );
      }
   fprintf( outputv, "bands =  %5.3f):\n", 2.0 / sqrt( nobs ) );
   fprintf( outputv, "\n" );
   fprintf( outputv, "%s\n", Marcas );
   fprintf( outputv, "%s\n", Guions );

   for ( i = 1; i <= lags; i++ )
       {
       TmpStr[0] = '\0';
       while ( strlen( TmpStr ) <= 52 ) strcat( TmpStr, " " );
       if ( (freq != 1) && ( i % freq == 0) )
          {
          fprintf( outputv, "%4d %7.3f +", i, corr[i] );
          symbol     = '+';
          TmpStr[51] = '+';
          }
       else
          {
          fprintf( outputv, "%4d %7.3f |", i, corr[i] );
          symbol     = '*';
          TmpStr[51] = '|';
          }
       pos  = corr[i] * HorInc;
       posi = abs( round_local( pos ) );
       if ( pos <= 0.0 )
          for ( j = 25 - posi; j <= 25; j++ ) TmpStr[j] = symbol;
       else
          for ( j = 25; j <= 25 + posi; j++ ) TmpStr[j] = symbol;
       TmpStr[25] = '|';
       pos  = 2.0 / sqrt( nobs ) * HorInc;
       posi = round_local( pos );
       if ( TmpStr[25 + posi] == ' ' )
          TmpStr[25 + posi] = ':';
       if ( TmpStr[25 - posi] == ' ' )
          TmpStr[25 - posi] = ':';
       fprintf( outputv, "%s", TmpStr );

       if ( (freq != 1) && (i % freq == 0) && (isacf) && (i - npar >= 1) )
          {
          fprintf( outputv, "%6.2f ", ChiTest( corr, i, nobs ) );
          fprintf( outputv, "%3d", i - npar );
          }
       if ( (i % freq != 0) && (isacf) && (i == lags) && (i - npar >= 1) )
          {
          fprintf( outputv, "%6.2f ", ChiTest( corr, i, nobs ) );
          fprintf( outputv, "%3d", i - npar );
          }
       if ( (freq == 1) && (isacf) && (i == lags) && (i - npar >= 1) )
          {
          fprintf( outputv, "%6.2f ", ChiTest( corr, i, nobs ) );
          fprintf( outputv, "%3d", i - npar );
          }
       fprintf( outputv, "\n" );

       }

   fprintf( outputv, "%s\n", Guions );
   fprintf( outputv, "%s\n", Marcas );
   fprintf( outputv, "\n" );

   FREE_STR( TmpStr );
   FREE_STR( Marcas );
   FREE_STR( Guions );
}

/*****************************************************************************/
/*****************************************************************************/

void PlotCCF( real *corr, int lags, struct Tseries *ser)

{
   int  nobs, freq, i, j, posi, symbol, round_local( real );
   real pos, HorInc;
   STRING Guions, Marcas, TmpStr;

   nobs = ser->nobs;
   freq = ser->freq;

   Guions = NEW_STR( 80 );
   Marcas = NEW_STR( 80 );
   TmpStr = NEW_STR( 80 );

   strcpy( Guions, "-------------+-------------------------+-------------------------+--------------" );
   HorInc = 25.0;
   strcpy( Marcas, "            -1                         0                         1" );
   fprintf( outputv, "CCF BANDS  2.0/SQRT(N) =  %2.5f:\n", 2.0 / sqrt( nobs ) );
   fprintf( outputv, "\n" );
   fprintf( outputv, "%s\n", Marcas );
   fprintf( outputv, "%s\n", Guions );

   for ( i = 1; i <= 2*lags+1; i++ )
       {
       TmpStr[0] = '\0';
       while ( strlen( TmpStr ) <= 52 ) strcat( TmpStr, " " );
       if ( (freq != 1) && ( ( i-lags-1) % freq == 0) )
          {
          fprintf( outputv, "%4d %7.3f +", i-lags-1, corr[i] );
          symbol     = '=';
          TmpStr[51] = '+';
          }
       else
          {
          fprintf( outputv, "%4d %7.3f |", i-lags-1, corr[i] );
          symbol     = '*';
          TmpStr[51] = '|';
          }
       pos  = corr[i] * HorInc;
       posi = abs( round_local( pos ) );
       if ( pos <= 0.0 )
          for ( j = 25 - posi; j <= 25; j++ ) TmpStr[j] = symbol;
       else
          for ( j = 25; j <= 25 + posi; j++ ) TmpStr[j] = symbol;
       TmpStr[25] = '|';
       pos  = 2.0 / sqrt( nobs ) * HorInc;
       posi = round_local( pos );
       if ( TmpStr[25 + posi] == ' ' )
          TmpStr[25 + posi] = ':';
       if ( TmpStr[25 - posi] == ' ' )
          TmpStr[25 - posi] = ':';
       fprintf( outputv, "%s", TmpStr );
       fprintf( outputv, "\n" );

       }

   fprintf( outputv, "%s\n", Guions );
   fprintf( outputv, "%s\n", Marcas );
   fprintf( outputv, "\n" );



   FREE_STR( TmpStr );
   FREE_STR( Marcas );
   FREE_STR( Guions );
}

/*****************************************************************************/
/*****************************************************************************/

void ccf( struct Tseries *ser1, struct Tseries *ser2, int lags, real *corr )

{
   int  i, j;
   real rtmp1, rtmp2, rtmp3, rtmp4;

   for ( i = 1; i <= lags+1; i++ ) corr[i] = 0.0;

   rtmp1 = ser1->mean;
   rtmp2 = Stdev( ser1->data, ser1->nobs );
   rtmp3 = ser2->mean;
   rtmp4 = Stdev( ser2->data, ser2->nobs );

   for ( j = 1; j <= lags+1; j++ ) for ( i = 1; i <= ser1->nobs-j+1; i++ )
       corr[j] += (ser1->data[i]-rtmp1)*(ser2->data[i+j-1]-rtmp3)/((ser1->nobs)*rtmp2*rtmp4);
}

/*****************************************************************************/
/*****************************************************************************/

real ChiTestC( real *corr, int lags, int nobs )

{
   int  i;
   real chisqr;

   chisqr = 0.0;
   for ( i = 1; i <= lags; i++ )
       chisqr += (corr[i] * corr[i]) / (nobs-i+1);
   chisqr *= nobs;
   chisqr *= (nobs + 2);
   return( chisqr );
}

/*****************************************************************************/


/*****************************************************************************/
/* FUNCIONES AUXILIARES PARA TRANSFORMACIÓN BEC                             */
/*****************************************************************************/

/* Calcular error estándar de α₁(k) usando método delta */
real calcular_se_alpha1(int k, real **phi11, real **phi21, real beta,
                       int **idx_phi11, int **idx_phi21, int idx_beta,
                       real **cov, int p, int beta_fijo) {

    real se = 0.0;

    if (beta_fijo) {
        /* β fijo = 1 */
        int idx11 = idx_phi11[k][1];
        int idx21 = idx_phi21[k][1];

        real var_phi11 = cov[idx11][idx11];
        real var_phi21 = cov[idx21][idx21];
        real cov_phi11_phi21 = cov[idx21][idx11];

        if (k == 1) {
            /* α₁ = (φ₁₁ - 1) + β·φ₂₁ */
            se = sqrt(var_phi11 + phi21[k][1]*phi21[k][1]*var_phi21 +
                      2.0*phi21[k][1]*cov_phi11_phi21);
        } else {
            /* α₁ = φ₁₁ + β·φ₂₁ */
            se = sqrt(var_phi11 + phi21[k][1]*phi21[k][1]*var_phi21 +
                      2.0*phi21[k][1]*cov_phi11_phi21);
        }
    } else {
        /* β estimable */
        int idx11 = idx_phi11[k][1];
        int idx21 = idx_phi21[k][1];

        real var_phi11 = cov[idx11][idx11];
        real var_phi21 = cov[idx21][idx21];
        real var_beta = cov[idx_beta][idx_beta];

        real cov_phi11_phi21 = cov[idx21][idx11];
        real cov_phi11_beta = cov[idx_beta][idx11];
        real cov_phi21_beta = cov[idx_beta][idx21];

        /* Derivadas parciales */
        real dalpha_dphi11 = 1.0;
        real dalpha_dphi21 = beta;
        real dalpha_dbeta = phi21[k][1];

        /* Varianza usando método delta */
        se = sqrt(dalpha_dphi11*dalpha_dphi11*var_phi11 +
                  dalpha_dphi21*dalpha_dphi21*var_phi21 +
                  dalpha_dbeta*dalpha_dbeta*var_beta +
                  2.0*dalpha_dphi11*dalpha_dphi21*cov_phi11_phi21 +
                  2.0*dalpha_dphi11*dalpha_dbeta*cov_phi11_beta +
                  2.0*dalpha_dphi21*dalpha_dbeta*cov_phi21_beta);
    }

    return se;
}

/* Calcular error estándar de γ₁₂(k) */
real calcular_se_gamma12(int k, real **phi12, real **phi22, real beta,
                        int **idx_phi12, int **idx_phi22, int idx_beta,
                        real **cov, int p, int beta_fijo) {

    real se = 0.0;

    if (beta_fijo) {
        /* β fijo = 1 */
        int idx12 = idx_phi12[k][1];
        int idx22 = idx_phi22[k][1];

        real var_phi12 = cov[idx12][idx12];
        real var_phi22 = cov[idx22][idx22];
        real cov_phi12_phi22 = cov[idx22][idx12];

        /* γ₁₂ = φ₁₂ + β·φ₂₂ */
        se = sqrt(var_phi12 + phi22[k][1]*phi22[k][1]*var_phi22 +
                  2.0*phi22[k][1]*cov_phi12_phi22);
    } else {
        /* β estimable */
        int idx12 = idx_phi12[k][1];
        int idx22 = idx_phi22[k][1];

        real var_phi12 = cov[idx12][idx12];
        real var_phi22 = cov[idx22][idx22];
        real var_beta = cov[idx_beta][idx_beta];

        real cov_phi12_phi22 = cov[idx22][idx12];
        real cov_phi12_beta = cov[idx_beta][idx12];
        real cov_phi22_beta = cov[idx_beta][idx22];

        /* Derivadas parciales */
        real dgamma_dphi12 = 1.0;
        real dgamma_dphi22 = beta;
        real dgamma_dbeta = phi22[k][1];

        /* Varianza usando método delta */
        se = sqrt(dgamma_dphi12*dgamma_dphi12*var_phi12 +
                  dgamma_dphi22*dgamma_dphi22*var_phi22 +
                  dgamma_dbeta*dgamma_dbeta*var_beta +
                  2.0*dgamma_dphi12*dgamma_dphi22*cov_phi12_phi22 +
                  2.0*dgamma_dphi12*dgamma_dbeta*cov_phi12_beta +
                  2.0*dgamma_dphi22*dgamma_dbeta*cov_phi22_beta);
    }

    return se;
}



/*----------------------------------------------------------------------------*/
/*  Wald test for weak exogeneity of a given variable (1 or 2)               */
/*  H0: all adjustment coefficients for that variable are zero.              */
/*  Returns chi-squared statistic.                                            */
/*----------------------------------------------------------------------------*/
real calcular_chi2_weakex(int var,
                          real **alpha1, real **alpha2,
                          real **phi11, real **phi21,
                          real beta,
                          int **idx_phi11, int **idx_phi21, int idx_beta,
                          real **cov, int p, int *df)
{
    int n_theta, i, j, k, m;
    int *indices;
    real **D, **Sigma_theta, **Sigma_alpha, **U, **V, *w, *alpha_vec, *tmp;
    real chi2 = 0.0;

    /* Número de alphas para la variable (p) */
    int n_alpha = p;

    /* Parámetros subyacentes:
       - Para var=1: phi11(k) y phi21(k) para k=1..p, y beta (si se estima)
       - Para var=2: solo phi21(k) para k=1..p (beta no afecta a alpha2)
    */
    if (var == 1) {
        n_theta = 2 * p + (idx_beta > 0 ? 1 : 0);
    } else {
        n_theta = p;   /* solo phi21 */
    }

    indices = ivector(1, n_theta);
    D       = matrix(1, n_alpha, 1, n_theta);
    Sigma_theta = matrix(1, n_theta, 1, n_theta);
    Sigma_alpha = matrix(1, n_alpha, 1, n_alpha);
    U       = matrix(1, n_alpha, 1, n_alpha);
    V       = matrix(1, n_alpha, 1, n_alpha);
    w       = vector(1, n_alpha);
    alpha_vec = vector(1, n_alpha);
    tmp     = vector(1, n_alpha);

    /* 1. Vector de alphas */
    for (k = 1; k <= p; k++) {
        alpha_vec[k] = (var == 1) ? alpha1[k][1] : alpha2[k][1];
    }

    /* 2. Índices de los parámetros subyacentes */
    m = 1;
    if (var == 1) {
        for (k = 1; k <= p; k++) indices[m++] = idx_phi11[k][1];
        for (k = 1; k <= p; k++) indices[m++] = idx_phi21[k][1];
        if (idx_beta > 0) indices[m++] = idx_beta;
    } else {
        for (k = 1; k <= p; k++) indices[m++] = idx_phi21[k][1];
    }

    /* 3. Extraer submatriz de covarianza Sigma_theta */
    for (i = 1; i <= n_theta; i++) {
        for (j = 1; j <= n_theta; j++) {
            Sigma_theta[i][j] = cov[indices[i]][indices[j]];
        }
    }

    /* 4. Matriz de derivadas D */
    for (i = 1; i <= n_alpha; i++) {
        for (j = 1; j <= n_theta; j++) D[i][j] = 0.0;
    }

    if (var == 1) {
        for (i = 1; i <= p; i++) {
            /* α₁(i) = φ₁₁(i) - δ_{i1} + β·φ₂₁(i)  (δ solo afecta a i=1 en la derivación,
               pero para el Wald usamos la expresión completa; la constante -δ no depende de parámetros) */
            int pos_phi11 = i;           /* columna de φ₁₁(i) */
            int pos_phi21 = p + i;       /* columna de φ₂₁(i) */
            int pos_beta = 2 * p + 1;     /* columna de β (si existe) */

            D[i][pos_phi11] = 1.0;
            D[i][pos_phi21] = beta;
            if (idx_beta > 0) D[i][pos_beta] = phi21[i][1];
        }
    } else {
        for (i = 1; i <= p; i++) {
            /* α₂(i) = φ₂₁(i) */
            int pos_phi21 = i;
            D[i][pos_phi21] = 1.0;
        }
    }

    /* 5. Sigma_alpha = D * Sigma_theta * D' */
    for (i = 1; i <= n_alpha; i++) {
        for (j = 1; j <= n_alpha; j++) {
            real sum = 0.0;
            for (k = 1; k <= n_theta; k++) {
                for (m = 1; m <= n_theta; m++) {
                    sum += D[i][k] * Sigma_theta[k][m] * D[j][m];
                }
            }
            Sigma_alpha[i][j] = sum;
        }
    }

    /* 6. Inversión (pseudoinversa) mediante SVD */
    for (i = 1; i <= n_alpha; i++)
        for (j = 1; j <= n_alpha; j++)
            U[i][j] = Sigma_alpha[i][j];

    svdcp(U, n_alpha, n_alpha, w, V);

    /* Resolver Σ_alpha^{-1} * alpha_vec */
    for (i = 1; i <= n_alpha; i++) tmp[i] = alpha_vec[i];
    svsol(U, w, V, n_alpha, tmp);

    /* 7. Estadístico de Wald */
    chi2 = 0.0;
    for (i = 1; i <= n_alpha; i++) chi2 += alpha_vec[i] * tmp[i];

    /* 8. Grados de libertad efectivos (rango de Sigma_alpha) */
    *df = 0;
    {
        real tol = w[1] * sqrt(macheps);  /* w[1] es el mayor valor singular */
        for (i = 1; i <= n_alpha; i++) {
            if (w[i] > tol) (*df)++;
        }
    }
    if (*df == 0) *df = 1;  /* mínimo 1 grado de libertad */

    /* Liberar memoria */
    free_ivector(indices, 1, n_theta);
    free_matrix(D, 1, n_alpha, 1, n_theta);
    free_matrix(Sigma_theta, 1, n_theta, 1, n_theta);
    free_matrix(Sigma_alpha, 1, n_alpha, 1, n_alpha);
    free_matrix(U, 1, n_alpha, 1, n_alpha);
    free_matrix(V, 1, n_alpha, 1, n_alpha);
    free_vector(w, 1, n_alpha);
    free_vector(alpha_vec, 1, n_alpha);
    free_vector(tmp, 1, n_alpha);

    return chi2;
}

/*----------------------------------------------------------------------------*/
/*  Joint Wald test for H0: all adjustment coefficients are zero.            */
/*  Returns chi-squared statistic and effective degrees of freedom.           */
/*----------------------------------------------------------------------------*/
real calcular_chi2_joint(real **alpha1, real **alpha2,
                         real **phi11, real **phi21,
                         real beta,
                         int **idx_phi11, int **idx_phi21, int idx_beta,
                         real **cov, int p, int *df)
{
    int n_alpha = 2 * p;
    int n_theta = 2 * p + (idx_beta > 0 ? 1 : 0);
    int *indices;
    real **D, **Sigma_theta, **Sigma_alpha, **U, **V, *w, *alpha_vec, *tmp;
    real chi2 = 0.0;
    int i, j, k, m;

    indices = ivector(1, n_theta);
    D       = matrix(1, n_alpha, 1, n_theta);
    Sigma_theta = matrix(1, n_theta, 1, n_theta);
    Sigma_alpha = matrix(1, n_alpha, 1, n_alpha);
    U       = matrix(1, n_alpha, 1, n_alpha);
    V       = matrix(1, n_alpha, 1, n_alpha);
    w       = vector(1, n_alpha);
    alpha_vec = vector(1, n_alpha);
    tmp     = vector(1, n_alpha);

    /* 1. Vector de alphas (orden: α₁(1), α₂(1), α₁(2), α₂(2), …) */
    for (k = 1; k <= p; k++) {
        alpha_vec[2*k - 1] = alpha1[k][1];
        alpha_vec[2*k]     = alpha2[k][1];
    }

    /* 2. Índices de parámetros subyacentes: primero φ₁₁, luego φ₂₁, luego β */
    m = 1;
    for (k = 1; k <= p; k++) indices[m++] = idx_phi11[k][1];
    for (k = 1; k <= p; k++) indices[m++] = idx_phi21[k][1];
    if (idx_beta > 0) indices[m++] = idx_beta;

    /* 3. Extraer Sigma_theta */
    for (i = 1; i <= n_theta; i++)
        for (j = 1; j <= n_theta; j++)
            Sigma_theta[i][j] = cov[indices[i]][indices[j]];

    /* 4. Matriz de derivadas D (n_alpha × n_theta) */
    for (i = 1; i <= n_alpha; i++)
        for (j = 1; j <= n_theta; j++)
            D[i][j] = 0.0;

    for (k = 1; k <= p; k++) {
        int row_alpha1 = 2*k - 1;
        int row_alpha2 = 2*k;
        int col_phi11 = k;                /* columna de φ₁₁(k) */
        int col_phi21 = p + k;             /* columna de φ₂₁(k) */
        int col_beta = 2*p + 1;             /* columna de β (si existe) */

        /* α₁(k) */
        D[row_alpha1][col_phi11] = 1.0;
        D[row_alpha1][col_phi21] = beta;
        if (idx_beta > 0) D[row_alpha1][col_beta] = phi21[k][1];

        /* α₂(k) */
        D[row_alpha2][col_phi21] = 1.0;
        /* β no afecta a α₂ */
    }

    /* 5. Sigma_alpha = D * Sigma_theta * D' */
    for (i = 1; i <= n_alpha; i++) {
        for (j = 1; j <= n_alpha; j++) {
            real sum = 0.0;
            for (k = 1; k <= n_theta; k++)
                for (m = 1; m <= n_theta; m++)
                    sum += D[i][k] * Sigma_theta[k][m] * D[j][m];
            Sigma_alpha[i][j] = sum;
        }
    }

    /* 6. Inversión mediante SVD */
    for (i = 1; i <= n_alpha; i++)
        for (j = 1; j <= n_alpha; j++)
            U[i][j] = Sigma_alpha[i][j];

    svdcp(U, n_alpha, n_alpha, w, V);

    /* Resolver Σ_alpha^{-1} * alpha_vec */
    for (i = 1; i <= n_alpha; i++) tmp[i] = alpha_vec[i];
    svsol(U, w, V, n_alpha, tmp);

    /* 7. Estadístico */
    chi2 = 0.0;
    for (i = 1; i <= n_alpha; i++) chi2 += alpha_vec[i] * tmp[i];

    /* 8. Grados de libertad efectivos */
    *df = 0;
    {
        real tol = w[1] * sqrt(macheps);  /* w[1] es el mayor valor singular */
        for (i = 1; i <= n_alpha; i++) {
            if (w[i] > tol) (*df)++;
        }
    }
    if (*df == 0) *df = 1;

    /* Liberar memoria */
    free_ivector(indices, 1, n_theta);
    free_matrix(D, 1, n_alpha, 1, n_theta);
    free_matrix(Sigma_theta, 1, n_theta, 1, n_theta);
    free_matrix(Sigma_alpha, 1, n_alpha, 1, n_alpha);
    free_matrix(U, 1, n_alpha, 1, n_alpha);
    free_matrix(V, 1, n_alpha, 1, n_alpha);
    free_vector(w, 1, n_alpha);
    free_vector(alpha_vec, 1, n_alpha);
    free_vector(tmp, 1, n_alpha);

    return chi2;
}



/*****************************************************************************/
/*  Impresión de parámetros estimados con estadísticos t y significancia     */
/*****************************************************************************/
void print_parameters(real *x, real *dev, real **cov, int npar, int m) {
    int i, idx = 1;
    real t_stat, p_val;
    char sig[5];

    fprintf(outputv, "\n%33s %12s %12s %8s %6s\n",
            "Parameter", "Estimate", "Std.Error", "t-stat", "p-val");
    fprintf(outputv, "%s\n",
            "--------------------------------------------------------------------");

    /* Medias */
    if (global_include_mean) {
        for (i = 1; i <= m; i++) {
            t_stat = x[idx] / dev[idx];
            p_val = 2.0 * (1.0 - normal_cdf(fabs(t_stat)));
            if (p_val < 0.001) strcpy(sig, "***");
            else if (p_val < 0.01) strcpy(sig, "** ");
            else if (p_val < 0.05) strcpy(sig, "*  ");
            else if (p_val < 0.1) strcpy(sig, ".  ");
            else strcpy(sig, "   ");
            fprintf(outputv, "mu[%d]               %12.6f %12.6f %8.3f %6.4f %s\n",
                    i, x[idx], dev[idx], t_stat, p_val, sig);
            idx++;
        }
    }

    /* AR */
    for (int k = 1; k <= global_p; k++) {
        for (i = 1; i <= m; i++) {
            for (int j = 1; j <= m; j++) {
                if (global_diagonal_ar && i != j) {
                    fprintf(outputv, "phi[%d]_%d%d          (fixed 0.0)\n", k, i, j);
                } else {
                    t_stat = x[idx] / dev[idx];
                    p_val = 2.0 * (1.0 - normal_cdf(fabs(t_stat)));
                    if (p_val < 0.001) strcpy(sig, "***");
                    else if (p_val < 0.01) strcpy(sig, "** ");
                    else if (p_val < 0.05) strcpy(sig, "*  ");
                    else if (p_val < 0.1) strcpy(sig, ".  ");
                    else strcpy(sig, "   ");
                    fprintf(outputv, "phi[%d]_%d%d          %12.6f %12.6f %8.3f %6.4f %s\n",
                            k, i, j, x[idx], dev[idx], t_stat, p_val, sig);
                    idx++;
                }
            }
        }
    }

    /* MA */
    for (int k = 1; k <= global_q; k++) {
        for (i = 1; i <= m; i++) {
            for (int j = 1; j <= m; j++) {
                if (global_diagonal_ma && i != j) {
                    fprintf(outputv, "theta[%d]_%d%d        (fixed 0.0)\n", k, i, j);
                } else {
                    t_stat = x[idx] / dev[idx];
                    p_val = 2.0 * (1.0 - normal_cdf(fabs(t_stat)));
                    if (p_val < 0.001) strcpy(sig, "***");
                    else if (p_val < 0.01) strcpy(sig, "** ");
                    else if (p_val < 0.05) strcpy(sig, "*  ");
                    else if (p_val < 0.1) strcpy(sig, ".  ");
                    else strcpy(sig, "   ");
                    fprintf(outputv, "theta[%d]_%d%d        %12.6f %12.6f %8.3f %6.4f %s\n",
                            k, i, j, x[idx], dev[idx], t_stat, p_val, sig);
                    idx++;
                }
            }
        }
    }

    /* Covarianza */
    if (global_diagonal_cov) {
        for (i = 1; i <= m; i++) {
            t_stat = x[idx] / dev[idx];
            p_val = 2.0 * (1.0 - normal_cdf(fabs(t_stat)));
            if (p_val < 0.001) strcpy(sig, "***");
            else if (p_val < 0.01) strcpy(sig, "** ");
            else if (p_val < 0.05) strcpy(sig, "*  ");
            else if (p_val < 0.1) strcpy(sig, ".  ");
            else strcpy(sig, "   ");
            fprintf(outputv, "cov[%d,%d]           %12.6f %12.6f %8.3f %6.4f %s\n",
                    i, i, x[idx], dev[idx], t_stat, p_val, sig);
            idx++;
        }
    } else {
        for (i = 1; i <= m; i++) {
            for (int j = 1; j <= i; j++) {
                t_stat = x[idx] / dev[idx];
                p_val = 2.0 * (1.0 - normal_cdf(fabs(t_stat)));
                if (p_val < 0.001) strcpy(sig, "***");
                else if (p_val < 0.01) strcpy(sig, "** ");
                else if (p_val < 0.05) strcpy(sig, "*  ");
                else if (p_val < 0.1) strcpy(sig, ".  ");
                else strcpy(sig, "   ");
                fprintf(outputv, "cov[%d,%d]           %12.6f %12.6f %8.3f %6.4f %s\n",
                        i, j, x[idx], dev[idx], t_stat, p_val, sig);
                idx++;
            }
        }
    }
    fprintf(outputv, "%s\n",
            "--------------------------------------------------------------------");
    fprintf(outputv, "Signif. codes:  0 '***' 0.001 '**' 0.01 '*' 0.05 '.' 0.1 ' ' 1\n\n");
}

/*****************************************************************************/
/*  Impresión de matrices del modelo normalizado                             */
/*****************************************************************************/
void print_matrices(struct Tvarma *varma) {
    int i, j, k;
    int m = varma->m;
    int p = varma->p;
    int q = varma->q;

    fprintf(outputv, "\nNormalized model matrices:\n");
    fprintf(outputv, "===========================\n");

    fprintf(outputv, "\nVector mu (intercepts):\n");
    for (i = 1; i <= m; i++)
        fprintf(outputv, "  mu[%d] = %12.6f\n", i, varma->mu[i]);

    for (k = 1; k <= p; k++) {
        fprintf(outputv, "\nAR matrix phi(%d):\n", k);
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++)
                fprintf(outputv, "  %12.6f", varma->phi[k][i][j]);
            fprintf(outputv, "\n");
        }
    }

    for (k = 1; k <= q; k++) {
        fprintf(outputv, "\nMA matrix theta(%d):\n", k);
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++)
                fprintf(outputv, "  %12.6f", varma->theta[k][i][j]);
            fprintf(outputv, "\n");
        }
    }

    fprintf(outputv, "\nCovariance matrix Q (symmetric):\n");
    for (i = 1; i <= m; i++) {
        for (j = 1; j <= i; j++)
            fprintf(outputv, "  %12.6f", varma->qq[i][j]);
        fprintf(outputv, "\n");
    }

    fprintf(outputv, "\nResidual covariance matrix (sigma2 * Q):\n");
    for (i = 1; i <= m; i++) {
        for (j = 1; j <= i; j++)
            fprintf(outputv, "  %12.6f", varma->sigma2 * varma->qq[i][j]);
        fprintf(outputv, "\n");
    }
}

/*****************************************************************************/
/*  Impresión de raíces inversas de los polinomios AR y MA                   */
/*****************************************************************************/
void print_roots(struct Tvarma *varma) {
    int i, ifault;
    real *wr, *wi, *wmod;
    int m = varma->m;
    int p = varma->p;
    int q = varma->q;

    if (p > 0) {
        wr = vector(1, m * p);
        wi = vector(1, m * p);
        wmod = vector(1, m * p);
        chekma(m, p, varma->phi, wr, wi, wmod, &ifault);
        fprintf(outputv, "\nInverse roots of AR polynomial det[phi(B)] = 0:\n");
        fprintf(outputv, "%20s %20s %20s\n", "Real", "Imaginary", "Modulus");
        for (i = 1; i <= m * p; i++)
            if (fabs(wmod[i]) >= 0.00005)
                fprintf(outputv, "  %20.8f %20.8f %20.8f\n", wr[i], wi[i], wmod[i]);
        free_vector(wmod, 1, m * p);
        free_vector(wi, 1, m * p);
        free_vector(wr, 1, m * p);
    }

    if (q > 0) {
        wr = vector(1, m * q);
        wi = vector(1, m * q);
        wmod = vector(1, m * q);
        chekma(m, q, varma->theta, wr, wi, wmod, &ifault);
        fprintf(outputv, "\nInverse roots of MA polynomial det[theta(B)] = 0:\n");
        fprintf(outputv, "%20s %20s %20s\n", "Real", "Imaginary", "Modulus");
        for (i = 1; i <= m * q; i++)
            if (fabs(wmod[i]) >= 0.00005)
                fprintf(outputv, "  %20.8f %20.8f %20.8f\n", wr[i], wi[i], wmod[i]);
        free_vector(wmod, 1, m * q);
        free_vector(wi, 1, m * q);
        free_vector(wr, 1, m * q);
    }
}

