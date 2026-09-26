/*****************************************************************************/
/*  drtran.h -- part of drtran (Box-Jenkins transfer function models).
 *
 *  Original to drtran.
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

/*****************************************************************************/
/*  DRTRAN.H                                                                 */
/*  Cabecera específica del programa de estimación de funciones de           */
/*  transferencia Box‑Jenkins (DRTRAN).                                      */
/*  Contiene las definiciones y variables globales compartidas entre         */
/*  drtran.c, tran_shootx.c y prewhiten.c.                                */
/*****************************************************************************/
#ifndef DRTRAN_H
#define DRTRAN_H

#include "main.h"
#include "fue_pre_reader.h"   /* el modelo del .pre y sus coeficientes libres */

/* -------------------------------------------------------------------------- */
/* Órdenes máximos permitidos para la función de transferencia               */
/* (se pueden modificar si se necesitan modelos más largos)                   */
/* -------------------------------------------------------------------------- */
#define MAX_R  10
#define MAX_S  10
#define MAX_LAG_WEIGHTS 5000   /* longitud máxima para los pesos nu[t] */

/* -------------------------------------------------------------------------- */
/* Número máximo de series: 1 salida + hasta MAX_INP entradas                 */
/* -------------------------------------------------------------------------- */
#define MAX_SER 8
#define MAX_INP (MAX_SER - 1)

/* -------------------------------------------------------------------------- */
/* Variables globales compartidas                                             */
/*                                                                            */
/* El modelo es multivariante de transferencia:                               */
/*                                                                            */
/*     Y_t = SUM_j  omega_j(B)/delta_j(B) * B^b_j * X_j,t  +  N_t             */
/*                                                                            */
/* y se castea a un VARMA de m = 1 + n_inp series, DIAGONAL:                  */
/*                                                                            */
/*     serie 1      = w_1 - SUM_j transferencia_j   (el ruido N)              */
/*     serie j+1    = w_{j+1}                       (la entrada j)            */
/*                                                                            */
/* Índice 1 = SALIDA (Y); índices 2..n_ser = ENTRADAS. La entrada j (j=1..    */
/* n_inp) es la serie j+1.                                                    */
/* -------------------------------------------------------------------------- */

extern int n_ser;    /* número de series = 1 + n_inp */
extern int n_inp;    /* número de entradas exógenas  */

/* Modelos univariantes leídos de los .pre de fue, y sus series */
extern struct Tusmodel Tm[MAX_SER + 1];
extern struct Tseries  Ts[MAX_SER + 1];
extern real **DataMat[MAX_SER + 1];

/* Series estacionarias (tras Box-Cox, deterministas y diferenciación),
   recortadas todas a la ventana común */
extern real *w[MAX_SER + 1];
extern int   n_stat;


/* ARMA de cada serie: órdenes EXPANDIDOS y polinomios */
extern int   p_ord[MAX_SER + 1], q_ord[MAX_SER + 1];
extern real *phi[MAX_SER + 1], *theta[MAX_SER + 1];

/* Media de cada serie, y si es libre o fija (según el flag del .pre) */
extern real mu[MAX_SER + 1];
extern int  fix_mu[MAX_SER + 1];

/* Banderas: 1 = mantener fijo (por serie) */
extern int fix_arma[MAX_SER + 1];   /* parámetros ARMA */
extern int fix_det[MAX_SER + 1];    /* coeficientes deterministas */

/* Función de transferencia de la entrada j (j = 1..n_inp), en el caso simple
   de UNA salida (los flags -b/-r/-s de la línea de órdenes):
     b_del[j] : retardo puro B^b
     r_ord[j] : orden del denominador delta(B)
     s_ord[j] : orden del numerador omega(B)   (-1 = sin transferencia) */
extern int b_del[MAX_SER + 1], r_ord[MAX_SER + 1], s_ord[MAX_SER + 1];

/* El cast: 0 = restar la transferencia; 1 = EMPOTRARLA en el VARMA (-V). */
extern int embed_varma;

/* Phi(0) del cast empotrado, para des-normalizar los residuos en la diagnosis. */
extern real phi0_last[MAX_SER + 1][MAX_SER + 1];
extern real phi0_inv[MAX_SER + 1][MAX_SER + 1];
extern real qq_struct[MAX_SER + 1][MAX_SER + 1];
extern int  phi0_is_identity;

/* -------------------------------------------------------------------------- */
/* LA RED de transferencias                                                    */
/*                                                                            */
/* El modelo general no es una salida y k entradas, sino un DAG: cualquier     */
/* serie puede recibir transferencias Y ser a la vez entrada de otra. En el    */
/* sistema laboral de Mauricio (m6), EU es SALIDA de EC y ENTRADA de EI:       */
/*                                                                            */
/*     EC ── b=2 ──► EU ── b=1 ──► EI ── b=1 ──► EP                            */
/*                                                                            */
/* Cada ENLACE es una transferencia nu(B) = omega(B)/delta(B)·B^b de una serie */
/* de entrada a una de salida. El caso de una sola salida es el particular en  */
/* que todos los enlaces apuntan a la serie 1.                                 */
/* -------------------------------------------------------------------------- */
#define MAX_LINK 32

struct Tlink {
    int out;    /* serie que RECIBE la transferencia */
    int inp;    /* serie que la EMITE                */
    int b, r, s;
};

extern struct Tlink lnk[MAX_LINK + 1];
extern int n_link;
/* BUG-8: la entrada de cada enlace diferenciada por el operador de SU SALIDA.
   NULL cuando los dos operadores coinciden. Ver build_stationary_series. */
extern real *w_alt[MAX_LINK + 1];
int operators_differ(int out, int inp);
int links_need_subtracting(void);
extern real *w_head[MAX_SER + 1];
extern int   n_head[MAX_SER + 1];
extern real *alt_delta[MAX_LINK + 1];
extern int   n_alt_delta[MAX_LINK + 1];
int build_pre_sample(int j, const real *nu, int nlen, real **pre);

/* Orden topológico de las series (las entradas antes que sus salidas). */
extern int topo[MAX_SER + 1];

/* Indicador de matriz de covarianza diagonal (1 = sí, 0 = completa) */
extern int diag_cov;

/* Expande el vector de parámetros LIBRES a la estructura completa, aplicando
   los coeficientes fijos y los COMPARTIDOS. */
real *expand_params(real *xfree);
void compute_irf(real *omega, int s, real *delta, int r, int b,
                 real *nu, int length);

/* Construye las series estacionarias de TODAS las series y las recorta a la
   ventana común (cada modelo puede diferenciar distinto: ∇∇₁₂ pierde 13
   observaciones y ∇ solo 1). Fija n_stat. */
void build_stationary_series(void);

/* Unscramble: expandir factores AR/MA de FUE a polinomios VARMA */
void expand_ar_factors(struct Tusmodel *Tm, real *phi_out, int p);
void expand_ma_factors(struct Tusmodel *Tm, real *theta_out, int q);

#endif /* DRTRAN_H */
