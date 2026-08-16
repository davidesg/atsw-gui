/*****************************************************************************/
/*  drvec.c — Exact ML estimation of VEC models (Mauricio 2006).              */
/*                                                                             */
/*  Implements the transformation described in:                               */
/*    Mauricio, J.A. (2006), "Exact maximum likelihood estimation of          */
/*    partially nonstationary vector ARMA models",                           */
/*    Computational Statistics & Data Analysis, 50, 3644-3662.               */
/*                                                                             */
/*  The stationary representation Ȳ_t = (∇Y_{2t}', W_t')' follows a          */
/*  standard VARMA(p,q), estimated by Mauricio's EML engine.                 */
/*  The VEC parameters are recovered after estimation.                        */
/*                                                                             */
/*  Usage:  drvec file p q r [-mean] [-case 1|2|3] [-diagar] [-diagma]       */
/*                          [-diagcov] [-m 1|2] [-lrtest]                     */
/*                                                                             */
/*    file   : data file name (without .inp extension)                        */
/*    p      : AR order of the stationary VARMA on Ȳ_t                        */
/*    q      : MA order                                                       */
/*    r      : cointegration rank (0 < r < M, or 0 for -lrtest)               */
/*                                                                             */
/*  Copyright (C) 1995-2026  J.A. Mauricio, A.B. Treadway & D.E. Guerrero.   */
/*  GPL v2 or later.                                                           */
/*****************************************************************************/

#include "main.h"

real macheps;
FILE *outputv;
int quiet_mode = 0;

real **datamat;     /* stationary data for estimation: ∇Y_{2t} (cols 1..s)
                       and Y_{1t} in levels (cols s+1..M) */
int  nser, nobs;

/* --- .inp metadata -------------------------------------------------------- */
int   data_freq      = 1;
int   data_start_sub = 1;
int   data_start_year = 1;
char **series_names  = NULL;

/* --- transformation spec -------------------------------------------------- */
real trans_lambda = 1.0;
real trans_scale  = 1.0;
int  trans_d = 0, trans_D = 0;

/* --- model configuration -------------------------------------------------- */
int global_p, global_q, global_r;    /* r = cointegration rank */
int global_include_mean = 0;
int global_diag_ar = 0;
int global_diag_ma = 0;
int global_diag_cov = 0;
int met = 1;          /* 1 = exact, 2 = approximate */
int global_case = 1;  /* deterministic case (Mauricio Remark 6) */
int global_lrtest = 0; /* if 1, perform sequential LR test for rank */

/* --- Mauricio transformation matrices (Mauricio 2006, eq. 11-13) --------- */
/*   Cbar = [ 0_{s x r}    I_s      ]                                      */
/*         [    I_r       B2'       ]                                      */
/*   Hbar = [ 0_{s x s}   0_{s x r} ]                                      */
/*         [ 0_{r x s}      I_r     ]                                      */
/* where s = M - r                                                         */

/* --- Local prototypes ----------------------------------------------------- */
static void vec_shootx(real *x, struct Tvarma *armax,
                       int *ifaultx, int firstx, int lastx);
static int  calc_nparametrs(void);
static void init_guess(real *x, int npar);

/*****************************************************************************/
/*  calc_nparametrs — number of free parameters in the VEC model             */
/*                                                                           */
/*  Parameter vector x[] layout (Mauricio 2006, Remark 1 and 6):            */
/*    1. Mean E[Ȳ_t] (Remark 6):                                            */
/*         case 1: none                                                      */
/*         case 2: E[W_j], j=1..r                          (r params)        */
/*         case 3: E[∇Y₂_i], i=1..s; E[W_j], j=1..r        (M params)        */
/*    2. Λ  (M×r) adjustment matrix                     (M·r params)        */
/*    3. F_i, i=1..p-1, each M×M                     ((p-1)·M² params)      */
/*    4. Θ_i, i=1..q, each M×M                         (q·M² params)         */
/*    5. Σ lower triangle                            (M(M+1)/2 params)       */
/*    6. B₂ (s×r) cointegration matrix                 (s·r params)          */
/*****************************************************************************/
static int calc_nparametrs(void)
{
    int M = nser, r = global_r, s = M - r;
    int p = global_p, q = global_q;
    int npar = 0;

    /* 1. Mean E[Ȳ_t] */
    if      (global_case == 2) npar += r;
    else if (global_case == 3) npar += M;

    /* 2. Adjustment matrix Lambda (M x r) */
    npar += M * r;

    /* 3. F_i (M x M, i=1..p-1) */
    int nf = (p > 1) ? p - 1 : 0;
    npar += nf * (global_diag_ar ? M : M * M);

    /* 4. Theta_j (M x M, j=1..q) */
    npar += q * (global_diag_ma ? M : M * M);

    /* 5. Sigma (lower triangle) */
    npar += global_diag_cov ? M : M * (M + 1) / 2;

    /* 6. B_2 (s x r) */
    npar += s * r;

    return npar;
}

/*****************************************************************************/
/*  init_guess — conditional (Johansen-style) initial parameter values       */
/*                                                                           */
/*  Runs the concentrated regression  ∇Y_t = Σ F_i ∇Y_{t-i} − Λ W_{t-1} + e, */
/*  with W_t = Y_{1t} + B₂'Y_{2t} built from a static OLS B₂, to initialise   */
/*  Λ and F_i directly (this is the conditional estimator the paper mentions  */
/*  as the natural starting point, Remark 1.1).                              */
/*****************************************************************************/
static void init_guess(real *x, int npar)
{
    int M = nser, r = global_r, s = M - r;
    int p = global_p, q = global_q;
    int i, j, k, t, idx = 1;
    int nf = (p > 1) ? p - 1 : 0;

    /* --- 0. Reconstruct Y_{2t} levels by cumulating ∇Y_{2t} ------------ */
    real **Y2lev = matrix(1, nobs, 1, s);
    for (i = 1; i <= s; i++) Y2lev[1][i] = 0.0;
    for (t = 2; t <= nobs; t++)
        for (i = 1; i <= s; i++)
            Y2lev[t][i] = Y2lev[t-1][i] + datamat[t][i];

    /* --- 1. Initial B₂ via static OLS with intercept ------------------- */
    real **B2 = matrix(1, s, 1, r);
    for (j = 1; j <= r; j++) {
        int nc = s + 1;
        real *yy = vector(1, nobs);
        for (t = 1; t <= nobs; t++) yy[t] = datamat[t][s + j];
        real **XX = matrix(1, nc, 1, nc);
        real  *Xy = vector(1, nc);
        for (i = 1; i <= nc; i++) {
            for (int ii = 1; ii <= nc; ii++) {
                real ss = 0.0;
                for (t = 1; t <= nobs; t++) {
                    real xi  = (i  == 1) ? 1.0 : Y2lev[t][i-1];
                    real xii = (ii == 1) ? 1.0 : Y2lev[t][ii-1];
                    ss += xi * xii;
                }
                XX[i][ii] = ss;
            }
            Xy[i] = 0.0;
            for (t = 1; t <= nobs; t++) {
                real xi = (i == 1) ? 1.0 : Y2lev[t][i-1];
                Xy[i] += xi * yy[t];
            }
        }
        int *ind = ivector(1, nc);
        ludcp(XX, nc, ind);
        lusol(XX, Xy, nc, ind);
        for (i = 1; i <= s; i++) B2[i][j] = -Xy[i+1];
        free_ivector(ind, 1, nc);
        free_vector(yy, 1, nobs);
        free_matrix(XX, 1, nc, 1, nc);
        free_vector(Xy, 1, nc);
    }

    /* --- 2. Build W_t = Y_{1t} + B₂'Y_{2t} and ∇Y_t = [∇Y_{1t};∇Y_{2t}] - */
    real **W  = matrix(1, nobs, 1, r);
    real **dY = matrix(1, nobs, 1, M);   /* rows: [∇Y₁ (r); ∇Y₂ (s)] */
    for (t = 1; t <= nobs; t++) {
        for (j = 1; j <= r; j++) {
            real w = datamat[t][s + j];
            for (i = 1; i <= s; i++) w += B2[i][j] * Y2lev[t][i];
            W[t][j] = w;
        }
        for (j = 1; j <= r; j++)
            dY[t][j] = (t > 1) ? datamat[t][s+j] - datamat[t-1][s+j] : 0.0;
        for (i = 1; i <= s; i++)
            dY[t][r + i] = datamat[t][i];
    }

    /* --- 3. Mean E[Ȳ_t] (Remark 6) --------------------------------------- */
    real *EW   = vector(1, r);   /* sample mean of W */
    real *EdY2 = vector(1, s);   /* sample mean of ∇Y₂ */
    for (j = 1; j <= r; j++) { real sm=0.0; for (t=1;t<=nobs;t++) sm+=W[t][j]; EW[j]=sm/nobs; }
    for (i = 1; i <= s; i++) { real sm=0.0; for (t=1;t<=nobs;t++) sm+=dY[t][r+i]; EdY2[i]=sm/nobs; }

    /* --- 4. Conditional regression: ∇Y_t = Σ F_i ∇Y_{t-i} − Λ (W_{t−1}−E[W]) */
    int nreg = r + nf * M;
    int T = nobs - p;              /* rows t = p+1 .. nobs */
    if (T < 1) T = 1;
    real **X = matrix(1, T, 1, nreg);
    real **Ydep = matrix(1, T, 1, M);
    for (t = p + 1; t <= nobs; t++) {
        int row = t - p, col = 1;
        for (j = 1; j <= r; j++) X[row][col++] = W[t-1][j] - EW[j];
        for (k = 1; k <= nf; k++)
            for (i = 1; i <= M; i++)
                X[row][col++] = dY[t-k][i];
        for (i = 1; i <= M; i++) Ydep[row][i] = dY[t][i];
    }
    real **XtX = matrix(1, nreg, 1, nreg);
    real  *Xty = vector(1, nreg);
    for (i = 1; i <= nreg; i++)
        for (j = 1; j <= nreg; j++) {
            real ss = 0.0;
            for (t = 1; t <= T; t++) ss += X[t][i]*X[t][j];
            XtX[i][j] = ss;
        }
    int *indx = ivector(1, nreg);
    ludcp(XtX, nreg, indx);

    real **Lambda = matrix(1, M, 1, r);
    real ***F = tensor(1, (nf > 0 ? nf : 1), 1, M, 1, M);
    for (int eq = 1; eq <= M; eq++) {
        for (i = 1; i <= nreg; i++) {
            Xty[i] = 0.0;
            for (t = 1; t <= T; t++) Xty[i] += X[t][i]*Ydep[t][eq];
        }
        lusol(XtX, Xty, nreg, indx);
        for (j = 1; j <= r; j++) Lambda[eq][j] = -Xty[j];          /* −Λ */
        for (k = 1; k <= nf; k++)
            for (i = 1; i <= M; i++)
                F[k][eq][i] = Xty[r + (k-1)*M + i];                /* F_k */
    }
    /* Residual covariance Σ from the conditional regression */
    real **Sig = matrix(1, M, 1, M);
    for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) {
        real ss = 0.0; int cnt = 0;
        for (t = 1; t <= T; t++) {
            real ei = Ydep[t][i], ej = Ydep[t][j];
            for (int c = 1; c <= nreg; c++) {
                real bi = (c <= r) ? -Lambda[i][c] : F[(c-r-1)/M+1][i][(c-r-1)%M+1];
                real bj = (c <= r) ? -Lambda[j][c] : F[(c-r-1)/M+1][j][(c-r-1)%M+1];
                ei -= bi * X[t][c];
                ej -= bj * X[t][c];
            }
            ss += ei*ej; cnt++;
        }
        Sig[i][j] = (cnt > 0) ? ss/cnt : 1.0;
    }

    /* --- 5. Write x[] in the canonical VEC order ------------------------- */
    if (global_case == 2) { for (j = 1; j <= r; j++) x[idx++] = EW[j]; }
    else if (global_case == 3) {
        for (i = 1; i <= s; i++) x[idx++] = EdY2[i];
        for (j = 1; j <= r; j++) x[idx++] = EW[j];
    }
    for (i = 1; i <= M; i++) for (j = 1; j <= r; j++) x[idx++] = Lambda[i][j];
    for (k = 1; k <= nf; k++) {
        if (global_diag_ar) { for (i = 1; i <= M; i++) x[idx++] = F[k][i][i]; }
        else { for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) x[idx++] = F[k][i][j]; }
    }
    for (k = 1; k <= q; k++) {
        if (global_diag_ma) { for (i = 1; i <= M; i++) x[idx++] = 0.0; }
        else { for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) x[idx++] = 0.0; }
    }
    for (i = 1; i <= M; i++) {
        if (global_diag_cov) { x[idx++] = Sig[i][i]; }
        else { for (j = 1; j <= i; j++) x[idx++] = Sig[i][j]; }
    }
    for (j = 1; j <= r; j++) for (i = 1; i <= s; i++) x[idx++] = B2[i][j];

    if (idx != npar + 1)
        fprintf(stderr, "ERROR init_guess: idx=%d, npar=%d\n", idx-1, npar);

    free_matrix(Sig, 1, M, 1, M);
    free_tensor(F, 1, (nf > 0 ? nf : 1), 1, M, 1, M);
    free_matrix(Lambda, 1, M, 1, r);
    free_ivector(indx, 1, nreg);
    free_matrix(XtX, 1, nreg, 1, nreg);
    free_vector(Xty, 1, nreg);
    free_matrix(X, 1, T, 1, nreg);
    free_matrix(Ydep, 1, T, 1, M);
    free_vector(EdY2, 1, s);
    free_vector(EW, 1, r);
    free_matrix(dY, 1, nobs, 1, M);
    free_matrix(W, 1, nobs, 1, r);
    free_matrix(B2, 1, s, 1, r);
    free_matrix(Y2lev, 1, nobs, 1, s);
}

/*****************************************************************************/
/*  vec_shootx — Mauricio (2006) transformation: x[] → Tvarma + Ȳ_t         */
/*                                                                             */
/*  This is the "user function" (Remark 4 of Mauricio 2006).  On every        */
/*  call it:                                                                   */
/*    1. Unpacks parameters from x[]                                           */
/*    2. Builds Ȳ_t = (∇Y_{2t}', W_t')' using current B₂                    */
/*    3. Places the VARMA parameters into the Tvarma structure                */
/*    4. Normalises (Mauricio's standard normalisation)                        */
/*  The estimator then computes the exact log-likelihood of Ȳ_t.              */
/*****************************************************************************/
static void vec_shootx(real *x, struct Tvarma *armax,
                       int *ifaultx, int firstx, int lastx)
{
    int M = nser, r = global_r, s = M - r;
    int p = global_p, q = global_q;
    int i, j, k, idx = 1;

    *ifaultx = 0;

    /* [1] Set dimensions --------------------------------------------------- */
    armax->m = M;
    armax->n = nobs;
    armax->p = p;
    armax->q = q;

    /* [2] Allocate memory on first call ------------------------------------ */
    if (firstx) {
        armax->mu    = vector(1, M);
        armax->phi   = tensor(0, p, 1, M, 1, M);
        armax->theta = tensor(0, q, 1, M, 1, M);
        armax->qq    = matrix(1, M, 1, M);
        armax->w     = matrix(1, nobs, 1, M);
        armax->a     = matrix(1, nobs, 1, M);

        for (i = 1; i <= M; i++) {
            armax->mu[i] = 0.0;
            for (j = 1; j <= M; j++) {
                for (k = 0; k <= p; k++) armax->phi[k][i][j] = 0.0;
                for (k = 0; k <= q; k++) armax->theta[k][i][j] = 0.0;
                armax->qq[i][j] = 0.0;
            }
            for (j = 1; j <= nobs; j++) {
                armax->w[j][i] = 0.0;
                armax->a[j][i] = 0.0;
            }
        }
        for (i = 1; i <= M; i++) {
            armax->phi[0][i][i]   = 1.0;
            armax->theta[0][i][i] = 1.0;
        }
    }

    /* [3] Unpack VEC parameters in the canonical order ---------------------- */
    /*   1. Mean E[Ȳ_t] (Remark 6): Ȳ_t = [∇Y₂; W], so μ = [E[∇Y₂]; E[W]]  */
    real *mu = vector(1, M);
    for (i = 1; i <= M; i++) mu[i] = 0.0;
    if (global_case == 2) {
        for (j = 1; j <= r; j++) mu[s + j] = x[idx++];
    } else if (global_case == 3) {
        for (i = 1; i <= s; i++) mu[i] = x[idx++];
        for (j = 1; j <= r; j++) mu[s + j] = x[idx++];
    }

    /*   2. Adjustment matrix Lambda (M x r) */
    real **Lambda = matrix(1, M, 1, r);
    for (i = 1; i <= M; i++)
        for (j = 1; j <= r; j++)
            Lambda[i][j] = x[idx++];

    /*   3. F_i (M x M, i=1..p-1) */
    int nf = (p > 1) ? p - 1 : 0;
    real ***F = tensor(1, (nf > 0 ? nf : 1), 1, M, 1, M);
    for (k = 1; k <= (nf > 0 ? nf : 1); k++)
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) F[k][i][j] = 0.0;
    for (k = 1; k <= nf; k++) {
        if (global_diag_ar) {
            for (i = 1; i <= M; i++) F[k][i][i] = x[idx++];
        } else {
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++)
                    F[k][i][j] = x[idx++];
        }
    }

    /*   4. Theta_i (M x M, i=1..q) */
    real ***Theta = tensor(1, (q > 0 ? q : 1), 1, M, 1, M);
    for (k = 1; k <= (q > 0 ? q : 1); k++)
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) Theta[k][i][j] = 0.0;
    for (k = 1; k <= q; k++) {
        if (global_diag_ma) {
            for (i = 1; i <= M; i++) Theta[k][i][i] = x[idx++];
        } else {
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++)
                    Theta[k][i][j] = x[idx++];
        }
    }

    /*   5. Sigma (M x M, lower triangle) — innovation covariance of A_t */
    real **Sigma = matrix(1, M, 1, M);
    for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) Sigma[i][j] = 0.0;
    if (global_diag_cov) {
        for (i = 1; i <= M; i++) Sigma[i][i] = x[idx++];
    } else {
        for (i = 1; i <= M; i++)
            for (j = 1; j <= i; j++) {
                Sigma[i][j] = x[idx++];
                Sigma[j][i] = Sigma[i][j];
            }
    }

    /*   6. Cointegration matrix B₂ (s x r) */
    real **B2 = matrix(1, s, 1, r);
    for (j = 1; j <= r; j++)
        for (i = 1; i <= s; i++)
            B2[i][j] = x[idx++];

    /* [4] Mauricio transformation matrices (eq. 10-14) ---------------------- */
    real **Cbar   = matrix(1, M, 1, M);
    real **Cinv   = matrix(1, M, 1, M);
    real **Hbar   = matrix(1, M, 1, M);
    real **LamBar = matrix(1, M, 1, M);
    for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) {
        Cbar[i][j] = 0.0; Cinv[i][j] = 0.0; Hbar[i][j] = 0.0; LamBar[i][j] = 0.0;
    }
    /* Cbar = [0_{s x r}  I_s ; I_r  B2'] */
    for (i = 1; i <= s; i++) Cbar[i][r + i] = 1.0;
    for (j = 1; j <= r; j++) Cbar[s + j][j] = 1.0;
    for (j = 1; j <= r; j++) for (i = 1; i <= s; i++) Cbar[s + j][r + i] = B2[i][j];
    /* Cinv = [-B2'  I_r ; I_s  0] */
    for (i = 1; i <= r; i++) for (j = 1; j <= s; j++) Cinv[i][j] = -B2[j][i];
    for (i = 1; i <= r; i++) Cinv[i][s + i] = 1.0;
    for (i = 1; i <= s; i++) Cinv[r + i][i] = 1.0;
    /* Hbar = [0 0 ; 0 I_r] */
    for (i = 1; i <= r; i++) Hbar[s + i][s + i] = 1.0;
    /* LamBar = [0, Lambda] (Lambda in the last r columns) */
    for (i = 1; i <= M; i++) for (j = 1; j <= r; j++) LamBar[i][s + j] = Lambda[i][j];

    /* [5] PhiBar_i (eq. 16) ------------------------------------------------- */
    real ***PhBar = tensor(0, p, 1, M, 1, M);
    for (k = 0; k <= p; k++)
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) PhBar[k][i][j] = 0.0;
    /* PhBar[0] = Cinv */
    for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) PhBar[0][i][j] = Cinv[i][j];
    /* PhBar[1] = Cinv*Hbar - LamBar + F1*Cinv (F1 term only if p>=2) */
    {
        real **T1 = matrix(1, M, 1, M);
        matrix_multiply(Cinv, Hbar, T1, M, M, M);
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++)
            PhBar[1][i][j] = T1[i][j] - LamBar[i][j];
        if (p >= 2) {
            real **T2 = matrix(1, M, 1, M);
            matrix_multiply(F[1], Cinv, T2, M, M, M);
            for (i = 1; i <= M; i++) for (j = 1; j <= M; j++)
                PhBar[1][i][j] += T2[i][j];
            free_matrix(T2, 1, M, 1, M);
        }
        free_matrix(T1, 1, M, 1, M);
    }
    /* PhBar[i] = F_i*Cinv - F_{i-1}*Cinv*Hbar  (i=2..p-1) */
    for (k = 2; k <= p - 1; k++) {
        real **T1 = matrix(1, M, 1, M);
        real **T2 = matrix(1, M, 1, M);
        real **T3 = matrix(1, M, 1, M);
        matrix_multiply(F[k],   Cinv, T1, M, M, M);
        matrix_multiply(F[k-1], Cinv, T2, M, M, M);
        matrix_multiply(T2, Hbar, T3, M, M, M);
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++)
            PhBar[k][i][j] = T1[i][j] - T3[i][j];
        free_matrix(T3, 1, M, 1, M);
        free_matrix(T2, 1, M, 1, M);
        free_matrix(T1, 1, M, 1, M);
    }
    /* PhBar[p] = -F_{p-1}*Cinv*Hbar */
    if (p >= 2) {
        real **T1 = matrix(1, M, 1, M);
        real **T2 = matrix(1, M, 1, M);
        matrix_multiply(F[p-1], Cinv, T1, M, M, M);
        matrix_multiply(T1, Hbar, T2, M, M, M);
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++)
            PhBar[p][i][j] = -T2[i][j];
        free_matrix(T2, 1, M, 1, M);
        free_matrix(T1, 1, M, 1, M);
    }

    /* [6] Standard VARMA (eq. 18) ------------------------------------------- */
    /* Phi*_i = Cbar * PhBar_i */
    for (k = 1; k <= p; k++) {
        real **T = matrix(1, M, 1, M);
        matrix_multiply(Cbar, PhBar[k], T, M, M, M);
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++)
            armax->phi[k][i][j] = T[i][j];
        free_matrix(T, 1, M, 1, M);
    }
    /* Theta*_i = Cbar * Theta_i * Cinv */
    for (k = 1; k <= q; k++) {
        real **T1 = matrix(1, M, 1, M);
        real **T2 = matrix(1, M, 1, M);
        matrix_multiply(Cbar, Theta[k], T1, M, M, M);
        matrix_multiply(T1, Cinv, T2, M, M, M);
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++)
            armax->theta[k][i][j] = T2[i][j];
        free_matrix(T2, 1, M, 1, M);
        free_matrix(T1, 1, M, 1, M);
    }
    /* Sigma* = Cbar * Sigma * Cbar'  (covariance of A*_t = Cbar A_t) */
    {
        real **T1 = matrix(1, M, 1, M);
        matrix_multiply(Cbar, Sigma, T1, M, M, M);
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) {
            real ss = 0.0;
            for (int k1 = 1; k1 <= M; k1++) ss += T1[i][k1] * Cbar[j][k1];
            armax->qq[i][j] = ss;
        }
        free_matrix(T1, 1, M, 1, M);
    }
    /* Mean E[Ȳ_t] */
    for (i = 1; i <= M; i++) armax->mu[i] = mu[i];

    free_matrix(LamBar, 1, M, 1, M);
    free_matrix(Hbar, 1, M, 1, M);
    free_matrix(Cinv, 1, M, 1, M);
    free_matrix(Cbar, 1, M, 1, M);
    free_tensor(PhBar, 0, p, 1, M, 1, M);
    free_matrix(Sigma, 1, M, 1, M);
    free_tensor(Theta, 1, (q > 0 ? q : 1), 1, M, 1, M);
    free_tensor(F, 1, (nf > 0 ? nf : 1), 1, M, 1, M);
    free_matrix(Lambda, 1, M, 1, r);
    free_vector(mu, 1, M);

    /* [5] Build Ybar_t = (nabla Y_{2t}', W_t')'                            */
    /* datamat cols 1..s = nabla Y_{2t} (differenced), cols s+1..M = levels */
    /* W_t = Y_{1t} + B2' Y_{2t} with Y_{2t} from cumulated nabla Y_{2t}   */
    /* Reconstruct Y_{2t} by cumulating nabla Y_{2t} */
    int tt;
    real **Y2_level = matrix(1, nobs, 1, s);
    for (i = 1; i <= s; i++)
        Y2_level[1][i] = 0.0;  /* initial level unknown; offset absorbed in mean */
    for (tt = 2; tt <= nobs; tt++)
        for (i = 1; i <= s; i++)
            Y2_level[tt][i] = Y2_level[tt-1][i] + datamat[tt][i];

    for (tt = 1; tt <= nobs; tt++) {
        /* nabla Y_{2t} */
        for (i = 1; i <= s; i++)
            armax->w[tt][i] = datamat[tt][i];
        /* W_t = Y_{1t} + B2' Y_{2t} */
        for (j = 1; j <= r; j++) {
            real w = datamat[tt][s + j];   /* Y_{1t,j} in levels */
            for (i = 1; i <= s; i++)
                w += B2[i][j] * Y2_level[tt][i];
            armax->w[tt][s + j] = w;
        }
    }

    free_matrix(Y2_level, 1, nobs, 1, s);
    free_matrix(B2, 1, s, 1, r);

    /* [7] Deallocate on last call ------------------------------------------ */
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
/*  main                                                                      */
/*****************************************************************************/
int main(int argc, char *argv[])
{
    STRING inputf, outputf, base_name;
    FILE  *inputv;

    if (argc < 5) {
        printf("\nUsage: drvec file p q r [-mean] [-case 1|2|3] [-diagar] "
               "[-diagma] [-diagcov] [-m 1|2] [-lrtest]\n\n");
        printf("  file  : data file name (without .inp extension)\n");
        printf("  p     : AR order of stationary VARMA on Ȳ_t\n");
        printf("  q     : MA order\n");
        printf("  r     : cointegration rank (0 < r < M)\n\n");
        printf("Deterministic cases (Mauricio 2006, Remark 6):\n");
        printf("  -case 1 : E[∇Y₂]=0, E[W]=0     (default)\n");
        printf("  -case 2 : E[∇Y₂]=0, E[W]≠0     (-mean needed)\n");
        printf("  -case 3 : E[∇Y₂]≠0, E[W]≠0     (-mean needed)\n");
        exit(1);
    }

    inputf   = NEW_STR(80);
    outputf  = NEW_STR(80);
    base_name = NEW_STR(80);

    strcpy(base_name, argv[1]);
    strcpy(inputf, argv[1]);

    global_p = atoi(argv[2]);
    global_q = atoi(argv[3]);
    global_r = atoi(argv[4]);

    /* Parse options */
    for (int i = 5; i < argc; i++) {
        if      (strcmp(argv[i], "-mean") == 0)    global_include_mean = 1;
        else if (strcmp(argv[i], "-case") == 0 && i+1 < argc)
            global_case = atoi(argv[++i]);
        else if (strcmp(argv[i], "-diagar") == 0)  global_diag_ar = 1;
        else if (strcmp(argv[i], "-diagma") == 0)  global_diag_ma = 1;
        else if (strcmp(argv[i], "-diagcov") == 0) global_diag_cov = 1;
        else if (strcmp(argv[i], "-m") == 0 && i+1 < argc)
            met = atoi(argv[++i]);
        else if (strcmp(argv[i], "-lrtest") == 0)  global_lrtest = 1;
    }
    /* -mean implies case 2 (E[W]≠0) unless a case was given explicitly */
    if (global_include_mean && global_case == 1) global_case = 2;

    if (global_r < 1) {
        fprintf(stderr, "ERROR: cointegration rank r must be >= 1 "
                "(use -lrtest to test)\n");
        exit(1);
    }

    strcpy(outputf, base_name);
    strcat(outputf, ".out");
    strcat(inputf, ".inp");

    printf("\nDRVEC — VEC model EML estimation (Mauricio 2006)\n");
    printf("Input  : %s\n", inputf);
    printf("Output : %s\n", outputf);
    printf("Model  : VEC(%d) with stationary VARMA(%d,%d) on Ȳ_t\n",
           global_r, global_p, global_q);
    printf("Case   : %d\n", global_case);

    /* [1] Read .inp file --------------------------------------------------- */
    if (NULL == (inputv = fopen(inputf, "r"))) {
        fprintf(stderr, "ERROR: cannot open %s\n", inputf);
        exit(1);
    }
    {
        /* InpReader: same as drvarma v.04.1 */
        /* Minimal reader: read nser, nobs_raw, start, names, lambda, d, D, data */
        char line[512];
        /* skip comments */
        do { if (!fgets(line, sizeof line, inputv)) { fprintf(stderr,"ERROR: unexpected EOF\n"); exit(1); } }
        while (line[0] == '*');
        data_freq = atoi(line);
        /* next non-comment line: nser nobs start_sub start_year */
        do { if (!fgets(line, sizeof line, inputv)) { fprintf(stderr,"ERROR: unexpected EOF\n"); exit(1); } }
        while (line[0] == '*');
        sscanf(line, "%d %d %d %d", &nser, &nobs, &data_start_sub, &data_start_year);

        if (global_r >= nser) {
            fprintf(stderr, "ERROR: r=%d must be < M=%d\n", global_r, nser);
            exit(1);
        }

        series_names = (char **) malloc((nser + 1) * sizeof(char *));
        do { if (!fgets(line, sizeof line, inputv)) { fprintf(stderr,"ERROR: missing series names\n"); exit(1); } }
        while (line[0] == '*');
        {
            char *tok = strtok(line, " \t\n");
            for (int j = 1; j <= nser; j++) {
                if (!tok) { fprintf(stderr,"ERROR: need %d series names\n", nser); exit(1); }
                series_names[j] = strdup(tok);
                tok = strtok(NULL, " \t\n");
            }
        }
        /* Box-Cox lambda, d, D */
        do { if (!fgets(line, sizeof line, inputv)) { fprintf(stderr,"ERROR: missing transform line\n"); exit(1); } }
        while (line[0] == '*');
        sscanf(line, "%lf %d %d", &trans_lambda, &trans_d, &trans_D);

        /* Read data: nobs rows, nser cols */
        datamat = matrix(1, nobs, 1, nser);
        for (int t = 1; t <= nobs; t++) {
            do { if (!fgets(line, sizeof line, inputv)) { fprintf(stderr,"ERROR: data too short at obs %d\n", t); exit(1); } }
            while (line[0] == '*');
            char *tok = strtok(line, " \t\n");
            for (int j = 1; j <= nser; j++) {
                if (!tok) { fprintf(stderr,"ERROR: missing value obs %d col %d\n", t, j); exit(1); }
                datamat[t][j] = atof(tok);
                tok = strtok(NULL, " \t\n");
            }
        }
        fclose(inputv);
    }

    /* The .inp data must contain:
       - cols 1..s (s = M - r): ∇Y_{2t}  (pre-differenced)
       - cols s+1..M (r):       Y_{1t}    (levels)
    */

    printf("Series: %d, Obs: %d, Rank: r=%d\n", nser, nobs, global_r);

    /* [2] Open output ------------------------------------------------------ */
    outputv = fopen(outputf, "w");
    if (!outputv) { fprintf(stderr, "ERROR: cannot write %s\n", outputf); exit(1); }

    fprintf(outputv, "DRVEC — VEC(%d) EML Estimation (Mauricio 2006)\n", global_r);
    fprintf(outputv, "==============================================\n\n");
    fprintf(outputv, "Input  : %s\n", inputf);
    fprintf(outputv, "M = %d, r = %d, s = M-r = %d\n", nser, global_r, nser - global_r);
    fprintf(outputv, "Stationary VARMA(%d,%d) on Ȳ_t\n", global_p, global_q);

    /* [3] Estimation ------------------------------------------------------- */
    int npar = calc_nparametrs();
    real *x   = vector(1, npar);
    real *dev = vector(1, npar);
    real **cov = matrix(1, npar, 1, npar);

    struct Tvarma varma1;
    macheps = cmacheps();
    varma1.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;

    init_guess(x, npar);

    int ifault;
    vec_shootx(x, &varma1, &ifault, 1, 0);  /* allocate */

    int maxits = 500, nrits = 200;
    real gradtol = 1e-5, sptol = 1e-7;
    est(&vec_shootx, npar, x, dev, cov, maxits, nrits, gradtol, sptol,
        varma1.xitol, varma1.a, &varma1.sigma2, &varma1.logelf, &ifault);

    if (ifault == 0) {
        vec_shootx(x, &varma1, &ifault, 0, 0);  /* retrieve final */

        fprintf(outputv, "\nESTIMATION SUCCESSFUL (ifault=0)\n");
        fprintf(outputv, "sigma2 : %15.10f\n", varma1.sigma2);
        fprintf(outputv, "logelf : %15.10f\n", varma1.logelf);

        /* --- Structured VEC output --------------------------------------- */
        int s = nser - global_r, r = global_r;
        int ii = 1;
        fprintf(outputv, "\nVEC model (Mauricio 2006):\n");
        fprintf(outputv, "  (I - F1 L - ... - F_{p-1} L^{p-1}) nabla Y_t =\n");
        fprintf(outputv, "      -Lambda (B' Y_{t-1} - E[W_t]) + (I - Theta1 L - ...) A_t\n\n");

        if (global_case == 2) {
            fprintf(outputv, "E[W] =\n");
            for (int j = 1; j <= r; j++)
                fprintf(outputv, "  %12.6f  (sd = %10.6f)\n", x[ii], dev[ii]), ii++;
        } else if (global_case == 3) {
            fprintf(outputv, "E[nabla Y2] =\n");
            for (int i = 1; i <= s; i++)
                fprintf(outputv, "  %12.6f  (sd = %10.6f)\n", x[ii], dev[ii]), ii++;
            fprintf(outputv, "E[W] =\n");
            for (int j = 1; j <= r; j++)
                fprintf(outputv, "  %12.6f  (sd = %10.6f)\n", x[ii], dev[ii]), ii++;
        }

        fprintf(outputv, "Lambda (M x r) =\n");
        for (int i = 1; i <= nser; i++) {
            fprintf(outputv, "  ");
            for (int j = 1; j <= r; j++)
                fprintf(outputv, "%12.6f", x[ii]), ii++;
            fprintf(outputv, "\n");
        }
        int nf = (global_p > 1) ? global_p - 1 : 0;
        for (int k = 1; k <= nf; k++) {
            fprintf(outputv, "F[%d] (M x M) =\n", k);
            for (int i = 1; i <= nser; i++) {
                fprintf(outputv, "  ");
                for (int j = 1; j <= nser; j++)
                    fprintf(outputv, "%12.6f", x[ii]), ii++;
                fprintf(outputv, "\n");
            }
        }
        for (int k = 1; k <= global_q; k++) {
            fprintf(outputv, "Theta[%d] (M x M) =\n", k);
            for (int i = 1; i <= nser; i++) {
                fprintf(outputv, "  ");
                for (int j = 1; j <= nser; j++)
                    fprintf(outputv, "%12.6f", x[ii]), ii++;
                fprintf(outputv, "\n");
            }
        }
        fprintf(outputv, "Sigma (M x M, lower triangle) =\n");
        for (int i = 1; i <= nser; i++) {
            fprintf(outputv, "  ");
            for (int j = 1; j <= i; j++)
                fprintf(outputv, "%12.6f", x[ii]), ii++;
            fprintf(outputv, "\n");
        }
        fprintf(outputv, "B2 (s x r) =\n");
        for (int i = 1; i <= s; i++) {
            fprintf(outputv, "  ");
            for (int j = 1; j <= r; j++)
                fprintf(outputv, "%12.6f", x[ii]), ii++;
            fprintf(outputv, "\n");
        }

        fprintf(outputv, "\nCointegration matrix B = [I_r; B2'] :\n");
        for (int row = 1; row <= nser; row++) {
            fprintf(outputv, "  row %d: ", row);
            if (row <= r) {
                for (int c = 1; c <= r; c++) fprintf(outputv, "%12.6f", (c==row)?1.0:0.0);
            } else {
                for (int c = 1; c <= r; c++) {
                    int b2idx = npar - s*r + (c-1)*s + (row - r - 1) + 1;
                    fprintf(outputv, "%12.6f", x[b2idx]);
                }
            }
            fprintf(outputv, "\n");
        }

    } else {
        fprintf(outputv, "\nESTIMATION FAILED: ifault = %d\n", ifault);
        switch (ifault) {
            case 1: fprintf(outputv, "  Q not positive definite\n"); break;
            case 2: fprintf(outputv, "  AR operator has unit root\n"); break;
            case 3: fprintf(outputv, "  AR operator non-stationary\n"); break;
            case 4: fprintf(outputv, "  MA operator non-invertible\n"); break;
            case 5: fprintf(outputv, "  Numerical problem\n"); break;
            case 6: fprintf(outputv, "  Error in vec_shootx()\n"); break;
        }
    }

    /* [4] Cleanup ---------------------------------------------------------- */
    vec_shootx(x, &varma1, &ifault, 0, 1);  /* deallocate */
    free_matrix(cov, 1, npar, 1, npar);
    free_vector(dev, 1, npar);
    free_vector(x, 1, npar);
    free_matrix(datamat, 1, nobs, 1, nser);
    for (int j = 1; j <= nser; j++) free(series_names[j]);
    free(series_names);
    FREE_STR(outputf);
    FREE_STR(inputf);
    FREE_STR(base_name);
    fclose(outputv);

    printf("Done. Output written to %s\n", argv[1]);
    return 0;
}