/*****************************************************************************/
/*  DRTRAN.H                                                                 */
/*  Cabecera específica del programa de estimación de funciones de           */
/*  transferencia Box‑Jenkins (DRTRAN).                                      */
/*  Contiene las definiciones y variables globales compartidas entre         */
/*  drtran.c, tran_shootx.c, prewhiten.c y fue_pre_reader.c.                 */
/*****************************************************************************/
#ifndef DRTRAN_H
#define DRTRAN_H

#include "main.h"

/* -------------------------------------------------------------------------- */
/* Órdenes máximos permitidos para la función de transferencia               */
/* (se pueden modificar si se necesitan modelos más largos)                   */
/* -------------------------------------------------------------------------- */
#define MAX_R  10
#define MAX_S  10
#define MAX_LAG_WEIGHTS 5000   /* longitud máxima para los pesos nu[t] */

/* -------------------------------------------------------------------------- */
/* Variables globales compartidas                                             */
/* -------------------------------------------------------------------------- */

/* Modelos univariantes leídos de los archivos .pre (FUE) */
extern struct Tusmodel TmX, TmY;

/* Series temporales originales (metadatos y datos) */
extern struct Tseries TsX, TsY;

/* Matrices de datos: fila 0 = serie transformada (Box‑Cox),
   filas 1..NdetVar = variables deterministas */
extern real **DataMatX, **DataMatY;

/* Longitud de las series después de eliminar deterministas y diferenciar */
extern int n_stat;

/* Series estacionarias w_X (entrada) y w_Y (salida) */
extern real *w_X, *w_Y;

/* Órdenes de la función de transferencia:
   r_ord : orden del denominador δ(B)
   s_ord : orden del numerador ω(B)
   b_delay : retardo puro B^b                              */
extern int r_ord, s_ord, b_delay;

/* Banderas de estimación:
   fix_X    : 1 = mantener fijos los parámetros ARMA de X
   fix_noise: 1 = mantener fijos los parámetros ARMA del ruido N_t
   fix_det_X: 1 = mantener fijos los coefs deterministas de X
   fix_det_Y: 1 = mantener fijos los coefs deterministas de Y   */
extern int fix_X, fix_noise;
extern int fix_det_X, fix_det_Y;

/* Medias: se fijan o liberan POR SERIE, según el flag de estimación que trae
   el .pre de FUE (Tm->Imu). Si la media está fija, conserva el valor del .pre
   (típicamente 0); si es libre, entra en x[] como un parámetro más.        */
extern int fix_mu_Y, fix_mu_X;
extern real mu_Y, mu_X;

/* Órdenes ARMA del ruido (N_t, asociado a Y) y de la entrada (X) */
extern int p_N, q_N, p_X, q_X;

/* Coeficientes ARMA (se leen de FUE y pueden ser fijos o estimables) */
extern real *phi_N, *theta_N;   /* ruido N_t */
extern real *phi_X, *theta_X;   /* entrada X_t */

/* Indicador de matriz de covarianza diagonal (1 = sí, 0 = completa) */
extern int diag_cov;

/* Deterministas: desempaqueta desde x[] los coeficientes ω/δ marcados como
   estimables en el .pre (Imega/Ielta) */
void unpack_det_params(struct Tusmodel *Tm, real *x, int *idx);

/* Rechaza factores de frecuencia fija invalidos (c2 >= 0) */
int invalid_fixfreq(struct Tusmodel *Tm);

/* Construye w_X y w_Y y las recorta a la ventana común (los dos modelos pueden
   diferenciar distinto: ∇∇₁₂ pierde 13 observaciones y ∇ solo 1). Fija n_stat. */
void build_stationary_pair(void);

/* Unscramble: expandir factores AR/MA de FUE a polinomios VARMA */
void expand_ar_factors(struct Tusmodel *Tm, real *phi_out, int p);
void expand_ma_factors(struct Tusmodel *Tm, real *theta_out, int q);
void unpack_ar_factors(struct Tusmodel *Tm, real *x, int *idx);
void unpack_ma_factors(struct Tusmodel *Tm, real *x, int *idx);

#endif /* DRTRAN_H */
