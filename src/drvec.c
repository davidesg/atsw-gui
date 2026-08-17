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
/*                          [-diagcov] [-m 1|2] [-differenced] [-fixb2]      */
/*                          [-lrtest]                                        */
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

real **datamat;     /* data as the estimation needs it: ∇Y_{2t} (cols 1..s)
                       and Y_{1t} in levels (cols s+1..M).  Derived from
                       rawmat by build_y2_levels().                     */
int  nser, nobs;

/* Levels of Y_{2t}, aligned row-for-row with datamat.  W_t = Y_{1t} + B2'Y_{2t}
   needs them, and they do NOT depend on any parameter, so they are built once
   here instead of being rebuilt on every likelihood evaluation.
   By default they are the true levels read from the .inp; with -differenced
   they are reconstructed by cumulating from an arbitrary zero origin, which is
   the legacy behaviour and is wrong for case 1 (see build_y2_levels).         */
real **Y2_levels = NULL;
int  global_levels = 1;   /* default: .inp carries every series in LEVELS.
                             -differenced selects the legacy layout, where
                             cols 1..s arrive already differenced.            */

/* Data exactly as read from the .inp.  datamat and Y2_levels are derived from
   it, and the derivation depends on r (the column split is s = M - r), so with
   -lrtest they are rebuilt for every candidate rank.                          */
real **rawmat = NULL;
int  nobs_raw = 0;

/* Asymptotic critical values for the sequential (lambda-max type) rank test,
   indexed by the number of common trends M-r = 1..11, at 10%, 5% and 1%.
   Non-standard Johansen distribution; MA terms do not affect it (Yap and
   Reinsel, 1995, Theorem 3), as noted in Mauricio (2006), Remark 5.
   Extracted from R's `urca` 1.3.4 (ca.jo, type="eigen"), the same reference
   implementation used throughout benchmark/; Osterwald-Lenum (1992) tables.  */
#define LR_MAXTRENDS 11
static const real lr_cval_none[LR_MAXTRENDS][3] = {   /* case 1: no constant  */
    {  6.50,   8.18,  11.65}, { 12.91,  14.90,  19.19}, { 18.90,  21.07,  25.75},
    { 24.78,  27.14,  32.14}, { 30.84,  33.32,  38.78}, { 36.25,  39.43,  44.59},
    { 42.06,  44.91,  51.30}, { 48.43,  51.07,  57.07}, { 54.01,  57.00,  63.37},
    { 59.00,  62.42,  68.61}, { 65.07,  68.27,  74.36}
};
static const real lr_cval_const[LR_MAXTRENDS][3] = {  /* case 2: restricted c */
    {  7.52,   9.24,  12.97}, { 13.75,  15.67,  20.20}, { 19.77,  22.00,  26.81},
    { 25.56,  28.14,  33.24}, { 31.66,  34.40,  39.79}, { 37.45,  40.30,  46.82},
    { 43.25,  46.45,  51.91}, { 48.91,  52.00,  57.95}, { 54.35,  57.42,  63.71},
    { 60.25,  63.57,  69.94}, { 66.02,  69.74,  76.63}
};

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

/* -fixb2: hold B2 at the static-OLS value computed by init_guess instead of
   estimating it.  This is the restricted model the literature tests against
   the free one (Mauricio 2006, Table 5: B = [1,0]'; BVECM Table 1: beta = 1),
   and it is also the natural first leg of a fix-then-relax warm start.       */
int  global_fixb2 = 0;
int  global_fixb2_given = 0;        /* 1 = a value was supplied on the line */
real global_fixb2_value = 0.0;      /* that value, applied to every entry   */
static real **B2_fixed = NULL;      /* (s x r), owned here */
static int   b2f_s = 0, b2f_r = 0;  /* dims of the current allocation */

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
static void build_y2_levels(void);

/*****************************************************************************/
/*  build_y2_levels — fill Y2_levels, once, before any estimation            */
/*                                                                           */
/*  default       : rawmat cols 1..s hold Y_{2t} in LEVELS.  The first row is  */
/*                  consumed to form the differences, so datamat has one row   */
/*                  fewer: col i = nabla Y_{2t}, col s+j = Y_{1t}, and         */
/*                  Y2_levels holds the matching true levels.                  */
/*  -differenced  : legacy layout, rawmat cols 1..s already hold nabla Y_{2t}. */
/*                  The levels are then unknown and get cumulated from zero, so */
/*                  W_t is off by B2'c for an unknown c.  E[W] absorbs that in  */
/*                  cases 2 and 3 (verified: identical log-likelihood), but NOT */
/*                  in case 1, where E[W] = 0 leaves nothing to absorb it.      */
/*****************************************************************************/
static void build_y2_levels(void)
{
    static int alloc_nobs = 0, alloc_s = 0;   /* dims of the previous build */
    int M = nser, r = global_r, s = M - r;
    int t, i, j;

    if (datamat)   free_matrix(datamat,   1, alloc_nobs, 1, M);
    if (Y2_levels) free_matrix(Y2_levels, 1, alloc_nobs, 1, alloc_s);

    nobs      = global_levels ? nobs_raw - 1 : nobs_raw;
    datamat   = matrix(1, nobs, 1, M);
    Y2_levels = matrix(1, nobs, 1, s);
    alloc_nobs = nobs;
    alloc_s    = s;

    if (global_levels) {
        if (nobs < 3) {
            fprintf(stderr, "ERROR: the levels layout needs at least 4 observations\n");
            exit(1);
        }
        for (t = 1; t <= nobs; t++) {
            for (i = 1; i <= s; i++) {
                datamat[t][i]   = rawmat[t+1][i] - rawmat[t][i];
                Y2_levels[t][i] = rawmat[t+1][i];
            }
            for (j = 1; j <= r; j++)
                datamat[t][s + j] = rawmat[t+1][s + j];
        }
    } else {
        for (t = 1; t <= nobs; t++)
            for (i = 1; i <= M; i++)
                datamat[t][i] = rawmat[t][i];
        for (i = 1; i <= s; i++) Y2_levels[1][i] = 0.0;
        for (t = 2; t <= nobs; t++)
            for (i = 1; i <= s; i++)
                Y2_levels[t][i] = Y2_levels[t-1][i] + datamat[t][i];
    }
}

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

    /* 5. Sigma (lower triangle), minus the redundant scale.
       The engine calls elf with sigma2 = 1 and concentrates the scale out, so
       the objective is exactly invariant to rescaling this block (f1 -> f1/c,
       f2 -> c^m f2).  Carrying the full triangle would leave one direction the
       likelihood cannot see: a flat ridge that makes the line search fail and
       the Hessian singular.  Sigma[1][1] is therefore fixed at 1 and the scale
       is reported through sigma2 (so Sigma[1][1] = sigma2 exactly).          */
    npar += (global_diag_cov ? M : M * (M + 1) / 2) - 1;

    /* 6. B_2 (s x r), unless held fixed by -fixb2 */
    if (!global_fixb2) npar += s * r;

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

    /* --- 0. Levels of Y_{2t}: built once by build_y2_levels() ---------- */
    real **Y2lev = Y2_levels;

    /* --- 1. Initial B₂ via static OLS with intercept ------------------- */
    real **B2 = matrix(1, s, 1, (r > 0 ? r : 1));
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
    real **W  = matrix(1, nobs, 1, (r > 0 ? r : 1));
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
    real *EW   = vector(1, (r > 0 ? r : 1));   /* sample mean of W */
    real *EdY2 = vector(1, s);   /* sample mean of ∇Y₂ */
    for (j = 1; j <= r; j++) { real sm=0.0; for (t=1;t<=nobs;t++) sm+=W[t][j]; EW[j]=sm/nobs; }
    for (i = 1; i <= s; i++) { real sm=0.0; for (t=1;t<=nobs;t++) sm+=dY[t][r+i]; EdY2[i]=sm/nobs; }

    /* --- 4. Conditional regression: ∇Y_t = Σ F_i ∇Y_{t-i} − Λ (W_{t−1}−E[W]) */
    int nreg = r + nf * M;
    int T = nobs - p;              /* rows t = p+1 .. nobs */
    if (T < 1) T = 1;
    real **X = matrix(1, T, 1, (nreg > 0 ? nreg : 1));
    real **Ydep = matrix(1, T, 1, M);
    for (t = p + 1; t <= nobs; t++) {
        int row = t - p, col = 1;
        for (j = 1; j <= r; j++) X[row][col++] = W[t-1][j] - EW[j];
        for (k = 1; k <= nf; k++)
            for (i = 1; i <= M; i++)
                X[row][col++] = dY[t-k][i];
        for (i = 1; i <= M; i++) Ydep[row][i] = dY[t][i];
    }
    /* With r = 0 and p <= 1 there is nothing to regress on (no error-correction
       term, no F_i), so the whole conditional regression is skipped and the
       residuals are the differences themselves.                              */
    int nalloc = (nreg > 0) ? nreg : 1;
    real **XtX = matrix(1, nalloc, 1, nalloc);
    real  *Xty = vector(1, nalloc);
    int *indx = ivector(1, nalloc);
    if (nreg > 0) {
        for (i = 1; i <= nreg; i++)
            for (j = 1; j <= nreg; j++) {
                real ss = 0.0;
                for (t = 1; t <= T; t++) ss += X[t][i]*X[t][j];
                XtX[i][j] = ss;
            }
        ludcp(XtX, nreg, indx);
    }

    real **Lambda = matrix(1, M, 1, (r > 0 ? r : 1));
    real ***F = tensor(1, (nf > 0 ? nf : 1), 1, M, 1, M);
    for (int eq = 1; eq <= M; eq++) {
        if (nreg == 0) break;
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
    /* Covariance block.  The concentrated objective is scale-invariant in qq
       (f1 -> f1/c, f2 -> c^m f2), so only the RATIOS are identified and the
       scale is reported through sigma2.  Divide the residual covariance by its
       (1,1) entry: that pins the scale at var_1 = 1 and, crucially, KEEPS the
       ratios var_i/var_1 that the data provides.
       Seeding them at 1 instead -- which is what normalising to the correlation
       matrix does -- throws that information away.  drtran measured the cost on
       its canonical case: logL -1371 instead of -767, with scales differing by
       1098x.  Here the spread is milder (1.06 on mink-muskrat, up to 15x between
       components on UK consumption) but the failure mode is the same.          */
    {
        real s11 = (Sig[1][1] > 1.0e-24) ? Sig[1][1] : 1.0e-24;
        for (i = 1; i <= M; i++)
            for (j = 1; j <= M; j++)
                Sig[i][j] /= s11;
        Sig[1][1] = 1.0;
    }
    /* Sig[1][1] = 1 is the normalisation, not a parameter. */
    if (global_diag_cov) {
        for (i = 2; i <= M; i++) x[idx++] = Sig[i][i];
    } else {
        for (i = 2; i <= M; i++) x[idx++] = Sig[i][i];
        for (i = 2; i <= M; i++)
            for (j = 1; j < i; j++) x[idx++] = Sig[i][j];
    }
    if (global_fixb2) {
        /* Hand B2 to vec_shootx through B2_fixed; it is not in x[]. */
        if (B2_fixed) free_matrix(B2_fixed, 1, b2f_s, 1, (b2f_r > 0 ? b2f_r : 1));
        B2_fixed = matrix(1, s, 1, (r > 0 ? r : 1));
        b2f_s = s; b2f_r = r;
        for (j = 1; j <= r; j++) for (i = 1; i <= s; i++)
            B2_fixed[i][j] = global_fixb2_given ? global_fixb2_value : B2[i][j];
    } else {
        for (j = 1; j <= r; j++) for (i = 1; i <= s; i++) x[idx++] = B2[i][j];
    }

    if (idx != npar + 1)
        fprintf(stderr, "ERROR init_guess: idx=%d, npar=%d\n", idx-1, npar);

    free_matrix(Sig, 1, M, 1, M);
    free_tensor(F, 1, (nf > 0 ? nf : 1), 1, M, 1, M);
    free_matrix(Lambda, 1, M, 1, (r > 0 ? r : 1));
    free_ivector(indx, 1, nalloc);
    free_matrix(XtX, 1, nalloc, 1, nalloc);
    free_vector(Xty, 1, nalloc);
    free_matrix(X, 1, T, 1, (nreg > 0 ? nreg : 1));
    free_matrix(Ydep, 1, T, 1, M);
    free_vector(EdY2, 1, s);
    free_vector(EW, 1, (r > 0 ? r : 1));
    free_matrix(dY, 1, nobs, 1, M);
    free_matrix(W, 1, nobs, 1, (r > 0 ? r : 1));
    free_matrix(B2, 1, s, 1, (r > 0 ? r : 1));
    /* Y2lev aliases the global Y2_levels; it is not owned here. */
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

    /*   2. Adjustment matrix Lambda (M x r).  With r = 0 there is no
           error-correction term at all: Lambda and B2 are empty, Cbar and
           Cinv collapse to the identity, Hbar to zero, and Ybar_t = nabla Y_t.
           That is the no-cointegration null of the rank test.               */
    real **Lambda = matrix(1, M, 1, (r > 0 ? r : 1));
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

    /*   5. Sigma (M x M) — innovation covariance of A_t, up to the scale.
           Sigma[1][1] = 1 (see calc_nparametrs); the rest goes in RAW, in units
           where var_1 = 1: the ratios var_i/var_1 on the diagonal and the
           covariances off it.
           drtran carries the diagonal as log(var_i/var_1) and applies exp() here,
           which makes positivity structural.  That was tried and REVERTED: it is
           not free.  Measured, exp() drove the optimiser into regions where each
           likelihood evaluation is very slow — the legacy-layout case 3 stopped
           finishing at all, and with correlation seeding the M=5 case did — and it
           lost log-likelihood on three of four configurations against the raw
           diagonal.  Positivity is enforced by the check below instead, which
           costs one Cholesky and does not touch the geometry.                  */
    real **Sigma = matrix(1, M, 1, M);
    for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) Sigma[i][j] = 0.0;
    Sigma[1][1] = 1.0;
    if (global_diag_cov) {
        for (i = 2; i <= M; i++) Sigma[i][i] = x[idx++];
    } else {
        for (i = 2; i <= M; i++) Sigma[i][i] = x[idx++];
        for (i = 2; i <= M; i++)
            for (j = 1; j < i; j++) {
                Sigma[i][j] = x[idx++];
                Sigma[j][i] = Sigma[i][j];
            }
    }

    /* Reject a non-PD Sigma at translation time.  With the raw diagonal the
       optimiser CAN step a variance negative, so this is the guard that keeps
       the parameterisation honest — it is not a redundant extra.  elf() would
       also catch it on qq = Cbar*Sigma*Cbar' (Cbar is nonsingular, so the two
       are equivalent), but here we can say WHICH matrix is the problem.
       objcfunc answers 1.0 on ifault > 0 and the search moves away.           */
    {
        real **Schk = matrix(1, M, 1, M);
        real d1, d2;
        int ifchol = 0;
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) Schk[i][j] = Sigma[i][j];
        choldcp(Schk, M, &d1, &d2, &ifchol);
        free_matrix(Schk, 1, M, 1, M);
        if (ifchol > 0) *ifaultx = 1;      /* Sigma not positive definite */
    }

    /*   6. Cointegration matrix B₂ (s x r) */
    real **B2 = matrix(1, s, 1, (r > 0 ? r : 1));
    for (j = 1; j <= r; j++)
        for (i = 1; i <= s; i++)
            B2[i][j] = global_fixb2 ? B2_fixed[i][j] : x[idx++];

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
    free_matrix(Lambda, 1, M, 1, (r > 0 ? r : 1));
    free_vector(mu, 1, M);

    /* [5] Build Ybar_t = (nabla Y_{2t}', W_t')'                            */
    /* datamat cols 1..s = nabla Y_{2t}, cols s+1..M = Y_{1t} in levels.    */
    /* W_t = Y_{1t} + B2' Y_{2t}; the Y_{2t} levels are parameter-free and  */
    /* were built once by build_y2_levels(), so nothing is recomputed here. */
    int tt;
    real **Y2_level = Y2_levels;

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

    /* Y2_level aliases the global Y2_levels; it is not owned here. */
    free_matrix(B2, 1, s, 1, (r > 0 ? r : 1));

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
               "[-diagma] [-diagcov] [-m 1|2]\n"
               "                 [-differenced] [-fixb2] [-lrtest]\n\n");
        printf("  file  : data file name (without .inp extension)\n");
        printf("  p     : AR order of stationary VARMA on Ȳ_t\n");
        printf("  q     : MA order\n");
        printf("  r     : cointegration rank (0 < r < M; ignored with -lrtest)\n\n");
        printf("Deterministic cases (Mauricio 2006, Remark 6):\n");
        printf("  -case 1 : E[∇Y₂]=0, E[W]=0     (default)\n");
        printf("  -case 2 : E[∇Y₂]=0, E[W]≠0     (-mean needed)\n");
        printf("  -case 3 : E[∇Y₂]≠0, E[W]≠0     (-mean needed)\n\n");
        printf("Data layout (cols 1..s are the Y₂ block, cols s+1..M the Y₁ block):\n");
        printf("  default        every series in LEVELS; ∇Y₂ is formed internally\n");
        printf("                 (one observation is consumed)\n");
        printf("  -differenced   legacy: cols 1..s already hold ∇Y₂.  The Y₂ levels\n");
        printf("                 are then unknown and get cumulated from zero, which\n");
        printf("                 breaks -case 1 and makes E[W] incomparable\n\n");
        printf("  -fixb2 [v]     hold B2 fixed instead of estimating it; npar\n");
        printf("                 drops by s*r.  With a value, every entry of B2 is\n");
        printf("                 pinned at v -- an a-priori restriction, so 2*[L(free)\n");
        printf("                 - L(fixed)] IS a valid LR test, chi2 with s*r df\n");
        printf("                 (Mauricio 2006 Table 5 tests B = [1,0]', i.e. -fixb2 0).\n");
        printf("                 Without a value B2 is held at its static-OLS estimate:\n");
        printf("                 useful as a warm start or a conditioning check, but\n");
        printf("                 the restriction is then data-chosen, so the LR\n");
        printf("                 statistic is NOT a valid test.\n\n");
        printf("  -lrtest        sequential LR test for the cointegration rank:\n");
        printf("                 estimates r = 0..M-1 and reports 2*[L(r+1) - L(r)];\n");
        printf("                 incompatible with -differenced\n");
        exit(1);
    }

    /* Size these from the actual argument, not a fixed 80: a longer path used
       to overflow them through the strcpy/strcat below and abort the run.
       +8 covers the ".inp"/".out" suffix and the terminator.                 */
    {
        int need = (int) strlen(argv[1]) + 8;
        inputf    = NEW_STR(need);
        outputf   = NEW_STR(need);
        base_name = NEW_STR(need);
        if (!inputf || !outputf || !base_name) {
            fprintf(stderr, "ERROR: out of memory for file names\n");
            exit(1);
        }
    }

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
        else if (strcmp(argv[i], "-levels") == 0)  global_levels = 1;  /* default */
        else if (strcmp(argv[i], "-differenced") == 0) global_levels = 0;
        else if (strcmp(argv[i], "-fixb2") == 0) {
            global_fixb2 = 1;
            /* An optional numeric argument pins B2 at a value chosen a priori,
               which is what makes the LR test against the free model valid.  */
            if (i + 1 < argc) {
                char *end;
                double v = strtod(argv[i+1], &end);
                if (end != argv[i+1] && *end == '\0') {
                    global_fixb2_value = v;
                    global_fixb2_given = 1;
                    i++;
                }
            }
        }
    }
    /* -mean implies case 2 (E[W]≠0) unless a case was given explicitly */
    if (global_include_mean && global_case == 1) global_case = 2;

    if (global_lrtest) {
        /* The column split of the .inp is s = M - r, so varying r only makes
           sense when every series is supplied in levels.                     */
        if (!global_levels) {
            fprintf(stderr,
                "ERROR: -lrtest is incompatible with -differenced.  The column\n"
                "       split depends on r (cols 1..M-r are Y_2), so a file that\n"
                "       is already differenced cannot be re-read at another rank.\n"
                "       Supply every series in levels (the default layout).\n");
            exit(1);
        }
        if (global_r < 1) global_r = 1;   /* r on the command line is ignored */
    } else if (global_r < 1) {
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
        sscanf(line, "%d %d %d %d", &nser, &nobs_raw, &data_start_sub, &data_start_year);
        nobs = nobs_raw;

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

        /* Read data: nobs_raw rows, nser cols, kept untouched in rawmat */
        rawmat = matrix(1, nobs_raw, 1, nser);
        for (int t = 1; t <= nobs_raw; t++) {
            do { if (!fgets(line, sizeof line, inputv)) { fprintf(stderr,"ERROR: data too short at obs %d\n", t); exit(1); } }
            while (line[0] == '*');
            char *tok = strtok(line, " \t\n");
            for (int j = 1; j <= nser; j++) {
                if (!tok) { fprintf(stderr,"ERROR: missing value obs %d col %d\n", t, j); exit(1); }
                rawmat[t][j] = atof(tok);
                tok = strtok(NULL, " \t\n");
            }
        }
        fclose(inputv);
    }

    /* The .inp data must contain, in column order [Y_2 block ; Y_1 block]:
       - cols 1..s (s = M - r): Y_{2t} in levels  (or ∇Y_{2t} with -differenced)
       - cols s+1..M (r):       Y_{1t} in levels
    */
    build_y2_levels();          /* once: never inside the likelihood loop */

    if (!global_levels && global_case == 1)
        fprintf(stderr,
            "WARNING: -case 1 with -differenced.  Y_2 levels are reconstructed by\n"
            "         cumulating from an arbitrary zero origin, which shifts W_t\n"
            "         by B2'c.  With E[W] = 0 (case 1) nothing absorbs that shift\n"
            "         and it contaminates B2.  Supply every series in levels (the\n"
            "         default layout), or use -case 2 / -case 3.\n");

    printf("Series: %d, Obs: %d, Rank: r=%d  (%s)\n", nser, nobs, global_r,
           global_levels ? "levels" : "legacy pre-differenced layout");

    /* [2] Open output ------------------------------------------------------ */
    outputv = fopen(outputf, "w");
    if (!outputv) { fprintf(stderr, "ERROR: cannot write %s\n", outputf); exit(1); }

    fprintf(outputv, "DRVEC — VEC(%d) EML Estimation (Mauricio 2006)\n", global_r);
    fprintf(outputv, "==============================================\n\n");
    fprintf(outputv, "Input  : %s\n", inputf);
    fprintf(outputv, "M = %d, r = %d, s = M-r = %d\n", nser, global_r, nser - global_r);
    fprintf(outputv, "Stationary VARMA(%d,%d) on Ȳ_t\n", global_p, global_q);
    fprintf(outputv, "Case   : %d\n", global_case);
    fprintf(outputv, "Layout : %s (%d of %d observations used)\n",
            global_levels ? "all series in levels"
                          : "legacy, cols 1..s pre-differenced (-differenced)",
            nobs, nobs_raw);

    /* [3a] Sequential LR test for the cointegration rank (Mauricio 2006,
            Remark 5 and Table 3): estimate r = 1..M-1 and report
            2*[L(r+1) - L(r)] against the non-standard asymptotic values.    */
    if (global_lrtest) {
        int M = nser, ok;
        macheps = cmacheps();           /* the engine needs it; [3] is skipped */
        /* Ranks 0..M-1.  r = 0 is the no-cointegration null: Pi = 0, so the
           model is a plain VARMA on nabla Y.  It is the first and most
           important comparison of the sequence.                              */
        real *ll  = vector(0, M - 1);
        int  *npr = ivector(0, M - 1);
        int  *good = ivector(0, M - 1);

        fprintf(outputv, "\n=== Sequential LR test for the cointegration rank ===\n");
        printf("\nSequential LR test for the cointegration rank:\n");

        for (int rr = 0; rr <= M - 1; rr++) {
            global_r = rr;
            build_y2_levels();
            int np = calc_nparametrs();
            real *xr   = vector(1, np);
            real *devr = vector(1, np);
            real **covr = matrix(1, np, 1, np);
            struct Tvarma vr;
            vr.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;
            init_guess(xr, np);
            int ifr;
            vec_shootx(xr, &vr, &ifr, 1, 0);
            est(&vec_shootx, np, xr, devr, covr, 500, 200, 1e-5, 1e-7,
                vr.xitol, vr.a, &vr.sigma2, &vr.logelf, &ifr);
            ok = (ifr == 0);
            good[rr] = ok;
            npr[rr]  = np;
            ll[rr]   = ok ? vr.logelf : 0.0;
            printf("  r = %d : %s (ifault=%d)\n", rr,
                   ok ? "ok" : "estimation failed", ifr);
            vec_shootx(xr, &vr, &ifr, 0, 1);   /* deallocate */
            free_matrix(covr, 1, np, 1, np);
            free_vector(devr, 1, np);
            free_vector(xr, 1, np);
        }

        fprintf(outputv, "\n  r    npar        logL         AIC         BIC\n");
        fprintf(outputv, "  ---------------------------------------------------\n");
        for (int rr = 0; rr <= M - 1; rr++) {
            if (!good[rr]) { fprintf(outputv, "  %-4d  --   estimation failed\n", rr); continue; }
            real aic = (-2.0 * ll[rr] + 2.0 * npr[rr]) / nobs;
            real bic = (-2.0 * ll[rr] + npr[rr] * log((real) nobs)) / nobs;
            fprintf(outputv, "  %-4d %4d %12.4f %11.4f %11.4f\n",
                    rr, npr[rr], ll[rr], aic, bic);
        }

        fprintf(outputv,
            "\n  H0: P = r   vs   H1: P = r+1        LR = 2*[L(r+1) - L(r)]\n");
        fprintf(outputv,
            "  (asymptotic critical values: %s)\n",
            (global_case == 1) ? "case 1, no constant" :
            (global_case == 2) ? "case 2, restricted constant" :
                                 "case 3 — NOT TABULATED HERE");
        fprintf(outputv, "\n  r    M-r        LR      10%%      5%%      1%%\n");
        fprintf(outputv, "  ---------------------------------------------------\n");
        for (int rr = 0; rr <= M - 2; rr++) {
            if (!good[rr] || !good[rr+1]) {
                fprintf(outputv, "  %-4d  --   (a model in the pair failed)\n", rr);
                continue;
            }
            real lr = 2.0 * (ll[rr+1] - ll[rr]);
            int  g  = M - rr;                    /* common trends under H0 */
            /* Rank r is nested in rank r+1, so L(r+1) >= L(r) at the true
               maxima.  A negative LR proves at least one of the two fits did
               not reach its optimum, and the statistic means nothing.        */
            if (lr < 0.0) {
                fprintf(outputv,
                        "  %-4d %4d %10.4f   NOT INTERPRETABLE: LR < 0, so at "
                        "least one of the\n                              two fits "
                        "did not converge (rank r is nested in r+1)\n", rr, g, lr);
                continue;
            }
            fprintf(outputv, "  %-4d %4d %10.4f", rr, g, lr);
            if (global_case != 3 && g >= 1 && g <= LR_MAXTRENDS) {
                const real *cv = (global_case == 1) ? lr_cval_none[g-1]
                                                    : lr_cval_const[g-1];
                fprintf(outputv, " %8.2f %8.2f %8.2f", cv[0], cv[1], cv[2]);
                if      (lr > cv[2]) fprintf(outputv, "   reject H0 at 1%%");
                else if (lr > cv[1]) fprintf(outputv, "   reject H0 at 5%%");
                else if (lr > cv[0]) fprintf(outputv, "   reject H0 at 10%%");
                else                 fprintf(outputv, "   H0 not rejected");
            } else {
                fprintf(outputv, "        -        -        -   (no values)");
            }
            fprintf(outputv, "\n");
        }
        fprintf(outputv,
            "\n  Distribution is the non-standard Johansen one; MA terms do not\n"
            "  affect it (Yap and Reinsel 1995, Thm. 3; Mauricio 2006, Remark 5).\n"
            "  Values from R urca 1.3.4 ca.jo(type=\"eigen\"); Osterwald-Lenum (1992).\n"
            "  r = 0 is the no-cointegration null (Pi = 0, a plain VARMA on\n"
            "  nabla Y); r = M would be a stationary process in levels and is not\n"
            "  expressible here, so the sequence ends at r = M-1.  Read it with\n"
            "  AIC/BIC, and check the optimizer banner of every rank before\n"
            "  trusting a statistic.\n");

        free_ivector(good, 0, M - 1);
        free_ivector(npr, 0, M - 1);
        free_vector(ll, 0, M - 1);
        printf("Done. Output written to %s\n", base_name);
        fclose(outputv);
        return 0;
    }

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
        /* Under -diagar / -diagma only the M diagonal entries are carried in
           x[] (see calc_nparametrs and vec_shootx), so the walk must consume
           M, not M*M, while still displaying the full matrix.                */
        int nf = (global_p > 1) ? global_p - 1 : 0;
        for (int k = 1; k <= nf; k++) {
            fprintf(outputv, "F[%d] (M x M) =\n", k);
            for (int i = 1; i <= nser; i++) {
                fprintf(outputv, "  ");
                for (int j = 1; j <= nser; j++)
                    fprintf(outputv, "%12.6f",
                            global_diag_ar ? ((i == j) ? x[ii++] : 0.0) : x[ii++]);
                fprintf(outputv, "\n");
            }
        }
        for (int k = 1; k <= global_q; k++) {
            fprintf(outputv, "Theta[%d] (M x M) =\n", k);
            for (int i = 1; i <= nser; i++) {
                fprintf(outputv, "  ");
                for (int j = 1; j <= nser; j++)
                    fprintf(outputv, "%12.6f",
                            global_diag_ma ? ((i == j) ? x[ii++] : 0.0) : x[ii++]);
                fprintf(outputv, "\n");
            }
        }
        /* The engine concentrates the covariance scale: it calls elf with
           sigma2 = 1, so the block carried in x[] is identified only up to a
           positive constant.  Report it as Q (what is estimated) and the
           innovation covariance separately as sigma2 * Q, as drvarma does.   */
        real **Qm = matrix(1, nser, 1, nser);
        for (int i = 1; i <= nser; i++)
            for (int j = 1; j <= nser; j++) Qm[i][j] = 0.0;

        /* Same layout as vec_shootx: Q[1][1] = 1, then the variance ratios on
           the diagonal (i >= 2), then the off-diagonals by rows.              */
        Qm[1][1] = 1.0;
        for (int i = 2; i <= nser; i++) Qm[i][i] = x[ii++];
        if (!global_diag_cov)
            for (int i = 2; i <= nser; i++)
                for (int j = 1; j < i; j++) Qm[i][j] = Qm[j][i] = x[ii++];

        fprintf(outputv, "Q (M x M, lower triangle; Q[1][1] = 1 by normalisation) =\n");
        for (int i = 1; i <= nser; i++) {
            fprintf(outputv, "  ");
            for (int j = 1; j <= i; j++) fprintf(outputv, "%12.6f", Qm[i][j]);
            fprintf(outputv, "\n");
        }
        fprintf(outputv, "Sigma = sigma2 * Q  (innovation covariance of A_t) =\n");
        for (int i = 1; i <= nser; i++) {
            fprintf(outputv, "  ");
            for (int j = 1; j <= i; j++)
                fprintf(outputv, "%12.6f", varma1.sigma2 * Qm[i][j]);
            fprintf(outputv, "\n");
        }
        free_matrix(Qm, 1, nser, 1, nser);

        /* B2 is stored COLUMN-major in x[] (vec_shootx and init_guess both
           write `for j in 1..r { for i in 1..s }`), so it must be read back in
           that order before being displayed row by row.  Both printers below
           use this one copy, so they cannot disagree.                        */
        real **B2m = matrix(1, s, 1, (r > 0 ? r : 1));
        for (int j = 1; j <= r; j++)
            for (int i = 1; i <= s; i++)
                B2m[i][j] = global_fixb2 ? B2_fixed[i][j] : x[ii++];

        if (global_fixb2)
            fprintf(outputv, "B2 (s x r) = [FIXED at %s]\n",
                    global_fixb2_given ? "the value given on the command line"
                                       : "its static-OLS estimate (data-chosen: "
                                         "an LR test against the free model is "
                                         "NOT valid)");
        else
            fprintf(outputv, "B2 (s x r) =\n");
        for (int i = 1; i <= s; i++) {
            fprintf(outputv, "  ");
            for (int j = 1; j <= r; j++)
                fprintf(outputv, "%12.6f", B2m[i][j]);
            fprintf(outputv, "\n");
        }

        if (ii != npar + 1)
            fprintf(stderr, "ERROR output: consumed %d of %d parameters\n",
                    ii - 1, npar);

        fprintf(outputv, "\nCointegration matrix B = [I_r; B2] (M x r) :\n");
        for (int row = 1; row <= nser; row++) {
            fprintf(outputv, "  row %d: ", row);
            for (int c = 1; c <= r; c++)
                fprintf(outputv, "%12.6f",
                        (row <= r) ? ((c == row) ? 1.0 : 0.0) : B2m[row - r][c]);
            fprintf(outputv, "\n");
        }
        free_matrix(B2m, 1, s, 1, (r > 0 ? r : 1));

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
    free_matrix(Y2_levels, 1, nobs, 1, nser - global_r);
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