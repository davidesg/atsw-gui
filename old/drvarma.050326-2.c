/*****************************************************************************/
/*  DRVARMA.C                                                               */
/*  Exact maximum likelihood estimation of general VARMA(p,q) models.      */
/*  Refactored from original DRV.C to support flexible model specification. */
/*  Usage: drvarma input p q [-mean] [-diagar] [-diagma] [-diagcov] [-m mtd]*/
/*         [-twostep]                                                        */
/*         input : data file name (without .inp)                            */
/*         p     : AR order                                                  */
/*         q     : MA order                                                  */
/*         -mean       : include mean vector (default: no)                  */
/*         -diagar     : diagonal AR matrices (default: full)               */
/*         -diagma     : diagonal MA matrices (default: full)               */
/*         -diagcov    : diagonal covariance matrix (default: full)         */
/*         -m method   : 1 = exact, 2 = approximate (default: 1)            */
/*         -twostep    : use two-step initialization (diagonal then full)   */
/*****************************************************************************/

#include "main.h"
#include <getopt.h>  /* optional, can be replaced by manual parsing */

real macheps;
FILE *outputv;
int quiet_mode = 0;   /* default: not quiet */

real **datamat;
int nser, nobs;

/* Global model configuration */
int global_p, global_q;
int global_include_mean = 0;
int global_diag_ar = 0;
int global_diag_ma = 0;
int global_diag_cov = 0;
int met = 1;   /* estimation method: 1 exact, 2 approximate */
int global_twostep = 0;  /* two-step initialization */

/*****************************************************************************/
/*  Local prototypes                                                         */
/*****************************************************************************/
static void shootx(real *x, struct Tvarma *armax, int *ifaultx, int firstx, int lastx);
static int calc_nparametrs(void);
static void init_varma(real *x, int npar);
static void init_diag_varma(real *x, int npar);
static void combine_vectors(real *x_full, real *x_diag, int npar_full, int npar_diag);
static void print_parameters(real *x, real *dev, real **cov, int npar, struct Tvarma *varma);
static void print_matrices(struct Tvarma *varma);
static void print_roots(struct Tvarma *varma);

/*****************************************************************************/
/*  Main function                                                            */
/*****************************************************************************/
int main(int argc, char *argv[])
{
    STRING inputf, outputf;
    FILE *inputv;
    real *x, *dev, **cov, gradtol, steptol;
    int npar, maxits, nrits, ifault, i, j, k;
    struct Tvarma varma1;

    /* [1] Process command line arguments */
    if (argc < 4) {
        printf("Usage: %s file p q [-mean] [-diagar] [-diagma] [-diagcov] [-m method] [-twostep]\n", argv[0]);
        printf("  method: 1 = exact, 2 = approximate (default=1)\n");
        printf("  -twostep: use two-step initialization (diagonal then full)\n");
        exit(1);
    }

    inputf = NEW_STR(80);
    outputf = NEW_STR(80);
    strcpy(inputf, argv[1]);
    global_p = atoi(argv[2]);
    global_q = atoi(argv[3]);

    /* Parse additional options */
    for (i = 4; i < argc; i++) {
        if (strcmp(argv[i], "-mean") == 0)
            global_include_mean = 1;
        else if (strcmp(argv[i], "-diagar") == 0)
            global_diag_ar = 1;
        else if (strcmp(argv[i], "-diagma") == 0)
            global_diag_ma = 1;
        else if (strcmp(argv[i], "-diagcov") == 0)
            global_diag_cov = 1;
        else if (strcmp(argv[i], "-m") == 0 && i+1 < argc)
            met = atoi(argv[++i]);
        else if (strcmp(argv[i], "-twostep") == 0)
            global_twostep = 1;
    }

    strcpy(outputf, inputf);
    strcat(outputf, ".out");
    strcat(inputf, ".inp");

    printf("\nInput Data File  : %s\n", inputf);
    printf("Output File      : %s\n", outputf);
    printf("Model: VARMA(%d,%d)\n", global_p, global_q);
    printf("Include mean     : %s\n", global_include_mean ? "yes" : "no");
    printf("Diagonal AR      : %s\n", global_diag_ar ? "yes" : "no");
    printf("Diagonal MA      : %s\n", global_diag_ma ? "yes" : "no");
    printf("Diagonal Cov     : %s\n", global_diag_cov ? "yes" : "no");
    printf("Estimation method: %d\n", met);
    printf("Two-step init    : %s\n", global_twostep ? "yes" : "no");

    /* [2] Read data */
    if (NULL == (inputv = fopen(inputf, "r"))) {
        printf("ERROR: cannot open %s\n", inputf);
        exit(1);
    }
    fscanf(inputv, "%d", &nser);
    fscanf(inputv, "%d\n", &nobs);
    datamat = matrix(1, nobs, 1, nser);
    for (i = 1; i <= nobs; i++) {
        for (j = 1; j <= nser; j++)
            fscanf(inputv, "%lf", &datamat[i][j]);
        fscanf(inputv, "\n");
    }
    fclose(inputv);
    printf("Series: %d, Observations: %d\n", nser, nobs);

    /* [3] Open output file */
    if (NULL == (outputv = fopen(outputf, "w"))) {
        printf("ERROR: cannot create %s\n", outputf);
        exit(1);
    }
    fprintf(outputv, "Input Data File  : %s\n", inputf);
    fprintf(outputv, "Output File      : %s\n", outputf);
    fprintf(outputv, "Model: VARMA(%d,%d)\n", global_p, global_q);
    fprintf(outputv, "Include mean     : %s\n", global_include_mean ? "yes" : "no");
    fprintf(outputv, "Diagonal AR      : %s\n", global_diag_ar ? "yes" : "no");
    fprintf(outputv, "Diagonal MA      : %s\n", global_diag_ma ? "yes" : "no");
    fprintf(outputv, "Diagonal Cov     : %s\n", global_diag_cov ? "yes" : "no");
    fprintf(outputv, "Estimation method: %d\n", met);
    fprintf(outputv, "Two-step init    : %s\n\n", global_twostep ? "yes" : "no");

    /* [4] Determine number of parameters and allocate memory */
    npar = calc_nparametrs();
    x   = vector(1, npar);
    dev = vector(1, npar);
    cov = matrix(1, npar, 1, npar);

    printf("Number of parameters: %d\n", npar);
    fprintf(outputv, "Number of parameters: %d\n", npar);

    /* [5] Initialize parameters (pre?estimation) */
    init_varma(x, npar);

    /* [5b] Two-step initialization if requested and model not fully diagonal */
    if (global_twostep && (global_diag_ar == 0 || global_diag_ma == 0 || global_diag_cov == 0)) {
        printf("Performing two-step initialization...\n");
        /* Save current flags */
        int save_diag_ar = global_diag_ar;
        int save_diag_ma = global_diag_ma;
        int save_diag_cov = global_diag_cov;
        /* Force diagonal */
        global_diag_ar = global_diag_ma = global_diag_cov = 1;
        int npar_diag = calc_nparametrs();
        real *x_diag = vector(1, npar_diag);
        /* Initialize diagonal model */
        init_diag_varma(x_diag, npar_diag);

        /* Estimate diagonal model with few iterations */
        struct Tvarma varma_diag;
        real *dev_diag = vector(1, npar_diag);
        real **cov_diag = matrix(1, npar_diag, 1, npar_diag);
        int ifault_diag = 0;
        int maxits_diag = 50;
        int nrits_diag = 10;
        real gradtol_diag = 1e-5;
        real steptol_diag = 1e-5;
        real xitol_diag = (met == 2) ? -1e-3 : 1e-3;
        real sigma2_diag, logelf_diag;

        int old_quiet = quiet_mode;
        quiet_mode = 1;  /* suppress output */

        shootx(x_diag, &varma_diag, &ifault_diag, 1, 0);
        est(&shootx, npar_diag, x_diag, dev_diag, cov_diag, maxits_diag, nrits_diag,
            gradtol_diag, steptol_diag, xitol_diag, varma_diag.a, &sigma2_diag, &logelf_diag, &ifault_diag);
        shootx(x_diag, &varma_diag, &ifault_diag, 0, 1); /* deallocate */

        quiet_mode = old_quiet;

     /* Restore original flags BEFORE combining */
        global_diag_ar = save_diag_ar;
        global_diag_ma = save_diag_ma;
        global_diag_cov = save_diag_cov;

        if (ifault_diag == 0) {
            /* Combine vectors */
            combine_vectors(x, x_diag, npar, npar_diag);
            printf("Two-step initialization completed.\n");
        } else {
            printf("Warning: diagonal estimation failed (ifault=%d), using original initialization.\n", ifault_diag);
        }



        /* Free temporary memory */
        free_vector(x_diag, 1, npar_diag);
        free_vector(dev_diag, 1, npar_diag);
        free_matrix(cov_diag, 1, npar_diag, 1, npar_diag);
    }

    /* [6] Optimizer parameters */
    maxits  = 500;
    nrits   = 10;
    gradtol = 1.0e-7;
    steptol = 1.0e-7;
    ifault  = 0;
    macheps = cmacheps();

    varma1.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;

    /* [7] Estimate */
    shootx(x, &varma1, &ifault, 1, 0);   /* allocate memory */
    est(&shootx, npar, x, dev, cov, maxits, nrits, gradtol, steptol,
        varma1.xitol, varma1.a, &varma1.sigma2, &varma1.logelf, &ifault);

    if (ifault) {
        printf("Estimation error: ");
        switch (ifault) {
            case 1: printf("Q matrix not positive definite\n"); break;
            case 2: printf("unit root in AR\n"); break;
            case 3: printf("AR nonstationary\n"); break;
            case 4: printf("MA noninvertible\n"); break;
            case 5: printf("numerical problem\n"); break;
            case 6: printf("error in shootx\n"); break;
            default: printf("code %d\n", ifault);
        }
    }

    /* [8] Retrieve final model */
    shootx(x, &varma1, &ifault, 0, 0);

    /* [9] Write results */
    print_parameters(x, dev, cov, npar, &varma1);
    print_matrices(&varma1);
    print_roots(&varma1);

    /* [10] Free memory and close */
    shootx(x, &varma1, &ifault, 0, 1);   /* deallocate model memory */
    free_matrix(cov, 1, npar, 1, npar);
    free_vector(dev, 1, npar);
    free_vector(x, 1, npar);
    free_matrix(datamat, 1, nobs, 1, nser);
    FREE_STR(outputf);
    FREE_STR(inputf);
    fclose(outputv);
    return 0;
}

/*****************************************************************************/
/*  shootx: maps parameter vector to Tvarma structure                        */
/*  (unchanged)                                                              */
/*****************************************************************************/
static void shootx(real *x, struct Tvarma *armax, int *ifaultx, int firstx, int lastx)
{

    int i, j, k, idx = 1;
    int m = nser;        /* dimensión del sistema */
    int p = global_p;
    int q = global_q;

    *ifaultx = 0;

    /* [1] Asignar dimensiones */
    armax->m = m;
    armax->n = nobs;
    armax->p = p;
    armax->q = q;

    /* [2] Asignar memoria si es primera llamada */
    if (firstx) {
        armax->mu    = vector(1, m);
        armax->phi   = tensor(0, p, 1, m, 1, m);
        armax->theta = tensor(0, q, 1, m, 1, m);
        armax->qq    = matrix(1, m, 1, m);
        armax->w     = matrix(1, nobs, 1, m);
        armax->a     = matrix(1, nobs, 1, m);

        /* Inicializar a cero */
        for (i = 1; i <= m; i++) {
            armax->mu[i] = 0.0;
            for (j = 1; j <= m; j++) {
                for (k = 0; k <= p; k++) armax->phi[k][i][j] = 0.0;
                for (k = 0; k <= q; k++) armax->theta[k][i][j] = 0.0;
                armax->qq[i][j] = 0.0;
            }
            for (j = 1; j <= nobs; j++) {
                armax->w[j][i] = 0.0;
                armax->a[j][i] = 0.0;
            }
        }
        /* phi[0] y theta[0] = identidad */
        for (i = 1; i <= m; i++) {
            armax->phi[0][i][i] = 1.0;
            armax->theta[0][i][i] = 1.0;
        }
    }

    /* [3] Construir modelo no normalizado (phi1, theta1, qq1, mu1) */
    /* Asignar memoria temporal */
    real *mu1    = vector(1, m);
    real ***phi1   = tensor(0, p, 1, m, 1, m);
    real ***theta1 = tensor(0, q, 1, m, 1, m);
    real **qq1    = matrix(1, m, 1, m);

    /* Inicializar a cero */
    for (i = 1; i <= m; i++) {
        mu1[i] = 0.0;
        for (j = 1; j <= m; j++) {
            for (k = 0; k <= p; k++) phi1[k][i][j] = 0.0;
            for (k = 0; k <= q; k++) theta1[k][i][j] = 0.0;
            qq1[i][j] = 0.0;
        }
        phi1[0][i][i] = 1.0;
        theta1[0][i][i] = 1.0;
    }

    /* [3a] Medias (si se incluyen) */
    if (global_include_mean) {
        for (i = 1; i <= m; i++)
            mu1[i] = x[idx++];
    }

    /* [3b] Parámetros AR */
    for (k = 1; k <= p; k++) {
        if (global_diag_ar) {
            for (i = 1; i <= m; i++) {
                phi1[k][i][i] = x[idx++];
            }
        } else {
            for (i = 1; i <= m; i++) {
                for (j = 1; j <= m; j++) {
                    phi1[k][i][j] = x[idx++];
                }
            }
        }
    }

    /* [3c] Parámetros MA */
    for (k = 1; k <= q; k++) {
        if (global_diag_ma) {
            for (i = 1; i <= m; i++) {
                theta1[k][i][i] = x[idx++];
            }
        } else {
            for (i = 1; i <= m; i++) {
                for (j = 1; j <= m; j++) {
                    theta1[k][i][j] = x[idx++];
                }
            }
        }
    }

    /* [3d] Matriz de covarianza Q (triangular inferior) */
    if (global_diag_cov) {
        for (i = 1; i <= m; i++) {
            qq1[i][i] = x[idx++];
        }
    } else {
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= i; j++) {
                qq1[i][j] = x[idx++];
                qq1[j][i] = qq1[i][j];   /* simetría */
            }
        }
    }

    /* [4] Normalización (igual que en original, usando phi1[0] y theta1[0]) */
    /* Se asume que las funciones ludcp, lusol, etc. están disponibles */
    real **mtmp1 = matrix(1, m, 1, m);
    real **mtmp2 = matrix(1, m, 1, m);
    real **mtmp0 = matrix(1, m, 1, m);
    real *vtmp0  = vector(1, m);
    int *index   = ivector(1, m);

    /* mtmp0 = phi1[0] */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++)
            mtmp0[i][j] = phi1[0][i][j];
    ludcp(mtmp0, m, index);
    for (j = 1; j <= m; j++) {
        for (i = 1; i <= m; i++) vtmp0[i] = 0.0;
        vtmp0[j] = 1.0;
        lusol(mtmp0, vtmp0, m, index);
        for (i = 1; i <= m; i++) mtmp1[i][j] = vtmp0[i];
    }

    /* mtmp0 = theta1[0] */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++)
            mtmp0[i][j] = theta1[0][i][j];
    ludcp(mtmp0, m, index);
    for (j = 1; j <= m; j++) {
        for (i = 1; i <= m; i++) vtmp0[i] = 0.0;
        vtmp0[j] = 1.0;
        lusol(mtmp0, vtmp0, m, index);
        for (i = 1; i <= m; i++) mtmp2[i][j] = vtmp0[i];
    }

    free_ivector(index, 1, m);
    free_vector(vtmp0, 1, m);
    free_matrix(mtmp0, 1, m, 1, m);

    /* AR normalizado */
    for (k = 1; k <= p; k++)
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) {
                armax->phi[k][i][j] = 0.0;
                for (int k1 = 1; k1 <= m; k1++)
                    armax->phi[k][i][j] += mtmp1[i][k1] * phi1[k][k1][j];
            }

    real **mtmp3 = matrix(1, m, 1, m);
    real **mtmp4 = matrix(1, m, 1, m);

    /* mtmp3 = theta1[0]^{-1} * phi1[0] */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) {
            mtmp3[i][j] = 0.0;
            for (int k1 = 1; k1 <= m; k1++)
                mtmp3[i][j] += mtmp2[i][k1] * phi1[0][k1][j];
        }

    /* MA normalizado */
    for (k = 1; k <= q; k++) {
        /* mtmp4 = phi1[0]^{-1} * theta1[k] */
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) {
                mtmp4[i][j] = 0.0;
                for (int k1 = 1; k1 <= m; k1++)
                    mtmp4[i][j] += mtmp1[i][k1] * theta1[k][k1][j];
            }
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) {
                armax->theta[k][i][j] = 0.0;
                for (int k1 = 1; k1 <= m; k1++)
                    armax->theta[k][i][j] += mtmp4[i][k1] * mtmp3[k1][j];
            }
    }

    /* mtmp3 = phi1[0]^{-1} * theta1[0] */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) {
            mtmp3[i][j] = 0.0;
            for (int k1 = 1; k1 <= m; k1++)
                mtmp3[i][j] += mtmp1[i][k1] * theta1[0][k1][j];
        }

    /* Hacer simétrica qq1 (por si acaso) */
    for (i = 1; i <= m; i++)
        for (j = i+1; j <= m; j++)
            qq1[i][j] = qq1[j][i];

    /* mtmp4 = phi1[0]^{-1} * theta1[0] * qq1 */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) {
            mtmp4[i][j] = 0.0;
            for (int k1 = 1; k1 <= m; k1++)
                mtmp4[i][j] += mtmp3[i][k1] * qq1[k1][j];
        }

    /* Covarianza normalizada: armax->qq = mtmp4 * mtmp3' */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) {
            armax->qq[i][j] = 0.0;
            for (int k1 = 1; k1 <= m; k1++)
                armax->qq[i][j] += mtmp4[i][k1] * mtmp3[j][k1];
        }

    /* Media */
    for (i = 1; i <= m; i++)
        armax->mu[i] = mu1[i];

    /* Liberar memoria temporal */
    free_matrix(mtmp4, 1, m, 1, m);
    free_matrix(mtmp3, 1, m, 1, m);
    free_matrix(mtmp2, 1, m, 1, m);
    free_matrix(mtmp1, 1, m, 1, m);
    free_matrix(qq1, 1, m, 1, m);
    free_tensor(theta1, 0, q, 1, m, 1, m);
    free_tensor(phi1, 0, p, 1, m, 1, m);
    free_vector(mu1, 1, m);

    /* [5] Datos: copiar datamat a armax->w */
    for (i = 1; i <= nobs; i++)
        for (j = 1; j <= m; j++)
            armax->w[i][j] = datamat[i][j];

    /* [6] Liberar memoria del modelo si lastx está activo */
    if (lastx) {
        free_matrix(armax->a, 1, armax->n, 1, armax->m);
        free_matrix(armax->w, 1, armax->n, 1, armax->m);
        free_matrix(armax->qq, 1, armax->m, 1, armax->m);
        free_tensor(armax->theta, 0, armax->q, 1, armax->m, 1, armax->m);
        free_tensor(armax->phi, 0, armax->p, 1, armax->m, 1, armax->m);
        free_vector(armax->mu, 1, armax->m);
    }

}

/*****************************************************************************/
/*  calc_nparametrs: calculates number of parameters according to options   */
/*  (unchanged)                                                              */
/*****************************************************************************/
static int calc_nparametrs(void)
{
    int npar = 0;
    int m = nser;

    if (global_include_mean)
        npar += m;

    int n_ar = global_diag_ar ? m : m * m;
    npar += n_ar * global_p;

    int n_ma = global_diag_ma ? m : m * m;
    npar += n_ma * global_q;

    int n_cov = global_diag_cov ? m : m * (m + 1) / 2;
    npar += n_cov;

    return npar;
}

/*****************************************************************************/
/*  init_varma: initial values for varma (original)                         */
/*  (unchanged)                                                              */
/*****************************************************************************/
static void init_varma(real *x, int npar)
{
     int m = nser;
    int i, j, k, t, idx = 1;
    real *mean_est = NULL;

    /* 1. Medias muestrales (si se incluyen) */
    if (global_include_mean) {
        mean_est = vector(1, m);
        for (j = 1; j <= m; j++) {
            real sum = 0.0;
            for (t = 1; t <= nobs; t++) sum += datamat[t][j];
            mean_est[j] = sum / nobs;
            x[idx++] = mean_est[j];
        }
    }

    /* Centrar datos */
    real **datac = matrix(1, nobs, 1, m);
    for (t = 1; t <= nobs; t++)
        for (j = 1; j <= m; j++)
            datac[t][j] = datamat[t][j] - (global_include_mean ? mean_est[j] : 0.0);

    /* Matrices de covarianza muestral (necesarias para AR y MA) */
    real **gamma0 = matrix(1, m, 1, m);
    real **gamma1 = matrix(1, m, 1, m);   // para lag 1
    for (i = 1; i <= m; i++) {
        for (j = 1; j <= m; j++) {
            real sum0 = 0.0, sum1 = 0.0;
            for (t = 1; t <= nobs; t++) {
                sum0 += datac[t][i] * datac[t][j];
                if (t > 1) sum1 += datac[t][i] * datac[t-1][j];
            }
            gamma0[i][j] = sum0 / nobs;
            gamma1[i][j] = sum1 / (nobs - 1);
        }
    }

    /* Calcular factor de escala para que las varianzas estén cerca de 1 */

    real scale_factor = 0.0;
        for (i = 1; i <= m; i++) scale_factor += gamma0[i][i];
        scale_factor /= m;
    if (scale_factor < 1e-12) scale_factor = 1.0;  /* evitar división por cero */

    /* Escalar matrices de covarianza */
    for (i = 1; i <= m; i++) {
    for (j = 1; j <= m; j++) {
        gamma0[i][j] /= scale_factor;
        gamma1[i][j] /= scale_factor;
        }
    }

    /* 2. Parámetros AR (si p>0) */
    if (global_p > 0) {
        int T = nobs - global_p;
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
            for (j = 1; j <= m; j++)
                y[row][j] = datac[t][j];
        }

        /* Z'Z */
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

        /* Coeficientes por ecuación */
        real **coef = matrix(1, m, 1, m * global_p);
        for (int eq = 1; eq <= m; eq++) {
            real *Zty = vector(1, m * global_p);
            for (i = 1; i <= m * global_p; i++) {
                Zty[i] = 0.0;
                for (t = 1; t <= T; t++)
                    Zty[i] += Z[t][i] * y[t][eq];
            }
            lusol(ZtZ, Zty, m * global_p, indx);
            for (i = 1; i <= m * global_p; i++)
                coef[eq][i] = Zty[i];
            free_vector(Zty, 1, m * global_p);
        }

        /* Guardar coeficientes AR en x */
        for (k = 1; k <= global_p; k++) {
            for (i = 1; i <= m; i++) {
                for (j = 1; j <= m; j++) {
                    if (global_diag_ar && i != j) {
                        /* no se guarda */
                    } else {
                        int pos = (k - 1) * m * m + (i - 1) * m + j; /* 1?based */
                        int coef_idx = (k - 1) * m + j;
                        x[idx + pos - 1] = coef[i][coef_idx];
                    }
                }
            }
        }
        idx += (global_diag_ar ? m : m * m) * global_p;

        free_matrix(Z, 1, T, 1, m * global_p);
        free_matrix(y, 1, T, 1, m);
        free_matrix(ZtZ, 1, m * global_p, 1, m * global_p);
        free_ivector(indx, 1, m * global_p);
        free_matrix(coef, 1, m, 1, m * global_p);
    }

    /* 3. Parámetros MA (si q>0) */
    if (global_q > 0) {
        int n_ma_est = (global_diag_ma ? m : m * m) * global_q;

        if (global_p == 0) {
            /* Modelo MA puro: inicializar usando autocorrelación simple */
            for (k = 1; k <= global_q; k++) {
                for (i = 1; i <= m; i++) {
                    for (j = 1; j <= m; j++) {
                        if (global_diag_ma && i != j) {
                            /* no se guarda */
                        } else {
                            if (k == 1 && i == j) {
                                /* MA(1) diagonal: aproximación de momentos */
                                real var = gamma0[i][i];
                                real cov1 = gamma1[i][i];
                                real r1 = cov1 / var;
                                real theta = -r1;   // aproximación lineal
                                if (fabs(theta) >= 1.0)
                                    theta = (theta > 0) ? 0.99 : -0.99;
                                x[idx++] = theta;
                            } else {
                                x[idx++] = 0.0;
                            }
                        }
                    }
                }
            }
        } else {
            /* Modelo con AR: inicializar MA a cero */
            for (i = 0; i < n_ma_est; i++)
                x[idx++] = 0.0;
        }
    }

    /* 4. Parámetros de covarianza Q */
    int n_cov_est = global_diag_cov ? m : m * (m + 1) / 2;
    if (global_diag_cov) {
        for (i = 1; i <= m; i++)
            x[idx++] = gamma0[i][i];
    } else {
        for (i = 1; i <= m; i++)
            for (j = 1; j <= i; j++)
                x[idx++] = gamma0[i][j];
    }

    /* Liberar memoria */
    free_matrix(datac, 1, nobs, 1, m);
    free_matrix(gamma0, 1, m, 1, m);
    free_matrix(gamma1, 1, m, 1, m);
    if (global_include_mean) free_vector(mean_est, 1, m);
}

/*****************************************************************************/
/*  init_diag_varma: initial values for diagonal model                      */
/*  Estimates univariate AR for each series and sets MA to zero.            */
/*  Now includes scaling of residual variances to have average 1.           */
/*****************************************************************************/
static void init_diag_varma(real *x, int npar)
{
    int m = nser;
    int p = global_p;
    int q = global_q;
    int i, j, k, t, idx = 1;
    real *mean_est = NULL;

    /* Sample means (if included) */
    if (global_include_mean) {
        mean_est = vector(1, m);
        for (j = 1; j <= m; j++) {
            real sum = 0.0;
            for (t = 1; t <= nobs; t++) sum += datamat[t][j];
            mean_est[j] = sum / nobs;
            x[idx++] = mean_est[j];
        }
    }

    /* Center data */
    real **datac = matrix(1, nobs, 1, m);
    for (t = 1; t <= nobs; t++)
        for (j = 1; j <= m; j++)
            datac[t][j] = datamat[t][j] - (global_include_mean ? mean_est[j] : 0.0);

    /* Arrays for diagonal AR coefficients and residuals */
    real **phi_diag = matrix(1, p, 1, m);  /* phi_diag[k][i] */
    real **resid = matrix(1, nobs, 1, m);

    for (i = 1; i <= m; i++) {
        int T = nobs - p;
        if (T <= 0) {
            for (k = 1; k <= p; k++) phi_diag[k][i] = 0.0;
            for (t = 1; t <= nobs; t++) resid[t][i] = datac[t][i];
            continue;
        }
        /* Build regressor matrix Z (p lags) and vector y */
        real **Z = matrix(1, T, 1, p);
        real *y = vector(1, T);
        for (t = p+1; t <= nobs; t++) {
            int row = t - p;
            for (k = 1; k <= p; k++)
                Z[row][k] = datac[t - k][i];
            y[row] = datac[t][i];
        }

        /* Z'Z */
        real **ZtZ = matrix(1, p, 1, p);
        for (k = 1; k <= p; k++) {
            for (j = 1; j <= p; j++) {
                real sum = 0.0;
                for (t = 1; t <= T; t++) sum += Z[t][k] * Z[t][j];
                ZtZ[k][j] = sum;
            }
        }
        int *indx = ivector(1, p);
        ludcp(ZtZ, p, indx);
        real *Zty = vector(1, p);
        for (k = 1; k <= p; k++) {
            Zty[k] = 0.0;
            for (t = 1; t <= T; t++) Zty[k] += Z[t][k] * y[t];
        }
        lusol(ZtZ, Zty, p, indx);

        for (k = 1; k <= p; k++) phi_diag[k][i] = Zty[k];

        /* Compute residuals for all observations */
        for (t = 1; t <= nobs; t++) {
            if (t <= p) {
                resid[t][i] = datac[t][i];
            } else {
                real pred = 0.0;
                for (k = 1; k <= p; k++) pred += phi_diag[k][i] * datac[t - k][i];
                resid[t][i] = datac[t][i] - pred;
            }
        }

        free_matrix(Z, 1, T, 1, p);
        free_vector(y, 1, T);
        free_matrix(ZtZ, 1, p, 1, p);
        free_ivector(indx, 1, p);
        free_vector(Zty, 1, p);
    }

    /* Store AR coefficients */
    for (k = 1; k <= p; k++)
        for (i = 1; i <= m; i++)
            x[idx++] = phi_diag[k][i];

    /* MA parameters (all zero) */
    for (k = 1; k <= q; k++)
        for (i = 1; i <= m; i++)
            x[idx++] = 0.0;

    /* --- Escalar varianzas residuales para que el promedio sea 1 --- */
    real *var_resid = vector(1, m);
    for (i = 1; i <= m; i++) {
        real sum = 0.0;
        for (t = 1; t <= nobs; t++) sum += resid[t][i] * resid[t][i];
        var_resid[i] = sum / nobs;
    }
    real scale_factor = 0.0;
    for (i = 1; i <= m; i++) scale_factor += var_resid[i];
    scale_factor /= m;
    if (scale_factor < 1e-12) scale_factor = 1.0;
    for (i = 1; i <= m; i++) var_resid[i] /= scale_factor;
    for (i = 1; i <= m; i++) x[idx++] = var_resid[i];
    free_vector(var_resid, 1, m);

    /* Free memory */
    free_matrix(phi_diag, 1, p, 1, m);
    free_matrix(resid, 1, nobs, 1, m);
    free_matrix(datac, 1, nobs, 1, m);
    if (global_include_mean) free_vector(mean_est, 1, m);
}

/*****************************************************************************/
/*  combine_vectors: combina el vector diagonal refinado con el original    */
/*  Los elementos diagonales de AR, MA y covarianza se toman de x_diag;     */
/*  los no diagonales se conservan de x_full (original).                    */
/*****************************************************************************/
static void combine_vectors(real *x_full, real *x_diag, int npar_full, int npar_diag)
{
    int m = nser;
    int p = global_p;
    int q = global_q;
    int idx_full = 1, idx_diag = 1;

    /* Medias */
    if (global_include_mean) {
        for (int i = 1; i <= m; i++)
            x_full[idx_full++] = x_diag[idx_diag++];
    }

    /* AR */
    for (int k = 1; k <= p; k++) {
        if (global_diag_ar) {
            for (int i = 1; i <= m; i++)
                x_full[idx_full++] = x_diag[idx_diag++];
        } else {
            for (int i = 1; i <= m; i++) {
                for (int j = 1; j <= m; j++) {
                    if (i == j)
                        x_full[idx_full] = x_diag[idx_diag++];
                    /* else conservar el valor original (ya en x_full) */
                    idx_full++;
                }
            }
        }
    }

    /* MA */
    for (int k = 1; k <= q; k++) {
        if (global_diag_ma) {
            for (int i = 1; i <= m; i++)
                x_full[idx_full++] = x_diag[idx_diag++];
        } else {
            for (int i = 1; i <= m; i++) {
                for (int j = 1; j <= m; j++) {
                    if (i == j)
                        x_full[idx_full] = x_diag[idx_diag++];
                    /* else conservar el valor original (cero en init_varma) */
                    idx_full++;
                }
            }
        }
    }

    /* Covarianza */
    if (global_diag_cov) {
        for (int i = 1; i <= m; i++)
            x_full[idx_full++] = x_diag[idx_diag++];
    } else {
        for (int i = 1; i <= m; i++) {
            for (int j = 1; j <= i; j++) {
                if (i == j)
                    x_full[idx_full] = x_diag[idx_diag++];
                /* else conservar la covarianza original (ya reescalada) */
                idx_full++;
            }
        }
    }

    /* Verificación de consistencia */
    if (idx_full - 1 != npar_full || idx_diag - 1 != npar_diag) {
        printf("Error in vector combination: inconsistent indices (full:%d vs %d, diag:%d vs %d).\n",
               idx_full-1, npar_full, idx_diag-1, npar_diag);
        exit(1);
    }
}

/*****************************************************************************/
/*  Printing functions (unchanged, but comments translated)                  */
/*****************************************************************************/
static void print_parameters(real *x, real *dev, real **cov, int npar, struct Tvarma *varma)
{
    /* ... original code ... */
    int i, j, k, idx = 1;
    int m = varma->m;
    fprintf(outputv, "\nParámetros estimados y desviaciones típicas:\n");

    if (global_include_mean) {
        fprintf(outputv, "Medias:\n");
        for (i = 1; i <= m; i++) {
            fprintf(outputv, "  mu[%d] = %12.6f (%12.6f)\n", i, x[idx], dev[idx]);
            idx++;
        }
    }

    if (global_p > 0) {
        fprintf(outputv, "\nMatrices AR:\n");
        for (k = 1; k <= global_p; k++) {
            fprintf(outputv, "  phi(%d):\n", k);
            for (i = 1; i <= m; i++) {
                for (j = 1; j <= m; j++) {
                    if (!global_diag_ar || i == j) {
                        fprintf(outputv, "    phi[%d][%d][%d] = %12.6f (%12.6f)\n", k, i, j, x[idx], dev[idx]);
                        idx++;
                    }
                }
            }
        }
    }

    if (global_q > 0) {
        fprintf(outputv, "\nMatrices MA:\n");
        for (k = 1; k <= global_q; k++) {
            fprintf(outputv, "  theta(%d):\n", k);
            for (i = 1; i <= m; i++) {
                for (j = 1; j <= m; j++) {
                    if (!global_diag_ma || i == j) {
                        fprintf(outputv, "    theta[%d][%d][%d] = %12.6f (%12.6f)\n", k, i, j, x[idx], dev[idx]);
                        idx++;
                    }
                }
            }
        }
    }

    fprintf(outputv, "\nMatriz de covarianzas Q (sin escala sigma^2):\n");
    if (global_diag_cov) {
        for (i = 1; i <= m; i++) {
            fprintf(outputv, "  Q[%d][%d] = %12.6f (%12.6f)\n", i, i, x[idx], dev[idx]);
            idx++;
        }
    } else {
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= i; j++) {
                fprintf(outputv, "  Q[%d][%d] = %12.6f (%12.6f)\n", i, j, x[idx], dev[idx]);
                idx++;
            }
        }
    }
    fprintf(outputv, "\n");

    /* Matriz de covarianzas de los estimadores (triangular inferior) */
    fprintf(outputv, "Matriz de covarianzas de los estimadores:\n");
    for (i = 1; i <= npar; i++) {
        for (j = 1; j <= i; j++)
            fprintf(outputv, "%12.6f", cov[i][j]);
        fprintf(outputv, "\n");
    }
    fprintf(outputv, "\n");

    fprintf(outputv, "sigma2 = %12.6f\n", varma->sigma2);
    fprintf(outputv, "log-verosimilitud = %12.6f\n", varma->logelf);
}

static void print_matrices(struct Tvarma *varma)
{
    /* ... original code ... */
    int i, j, k;
    int m = varma->m;
    fprintf(outputv, "\nModelo normalizado:\n");
    fprintf(outputv, "Vector mu:\n");
    for (i = 1; i <= m; i++)
        fprintf(outputv, "  %12.6f\n", varma->mu[i]);

    for (k = 1; k <= varma->p; k++) {
        fprintf(outputv, "Matriz phi(%d):\n", k);
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++)
                fprintf(outputv, "  %12.6f", varma->phi[k][i][j]);
            fprintf(outputv, "\n");
        }
    }
    for (k = 1; k <= varma->q; k++) {
        fprintf(outputv, "Matriz theta(%d):\n", k);
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++)
                fprintf(outputv, "  %12.6f", varma->theta[k][i][j]);
            fprintf(outputv, "\n");
        }
    }
    fprintf(outputv, "Matriz Q:\n");
    for (i = 1; i <= m; i++) {
        for (j = 1; j <= i; j++)
            fprintf(outputv, "  %12.6f", varma->qq[i][j]);
        fprintf(outputv, "\n");
    }
    fprintf(outputv, "Matriz Sigma = sigma2 * Q:\n");
    for (i = 1; i <= m; i++) {
        for (j = 1; j <= i; j++)
            fprintf(outputv, "  %12.6f", varma->sigma2 * varma->qq[i][j]);
        fprintf(outputv, "\n");
    }
    fprintf(outputv, "\n");
}

static void print_roots(struct Tvarma *varma)
{
    /* ... original code ... */
    int i;
    real *wr, *wi, *wmod;
    int ifault = 0;

    if (varma->p > 0) {
        wr = vector(1, varma->m * varma->p);
        wi = vector(1, varma->m * varma->p);
        wmod = vector(1, varma->m * varma->p);
        chekma(varma->m, varma->p, varma->phi, wr, wi, wmod, &ifault);
        fprintf(outputv, "Raíces inversas de |phi(B)|=0:\n");
        for (i = 1; i <= varma->m * varma->p; i++)
            if (fabs(wmod[i]) >= 0.00005)
                fprintf(outputv, "  %12.6f  %12.6f i  (módulo %12.6f)\n", wr[i], wi[i], wmod[i]);
        free_vector(wmod, 1, varma->m * varma->p);
        free_vector(wi, 1, varma->m * varma->p);
        free_vector(wr, 1, varma->m * varma->p);
    }

    if (varma->q > 0) {
        wr = vector(1, varma->m * varma->q);
        wi = vector(1, varma->m * varma->q);
        wmod = vector(1, varma->m * varma->q);
        chekma(varma->m, varma->q, varma->theta, wr, wi, wmod, &ifault);
        fprintf(outputv, "Raíces inversas de |theta(B)|=0:\n");
        for (i = 1; i <= varma->m * varma->q; i++)
            if (fabs(wmod[i]) >= 0.00005)
                fprintf(outputv, "  %12.6f  %12.6f i  (módulo %12.6f)\n", wr[i], wi[i], wmod[i]);
        free_vector(wmod, 1, varma->m * varma->q);
        free_vector(wi, 1, varma->m * varma->q);
        free_vector(wr, 1, varma->m * varma->q);
    }
    fprintf(outputv, "\n");
}
