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
#include "fue_pre_reader.h"   /* lector de .pre, copiado de drtran (ver F2.1) */
#include "fue_bridge.h"       /* expansion de los factores del .pre           */
#include <gsl/gsl_cdf.h>      /* p-valor chi2 del LR de H1(r) contra H(r)     */

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

/*  F3 — restricciones lineales sobre los coeficientes de ajuste.
 *
 *  Johansen y Swensen (2024, JTSA 45:248-268) definen H1(r): alpha = A*psi con A
 *  conocida M x sa de rango sa, frente a H(r) con alpha libre.  La exogeneidad
 *  debil es el caso particular en que A selecciona filas, asi que no hace falta
 *  un test ad hoc: se implementa la clase general y aquella sale de ella.
 *
 *  Y aqui drvec esta bien colocado, mejor que el legado: **Lambda esta EN su
 *  vector de parametros**, asi que imponer alpha = A*psi es sustituir M*r
 *  entradas libres por sa*r y calcular Lambda = A*psi dentro del cast -- el
 *  mismo tipo de cambio que -fixb2 -- y su covarianza sale directa del hessiano.
 *  En coordenadas BVECM alpha es DERIVADA, y por eso drv_project necesitaba el
 *  metodo delta con pseudoinversa SVD (LEGACY_NOTES.md 5) para lo mismo.
 *
 *  Grados de libertad del LR contra H(r): (M - sa) * r, explicitos en el
 *  articulo.                                                                  */
static int    global_alpha = 0;      /* -alpha <fichero> o -weakex <i>         */
static real **alpha_A      = NULL;   /* (M x sa), la A de Johansen y Swensen   */
static int    alpha_sa     = 0;
static char  *alpha_file   = NULL;
static int    alpha_weakex = 0;      /* i > 0: ecuacion declarada exogena debil */

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

    /* 2. Adjustment matrix Lambda (M x r), o psi (sa x r) si alpha = A*psi */
    npar += (global_alpha ? alpha_sa : M) * r;

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
/*  prelim_b2 — B₂ inicial por OLS estático con constante.
 *
 *  Extraído de init_guess sin cambiarle nada, porque lo necesitan DOS sitios:
 *  la siembra y el escritor de .inp de F2, que tiene que construir el mismo Ȳ
 *  que se va a estimar.  B2 se espera dimensionada (1..s, 1..max(r,1)).       */
static void prelim_b2(real **B2)
{
    int M = nser, r = global_r, s = M - r;
    int i, j, t;
    real **Y2lev = Y2_levels;

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
    (void) M;
}

/*  build_ybar — Ȳ_t = (∇Y_{2t}', W_t')' para un B₂ dado.
 *
 *  MISMA construcción que el bloque [5] de vec_shootx; si las dos dejan de
 *  coincidir, lo que drvec escribe en los .inp no es lo que estima.  Ybar se
 *  espera dimensionada (1..nobs, 1..M).                                      */
static void build_ybar(real **B2, real **Ybar)
{
    int M = nser, r = global_r, s = M - r;
    int i, j, t;
    for (t = 1; t <= nobs; t++) {
        for (i = 1; i <= s; i++) Ybar[t][i] = datamat[t][i];
        for (j = 1; j <= r; j++) {
            real w = datamat[t][s + j];
            for (i = 1; i <= s; i++) w += B2[i][j] * Y2_levels[t][i];
            Ybar[t][s + j] = w;
        }
    }
}

/*  cleanup_names — lo que hay que soltar al terminar, en UN sitio.
 *
 *  main tiene cuatro salidas -- la normal, -lrtest, -writeinp/-writeres y
 *  -eval -- y solo la normal liberaba.  valgrind lo caza en cuanto se le
 *  pregunta: 350 bytes por corrida de -lrtest.  Es una fuga de fin de programa
 *  y no le hace dano a nadie, pero tener cuatro salidas y una sola limpieza es
 *  la forma en la que estas cosas se convierten en algo peor.                */
static void cleanup_names(char *outf, char *inf, char *basef)
{
    if (series_names) {
        for (int j = 1; j <= nser; j++) free(series_names[j]);
        free(series_names);
        series_names = NULL;
    }
    if (outf)  FREE_STR(outf);
    if (inf)   FREE_STR(inf);
    if (basef) FREE_STR(basef);
}

/*  subtract_interventions — quita de los datos el componente determinista que
 *  cada serie declara en su .pre.
 *
 *  POR QUE HACE FALTA.  El cast de fue admite intervenciones -- omega(B)/delta(B)
 *  sobre un impulso, escalon, rampa... -- y los modelos univariantes de la
 *  escalera las usan.  Estimar despues un VEC que las ignora es estimar otro
 *  modelo: en el ejercicio que motivo esto, una serie con un impulso de
 *  respuesta de cinco anios sobre una muestra de 76 daba un vector de
 *  cointegracion sin sentido economico (positivo), y la causa era esa omision.
 *
 *  COMO.  build_det_component viene con el lector vendorizado y calcula
 *  nu(B) = omega(B)/delta(B) aplicado al regresor, con la convencion de signos
 *  de Box-Jenkins (omega_0 suma, los demas restan) y el caso racional incluido.
 *  Se le pasa el modelo del .pre pero LAS FECHAS DE ESTA MUESTRA, porque una
 *  determinista es funcion del tiempo: si se alinean por indice en vez de por
 *  fecha, la intervencion cae en el anio equivocado.
 *
 *  LIMITE, declarado: los omega quedan FIJADOS en lo que estimo fue por
 *  separado; no se reestiman conjuntamente.  drtran si los lleva en su vector de
 *  parametros (BRIDGE_DESIGN.md), y ese es el paso siguiente natural.  Mientras
 *  tanto esto es "las intervenciones del modelo univariante, aplicadas", que es
 *  bastante mejor que "sin intervenciones" y peor que estimarlas.             */
static void subtract_interventions(const char *prefix)
{
    int M = nser, i, t, nsub = 0;

    for (i = 1; i <= M; i++) {
        char path[1024];
        struct Tusmodel Tm;
        struct Tseries  Ts, Tsx;
        real **DataMat = NULL;
        real  *det;

        snprintf(path, sizeof path, "%s.%d.pre", prefix, i);
        if (read_fue_pre(path, &Tm, &Ts, &DataMat) != 0) {
            char alt[1024];
            snprintf(alt, sizeof alt, "%s.%d.inp", prefix, i);
            if (read_fue_pre(alt, &Tm, &Ts, &DataMat) != 0) {
                fprintf(stderr, "WARNING: no se pudo leer %s ni %s; la serie %d "
                                "va sin deterministas\n", path, alt, i);
                continue;
            }
        }
        if (Tm.NdetVar > 0) {
            /* fechas de ESTA muestra, modelo del .pre */
            Tsx = Ts;
            Tsx.freq    = data_freq;
            Tsx.begyear = data_start_year;
            Tsx.begtime = data_start_sub;
            Tsx.nobs    = nobs_raw;
            det = vector(1, nobs_raw);
            build_det_component(&Tm, &Tsx, nobs_raw, det);
            for (t = 1; t <= nobs_raw; t++) rawmat[t][i] -= det[t];
            if (!quiet_mode) {
                printf("  serie %d: %d determinista(s) del .pre restadas (", i,
                       Tm.NdetVar);
                for (int k = 1; k <= Tm.NdetVar; k++)
                    printf("%s%s", (k > 1 ? "; " : ""),
                           Tm.detspec[k] ? Tm.detspec[k] : "?");
                printf(")\n");
            }
            fprintf(outputv, "Series %d: %d deterministic term(s) from %s "
                             "subtracted before estimation.\n",
                    i, Tm.NdetVar, path);
            free_vector(det, 1, nobs_raw);
            nsub++;
        }
        free_fue_pre(&Tm, &Ts, DataMat);
    }
    if (nsub == 0 && !quiet_mode)
        printf("  (ningun .pre declaraba deterministas)\n");
}

/*  residual_diagnostics — la diagnosis multivariante de los residuos.
 *
 *  POR QUE ESTA AQUI.  drvec no hacia NINGUNA diagnosis: main.h declara
 *  hosking_test y multivariate_diagnostics por herencia del header de drvarma,
 *  pero esas rutinas no existen en este proyecto y el .out no decia nada de los
 *  residuos.  Un estimador que no ensena sus residuos no se puede usar para
 *  identificar, y en la aplicacion que motivo esto la pregunta es precisamente
 *  de identificacion: heredado el ARMA univariante de un trabajo de ACF/PACF
 *  limpio, lo unico que queda por decidir es si hay EFECTOS CRUZADOS y de que
 *  orden.  Eso no se contesta mirando la verosimilitud; se contesta mirando las
 *  correlaciones CRUZADAS de los residuos, retardo por retardo.
 *
 *  QUE IMPRIME, EN DOS PARTES
 *
 *   1. LA DIAGNOSIS DE LA SUITE, tal cual: multivariate_diagnostics de
 *      drtran -- portmanteau de Hosking y Jarque-Bera multivariante --, copiada
 *      sin cambios en src/diagnose_mv.c.  Es deliberado: el mismo residuo tiene
 *      que leerse igual en drvarma, en drtran y aqui.  La primera version de
 *      esto fue un portmanteau escrito a mano en este fichero, y estaba mal
 *      planteado aunque fuera correcto: obligaba a comparar peras con manzanas.
 *
 *   2. LO QUE DRVEC ANADE, adaptado a su realidad: la matriz de correlaciones
 *      cruzadas R(k) para k = 0..K, con lo que pasa la banda +-2/sqrt(n)
 *      marcado.  La DIAGONAL de R(k) es la ACF de cada ecuacion (dinamica
 *      propia mal recogida); las FUERA DE DIAGONAL son el efecto cruzado que el
 *      modelo no ha capturado, y su k es SU ORDEN.  Eso es lo que un
 *      portmanteau agregado no puede decir, y es justo la pregunta que queda
 *      cuando el ARMA univariante viene ya identificado de un trabajo de
 *      ACF/PACF limpio: si hay efectos cruzados y de que orden.
 *
 *  R(k)[i][j] correlaciona a_i(t) con a_j(t-k), asi que un elemento (i,j)
 *  significativo con k >= 1 dice que la ecuacion i responde a la innovacion
 *  PASADA de j: es un efecto cruzado retardado de orden k.  El triangulo
 *  superior y el inferior NO son lo mismo, y ahi esta la direccion.           */
static void residual_diagnostics(struct Tvarma *v)
{
    int M = v->m, n = v->n, K, i, j, k, t;
    real band, ***C;
    real *mean = vector(1, M);

    if (n < 20 || M < 1) return;
    K = (n / 4 < 12) ? n / 4 : 12;
    if (K < 1) return;
    band = 2.0 / sqrt((real) n);

    /* medias (deberian ser ~0) y matrices de autocovarianza C(k) */
    for (i = 1; i <= M; i++) {
        real sm = 0.0;
        for (t = 1; t <= n; t++) sm += v->a[t][i];
        mean[i] = sm / n;
    }
    C = tensor(0, K, 1, M, 1, M);
    for (k = 0; k <= K; k++)
        for (i = 1; i <= M; i++)
            for (j = 1; j <= M; j++) {
                real sm = 0.0;
                for (t = k + 1; t <= n; t++)
                    sm += (v->a[t][i] - mean[i]) * (v->a[t-k][j] - mean[j]);
                C[k][i][j] = sm / n;
            }

    fprintf(outputv, "\n=== Residual diagnostics ===\n");
    fprintf(outputv, "  residual sd:");
    for (i = 1; i <= M; i++) fprintf(outputv, " %10.6f", sqrt(C[0][i][i]));
    fprintf(outputv, "\n  band = 2/sqrt(n) = %.4f;  * marks |r| > band\n", band);
    fprintf(outputv, "\n  Cross-correlation matrices R(k): R(k)[i][j] = "
                     "corr(a_i(t), a_j(t-k))\n"
                     "  The DIAGONAL is each equation's own ACF; the "
                     "OFF-DIAGONAL is the cross\n"
                     "  effect the model has not captured, and its k is its "
                     "order.\n");
    for (k = 0; k <= K; k++) {
        fprintf(outputv, "  k=%-2d ", k);
        for (i = 1; i <= M; i++) {
            if (i > 1) fprintf(outputv, "\n       ");
            for (j = 1; j <= M; j++) {
                real r = C[k][i][j] / sqrt(C[0][i][i] * C[0][j][j]);
                fprintf(outputv, "%8.3f%s", r, (fabs(r) > band) ? "*" : " ");
            }
        }
        fprintf(outputv, "\n");
    }

    /* La diagnosis ESTANDAR de la suite, sin tocar (src/diagnose_mv.c). */
    multivariate_diagnostics(v->a, n, M, outputv);

    /* el veredicto sobre efectos cruzados, que es la pregunta que importa */
    {
        int worst_k = -1, wi = 0, wj = 0, any = 0;
        real worst = 0.0;
        /* SOLO k >= 1.  La correlacion cruzada CONTEMPORANEA (k = 0) no es un
           fallo del modelo: es la fuera-de-diagonal de Sigma, que el modelo
           ESTIMA -- salvo con -diagcov, donde si seria una restriccion mal
           puesta.  Contarla aqui haria saltar la alarma en cualquier modelo con
           innovaciones correlacionadas, que es la situacion normal.          */
        for (k = 1; k <= K; k++)
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) {
                    real r;
                    if (i == j) continue;                    /* solo cruzados */
                    r = C[k][i][j] / sqrt(C[0][i][i] * C[0][j][j]);
                    if (fabs(r) > band) any = 1;
                    if (fabs(r) > fabs(worst)) { worst = r; worst_k = k; wi = i; wj = j; }
                }
        {   /* la contemporanea se informa aparte, no como fallo */
            real r0 = 0.0;
            for (i = 1; i <= M; i++)
                for (j = 1; j < i; j++) {
                    real r = C[0][i][j] / sqrt(C[0][i][i] * C[0][j][j]);
                    if (fabs(r) > fabs(r0)) r0 = r;
                }
            fprintf(outputv, "\n  Contemporaneous innovation correlation "
                             "(largest |r| at k=0): %+.3f\n", r0);
            if (global_diag_cov && fabs(r0) > band)
                fprintf(outputv, "    NOTE: -diagcov forces Sigma diagonal, so "
                                 "this one IS an imposed\n    restriction the "
                                 "data does not support.\n");
            else
                fprintf(outputv, "    Not a defect: Sigma carries it "
                                 "(it is estimated).\n");
        }
        fprintf(outputv, "  Cross DYNAMICS left in the residuals (k >= 1): ");
        if (!any)
            fprintf(outputv, "none beyond the band.\n"
                    "    The cross structure in the model is enough for this "
                    "sample.\n");
        else
            fprintf(outputv, "YES.\n"
                    "    Largest: equation %d against the innovation of %d at "
                    "lag %d, r = %+.3f.\n"
                    "    A cross effect at lag k needs the model to reach lag k: "
                    "raise p (or q)\n"
                    "    if k >= the current order, and check the DIRECTION -- "
                    "R(k)[i][j] and\n"
                    "    R(k)[j][i] are different statements.\n",
                    wi, wj, worst_k, worst);
    }

    free_tensor(C, 0, K, 1, M, 1, M);
    free_vector(mean, 1, M);
}

/*  Nota de convergencia — POR QUE paro, no solo SI paro.
 *
 *  En VARMA multivariante la razon de la parada es un diagnostico de primer
 *  orden: verosimilitudes mal condicionadas, casi no identificacion y factores
 *  comunes se manifiestan como terminacion en steptol y no en el gradiente.
 *  La suite ya lo tenia establecido -- drvarma lo arreglo en su
 *  report._convergence_block y drtran lo expone como Fit.convergence_note --,
 *  y drvec imprimia el criterio sin decir lo que significa.
 *
 *  DOS COSAS QUE ESTA NOTA ARREGLA, las dos heredadas:
 *
 *   - termcode 2 (steptol) se anunciaba como "OPTIMIZER CONVERGED" a secas.  Lo
 *     es en el sentido del programa, pero es el sintoma tipico de una
 *     verosimilitud mal condicionada y los errores estandar no son de fiar.
 *   - "ESTIMATION SUCCESSFUL (ifault=0)" se lee como convergencia y NO LO ES:
 *     ifault es adecuacion del MODELO, no del optimizador (drvarma lo documenta
 *     explicitamente).  Un ajuste que paro lejos de un optimo puede tener
 *     ifault = 0 perfectamente.
 *
 *  COMO SE OBTIENE EL TERMCODE.  est() no lo devuelve, y report() vive en
 *  qnewtopt.c, que es MOTOR y no se toca -- ni por una linea, ni para exponer un
 *  observable.  Asi que se lee del texto que report() ya escribio en el propio
 *  .out.  Es fragil respecto a esa cadena y sobre nada mas, y la alternativa era
 *  tocar codigo publicado y refereado.
 *
 *  Devuelve el termcode 1..5, o 0 si no se pudo determinar.                  */
static int termcode_from_out(const char *path)
{
    FILE *f;
    char line[512];
    int code = 0;

    fflush(outputv);
    f = fopen(path, "r");
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        if (!strstr(line, "Convergence criterion:")) continue;
        if      (strstr(line, "gradtol"))        code = 1;
        else if (strstr(line, "steptol"))        code = 2;
        else if (strstr(line, "lower point"))    code = 3;
        else if (strstr(line, "iteration limit"))code = 4;
        else if (strstr(line, "maximum length")) code = 5;
    }
    fclose(f);
    return code;
}

/*  convergence_note — la interpretacion, en la salida y en la consola.        */
static void convergence_note(int code)
{
    const char *note = NULL, *head = NULL;

    switch (code) {
    case 1:
        head = "clean convergence";
        note = "the scaled gradient is at the tolerance.  This is the one to\n"
               "  trust.";
        break;
    case 2:
        head = "stopped on steptol, NOT on the gradient";
        note = "the step collapsed while the gradient may still be\n"
               "  appreciable.  Typical of an ill-conditioned likelihood (near\n"
               "  non-identification, common factors).  Treat the standard errors\n"
               "  with caution and re-estimate from other starting values or with\n"
               "  a smaller order.";
        break;
    case 3:
        head = "NOT a convergence: the line search failed to improve";
        note = "in drvec this is the COMMON outcome, and it is worth knowing why\n"
               "  it is not the same situation as in drtran.  There, termcode 3\n"
               "  usually means the fit started AT the optimum, having been seeded\n"
               "  from a .pre.  Here it means the surface is hard: measured, moving\n"
               "  Theta by hundredths can move the answer by units (docs/\n"
               "  CONVERGENCE.md).  The estimates are a stationary-ish point of an\n"
               "  exact likelihood, not a demonstrated maximum.  Cross-check with\n"
               "  -fixb2, compare |Sigma| across equivalent configurations, and\n"
               "  treat one run as evidence rather than as an answer.";
        break;
    case 4: case 5:
        head = "NOT a convergence: the optimiser gave up";
        note = "the estimates are not a maximum, and every criterion derived from\n"
               "  this fit -- standard errors, AIC/BIC, any LR statistic -- is\n"
               "  unreliable.";
        break;
    default:
        head = "the termination criterion could not be read";
        note = "no interpretation available.";
        break;
    }

    fprintf(outputv, "\nConvergence note: %s.\n  %s\n", head, note);
    fprintf(outputv,
        "  (Note that ifault above is MODEL adequacy, not convergence: a fit\n"
        "   that stopped short of an optimum can still report ifault = 0.)\n");
    if (!quiet_mode && code != 1)
        printf("  Convergence note: %s.  See the .out and docs/CONVERGENCE.md\n",
               head);
}

/*  load_alpha_A — lee la matriz A de la restriccion alpha = A*psi.
 *
 *  Formato, deliberadamente simple y ASCII: una primera linea con  M sa  y
 *  despues M filas de sa numeros.  Las lineas que empiezan por '*' o '#' son
 *  comentarios.  No se copia aqui el formato posicional de fue porque esto no
 *  es un fichero de la escalera: es una hipotesis del usuario.
 *
 *  Se comprueba el rango de A por su Gram: si A'A es singular la restriccion no
 *  identifica psi, y eso hay que decirlo antes de estimar y no despues.       */
static int load_alpha_A(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[1024];
    int mm = 0, sa = 0, i, j, got = 0;

    if (!f) { fprintf(stderr, "ERROR: no se pudo abrir %s\n", path); return 1; }
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '*' || line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "%d %d", &mm, &sa) == 2) { got = 1; break; }
    }
    if (!got || mm != nser || sa < 1 || sa > nser) {
        fprintf(stderr, "ERROR: %s debe empezar con 'M sa' con M = %d y "
                        "1 <= sa <= %d (leido %d %d)\n", path, nser, nser, mm, sa);
        fclose(f); return 1;
    }
    alpha_A  = matrix(1, nser, 1, sa);
    alpha_sa = sa;
    for (i = 1; i <= nser; i++) {
        char *tok;
        do { if (!fgets(line, sizeof line, f)) {
                 fprintf(stderr, "ERROR: %s se acaba en la fila %d\n", path, i);
                 fclose(f); return 1; }
        } while (line[0] == '*' || line[0] == '#' || line[0] == '\n');
        tok = strtok(line, " \t\n");
        for (j = 1; j <= sa; j++) {
            if (!tok) { fprintf(stderr, "ERROR: %s, fila %d: faltan columnas\n",
                                path, i); fclose(f); return 1; }
            alpha_A[i][j] = atof(tok);
            tok = strtok(NULL, " \t\n");
        }
    }
    fclose(f);

    {   /* rango de A via A'A */
        real **G = matrix(1, sa, 1, sa);
        real d1, d2; int ifc = 0;
        for (i = 1; i <= sa; i++) for (j = 1; j <= sa; j++) {
            real acc = 0.0; int k;
            for (k = 1; k <= nser; k++) acc += alpha_A[k][i] * alpha_A[k][j];
            G[i][j] = acc;
        }
        choldcp(G, sa, &d1, &d2, &ifc);
        free_matrix(G, 1, sa, 1, sa);
        if (ifc > 0) {
            fprintf(stderr, "ERROR: A no tiene rango %d (A'A es singular), asi "
                            "que psi no queda identificada\n", sa);
            return 1;
        }
    }
    return 0;
}

/*  build_weakex_A — la A que declara la ecuacion `eq` debilmente exogena.
 *  Es la identidad M x M sin su columna eq: alpha_eq = 0 para todo j.  La
 *  exogeneidad debil no necesita test propio, es H1(r) con esta A.            */
static int build_weakex_A(int eq)
{
    int i, j, c;
    if (eq < 1 || eq > nser) {
        fprintf(stderr, "ERROR: -weakex %d fuera de 1..%d\n", eq, nser);
        return 1;
    }
    alpha_sa = nser - 1;
    alpha_A  = matrix(1, nser, 1, (alpha_sa > 0 ? alpha_sa : 1));
    for (i = 1; i <= nser; i++)
        for (j = 1; j <= alpha_sa; j++) alpha_A[i][j] = 0.0;
    for (i = 1, c = 0; i <= nser; i++) {
        if (i == eq) continue;
        alpha_A[i][++c] = 1.0;
    }
    return 0;
}

/*****************************************************************************/
/*  F2 — el puente con la suite: drvec escribe .inp y lee .pre               */
/*                                                                           */
/*  Ver docs/PLAN_BETA.md F2 para el estudio completo.  Lo que gobierna este  */
/*  bloque, en tres frases:                                                  */
/*                                                                           */
/*   - drvec ESCRIBE .inp (una especificación) y nunca .pre (una afirmación   */
/*     de optimalidad, que sólo puede hacer quien estimó).                   */
/*   - El parser de fue es POSICIONAL y no valida nada, así que las secciones */
/*     van todas y en orden, incluida la de factores de la diferencia anual,  */
/*     que con datos anuales lleva un " 0" literal pero TIENE que estar.      */
/*   - Todo lo que se escribe es ASCII puro: el parser de Python de fue no    */
/*     lee Latin-1 (BUG-0010, abierto) y los fuentes de este motor lo son.    */
/*****************************************************************************/

static int   global_writeinp = 0;    /* -writeinp <prefijo>  (componentes de Ȳ) */
static int   global_writeres = 0;    /* -writeres <prefijo>  (residuos)         */
static char *inp_prefix      = NULL;
static int   global_eval     = 0;    /* -eval: evaluar y salir, sin optimizar */
static int   global_interv   = 0;    /* -interv <prefijo>: deterministas del .pre */
static char *interv_prefix   = NULL;
static int   global_multistart = 0;  /* -multistart n: n arranques, quedarse el mejor */

static int   global_seed     = 0;    /* -seed <prefijo> */
static char *pre_prefix      = NULL;

/*  Residuos de la regresión condicional, publicados por init_guess para que
 *  -writeres pueda escribirlos.  e_t = Θ(L)A_t.                              */
static real **cond_resid   = NULL;
static int    cond_resid_T = 0, cond_resid_M = 0;

/*  La semilla que se lee de los .pre: la DIAGONAL de Θ̄_k, k=1..q.
 *
 *  SÓLO Θ, y no por comodidad: el .pre **no lleva σ²** — comprobado sobre la
 *  gramática (FILE_CONTRACT.md: la varianza de innovaciones sólo aparece en
 *  ficheros fuf) y sobre un .pre real escrito por fue.  Así que las razones de
 *  Σ siguen saliendo de los residuos de la regresión condicional, donde F1 las
 *  puso.  Y el AR tampoco se siembra: los univariantes dan Φ*_k para k=1..p,
 *  pero el modelo sólo tiene F_1..F_{p-1} y Φ̄_p = −F_{p-1}C̄⁻¹H̄ queda
 *  determinada, así que con r ≥ 1 el sistema está sobredeterminado y no hay
 *  forma consistente de repartirlo.  Θ es justo lo que hoy arranca en cero.  */
static real **seed_tbar  = NULL;     /* [1..q][1..M]   diagonal de ThetaBar */
static real **seed_phi   = NULL;     /* [1..p-1][1..M] diagonal de Phi*      */
static real  *seed_var   = NULL;     /* [1..M]  sigma^2 de cada univariante  */
static real  *seed_logl  = NULL;     /* [1..M]  logL de cada univariante     */
static int    seed_have_uv = 0;      /* 1 si sigma2/logL se pudieron evaluar */
static real   seed_logl_sum = 0.0;   /* suma de las logL univariantes        */
static int    seed_loaded = 0;

/*  De qué ruta viene la semilla, que decide en qué coordenadas está:
 *
 *    SEED_RESID (-seed)      los .pre son de los RESIDUOS de la regresión
 *                            condicional.  e_t = Θ(L)A_t, así que la θ
 *                            univariante estima Θ DIRECTAMENTE, en coordenadas
 *                            de ∇Y.  No se transforma.
 *    SEED_YBAR  (-seedybar)  los .pre son de los COMPONENTES DE Ȳ.  Lo que un
 *                            univariante de Ȳ ve es Θ̄ = C̄ΘC̄⁻¹, así que hay
 *                            que deshacerlo: Θ_k = C̄⁻¹Θ̄_kC̄.
 *
 *  Confundir las dos es un error silencioso: la misma θ metida en la coordenada
 *  equivocada da otro modelo sin que nada proteste.  La ruta Ȳ está medida y es
 *  PEOR (ver PLAN_BETA.md F2.7); se conserva para poder reproducir la medida.  */
#define SEED_NONE  0
#define SEED_RESID 1
#define SEED_YBAR  2
static int seed_route = SEED_NONE;

/*  ybar_start_date — fecha de la primera observación de Ȳ.
 *  En el layout de niveles datamat[t] corresponde a rawmat[t+1], porque la
 *  primera observación se consume al diferenciar; con -differenced no hay
 *  desfase.  ObsToDate viene del puente (src/fue_bridge.c).                  */
static void ybar_start_date(int *year, int *sub)
{
    int first = global_levels ? 2 : 1;
    ObsToDate(data_start_year, data_start_sub, first, data_freq, year, sub);
}

/*  ar_ols_seed — semilla del AR de un componente: OLS sobre sus propios
 *  retardos.  No es una identificación (eso es ART); es un punto de partida
 *  mejor que una constante, y fue lo reestimará de todas formas.             */
static void ar_ols_seed(real *y, int n, int k, real *phi)
{
    int i, j, t;
    real mean = 0.0;
    for (t = 1; t <= n; t++) mean += y[t];
    mean /= n;
    for (i = 1; i <= k; i++) phi[i] = 0.0;
    if (k < 1 || n <= k + 1) return;

    real **XX = matrix(1, k, 1, k);
    real  *Xy = vector(1, k);
    int   *ind = ivector(1, k);
    for (i = 1; i <= k; i++) {
        for (j = 1; j <= k; j++) {
            real ss = 0.0;
            for (t = k + 1; t <= n; t++) ss += (y[t-i] - mean) * (y[t-j] - mean);
            XX[i][j] = ss;
        }
        Xy[i] = 0.0;
        for (t = k + 1; t <= n; t++) Xy[i] += (y[t-i] - mean) * (y[t] - mean);
    }
    /* Un componente degenerado (varianza nula) haría singular a XX; en ese caso
       se deja el AR en cero, que es una semilla legítima.                     */
    for (i = 1; i <= k; i++) if (XX[i][i] <= 1.0e-30) goto done;
    ludcp(XX, k, ind);
    lusol(XX, Xy, k, ind);
    for (i = 1; i <= k; i++) phi[i] = Xy[i];
done:
    free_ivector(ind, 1, k);
    free_vector(Xy, 1, k);
    free_matrix(XX, 1, k, 1, k);
}

/*  ascii_name — nombre de serie apto para un .inp: ASCII, sin espacios.      */
static void ascii_name(const char *src, const char *prefix, char *dst, int cap)
{
    int n = 0;
    while (*prefix && n < cap - 1) dst[n++] = *prefix++;
    while (src && *src && n < cap - 1) {
        unsigned char c = (unsigned char) *src++;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_') dst[n++] = (char) c;
    }
    if (n == 0) dst[n++] = 'y';
    dst[n] = '\0';
}

/*  write_inp_series — escribe UN .inp para una serie ya estacionaria.
 *
 *  par = orden del factor AR (0 = sin AR), qma = orden del factor MA.  Se piden
 *  como UN factor de orden k ("1 k") y no como k factores de primer orden
 *  ("k 1 1 ..."), para que los coeficientes del .pre mapeen directamente sobre
 *  phi_1..phi_k / theta_1..theta_k al expandirlos (FILE_CONTRACT.md 2.3).     */
static int write_inp_series(const char *path, const char *name,
                            real *col, int n, int year, int sub,
                            int par, int qma, int mu_free, const char *what)
{
    int t, k;
    real mean = 0.0, refactor;
    real *phi = vector(1, (par > 0 ? par : 1));
    FILE *f;

    for (t = 1; t <= n; t++) mean += col[t];
    mean /= n;

    /* refactor: la regla medida en la suite (drtran-python pre.py:check_scale y
       BRIDGE_DESIGN.md).  cdgrad usa un paso de diferencias finitas de ~6e-6
       ABSOLUTO, asi que una serie diminuta da un gradiente que es ruido; la
       banda comoda es |w| tipico entre 0.01 y 100 y el objetivo ~1.  Se mide
       sobre la propia serie y se redondea a potencia de diez.  Que cada
       componente lleve el suyo es inocuo porque lo unico que se siembra de
       vuelta es Theta, que es invariante de escala.                           */
    {
        real med = 0.0;
        int cnt = 0;
        for (t = 1; t <= n; t++) { real a = fabs(col[t]);
                                  if (a > 0.0) { med += a; cnt++; } }
        med = (cnt > 0) ? med / cnt : 1.0;
        refactor = (med > 0.0) ? pow(10.0, floor(log10(1.0 / med) + 0.5)) : 1.0;
        if (refactor < 1.0e-6) refactor = 1.0e-6;
        if (refactor > 1.0e+6) refactor = 1.0e+6;
    }

    f = fopen(path, "w");
    if (!f) { fprintf(stderr, "ERROR: cannot write %s\n", path);
              free_vector(phi, 1, (par > 0 ? par : 1)); return 1; }

    ar_ols_seed(col, n, par, phi);

    /* Cabecera libre: el parser salta lineas hasta el separador que dice
       "frequency".  Libre, pero ASCII (BUG-0010 de fue).                      */
    fprintf(f, "************************************************\n");
    fprintf(f, "* Input file for program FUE                   *\n");
    fprintf(f, "* written by drvec: %-26s *\n", what);
    fprintf(f, "************************************************\n");
    fprintf(f, "** Frequency of time series: either 1(A), 4(Q) or 12(M):\n");
    fprintf(f, " %d\n", data_freq);
    fprintf(f, "** Number of observations and starting date of time series:\n");
    fprintf(f, " %d %d %d %s\n", n, sub, year, name);
    fprintf(f, "** Number of deterministic variables"
               " (including seasonal components):\n0\n");
    fprintf(f, "** Number and orders of regular AR operators:\n");
    if (par > 0) {
        fprintf(f, "1 %d\n**\n", par);
        for (k = 1; k <= par; k++) fprintf(f, "%.6f  1\n", phi[k] * 1.0);
    } else fprintf(f, "0\n");
    fprintf(f, "** Number and orders of annual AR operators:\n0\n");
    fprintf(f, "** Number and orders of regular MA operators:\n");
    if (qma > 0) {
        fprintf(f, "1 %d\n**\n", qma);
        for (k = 1; k <= qma; k++) fprintf(f, "%.6f  1\n", 0.1);
    } else fprintf(f, "0\n");
    fprintf(f, "** Number and orders of anual MA operators:\n0\n");
    fprintf(f, "** Number and frequencies of regular AR(2) operators"
               " with fixed frequency:\n0\n");
    fprintf(f, "** Number and frequencies of regular MA(2) operators"
               " with fixed frequency:\n0\n");
    /* mu: la media de la variable YA diferenciada, que es lo que estas series
       son.  Sembrarla mal cuesta caro -- BUG-0012 de fue: un mu_0 de 2.5 contra
       una serie de media 17.06 deja a fue 6.86 de logL por debajo del optimo.
       Va escalada por refactor, como el resto de la serie.
       Y mu_free TIENE que seguir el caso determinista del modelo conjunto: si
       aqui se estima una media que el modelo conjunto no puede representar, la
       theta que devuelve fue esta condicionada a algo que no existe.  El
       formato distingue las dos cosas por el FLAG, no por el valor: "valor 1"
       es estimar y un unico "0" es que la media no forma parte del modelo.    */
    fprintf(f, "** Mean parameter (mu):\n");
    if (mu_free) fprintf(f, "%.6f  1\n", mean * refactor);
    else         fprintf(f, "0\n");
    /* Series ya estacionarias, y ya en logs si el .inp original lo estaba:
       identidad y cero diferencias.                                          */
    fprintf(f, "** Box-Cox lambda, regular differences and complete"
               " annual differences:\n1.00 0 0\n");
    /* Esta seccion la escriben SIEMPRE los dos escritores de fue (fue.c:3485 y
       report.py:1203) y el parser de Python la lee SIEMPRE: con datos anuales,
       un " 0" literal.  Omitirla desplaza todo lo que viene detras sin dar
       error -- que es el mismo fallo que tenia el lector en C (ver F2.1).     */
    fprintf(f, "** Individual factors of the annual difference"
               " (from freq 0.0): \n");
    if (data_freq > 1) {
        for (k = 0; k <= data_freq / 2; k++) fprintf(f, " 0");
        fprintf(f, "\n");
    } else fprintf(f, " 0\n");
    fprintf(f, "** ACF/PACF bands (0 Automatic) and reescaling factor: \n");
    fprintf(f, " 0.00 %.2f\n", refactor);
    fprintf(f, "** Time series (stochastic and non-standard deterministic"
               " variables): \n");
    /* Los datos van CRUDOS: refactor es una directiva y fue lo aplica el mismo
       (w = refactor * BoxCox(z), BRIDGE_DESIGN.md).  Pre-multiplicarlos aqui lo
       aplicaria dos veces.  mu si va escalada, porque es la media de w.       */
    for (t = 1; t <= n; t++) fprintf(f, "%.10f\n", col[t]);
    fclose(f);

    if (!quiet_mode)
        printf("  escrito %s  (%s, n=%d, ARMA(%d,%d), refactor=%.2f)\n",
               path, name, n, par, qma, refactor);
    free_vector(phi, 1, (par > 0 ? par : 1));
    return 0;
}

/*  write_component_inps — un .inp por COMPONENTE DE Ybar.
 *
 *  El modelo que se pide es ARMA(p-1, q) con media: p es el orden AR sobre Ybar,
 *  luego sobre nabla Y el orden efectivo es p-1 (seccion 2 del plan).
 *
 *  AVISO MEDIDO: sembrar Theta desde estos ficheros EMPEORA el ajuste (ver
 *  PLAN_BETA.md F2.7).  El marginal univariante de un componente de un VARMA no
 *  es Theta_ii: marginalizar mezcla AR y MA e infla los ordenes.  Este modo se
 *  conserva porque es la unidad natural para que ART identifique el modelo
 *  -- que es informacion sobre p y q --, no como fuente de semilla.  Para eso
 *  esta -writeres.                                                            */
static int write_component_inps(const char *prefix)
{
    int M = nser, r = global_r, s = M - r;
    int p = global_p, q = global_q;
    int par = (p > 1) ? p - 1 : 0;
    int i, t, year, sub, nbad = 0;

    real **B2   = matrix(1, s, 1, (r > 0 ? r : 1));
    real **Ybar = matrix(1, nobs, 1, M);
    real  *col  = vector(1, nobs);
    prelim_b2(B2);
    build_ybar(B2, Ybar);
    ybar_start_date(&year, &sub);

    for (i = 1; i <= M; i++) {
        char name[64], path[1024], what[64];
        for (t = 1; t <= nobs; t++) col[t] = Ybar[t][i];
        if (i <= s) ascii_name(series_names[i], "d", name, sizeof name);
        else        ascii_name(series_names[i], "W", name, sizeof name);
        snprintf(path, sizeof path, "%s.%d.inp", prefix, i);
        snprintf(what, sizeof what, "Ybar component %d", i);
        /* Caso 1: E[nabla Y2] = 0 y E[W] = 0, luego ninguna media.
           Caso 2: E[W] != 0 pero E[nabla Y2] = 0.
           Caso 3: las dos libres.  (Mauricio 2006, Remark 6.)                */
        {
            int mu_free = (global_case == 3) ||
                          (global_case == 2 && i > s);
            nbad += write_inp_series(path, name, col, nobs, year, sub,
                                     par, q, mu_free, what);
        }
    }

    free_vector(col, 1, nobs);
    free_matrix(Ybar, 1, nobs, 1, M);
    free_matrix(B2, 1, s, 1, (r > 0 ? r : 1));
    return nbad ? 1 : 0;
}

/*  write_resid_inps — un .inp por componente de los RESIDUOS de la regresion
 *  condicional.  Esta es la ruta buena para sembrar Theta, y la razon es de
 *  fondo: los residuos son e_t = Theta(L)A_t, cuyo marginal por componente es
 *  MA(q) EXACTAMENTE, sin la inflacion de orden del marginal de Ybar.  Asi que
 *  se pide ARMA(0, q) -- MA puro, sin AR, que ya se ha descontado.
 *
 *  Requiere que init_guess se haya ejecutado (es quien publica cond_resid).    */
static int write_resid_inps(const char *prefix)
{
    int M = nser, q = global_q, p = global_p;
    int i, t, year, sub, nbad = 0;
    int T = cond_resid_T;
    real *col;

    if (!cond_resid || T < 4) {
        fprintf(stderr, "ERROR: no hay residuos que escribir\n");
        return 1;
    }
    /* Los residuos empiezan en t = p+1 sobre el indice de Ybar. */
    {
        int first = (global_levels ? 2 : 1) + p;
        ObsToDate(data_start_year, data_start_sub, first, data_freq, &year, &sub);
    }
    col = vector(1, T);
    for (i = 1; i <= M; i++) {
        char name[64], path[1024], what[64];
        for (t = 1; t <= T; t++) col[t] = cond_resid[t][i];
        ascii_name(series_names[i], "e", name, sizeof name);
        snprintf(path, sizeof path, "%s.%d.inp", prefix, i);
        snprintf(what, sizeof what, "residual %d, MA(%d)", i, q);
        /* La media del residuo es una molestia del ajuste preliminar --- la
           regresion condicional no lleva constante --- y no un parametro del
           modelo conjunto, asi que en general va libre: lo unico que se quiere
           de aqui es la estructura MA.
           EXCEPTO en el caso 1, donde el modelo conjunto NO ADMITE MEDIA
           ninguna (E[nabla Y2] = 0 y E[W] = 0).  Dejarla libre alli devuelve una
           theta condicionada a algo que el modelo no puede representar, y esta
           medido: con mu libre la semilla arranca 3.40 por debajo del arranque
           en frio, y fijandola en 0 recupera 0.53 de esos 3.40.  Ver F2.7.    */
        nbad += write_inp_series(path, name, col, T, year, sub, 0, q,
                                 (global_case != 1), what);
    }
    free_vector(col, 1, T);
    return nbad ? 1 : 0;
}

/*  load_seed_pre — lee un .pre por componente y deja en seed_tbar la diagonal
 *  de Θ̄_k.  Devuelve 0 si pudo leer los M ficheros.
 *
 *  Los factores del .pre se expanden con expand_ma_factors, que viene de
 *  drtran y es el espejo en C del _unscramble del cast de fue: el .pre guarda
 *  operadores FACTORIZADOS, y "2 1 1" (dos factores de primer orden) no es
 *  "1 2" (uno de segundo).  Expandir con la rutina de la suite, en vez de
 *  reimplementar la convolución, es lo que garantiza que drvec lea el mismo
 *  modelo que fue estimó.                                                    */
/*  pre_univariate — evalúa el modelo de UN `.pre` sobre su propia serie y
 *  devuelve su log-verosimilitud exacta y su σ².
 *
 *  El `.pre` no lleva σ² — eso es cierto y está comprobado sobre el formato —
 *  pero **sí lleva el modelo y los datos**, así que σ² es *derivable*: basta
 *  evaluar la verosimilitud univariante con el mismo `elf` que usa toda la
 *  suite.  Decir «no se puede sembrar Σ desde el .pre» era quedarse en la
 *  primera mitad del argumento.
 *
 *  Y de paso da lo que hace falta para los dos contratos de la escalera
 *  (`drtran-python/docs/LADDER_AS_OPTIMISATION.md` §2.1 y §3): la suma de las
 *  logL univariantes, que con r = 0 y estructura diagonal debe coincidir con la
 *  conjunta, y el certificado de optimalidad, que es la brecha entre evaluar y
 *  ajustar.
 *
 *  σ² sale en las unidades REESCALADAS (w = refactor·z), así que se devuelve
 *  dividido por refactor² para que las razones entre componentes con distinto
 *  refactor sean comparables.  Se rechaza lo que no sabemos manejar: Box-Cox
 *  distinto de la identidad, diferencias, o deterministas.                    */
static int pre_univariate(struct Tusmodel *Tm, struct Tseries *Ts,
                          real *logl_out, real *sigma2_out)
{
    const real LOG2PI = 1.837877066;
    int n = Ts->nobs, p = 0, q = 0, k, t, ifault = 0;
    real pi1, pi2, pi3, refac = (Ts->refactor != 0.0) ? Ts->refactor : 1.0;
    struct Tvarma uv;

    if (Tm->NdetVar != 0 || Tm->nrdiff != 0 || Tm->nadiff != 0
        || Tm->boxlam != 1.0) {
        fprintf(stderr, "WARNING: el .pre lleva deterministas, diferencias o "
                        "Box-Cox; no se evalua su sigma2\n");
        return 1;
    }
    for (k = 1; k <= Tm->NumAr1; k++) p += Tm->p1[k];
    for (k = 1; k <= Tm->NumMa1; k++) q += Tm->q1[k];

    uv.m = 1; uv.n = n; uv.p = p; uv.q = q;
    uv.xitol = 1.0e-3;
    uv.mu    = vector(1, 1);
    uv.phi   = tensor(0, (p > 0 ? p : 1), 1, 1, 1, 1);
    uv.theta = tensor(0, (q > 0 ? q : 1), 1, 1, 1, 1);
    uv.qq    = matrix(1, 1, 1, 1);
    uv.w     = matrix(1, n, 1, 1);
    uv.a     = matrix(1, n, 1, 1);

    uv.mu[1]      = (Tm->Imu ? Tm->mu : 0.0);
    uv.qq[1][1]   = 1.0;
    uv.phi[0][1][1]   = 1.0;
    uv.theta[0][1][1] = 1.0;
    if (p > 0) { real *ph = vector(1, p);
                 for (k = 1; k <= p; k++) ph[k] = 0.0;
                 expand_ar_factors(Tm, ph, p);
                 for (k = 1; k <= p; k++) uv.phi[k][1][1] = ph[k];
                 free_vector(ph, 1, p); }
    if (q > 0) { real *th = vector(1, q);
                 for (k = 1; k <= q; k++) th[k] = 0.0;
                 expand_ma_factors(Tm, th, q);
                 for (k = 1; k <= q; k++) uv.theta[k][1][1] = th[k];
                 free_vector(th, 1, q); }
    for (t = 1; t <= n; t++) uv.w[t][1] = Ts->data[t] * refac;

    elf(uv.m, uv.n, uv.p, uv.q, uv.mu, uv.phi, uv.theta, uv.qq, uv.w,
        1.0, uv.xitol, TRUE, uv.a, &pi1, &pi2, &pi3, &ifault);

    if (ifault == 0) {
        *logl_out = -0.5 * uv.m * uv.n * (LOG2PI - log((real) uv.m)
                    - log((real) uv.n) + 1.0)
                    - 0.5 * uv.n * (uv.m * log(pi1) + log(pi2));
        /* De vuelta a las unidades ORIGINALES.  fue estima sobre w = refactor*z,
           y una logL no es invariante de escala: hay que quitarle el jacobiano
           n*log(refactor).  Sin esto la suma de univariantes no es comparable
           con la conjunta y la identidad de cruce parece fallar por cientos de
           unidades (en mink-muskrat, por 2*61*log(10) = 280.92).
           El signo: si w = c*z entonces p_z(z) = c^n * p_w(w), luego
           logL_z = logL_w + n*log(c).                                        */
        *logl_out += uv.n * log(refac);
        *sigma2_out = (pi1 / (uv.n * uv.m)) / (refac * refac);
    }

    free_matrix(uv.a, 1, n, 1, 1);
    free_matrix(uv.w, 1, n, 1, 1);
    free_matrix(uv.qq, 1, 1, 1, 1);
    free_tensor(uv.theta, 0, (q > 0 ? q : 1), 1, 1, 1, 1);
    free_tensor(uv.phi,   0, (p > 0 ? p : 1), 1, 1, 1, 1);
    free_vector(uv.mu, 1, 1);
    return (ifault == 0) ? 0 : 1;
}

/*  Cada .pre leido se libera con free_fue_pre (src/fue_bridge.c).  El lector no
 *  trae desasignador -- ni aqui ni en drtran, que es su BUG-12 --, asi que se
 *  escribio uno; no liberar es un fallo aunque el programa termine enseguida, y
 *  el mismo lector se usa desde procesos que no terminan.                    */
static int load_seed_pre(const char *prefix)
{
    int M = nser, q = global_q;
    int i, k;

    int nf = (global_p > 1) ? global_p - 1 : 0;

    seed_tbar = matrix(1, (q  > 0 ? q  : 1), 1, M);
    seed_phi  = matrix(1, (nf > 0 ? nf : 1), 1, M);
    seed_var  = vector(1, M);
    seed_logl = vector(1, M);
    for (k = 1; k <= q;  k++) for (i = 1; i <= M; i++) seed_tbar[k][i] = 0.0;
    for (k = 1; k <= nf; k++) for (i = 1; i <= M; i++) seed_phi[k][i]  = 0.0;
    for (i = 1; i <= M; i++) { seed_var[i] = 1.0; seed_logl[i] = 0.0; }
    seed_have_uv = 1;

    for (i = 1; i <= M; i++) {
        char path[1024];
        struct Tusmodel Tm;
        struct Tseries  Ts;
        real **DataMat = NULL;
        int qpre;

        /* Se prueba .pre y si no está, .inp.  Los dos son el MISMO formato y
           distinta afirmación: el .pre dice «esto es un óptimo» y el .inp «esto
           es una especificación» (FILE_CONTRACT.md §4).  Aceptar los dos es lo
           que hacen el lector de fue y el load_pre de drtran, y es lo que
           permite probar la siembra con un fichero retocado a mano sin tener
           que fabricar un .pre, que sería afirmar un óptimo que no existe.    */
        snprintf(path, sizeof path, "%s.%d.pre", prefix, i);
        if (read_fue_pre(path, &Tm, &Ts, &DataMat) != 0) {
            char alt[1024];
            snprintf(alt, sizeof alt, "%s.%d.inp", prefix, i);
            if (read_fue_pre(alt, &Tm, &Ts, &DataMat) != 0) {
                fprintf(stderr, "ERROR: no se pudo leer %s ni %s\n", path, alt);
                free_matrix(seed_tbar, 1, q, 1, M); seed_tbar = NULL;
                return 1;
            }
            if (!quiet_mode)
                printf("  (%s no está; se usa %s, que es una especificación"
                       " y no un óptimo)\n", path, alt);
        }
        if (Ts.nobs != nobs)
            fprintf(stderr, "WARNING: %s trae %d observaciones y Ȳ tiene %d\n",
                    path, Ts.nobs, nobs);

        /* Orden MA que trae el fichero, sumando los factores regulares. */
        qpre = 0;
        for (k = 1; k <= Tm.NumMa1; k++) qpre += Tm.q1[k];
        if (qpre > 0) {
            real *th = vector(1, qpre);
            for (k = 1; k <= qpre; k++) th[k] = 0.0;
            expand_ma_factors(&Tm, th, qpre);
            for (k = 1; k <= q && k <= qpre; k++) seed_tbar[k][i] = th[k];
            free_vector(th, 1, qpre);
        }
        if (qpre != q)
            fprintf(stderr, "WARNING: %s trae MA de orden %d y el modelo pide %d;"
                            " los retardos que falten se siembran en 0\n",
                    path, qpre, q);

        /* AR: la diagonal de Phi*.  Con r = 0 se tiene Phi*_k = F_k, asi que
           esto siembra F directamente; con r >= 1 hay que deshacer Cbar, que es
           lo que hace init_guess.                                            */
        {
            int ppre = 0;
            for (k = 1; k <= Tm.NumAr1; k++) ppre += Tm.p1[k];
            if (ppre > 0) {
                real *ph = vector(1, ppre);
                for (k = 1; k <= ppre; k++) ph[k] = 0.0;
                expand_ar_factors(&Tm, ph, ppre);
                for (k = 1; k <= nf && k <= ppre; k++) seed_phi[k][i] = ph[k];
                free_vector(ph, 1, ppre);
            }
            if (ppre != nf)
                fprintf(stderr, "WARNING: %s trae AR de orden %d y el modelo "
                                "pide %d\n", path, ppre, nf);
        }

        /* sigma^2 y logL: NO estan en el fichero, pero el fichero trae el
           modelo Y los datos, asi que se derivan evaluando con elf.          */
        {
            real ll = 0.0, s2 = 1.0;
            if (pre_univariate(&Tm, &Ts, &ll, &s2) == 0) {
                seed_logl[i] = ll; seed_var[i] = s2;
            } else {
                seed_have_uv = 0;
            }
        }
        free_fue_pre(&Tm, &Ts, DataMat);
    }

    if (seed_have_uv) {
        real tot = 0.0;
        for (i = 1; i <= M; i++) tot += seed_logl[i];
        seed_logl_sum = tot;
        if (!quiet_mode) {
            printf("  univariantes del .pre:");
            for (i = 1; i <= M; i++) printf(" %.6f", seed_logl[i]);
            printf("   suma = %.6f\n", tot);
        }
    }
    seed_loaded = 1;
    return 0;
}

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
    prelim_b2(B2);

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
    /* Residuos de la regresión condicional, y de ahí Σ.
       Antes se recalculaba e_t dentro del doble bucle de (i,j), lo que repetía
       el mismo cálculo M² veces; ahora se calcula UNA vez, en el mismo orden de
       restas, así que el resultado es idéntico bit a bit.  Se guardan además en
       cond_resid porque son lo que necesita -writeres: e_t = Θ(L)A_t, luego el
       marginal de cada componente es MA(q) EXACTAMENTE — sin la inflación de
       orden que sufre el marginal de un componente de Ȳ.                      */
    real **E = matrix(1, T, 1, M);
    for (t = 1; t <= T; t++)
        for (i = 1; i <= M; i++) {
            real ei = Ydep[t][i];
            for (int c = 1; c <= nreg; c++) {
                real bi = (c <= r) ? -Lambda[i][c] : F[(c-r-1)/M+1][i][(c-r-1)%M+1];
                ei -= bi * X[t][c];
            }
            E[t][i] = ei;
        }
    real **Sig = matrix(1, M, 1, M);
    for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) {
        real ss = 0.0;
        for (t = 1; t <= T; t++) ss += E[t][i] * E[t][j];
        Sig[i][j] = (T > 0) ? ss / T : 1.0;
    }
    /* Publicar los residuos para -writeres.  Los posee este módulo. */
    if (cond_resid) free_matrix(cond_resid, 1, cond_resid_T, 1, cond_resid_M);
    cond_resid = matrix(1, T, 1, M);
    cond_resid_T = T; cond_resid_M = M;
    for (t = 1; t <= T; t++) for (i = 1; i <= M; i++) cond_resid[t][i] = E[t][i];
    free_matrix(E, 1, T, 1, M);

    /* --- 4b. La semilla del .pre, llevada a las coordenadas de drvec -------
       Solo para la ruta Ybar: los .pre describen los COMPONENTES DE Ybar, y el
       modelo esta parametrizado en Lambda / F / Theta / Sigma sobre nabla Y.
       Las tres vueltas, con Phi*_k = Cbar*PhiBar_k y Theta*_k = Cbar*Theta_k*Cinv:

         Theta_k = Cinv * diag(theta_k) * Cbar
         F_1     = (Cinv*diag(phi_1) - Cinv*Hbar + LamBar) * Cbar
         F_i     = (Cinv*diag(phi_i) + F_{i-1}*Cinv*Hbar) * Cbar     i = 2..p-1
         Sigma   = Cinv * diag(sigma^2) * Cinv'

       Con r = 0 todo esto colapsa a la identidad (Cbar = I, Hbar = 0), que es
       el peldano diagonal donde viven los contratos de la escalera.
       Phi*_p queda determinada por F_{p-1} y no se puede imponer, asi que con
       r >= 1 el AR esta sobredeterminado y esto es una proyeccion, no una
       vuelta exacta.  Ver docs/PLAN_BETA.md F2.8.                            */
    real ***Fseed = NULL, ***Tseed = NULL, **Sigseed = NULL;
    if (seed_loaded && seed_route == SEED_YBAR) {
        real **Cb = matrix(1, M, 1, M), **Ci = matrix(1, M, 1, M);
        real **Hb = matrix(1, M, 1, M), **Lb = matrix(1, M, 1, M);
        real **T1 = matrix(1, M, 1, M), **T2 = matrix(1, M, 1, M);
        int a, b;
        for (a = 1; a <= M; a++) for (b = 1; b <= M; b++) {
            Cb[a][b] = 0.0; Ci[a][b] = 0.0; Hb[a][b] = 0.0; Lb[a][b] = 0.0; }
        for (a = 1; a <= s; a++) Cb[a][r + a] = 1.0;
        for (b = 1; b <= r; b++) Cb[s + b][b] = 1.0;
        for (b = 1; b <= r; b++) for (a = 1; a <= s; a++) Cb[s + b][r + a] = B2[a][b];
        for (a = 1; a <= r; a++) for (b = 1; b <= s; b++) Ci[a][b] = -B2[b][a];
        for (a = 1; a <= r; a++) Ci[a][s + a] = 1.0;
        for (a = 1; a <= s; a++) Ci[r + a][a] = 1.0;
        for (a = 1; a <= r; a++) Hb[s + a][s + a] = 1.0;
        for (a = 1; a <= M; a++) for (b = 1; b <= r; b++) Lb[a][s + b] = Lambda[a][b];

        if (q > 0) {
            Tseed = tensor(1, q, 1, M, 1, M);
            for (k = 1; k <= q; k++) {
                for (a = 1; a <= M; a++) for (b = 1; b <= M; b++)
                    T1[a][b] = (a == b) ? seed_tbar[k][a] : 0.0;
                matrix_multiply(Ci, T1, T2, M, M, M);
                matrix_multiply(T2, Cb, Tseed[k], M, M, M);
            }
        }
        if (nf > 0) {
            real **CiH = matrix(1, M, 1, M), **W1 = matrix(1, M, 1, M);
            matrix_multiply(Ci, Hb, CiH, M, M, M);
            Fseed = tensor(1, nf, 1, M, 1, M);
            for (k = 1; k <= nf; k++) {
                for (a = 1; a <= M; a++) for (b = 1; b <= M; b++)
                    T1[a][b] = (a == b) ? seed_phi[k][a] : 0.0;
                matrix_multiply(Ci, T1, W1, M, M, M);        /* Cinv*diag(phi) */
                if (k == 1) {
                    for (a = 1; a <= M; a++) for (b = 1; b <= M; b++)
                        W1[a][b] += -CiH[a][b] + Lb[a][b];
                } else {
                    matrix_multiply(Fseed[k-1], CiH, T2, M, M, M);
                    for (a = 1; a <= M; a++) for (b = 1; b <= M; b++)
                        W1[a][b] += T2[a][b];
                }
                matrix_multiply(W1, Cb, Fseed[k], M, M, M);
            }
            free_matrix(W1, 1, M, 1, M);
            free_matrix(CiH, 1, M, 1, M);
        }
        if (seed_have_uv) {
            Sigseed = matrix(1, M, 1, M);
            for (a = 1; a <= M; a++) for (b = 1; b <= M; b++)
                T1[a][b] = (a == b) ? seed_var[a] : 0.0;
            matrix_multiply(Ci, T1, T2, M, M, M);
            for (a = 1; a <= M; a++) for (b = 1; b <= M; b++) {
                real acc = 0.0;
                for (k = 1; k <= M; k++) acc += T2[a][k] * Ci[b][k];  /* *Cinv' */
                Sigseed[a][b] = acc;
            }
        }
        free_matrix(T2, 1, M, 1, M); free_matrix(T1, 1, M, 1, M);
        free_matrix(Lb, 1, M, 1, M); free_matrix(Hb, 1, M, 1, M);
        free_matrix(Ci, 1, M, 1, M); free_matrix(Cb, 1, M, 1, M);
    }

    /* --- 5. Write x[] in the canonical VEC order ------------------------- */
    if (global_case == 2) { for (j = 1; j <= r; j++) x[idx++] = EW[j]; }
    else if (global_case == 3) {
        for (i = 1; i <= s; i++) x[idx++] = EdY2[i];
        for (j = 1; j <= r; j++) x[idx++] = EW[j];
    }
    if (global_alpha) {
        /* psi = (A'A)^-1 A' Lambda_ols: la proyeccion de la semilla libre sobre
           el subespacio que la restriccion permite.  Es la mejor semilla
           disponible y no cuesta nada.                                        */
        real **AtA = matrix(1, alpha_sa, 1, alpha_sa);
        real **psi_seed = matrix(1, alpha_sa, 1, (r > 0 ? r : 1));
        real  *AtL = vector(1, alpha_sa);
        int   *ind = ivector(1, alpha_sa);
        int    a, b;
        for (a = 1; a <= alpha_sa; a++)
            for (b = 1; b <= alpha_sa; b++) {
                real acc = 0.0;
                for (i = 1; i <= M; i++) acc += alpha_A[i][a] * alpha_A[i][b];
                AtA[a][b] = acc;
            }
        ludcp(AtA, alpha_sa, ind);
        for (j = 1; j <= r; j++) {
            for (a = 1; a <= alpha_sa; a++) {
                real acc = 0.0;
                for (i = 1; i <= M; i++) acc += alpha_A[i][a] * Lambda[i][j];
                AtL[a] = acc;
            }
            lusol(AtA, AtL, alpha_sa, ind);
            for (a = 1; a <= alpha_sa; a++) psi_seed[a][j] = AtL[a];
        }
        for (a = 1; a <= alpha_sa; a++)
            for (j = 1; j <= r; j++) x[idx++] = psi_seed[a][j];
        free_ivector(ind, 1, alpha_sa);
        free_vector(AtL, 1, alpha_sa);
        free_matrix(psi_seed, 1, alpha_sa, 1, (r > 0 ? r : 1));
        free_matrix(AtA, 1, alpha_sa, 1, alpha_sa);
    } else {
        for (i = 1; i <= M; i++) for (j = 1; j <= r; j++) x[idx++] = Lambda[i][j];
    }
    for (k = 1; k <= nf; k++) {
        real **Fk = (Fseed ? Fseed[k] : F[k]);
        if (global_diag_ar) { for (i = 1; i <= M; i++) x[idx++] = Fk[i][i]; }
        else { for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) x[idx++] = Fk[i][j]; }
    }
    /* Bloque MA.  Sin -seed arranca en CERO EXACTO, que es el único hueco real
       del arranque en frío (todo lo demás sale de datos: B₂ por OLS, Λ y F por
       la regresión condicional, Σ de sus residuos).
       Con -seed se usa lo estimado por fue sobre cada componente de Ȳ, y hay
       que devolverlo a las coordenadas de drvec: lo que un univariante de Ȳ ve
       es Θ̄ = C̄ΘC̄⁻¹ —así lo monta vec_shootx en armax->theta—, luego
                              Θ_k = C̄⁻¹ Θ̄_k C̄
       con Θ̄_k diagonal.  Sembrar los θ del .pre directamente en Θ funciona
       sólo si C̄ = I (r = 0) y es un error silencioso en cuanto r ≥ 1.        */
    if (seed_loaded && q > 0 && seed_route == SEED_RESID) {
        /* Ruta de residuos: la θ leída ES la diagonal de Θ, sin transformar. */
        for (k = 1; k <= q; k++) {
            if (global_diag_ma) {
                for (i = 1; i <= M; i++) x[idx++] = seed_tbar[k][i];
            } else {
                for (i = 1; i <= M; i++)
                    for (j = 1; j <= M; j++)
                        x[idx++] = (i == j) ? seed_tbar[k][i] : 0.0;
            }
        }
    } else if (seed_loaded && q > 0 && seed_route == SEED_YBAR && Tseed) {
        /* Ruta Ybar: Theta_k = Cinv * diag(theta_k) * Cbar, ya montada arriba. */
        for (k = 1; k <= q; k++) {
            if (global_diag_ma) {
                /* Con -diagma solo la diagonal de Theta_k es parametro, y
                   Cinv*diag(.)*Cbar no es diagonal en general: se toma su
                   diagonal, que es una aproximacion y no la vuelta exacta.    */
                for (i = 1; i <= M; i++) x[idx++] = Tseed[k][i][i];
            } else {
                for (i = 1; i <= M; i++)
                    for (j = 1; j <= M; j++) x[idx++] = Tseed[k][i][j];
            }
        }
    } else {
        for (k = 1; k <= q; k++) {
            if (global_diag_ma) { for (i = 1; i <= M; i++) x[idx++] = 0.0; }
            else { for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) x[idx++] = 0.0; }
        }
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
    /* Con la semilla del .pre, Sigma sale de las varianzas univariantes
       (derivadas evaluando cada .pre con elf) en vez de los residuos de la
       regresion condicional.  Es lo que cierra el bloque univariante entero:
       sembrar solo Theta deja un punto que no es el optimo de nadie.         */
    if (Sigseed) {
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) Sig[i][j] = Sigseed[i][j];
    }
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

    if (Sigseed) free_matrix(Sigseed, 1, M, 1, M);
    if (Tseed)   free_tensor(Tseed, 1, q, 1, M, 1, M);
    if (Fseed)   free_tensor(Fseed, 1, nf, 1, M, 1, M);
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
    if (global_alpha) {
        /* Lambda = A * psi.  psi (sa x r) es lo que ve el optimizador; A es
           dato del usuario.  Mismo patron que -fixb2: la restriccion vive en el
           cast, no en el optimizador.                                        */
        real **psi = matrix(1, alpha_sa, 1, (r > 0 ? r : 1));
        for (i = 1; i <= alpha_sa; i++)
            for (j = 1; j <= r; j++) psi[i][j] = x[idx++];
        for (i = 1; i <= M; i++)
            for (j = 1; j <= r; j++) {
                real acc = 0.0;
                for (int kk = 1; kk <= alpha_sa; kk++) acc += alpha_A[i][kk] * psi[kk][j];
                Lambda[i][j] = acc;
            }
        free_matrix(psi, 1, alpha_sa, 1, (r > 0 ? r : 1));
    } else {
        for (i = 1; i <= M; i++)
            for (j = 1; j <= r; j++)
                Lambda[i][j] = x[idx++];
    }

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
        else if (strcmp(argv[i], "-writeinp") == 0 && i+1 < argc) {
            global_writeinp = 1; inp_prefix = argv[++i];
        }
        else if (strcmp(argv[i], "-writeres") == 0 && i+1 < argc) {
            global_writeres = 1; inp_prefix = argv[++i];
        }
        else if (strcmp(argv[i], "-eval") == 0) global_eval = 1;
        else if (strcmp(argv[i], "-interv") == 0 && i+1 < argc) {
            global_interv = 1; interv_prefix = argv[++i];
        }
        else if (strcmp(argv[i], "-multistart") == 0 && i+1 < argc)
            global_multistart = atoi(argv[++i]);
        else if (strcmp(argv[i], "-alpha") == 0 && i+1 < argc) {
            global_alpha = 1; alpha_file = argv[++i];
        }
        else if (strcmp(argv[i], "-weakex") == 0 && i+1 < argc) {
            global_alpha = 1; alpha_weakex = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-seed") == 0 && i+1 < argc) {
            global_seed = 1; seed_route = SEED_RESID; pre_prefix = argv[++i];
        }
        else if (strcmp(argv[i], "-seedybar") == 0 && i+1 < argc) {
            global_seed = 1; seed_route = SEED_YBAR;  pre_prefix = argv[++i];
        }
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
    } else if (global_r < 0) {
        fprintf(stderr, "ERROR: cointegration rank r must be >= 0\n");
        exit(1);
    } else if (global_r == 0 && !quiet_mode) {
        /* r = 0 ya no es un error.  Es el PELDANO DIAGONAL de la escalera:
           con r = 0 se tiene Cbar = I y Hbar = 0, la verosimilitud exacta
           factoriza con las banderas diagonales, y ahi es donde viven los dos
           contratos de la suite (LADDER_AS_OPTIMISATION.md 2.1 y 3): la
           identidad de cruce y el certificado de optimalidad.  Antes solo se
           llegaba a el por dentro de -lrtest, que es justamente donde no se
           puede inspeccionar.  Ver docs/PLAN_BETA.md F2.8.                   */
        printf("r = 0: sin cointegracion, VARMA(%d,%d) sobre nabla Y "
               "(el peldano diagonal)\n", global_p, global_q);
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

    /* -alpha / -weakex: cargar la A de la restriccion alpha = A*psi.  Se hace
       aqui porque necesita nser, y antes de calcular npar.                   */
    if (global_alpha) {
        int bad = alpha_weakex ? build_weakex_A(alpha_weakex)
                               : load_alpha_A(alpha_file);
        if (bad) exit(1);
        if (global_r < 1) {
            fprintf(stderr, "ERROR: alpha = A*psi no significa nada con r = 0 "
                            "(no hay termino de correccion de error)\n");
            exit(1);
        }
        printf("Restriccion H1(r): alpha = A*psi, con A de %d x %d%s\n",
               nser, alpha_sa,
               alpha_weakex ? " (exogeneidad debil)" : "");
    }

    /* -writeinp: emitir un .inp por componente de Ȳ y parar.  Es un modo, no un
       añadido a la estimación: el siguiente paso de la escalera lo da fue.     */
    if (global_writeinp) {
        int bad;
        printf("Escribiendo un .inp por componente de Ȳ (para ART/fue):\n");
        bad = write_component_inps(inp_prefix);
        if (bad) { fprintf(stderr, "ERROR: no se pudieron escribir todos\n"); exit(1); }
        printf("Ahora: para cada fichero, 'python -m fue %s.<i> eml' (o ART),\n"
               "y despues drvec ... -seed %s\n", inp_prefix, inp_prefix);
        exit(0);
    }

    /* -seed: leer los .pre y sembrar el bloque MA (lo unico que el .pre puede
       sembrar; ver load_seed_pre).                                            */
    if (global_seed) {
        /* La informacion univariante llega al PELDANO DIAGONAL y no mas arriba.
           Con r >= 1 el marginal de un componente de Ybar no es el bloque
           diagonal del conjunto, Cbar y Lambda acoplan, y PhiBar_p queda
           determinada por F_{p-1}, asi que el AR esta sobredeterminado.  Esta
           medido: en -case 2 la semilla arranca 17 unidades por debajo del
           arranque en frio.  Se avisa en vez de prohibir, porque la medida hay
           que poder reproducirla.  Ver docs/PLAN_BETA.md F2.8.               */
        if (global_r > 0 && seed_route == SEED_YBAR)
            fprintf(stderr,
                "WARNING: -seedybar con r = %d.  La informacion univariante solo\n"
                "         transporta un optimo en el peldano diagonal (r = 0);\n"
                "         con r >= 1 empeora el punto de partida.  Medido en\n"
                "         docs/PLAN_BETA.md F2.8.\n", global_r);
        if (load_seed_pre(pre_prefix) != 0)
            fprintf(stderr, "WARNING: sin semilla; se arranca en frio\n");
        else
            printf("Semilla MA leida de %s.<1..%d>.pre (ruta %s)\n",
                   pre_prefix, nser,
                   seed_route == SEED_RESID ? "residuos" : "componentes de Ybar");
    }

    /* [2] Open output ------------------------------------------------------ */
    outputv = fopen(outputf, "w");
    if (!outputv) { fprintf(stderr, "ERROR: cannot write %s\n", outputf); exit(1); }

    fprintf(outputv, "DRVEC — VEC(%d) EML Estimation (Mauricio 2006)\n", global_r);
    fprintf(outputv, "==============================================\n\n");
    fprintf(outputv, "Input  : %s\n", inputf);
    fprintf(outputv, "M = %d, r = %d, s = M-r = %d\n", nser, global_r, nser - global_r);
    fprintf(outputv, "Stationary VARMA(%d,%d) on Ȳ_t\n", global_p, global_q);
    fprintf(outputv, "Case   : %d\n", global_case);
    /* Las deterministas se quitan de los NIVELES, antes de formar nabla Y2 y W
       -- que es donde el cast de fue las quita tambien, su bloque [6] va antes
       del [7] --, y por eso hay que reconstruir los niveles despues.
       Va aqui, y no antes, porque deja constancia en el .out y ese fichero no
       esta abierto todavia mas arriba: escribir alli reventaba con outputv en
       NULL.                                                                   */
    if (global_interv) {
        printf("Restando las deterministas declaradas en los .pre:\n");
        subtract_interventions(interv_prefix);
        build_y2_levels();
    }

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
        if (global_alpha)
            fprintf(outputv,
                "  (every rank estimated UNDER the restriction alpha = A*psi,\n"
                "   so this is the rank sequence within H1(r) and NOT the usual\n"
                "   one.  The tabulated critical values do not apply and are not\n"
                "   printed: they are for alpha free.)\n");
        else
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
            if (global_alpha) {
                /* Bajo alpha = A*psi el estadistico tiene OTRA distribucion: las
                   tablas son para alpha libre.  Imprimirlas aqui seria dar
                   valores criticos equivocados con aspecto de correctos, que es
                   peor que no darlos.                                        */
                fprintf(outputv, "        -        -        -   (restricted:"
                                 " tabulated values do not apply)");
            } else if (global_case != 3 && g >= 1 && g <= LR_MAXTRENDS) {
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
        if (global_alpha)
            fprintf(outputv,
                "  NOTE: with alpha = A*psi imposed, what is being tested at each\n"
                "  step is the rank WITHIN the restricted model.  That is a\n"
                "  legitimate question and a different one; its distribution is\n"
                "  not the tabulated Johansen one.  For the usual rank test, drop\n"
                "  the restriction.\n");

        free_ivector(good, 0, M - 1);
        free_ivector(npr, 0, M - 1);
        free_vector(ll, 0, M - 1);
        printf("Done. Output written to %s\n", base_name);
        fclose(outputv);
        cleanup_names(outputf, inputf, base_name);
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

    /* Con alpha = A*psi hace falta el modelo LIBRE para el LR, asi que se
       estima primero H(r) y luego H1(r).  Los grados de libertad son
       (M - sa)*r, explicitos en Johansen y Swensen (2024).                    */
    real lr_free = 0.0; int lr_free_ok = 0;
    if (global_alpha) {
        int save = global_alpha;
        global_alpha = 0;
        {
            int npf = calc_nparametrs();
            real *xf = vector(1, npf), *devf = vector(1, npf);
            real **covf = matrix(1, npf, 1, npf);
            struct Tvarma vf;
            int iff;
            vf.xitol = varma1.xitol;
            init_guess(xf, npf);
            vec_shootx(xf, &vf, &iff, 1, 0);
            est(&vec_shootx, npf, xf, devf, covf, 500, 200, 1e-5, 1e-7,
                vf.xitol, vf.a, &vf.sigma2, &vf.logelf, &iff);
            lr_free_ok = (iff == 0);
            lr_free    = vf.logelf;
            printf("  H(r)  libre        : logL = %15.10f%s\n", lr_free,
                   lr_free_ok ? "" : "  (la estimacion fallo)");
            vec_shootx(xf, &vf, &iff, 0, 1);
            free_matrix(covf, 1, npf, 1, npf);
            free_vector(devf, 1, npf);
            free_vector(xf, 1, npf);
        }
        global_alpha = save;
    }

    init_guess(x, npar);

    /* -writeres: los residuos de la regresión condicional, que es lo que
       init_guess acaba de publicar.  Es un modo y termina aquí.                */
    if (global_writeres) {
        printf("Escribiendo un .inp por residuo de la regresión condicional:\n");
        if (write_resid_inps(inp_prefix) != 0) exit(1);
        printf("Ahora: 'python -m fue %s.<i> eml' y después drvec ... -seed %s\n",
               inp_prefix, inp_prefix);
        fclose(outputv);
        cleanup_names(outputf, inputf, base_name);
        return 0;
    }

    int ifault;
    vec_shootx(x, &varma1, &ifault, 1, 0);  /* allocate */

    /* -eval: la verosimilitud EN EL PUNTO DE PARTIDA, sin optimizar.
       Es el diagnóstico que separa dos cosas que se confunden con facilidad:
       una semilla mala (arranca peor) de un optimizador que desde una semilla
       mejor acaba peor (la superficie).  Sin esto, comparar sólo los logL
       finales no distingue un fallo de signo de un problema de camino.
       La fórmula es la misma de drvmlest.c:133, con sigma2 = 1 y atf = TRUE.  */
    if (global_eval) {
        const real LOG2PI = 1.837877066;
        real pi1, pi2, pi3, ll;
        int ifev = 0;
        elf(varma1.m, varma1.n, varma1.p, varma1.q, varma1.mu, varma1.phi,
            varma1.theta, varma1.qq, varma1.w, 1.0, varma1.xitol,
            TRUE, varma1.a, &pi1, &pi2, &pi3, &ifev);
        if (ifev > 0) {
            printf("eval: elf devuelve ifault = %d en el punto de partida\n", ifev);
            fprintf(outputv, "eval: ifault = %d\n", ifev);
        } else {
            ll = -0.5 * varma1.m * varma1.n * (LOG2PI - log((real) varma1.m)
                 - log((real) varma1.n) + 1.0)
                 - 0.5 * varma1.n * (varma1.m * log(pi1) + log(pi2));
            printf("eval: logelf en el punto de partida = %15.10f  "
                   "(sigma2 = %.10f)\n", ll, pi1 / (varma1.n * varma1.m));
            fprintf(outputv, "eval logelf : %15.10f\n", ll);
            if (seed_have_uv) {
                /* La identidad de cruce de la escalera: con r = 0 y estructura
                   diagonal la verosimilitud exacta factoriza, asi que esto debe
                   coincidir con la suma de las univariantes de los .pre.  Lo
                   que sobre es la brecha de la transformacion, no del ajuste. */
                fprintf(outputv, "sum univariate : %15.10f\n", seed_logl_sum);
                printf("      suma univariante = %15.10f   diferencia = %.3e\n",
                       seed_logl_sum, ll - seed_logl_sum);
            }
        }
        vec_shootx(x, &varma1, &ifault, 0, 1);   /* liberar */
        fclose(outputv);
        cleanup_names(outputf, inputf, base_name);
        return 0;
    }

    int maxits = 500, nrits = 200;
    real gradtol = 1e-5, sptol = 1e-7;

    /* -multistart n: estimar desde n puntos de partida y quedarse con el mejor.
     *
     * NO es tocar el optimizador -- que no se toca --, es ejecutarlo varias
     * veces.  Y esta justificado por dos medidas propias, no por costumbre:
     *
     *   - F2 midio que el punto donde para depende fuertemente del punto donde
     *     arranca: en el caso 1, mover Theta por centesimas mueve la respuesta
     *     14.45 unidades.  Esa es exactamente la condicion en la que el
     *     multiarranque paga.
     *   - La busqueda global que sirve de referencia para |Sigma| (0.002311)
     *     SE HIZO ASI, por multiarranque, y acaba pegada a la barrera de
     *     invertibilidad de chekma con max|lambda(Theta1)| = 1.00005.  El ajuste
     *     de drvec acaba en la MISMA barrera -- 1.000050 medido -- pero en otro
     *     punto de ella, con |Sigma| 0.002461.  O sea: mismo borde, peor sitio.
     *
     * Las perturbaciones son deterministas (generador propio con semilla fija)
     * para que un resultado se pueda reproducir: un multiarranque que no se
     * puede repetir no sirve como evidencia.
     */
    int ms_done = 0;         /* 1 = el multiarranque ya dejo el ajuste final */
    if (global_multistart > 1) {
        real *xbest = vector(1, npar), *xtry = vector(1, npar);
        real *devb  = vector(1, npar), *devbest = vector(1, npar);
        real **covb = matrix(1, npar, 1, npar);
        real **covbest = matrix(1, npar, 1, npar);
        real best = 0.0, worst = 0.0, best_s2 = 0.0;
        int  k, i2, nok = 0, ifb, bestk = 0;
        unsigned long rng = 20260818UL;      /* semilla fija, a proposito */

        for (i2 = 1; i2 <= npar; i2++) xbest[i2] = x[i2];
        for (k = 0; k < global_multistart; k++) {
            struct Tvarma vk;
            vk.xitol = varma1.xitol;
            init_guess(xtry, npar);
            if (k > 0) {
                /* Jitter multiplicativo sobre la semilla, en una escalera de
                   amplitud que depende SOLO de k y no de n.  Eso hace el
                   procedimiento MONOTONO en n: los primeros n arranques de una
                   corrida larga son exactamente los de una corta, asi que pedir
                   mas arranques solo puede mejorar.  Con la amplitud escalada
                   por n -- como estaba -- aumentar n cambiaba el conjunto en vez
                   de ampliarlo, y se midio el sinsentido: n=24 daba |Sigma|
                   0.002349 y n=40 daba 0.002453.                             */
                real amp = 0.05 * (real) (1 + (k - 1) % 20);
                for (i2 = 1; i2 <= npar; i2++) {
                    real u;
                    rng = rng * 6364136223846793005UL + 1442695040888963407UL;
                    u = ((real) ((rng >> 33) & 0x7FFFFFFF)) / 2147483647.0;
                    u = 2.0 * u - 1.0;                       /* U(-1, 1) */
                    xtry[i2] += amp * u * (fabs(xtry[i2]) > 1.0e-8
                                           ? fabs(xtry[i2]) : 0.1);
                }
            }
            vec_shootx(xtry, &vk, &ifb, 1, 0);
            est(&vec_shootx, npar, xtry, devb, covb, maxits, nrits, gradtol,
                sptol, vk.xitol, vk.a, &vk.sigma2, &vk.logelf, &ifb);
            if (ifb == 0) {
                if (nok == 0 || vk.logelf > best) {
                    best = vk.logelf; bestk = k; best_s2 = vk.sigma2;
                    for (i2 = 1; i2 <= npar; i2++) {
                        xbest[i2] = xtry[i2];
                        /* La covarianza hay que guardarla DEL ARRANQUE QUE LA
                           PRODUJO.  cov sale del factor que raxopt acumula
                           mientras itera, asi que volver a llamar a est desde
                           el optimo -- donde no itera -- deja mtmp en su
                           inicializacion y devuelve errores estandar TODOS
                           IGUALES.  Medido: 0.134231 para los tres parametros
                           de un caso donde los verdaderos son 0.062, 0.123 y
                           0.106.  Habrian sido errores estandar inventados con
                           aspecto de calculados.                             */
                        devbest[i2] = devb[i2];
                        for (int j2 = 1; j2 <= npar; j2++)
                            covbest[i2][j2] = covb[i2][j2];
                    }
                }
                if (nok == 0 || vk.logelf < worst) worst = vk.logelf;
                nok++;
            }
            vec_shootx(xtry, &vk, &ifb, 0, 1);
            if (!quiet_mode)
                printf("  arranque %2d/%d: %s\n", k + 1, global_multistart,
                       (ifb == 0) ? "ok" : "fallo");
        }
        if (nok > 0) {
            for (i2 = 1; i2 <= npar; i2++) {
                x[i2]   = xbest[i2];
                dev[i2] = devbest[i2];
                for (int j2 = 1; j2 <= npar; j2++) cov[i2][j2] = covbest[i2][j2];
            }
            varma1.logelf = best;
            varma1.sigma2 = best_s2;
            ifault  = 0;
            ms_done = 1;    /* no se vuelve a estimar: ya esta el mejor ajuste */
            fprintf(outputv, "\nMulti-start: %d of %d starting points converged; "
                             "logL from %.6f to %.6f (best is start %d).\n",
                    nok, global_multistart, worst, best, bestk + 1);
            fprintf(outputv, "  The SPREAD is the diagnostic: on a well-behaved\n"
                             "  surface every start lands in the same place.\n");
            if (!quiet_mode)
                printf("  Multi-start: %d/%d ok, logL from %.6f to %.6f\n",
                       nok, global_multistart, worst, best);
        } else {
            fprintf(outputv, "\nMulti-start: no starting point converged.\n");
        }
        free_matrix(covbest, 1, npar, 1, npar);
        free_matrix(covb, 1, npar, 1, npar);
        free_vector(devbest, 1, npar);
        free_vector(devb, 1, npar);
        free_vector(xtry, 1, npar);
        free_vector(xbest, 1, npar);
        /* NO se realoja varma1: sus bufers siguen vivos desde la llamada de
           arriba, y el bucle uso su propia estructura vk.  Volver a llamar con
           firstx = 1 dejaba huerfana la primera asignacion -- 1080 bytes que
           valgrind marcaba como definitely lost.                             */
    }

    if (!ms_done)
        est(&vec_shootx, npar, x, dev, cov, maxits, nrits, gradtol, sptol,
            varma1.xitol, varma1.a, &varma1.sigma2, &varma1.logelf, &ifault);

    if (ifault == 0) {
        vec_shootx(x, &varma1, &ifault, 0, 0);  /* retrieve final */

        /* Rellenar los RESIDUOS del ajuste final.  Los escribe elf con
           atf = TRUE, y hasta ahora llegaban de rebote porque est hacia esa
           llamada al terminar.  Con -multistart no hay est final -- el mejor
           punto ya esta elegido -- y los residuos se quedaban SIN CALCULAR: la
           diagnosis salia con Q = nan y "los residuos parecen ruido blanco",
           que es la peor forma posible de equivocarse.  Se calculan aqui, que
           es donde se sabe cual es el ajuste final.                          */
        {
            real pi1, pi2, pi3;
            int ifr = 0;
            elf(varma1.m, varma1.n, varma1.p, varma1.q, varma1.mu, varma1.phi,
                varma1.theta, varma1.qq, varma1.w, 1.0, varma1.xitol,
                TRUE, varma1.a, &pi1, &pi2, &pi3, &ifr);
        }

        fprintf(outputv, "\nESTIMATION SUCCESSFUL (ifault=0)\n");
        fprintf(outputv, "sigma2 : %15.10f\n", varma1.sigma2);
        fprintf(outputv, "logelf : %15.10f\n", varma1.logelf);
        convergence_note(termcode_from_out(outputf));
        residual_diagnostics(&varma1);

        /* El LR de H1(r) contra H(r).  Johansen y Swensen (2024): los grados de
           libertad son (M - sa)*r, que es cuantas entradas libres de alpha
           elimina la restriccion.                                            */
        if (global_alpha && lr_free_ok) {
            int df = (nser - alpha_sa) * global_r;
            real lr = 2.0 * (lr_free - varma1.logelf);
            real pv = (df > 0 && lr > 0.0) ? gsl_cdf_chisq_Q(lr, df) : 1.0;
            fprintf(outputv, "\n--- H1(r): alpha = A*psi, contra H(r) ---\n");
            fprintf(outputv, "logL H(r)  libre       : %15.10f\n", lr_free);
            fprintf(outputv, "logL H1(r) restringido : %15.10f\n", varma1.logelf);
            fprintf(outputv, "LR = 2(libre - restr.) : %15.10f\n", lr);
            fprintf(outputv, "grados de libertad     : %d   (M - sa)*r\n", df);
            fprintf(outputv, "p-valor (chi2)         : %15.10f\n", pv);
            printf("  H1(r) restringido  : logL = %15.10f\n", varma1.logelf);
            printf("  LR = %.6f, %d g.l., p = %.6f%s\n", lr, df, pv,
                   (lr < -1.0e-6) ? "   <- NEGATIVO: el restringido bate al libre,"
                                    " luego uno de los dos no convergio" : "");
        }

        /* --- Structured VEC output --------------------------------------- */
        int s = nser - global_r, r = global_r;
        int ii = 1;
        real **Lam_m = matrix(1, nser, 1, (r > 0 ? r : 1));
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

        if (global_alpha) {
            /* Con alpha = A*psi lo que x[] lleva es psi (sa x r), no Lambda, y
               el recorrido tiene que consumir sa*r y no M*r -- la impresora es
               el otro sitio donde el layout del vector se puede desincronizar,
               y el bloque estructural de la bateria existe por eso.
               Se imprimen las dos: psi es lo estimado, Lambda = A*psi es lo
               interpretable, y los errores estandar solo existen para psi.   */
            real **psi = matrix(1, alpha_sa, 1, (r > 0 ? r : 1));
            fprintf(outputv, "psi (sa x r), the free part of alpha = A*psi =\n");
            for (int i = 1; i <= alpha_sa; i++) {
                fprintf(outputv, "  ");
                for (int j = 1; j <= r; j++) {
                    psi[i][j] = x[ii];
                    fprintf(outputv, "%12.6f (sd %9.6f)", x[ii], dev[ii]);
                    ii++;
                }
                fprintf(outputv, "\n");
            }
            fprintf(outputv, "Lambda = A*psi (M x r) =\n");
            for (int i = 1; i <= nser; i++) {
                fprintf(outputv, "  ");
                for (int j = 1; j <= r; j++) {
                    real acc = 0.0;
                    for (int kk = 1; kk <= alpha_sa; kk++)
                        acc += alpha_A[i][kk] * psi[kk][j];
                    Lam_m[i][j] = acc;
                    fprintf(outputv, "%12.6f", acc);
                }
                fprintf(outputv, "\n");
            }
            free_matrix(psi, 1, alpha_sa, 1, (r > 0 ? r : 1));
        } else {
            fprintf(outputv, "Lambda (M x r) =\n");
            for (int i = 1; i <= nser; i++) {
                fprintf(outputv, "  ");
                for (int j = 1; j <= r; j++) {
                    Lam_m[i][j] = x[ii];
                    fprintf(outputv, "%12.6f (sd %9.6f)", x[ii], dev[ii]);
                    ii++;
                }
                fprintf(outputv, "\n");
            }
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

        /* ---- Triangularizacion Sigma = P D P' ----------------------------- */
        /* Descomposicion LDL' de la covarianza de innovaciones: P unitriangular
           inferior, D diagonal.  Con A_t = P A*_t, las innovaciones A*_t estan
           INCORRELACIONADAS (cov = D), asi que el sistema premultiplicado por
           P^-1 se lee ecuacion a ecuacion: es lo que permite hablar de una
           ecuacion sin arrastrar la correlacion contemporanea de las demas.
           BVECM seccion 4; el legado lo hacia solo para el caso bivariante.

           IMPORTANTE, y por eso se dice en la salida: el orden es el de las
           COLUMNAS DEL .inp.  Otro orden da otra P.  Es la misma clase de
           decision silenciosa que la eleccion del bloque Y1.                  */
        {
            real **Sg = matrix(1, nser, 1, nser);
            real **P  = matrix(1, nser, 1, nser);
            real  *D  = vector(1, nser);
            int a, b, k, ok_ldl = 1;
            for (a = 1; a <= nser; a++)
                for (b = 1; b <= nser; b++)
                    Sg[a][b] = varma1.sigma2 * ((b <= a) ? Qm[a][b] : Qm[b][a]);
            for (a = 1; a <= nser; a++)
                for (b = 1; b <= nser; b++) P[a][b] = (a == b) ? 1.0 : 0.0;
            for (b = 1; b <= nser; b++) {
                real acc = Sg[b][b];
                for (k = 1; k < b; k++) acc -= P[b][k] * P[b][k] * D[k];
                D[b] = acc;
                if (D[b] <= 0.0) { ok_ldl = 0; break; }
                for (a = b + 1; a <= nser; a++) {
                    real s2 = Sg[a][b];
                    for (k = 1; k < b; k++) s2 -= P[a][k] * P[b][k] * D[k];
                    P[a][b] = s2 / D[b];
                }
            }
            if (ok_ldl) {
                fprintf(outputv, "\nSigma = P D P'  (P unit lower triangular; "
                                 "A_t = P A*_t with cov(A*_t) = D)\n");
                fprintf(outputv, "P =\n");
                for (a = 1; a <= nser; a++) {
                    fprintf(outputv, "  ");
                    for (b = 1; b <= a; b++) fprintf(outputv, "%12.6f", P[a][b]);
                    fprintf(outputv, "\n");
                }
                fprintf(outputv, "D (diagonal) =\n  ");
                for (a = 1; a <= nser; a++) fprintf(outputv, "%12.6f", D[a]);
                fprintf(outputv, "\n");
                fprintf(outputv, "  own share of each innovation variance "
                                 "(D_i / Sigma_ii):\n  ");
                for (a = 1; a <= nser; a++)
                    fprintf(outputv, "%11.1f%%", 100.0 * D[a] / Sg[a][a]);
                fprintf(outputv, "\n");
                fprintf(outputv,
                    "  A*_t is uncorrelated, so premultiplying the system by P^-1\n"
                    "  gives equations that can be read one at a time.  NOTE the\n"
                    "  ordering is the COLUMN ORDER of the .inp: a different order\n"
                    "  gives a different P, and the choice is the user's.\n");
            } else {
                fprintf(outputv, "\nSigma = P D P': not computed (Sigma is not "
                                 "positive definite at the optimum)\n");
            }
            free_vector(D, 1, nser);
            free_matrix(P, 1, nser, 1, nser);
            free_matrix(Sg, 1, nser, 1, nser);
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
        /* B2 va con su error estandar cuando es parametro.  Con -fixb2 no lo
           lleva, y eso es correcto: un valor fijado no tiene error estandar, y
           ponerle uno seria inventarlo.  El orden de lectura de dev es el mismo
           column-major con el que se leyo B2m.                               */
        {
            int jj = ii - (global_fixb2 ? 0 : s * r);
            for (int i = 1; i <= s; i++) {
                fprintf(outputv, "  ");
                for (int j = 1; j <= r; j++) {
                    if (global_fixb2) fprintf(outputv, "%12.6f", B2m[i][j]);
                    else fprintf(outputv, "%12.6f (sd %9.6f)", B2m[i][j],
                                 dev[jj + (j - 1) * s + (i - 1)]);
                }
                fprintf(outputv, "\n");
            }
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

        /* ---- Pi = Lambda * B', y sus autovalores ------------------------- */
        /* Pi es la matriz de largo plazo y, a diferencia de Lambda y de B, es
           INVARIANTE a la normalizacion: cualquier reparametrizacion
           Lambda -> Lambda*G, B -> B*G^-T deja Pi igual.  Por eso es lo que hay
           que mirar para comparar ajustes, y por eso se imprime aqui.        */
        if (r > 0) {
            real **Pi = matrix(1, nser, 1, nser);
            real *wr = vector(1, nser), *wi = vector(1, nser);
            int a, b, j;
            for (a = 1; a <= nser; a++)
                for (b = 1; b <= nser; b++) {
                    real acc = 0.0;
                    for (j = 1; j <= r; j++) {
                        real Bbj = (b <= r) ? ((b == j) ? 1.0 : 0.0) : B2m[b - r][j];
                        acc += Lam_m[a][j] * Bbj;
                    }
                    Pi[a][b] = acc;
                }
            fprintf(outputv, "\nPi = Lambda B' (M x M), the long-run matrix =\n");
            for (a = 1; a <= nser; a++) {
                fprintf(outputv, "  ");
                for (b = 1; b <= nser; b++) fprintf(outputv, "%12.6f", Pi[a][b]);
                fprintf(outputv, "\n");
            }
            fprintf(outputv, "  (Pi is INVARIANT to the normalisation, while "
                             "Lambda and B are not:\n"
                             "   Lambda->Lambda G, B->B G^-T leaves it "
                             "unchanged.  Compare fits on Pi.)\n");
            {   /* autovalores, sobre una copia: eigenqr destruye su argumento */
                real **Pc = matrix(1, nser, 1, nser);
                for (a = 1; a <= nser; a++) for (b = 1; b <= nser; b++)
                    Pc[a][b] = Pi[a][b];
                eigenqr(Pc, nser, wr, wi);
                fprintf(outputv, "  eigenvalues of Pi:");
                for (a = 1; a <= nser; a++) {
                    if (fabs(wi[a]) < 1.0e-12) fprintf(outputv, "  %.6f", wr[a]);
                    else fprintf(outputv, "  %.6f%+.6fi", wr[a], wi[a]);
                }
                fprintf(outputv, "\n");
                free_matrix(Pc, 1, nser, 1, nser);
            }
            fprintf(outputv,
                "  CAUTION, and it is stronger than the usual one: here Pi = Lambda B'\n"
                "  has rank r BY CONSTRUCTION, so its M-r zero eigenvalues are\n"
                "  guaranteed and say nothing about whether r is right -- reading them\n"
                "  as evidence for the rank is circular.  Even in the unrestricted\n"
                "  case they are only an indication: Melard, Roy and Saidi (2004) show\n"
                "  the assumption on Phi(1) does not imply what that reading assumes\n"
                "  (Pham, Roy and Cedras 2003).  The instrument is -lrtest.\n");
            free_vector(wi, 1, nser);
            free_vector(wr, 1, nser);
            free_matrix(Pi, 1, nser, 1, nser);
        }

        /* ---- Diagnostico de la normalizacion ----------------------------- */
        /* B = [I_r ; B2] asume que el bloque Y1 aparece de verdad en cada
           relacion de cointegracion.  Si no, B2 se dispara y el modelo se
           vuelve una trampa silenciosa: el ajuste "funciona" y describe otra
           cosa.  Mauricio lo advierte (p. 3648) y remite a Luukkonen et al.
           (1999) y Kurozumi (2005); Melard, Roy y Saidi lo evitan usando el
           espacio nulo de Phi(1) en vez de una normalizacion.
           La medida que se usa aqui es libre de unidades: en W = Y1 + B2'Y2
           cada serie pesa |coeficiente| * sd(serie), asi que se informa la
           cuota del bloque Y1 en ese peso total.  Una cuota diminuta dice que
           la relacion no es realmente sobre Y1 y que la normalizacion esta
           forzada.                                                            */
        if (r > 0 && s > 0) {
            real *sdY2 = vector(1, s);
            int a, t, j;
            for (a = 1; a <= s; a++) {
                real m1 = 0.0, v = 0.0;
                for (t = 1; t <= nobs; t++) m1 += Y2_levels[t][a];
                m1 /= nobs;
                for (t = 1; t <= nobs; t++) v += (Y2_levels[t][a] - m1)
                                               * (Y2_levels[t][a] - m1);
                sdY2[a] = sqrt(v / (nobs > 1 ? nobs - 1 : 1));
            }
            fprintf(outputv, "\nNormalisation check (which series carry each "
                             "cointegrating relation):\n");
            for (j = 1; j <= r; j++) {
                real m1 = 0.0, v = 0.0, w1, w2 = 0.0, share;
                for (t = 1; t <= nobs; t++) m1 += datamat[t][s + j];
                m1 /= nobs;
                for (t = 1; t <= nobs; t++) v += (datamat[t][s + j] - m1)
                                               * (datamat[t][s + j] - m1);
                w1 = sqrt(v / (nobs > 1 ? nobs - 1 : 1));      /* coef = 1 */
                for (a = 1; a <= s; a++) w2 += fabs(B2m[a][j]) * sdY2[a];
                share = (w1 + w2 > 0.0) ? w1 / (w1 + w2) : 1.0;
                fprintf(outputv, "  relation %d: the Y1 block carries %5.1f%% "
                                 "of the weight", j, 100.0 * share);
                if (share < 0.05) {
                    fprintf(outputv, "   <-- DUBIOUS\n");
                    fprintf(stderr,
                        "WARNING: cointegrating relation %d barely involves the "
                        "Y1 block (%.1f%%).\n"
                        "         B = [I_r; B2] normalises on Y1, so this fit "
                        "may be describing\n"
                        "         a relation among the other series with an "
                        "inflated B2.  Consider\n"
                        "         reordering the columns of the .inp.  See "
                        "docs/MODEL.md 5.4.\n", j, 100.0 * share);
                } else fprintf(outputv, "\n");
            }
            free_vector(sdY2, 1, s);
        }

        free_matrix(Lam_m, 1, nser, 1, (r > 0 ? r : 1));
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
    fclose(outputv);
    cleanup_names(outputf, inputf, base_name);

    printf("Done. Output written to %s\n", argv[1]);
    return 0;
}