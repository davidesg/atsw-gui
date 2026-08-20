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
/*                          [-lrtest] [-rungs]                               */
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
#include <gsl/gsl_eigen.h>    /* problema de autovalores generalizado simetrico */
#include <gsl/gsl_linalg.h>   /* QR y SVD para la condicion de rango de Granger */

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
int global_rungs  = 0; /* if 1, report the ladder's rungs 0-2 and their LRs   */

/*  -seedgate — LA RUTA (B) DEL PLAN, detras de una opcion y NO por defecto.
 *
 *  El puente que la escalera usa en todas partes -- coger el optimo de abajo y
 *  arrancar ahi -- no alcanza el peldano de r = 1: con Lambda = 0 el sistema
 *  transformado tiene una raiz AR de modulo exactamente 1 y la verosimilitud no
 *  esta definida ahi (docs/VEC_EMBEDDING_PLAN.md 3).  (B) cruza sin elegir
 *  ninguna constante: se sujetan F, Theta y Sigma en el optimo de r = 0 y se
 *  estiman SOLO Lambda y B2; el paso fuera de la frontera lo escoge la
 *  verosimilitud.  Despues se suelta todo.
 *
 *  prof_hold es el modo condicional: mientras esta puesto, el vector de
 *  parametros lleva la media, Lambda y B2, y los bloques F, Theta y Sigma se
 *  leen de hold_*, no de x.  Es el mismo patron que -fixb2 y que -alpha: la
 *  restriccion vive en el cast y el optimizador no se entera.                */
int global_seedgate = 0;

/*  -seedb2 v — arrancar B2 en v y estimarlo LIBRE.  No es -fixb2, que lo sujeta:
 *  aqui se mueve.  Existe como INSTRUMENTO DE MEDIDA, para poder preguntar de
 *  que depende el ajuste -- si de donde arranca B2 o del sitio donde para el
 *  optimizador -- sin tener que recompilar para cada valor.  Que una pregunta
 *  sobre el arranque solo se pueda contestar recompilando es, por si mismo, una
 *  razon para que la opcion exista.                                          */
int  global_seedb2 = 0;
real global_seedb2_value = 0.0;

/*  -seedjoh — sembrar B2 con la solucion canonica de rango reducido en vez de
 *  con el OLS estatico.  Ver canonical_b2.                                   */
int  global_seedjoh = 0;
static int canon_used = 0;      /* 1 = la solucion canonica entro de verdad   */

/*  -mawarma — EL MA NO ES LIBRE: LO HEREDA.
 *
 *  El corolario 2 del articulo BVECM (Equivalencia WARMA-VEC con MA) dice que
 *  si el proceso admite representacion WARMA
 *
 *      Phi(B) w_t = Theta(B) a_t,     Delta z2_t = gamma w_{t-1} + ... + eta_t
 *
 *  -- o sea el MA vive en el bloque cointegrado y el bloque diferenciado es
 *  ruido blanco --, entonces el error de la representacion VEC es
 *
 *      eps_t = [ beta' eta_t + Theta(B) a_t ;  eta_t ].
 *
 *  Reagrupando sobre A_t = [a_t + beta' eta_t ; eta_t], que es una
 *  transformacion invertible del ruido, eso es eps_t = A_t - Theta1 A_{t-1} con
 *
 *      Theta = [ Theta11   Theta11 B2' ]        (B2 = -beta; ESTUDIO_BVECM 2.1)
 *              [    0           0      ]
 *
 *  o sea: LAS ULTIMAS s FILAS SON CERO y el bloque superior derecho NO es libre,
 *  esta determinado por el izquierdo y por B2.  Con M = 2 y r = 1 eso deja UN
 *  parametro de medias moviles donde el modelo libre lleva CUATRO.
 *
 *  Y esto no es una preferencia de modelizacion, esta medido.  Simulando el
 *  propio DGP WARMA del articulo (theta = 0.5, beta = 0.5, n = 1000) y ajustando
 *  con Theta libre, drvec devuelve entradas de 3.26, 7.50 o -2.44 donde la
 *  verdad es [[0.5, -0.25],[0,0]], y aparca en la frontera de invertibilidad.
 *  B2 sale bien en todas las replicas -- es superconsistente -- pero el bloque
 *  (Lambda, Theta) esta practicamente no identificado cuando Theta es libre.
 *  Ver docs/HOMOLOGATION.md 4g.                                              */
int global_mawarma = 0;

/*  -matri — LA SOLUCION DE COMPROMISO, y es la restriccion MINIMA de las tres.
 *
 *  El corolario 2 impone dos cosas a la vez: que el bloque inferior izquierdo de
 *  Theta sea CERO -- las innovaciones del bloque cointegrado no entran con
 *  retardo en las ecuaciones diferenciadas -- y que el superior derecho sea
 *  Theta11 B2', o sea que el bloque diferenciado NO TIENE MA propio.  El
 *  bootstrap de 4i dice que los datos rechazan lo segundo en ocho de once
 *  casos.  Lo primero no se ha contrastado nunca por separado, y es donde el
 *  ajuste libre se descontrola: las entradas (2,1) estimadas libremente valen
 *  -0.75, -1.32, 1.51 y hasta 5.09 en los ocho pares, y la (2,2) queda entre
 *  1.0 y 2.0, que es lo que pone la raiz en el circulo.
 *
 *  -matri deja Theta TRIANGULAR POR BLOQUES: [T11  T12 ; 0  T22], con T22
 *  libre.  Cuesta q*s*r parametros contra el libre -- uno solo con M = 2 y
 *  r = 1 -- y conserva del corolario justo la parte que la estructura implica
 *  y los datos no contradicen.                                              */
int global_matri = 0;

/*  -marow — EL PELDANO DE EN MEDIO, y el que la medida senala.
 *
 *  Theta = [T11  T12 ; 0  0]: el bloque DIFERENCIADO no lleva medias moviles
 *  propias, pero el cruzado T12 queda libre en vez de determinado por B2.
 *
 *  Por que ahi y no en otro sitio: -matri, que anula solo el bloque inferior
 *  IZQUIERDO y deja T22 libre, NO quita la patologia -- G se queda entre 0.02 y
 *  0.12 y la raiz MA en 1.000 en ocho de once casos.  Lo que la quita es que
 *  Theta(1) tenga la identidad en su bloque inferior, y eso lo da anular T22:
 *  con T22 libre el optimizador lo lleva a la unidad, que es (1-B) sentado
 *  sobre el bloque ya diferenciado.  O sea que de las dos restricciones que el
 *  corolario 2 impone a la vez -- T21 = 0 con T22 = 0, y T12 determinado --,
 *  la que sostiene la admisibilidad es la primera y la que los datos rechazan
 *  es la segunda.  Este peldano separa las dos.                              */
int global_marow = 0;

/*  -warma — LA CLASE DE LOS TEOREMAS, PARAMETRIZADA DONDE ESTA ENUNCIADA.
 *
 *  Hasta aqui todas las restricciones se han escrito sobre Theta en coordenadas
 *  VEC, y ahi la misma restriccion acopla Theta con B2 y hay que reconstruirla
 *  en cada evaluacion.  En las coordenadas del sistema transformado,
 *  Ybar = [nabla Y2 ; W], que son las que usa la definicion 3 del BVECM y las
 *  que usa Phillips, la clase es un PATRON DE CEROS:
 *
 *      Phi*_k = [ 0   Psi_k ]      Theta*_k = [ 0    0    ]
 *               [ 0   Phi_k ]                 [ 0  Th_k   ]
 *
 *  o sea: nada depende de retardos de nabla Y2 -- todo entra por W --, y el
 *  bloque diferenciado no lleva medias moviles.  Y B2 entra SOLO POR LOS DATOS,
 *  al formar W por resta, como la entrada de una funcion de transferencia; no
 *  toca ningun parametro.  Eso es lo que hace el shootx del legado y es la
 *  razon medida de que su superficie este mejor condicionada
 *  (docs/ESTUDIO_BVECM_vs_DRVEC.md 3.1).
 *
 *  El vector de parametros REUTILIZA las mismas casillas -- media, un bloque
 *  M x r, luego (p-1) bloques M x r, q bloques r x r, Sigma, y B2 en la cola --
 *  para no tocar ni el bootstrap ni el perfilado ni la cola de B2, que dependen
 *  de esa disposicion.  Lo que cambia es que se leen como coeficientes de
 *  W_{t-k} y no como Lambda y F.                                             */
int global_warma = 0;

/*  -rankadm — LA CONDICION QUE HACE QUE EL RANGO SEA EL QUE SE DICE.
 *
 *  Para que un VEC con errores de medias moviles represente un proceso I(1) con
 *  rango de cointegracion EXACTAMENTE r, la matriz
 *
 *      G = Lambda_perp' Theta(1) B_perp        (s x s,  Theta(1) = I - sum Theta_k)
 *
 *  tiene que ser no singular: es la que aparece en la representacion de Granger,
 *  C(1) = B_perp (Lambda_perp' Gamma B_perp)^-1 Lambda_perp' Theta(1).  Si G
 *  degenera, C(1) pierde rango y el modelo AJUSTADO NIEGA SU PROPIO RANGO: dice
 *  r y sus parametros implican que no queda tendencia estocastica.
 *
 *  Mauricio (2006) supone la no estacionariedad parcial DEL PROCESO VERDADERO
 *  (seccion 2) y remite a Yap y Reinsel (1995) para las condiciones de
 *  identificacion, pero la ESTIMACION no impone ninguna: el conjunto donde G
 *  degenera esta dentro de la region que el programa admite, porque el motor
 *  solo comprueba que las raices no esten DENTRO del circulo y esta patologia
 *  vive exactamente SOBRE el, en el borde permitido.  Medido en los ocho pares
 *  con Theta libre: |Theta(1)| entre -7e-5 y 4e-4, y la direccion en que
 *  Theta(1) es singular alineada con Lambda_perp entre 0.946 y 0.9996.  O sea
 *  que el optimo libre esta AHI, no cerca.  Ver docs/HOMOLOGATION.md 4h.
 *
 *  sigma_min(G) se REPORTA siempre, como las raices.  -rankadm ademas lo
 *  IMPONE, rechazando el punto igual que se rechaza una Sigma no definida
 *  positiva, para que el optimizador no entre.                               */
int  global_rankadm = 0;
/*  EL SUELO, PUESTO CON LA MEDIDA DELANTE Y NO ANTES.  Sobre el banco entero,
 *  las especificaciones admisibles dan G entre 0.52 y 1.00 y las que degeneran
 *  entre 0.016 y 0.133 (HOMOLOGATION.md 4h y 4j): un orden de magnitud de
 *  separacion y un hueco vacio en medio.  0.2 es el numero redondo de ese
 *  hueco.  Es una eleccion, se dice que lo es, y -rankadm la cambia -- pero no
 *  es una eleccion arbitraria: cualquier corte entre 0.15 y 0.5 clasifica igual
 *  los veintitantos ajustes del registro.  El valor anterior, 1e-3, no mordia
 *  en ningun caso medido, que es otra forma de estar mal elegido.            */
real global_rankadm_tol = 0.2;
int  global_matest = 0;      /* -matest N: bootstrap del MA heredado vs libre */
int  global_specs  = 0;      /* -specs: la escalera de especificaciones        */
int  global_artest = 0;      /* -artest N: bootstrap de Gamma_i = m_i alpha'   */
static real granger_sv = -1.0;   /* sigma_min(G) en la ultima evaluacion */

/*  granger_smin — sigma_min(Lambda_perp' Theta(1) B_perp), o -1 si no aplica.
 *  Lambda_perp y B_perp se ortonormalizan (QR), asi que la escala del
 *  estadistico es la de Theta(1) y no la de como venga escrito Lambda.       */
static real granger_smin(real **Lam, real **B2, real ***Th, int M, int r, int q)
{
    int s = M - r, i, j, k;
    gsl_matrix *L, *Q, *Bp, *T1, *G, *V;
    gsl_vector *tau, *sv, *wk;
    real out = -1.0;

    if (r <= 0 || s <= 0) return -1.0;

    /*  Lambda_perp: las ultimas s columnas de la Q de la QR de Lambda.       */
    L   = gsl_matrix_alloc(M, r);
    Q   = gsl_matrix_alloc(M, M);
    tau = gsl_vector_alloc(r < M ? r : M);
    for (i = 0; i < M; i++)
        for (j = 0; j < r; j++) gsl_matrix_set(L, i, j, Lam[i+1][j+1]);
    {
        gsl_matrix *R = gsl_matrix_alloc(M, r);
        gsl_linalg_QR_decomp(L, tau);
        gsl_linalg_QR_unpack(L, tau, Q, R);
        gsl_matrix_free(R);
    }

    /*  B_perp = [-B2' ; I_s], ortonormalizada igual.                         */
    Bp = gsl_matrix_alloc(M, s);
    for (j = 0; j < s; j++) {
        for (i = 0; i < r; i++) gsl_matrix_set(Bp, i, j, -B2[j+1][i+1]);
        for (i = 0; i < s; i++) gsl_matrix_set(Bp, r+i, j, (i == j) ? 1.0 : 0.0);
    }
    {
        gsl_matrix *Qb = gsl_matrix_alloc(M, M), *Rb = gsl_matrix_alloc(M, s);
        gsl_vector *tb = gsl_vector_alloc(s < M ? s : M);
        gsl_linalg_QR_decomp(Bp, tb);
        gsl_linalg_QR_unpack(Bp, tb, Qb, Rb);
        for (i = 0; i < M; i++)
            for (j = 0; j < s; j++) gsl_matrix_set(Bp, i, j, gsl_matrix_get(Qb, i, j));
        gsl_vector_free(tb); gsl_matrix_free(Rb); gsl_matrix_free(Qb);
    }

    /*  Theta(1) = I - sum_k Theta_k                                          */
    T1 = gsl_matrix_alloc(M, M);
    for (i = 0; i < M; i++)
        for (j = 0; j < M; j++) {
            real acc = (i == j) ? 1.0 : 0.0;
            for (k = 1; k <= q; k++) acc -= Th[k][i+1][j+1];
            gsl_matrix_set(T1, i, j, acc);
        }

    /*  G = Lambda_perp' Theta(1) B_perp,  con Lambda_perp = Q[:, r..M-1]     */
    G = gsl_matrix_alloc(s, s);
    for (i = 0; i < s; i++)
        for (j = 0; j < s; j++) {
            real acc = 0.0;
            for (int a = 0; a < M; a++)
                for (int b = 0; b < M; b++)
                    acc += gsl_matrix_get(Q, a, r+i) * gsl_matrix_get(T1, a, b)
                         * gsl_matrix_get(Bp, b, j);
            gsl_matrix_set(G, i, j, acc);
        }

    V  = gsl_matrix_alloc(s, s);
    sv = gsl_vector_alloc(s);
    wk = gsl_vector_alloc(s);
    if (gsl_linalg_SV_decomp(G, V, sv, wk) == 0) out = gsl_vector_get(sv, s-1);
    gsl_vector_free(wk); gsl_vector_free(sv); gsl_matrix_free(V);
    gsl_matrix_free(G); gsl_matrix_free(T1); gsl_matrix_free(Bp);
    gsl_vector_free(tau); gsl_matrix_free(Q); gsl_matrix_free(L);
    return out;
}

static int    prof_hold = 0;
static real ***hold_F = NULL, ***hold_Th = NULL;
static real  **hold_S = NULL;
static int     hold_nf = 0, hold_q = 0, hold_M = 0;
static real    gate_seed_ll0 = 0.0, gate_seed_ll1 = 0.0;  /* r=0 y condicional */
static real    gate_seed_lam = 0.0;       /* el multiplo de Lambda admisible   */
static real    gate_seed_ll_start = 0.0;  /* logL en ese arranque              */
static int     gate_seed_ok  = 0;

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
/*  par_blocks — el vector de parametros, partido en los tres tramos que el
 *  perfilado necesita separar, y en UN solo sitio.
 *
 *    cabeza  la media y Lambda      lo que el paso condicional estima
 *    medio   F, Theta y Sigma       lo que el paso condicional sujeta
 *    cola    B2                     lo que el paso condicional estima
 *
 *  Cabeza y cola son contiguas por los dos extremos del vector, que es lo que
 *  hace barato el modo condicional: quitar el tramo de en medio no reordena
 *  nada.  Se calcula aqui y no en cada sitio porque este programa ya tiene
 *  CUATRO recorridos del mismo vector -- calc_nparametrs, init_guess,
 *  vec_shootx y el impresor -- y anadir un quinto criterio de conteo suelto es
 *  exactamente como se abrio el fallo de 4.1.                                */
static void par_blocks(int *nmean, int *nlam, int *nmid, int *ntail)
{
    int M = nser, r = global_r, s = M - r;
    int p = global_p, q = global_q;
    int nf = (p > 1) ? p - 1 : 0;

    /* 1. Mean E[Ȳ_t] */
    *nmean = (global_case == 2 ? r : (global_case == 3 ? M : 0));

    /* 2. Lambda (M x r), o psi (sa x r) con alpha = A*psi */
    *nlam  = (global_alpha ? alpha_sa : M) * r;

    /*  -warma: los bloques de en medio son (p-1) matrices M x r -- los
     *  coeficientes de W_{t-k} -- y q matrices r x r de medias moviles.      */
    if (global_warma) {
        *nmid = nf * M * r + q * r * r
              + (global_diag_cov ? M : M * (M + 1) / 2) - 1;
        *ntail = global_fixb2 ? 0 : s * r;
        return;
    }

    /* 3. F_i (M x M, i=1..p-1)   4. Theta_j (M x M, j=1..q)
       5. Sigma (triangulo inferior), menos la escala redundante.
          El motor llama a elf con sigma2 = 1 y concentra la escala, asi que el
          objetivo es exactamente invariante a reescalar este bloque
          (f1 -> f1/c, f2 -> c^m f2).  Llevar el triangulo entero dejaria una
          direccion que la verosimilitud no ve: una cresta plana que hace
          fallar la busqueda lineal y hace singular el hessiano.  Sigma[1][1]
          queda fijo en 1 y la escala se reporta por sigma2 (de modo que
          Sigma[1][1] = sigma2 exactamente).                                  */
    *nmid  = nf * (global_diag_ar ? M : M * M)
           + q  * (global_mawarma ? r * r
                  : (global_marow ? r * M
                  : (global_matri  ? M * M - s * r
                                   : (global_diag_ma ? M : M * M))))
           + (global_diag_cov ? M : M * (M + 1) / 2) - 1;

    /* 6. B_2 (s x r), salvo que -fixb2 lo sujete */
    *ntail = global_fixb2 ? 0 : s * r;
}

static int calc_nparametrs(void)
{
    int nmean, nlam, nmid, ntail;
    par_blocks(&nmean, &nlam, &nmid, &ntail);
    return nmean + nlam + (prof_hold ? 0 : nmid) + ntail;
}

/*****************************************************************************/
/*  warma_inverse — DE VUELTA A LAS COORDENADAS VEC.                          */
/*                                                                           */
/*  -warma estima el sistema transformado, que es donde la clase de los       */
/*  teoremas es un patron de ceros y donde B2 no toca ningun parametro.  Pero */
/*  lo que un usuario necesita leer es Lambda, F, Pi y la condicion de rango, */
/*  asi que la transformacion se INVIERTE UNA VEZ AL FINAL -- que es la       */
/*  direccion dual que describe ESTUDIO_BVECM_vs_DRVEC.md 1, y lo que el      */
/*  analisis_BEC del legado hace.                                            */
/*                                                                           */
/*  LAS ECUACIONES.  De PhiBar_k = Cinv Phi*_k y de la recursion (16):        */
/*                                                                           */
/*      F_1     = (PhiBar_1 - E) Cbar + Pi          con E = Cinv Hbar        */
/*      F_i     = (PhiBar_i + F_{i-1} E) Cbar       (i = 2 .. p-1)           */
/*      0       = PhiBar_p + F_{p-1} E              (la que queda)           */
/*                                                                           */
/*  y Pi = LamBar Cbar = Lambda B', asi que la ultima ecuacion determina      */
/*  Lambda.  Con p = 2 y M = 2 sale en cerrado -- comprobado con sympy --:    */
/*  Lambda = -(PhiBar_2)_{:,s+1..M} - ((PhiBar_1 - E) Cbar)_{:,1..r}.  Aqui   */
/*  se resuelve el caso general por minimos cuadrados sobre un sistema AFIN   */
/*  en Lambda, que evita un analisis de casos por p y da ademas el RESIDUO:   */
/*  si el punto no estuviera en la imagen del mapa, el residuo lo diria en    */
/*  vez de que el programa publicara una Lambda inventada.                    */
/*                                                                           */
/*  Theta_j = Cinv Theta*_j Cbar y Sigma = Cinv Sigma* Cinv', que son las     */
/*  mismas relaciones del teorema 1 leidas al reves.                          */
/*****************************************************************************/
static void wi_forward(real ***PhB, real **Cbar, real **E, real **Lam,
                       int M, int r, int p, real ***F, real **R)
{
    int s = M - r, nf = (p > 1) ? p - 1 : 0;
    int i, j, k;
    real **T1 = matrix(1, M, 1, M), **T2 = matrix(1, M, 1, M);

    /*  LamBar = [0, Lambda], y Pi = LamBar Cbar = Lambda B'.                */
    for (i = 1; i <= M; i++)
        for (j = 1; j <= M; j++) T1[i][j] = 0.0;
    for (i = 1; i <= M; i++)
        for (j = 1; j <= r; j++) T1[i][s + j] = Lam[i][j];
    matrix_multiply(T1, Cbar, T2, M, M, M);          /* T2 = Pi */

    if (nf >= 1) {
        for (i = 1; i <= M; i++)
            for (j = 1; j <= M; j++) T1[i][j] = PhB[1][i][j] - E[i][j];
        {
            real **T3 = matrix(1, M, 1, M);
            matrix_multiply(T1, Cbar, T3, M, M, M);
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) F[1][i][j] = T3[i][j] + T2[i][j];
            free_matrix(T3, 1, M, 1, M);
        }
        for (k = 2; k <= nf; k++) {
            real **T3 = matrix(1, M, 1, M), **T4 = matrix(1, M, 1, M);
            matrix_multiply(F[k-1], E, T3, M, M, M);
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) T3[i][j] += PhB[k][i][j];
            matrix_multiply(T3, Cbar, T4, M, M, M);
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) F[k][i][j] = T4[i][j];
            free_matrix(T4, 1, M, 1, M);
            free_matrix(T3, 1, M, 1, M);
        }
        /*  El residuo de la ecuacion que queda: PhiBar_p + F_{p-1} E.        */
        matrix_multiply(F[nf], E, T1, M, M, M);
        for (i = 1; i <= M; i++)
            for (j = 1; j <= M; j++) R[i][j] = PhB[p][i][j] + T1[i][j];
    } else {
        /*  p = 1: la unica ecuacion es PhiBar_1 = E - LamBar, y su residuo. */
        for (i = 1; i <= M; i++)
            for (j = 1; j <= M; j++) R[i][j] = PhB[1][i][j] - E[i][j];
        for (i = 1; i <= M; i++)
            for (j = 1; j <= r; j++) R[i][s + j] += Lam[i][j];
    }
    free_matrix(T2, 1, M, 1, M);
    free_matrix(T1, 1, M, 1, M);
}

static real warma_inverse(struct Tvarma *v, real **B2, real **Lam, real ***F,
                          real ***Th, real **Sg)
{
    int M = nser, r = global_r, s = M - r, p = global_p, q = global_q;
    int nf = (p > 1) ? p - 1 : 0, nl = M * r;
    int i, j, k, a, b, c;
    real **Cbar = matrix(1, M, 1, M), **Cinv = matrix(1, M, 1, M);
    real **Hbar = matrix(1, M, 1, M), **E = matrix(1, M, 1, M);
    real ***PhB = tensor(1, p, 1, M, 1, M);
    real **R0 = matrix(1, M, 1, M), **R1 = matrix(1, M, 1, M);
    real **J  = matrix(1, M * M, 1, (nl > 0 ? nl : 1));
    real **N  = matrix(1, (nl > 0 ? nl : 1), 1, (nl > 0 ? nl : 1));
    real  *rhs = vector(1, (nl > 0 ? nl : 1));
    int   *ind = ivector(1, (nl > 0 ? nl : 1));
    real  res = 0.0;

    for (i = 1; i <= M; i++)
        for (j = 1; j <= M; j++) {
            Cbar[i][j] = 0.0; Cinv[i][j] = 0.0; Hbar[i][j] = 0.0;
        }
    for (i = 1; i <= s; i++) Cbar[i][r + i] = 1.0;
    for (j = 1; j <= r; j++) Cbar[s + j][j] = 1.0;
    for (j = 1; j <= r; j++)
        for (i = 1; i <= s; i++) Cbar[s + j][r + i] = B2[i][j];
    for (i = 1; i <= r; i++)
        for (j = 1; j <= s; j++) Cinv[i][j] = -B2[j][i];
    for (i = 1; i <= r; i++) Cinv[i][s + i] = 1.0;
    for (i = 1; i <= s; i++) Cinv[r + i][i] = 1.0;
    for (i = 1; i <= r; i++) Hbar[s + i][s + i] = 1.0;
    matrix_multiply(Cinv, Hbar, E, M, M, M);

    for (k = 1; k <= p; k++) matrix_multiply(Cinv, v->phi[k], PhB[k], M, M, M);
    for (k = 1; k <= q; k++) {
        real **T = matrix(1, M, 1, M);
        matrix_multiply(Cinv, v->theta[k], T, M, M, M);
        matrix_multiply(T, Cbar, Th[k], M, M, M);
        free_matrix(T, 1, M, 1, M);
    }
    {
        real **T = matrix(1, M, 1, M);
        matrix_multiply(Cinv, v->qq, T, M, M, M);
        for (i = 1; i <= M; i++)
            for (j = 1; j <= M; j++) {
                real ss = 0.0;
                for (k = 1; k <= M; k++) ss += T[i][k] * Cinv[j][k];
                Sg[i][j] = ss;
            }
        free_matrix(T, 1, M, 1, M);
    }

    /*  El sistema afin en Lambda: R(Lambda) = R0 + J vec(Lambda).           */
    for (i = 1; i <= M; i++) for (j = 1; j <= r; j++) Lam[i][j] = 0.0;
    wi_forward(PhB, Cbar, E, Lam, M, r, p, F, R0);
    c = 0;
    for (a = 1; a <= M; a++)
        for (b = 1; b <= r; b++) {
            c++;
            Lam[a][b] = 1.0;
            wi_forward(PhB, Cbar, E, Lam, M, r, p, F, R1);
            Lam[a][b] = 0.0;
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++)
                    J[(i-1)*M + j][c] = R1[i][j] - R0[i][j];
        }
    for (a = 1; a <= nl; a++) {
        for (b = 1; b <= nl; b++) {
            real ss = 0.0;
            for (i = 1; i <= M * M; i++) ss += J[i][a] * J[i][b];
            N[a][b] = ss;
        }
        {
            real ss = 0.0;
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) ss += J[(i-1)*M + j][a] * R0[i][j];
            rhs[a] = -ss;
        }
    }
    if (nl > 0) {
        ludcp(N, nl, ind);
        lusol(N, rhs, nl, ind);
        c = 0;
        for (a = 1; a <= M; a++)
            for (b = 1; b <= r; b++) Lam[a][b] = rhs[++c];
    }
    wi_forward(PhB, Cbar, E, Lam, M, r, p, F, R1);
    for (i = 1; i <= M; i++)
        for (j = 1; j <= M; j++) res += R1[i][j] * R1[i][j];
    res = sqrt(res);

    free_ivector(ind, 1, (nl > 0 ? nl : 1));
    free_vector(rhs, 1, (nl > 0 ? nl : 1));
    free_matrix(N, 1, (nl > 0 ? nl : 1), 1, (nl > 0 ? nl : 1));
    free_matrix(J, 1, M * M, 1, (nl > 0 ? nl : 1));
    free_matrix(R1, 1, M, 1, M);
    free_matrix(R0, 1, M, 1, M);
    free_tensor(PhB, 1, p, 1, M, 1, M);
    free_matrix(E, 1, M, 1, M);
    free_matrix(Hbar, 1, M, 1, M);
    free_matrix(Cinv, 1, M, 1, M);
    free_matrix(Cbar, 1, M, 1, M);
    (void) nf;
    return res;
}

/*****************************************************************************/
/*  canonical_b2 — B2 POR LA SOLUCION CANONICA DE RANGO REDUCIDO (Johansen). */
/*                                                                           */
/*  QUE ES.  El estimador de Johansen resuelve el vector de cointegracion en  */
/*  FORMA CERRADA, por un problema de autovalores, sin optimizar nada:        */
/*                                                                           */
/*    R0  residuos de regresar nabla Y_t en los nabla Y retardados            */
/*    R1  residuos de regresar Y_{t-1}   en los mismos                        */
/*    S_ij = R_i' R_j / T                                                     */
/*    |lambda S11 - S10 S00^-1 S01| = 0,  beta = los r autovectores mayores   */
/*                                                                           */
/*  POR QUE COMO SEMILLA.  Porque ya esta medido lo cerca que cae del optimo  */
/*  de este programa, y se midio para otra cosa: HOMOLOGATION.md 2.1b compara */
/*  las dos rutas en la MISMA especificacion (q = 0) sobre los ocho pares y   */
/*  las encuentra a entre 0.0003 y 0.052 la una de la otra, en 24             */
/*  comparaciones.  Ninguna otra semilla de las que este programa ha probado  */
/*  esta a esa distancia: la de (C) arranca 11 a 17 unidades de logL por      */
/*  debajo del optimo y la de (B) llega a equivocar el signo de B2 (4b, 4c).  */
/*                                                                           */
/*  CONVENIOS, que es donde esto se rompe si se rompe.  La normalizacion de   */
/*  drvec es B = [I_r ; B2] sobre Y = [Y1 ; Y2], o sea W = Y1 + B2'Y2, asi    */
/*  que el beta canonico -- que sale normalizado como quiera el autovector -- */
/*  hay que RENORMALIZARLO dividiendo por su bloque superior r x r.  Y alpha  */
/*  no se calcula aqui: la regresion condicional que init_guess ya hace, con  */
/*  el W canonico, ES la formula de alpha de Johansen, alpha = S01 beta       */
/*  (beta' S11 beta)^-1, de modo que pedirla dos veces seria escribir dos     */
/*  implementaciones del mismo estimador.  El signo tambien lo pone esa       */
/*  regresion: drvec lleva -Lambda(W - E[W]), luego Lambda = -alpha.          */
/*                                                                           */
/*  Devuelve 1 si dejo un B2 nuevo, 0 si no pudo (y entonces vale el de OLS   */
/*  estatico, que es la ruta de siempre).                                     */
/*****************************************************************************/
static int canonical_b2(real **B2)
{
    int M = nser, r = global_r, s = M - r, p = global_p;
    int nf = (p > 1) ? p - 1 : 0;
    int T  = nobs - p;
    int nd = nf * M;                 /* los nabla Y retardados, sin constante  */
    int i, j, k, t, ok = 0;
    real **Y2lev = Y2_levels;
    real **R0, **R1, **S00, **S01, **S11, **A, **bet;
    gsl_matrix *Ag, *Bg, *evec;
    gsl_vector *eval;
    gsl_eigen_gensymmv_workspace *ws;

    if (r <= 0 || T <= nd + M + 1) return 0;

    R0 = matrix(1, T, 1, M);
    R1 = matrix(1, T, 1, M);
    for (t = p + 1; t <= nobs; t++) {
        int row = t - p;
        for (j = 1; j <= r; j++) {
            R0[row][j]     = datamat[t][s+j] - datamat[t-1][s+j];   /* nabla Y1 */
            R1[row][j]     = datamat[t-1][s+j];                     /* Y1_{t-1} */
        }
        for (i = 1; i <= s; i++) {
            R0[row][r+i]   = datamat[t][i];         /* nabla Y2, ya diferenciado */
            R1[row][r+i]   = Y2lev[t-1][i];         /* Y2_{t-1} en niveles       */
        }
    }

    /*  Las dos regresiones auxiliares, con constante: la constante restringida
     *  a la relacion es el caso 2 de drvec y el det_order = 0 con el que se
     *  hizo la comparacion externa, asi que centrar es lo que corresponde.    */
    {
        int nc = nd + 1;                        /* +1 por la constante         */
        real **D = matrix(1, T, 1, nc);
        real **XtX = matrix(1, nc, 1, nc);
        real  *Xty = vector(1, nc);
        int   *ind = ivector(1, nc);
        int    c, c2, e;

        for (t = p + 1; t <= nobs; t++) {
            int row = t - p; c = 1;
            D[row][c++] = 1.0;
            for (k = 1; k <= nf; k++) {
                for (j = 1; j <= r; j++)
                    D[row][c++] = datamat[t-k][s+j] - datamat[t-k-1][s+j];
                for (i = 1; i <= s; i++)
                    D[row][c++] = datamat[t-k][i];
            }
        }
        for (c = 1; c <= nc; c++)
            for (c2 = 1; c2 <= nc; c2++) {
                real ss = 0.0;
                for (t = 1; t <= T; t++) ss += D[t][c] * D[t][c2];
                XtX[c][c2] = ss;
            }
        ludcp(XtX, nc, ind);
        for (e = 1; e <= M; e++) {
            real **RR;
            int w;
            for (w = 0; w < 2; w++) {
                RR = w ? R1 : R0;
                for (c = 1; c <= nc; c++) {
                    Xty[c] = 0.0;
                    for (t = 1; t <= T; t++) Xty[c] += D[t][c] * RR[t][e];
                }
                lusol(XtX, Xty, nc, ind);
                for (t = 1; t <= T; t++) {
                    real fit = 0.0;
                    for (c = 1; c <= nc; c++) fit += Xty[c] * D[t][c];
                    RR[t][e] -= fit;
                }
            }
        }
        free_ivector(ind, 1, nc);
        free_vector(Xty, 1, nc);
        free_matrix(XtX, 1, nc, 1, nc);
        free_matrix(D, 1, T, 1, nc);
    }

    S00 = matrix(1, M, 1, M);
    S01 = matrix(1, M, 1, M);
    S11 = matrix(1, M, 1, M);
    for (i = 1; i <= M; i++)
        for (j = 1; j <= M; j++) {
            real a0 = 0.0, a1 = 0.0, a2 = 0.0;
            for (t = 1; t <= T; t++) {
                a0 += R0[t][i] * R0[t][j];
                a1 += R0[t][i] * R1[t][j];
                a2 += R1[t][i] * R1[t][j];
            }
            S00[i][j] = a0 / T; S01[i][j] = a1 / T; S11[i][j] = a2 / T;
        }

    /*  A = S10 S00^-1 S01, simetrica semidefinida positiva.                  */
    A = matrix(1, M, 1, M);
    {
        real **S00i = matrix(1, M, 1, M);
        real  *col  = vector(1, M);
        int   *ind  = ivector(1, M);
        real **cp   = matrix(1, M, 1, M);
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) cp[i][j] = S00[i][j];
        ludcp(cp, M, ind);
        for (j = 1; j <= M; j++) {
            for (i = 1; i <= M; i++) col[i] = (i == j) ? 1.0 : 0.0;
            lusol(cp, col, M, ind);
            for (i = 1; i <= M; i++) S00i[i][j] = col[i];
        }
        for (i = 1; i <= M; i++)
            for (j = 1; j <= M; j++) {
                real ss = 0.0;
                for (k = 1; k <= M; k++)
                    for (t = 1; t <= M; t++)
                        ss += S01[k][i] * S00i[k][t] * S01[t][j];
                A[i][j] = ss;
            }
        free_matrix(cp, 1, M, 1, M);
        free_ivector(ind, 1, M);
        free_vector(col, 1, M);
        free_matrix(S00i, 1, M, 1, M);
    }

    Ag = gsl_matrix_alloc(M, M); Bg = gsl_matrix_alloc(M, M);
    evec = gsl_matrix_alloc(M, M); eval = gsl_vector_alloc(M);
    ws = gsl_eigen_gensymmv_alloc(M);
    for (i = 1; i <= M; i++)
        for (j = 1; j <= M; j++) {
            gsl_matrix_set(Ag, i-1, j-1, 0.5 * (A[i][j] + A[j][i]));
            gsl_matrix_set(Bg, i-1, j-1, 0.5 * (S11[i][j] + S11[j][i]));
        }
    if (gsl_eigen_gensymmv(Ag, Bg, eval, evec, ws) == 0) {
        gsl_eigen_gensymmv_sort(eval, evec, GSL_EIGEN_SORT_VAL_DESC);
        bet = matrix(1, M, 1, r);
        for (i = 1; i <= M; i++)
            for (j = 1; j <= r; j++) bet[i][j] = gsl_matrix_get(evec, i-1, j-1);

        /*  Renormalizar sobre el bloque superior r x r: beta -> beta inv(Btop),
         *  que es lo que hace que las r primeras filas sean I_r y las s de
         *  abajo sean B2.  Si ese bloque es singular la normalizacion de drvec
         *  no existe para estos datos, y entonces NO se siembra: preferible a
         *  sembrar un numero enorme.                                          */
        {
            real **Bt = matrix(1, r, 1, r);
            real  *z  = vector(1, r);
            int   *ind = ivector(1, r);
            int    sing = 0;
            for (i = 1; i <= r; i++)
                for (j = 1; j <= r; j++) Bt[i][j] = bet[j][i];   /* Btop' */
            ludcp(Bt, r, ind);
            for (i = 1; i <= r; i++) if (fabs(Bt[i][i]) < 1.0e-12) sing = 1;
            if (!sing) {
                real **bn = matrix(1, M, 1, r);
                for (i = 1; i <= M; i++) {
                    for (j = 1; j <= r; j++) z[j] = bet[i][j];
                    lusol(Bt, z, r, ind);
                    for (j = 1; j <= r; j++) bn[i][j] = z[j];
                }
                for (i = 1; i <= s; i++)
                    for (j = 1; j <= r; j++) B2[i][j] = bn[r+i][j];
                ok = 1;
                free_matrix(bn, 1, M, 1, r);
            }
            free_ivector(ind, 1, r);
            free_vector(z, 1, r);
            free_matrix(Bt, 1, r, 1, r);
        }
        free_matrix(bet, 1, M, 1, r);
    }
    gsl_eigen_gensymmv_free(ws);
    gsl_vector_free(eval); gsl_matrix_free(evec);
    gsl_matrix_free(Bg); gsl_matrix_free(Ag);
    free_matrix(A, 1, M, 1, M);
    free_matrix(S11, 1, M, 1, M);
    free_matrix(S01, 1, M, 1, M);
    free_matrix(S00, 1, M, 1, M);
    free_matrix(R1, 1, T, 1, M);
    free_matrix(R0, 1, T, 1, M);
    return ok;
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

/*  exact_hessian_se — errores estandar por el hessiano EN EL OPTIMO.
 *
 *  EL PROBLEMA.  est() calcula la covarianza invirtiendo el hessiano que ACUMULA
 *  BFGS a lo largo de la trayectoria (raxopt lo deja en mtmp).  Eso sirve para
 *  dirigir la busqueda pero NO es la curvatura en el optimo: depende del camino
 *  recorrido y se degrada justo en las direcciones mas planas, que son las de
 *  mayor error estandar.  No es una sospecha -- drtran lo diagnostico y lo
 *  arreglo (BRIDGE_DESIGN.md 8c), y aqui se vio el sintoma extremo: con
 *  -multistart, al no iterar el est final, TODOS los errores estandar salian
 *  identicos.
 *
 *  LA ALTERNATIVA ESTABA APUNTADA EN EL PROPIO MOTOR, comentada en
 *  drvmlest.c:104-107:  fdhess(objcfunc, ...) + choldcp.  Se usa eso.
 *
 *  Y SE HACE SIN TOCAR EL MOTOR.  fdhess (qnewtopt.c) y objcfunc (drvmlest.c)
 *  son simbolos publicos; se llaman desde aqui despues de est(), cuando sus
 *  globales -- castx y varmax -- siguen apuntando a este ajuste.  La formula de
 *  la covarianza es la MISMA que usa est (drvmlest.c:111-119),
 *      cov = 2 * f * H^-1 / n,
 *  con H el hessiano del objetivo concentrado; lo unico que cambia es de donde
 *  sale H.
 *
 *  Devuelve 0 si pudo; deja dev y cov sobrescritos.                          */
extern void fdhess(real (*func)(real *), int n, real *x, real f, real eta,
                   real **H);

/*  El objetivo, replicado aqui con SU PROPIA estructura.
 *
 *  No se puede reutilizar el objcfunc del motor: est() termina llamando al cast
 *  con lastx = 1, que DESASIGNA la estructura, asi que llamarlo despues escribe
 *  en memoria liberada -- comprobado, segfault.  Y no hay punto de entrada para
 *  que la reasigne.
 *
 *  La formula es la de drvmlest.c:159-190, y la CONSTANTE DE NORMALIZACION DA
 *  IGUAL: si g = c*f, entonces H_g = c*H_f y 2*g*H_g^-1 = 2*f*H_f^-1, o sea que
 *  la covarianza no depende de c.  Se normaliza por el valor en el optimo, que
 *  deja el objetivo en 1 y es lo mas comodo numericamente.                    */
static struct Tvarma  fdh_varma;
static int            hess_nneg = 0;
static long           fdh_rej = 0;
static real           hess_ratio = 0.0;
static real           fdh_norm1 = 1.0, fdh_norm2 = 1.0;

static real fdh_obj(real *x)
{
    real pi1, pi2, pi3;
    int ifault = 0;

    vec_shootx(x, &fdh_varma, &ifault, 0, 0);
    if (ifault > 0) { fdh_rej++; return 1.0e10; }   /* Sigma no definida positiva */
    elf(fdh_varma.m, fdh_varma.n, fdh_varma.p, fdh_varma.q, fdh_varma.mu,
        fdh_varma.phi, fdh_varma.theta, fdh_varma.qq, fdh_varma.w, 1.0,
        fdh_varma.xitol, FALSE, fdh_varma.a, &pi1, &pi2, &pi3, &ifault);
    if (ifault > 0) { fdh_rej++; return 1.0e10; }   /* no estacionario / no invertible */
    return pow(pi1 / fdh_norm1, (real) fdh_varma.m) * (pi2 / fdh_norm2);
}

/*  exact_hessian_se — errores estandar por el hessiano EN EL OPTIMO.
 *
 *  EL PROBLEMA.  est() calcula la covarianza invirtiendo el hessiano que ACUMULA
 *  BFGS a lo largo de la trayectoria (raxopt lo deja en mtmp).  Eso sirve para
 *  dirigir la busqueda pero NO es la curvatura en el optimo: depende del camino
 *  recorrido y se degrada justo en las direcciones mas planas, que son las de
 *  mayor error estandar.  No es una sospecha -- drtran lo diagnostico y lo
 *  arreglo (BRIDGE_DESIGN.md 8c) -- y aqui se vio el sintoma extremo: con
 *  -multistart, al no iterar el est final, TODOS los errores estandar salian
 *  identicos.
 *
 *  LA ALTERNATIVA ESTABA APUNTADA EN EL PROPIO MOTOR, comentada en
 *  drvmlest.c:104-107:  fdhess + choldcp.  Se usa eso, sin tocar el motor:
 *  fdhess es un simbolo publico de qnewtopt.c y el objetivo es propio.
 *
 *  cov = 2 * f * H^-1 / n, la misma formula que est (drvmlest.c:111-119); lo
 *  unico que cambia es de donde sale H.  Devuelve 0 si pudo.                 */
static int exact_hessian_se(int npar, real *x, real *dev, real **cov, int neff)
{
    real **H = matrix(1, npar, 1, npar);
    real  *e = vector(1, npar);
    real d1, d2, pi1, pi2, pi3, f;
    int i, j, ifc = 0, ifault = 0;

    /* Asignar la estructura propia y fijar la normalizacion en el optimo. */
    fdh_varma.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;
    fdh_norm1 = fdh_norm2 = 1.0;
    vec_shootx(x, &fdh_varma, &ifault, 1, 0);
    if (ifault > 0) goto fail;
    elf(fdh_varma.m, fdh_varma.n, fdh_varma.p, fdh_varma.q, fdh_varma.mu,
        fdh_varma.phi, fdh_varma.theta, fdh_varma.qq, fdh_varma.w, 1.0,
        fdh_varma.xitol, FALSE, fdh_varma.a, &pi1, &pi2, &pi3, &ifault);
    if (ifault > 0) goto fail;
    fdh_norm1 = pi1; fdh_norm2 = pi2;

    f = fdh_obj(x);                      /* = 1 por construccion */
    fdh_rej = 0;
    fdhess(fdh_obj, npar, x, f, macheps, H);
    {   /* Espectro ANTES de la Cholesky, que destruye la matriz.  Si falla hay
           que poder decir CUANTO falla: un autovalor negativo minusculo es ruido
           numerico en una direccion plana, y varios grandes son un punto de
           silla -- o sea que el optimizador no paro en un maximo.            */
        real **Hc = matrix(1, npar, 1, npar);
        real *wr = vector(1, npar), *wi = vector(1, npar);
        real mx = 0.0, mn = 0.0;
        int nneg = 0, k;
        for (i = 1; i <= npar; i++) for (j = 1; j <= npar; j++) Hc[i][j] = H[i][j];
        eigenqr(Hc, npar, wr, wi);
        for (k = 1; k <= npar; k++) {
            if (wr[k] > mx) mx = wr[k];
            if (wr[k] < mn) mn = wr[k];
            if (wr[k] <= 0.0) nneg++;
        }
        hess_nneg = nneg; hess_ratio = (mx > 0.0) ? -mn / mx : 0.0;
        free_vector(wi, 1, npar); free_vector(wr, 1, npar);
        free_matrix(Hc, 1, npar, 1, npar);
    }
    choldcp(H, npar, &d1, &d2, &ifc);
    if (ifc > 0) {
        /*  DOS causas distintas, y confundirlas lleva a decir algo falso.
         *
         *  (a) fdh_rej > 0: alguna perturbacion de diferencias finitas salio de
         *      la region admisible y se le respondio con la penalizacion.  Esas
         *      filas y columnas del hessiano NO son curvatura -- son el salto a
         *      la penalizacion --, asi que su espectro no significa nada y no se
         *      informa.  Lo que dice es que el optimo esta EN la frontera: un
         *      optimo restringido, donde la curvatura libre no esta definida.
         *      Las raices que se informan mas arriba senalan cual es.
         *
         *  (b) fdh_rej == 0: el hessiano se formo entero con evaluaciones
         *      validas y aun asi es indefinido.  Ahi si es informativo, y el
         *      espectro dice cuanto.                                          */
        if (fdh_rej > 0)
            fprintf(stderr,
                "WARNING: -fdhess: the optimum lies ON the boundary of the admissible\n"
                "         region -- %ld of the finite-difference evaluations fell\n"
                "         outside it -- so the unconstrained Hessian is not defined\n"
                "         there and the BFGS standard errors are kept.  See the roots\n"
                "         of the AR and MA operators reported above: a modulus at one\n"
                "         identifies the binding direction.\n", fdh_rej);
        else
            fprintf(stderr,
                "WARNING: -fdhess: the Hessian at the reported optimum is not positive\n"
                "         definite: %d of %d eigenvalues are non-positive, the most\n"
                "         negative being %.3g times the largest positive one.  Every\n"
                "         evaluation was admissible, so the optimiser did not stop at\n"
                "         a maximum.  The BFGS standard errors are kept.\n",
                hess_nneg, npar, hess_ratio);
        goto fail;
    }
    for (i = 1; i <= npar; i++) {
        for (j = 1; j <= npar; j++) e[j] = 0.0;
        e[i] = 1.0;
        cholsol(H, npar, e);             /* e <- H^-1 e_i */
        for (j = 1; j <= npar; j++) cov[j][i] = (2.0 * f * e[j]) / neff;
        dev[i] = sqrt(cov[i][i] > 0.0 ? cov[i][i] : 0.0);
    }
    vec_shootx(x, &fdh_varma, &ifault, 0, 1);
    free_vector(e, 1, npar); free_matrix(H, 1, npar, 1, npar);
    return 0;
fail:
    vec_shootx(x, &fdh_varma, &ifault, 0, 1);
    free_vector(e, 1, npar); free_matrix(H, 1, npar, 1, npar);
    return 1;
}

/*****************************************************************************/
/*  report_operator_roots — moduli of the AR and MA roots at the optimum.     */
/*                                                                           */
/*  WHY THIS IS REPORTED.  drvec places nabla Y_2 in Ybar, so the second      */
/*  block is differenced by construction.  When the data do not need that     */
/*  differencing -- when the declared rank is too low -- the MA operator      */
/*  absorbs it with a root on the unit circle, which is the classical         */
/*  signature of overdifferencing.  The likelihood cannot go there: the       */
/*  engine's invertibility check (chekma, elfvarma.c) rejects any point whose */
/*  companion eigenvalue reaches 1.00005, so the optimiser stops ON the       */
/*  boundary.  The fit that results is a CONSTRAINED optimum, and standard    */
/*  errors from an unconstrained Hessian are not defined along that           */
/*  direction.  Reporting the roots is what lets the user see it.            */
/*                                                                           */
/*  The companion matrix is built exactly as chekma builds it, so the two     */
/*  agree by construction: for A(B) = I - A_1 B - ... - A_k B^k its           */
/*  eigenvalues are lambda = 1/z with z the roots of det A(z) = 0, whence the */
/*  modulus reported below is 1/|lambda|.                                     */
/*****************************************************************************/
/*  quiet = 1: solo calcula el modulo menor y no imprime.  Lo necesita la
 *  escalera de especificaciones, que quiere el numero de cada peldano sin la
 *  tabla de raices de cada uno.                                             */
static void report_operator_roots(const char *label, real ***A, int m, int k,
                                  real *minmod, int quiet)
{
    int mk = m * k, i, j, l;
    real **C, *wr, *wi;

    if (k <= 0) return;
    C  = matrix(1, mk, 1, mk);
    wr = vector(1, mk);
    wi = vector(1, mk);
    for (i = 1; i <= mk; i++)
        for (j = 1; j <= mk; j++) C[i][j] = 0.0;
    for (l = 1; l <= k; l++)
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) C[i][j + (l - 1) * m] = A[l][i][j];
    for (l = 1; l <= k - 1; l++)
        for (j = 1; j <= m; j++) C[j + l * m][j + (l - 1) * m] = 1.0;

    eigenqr(C, mk, wr, wi);

    if (!quiet) fprintf(outputv, "  %-11s", label);
    for (i = 1; i <= mk; i++) {
        real lam = sqrt(wr[i] * wr[i] + wi[i] * wi[i]);
        /* A null companion eigenvalue is an infinite root: it happens whenever
           the last coefficient matrix is singular, and it is no defect.      */
        if (lam <= 1.0e-12) {
            if (!quiet) fprintf(outputv, "  %8s ", "inf");
            continue;
        }
        if (1.0 / lam < *minmod) *minmod = 1.0 / lam;
        if (!quiet)
            fprintf(outputv, "  %8.5f%s", 1.0 / lam,
                    (1.0 / lam < 1.0001) ? "*" : " ");
    }
    if (!quiet) fprintf(outputv, "\n");

    free_vector(wi, 1, mk);
    free_vector(wr, 1, mk);
    free_matrix(C, 1, mk, 1, mk);
}

static void operator_roots(struct Tvarma *v)
{
    real minmod = 1.0e12;

    if (v->p <= 0 && v->q <= 0) return;
    fprintf(outputv, "\nRoots of the AR and MA operators (moduli; the model is "
                     "stationary and\ninvertible when every modulus exceeds "
                     "one):\n\n");
    report_operator_roots("AR (Phi)",   v->phi,   v->m, v->p, &minmod, 0);
    report_operator_roots("MA (Theta)", v->theta, v->m, v->q, &minmod, 0);
    if (minmod < 1.0001)
        fprintf(outputv,
            "\n  * A root sits on the unit circle.  The estimate lies against the\n"
            "    boundary the likelihood enforces, so this is a CONSTRAINED optimum\n"
            "    and the standard errors are not defined along that direction.  A\n"
            "    unit MA root here is the signature of overdifferencing: nabla Y_2\n"
            "    is differenced by construction, so it appears when the declared\n"
            "    rank is lower than the true one.  Re-examine the rank before\n"
            "    reading the estimates.\n");
}

/*****************************************************************************/
/*  F4 — bootstrap parametrico para el test de rango                          */
/*****************************************************************************/
/*  POR QUE.  Bajo H0 el estadistico de rango NO sigue una chi2, y los valores
 *  criticos asintoticos que -lrtest imprime estan medidos como insuficientes a
 *  estos tamanos: sobre 20 replicas de un proceso con r = 1 verdadero y n = 120,
 *  el test sobre-rechaza unas TRES VECES su nivel nominal (HOMOLOGATION.md 2.3).
 *  Melard, Roy y Saidi lo dicen para esta misma clase de modelos: los terminos MA
 *  no alteran la distribucion asintotica del LR, pero "finite sample performance
 *  of the test is affected by the MA terms".
 *
 *  COMO.  Bootstrap parametrico: se simulan N muestras BAJO H0 con los parametros
 *  estimados al rango r, se recalcula el estadistico LR(r -> r+1) en cada una, y
 *  los percentiles empiricos son los valores criticos.  Es lo que prescribe
 *  BVECM 6.3-6.5.
 *
 *  LA SIMULACION APROVECHA LA TRANSFORMACION, en vez de reimplementar un VEC:
 *  el modelo ajustado ES un VARMA estacionario sobre Ybar, asi que se simula ahi
 *  -- con la convencion de elf, (w-mu) = SUM phi (w-mu) + a - SUM theta a -- y se
 *  INVIERTE la transformacion para volver a niveles:
 *
 *      nabla Y2 = Ybar[1..s]         -> Y2 por acumulacion desde el nivel real
 *      Y1       = W - B2' Y2          con W = Ybar[s+1..M]
 *
 *  Eso deja una muestra en el mismo formato que el .inp, de modo que las
 *  reestimaciones son EXACTAMENTE las del camino normal, sin codigo paralelo que
 *  pueda divergir del que se quiere calibrar.
 *
 *  El generador es determinista con semilla fija: un valor critico que no se
 *  puede reproducir no sirve para decidir nada.                              */

static unsigned long boot_rng = 987654321UL;

static real boot_normal(void)
{
    /* Box-Muller sobre un LCG propio; determinista y sin depender de la libc. */
    static int have = 0;
    static real spare = 0.0;
    real u1, u2, r, th;
    if (have) { have = 0; return spare; }
    do {
        boot_rng = boot_rng * 6364136223846793005UL + 1442695040888963407UL;
        u1 = ((real)((boot_rng >> 33) & 0x7FFFFFFF)) / 2147483648.0;
    } while (u1 <= 1.0e-12);
    boot_rng = boot_rng * 6364136223846793005UL + 1442695040888963407UL;
    u2 = ((real)((boot_rng >> 33) & 0x7FFFFFFF)) / 2147483648.0;
    r  = sqrt(-2.0 * log(u1));
    th = 2.0 * M_PI * u2;
    spare = r * sin(th); have = 1;
    return r * cos(th);
}

/*  simulate_h0 — una muestra de NIVELES bajo el modelo de v, con B2 dado.
 *  out se espera dimensionada (1..nobs_raw, 1..M).  Devuelve 0 si pudo.       */
static int simulate_h0(struct Tvarma *v, real **B2, int r, real **out)
{
    int M = nser, s = M - r, n = v->n, p = v->p, q = v->q;
    int burn = 50 + 10 * p, T = n + burn;
    real **wb = matrix(1, T, 1, M);
    real **ab = matrix(1, T, 1, M);
    real **L  = matrix(1, M, 1, M);
    real d1, d2;
    int t, i, j, k, ifc = 0;

    /* Cholesky de Sigma* = sigma2 * qq para dar a los choques su covarianza. */
    for (i = 1; i <= M; i++)
        for (j = 1; j <= M; j++) L[i][j] = v->sigma2 * v->qq[i][j];
    choldcp(L, M, &d1, &d2, &ifc);
    if (ifc > 0) { free_matrix(L,1,M,1,M); free_matrix(ab,1,T,1,M);
                   free_matrix(wb,1,T,1,M); return 1; }
    for (i = 1; i <= M; i++) for (j = i+1; j <= M; j++) L[i][j] = 0.0;

    for (t = 1; t <= T; t++) {
        real *z = vector(1, M);
        for (i = 1; i <= M; i++) z[i] = boot_normal();
        for (i = 1; i <= M; i++) {
            real acc = 0.0;
            for (k = 1; k <= i; k++) acc += L[i][k] * z[k];
            ab[t][i] = acc;
        }
        free_vector(z, 1, M);
        /* (w - mu) = SUM phi_j (w - mu)_{t-j} + a_t - SUM theta_j a_{t-j} */
        for (i = 1; i <= M; i++) {
            real acc = ab[t][i];
            for (j = 1; j <= p; j++) if (t-j >= 1)
                for (k = 1; k <= M; k++) acc += v->phi[j][i][k] * (wb[t-j][k] - v->mu[k]);
            for (j = 1; j <= q; j++) if (t-j >= 1)
                for (k = 1; k <= M; k++) acc -= v->theta[j][i][k] * ab[t-j][k];
            wb[t][i] = v->mu[i] + acc;
        }
    }

    /* Invertir la transformacion.  El origen de Y2 es el real: en el caso 1 la
       constante no es libre, asi que un origen arbitrario contaminaria W.     */
    for (i = 1; i <= M; i++) out[1][i] = rawmat[1][i];
    for (t = 1; t <= n; t++) {
        int tb = t + burn;
        for (i = 1; i <= s; i++) out[t+1][i] = out[t][i] + wb[tb][i];
        for (j = 1; j <= r; j++) {
            real w = wb[tb][s+j];
            for (i = 1; i <= s; i++) w -= B2[i][j] * out[t+1][i];
            out[t+1][s+j] = w;
        }
    }
    free_matrix(L, 1, M, 1, M);
    free_matrix(ab, 1, T, 1, M);
    free_matrix(wb, 1, T, 1, M);
    return 0;
}

/*  fit_ll — reestima al rango rr y devuelve la logL, o 0 con ok = 0.          */
static real fit_ll(int rr, int *ok)
{
    int np, ifr = 0;
    real *xr, *devr, **covr, ll = 0.0;
    struct Tvarma vr;
    int save_r = global_r;

    global_r = rr;
    build_y2_levels();
    np = calc_nparametrs();
    xr = vector(1, np); devr = vector(1, np); covr = matrix(1, np, 1, np);
    vr.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;
    init_guess(xr, np);
    vec_shootx(xr, &vr, &ifr, 1, 0);
    est(&vec_shootx, np, xr, devr, covr, 500, 200, 1e-5, 1e-7,
        vr.xitol, vr.a, &vr.sigma2, &vr.logelf, &ifr);
    *ok = (ifr == 0);
    if (*ok) ll = vr.logelf;
    vec_shootx(xr, &vr, &ifr, 0, 1);
    free_matrix(covr, 1, np, 1, np); free_vector(devr, 1, np); free_vector(xr, 1, np);
    global_r = save_r;
    return ll;
}

static int cmp_real(const void *a, const void *b)
{
    real x = *(const real *)a, y = *(const real *)b;
    return (x < y) ? -1 : ((x > y) ? 1 : 0);
}

/*  bootstrap_rank — valores criticos de LR(rr -> rr+1) bajo H0: rango = rr.
 *  x es el ajuste al rango rr.  Devuelve el numero de replicas utiles.        */
static int bootstrap_rank(int rr, real *x, int npar, int N, real *cv, real *pval,
                          real lr_obs)
{
    int M = nser, s = M - rr, i, j, b, nok = 0, ifr = 0;
    real **B2 = matrix(1, (s > 0 ? s : 1), 1, (rr > 0 ? rr : 1));
    real **sim = matrix(1, nobs_raw, 1, M);
    real **saved = matrix(1, nobs_raw, 1, M);
    real *stat = vector(1, N);
    struct Tvarma vh;
    FILE *save_out = outputv;
    int save_quiet = quiet_mode, save_r = global_r, ge = 0;

    /* B2 del ajuste: ultimas s*rr entradas de x[], column-major (o fijada). */
    if (rr > 0) {
        int idx = npar - s * rr + 1;
        for (j = 1; j <= rr; j++) for (i = 1; i <= s; i++)
            B2[i][j] = global_fixb2 ? B2_fixed[i][j] : x[idx++];
    }
    /* El modelo bajo H0, recuperado del ajuste. */
    global_r = rr; build_y2_levels();
    vh.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;
    vec_shootx(x, &vh, &ifr, 1, 0);
    {   /* rellenar sigma2/logelf evaluando: vec_shootx no los pone */
        real pi1, pi2, pi3; int ife = 0;
        const real LOG2PI = 1.837877066;
        elf(vh.m, vh.n, vh.p, vh.q, vh.mu, vh.phi, vh.theta, vh.qq, vh.w, 1.0,
            vh.xitol, TRUE, vh.a, &pi1, &pi2, &pi3, &ife);
        vh.sigma2 = pi1 / (vh.n * vh.m);
        vh.logelf = -0.5*vh.m*vh.n*(LOG2PI - log((real)vh.m) - log((real)vh.n) + 1.0)
                    - 0.5*vh.n*(vh.m*log(pi1) + log(pi2));
    }

    for (i = 1; i <= nobs_raw; i++)
        for (j = 1; j <= M; j++) saved[i][j] = rawmat[i][j];

    outputv = fopen("/dev/null", "w"); quiet_mode = 1;
    for (b = 1; b <= N; b++) {
        int ok0 = 0, ok1 = 0;
        real l0, l1;
        if (simulate_h0(&vh, B2, rr, sim) != 0) continue;
        for (i = 1; i <= nobs_raw; i++)
            for (j = 1; j <= M; j++) rawmat[i][j] = sim[i][j];
        l0 = fit_ll(rr,   &ok0);
        l1 = fit_ll(rr+1, &ok1);
        if (ok0 && ok1 && l1 >= l0) stat[++nok] = 2.0 * (l1 - l0);
    }
    for (i = 1; i <= nobs_raw; i++)
        for (j = 1; j <= M; j++) rawmat[i][j] = saved[i][j];
    if (outputv) fclose(outputv);
    outputv = save_out; quiet_mode = save_quiet;
    global_r = rr; build_y2_levels();
    vec_shootx(x, &vh, &ifr, 0, 1);

    if (nok >= 10) {
        /* Cuantil (1-alpha): indice ceil((1-alpha)*nok), acotado.  Con el
           redondeo al mas cercano que habia antes, el percentil 99 de 97 valores
           caia en el puesto 96 y dejaba DOS por encima, o sea un 2% donde se
           pedia un 1%.                                                       */
        int i90 = (int) ceil(0.90 * nok), i95 = (int) ceil(0.95 * nok),
            i99 = (int) ceil(0.99 * nok);
        qsort(&stat[1], (size_t) nok, sizeof(real), cmp_real);
        if (i90 < 1) i90 = 1;
        if (i90 > nok) i90 = nok;
        if (i95 < 1) i95 = 1;
        if (i95 > nok) i95 = nok;
        if (i99 < 1) i99 = 1;
        if (i99 > nok) i99 = nok;
        cv[0] = stat[i90]; cv[1] = stat[i95]; cv[2] = stat[i99];
        for (i = 1; i <= nok; i++) if (stat[i] >= lr_obs) ge++;
        *pval = (real) (ge + 1) / (real) (nok + 1);   /* p-valor bootstrap */
    }
    free_vector(stat, 1, N);
    free_matrix(saved, 1, nobs_raw, 1, M);
    free_matrix(sim, 1, nobs_raw, 1, M);
    free_matrix(B2, 1, (s > 0 ? s : 1), 1, (rr > 0 ? rr : 1));
    global_r = save_r;
    return nok;
}

/*****************************************************************************/
/*  bootstrap_ma — la distribucion del LR entre el MA HEREDADO y el LIBRE,     */
/*  simulada bajo el restringido.                                             */
/*                                                                           */
/*  POR QUE NO BASTA LA chi2.  El estadistico compara q*r*r parametros de     */
/*  medias moviles contra q*M*M, o sea df = q(M^2 - r^2), y leido asi el       */
/*  restringido se rechaza en los ocho pares.  Pero esa lectura no vale: el    */
/*  optimo NO RESTRINGIDO se para en la vecindad donde la condicion de rango   */
/*  degenera -- G entre 0.016 y 0.133 contra ~1 de un modelo bien             */
/*  especificado, con Theta(1) singular a precision de trabajo (4h) --, y un   */
/*  LR cuyo estimador no restringido esta en el borde de la region admisible   */
/*  no tiene su distribucion asintotica.  Es la misma advertencia que 3b lleva */
/*  para los errores estandar, aplicada al contraste.                         */
/*                                                                           */
/*  Lo que se puede hacer sin distribucion asintotica es simular la que hay:   */
/*  generar bajo el modelo RESTRINGIDO -- que es H0 -- y mirar donde cae el    */
/*  estadistico observado en esa distribucion.  Cada replica cuesta DOS        */
/*  ajustes, el restringido y el libre, exactamente como el observado.        */
/*****************************************************************************/
/*  set_spec — LA ESCALERA, EN UN SOLO SITIO.  0 warma, 1 mawarma, 2 marow,
 *  3 matri, 4 libre.  Existe porque el bootstrap tiene que poder poner y quitar
 *  una especificacion entera sin que se le olvide una bandera, y porque cuatro
 *  banderas puestas a mano en cinco sitios es como se cuela una combinacion que
 *  nadie quiso.                                                              */
static void set_spec(int k)
{
    global_warma = (k == 0); global_mawarma = (k == 1);
    global_marow = (k == 2); global_matri   = (k == 3);
}

static int bootstrap_ma(real *x0, int npar0, int N, real *cv, real *pval,
                        real lr_obs, int k0, int k1)
{
    int M = nser, r = global_r, s = M - r, i, j, b, nok = 0, ifr = 0, ge = 0;
    real **B2  = matrix(1, (s > 0 ? s : 1), 1, (r > 0 ? r : 1));
    real **sim = matrix(1, nobs_raw, 1, M);
    real **saved = matrix(1, nobs_raw, 1, M);
    real *stat = vector(1, N);
    struct Tvarma vh;
    FILE *save_out = outputv;
    int save_quiet = quiet_mode, save_wa = global_mawarma;

    /*  El modelo bajo H0 es el RESTRINGIDO, asi que se recupera con SU
     *  especificacion puesta: con otra, vec_shootx leeria otro vector.       */
    set_spec(k0);
    {
        int idx = npar0 - s * r + 1;
        for (j = 1; j <= r; j++) for (i = 1; i <= s; i++)
            B2[i][j] = global_fixb2 ? B2_fixed[i][j] : x0[idx++];
    }
    build_y2_levels();
    vh.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;
    vec_shootx(x0, &vh, &ifr, 1, 0);
    {
        real pi1, pi2, pi3; int ife = 0;
        const real LOG2PI = 1.837877066;
        elf(vh.m, vh.n, vh.p, vh.q, vh.mu, vh.phi, vh.theta, vh.qq, vh.w, 1.0,
            vh.xitol, TRUE, vh.a, &pi1, &pi2, &pi3, &ife);
        vh.sigma2 = pi1 / (vh.n * vh.m);
        vh.logelf = -0.5*vh.m*vh.n*(LOG2PI - log((real)vh.m) - log((real)vh.n) + 1.0)
                    - 0.5*vh.n*(vh.m*log(pi1) + log(pi2));
    }

    for (i = 1; i <= nobs_raw; i++)
        for (j = 1; j <= M; j++) saved[i][j] = rawmat[i][j];

    outputv = fopen("/dev/null", "w"); quiet_mode = 1;
    for (b = 1; b <= N; b++) {
        int ok0 = 0, ok1 = 0;
        real l0, l1;
        set_spec(k0);
        if (simulate_h0(&vh, B2, r, sim) != 0) continue;
        for (i = 1; i <= nobs_raw; i++)
            for (j = 1; j <= M; j++) rawmat[i][j] = sim[i][j];
        set_spec(k0); l0 = fit_ll(r, &ok0);
        set_spec(k1); l1 = fit_ll(r, &ok1);
        /*  Se descartan las replicas donde el libre acaba POR DEBAJO del
         *  restringido: el restringido esta anidado, asi que un LR negativo es
         *  un ajuste que no convergio, no una realizacion del estadistico.   */
        if (ok0 && ok1 && l1 >= l0) stat[++nok] = 2.0 * (l1 - l0);
    }
    for (i = 1; i <= nobs_raw; i++)
        for (j = 1; j <= M; j++) rawmat[i][j] = saved[i][j];
    if (outputv) fclose(outputv);
    outputv = save_out; quiet_mode = save_quiet;
    set_spec(k0);
    build_y2_levels();
    vec_shootx(x0, &vh, &ifr, 0, 1);
    set_spec(-1); global_mawarma = save_wa;

    if (nok >= 10) {
        int i90 = (int) ceil(0.90 * nok), i95 = (int) ceil(0.95 * nok),
            i99 = (int) ceil(0.99 * nok);
        qsort(&stat[1], (size_t) nok, sizeof(real), cmp_real);
        if (i90 < 1) i90 = 1;
        if (i90 > nok) i90 = nok;
        if (i95 < 1) i95 = 1;
        if (i95 > nok) i95 = nok;
        if (i99 < 1) i99 = 1;
        if (i99 > nok) i99 = nok;
        cv[0] = stat[i90]; cv[1] = stat[i95]; cv[2] = stat[i99];
        for (i = 1; i <= nok; i++) if (stat[i] >= lr_obs) ge++;
        *pval = (real) (ge + 1) / (real) (nok + 1);
    }
    free_vector(stat, 1, N);
    free_matrix(saved, 1, nobs_raw, 1, M);
    free_matrix(sim, 1, nobs_raw, 1, M);
    free_matrix(B2, 1, (s > 0 ? s : 1), 1, (r > 0 ? r : 1));
    return nok;
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
static int   global_fdhess   = 0;    /* -fdhess: errores estandar por hessiano
                                        de diferencias finitas en el optimo   */
static int   global_boot     = 0;    /* -bootstrap N: valores criticos por
                                        bootstrap parametrico bajo H0         */
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
static real   gate_ll_start  = 0.0;  /* logL EN los valores traidos (pre-ajuste) */
static int    gate_have_start = 0;   /* 1 si se pudo evaluar antes de optimizar  */
static real   gate_move      = 0.0;  /* mayor desplazamiento de un coeficiente   */
static int    gate_have_move = 0;

/*  free_seed_pre — libera los cuatro bufers de siembra.
 *
 *  No existia: se asignaban en load_seed_pre y nunca se liberaban.  La fuga era
 *  invisible mientras vector() devolvia la base del bloque, porque valgrind veia
 *  un puntero al principio y lo daba por "todavia alcanzable"; al alinear
 *  vector() con la suite el puntero guardado apunta dentro del bloque y la fuga
 *  aparece como definitely lost.  O sea que el asignador con desplazamiento
 *  DETECTA mejor, y esto es lo primero que saco a la luz.
 *
 *  Los limites tienen que ser los de la asignacion, no los logicos: con q = 0 o
 *  p = 1 se reservo una fila igualmente.                                      */
static void free_seed_pre(void)
{
    int M = nser, q = global_q;
    int nf = (global_p > 1) ? global_p - 1 : 0;

    if (seed_tbar) { free_matrix(seed_tbar, 1, (q  > 0 ? q  : 1), 1, M);
                     seed_tbar = NULL; }
    if (seed_phi)  { free_matrix(seed_phi,  1, (nf > 0 ? nf : 1), 1, M);
                     seed_phi  = NULL; }
    if (seed_var)  { free_vector(seed_var,  1, M); seed_var  = NULL; }
    if (seed_logl) { free_vector(seed_logl, 1, M); seed_logl = NULL; }
}

/*****************************************************************************/
/*  gate_contract — the entry gate verifies ITSELF, by the ladder's contract. */
/*                                                                           */
/*  WHY.  At the diagonal rung -- r = 0 with diagonal Phi, Theta and Sigma -- */
/*  the exact likelihood FACTORISES: the joint model is M independent         */
/*  univariate models, so                                                    */
/*                                                                           */
/*      logL(joint diagonal fit)  =  SUM_i logL(univariate i).               */
/*                                                                           */
/*  That is an identity, not an approximation, and it is the sharpest check   */
/*  the program has on everything upstream of the likelihood: the            */
/*  transformation, the differencing implied by the rank, the layout of the   */
/*  parameter vector, the deterministic terms subtracted, and the scaling.    */
/*  If any of them is wrong the two sides part company; if all are right they */
/*  agree to rounding.  It is the same contract the suite states for its own  */
/*  entry gate, and it is checked here rather than only in the test suite,    */
/*  so that any run at the diagonal rung certifies itself.                    */
/*                                                                           */
/*  HOW.  The univariate side is computed from the FITTED diagonal blocks --  */
/*  no external file is involved -- by evaluating the same elf() with m = 1   */
/*  on each component in turn.  The scale is concentrated in both cases, so   */
/*  the two sides are on the same footing.                                    */
/*****************************************************************************/
static void gate_contract(struct Tvarma *v)
{
    const real LOG2PI = 1.837877066;
    int M = v->m, n = v->n, p = v->p, q = v->q, i, k, t;
    real sum = 0.0;
    int failed = 0;

    if (global_r != 0 || !global_diag_ar || !global_diag_ma || !global_diag_cov)
        return;                       /* la factorizacion solo vale aqui */

    /*  Senal legible al lado del hueco: cuanto se movio el coeficiente que mas
     *  se movio.  Se compara la semilla con el ajuste en la MISMA convencion,
     *  la del motor, para que un cambio de signo no se lea como movimiento.  */
    if (seed_have_uv && seed_tbar) {
        gate_move = 0.0;
        for (k = 1; k <= q; k++)
            for (i = 1; i <= M; i++) {
                real d = fabs(seed_tbar[k][i] - v->theta[k][i][i]);
                if (d > gate_move) gate_move = d;
            }
        gate_have_move = 1;
    }

    fprintf(outputv,
        "\n=== Entry gate: the factorisation contract ===\n\n"
        "At r = 0 with diagonal Phi, Theta and Sigma the likelihood factorises,\n"
        "so the joint fit must equal the sum of the univariate fits exactly.\n\n");

    for (i = 1; i <= M; i++) {
        struct Tvarma u;
        real pi1, pi2, pi3, ll = 0.0;
        int ifault = 0;

        u.m = 1; u.n = n; u.p = p; u.q = q; u.xitol = v->xitol;
        u.mu    = vector(1, 1);
        u.phi   = tensor(0, (p > 0 ? p : 1), 1, 1, 1, 1);
        u.theta = tensor(0, (q > 0 ? q : 1), 1, 1, 1, 1);
        u.qq    = matrix(1, 1, 1, 1);
        u.w     = matrix(1, n, 1, 1);
        u.a     = matrix(1, n, 1, 1);

        u.mu[1] = v->mu[i];
        u.qq[1][1] = 1.0;                 /* la escala se concentra, como arriba */
        u.phi[0][1][1] = 1.0; u.theta[0][1][1] = 1.0;
        for (k = 1; k <= p; k++) u.phi[k][1][1]   = v->phi[k][i][i];
        for (k = 1; k <= q; k++) u.theta[k][1][1] = v->theta[k][i][i];
        for (t = 1; t <= n; t++) u.w[t][1] = v->w[t][i];

        elf(u.m, u.n, u.p, u.q, u.mu, u.phi, u.theta, u.qq, u.w, 1.0,
            u.xitol, FALSE, u.a, &pi1, &pi2, &pi3, &ifault);

        if (ifault == 0) {
            ll = -0.5 * u.n * (LOG2PI - log((real) u.n) + 1.0)
                 - 0.5 * u.n * (log(pi1) + log(pi2));
            sum += ll;
            fprintf(outputv, "  univariate %d (p=%d, q=%d)   logL = %18.10f\n",
                    i, p, q, ll);
        } else {
            failed = 1;
            fprintf(outputv, "  univariate %d              elf ifault = %d\n",
                    i, ifault);
        }

        free_matrix(u.a, 1, n, 1, 1);
        free_matrix(u.w, 1, n, 1, 1);
        free_matrix(u.qq, 1, 1, 1, 1);
        free_tensor(u.theta, 0, (q > 0 ? q : 1), 1, 1, 1, 1);
        free_tensor(u.phi,   0, (p > 0 ? p : 1), 1, 1, 1, 1);
        free_vector(u.mu, 1, 1);
    }

    if (failed) {
        fprintf(outputv, "\n  Contract NOT verified: a univariate evaluation "
                         "failed.\n");
        return;
    }

    /*  LA TOLERANCIA ES LA TRUNCACION, y esta medida, no elegida.
     *
     *  La identidad es exacta en algebra.  Lo que la separa en la maquina es
     *  que elf trunca la sucesion xi cuando la suma de valores absolutos de su
     *  termino baja de xitol (elfvarma.c, cxi [1]), y el sistema conjunto y las
     *  univariantes NO truncan en el mismo termino: el conjunto suma m entradas
     *  y cada univariante una sola.  Medido, bajando xitol y volviendo a
     *  construir, sobre el hueco mayor del banco (Milan, p = 2, q = 1):
     *
     *      xitol     joint - sum
     *      1e-3       1.385e-04
     *      1e-8       3.100e-09
     *
     *  El hueco ES xitol, con un factor de ~0.15, y con q = 0 -- donde no hay
     *  sucesion que truncar -- es CERO EXACTO en las doce series del banco.
     *
     *  El umbral fijo de 1e-4 que habia aqui declaraba entonces NO VERIFICADA
     *  la puerta de Milan, cuyo desacuerdo RELATIVO (2.1e-6) es menor que el de
     *  Angers (6.3e-6), que pasaba: ordenaba los casos por el tamano de su logL
     *  y no por su acuerdo, que es lo contrario de lo que dice comprobar.  Una
     *  alarma que suena por el tamano del dato no es una alarma.
     *
     *  Ligado a xitol, en cambio, el contrato afirma lo unico que se puede
     *  afirmar: que las dos rutas coinciden HASTA DONDE LA APROXIMACION LLEGA.
     *  Los fallos que esta puerta tiene que atrapar -- el signo de Lambda en la
     *  transformacion, B2 leido traspuesto -- abren huecos de UNIDADES, tres
     *  ordenes por encima de este umbral, asi que no se afloja nada real.     */
    {
        real gap = v->logelf - sum;
        real tol = (q > 0) ? fabs(v->xitol) : 1.0e-6;

        fprintf(outputv, "  %-28s   sum  = %18.10f\n", "", sum);
        fprintf(outputv, "  %-28s   joint= %18.10f\n", "", v->logelf);
        fprintf(outputv, "\n  crossing identity (joint - sum) = %.3e   "
                         "(tolerance %.1e)   %s\n", gap, tol,
                (fabs(gap) < tol) ? "VERIFIED" : "*** NOT VERIFIED ***");
        if (fabs(gap) >= tol)
            fprintf(outputv,
                "\n  The two sides must agree at this rung to within the xi\n"
                "  truncation, which is what the tolerance is: with q > 0 it is\n"
                "  xitol itself, and with q = 0, where there is no series to\n"
                "  truncate, the identity holds exactly.  A gap LARGER than that\n"
                "  is not truncation, and the fault is then upstream of the\n"
                "  likelihood -- the transformation, the rank's differencing, the\n"
                "  parameter walk, the deterministic terms or the scaling -- and\n"
                "  never in elf() itself.\n");
    }

    /*  THE OPTIMALITY CERTIFICATE.  Second of the two contracts, and it costs
     *  one likelihood evaluation that has already been made.  The gap between
     *  the fit and the values that were brought in is non-negative by
     *  construction and is zero if and only if those values were the
     *  univariate optima.  So it answers a question the files themselves
     *  cannot: was this a `.pre` -- an optimum in re-runnable form -- or a
     *  specification that still needed estimating?  Neither is a defect; both
     *  are legitimate inputs.  What was missing was being told which.
     *
     *  The threshold is the suite's, and it is MEASURED rather than chosen: a
     *  genuine `.pre` does not return exactly to its own values, because the
     *  format stores six decimals and the optimiser stops inside its own
     *  tolerance, which leaves a residue of order 1e-5; a specification moves
     *  by orders of magnitude more.  1e-3 sits in between with room to spare.  */
    if (gate_have_start) {
        real gap = v->logelf - gate_ll_start;

        fprintf(outputv, "\n  --- optimality certificate ---\n");
        fprintf(outputv, "  logL AT the values brought in   = %18.10f\n",
                gate_ll_start);
        fprintf(outputv, "  logL of the diagonal fit        = %18.10f\n",
                v->logelf);
        fprintf(outputv, "  optimality gap (fit - brought)  = %+.3e\n", gap);
        if (gate_have_move)
            fprintf(outputv, "  largest coefficient movement    = %.3e\n",
                    gate_move);
        fprintf(outputv,
            "\n  The gap is >= 0 always, and zero exactly when what came in were\n"
            "  the univariate optima.  Here they %s: this input is %s.\n",
            (gap <= 1.0e-3) ? "were" : "were not",
            (gap <= 1.0e-3) ? "AN OPTIMUM" : "A SPECIFICATION");
        if (gap < -1.0e-6)
            fprintf(outputv,
                "\n  A NEGATIVE gap cannot happen at a converged fit: the fitted\n"
                "  point is worse than the one it started from, so the optimiser\n"
                "  did not converge here and the fit should not be read.\n");
    }
}

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

    /*  -warma: la semilla es su propia regresion, porque el sistema que se
     *  parametriza es OTRO -- las ecuaciones son las de Ybar = [nabla Y2 ; W] y
     *  no las de nabla Y.  Se regresa cada componente de Ybar_t sobre
     *  W_{t-1}..W_{t-p} y una constante, que es exactamente la forma de la
     *  definicion 3, y Theta arranca en cero.                                */
    if (global_warma) {
        real **B2w = matrix(1, s, 1, (r > 0 ? r : 1));
        real **Yb, **X, **Yd, **XtX;
        real *Xty, *EWv;
        int *ind, nreg2, T2, t2, e2, c2, c3, nf2 = (p > 1) ? p - 1 : 0;

        prelim_b2(B2w);
        if (global_seedjoh && r > 0) canonical_b2(B2w);

        Yb = matrix(1, nobs, 1, M);
        for (t2 = 1; t2 <= nobs; t2++) {
            for (i = 1; i <= s; i++) Yb[t2][i] = datamat[t2][i];
            for (j = 1; j <= r; j++) {
                real wv = datamat[t2][s + j];
                for (i = 1; i <= s; i++) wv += B2w[i][j] * Y2_levels[t2][i];
                Yb[t2][s + j] = wv;
            }
        }
        EWv = vector(1, (r > 0 ? r : 1));
        for (j = 1; j <= r; j++) {
            real sm = 0.0;
            for (t2 = 1; t2 <= nobs; t2++) sm += Yb[t2][s + j];
            EWv[j] = sm / nobs;
        }
        nreg2 = p * r;
        T2 = nobs - p; if (T2 < 1) T2 = 1;
        X   = matrix(1, T2, 1, (nreg2 > 0 ? nreg2 : 1));
        Yd  = matrix(1, T2, 1, M);
        for (t2 = p + 1; t2 <= nobs; t2++) {
            int row = t2 - p; c2 = 1;
            for (k = 1; k <= p; k++)
                for (j = 1; j <= r; j++) X[row][c2++] = Yb[t2-k][s+j] - EWv[j];
            for (i = 1; i <= M; i++) Yd[row][i] = Yb[t2][i];
        }
        XtX = matrix(1, (nreg2 > 0 ? nreg2 : 1), 1, (nreg2 > 0 ? nreg2 : 1));
        Xty = vector(1, (nreg2 > 0 ? nreg2 : 1));
        ind = ivector(1, (nreg2 > 0 ? nreg2 : 1));
        {
            real ***Cw = tensor(1, p, 1, M, 1, (r > 0 ? r : 1));
            real **E2  = matrix(1, T2, 1, M);
            real **Sg2 = matrix(1, M, 1, M);
            for (c2 = 1; c2 <= nreg2; c2++)
                for (c3 = 1; c3 <= nreg2; c3++) {
                    real ss = 0.0;
                    for (t2 = 1; t2 <= T2; t2++) ss += X[t2][c2] * X[t2][c3];
                    XtX[c2][c3] = ss;
                }
            if (nreg2 > 0) ludcp(XtX, nreg2, ind);
            for (e2 = 1; e2 <= M; e2++) {
                real **XX = matrix(1, nreg2, 1, nreg2);
                for (c2 = 1; c2 <= nreg2; c2++)
                    for (c3 = 1; c3 <= nreg2; c3++) XX[c2][c3] = XtX[c2][c3];
                for (c2 = 1; c2 <= nreg2; c2++) {
                    Xty[c2] = 0.0;
                    for (t2 = 1; t2 <= T2; t2++) Xty[c2] += X[t2][c2] * Yd[t2][e2];
                }
                lusol(XX, Xty, nreg2, ind);
                for (k = 1; k <= p; k++)
                    for (j = 1; j <= r; j++) Cw[k][e2][j] = Xty[(k-1)*r + j];
                for (t2 = 1; t2 <= T2; t2++) {
                    real ei = Yd[t2][e2];
                    for (c2 = 1; c2 <= nreg2; c2++) ei -= Xty[c2] * X[t2][c2];
                    E2[t2][e2] = ei;
                }
                free_matrix(XX, 1, nreg2, 1, nreg2);
            }
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) {
                    real ss = 0.0;
                    for (t2 = 1; t2 <= T2; t2++) ss += E2[t2][i] * E2[t2][j];
                    Sg2[i][j] = ss / T2;
                }
            /* --- escritura, en el orden que espera el cast --- */
            if (global_case == 2) { for (j = 1; j <= r; j++) x[idx++] = EWv[j]; }
            else if (global_case == 3) {
                for (i = 1; i <= s; i++) {
                    real sm = 0.0;
                    for (t2 = 1; t2 <= nobs; t2++) sm += Yb[t2][i];
                    x[idx++] = sm / nobs;
                }
                for (j = 1; j <= r; j++) x[idx++] = EWv[j];
            }
            for (i = 1; i <= M; i++)
                for (j = 1; j <= r; j++) x[idx++] = Cw[1][i][j];
            for (k = 1; k <= nf2; k++)
                for (i = 1; i <= M; i++)
                    for (j = 1; j <= r; j++) x[idx++] = Cw[k+1][i][j];
            for (k = 1; k <= q; k++)
                for (i = 1; i <= r; i++)
                    for (j = 1; j <= r; j++) x[idx++] = 0.0;
            {
                real s11 = (Sg2[1][1] > 1.0e-12) ? Sg2[1][1] : 1.0;
                if (global_diag_cov) {
                    for (i = 2; i <= M; i++) x[idx++] = Sg2[i][i] / s11;
                } else {
                    for (i = 2; i <= M; i++) x[idx++] = Sg2[i][i] / s11;
                    for (i = 2; i <= M; i++)
                        for (j = 1; j < i; j++) x[idx++] = Sg2[i][j] / s11;
                }
            }
            if (!global_fixb2)
                for (j = 1; j <= r; j++)
                    for (i = 1; i <= s; i++) x[idx++] = B2w[i][j];
            else {
                if (!B2_fixed || b2f_s != s || b2f_r != r) {
                    if (B2_fixed) free_matrix(B2_fixed, 1, b2f_s, 1, b2f_r);
                    B2_fixed = matrix(1, s, 1, (r > 0 ? r : 1));
                    b2f_s = s; b2f_r = (r > 0 ? r : 1);
                }
                for (j = 1; j <= r; j++)
                    for (i = 1; i <= s; i++)
                        B2_fixed[i][j] = global_fixb2_given ? global_fixb2_value
                                                            : B2w[i][j];
            }
            free_matrix(Sg2, 1, M, 1, M);
            free_matrix(E2, 1, T2, 1, M);
            free_tensor(Cw, 1, p, 1, M, 1, (r > 0 ? r : 1));
        }
        if (idx - 1 != npar)
            fprintf(stderr, "ERROR init_guess (-warma): idx=%d, npar=%d\n",
                    idx-1, npar);
        free_ivector(ind, 1, (nreg2 > 0 ? nreg2 : 1));
        free_vector(Xty, 1, (nreg2 > 0 ? nreg2 : 1));
        free_matrix(XtX, 1, (nreg2 > 0 ? nreg2 : 1), 1, (nreg2 > 0 ? nreg2 : 1));
        free_matrix(Yd, 1, T2, 1, M);
        free_matrix(X, 1, T2, 1, (nreg2 > 0 ? nreg2 : 1));
        free_vector(EWv, 1, (r > 0 ? r : 1));
        free_matrix(Yb, 1, nobs, 1, M);
        free_matrix(B2w, 1, s, 1, (r > 0 ? r : 1));
        return;
    }

    /* --- 1. Initial B₂ via static OLS with intercept ------------------- */
    real **B2 = matrix(1, s, 1, (r > 0 ? r : 1));
    prelim_b2(B2);

    /*  -seedjoh: la solucion canonica en su lugar.  Se pide DESPUES del OLS
     *  estatico y no en vez de el, para que si el problema de autovalores no
     *  se puede resolver quede exactamente la ruta de siempre y no una tercera
     *  cosa a medio camino.  Todo lo que sigue -- Lambda, F, Sigma y E[W] --
     *  sale entonces de la regresion condicional CON ESTE W, que es la propia
     *  formula de alpha de Johansen.                                          */
    canon_used = 0;
    if (global_seedjoh && r > 0) {
        canon_used = canonical_b2(B2);
        if (canon_used) {
            fprintf(outputv, "\n-seedjoh: B2 seeded from the canonical "
                             "reduced-rank solution:\n");
            for (i = 1; i <= s; i++) {
                fprintf(outputv, "   ");
                for (j = 1; j <= r; j++) fprintf(outputv, " %12.6f", B2[i][j]);
                fprintf(outputv, "\n");
            }
            if (!quiet_mode) {
                printf("  -seedjoh: B2 canonico =");
                for (i = 1; i <= s; i++)
                    for (j = 1; j <= r; j++) printf(" %.6f", B2[i][j]);
                printf("\n");
            }
        } else {
            fprintf(outputv, "\n-seedjoh: the canonical solution could not be "
                             "formed; the static OLS seed stands.\n");
            if (!quiet_mode)
                printf("  -seedjoh: no se pudo formar la solucion canonica\n");
        }
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
    if (global_marow && q > 0) {
        for (k = 1; k <= q; k++)
            for (i = 1; i <= r; i++)
                for (j = 1; j <= M; j++) x[idx++] = 0.0;
    } else if (global_matri && q > 0) {
        for (k = 1; k <= q; k++) {
            for (i = 1; i <= r; i++)
                for (j = 1; j <= M; j++) x[idx++] = 0.0;
            for (i = r + 1; i <= M; i++)
                for (j = r + 1; j <= M; j++) x[idx++] = 0.0;
        }
    } else if (global_mawarma && q > 0) {
        /*  -mawarma: el bloque libre es solo Theta11 (r x r), y el resto lo
         *  construye el cast.  La semilla es la diagonal de lo que hubiera:
         *  con la ruta de residuos, la theta univariante del bloque
         *  cointegrado; si no, cero, que es el arranque de siempre.          */
        for (k = 1; k <= q; k++)
            for (i = 1; i <= r; i++)
                for (j = 1; j <= r; j++)
                    x[idx++] = (i == j && seed_loaded && seed_route == SEED_RESID
                                && seed_tbar) ? seed_tbar[k][i] : 0.0;
    } else if (seed_loaded && q > 0 && seed_route == SEED_RESID) {
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

    /*  -warma: la parametrizacion en coordenadas Ybar.  Se escribe Phi*, Theta*
     *  y Sigma* DIRECTAMENTE y no se construye ninguna C̄: no hay nada que
     *  transformar porque los parametros ya son los del sistema transformado.
     *  Ver -warma.  El vector w se monta igual que siempre, al final, con B2,
     *  que es por donde -- y solo por donde -- entra el vector de
     *  cointegracion.                                                        */
    if (global_warma) {
        int nf_w = (p > 1) ? p - 1 : 0;
        int idw = 1, kk, ii, jj, tt2;
        real **B2w = matrix(1, s, 1, (r > 0 ? r : 1));

        for (i = 1; i <= M; i++) armax->mu[i] = 0.0;
        if (global_case == 2) {
            for (j = 1; j <= r; j++) armax->mu[s + j] = x[idw++];
        } else if (global_case == 3) {
            for (i = 1; i <= s; i++) armax->mu[i] = x[idw++];
            for (j = 1; j <= r; j++) armax->mu[s + j] = x[idw++];
        }
        for (kk = 1; kk <= p; kk++)
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) armax->phi[kk][i][j] = 0.0;
        for (kk = 1; kk <= q; kk++)
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) armax->theta[kk][i][j] = 0.0;
        /*  Los coeficientes de W_{t-k}: un bloque M x r por retardo, k = 1..p-1
         *  mas el primero, que ocupa la casilla de Lambda.                    */
        for (i = 1; i <= M; i++)
            for (j = 1; j <= r; j++) armax->phi[1][i][s + j] = x[idw++];
        for (kk = 1; kk <= nf_w; kk++)
            for (i = 1; i <= M; i++)
                for (j = 1; j <= r; j++) armax->phi[kk + 1][i][s + j] = x[idw++];
        for (kk = 1; kk <= q; kk++)
            for (i = 1; i <= r; i++)
                for (j = 1; j <= r; j++) armax->theta[kk][s + i][s + j] = x[idw++];
        {
            real **Sg = matrix(1, M, 1, M);
            real **Sc = matrix(1, M, 1, M);
            real d1, d2; int ifc = 0;
            for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) Sg[i][j] = 0.0;
            Sg[1][1] = 1.0;
            if (global_diag_cov) {
                for (i = 2; i <= M; i++) Sg[i][i] = x[idw++];
            } else {
                for (i = 2; i <= M; i++) Sg[i][i] = x[idw++];
                for (i = 2; i <= M; i++)
                    for (j = 1; j < i; j++) { Sg[i][j] = x[idw++]; Sg[j][i] = Sg[i][j]; }
            }
            for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) Sc[i][j] = Sg[i][j];
            choldcp(Sc, M, &d1, &d2, &ifc);
            if (ifc > 0) *ifaultx = 1;
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) armax->qq[i][j] = Sg[i][j];
            free_matrix(Sc, 1, M, 1, M);
            free_matrix(Sg, 1, M, 1, M);
        }
        for (j = 1; j <= r; j++)
            for (i = 1; i <= s; i++)
                B2w[i][j] = global_fixb2 ? B2_fixed[i][j] : x[idw++];

        for (tt2 = 1; tt2 <= nobs; tt2++) {
            for (i = 1; i <= s; i++) armax->w[tt2][i] = datamat[tt2][i];
            for (j = 1; j <= r; j++) {
                real wv = datamat[tt2][s + j];
                for (i = 1; i <= s; i++) wv += B2w[i][j] * Y2_levels[tt2][i];
                armax->w[tt2][s + j] = wv;
            }
        }
        granger_sv = -1.0;
        free_matrix(B2w, 1, s, 1, (r > 0 ? r : 1));
        if (lastx == 1) {
            free_matrix(armax->a, 1, armax->n, 1, armax->m);
            free_matrix(armax->w, 1, armax->n, 1, armax->m);
            free_matrix(armax->qq, 1, armax->m, 1, armax->m);
            free_tensor(armax->theta, 0, armax->q, 1, armax->m, 1, armax->m);
            free_tensor(armax->phi, 0, armax->p, 1, armax->m, 1, armax->m);
            free_vector(armax->mu, 1, armax->m);
        }
        (void) ii; (void) jj;
        return;
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
        if (prof_hold) {          /* sujeto en el optimo de r = 0; ver -seedgate */
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) F[k][i][j] = hold_F[k][i][j];
        } else if (global_diag_ar) {
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
        if (prof_hold) {
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) Theta[k][i][j] = hold_Th[k][i][j];
        } else if (global_mawarma) {
            /*  Theta = [Theta11  Theta11 B2' ; 0  0].  Ojo al orden: B2 se lee
             *  mas abajo, asi que aqui se guarda solo el bloque libre y el
             *  resto se completa DESPUES de tener B2.  Ver -mawarma.         */
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) Theta[k][i][j] = 0.0;
            for (i = 1; i <= r; i++)
                for (j = 1; j <= r; j++) Theta[k][i][j] = x[idx++];
        } else if (global_marow) {
            /*  Theta = [T11  T12 ; 0  0]: las s filas de abajo, cero.  Ver
             *  -marow.                                                       */
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) Theta[k][i][j] = 0.0;
            for (i = 1; i <= r; i++)
                for (j = 1; j <= M; j++) Theta[k][i][j] = x[idx++];
        } else if (global_matri) {
            /*  Theta = [T11  T12 ; 0  T22]: solo el bloque de abajo a la
             *  izquierda se anula.  Ver -matri.                              */
            for (i = 1; i <= M; i++)
                for (j = 1; j <= M; j++) Theta[k][i][j] = 0.0;
            for (i = 1; i <= r; i++)
                for (j = 1; j <= M; j++) Theta[k][i][j] = x[idx++];
            for (i = r + 1; i <= M; i++)
                for (j = r + 1; j <= M; j++) Theta[k][i][j] = x[idx++];
        } else if (global_diag_ma) {
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
    if (prof_hold) {
        for (i = 1; i <= M; i++)
            for (j = 1; j <= M; j++) Sigma[i][j] = hold_S[i][j];
    } else if (global_diag_cov) {
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

    /*  -mawarma: el bloque superior derecho de Theta, que NO es libre.  Se
     *  completa aqui y no arriba porque necesita B2, que se acaba de leer:
     *  Theta[k][i][r+jj] = sum_ii Theta11[k][i][ii] * B2'[ii][jj].           */
    if (global_mawarma && !prof_hold) {
        for (k = 1; k <= q; k++)
            for (i = 1; i <= r; i++)
                for (int jj = 1; jj <= s; jj++) {
                    real acc = 0.0;
                    for (int ii = 1; ii <= r; ii++)
                        acc += Theta[k][i][ii] * B2[jj][ii];
                    Theta[k][i][r + jj] = acc;
                }
    }

    /*  LA CONDICION DE RANGO, medida en cada evaluacion y opcionalmente
     *  impuesta.  Va aqui porque es el primer punto donde Lambda, Theta y B2
     *  existen a la vez, y antes de construir nada con ellos.                */
    granger_sv = (r > 0 && q > 0) ? granger_smin(Lambda, B2, Theta, M, r, q) : -1.0;
    if (global_rankadm && granger_sv >= 0.0 && granger_sv < global_rankadm_tol)
        *ifaultx = 1;          /* el punto niega el rango: fuera, como Sigma no PD */

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
/*  gate_profile_seed — LA RUTA (B) DEL PLAN: el optimo de r = 0, y sobre el,  */
/*  Lambda y B2 por verosimilitud.                                            */
/*                                                                           */
/*  POR QUE EXISTE.  Toda la construccion de la suite va de OPTIMOS HACIA     */
/*  OPTIMOS: se estima un peldano, se certifica, y el de arriba arranca ahi.  */
/*  Ese puente NO alcanza el peldano de r = 1, y la razon esta medida en      */
/*  docs/VEC_EMBEDDING_PLAN.md 3: con Lambda = 0 el sistema transformado tiene */
/*  una raiz AR de modulo exactamente 1.000000, o sea que la base esta EN la   */
/*  frontera del espacio de arriba y no en su interior, la verosimilitud no    */
/*  esta definida ahi, y ademas B2 no esta identificado, porque Pi = Lambda B' */
/*  = 0 sea cual sea B2.  Sembrar el peldano de abajo tal cual es INADMISIBLE, */
/*  no solo inexacto.                                                         */
/*                                                                           */
/*  COMO CRUZA.  Sujetando F, Theta y Sigma en el optimo de r = 0 y estimando  */
/*  SOLO Lambda y B2 (y la media, que en r = 1 tiene un E[W] que abajo no      */
/*  existe).  El problema condicional es pequeno y mucho mejor condicionado    */
/*  que el conjunto, y su solucion es admisible POR CONSTRUCCION: el paso      */
/*  fuera de la frontera lo elige la verosimilitud y no quien programa.  Esa   */
/*  es la razon de preferir (B) a (A) -- entrar por la direccion que ajusta    */
/*  con un paso calibrado --: (A) necesita una constante, y una constante fija */
/*  es una distancia distinta en cada conjunto de datos.                       */
/*                                                                           */
/*  QUE SUJETA, EXACTAMENTE.  El peldano de abajo se estima con la MISMA       */
/*  estructura que se pidio arriba: sin banderas diagonales es el peldano 2 de */
/*  la escalera (F, Theta y Sigma libres), y con ellas es la puerta diagonal   */
/*  certificada.  El plan dice "los valores de la puerta"; se toma el optimo   */
/*  de r = 0 de la estructura pedida porque es el peldano inmediatamente       */
/*  inferior y es el que la escalera manda usar, y con las banderas puestas    */
/*  los dos coinciden.                                                        */
/*                                                                           */
/*  Devuelve 1 si dejo un punto de partida nuevo en x, y 0 si no lo consiguio, */
/*  en cuyo caso x sigue siendo el de init_guess -- la ruta (C) -- y se dice.  */
/*****************************************************************************/
static int gate_profile_seed(real *x, int npar)
{
    int M = nser, r0 = global_r, p = global_p, q = global_q;
    int nf = (p > 1) ? p - 1 : 0;
    int nmean, nlam, nhead, nmid, ntail, np0, np2, i, j, k, idx, ifr = 0, ifc;
    real *x0, *dev0, **cov0, *x2, *dev2, **cov2;
    struct Tvarma v0, v2;

    if (r0 <= 0) return 0;                  /* sin matriz VEC no hay que cruzar */
    par_blocks(&nmean, &nlam, &nmid, &ntail);
    nhead = nmean + nlam;
    if (nhead + nmid + ntail != npar) return 0;      /* el vector no es el que  */
                                                     /* este recorrido espera   */

    /* ---- 1. el peldano de abajo, estimado hasta su optimo ---------------- */
    global_r = 0;
    build_y2_levels();
    np0  = calc_nparametrs();
    x0   = vector(1, np0);
    dev0 = vector(1, np0);
    cov0 = matrix(1, np0, 1, np0);
    v0.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;
    init_guess(x0, np0);
    vec_shootx(x0, &v0, &ifr, 1, 0);
    est(&vec_shootx, np0, x0, dev0, cov0, 500, 200, 1e-5, 1e-7,
        v0.xitol, v0.a, &v0.sigma2, &v0.logelf, &ifr);

    if (ifr != 0) {
        fprintf(outputv, "\n-seedgate: the r = 0 rung did not converge "
                         "(ifault = %d); falling back to the cold start.\n", ifr);
        if (!quiet_mode)
            printf("  -seedgate: el peldano r = 0 no convergio; se sigue en frio\n");
        vec_shootx(x0, &v0, &ifr, 0, 1);
        free_matrix(cov0, 1, np0, 1, np0);
        free_vector(dev0, 1, np0);
        free_vector(x0, 1, np0);
        global_r = r0; build_y2_levels();
        return 0;
    }
    gate_seed_ll0 = v0.logelf;

    /*  Recuperar el ajuste en las estructuras: est() deja la ultima evaluacion,
     *  que no tiene por que ser el punto final.                              */
    vec_shootx(x0, &v0, &ifr, 0, 0);

    /*  CON r = 0 LAS COORDENADAS SON LAS MISMAS, y por eso esto se puede leer
     *  directamente del ajuste en vez de volver a desmontar x0: Cbar y Cinv
     *  colapsan a la identidad y Hbar a cero (vec_shootx [4]), de modo que
     *  Phi*_k = F_k, Theta*_k = Theta_k y Sigma* = Sigma, termino a termino.
     *  Es la misma propiedad de la que vive la puerta.                       */
    hold_M = M; hold_nf = nf; hold_q = q;
    hold_F  = tensor(1, (nf > 0 ? nf : 1), 1, M, 1, M);
    hold_Th = tensor(1, (q  > 0 ? q  : 1), 1, M, 1, M);
    hold_S  = matrix(1, M, 1, M);
    for (k = 1; k <= (nf > 0 ? nf : 1); k++)
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++)
            hold_F[k][i][j] = (k <= nf) ? v0.phi[k][i][j] : 0.0;
    for (k = 1; k <= (q > 0 ? q : 1); k++)
        for (i = 1; i <= M; i++) for (j = 1; j <= M; j++)
            hold_Th[k][i][j] = (k <= q) ? v0.theta[k][i][j] : 0.0;
    for (i = 1; i <= M; i++) for (j = 1; j <= M; j++) hold_S[i][j] = v0.qq[i][j];

    vec_shootx(x0, &v0, &ifr, 0, 1);
    free_matrix(cov0, 1, np0, 1, np0);
    free_vector(dev0, 1, np0);
    free_vector(x0, 1, np0);

    /* ---- 2. el paso condicional: solo la media, Lambda y B2 -------------- */
    global_r = r0;
    build_y2_levels();
    prof_hold = 1;
    np2  = calc_nparametrs();               /* = nhead + ntail, por par_blocks */
    x2   = vector(1, np2);
    dev2 = vector(1, np2);
    cov2 = matrix(1, np2, 1, np2);
    for (i = 1; i <= nhead; i++) x2[i] = x[i];
    for (i = 1; i <= ntail; i++) x2[nhead + i] = x[nhead + nmid + i];
    v2.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;
    vec_shootx(x2, &v2, &ifr, 1, 0);

    /*  UN PUNTO DE PARTIDA ADMISIBLE, Y LO ELIGE LA VEROSIMILITUD.
     *
     *  Esto lo obligo la medida y no estaba en el plan: con F, Theta y Sigma
     *  sujetos en el optimo de r = 0 y Lambda en el valor de la regresion
     *  condicional, el sistema transformado sale NO ESTACIONARIO -- elf
     *  devuelve ifault = 3 -- y el optimizador no arranca siquiera, porque est
     *  se planta si el punto inicial no es admisible (drvmlest.c, "bad initial
     *  estimates").  Es la otra cara de lo que el plan midio en 3: en Lambda =
     *  0 la raiz esta EXACTAMENTE en 1, y de las dos direcciones que salen de
     *  ahi solo una es admisible.  El signo que trae la regresion condicional
     *  no tiene por que ser ese.
     *
     *  Asi que se recorre una escalera de multiplos de la Lambda que trajo la
     *  regresion -- la DIRECCION la eligen los datos, no el programa -- y se
     *  arranca en el mejor punto ADMISIBLE de los que se evaluan.  Que no es
     *  una constante fija, que es lo que el plan prohibe: es un multiplo de
     *  algo estimado, y ademas el paso condicional lo mueve despues.  Si
     *  ninguno es admisible, no se cruza y se dice.                          */
    {
        const real LOG2PI = 1.837877066;
        static const real mult[18] = { 1.0, -1.0, 0.5, -0.5, 0.25, -0.25,
                                       0.1, -0.1, 0.05, -0.05, 0.02, -0.02,
                                       0.01, -0.01, 2.0, -2.0, 4.0, -4.0 };
        real *lam0 = vector(1, (nlam > 0 ? nlam : 1));
        real best = 0.0, bestc = 0.0;
        int  have = 0, mi;

        for (i = 1; i <= nlam; i++) lam0[i] = x2[nmean + i];
        for (mi = 0; mi < 18; mi++) {
            real pi1, pi2, pi3, ll;
            int ifev = 0, ifc2 = 0;
            for (i = 1; i <= nlam; i++) x2[nmean + i] = mult[mi] * lam0[i];
            vec_shootx(x2, &v2, &ifc2, 0, 0);
            if (ifc2 != 0) continue;                  /* Sigma no definida pos. */
            elf(v2.m, v2.n, v2.p, v2.q, v2.mu, v2.phi, v2.theta, v2.qq, v2.w,
                1.0, v2.xitol, TRUE, v2.a, &pi1, &pi2, &pi3, &ifev);
            if (ifev != 0) continue;                  /* no admisible: 1..5     */
            ll = -0.5 * v2.m * v2.n * (LOG2PI - log((real) v2.m)
                 - log((real) v2.n) + 1.0)
                 - 0.5 * v2.n * (v2.m * log(pi1) + log(pi2));
            if (!have || ll > best) { have = 1; best = ll; bestc = mult[mi]; }
        }
        for (i = 1; i <= nlam; i++) x2[nmean + i] = bestc * lam0[i];
        free_vector(lam0, 1, (nlam > 0 ? nlam : 1));
        gate_seed_lam = bestc;
        gate_seed_ll_start = have ? best : 0.0;
        if (!have) {
            fprintf(outputv, "\n-seedgate: no admissible starting point for the "
                             "conditional step -- every multiple of the "
                             "conditional-regression Lambda leaves the transformed "
                             "system non-stationary or non-invertible.  Falling "
                             "back to the cold start.\n");
            if (!quiet_mode)
                printf("  -seedgate: ningun arranque admisible; se sigue en frio\n");
            vec_shootx(x2, &v2, &ifr, 0, 1);
            prof_hold = 0;
            free_matrix(cov2, 1, np2, 1, np2);
            free_vector(dev2, 1, np2);
            free_vector(x2, 1, np2);
            free_matrix(hold_S, 1, M, 1, M);
            free_tensor(hold_Th, 1, (q  > 0 ? q  : 1), 1, M, 1, M);
            free_tensor(hold_F,  1, (nf > 0 ? nf : 1), 1, M, 1, M);
            hold_F = NULL; hold_Th = NULL; hold_S = NULL;
            return 0;
        }
    }

    est(&vec_shootx, np2, x2, dev2, cov2, 500, 200, 1e-5, 1e-7,
        v2.xitol, v2.a, &v2.sigma2, &v2.logelf, &ifr);
    ifc = ifr;                     /* est deja aqui su codigo, y la liberacion */
    gate_seed_ll1 = v2.logelf;     /* de abajo lo pisaria                      */
    vec_shootx(x2, &v2, &ifr, 0, 1);
    prof_hold = 0;

    if (ifc != 0) {
        fprintf(outputv, "\n-seedgate: the conditional step for Lambda and B2 "
                         "did not converge (ifault = %d); falling back to the "
                         "cold start.\n", ifc);
        if (!quiet_mode)
            printf("  -seedgate: el paso condicional no convergio; se sigue en frio\n");
        free_matrix(cov2, 1, np2, 1, np2);
        free_vector(dev2, 1, np2);
        free_vector(x2, 1, np2);
        free_matrix(hold_S, 1, M, 1, M);
        free_tensor(hold_Th, 1, (q  > 0 ? q  : 1), 1, M, 1, M);
        free_tensor(hold_F,  1, (nf > 0 ? nf : 1), 1, M, 1, M);
        hold_F = NULL; hold_Th = NULL; hold_S = NULL;
        return 0;
    }

    /* ---- 3. el vector completo: cabeza y cola del paso condicional, tramo
              de en medio del optimo de r = 0.  El orden de escritura es el del
              recorrido de vec_shootx, y tiene que serlo: es el quinto sitio que
              recorre este vector.                                            */
    for (i = 1; i <= nhead; i++) x[i] = x2[i];
    for (i = 1; i <= ntail; i++) x[nhead + nmid + i] = x2[nhead + i];
    idx = nhead + 1;
    for (k = 1; k <= nf; k++) {
        if (global_diag_ar) { for (i = 1; i <= M; i++) x[idx++] = hold_F[k][i][i]; }
        else for (i = 1; i <= M; i++)
                 for (j = 1; j <= M; j++) x[idx++] = hold_F[k][i][j];
    }
    for (k = 1; k <= q; k++) {
        if (global_diag_ma) { for (i = 1; i <= M; i++) x[idx++] = hold_Th[k][i][i]; }
        else for (i = 1; i <= M; i++)
                 for (j = 1; j <= M; j++) x[idx++] = hold_Th[k][i][j];
    }
    if (global_diag_cov) {
        for (i = 2; i <= M; i++) x[idx++] = hold_S[i][i];
    } else {
        for (i = 2; i <= M; i++) x[idx++] = hold_S[i][i];
        for (i = 2; i <= M; i++)
            for (j = 1; j < i; j++) x[idx++] = hold_S[i][j];
    }
    if (idx - 1 != nhead + nmid) {          /* el recorrido no cuadra: no se usa */
        fprintf(outputv, "\n-seedgate: internal walk mismatch (%d vs %d); "
                         "falling back to the cold start.\n",
                idx - 1, nhead + nmid);
        free_matrix(cov2, 1, np2, 1, np2);
        free_vector(dev2, 1, np2);
        free_vector(x2, 1, np2);
        free_matrix(hold_S, 1, M, 1, M);
        free_tensor(hold_Th, 1, (q  > 0 ? q  : 1), 1, M, 1, M);
        free_tensor(hold_F,  1, (nf > 0 ? nf : 1), 1, M, 1, M);
        hold_F = NULL; hold_Th = NULL; hold_S = NULL;
        return 0;
    }

    free_matrix(cov2, 1, np2, 1, np2);
    free_vector(dev2, 1, np2);
    free_vector(x2, 1, np2);
    free_matrix(hold_S, 1, M, 1, M);
    free_tensor(hold_Th, 1, (q  > 0 ? q  : 1), 1, M, 1, M);
    free_tensor(hold_F,  1, (nf > 0 ? nf : 1), 1, M, 1, M);
    hold_F = NULL; hold_Th = NULL; hold_S = NULL;

    gate_seed_ok = 1;
    fprintf(outputv,
        "\n=== -seedgate: the VEC block profiled on the rung below ===\n\n"
        "  r = 0 optimum (F, Theta, Sigma)        logL = %18.10f\n"
        "  admissible entry, Lambda x %-6.2f        logL = %18.10f\n"
        "  + Lambda and B2 profiled on it         logL = %18.10f\n\n"
        "  The starting point for the free fit below is that second value, not\n"
        "  a conditional regression.  Holding the rung below and estimating only\n"
        "  Lambda and B2 lands in the INTERIOR by construction: at Lambda = 0 the\n"
        "  transformed system has an AR root of modulus exactly one, so the rung\n"
        "  below cannot be carried up unchanged, and the step off that boundary\n"
        "  is chosen here by the likelihood rather than by a constant.\n",
        gate_seed_ll0, gate_seed_lam, gate_seed_ll_start, gate_seed_ll1);
    /*  LAS PREESTIMACIONES, ESCRITAS.  Son el producto del paso condicional y
     *  hay que poder verlas: si (B) acaba peor que (C), la pregunta siguiente
     *  es siempre si el punto de partida es razonable o disparatado, y esa no
     *  se contesta con el logL.  El orden es el del recorrido de vec_shootx:
     *  Lambda por filas (i exterior, j interior) y B2 por columnas.          */
    {
        int nl = (global_alpha ? alpha_sa : M), s0 = M - r0, c;
        fprintf(outputv, "\n  the pre-estimates the conditional step produced\n");
        fprintf(outputv, "    %s (%d x %d):\n",
                global_alpha ? "psi, with Lambda = A*psi" : "Lambda", nl, r0);
        c = nhead - nlam;                       /* donde empieza Lambda en x   */
        for (i = 1; i <= nl; i++) {
            fprintf(outputv, "     ");
            for (j = 1; j <= r0; j++)
                fprintf(outputv, " %12.6f", x[c + (i - 1) * r0 + j]);
            fprintf(outputv, "\n");
        }
        if (global_fixb2)
            fprintf(outputv, "    B2 (%d x %d): held fixed by -fixb2\n", s0, r0);
        else {
            fprintf(outputv, "    B2 (%d x %d):\n", s0, r0);
            c = nhead + nmid;                   /* donde empieza B2 en x       */
            for (i = 1; i <= s0; i++) {
                fprintf(outputv, "     ");
                for (j = 1; j <= r0; j++)
                    fprintf(outputv, " %12.6f", x[c + (j - 1) * s0 + i]);
                fprintf(outputv, "\n");
            }
        }
        if (nmean > 0) {
            fprintf(outputv, "    mean block (%d):  ", nmean);
            for (i = 1; i <= nmean; i++) fprintf(outputv, " %12.6f", x[i]);
            fprintf(outputv, "\n");
        }
    }

    if (!quiet_mode)
        printf("  -seedgate: r=0 logL = %.6f  ->  perfilando Lambda y B2: %.6f\n",
               gate_seed_ll0, gate_seed_ll1);
    return 1;
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
               "                 [-differenced] [-fixb2] [-lrtest] [-rungs]\n\n");
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
        printf("                 incompatible with -differenced\n\n");
        printf("  -specs         the specification ladder: warma, mawarma, marow,\n");
        printf("                 matri and free, in one run, with npar, logL,\n");
        printf("                 termination, the rank condition G, the smallest\n");
        printf("                 MA root, B2 and an ADMISSIBLE column -- and no\n");
        printf("                 chi2 p-value where the theory does not give one\n\n");
        printf("  -artest N      test the triangular short-run dynamics\n");
        printf("                 (Gamma_i = M_i alpha': every lag enters through\n");
        printf("                 W) against free F, with N bootstrap replications\n\n");
        printf("  -matest N      test the inherited moving average against the\n");
        printf("                 free one with N parametric bootstrap replications\n");
        printf("                 under the restricted model.  The chi2 reference\n");
        printf("                 is printed too, and it is NOT a test: the\n");
        printf("                 unrestricted optimum sits on the edge of the\n");
        printf("                 admissible region\n\n");
        printf("  -rankadm [tol] refuse parameter points where the fitted model\n");
        printf("                 denies its own rank: sigma_min of\n");
        printf("                 Lambda_perp' Theta(1) B_perp below tol (default\n");
        printf("                 0.2, which is the empty gap between the 0.016-\n");
        printf("                 0.133 the degenerate fits give and the 0.52-1.00\n");
        printf("                 the admissible ones do).  REPORTED always\n\n");
        printf("  -warma         parameterise the TRANSFORMED system directly, in\n");
        printf("                 the coordinates the triangular model is stated in:\n");
        printf("                 Phi*_k = [0 Psi_k ; 0 Phi_k], Theta*_k diagonal\n");
        printf("                 in the W block, and B2 entering ONLY through the\n");
        printf("                 data.  This is the class the BVECM theorems cover\n\n");
        printf("  -marow         Theta = [T11 T12 ; 0 0]: the differenced block\n");
        printf("                 carries no moving average of its own, but the\n");
        printf("                 cross block stays free.  q*s*M parameters fewer\n");
        printf("                 than free\n\n");
        printf("  -matri         the moving average is BLOCK-TRIANGULAR:\n");
        printf("                 Theta = [T11 T12 ; 0 T22].  Only the lower-left\n");
        printf("                 block is zeroed -- the differenced block keeps\n");
        printf("                 its own moving average.  q*s*r parameters fewer\n");
        printf("                 than free\n\n");
        printf("  -mawarma       the moving average INHERITS its structure instead\n");
        printf("                 of being free: Theta = [T11  T11*B2' ; 0  0], the\n");
        printf("                 form a WARMA process implies for its VEC\n");
        printf("                 representation (BVECM corollary 2).  q*r*r\n");
        printf("                 parameters instead of q*M*M\n\n");
        printf("  -seedjoh       seed B2 with the canonical reduced-rank solution\n");
        printf("                 (Johansen's eigenvalue problem, closed form)\n");
        printf("                 instead of the static OLS regression\n\n");
        printf("  -seedb2 v      start B2 at v and estimate it FREE (not -fixb2,\n");
        printf("                 which holds it).  A measuring instrument: it is\n");
        printf("                 how you ask whether the answer depends on where\n");
        printf("                 B2 starts\n\n");
        printf("  -seedgate      seed the r >= 1 fit by profiling: estimate the\n");
        printf("                 r = 0 rung, hold F, Theta and Sigma there, and fit\n");
        printf("                 Lambda and B2 on it before releasing everything.\n");
        printf("                 Not the default: the recorded results rest on the\n");
        printf("                 cold start, and this moves only when measured to\n");
        printf("                 be at least as good\n\n");
        printf("  -rungs         the ladder below the rank: rungs 0 (F, Theta and\n");
        printf("                 Sigma diagonal), 1 (Sigma free) and 2 (F and Theta\n");
        printf("                 free), all at r = 0, with their chi2 LRs.  These are\n");
        printf("                 ordinary nested comparisons; adding the VEC matrix\n");
        printf("                 is not, and lives in -lrtest\n\n");
        /*  El programa tiene mas opciones de las que caben aqui, y una lista
         *  duplicada en dos sitios diverge.  Se dice donde esta la completa.  */
        printf("This is not the full list: docs/USAGE.md documents every option,\n"
               "and docs/GETTING_STARTED.md the order to use them in -- select\n"
               "the rank at q = 0, look at -specs at the selected rank, then\n"
               "estimate.  Every fit reports the rank condition, and one that\n"
               "denies the rank it was estimated at says so.\n");
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
        else if (strcmp(argv[i], "-rungs") == 0)   global_rungs = 1;
        else if (strcmp(argv[i], "-seedgate") == 0) global_seedgate = 1;
        else if (strcmp(argv[i], "-seedjoh") == 0)  global_seedjoh = 1;
        else if (strcmp(argv[i], "-mawarma") == 0)  global_mawarma = 1;
        else if (strcmp(argv[i], "-matri") == 0)    global_matri = 1;
        else if (strcmp(argv[i], "-marow") == 0)    global_marow = 1;
        else if (strcmp(argv[i], "-warma") == 0)    global_warma = 1;
        else if (strcmp(argv[i], "-specs") == 0)    global_specs = 1;
        else if (strcmp(argv[i], "-artest") == 0 && i+1 < argc)
            global_artest = atoi(argv[++i]);
        else if (strcmp(argv[i], "-matest") == 0 && i+1 < argc)
            global_matest = atoi(argv[++i]);
        else if (strcmp(argv[i], "-rankadm") == 0) {
            global_rankadm = 1;
            if (i+1 < argc && argv[i+1][0] != '-') global_rankadm_tol = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-seedb2") == 0 && i+1 < argc) {
            global_seedb2 = 1; global_seedb2_value = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-levels") == 0)  global_levels = 1;  /* default */
        else if (strcmp(argv[i], "-differenced") == 0) global_levels = 0;
        else if (strcmp(argv[i], "-writeinp") == 0 && i+1 < argc) {
            global_writeinp = 1; inp_prefix = argv[++i];
        }
        else if (strcmp(argv[i], "-writeres") == 0 && i+1 < argc) {
            global_writeres = 1; inp_prefix = argv[++i];
        }
        else if (strcmp(argv[i], "-eval") == 0) global_eval = 1;
        else if (strcmp(argv[i], "-fdhess") == 0) global_fdhess = 1;
        else if (strcmp(argv[i], "-bootstrap") == 0 && i+1 < argc)
            global_boot = atoi(argv[++i]);
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
    /*  -rungs — LA ESCALERA, emitida por el programa y no armada por el usuario.
     *
     *  POR QUE.  La construccion de la suite es de OPTIMOS HACIA OPTIMOS: cada
     *  peldano se estima, se certifica y se entrega al de arriba.  Hasta ahora
     *  drvec sabia certificar SU BASE (la puerta diagonal) y sabia contrastar el
     *  rango (-lrtest), pero los peldanos intermedios -- los que van de la base
     *  a la dinamica cruzada libre -- habia que armarlos a mano, corriendo el
     *  programa tres veces y restando.  Un usuario que hace eso a mano se
     *  equivoca de grados de libertad, y sobre todo no deja constancia.
     *
     *  QUE ES CADA PELDANO.  Todos con r = 0, o sea sin matriz VEC todavia: lo
     *  que se anade es estructura de correlacion, no cointegracion.
     *
     *    0   F, Theta y Sigma diagonales    la base certificada; la
     *                                       verosimilitud factoriza
     *    1   Sigma libre                    correlacion contemporanea
     *    2   F y Theta libres               dinamica cruzada
     *
     *  Los tres son comparaciones anidadas ORDINARIAS -- el modelo restringido
     *  es un punto interior del amplio --, asi que la logL no puede bajar y el
     *  estadistico es chi2 con los grados de libertad que se imprimen.  El
     *  peldano siguiente, r = 1, NO es ordinario ni en la siembra ni en la
     *  distribucion, y por eso vive en -lrtest y no aqui: ver
     *  docs/VEC_EMBEDDING_PLAN.md.                                           */
    if (global_rungs) {
        const int NR = 3;
        const int DAR[3] = {1, 1, 0}, DMA[3] = {1, 1, 0}, DCOV[3] = {1, 0, 0};
        const char *NAME[3] = { "0  F, Theta, Sigma diagonal",
                                "1  Sigma free",
                                "2  F and Theta free" };
        real *ll  = vector(0, NR - 1);
        int  *npr = ivector(0, NR - 1);
        int  *good = ivector(0, NR - 1);
        int k;

        macheps = cmacheps();
        global_r = 0;

        fprintf(outputv, "\n=== The ladder: rungs at r = 0 ===\n");
        printf("\nThe ladder, rungs at r = 0:\n");

        for (k = 0; k < NR; k++) {
            struct Tvarma vr;
            real *xr, *devr, **covr;
            int np, ifr = 0;

            global_diag_ar = DAR[k]; global_diag_ma = DMA[k];
            global_diag_cov = DCOV[k];
            build_y2_levels();
            np = calc_nparametrs();
            xr = vector(1, np); devr = vector(1, np);
            covr = matrix(1, np, 1, np);
            vr.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;
            init_guess(xr, np);
            vec_shootx(xr, &vr, &ifr, 1, 0);
            est(&vec_shootx, np, xr, devr, covr, 500, 200, 1e-5, 1e-7,
                vr.xitol, vr.a, &vr.sigma2, &vr.logelf, &ifr);
            good[k] = (ifr == 0);
            npr[k]  = np;
            ll[k]   = good[k] ? vr.logelf : 0.0;
            printf("  rung %s : %s (ifault=%d)\n", NAME[k],
                   good[k] ? "ok" : "estimation failed", ifr);
            /*  El peldano 0 es la base: se le exige su contrato aqui mismo, que
             *  es donde se esta construyendo sobre el.                        */
            if (k == 0 && good[k]) {
                vec_shootx(xr, &vr, &ifr, 0, 0);       /* recuperar el ajuste */
                gate_contract(&vr);
            }
            vec_shootx(xr, &vr, &ifr, 0, 1);           /* liberar */
            free_matrix(covr, 1, np, 1, np);
            free_vector(devr, 1, np);
            free_vector(xr, 1, np);
        }

        fprintf(outputv, "\n  rung                          npar         logL"
                         "         AIC         BIC\n");
        fprintf(outputv, "  --------------------------------------------------"
                         "-------------------\n");
        for (k = 0; k < NR; k++) {
            if (!good[k]) {
                fprintf(outputv, "  %-28s  --   estimation failed\n", NAME[k]);
                continue;
            }
            fprintf(outputv, "  %-28s %4d %12.4f %11.4f %11.4f\n", NAME[k],
                    npr[k], ll[k],
                    (-2.0 * ll[k] + 2.0 * npr[k]) / nobs,
                    (-2.0 * ll[k] + npr[k] * log((real) nobs)) / nobs);
        }

        fprintf(outputv, "\n  step        LR = 2*[L(k+1) - L(k)]    df    "
                         "p-value\n");
        fprintf(outputv, "  ------------------------------------------------"
                         "-----\n");
        for (k = 0; k + 1 < NR; k++) {
            real lr;
            int df;
            if (!good[k] || !good[k + 1]) {
                fprintf(outputv, "  %d -> %d      not available\n", k, k + 1);
                continue;
            }
            lr = 2.0 * (ll[k + 1] - ll[k]);
            df = npr[k + 1] - npr[k];
            fprintf(outputv, "  %d -> %d   %14.4f       %3d   %9.4f%s\n",
                    k, k + 1, lr, df,
                    (df > 0 && lr > 0.0) ? gsl_cdf_chisq_Q(lr, df) : 1.0,
                    (lr < -1.0e-6) ? "   *** NEGATIVE: the wider fit is worse,"
                                     " so it did not converge" : "");
        }

        fprintf(outputv,
            "\n  These are ordinary nested comparisons: the restricted model is\n"
            "  an INTERIOR point of the wider one, so the log-likelihood cannot\n"
            "  fall and the statistic is chi2 on the stated degrees of freedom.\n"
            "  The next rung -- adding the VEC matrix, r = 1 -- is not ordinary\n"
            "  in either respect: the null sits ON the boundary of the alternative\n"
            "  and B2 is unidentified under it.  It is reported by -lrtest, with\n"
            "  a parametric bootstrap available for its distribution.\n");

        free_ivector(good, 0, NR - 1);
        free_ivector(npr, 0, NR - 1);
        free_vector(ll, 0, NR - 1);
        fclose(outputv);
        printf("Done. Output written to %s\n", outputf);
        cleanup_names(outputf, inputf, base_name);
        return 0;
    }

    /*  -specs — LA ESCALERA DE ESPECIFICACIONES, emitida por el programa.
     *
     *  Cinco modelos ANIDADOS de la media movil y la dinamica corta, del mas
     *  restringido al libre, y en un solo comando:
     *
     *    warma    Phi*_k = [0 Psi_k ; 0 Phi_k] y Theta* diagonal -- la clase
     *             que los teoremas cubren (docs/THEORY.md, definicion 3)
     *    mawarma  Theta = [T11 T11B2' ; 0 0]   -- la misma estructura de MA
     *             con F libre
     *    marow    Theta = [T11 T12 ; 0 0]      -- el bloque cruzado libre
     *    matri    Theta = [T11 T12 ; 0 T22]    -- y el diferenciado con MA
     *    libre    Theta libre
     *
     *  Por que en un comando: porque la pregunta que un usuario tiene delante
     *  no es "cuanto ajusta esta especificacion" sino "cual de ellas, y es
     *  admisible", y esas dos no se contestan con una corrida.
     *
     *  Y POR QUE LA COLUMNA DE ADMISIBILIDAD ES LA PRIMERA QUE HAY QUE LEER.
     *  Por el teorema 3 de docs/THEORY.md el proceso tiene rango r si y solo si
     *  rank(Lambda_perp' Theta(1)) = M - r, y por el teorema 4 el conjunto donde
     *  eso falla esta DENTRO del que el optimizador recorre.  Un peldano con G
     *  pequeno no es un ajuste peor: es un ajuste de otro modelo.  Por el
     *  corolario 5.1, ademas, ni sus errores estandar ni un LR contra el tienen
     *  su distribucion, asi que el p-valor chi2 se imprime SOLO cuando los dos
     *  peldanos comparados son admisibles, y donde no, se dice y se remite a
     *  -matest.                                                              */
    if (global_specs) {
        const int NS = 5;
        const char *NM[5] = { "warma  ", "mawarma", "marow  ", "matri  ", "free   " };
        real ll[5], gg[5], mam[5], b2v[5];
        int  npv[5], okv[5], tcv[5], adm[5];
        int  k, M = nser;

        macheps = cmacheps();
        if (global_r < 1) {
            fprintf(stderr, "ERROR: -specs needs r >= 1\n");
            exit(1);
        }
        fprintf(outputv, "\n=== The specification ladder ===\n");
        printf("\nEscalera de especificaciones:\n");

        for (k = 0; k < NS; k++) {
            struct Tvarma vs;
            real *xs, *devs, **covs;
            int np, ifs = 0, s2 = M - global_r;

            global_warma = (k == 0); global_mawarma = (k == 1);
            global_marow = (k == 2);  global_matri   = (k == 3);
            build_y2_levels();
            np = calc_nparametrs();
            xs = vector(1, np); devs = vector(1, np);
            covs = matrix(1, np, 1, np);
            vs.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;
            init_guess(xs, np);
            if (global_warma) {
                /*  el mismo encogido admisible que usa la ruta normal        */
                static const real shr[6] = { 1.0, 0.8, 0.5, 0.3, 0.1, 0.0 };
                int nm2, nl2, nmid2, nt2, nfw = (global_p > 1) ? global_p - 1 : 0;
                int nar, i2, mi;
                real *ar0;
                par_blocks(&nm2, &nl2, &nmid2, &nt2);
                nar = nl2 + nfw * M * global_r;
                ar0 = vector(1, (nar > 0 ? nar : 1));
                for (i2 = 1; i2 <= nar; i2++) ar0[i2] = xs[nm2 + i2];
                vec_shootx(xs, &vs, &ifs, 1, 0);
                for (mi = 0; mi < 6; mi++) {
                    real p1, p2, p3; int ife = 0, ifc = 0;
                    for (i2 = 1; i2 <= nar; i2++) xs[nm2 + i2] = shr[mi] * ar0[i2];
                    vec_shootx(xs, &vs, &ifc, 0, 0);
                    if (ifc != 0) continue;
                    elf(vs.m, vs.n, vs.p, vs.q, vs.mu, vs.phi, vs.theta, vs.qq,
                        vs.w, 1.0, vs.xitol, TRUE, vs.a, &p1, &p2, &p3, &ife);
                    if (ife == 0) break;
                }
                free_vector(ar0, 1, (nar > 0 ? nar : 1));
                vec_shootx(xs, &vs, &ifs, 0, 1);
            }
            vec_shootx(xs, &vs, &ifs, 1, 0);
            est(&vec_shootx, np, xs, devs, covs, 500, 200, 1e-5, 1e-7,
                vs.xitol, vs.a, &vs.sigma2, &vs.logelf, &ifs);
            okv[k] = (ifs == 0);
            npv[k] = np;
            ll[k]  = okv[k] ? vs.logelf : 0.0;
            tcv[k] = termcode_from_out(outputf);
            gg[k] = -1.0; mam[k] = -1.0; b2v[k] = 0.0;
            if (okv[k]) {
                real mm = 1.0e30;
                vec_shootx(xs, &vs, &ifs, 0, 0);      /* recuperar el ajuste  */
                report_operator_roots("MA", vs.theta, vs.m, vs.q, &mm, 1);
                mam[k] = (mm < 1.0e29) ? mm : -1.0;
                for (int j2 = 1; j2 <= global_r; j2++)
                    b2v[k] = global_fixb2 ? B2_fixed[1][j2]
                           : xs[np - s2 * global_r + (j2 - 1) * s2 + 1];
                if (global_warma) {
                    /*  en coordenadas Ybar hay que volver primero            */
                    real **B2w = matrix(1, s2, 1, global_r);
                    real **Lw = matrix(1, M, 1, global_r);
                    real ***Fw = tensor(1, (global_p > 1 ? global_p - 1 : 1), 1, M, 1, M);
                    real ***Tw = tensor(1, (global_q > 0 ? global_q : 1), 1, M, 1, M);
                    real **Sw = matrix(1, M, 1, M);
                    for (int j2 = 1; j2 <= global_r; j2++)
                        for (int i2 = 1; i2 <= s2; i2++)
                            B2w[i2][j2] = global_fixb2 ? B2_fixed[i2][j2]
                                        : xs[np - s2*global_r + (j2-1)*s2 + i2];
                    for (int k2 = 1; k2 <= (global_q > 0 ? global_q : 1); k2++)
                        for (int i2 = 1; i2 <= M; i2++)
                            for (int j2 = 1; j2 <= M; j2++) Tw[k2][i2][j2] = 0.0;
                    warma_inverse(&vs, B2w, Lw, Fw, Tw, Sw);
                    gg[k] = granger_smin(Lw, B2w, Tw, M, global_r, global_q);
                    free_matrix(Sw, 1, M, 1, M);
                    free_tensor(Tw, 1, (global_q > 0 ? global_q : 1), 1, M, 1, M);
                    free_tensor(Fw, 1, (global_p > 1 ? global_p - 1 : 1), 1, M, 1, M);
                    free_matrix(Lw, 1, M, 1, global_r);
                    free_matrix(B2w, 1, s2, 1, global_r);
                } else gg[k] = granger_sv;
            }
            adm[k] = (okv[k] && gg[k] >= global_rankadm_tol);
            printf("  %s : %s\n", NM[k], okv[k] ? "ok" : "fallo");
            vec_shootx(xs, &vs, &ifs, 0, 1);
            free_matrix(covs, 1, np, 1, np);
            free_vector(devs, 1, np);
            free_vector(xs, 1, np);
        }
        global_warma = global_mawarma = global_marow = global_matri = 0;

        fprintf(outputv,
          "\n  spec      npar          logL   term        G     MAmin       B2  adm\n"
          "  ------------------------------------------------------------------------\n");
        for (k = 0; k < NS; k++) {
            const char *tn = (tcv[k] == 1) ? "grad" : (tcv[k] == 2) ? "step"
                           : (tcv[k] == 3) ? "lower" : (tcv[k] == 0) ? "--" : "gave up";
            if (!okv[k]) {
                fprintf(outputv, "  %s  %4d   estimation failed\n", NM[k], npv[k]);
                continue;
            }
            fprintf(outputv, "  %s  %4d %13.4f  %-6s %8.3e %8.3f %8.4f  %s\n",
                    NM[k], npv[k], ll[k], tn, gg[k],
                    (mam[k] >= 0.0) ? mam[k] : 0.0, b2v[k],
                    adm[k] ? "yes" : "NO");
        }

        fprintf(outputv,
          "\n  step               LR     df   verdict\n"
          "  ----------------------------------------------------------------------\n");
        for (k = 0; k + 1 < NS; k++) {
            real lr;
            int df = npv[k+1] - npv[k];
            if (!okv[k] || !okv[k+1]) {
                fprintf(outputv, "  %s -> %s   not available\n", NM[k], NM[k+1]);
                continue;
            }
            lr = 2.0 * (ll[k+1] - ll[k]);
            fprintf(outputv, "  %s -> %s %8.3f %4d   ", NM[k], NM[k+1], lr, df);
            if (lr < -1.0e-6)
                fprintf(outputv, "NEGATIVE: the wider fit is worse, so it did "
                                 "not converge\n");
            else if (adm[k] && adm[k+1])
                fprintf(outputv, "chi2 p = %.4f\n",
                        (df > 0) ? gsl_cdf_chisq_Q(lr, df) : 1.0);
            else
                fprintf(outputv, "no p-value: %s is not admissible, so the "
                                 "statistic is not chi2 (-matest)\n",
                        adm[k] ? NM[k+1] : NM[k]);
        }

        fprintf(outputv,
          "\n  READ THE LAST COLUMN FIRST.  By Theorem 3 of docs/THEORY.md the\n"
          "  process has cointegrating rank r if and only if\n"
          "  rank(Lambda_perp' Theta(1)) = M - r, and by Theorem 4 the set where\n"
          "  that fails lies INSIDE the one the optimiser searches.  A rung with\n"
          "  a small G is not a worse fit of this model: it is a fit of another\n"
          "  one, whose rank is not the rank it was estimated at.  Corollary 5.1\n"
          "  then removes the usual distributions, which is why no chi2 p-value\n"
          "  is printed for a comparison involving it.  The floor used here is\n"
          "  %.1e (-rankadm sets it).\n", global_rankadm_tol);

        fclose(outputv);
        printf("Done. Output written to %s\n", outputf);
        cleanup_names(outputf, inputf, base_name);
        return 0;
    }

    /*  -matest N — el contraste del MA heredado contra el libre, con su
     *  distribucion SIMULADA en vez de supuesta.  Es un modo y termina aqui.  */
    if (global_matest > 0 || global_artest > 0) {
        /*  Dos contrastes con la misma maquinaria y distinta pareja:
         *    -matest   H0 = mawarma  contra  H1 = libre   (la mitad de MA)
         *    -artest   H0 = warma    contra  H1 = mawarma (la mitad de AR)
         *  El segundo es el que el paso 5 del plan pedia, y su razon para
         *  simular NO es la misma: aqui el modelo no restringido SI es
         *  admisible (G entre 0.90 y 1.00 en todo el banco), asi que no es el
         *  problema de frontera de 4i.  Es que la restriccion Gamma_i = m_i
         *  alpha' es de RANGO REDUCIDO sobre F_i, y el LR de una restriccion de
         *  rango no es chi2 cuando el rango verdadero puede estar por debajo
         *  del que la restriccion permite -- y en cinco de los ocho pares F1
         *  sale casi de rango uno, con lo que m_i queda casi no identificado.  */
        int use_ar = (global_artest > 0);
        int reps = use_ar ? global_artest : global_matest;
        int k0 = use_ar ? 0 : 1, k1 = use_ar ? 1 : 4;
        int np0, np1, ok0 = 0, ok1 = 0, nb, df;
        real l0, l1, lr, cv[3] = {0,0,0}, pv = -1.0;
        real *x0, *dev0, **cov0;
        struct Tvarma v0;
        int ifr = 0;

        macheps = cmacheps();
        if (global_r < 1 || global_q < 1) {
            fprintf(stderr, "ERROR: -matest/-artest need r >= 1 and q >= 1\n");
            exit(1);
        }
        /* [1] el restringido, que es H0 -- y se guarda, porque de el se simula */
        set_spec(k0);
        build_y2_levels();
        np0 = calc_nparametrs();
        x0 = vector(1, np0); dev0 = vector(1, np0); cov0 = matrix(1, np0, 1, np0);
        v0.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;
        init_guess(x0, np0);
        vec_shootx(x0, &v0, &ifr, 1, 0);
        est(&vec_shootx, np0, x0, dev0, cov0, 500, 200, 1e-5, 1e-7,
            v0.xitol, v0.a, &v0.sigma2, &v0.logelf, &ifr);
        ok0 = (ifr == 0); l0 = v0.logelf;
        vec_shootx(x0, &v0, &ifr, 0, 1);

        /* [2] el no restringido */
        set_spec(k1);
        l1 = fit_ll(global_r, &ok1);
        np1 = calc_nparametrs();
        df = np1 - np0;

        if (!ok0 || !ok1) {
            fprintf(outputv, "\n%s: one of the two fits failed "
                             "(restricted %s, unrestricted %s); no test.\n",
                    use_ar ? "-artest" : "-matest",
                    ok0 ? "ok" : "failed", ok1 ? "ok" : "failed");
        } else {
            lr = 2.0 * (l1 - l0);
            /*  La cabecera y los numeros van en DOS llamadas y no en una con
             *  la cadena de formato condicional: con el ternario, los literales
             *  que venian detras se concatenan a una sola de las dos ramas y
             *  los %f se quedan sin formato en la otra.  Compila, y se pierden
             *  los numeros en silencio -- que es como se perdieron la primera
             *  vez que se escribio esto.                                     */
            fprintf(outputv, "%s", use_ar
                ? "\n=== The triangular short-run dynamics against free F ===\n\n"
                  "  H0: Gamma_i = M_i alpha' -- every lag enters through W,\n"
                  "      which is what the triangular class implies\n"
                  "  H1: F_i free (the moving-average structure held in both)\n\n"
                : "\n=== The inherited moving average against the free one ===\n\n"
                  "  H0: Theta = [T11  T11*B2' ; 0  0], the structure a WARMA\n"
                  "      process implies for its VEC representation\n"
                  "  H1: Theta free\n\n");
            fprintf(outputv,
                "  restricted     logL = %15.10f   (%d parameters)\n"
                "  unrestricted   logL = %15.10f   (%d parameters)\n"
                "  LR = 2*[L(H1) - L(H0)] = %.4f   on %d df\n"
                "  chi2 p-value, FOR REFERENCE ONLY          = %.4f\n",
                l0, np0, l1, np1, lr, df,
                (lr > 0.0) ? gsl_cdf_chisq_Q(lr, df) : 1.0);
            printf("  restringido %.6f   libre %.6f   LR %.4f (%d gl)\n",
                   l0, l1, lr, df);

            nb = bootstrap_ma(x0, np0, reps, cv, &pv, lr, k0, k1);
            if (nb >= 10) {
                fprintf(outputv,
                  "\n  Parametric bootstrap under H0, %d of %d replications "
                  "usable:\n"
                  "    critical values   10%%: %8.4f   5%%: %8.4f   1%%: %8.4f\n"
                  "    bootstrap p-value = %.4f\n", nb, reps,
                  cv[0], cv[1], cv[2], pv);
                fprintf(outputv, "    verdict: %s\n",
                    (lr > cv[2]) ? "reject H0 at 1%" :
                    (lr > cv[1]) ? "reject H0 at 5%" :
                    (lr > cv[0]) ? "reject H0 at 10%" : "H0 not rejected");
                /*  El veredicto se lee de los VALORES CRITICOS y el p-valor
                 *  cuenta ademas el estadistico observado, asi que con pocas
                 *  replicas los dos pueden quedar a distinto lado de un
                 *  umbral.  Decirlo es mas barato que elegir uno y callar.   */
                if ((lr > cv[0] && pv > 0.10) || (lr > cv[1] && pv > 0.05) ||
                    (lr > cv[2] && pv > 0.01))
                    fprintf(outputv,
                      "    (the p-value counts the observed statistic itself, so\n"
                      "     with B = %d it can sit on the other side of the same\n"
                      "     threshold as the critical value; both are printed)\n",
                      reps);
                printf("  bootstrap: p = %.4f  (%d/%d replicas)\n", pv, nb,
                       reps);
            } else {
                fprintf(outputv, "\n  Parametric bootstrap: only %d usable "
                                 "replications; no critical values.\n", nb);
            }
            fprintf(outputv, use_ar ?
              "\n  WHY THE BOOTSTRAP AND NOT THE chi2, and the reason is NOT\n"
              "  the one in -matest.  Here the unrestricted model IS admissible\n"
              "  -- the rank condition reads 0.90 to 1.00 across the bank -- so\n"
              "  this is not a boundary of the model class.  It is that\n"
              "  Gamma_i = M_i alpha' is a REDUCED-RANK restriction on F_i, and\n"
              "  the likelihood ratio for a rank restriction is not chi2 when\n"
              "  the true rank may be below the one the restriction allows: in\n"
              "  five of the eight pairs F1 comes out near rank one already, and\n"
              "  M_i is then close to unidentified.  The bootstrap is simulated\n"
              "  FROM THE RESTRICTED FIT, which is H0.\n"
              :
              "\n  WHY THE BOOTSTRAP AND NOT THE chi2.  The unrestricted\n"
              "  optimum on this kind of data stops in the neighbourhood where\n"
              "  the rank condition degenerates -- sigma_min(Lambda_perp'\n"
              "  Theta(1) B_perp) of order 0.01-0.1 against ~1 for a correctly\n"
              "  specified model, with Theta(1) singular to working precision.\n"
              "  An LR whose unrestricted estimate sits on the edge of the\n"
              "  admissible region does not have its asymptotic distribution,\n"
              "  so the chi2 column above is a reference and not a test.  The\n"
              "  bootstrap distribution is simulated FROM THE RESTRICTED FIT,\n"
              "  which is H0, and it inherits the sample size, the\n"
              "  deterministic case and the moving-average structure.\n"
              "  Read it with its floor of 1/(B+1) = %.4f and its Monte Carlo\n"
              "  error sqrt(p(1-p)/B) = %.4f at p = 0.05.\n"
              "  See docs/HOMOLOGATION.md 4g and 4h.\n",
              1.0 / (real) (reps + 1),
              sqrt(0.05 * 0.95 / (real) reps));
        }
        free_matrix(cov0, 1, np0, 1, np0);
        free_vector(dev0, 1, np0);
        free_vector(x0, 1, np0);
        fclose(outputv);
        printf("Done. Output written to %s\n", outputf);
        cleanup_names(outputf, inputf, base_name);
        return 0;
    }

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

        /* Con -bootstrap hay que poder SIMULAR desde el ajuste de cada rango,
           asi que se guarda su vector de parametros en vez de liberarlo.      */
        real **xkeep = NULL; int *npkeep = NULL;
        if (global_boot > 0) {
            xkeep  = (real **) malloc((size_t)(M + 1) * sizeof(real *));
            npkeep = ivector(0, M - 1);
            for (int rr = 0; rr <= M - 1; rr++) { xkeep[rr] = NULL; npkeep[rr] = 0; }
        }

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
            if (global_boot > 0 && ok) {
                xkeep[rr] = vector(1, np); npkeep[rr] = np;
                for (int i2 = 1; i2 <= np; i2++) xkeep[rr][i2] = xr[i2];
            }
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

        /* ---- valores criticos por bootstrap parametrico, si se piden ------- */
        if (global_boot > 0) {
            fprintf(outputv,
                "\n  === Parametric bootstrap under H0 (%d replications) ===\n"
                "  The asymptotic values above are known to be optimistic at these\n"
                "  sample sizes: measured, the sequential test over-rejects about\n"
                "  SIX times its nominal level at n = 120, and these percentiles cut\n"
                "  that to four without closing it.  They are empirical\n"
                "  percentiles of the statistic simulated FROM THE FITTED MODEL at\n"
                "  rank r, so they carry the sample size, the deterministic case and\n"
                "  the MA component that the tables cannot.\n", global_boot);
            fprintf(outputv, "\n  r    M-r        LR      10%%      5%%      1%%"
                             "   p-value  reps\n");
            fprintf(outputv, "  ------------------------------------------------"
                             "-----------------\n");
            printf("\nBootstrap under H0 (%d replications per comparison):\n",
                   global_boot);
            for (int rr = 0; rr <= M - 2; rr++) {
                real cv[3] = {0.0, 0.0, 0.0}, pv = -1.0, lr;
                int reps;
                if (!good[rr] || !good[rr+1] || xkeep == NULL || xkeep[rr] == NULL) {
                    fprintf(outputv, "  %-4d  --   (a model in the pair failed)\n", rr);
                    continue;
                }
                lr = 2.0 * (ll[rr+1] - ll[rr]);
                printf("  r = %d -> %d ...", rr, rr+1); fflush(stdout);
                reps = bootstrap_rank(rr, xkeep[rr], npkeep[rr], global_boot,
                                      cv, &pv, lr);
                if (reps < 10) {
                    fprintf(outputv, "  %-4d %4d %10.4f   only %d usable "
                                     "replications: not reported\n",
                            rr, M - rr, lr, reps);
                    printf(" solo %d replicas utiles\n", reps);
                    continue;
                }
                fprintf(outputv, "  %-4d %4d %10.4f %8.2f %8.2f %8.2f  %7.4f  %4d",
                        rr, M - rr, lr, cv[0], cv[1], cv[2], pv, reps);
                /* El veredicto se lee de los VALORES CRITICOS, no del p-valor,
                   porque el p-valor tiene un suelo de 1/(reps+1): con 100
                   replicas no puede bajar de 0.0099, asi que "rechaza al 1%"
                   seria inalcanzable por construccion aunque el estadistico
                   supere el percentil 99.                                    */
                if      (lr > cv[2]) fprintf(outputv, "   reject H0 at 1%%\n");
                else if (lr > cv[1]) fprintf(outputv, "   reject H0 at 5%%\n");
                else if (lr > cv[0]) fprintf(outputv, "   reject H0 at 10%%\n");
                else                 fprintf(outputv, "   H0 not rejected\n");
                printf(" p = %.4f (%d replicas)\n", pv, reps);
            }
            /* El error de Monte Carlo, dicho en vez de escondido: la contingencia
               del plan pedia reportarlo si N tenia que ser pequeno.            */
            fprintf(outputv,
                "\n  Monte Carlo error, said rather than hidden:\n"
                "   - a bootstrap p-value from B replications has standard error\n"
                "     sqrt(p(1-p)/B); at p = 0.05 and B = %d that is %.4f, so read a\n"
                "     p-value near a threshold as undecided, not as a decision;\n"
                "   - and it has a FLOOR of 1/(B+1) = %.4f -- with this B the p-value\n"
                "     cannot go below that however extreme the statistic is, which is\n"
                "     why the verdict above is read from the critical values.  For a\n"
                "     p-value that can resolve 1%%, B >= 999.\n"
                "   - replications where either fit failed to converge, or gave\n"
                "     LR < 0, are discarded and counted in the reps column.\n",
                global_boot, sqrt(0.05 * 0.95 / (real) global_boot),
                1.0 / (real) (global_boot + 1));
            for (int rr = 0; rr <= M - 1; rr++)
                if (xkeep && xkeep[rr]) free_vector(xkeep[rr], 1, npkeep[rr]);
            if (xkeep) free(xkeep);
            if (npkeep) free_ivector(npkeep, 0, M - 1);
        }

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

    /*  -seedgate: la ruta (B).  Va DESPUES de init_guess y no en su lugar, por
     *  dos razones.  La cabeza y la cola del vector -- la media, Lambda y B2 --
     *  necesitan un punto de partida para el paso condicional, y el de la
     *  regresion condicional es el que hay.  Y si el perfilado no sale, lo que
     *  queda es exactamente la ruta (C), sin ninguna ruta intermedia inventada:
     *  o cruza entero o no cruza.                                            */
    /*  -warma: un arranque ADMISIBLE, encogiendo el bloque autorregresivo.
     *
     *  El mismo muro de -seedgate por otro lado: la regresion que siembra los
     *  coeficientes de W_{t-k} puede dar un Phi* no estacionario, y entonces
     *  est no arranca siquiera ("bad initial estimates") y no hay ajuste, que
     *  es lo que pasaba en mink_muskrat.  Se recorre una escalera de factores
     *  sobre ESE bloque -- la direccion la dan los datos, la escala la
     *  admisibilidad -- y se arranca en el primero que el motor acepta.  Con
     *  factor 0 el sistema es Ybar_t = A*_t, trivialmente estacionario, asi que
     *  la escalera siempre termina.                                          */
    if (global_warma) {
        static const real shr[6] = { 1.0, 0.8, 0.5, 0.3, 0.1, 0.0 };
        int nmean_, nlam_, nmid_, ntail_, nf_w = (global_p > 1) ? global_p - 1 : 0;
        int nar, i2, mi;
        real *ar0;
        struct Tvarma vt;
        int ift = 0;

        par_blocks(&nmean_, &nlam_, &nmid_, &ntail_);
        nar = nlam_ + nf_w * nser * global_r;
        ar0 = vector(1, (nar > 0 ? nar : 1));
        for (i2 = 1; i2 <= nar; i2++) ar0[i2] = x[nmean_ + i2];
        vt.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;
        vec_shootx(x, &vt, &ift, 1, 0);
        for (mi = 0; mi < 6; mi++) {
            real pi1, pi2, pi3;
            int ifev = 0, ifc = 0;
            for (i2 = 1; i2 <= nar; i2++) x[nmean_ + i2] = shr[mi] * ar0[i2];
            vec_shootx(x, &vt, &ifc, 0, 0);
            if (ifc != 0) continue;
            elf(vt.m, vt.n, vt.p, vt.q, vt.mu, vt.phi, vt.theta, vt.qq, vt.w,
                1.0, vt.xitol, TRUE, vt.a, &pi1, &pi2, &pi3, &ifev);
            if (ifev == 0) break;
        }
        if (mi > 0 && mi < 6 && !quiet_mode)
            printf("  -warma: arranque encogido a x%.1f para que sea admisible\n",
                   shr[mi]);
        if (mi >= 6) {
            fprintf(outputv, "\n-warma: no admissible starting point was found "
                             "even with the autoregressive block at zero.\n");
            for (i2 = 1; i2 <= nar; i2++) x[nmean_ + i2] = 0.0;
        }
        vec_shootx(x, &vt, &ift, 0, 1);
        free_vector(ar0, 1, (nar > 0 ? nar : 1));
    }

    if (global_seedb2) {
        int nmean_, nlam_, nmid_, ntail_, i2;
        par_blocks(&nmean_, &nlam_, &nmid_, &ntail_);
        for (i2 = 1; i2 <= ntail_; i2++)
            x[nmean_ + nlam_ + nmid_ + i2] = global_seedb2_value;
        if (ntail_ == 0)
            fprintf(stderr, "AVISO: -seedb2 no hace nada con -fixb2 o r = 0\n");
    }
    if (global_seedgate) gate_profile_seed(x, npar);

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
    /*  EL CERTIFICADO DE OPTIMALIDAD, y hay UNA sola ventana para tomarlo.
     *
     *  La escalera de la suite dice que un `.pre` es un OPTIMO en forma
     *  reejecutable, y ese convenio es COMPROBABLE: reestimar un optimo no
     *  mueve los numeros, mientras que una especificacion si.  La diferencia
     *  entre las dos verosimilitudes -- la del ajuste y la de los valores que
     *  se trajeron -- es >= 0 por construccion y vale cero si y solo si lo
     *  que entro eran los optimos univariantes.
     *
     *  Hay que evaluar AQUI porque est() sobrescribe la estructura: una vez ha
     *  corrido, la pregunta ya no se puede contestar.  Es el mismo protocolo
     *  que la puerta de drtran (LADDER_AS_OPTIMISATION.md 2.1 y 7.1), y se
     *  hereda entero en vez de reinventarlo.                                 */
    {
        const real LOG2PI = 1.837877066;
        real pi1, pi2, pi3;
        int ifs = 0;
        vec_shootx(x, &varma1, &ifs, 0, 0);
        if (ifs == 0) {
            elf(varma1.m, varma1.n, varma1.p, varma1.q, varma1.mu, varma1.phi,
                varma1.theta, varma1.qq, varma1.w, 1.0, varma1.xitol,
                FALSE, varma1.a, &pi1, &pi2, &pi3, &ifs);
            if (ifs == 0) {
                gate_ll_start = -0.5 * varma1.m * varma1.n
                    * (LOG2PI - log((real) varma1.m) - log((real) varma1.n) + 1.0)
                    - 0.5 * varma1.n * (varma1.m * log(pi1) + log(pi2));
                gate_have_start = 1;
            }
        }
    }

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
            /*  El multiarranque sacude LA SEMILLA QUE SE ESTA MIDIENDO, no otra:
             *  con -seedgate esa es el punto perfilado, que ya esta en x, y
             *  volver a llamar a init_guess aqui mediria la ruta (C) con una
             *  etiqueta equivocada.                                           */
            /*  Con -warma pasa lo mismo que con -seedgate: el punto de
             *  partida bueno es el que ya esta en x -- encogido hasta ser
             *  admisible --, y volver a llamar a init_guess aqui devolveria el
             *  crudo, que el motor rechaza.  Medido: en mink_muskrat solo 2 de
             *  20 arranques convergian por eso.                              */
            if (gate_seed_ok || global_warma) {
                for (i2 = 1; i2 <= npar; i2++) xtry[i2] = x[i2];
            } else init_guess(xtry, npar);
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
        /* Errores estandar por el hessiano en el optimo, si se piden.  Va antes
           de recuperar la estructura final porque objcfunc rellena varmax con
           el punto que se le pase.                                           */
        if (global_fdhess) {
            if (exact_hessian_se(npar, x, dev, cov, nobs) == 0)
                fprintf(outputv, "\nStandard errors from the finite-difference "
                                 "Hessian AT the optimum (-fdhess),\n"
                                 "not from the BFGS-accumulated factor.\n");
        }
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
        /* npar, AIC y BIC tambien en el ajuste simple.  Estaban solo dentro de
           la tabla de -lrtest, y sin ellos no se puede fijar un criterio de
           especificacion mecanico: comparar modelos anidados por LR vale para
           una pareja, pero elegir dentro de un conjunto declarado pide un
           criterio de informacion.  Misma normalizacion por nobs que la tabla
           de -lrtest, para que los numeros sean el mismo numero.             */
        fprintf(outputv, "npar   : %d\n", npar);
        fprintf(outputv, "AIC    : %15.10f   (-2logL + 2k, /n)\n",
                (-2.0 * varma1.logelf + 2.0 * npar) / nobs);
        fprintf(outputv, "BIC    : %15.10f   (-2logL + k log n, /n)\n",
                (-2.0 * varma1.logelf + npar * log((real) nobs)) / nobs);
        convergence_note(termcode_from_out(outputf));
        operator_roots(&varma1);
        /*  La condicion de rango, al lado de las raices y por la misma razon:
         *  dice si el punto donde se ha parado es un modelo del rango que se
         *  pidio o de otro.  Se recalcula en la ultima evaluacion, que es la
         *  que dejo vec_shootx justo antes.                                  */
        if (global_r > 0 && global_q > 0 && granger_sv >= 0.0) {
            fprintf(outputv,
                "\nRank condition (Granger): sigma_min(Lambda_perp' Theta(1) "
                "B_perp) = %.3e\n", granger_sv);
            if (granger_sv < global_rankadm_tol) {
                /*  Y TAMBIEN A LA TERMINAL.  Un ajuste que niega su propio
                 *  rango no es un ajuste peor: es el ajuste de otro modelo, y
                 *  quien corre el programa tiene que enterarse sin abrir el
                 *  .out.  Es la decision del paso 4 del plan: el CALCULO por
                 *  defecto no se mueve -- ningun resultado registrado se mueve
                 *  --, pero la PRESENTACION deja de dar por respuesta algo que
                 *  la teoria no licencia (docs/THEORY.md, corolario 5.1).    */
                if (!quiet_mode)
                    printf("\n  *** ATENCION: sigma_min(Lambda_perp' Theta(1) "
                           "B_perp) = %.3e < %.1e\n"
                           "      Este ajuste NIEGA EL RANGO con el que se ha "
                           "estimado: no es un\n"
                           "      ajuste peor, es el ajuste de otro modelo.  Sus "
                           "errores estandar y\n"
                           "      cualquier LR contra el NO tienen su "
                           "distribucion habitual.\n"
                           "      Vea la escalera:  drvec <fichero> %d %d %d "
                           "-specs\n", granger_sv, global_rankadm_tol,
                           global_p, global_q, global_r);
                fprintf(outputv,
                  "  *** This is ZERO to working precision, and it is not a\n"
                  "  detail: that matrix is what makes the long-run impact\n"
                  "  C(1) = B_perp (Lambda_perp' Gamma B_perp)^-1 Lambda_perp'\n"
                  "  Theta(1) have rank M-r.  Where it degenerates the FITTED\n"
                  "  model denies the rank it was estimated at -- it says r and\n"
                  "  its parameters leave no stochastic trend.  The estimate is\n"
                  "  then on the edge of the region the model class allows, so\n"
                  "  standard errors and LR statistics do not have their usual\n"
                  "  distributions there.  -rankadm refuses such points; -mawarma\n"
                  "  makes them unreachable by construction.  See\n"
                  "  docs/HOMOLOGATION.md 4h.\n");
            } else
                fprintf(outputv,
                  "  Comfortably away from zero: the fit is a model of the rank\n"
                  "  it was estimated at.\n");
        }
        gate_contract(&varma1);
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
        int ii = 1, warma_done = 0;
        real **Lam_m = matrix(1, nser, 1, (r > 0 ? r : 1));

        /*  -warma: los parametros NO son los del VEC, asi que no se imprimen
         *  como si lo fueran.  Se publica lo que se ha estimado, en las
         *  coordenadas en que se ha estimado, y se dice cuales son.          */
        if (global_warma) {
            int nf_w = (global_p > 1) ? global_p - 1 : 0, kk;
            fprintf(outputv,
              "\nTriangular (WARMA) parameterisation, on Ybar_t = [nabla Y2 ; W]:\n"
              "  Ybar_t = sum_k Phi*_k Ybar_{t-k} + (I - sum_k Theta*_k L^k) A*_t\n"
              "  with Phi*_k = [0  Psi_k ; 0  Phi_k] and Theta*_k in the W block\n"
              "  only.  B2 enters ONLY through W = Y1 + B2'Y2, by subtraction.\n\n");
            if (global_case == 2) {
                fprintf(outputv, "E[W] =\n");
                for (int j = 1; j <= r; j++)
                    fprintf(outputv, "  %12.6f  (sd = %10.6f)\n", x[ii], dev[ii]), ii++;
            } else if (global_case == 3) {
                fprintf(outputv, "E[nabla Y2] and E[W] =\n");
                for (int i = 1; i <= nser; i++)
                    fprintf(outputv, "  %12.6f  (sd = %10.6f)\n", x[ii], dev[ii]), ii++;
            }
            for (kk = 1; kk <= nf_w + 1; kk++) {
                fprintf(outputv, "coefficients of W_{t-%d}  (M x r) =\n", kk);
                for (int i = 1; i <= nser; i++) {
                    fprintf(outputv, "  ");
                    for (int j = 1; j <= r; j++)
                        fprintf(outputv, "%12.6f (sd %9.6f)", x[ii], dev[ii]), ii++;
                    fprintf(outputv, "\n");
                }
            }
            for (kk = 1; kk <= global_q; kk++) {
                fprintf(outputv, "Theta*[%d] in the W block (r x r) =\n", kk);
                for (int i = 1; i <= r; i++) {
                    fprintf(outputv, "  ");
                    for (int j = 1; j <= r; j++)
                        fprintf(outputv, "%12.6f (sd %9.6f)", x[ii], dev[ii]), ii++;
                    fprintf(outputv, "\n");
                }
            }
            fprintf(outputv, "Q (M x M, lower triangle; Q[1][1] = 1) =\n  1.000000\n");
            for (int i = 2; i <= nser; i++) fprintf(outputv, "  %12.6f\n", x[ii++]);
            if (!global_diag_cov)
                for (int i = 2; i <= nser; i++)
                    for (int j = 1; j < i; j++)
                        fprintf(outputv, "  off(%d,%d) %12.6f\n", i, j, x[ii++]);
            fprintf(outputv, "B2 (s x r) =\n");
            for (int i = 1; i <= s; i++) {
                fprintf(outputv, "  ");
                for (int j = 1; j <= r; j++)
                    fprintf(outputv, "%12.6f%s", global_fixb2 ? B2_fixed[i][j] : x[ii],
                            global_fixb2 ? "" : "");
                if (!global_fixb2) ii += r;
                fprintf(outputv, "\n");
            }
            if (ii != npar + 1)
                fprintf(stderr, "ERROR output (-warma): consumed %d of %d\n",
                        ii - 1, npar);

            /*  Y DE VUELTA A LAS COORDENADAS VEC, una sola vez, al final.
             *  Es lo que hace utilizable esta ruta: se estima donde la clase
             *  es un patron de ceros y se REPORTA donde el usuario lee.      */
            if (r > 0) {
                real **B2w = matrix(1, s, 1, (r > 0 ? r : 1));
                real **Lw  = matrix(1, nser, 1, r);
                real ***Fw = tensor(1, (global_p > 1 ? global_p - 1 : 1),
                                    1, nser, 1, nser);
                real ***Tw = tensor(1, (global_q > 0 ? global_q : 1),
                                    1, nser, 1, nser);
                real **Sw  = matrix(1, nser, 1, nser);
                real resid, gg;
                int nfw = (global_p > 1) ? global_p - 1 : 0;

                for (int j2 = 1; j2 <= r; j2++)
                    for (int i2 = 1; i2 <= s; i2++)
                        B2w[i2][j2] = global_fixb2 ? B2_fixed[i2][j2]
                                    : x[npar - s * r + (j2 - 1) * s + i2];
                for (int k2 = 1; k2 <= (global_q > 0 ? global_q : 1); k2++)
                    for (int i2 = 1; i2 <= nser; i2++)
                        for (int j2 = 1; j2 <= nser; j2++) Tw[k2][i2][j2] = 0.0;
                resid = warma_inverse(&varma1, B2w, Lw, Fw, Tw, Sw);

                fprintf(outputv,
                  "\n--- the same fit in VEC coordinates ---\n"
                  "  (I - F1 L - ...) nabla Y_t = -Lambda (B'Y_{t-1} - E[W]) "
                  "+ (I - Theta1 L - ...) A_t\n"
                  "  recovered by inverting the transformation once, not "
                  "estimated again.\n");
                fprintf(outputv, "\nLambda (M x r) =\n");
                for (int i2 = 1; i2 <= nser; i2++) {
                    fprintf(outputv, "  ");
                    for (int j2 = 1; j2 <= r; j2++)
                        fprintf(outputv, "%12.6f", Lw[i2][j2]);
                    fprintf(outputv, "\n");
                }
                for (int k2 = 1; k2 <= nfw; k2++) {
                    fprintf(outputv, "F[%d] (M x M) =\n", k2);
                    for (int i2 = 1; i2 <= nser; i2++) {
                        fprintf(outputv, "  ");
                        for (int j2 = 1; j2 <= nser; j2++)
                            fprintf(outputv, "%12.6f", Fw[k2][i2][j2]);
                        fprintf(outputv, "\n");
                    }
                }
                for (int k2 = 1; k2 <= global_q; k2++) {
                    fprintf(outputv, "Theta[%d] (M x M) =\n", k2);
                    for (int i2 = 1; i2 <= nser; i2++) {
                        fprintf(outputv, "  ");
                        for (int j2 = 1; j2 <= nser; j2++)
                            fprintf(outputv, "%12.6f", Tw[k2][i2][j2]);
                        fprintf(outputv, "\n");
                    }
                }
                fprintf(outputv, "Pi = Lambda B' (M x M) =\n");
                for (int i2 = 1; i2 <= nser; i2++) {
                    fprintf(outputv, "  ");
                    for (int j2 = 1; j2 <= nser; j2++) {
                        real acc = 0.0;
                        for (int k2 = 1; k2 <= r; k2++)
                            acc += Lw[i2][k2] * ((j2 <= r) ? (j2 == k2 ? 1.0 : 0.0)
                                                           : B2w[j2 - r][k2]);
                        fprintf(outputv, "%12.6f", acc);
                    }
                    fprintf(outputv, "\n");
                }
                gg = granger_smin(Lw, B2w, Tw, nser, r, global_q);
                if (gg >= 0.0)
                    fprintf(outputv,
                      "\nRank condition (Granger): sigma_min(Lambda_perp' "
                      "Theta(1) B_perp) = %.3e\n%s", gg,
                      (gg < global_rankadm_tol)
                        ? "  *** ZERO to working precision: this fit denies the "
                          "rank it was estimated at.\n"
                        : "  Comfortably away from zero: the fit is a model of "
                          "the rank it was estimated at.\n");
                /*  El residuo del mapa: si el punto no estuviera en la imagen
                 *  de la transformacion, esto lo diria en vez de dejar que se
                 *  publicara una Lambda inventada.                           */
                fprintf(outputv, "  inversion residual = %.3e%s\n", resid,
                        (resid > 1.0e-6)
                          ? "   *** the fitted point is NOT in the image of the "
                            "transformation; the VEC parameters above are a "
                            "least-squares projection, not the fit"
                          : "   (exact: the two coordinate systems describe the "
                            "same fit)");
                free_matrix(Sw, 1, nser, 1, nser);
                free_tensor(Tw, 1, (global_q > 0 ? global_q : 1), 1, nser, 1, nser);
                free_tensor(Fw, 1, (global_p > 1 ? global_p - 1 : 1), 1, nser, 1, nser);
                free_matrix(Lw, 1, nser, 1, r);
                free_matrix(B2w, 1, s, 1, (r > 0 ? r : 1));
            }
            /*  Se sale por la MISMA limpieza que el resto, y no por un return
             *  propio: un camino de salida nuevo es un juego nuevo de fugas, y
             *  valgrind lo encontro en cuanto se escribio (2112 bytes en 9
             *  bloques).  El resto de la impresion VEC se salta con la bandera. */
            warma_done = 1;
        }
        if (!warma_done) {

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
        /*  Con -mawarma el bloque libre son solo las r*r entradas de arriba a
         *  la izquierda; el bloque superior derecho lo determina B2 (que este
         *  recorrido aun no ha leido) y las s filas de abajo son cero.  Se
         *  guardan aqui y se imprimen despues de B2.  Este es el CUARTO
         *  recorrido del mismo vector, y es exactamente donde la version
         *  anterior de este bloque se desalineaba y publicaba una Theta que
         *  nadie habia estimado.                                             */
        real ***Th_m = tensor(1, (global_q > 0 ? global_q : 1), 1, nser, 1, nser);
        for (int k = 1; k <= (global_q > 0 ? global_q : 1); k++)
            for (int i = 1; i <= nser; i++)
                for (int j = 1; j <= nser; j++) Th_m[k][i][j] = 0.0;
        for (int k = 1; k <= global_q; k++) {
            if (global_mawarma) {
                for (int i = 1; i <= r; i++)
                    for (int j = 1; j <= r; j++) Th_m[k][i][j] = x[ii++];
            } else if (global_marow) {
                for (int i = 1; i <= r; i++)
                    for (int j = 1; j <= nser; j++) Th_m[k][i][j] = x[ii++];
            } else if (global_matri) {
                for (int i = 1; i <= r; i++)
                    for (int j = 1; j <= nser; j++) Th_m[k][i][j] = x[ii++];
                for (int i = r + 1; i <= nser; i++)
                    for (int j = r + 1; j <= nser; j++) Th_m[k][i][j] = x[ii++];
            } else if (global_diag_ma) {
                for (int i = 1; i <= nser; i++) Th_m[k][i][i] = x[ii++];
            } else {
                for (int i = 1; i <= nser; i++)
                    for (int j = 1; j <= nser; j++) Th_m[k][i][j] = x[ii++];
            }
        }
        if (!global_mawarma)
            for (int k = 1; k <= global_q; k++) {
                fprintf(outputv, "Theta[%d] (M x M)%s =\n", k,
                        global_matri ? ", block-triangular [T11 T12 ; 0 T22]"
                      : (global_marow ? ", [T11 T12 ; 0 0]" : ""));
                for (int i = 1; i <= nser; i++) {
                    fprintf(outputv, "  ");
                    for (int j = 1; j <= nser; j++)
                        fprintf(outputv, "%12.6f", Th_m[k][i][j]);
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

        /*  -mawarma: ahora que B2 esta leido, se completa e imprime Theta con
         *  la estructura que hereda: [T11  T11*B2' ; 0  0].                  */
        if (global_mawarma)
            for (int k = 1; k <= global_q; k++) {
                for (int i = 1; i <= r; i++)
                    for (int jj2 = 1; jj2 <= s; jj2++) {
                        real acc = 0.0;
                        for (int i2 = 1; i2 <= r; i2++)
                            acc += Th_m[k][i][i2] * B2m[jj2][i2];
                        Th_m[k][i][r + jj2] = acc;
                    }
                fprintf(outputv, "\nTheta[%d] (M x M), inherited structure "
                                 "[T11  T11*B2' ; 0  0] =\n", k);
                for (int i = 1; i <= nser; i++) {
                    fprintf(outputv, "  ");
                    for (int j = 1; j <= nser; j++)
                        fprintf(outputv, "%12.6f", Th_m[k][i][j]);
                    fprintf(outputv, "\n");
                }
            }
        free_tensor(Th_m, 1, (global_q > 0 ? global_q : 1), 1, nser, 1, nser);

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
        }   /* !warma_done */
        if (warma_done) free_matrix(Lam_m, 1, nser, 1, (r > 0 ? r : 1));

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
    free_seed_pre();
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