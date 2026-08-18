/*****************************************************************************/
/*  fue_bridge.c -- part of drvec (VEC estimation, Mauricio 2006).
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

/*  Puente hacia la suite: lo que hace falta para consumir un .pre de fue y que
 *  el motor de drvec no aporta.  CUATRO funciones, las cuatro copiadas VERBATIM
 *  de drtran (2026-08-17), que es la implementacion en C del mismo puente:
 *  drtran usa el cast de fue y el motor de drvarma, que es exactamente lo que
 *  hace drvec.
 *
 *  PROCEDENCIA
 *
 *    ObsToDate          drtran/src/diagnose.c:128
 *    DateToObs          drtran/src/drtran.c:838
 *    expand_ar_factors  drtran/src/drtran.c:712
 *    expand_ma_factors  drtran/src/drtran.c:779
 *
 *  Unico cambio: DRTRAN_PI -> M_PI (que main.h ya define).
 *
 *  Las dos de fecha ya estaban DECLARADAS en include/main.h de drvec pero
 *  ningun .c las definia; son la unica dependencia de fue_pre_reader.c que el
 *  motor no cubria.  Solo las usa gen_detvar() para deterministas FECHADAS
 *  (impulse, step, ramp, easter): los .inp que drvec escribe llevan cero
 *  deterministas, asi que hoy la ruta no se ejercita.  Se traen completas y no
 *  como stub porque un stub daria fechas equivocadas EN SILENCIO si alguien
 *  alimenta un .pre que si las lleve.
 *
 *  Las dos de expansion son el espejo en C del _unscramble del cast de fue:
 *  convierten los operadores FACTORIZADOS del .pre en los polinomios planos
 *  Phi(B) = 1 - phi_1 B - ... y Theta(B) = 1 - theta_1 B - ..., que es la
 *  convencion con la que drvec imprime su modelo y la que espera elf().  El
 *  .pre guarda factores, no polinomios: "2 1 1" son DOS factores de primer
 *  orden y "1 2" es UNO de segundo, y la diferencia cambia el modelo
 *  (FILE_CONTRACT.md 2.3).  Expandir aqui, y no reimplementar la convolucion,
 *  es lo que garantiza que drvec lea el mismo modelo que fue estimo.
 *
 *  Ver docs/PLAN_BETA.md F2.1 (procedencia) y F2.3 (el cast).
 */

#include "main.h"

/*---------------------------------------------------------------------------*/
/*  ObsToDate: convierte numero de observacion a (periodo, subperiodo)       */
/*---------------------------------------------------------------------------*/
void ObsToDate(int beg_per, int beg_sub, int obs_no, int freq,
               int *per, int *sub)
{
    div_t cad;
    if (obs_no + beg_sub - 1 <= freq) {
        *per = beg_per;
        *sub = beg_sub + obs_no - 1;
    } else {
        cad = div(obs_no - (freq - beg_sub + 1), freq);
        if (cad.rem > 0) {
            *per = beg_per + cad.quot + 1;
            *sub = cad.rem;
        } else {
            *per = beg_per + cad.quot;
            *sub = freq;
        }
    }
}

/*---------------------------------------------------------------------------*/
/*  DateToObs: convierte (periodo, subperiodo) a numero de observacion       */
/*---------------------------------------------------------------------------*/
void DateToObs(int beg_per, int beg_sub, int per, int sub, int freq,
               int *obs_no)
{
    int srest, pcad, sad;

    srest = freq - beg_sub + 1;
    if (sub == freq) {
        pcad = per - beg_per;
        *obs_no = srest + freq * pcad;
    } else {
        pcad = per - beg_per - 1;
        sad  = sub;
        *obs_no = srest + freq * pcad + sad;
    }
}

/* Expande todos los factores AR en un único polinomio Φ(B)=1-φ₁B-φ₂B²-...
   phi_out[1..p] recibe los coeficientes (phi_out[0] = 1 implícito).
   Se asume que phi_out está pre-dimensionado con vector(1, p).            */
void expand_ar_factors(struct Tusmodel *Tm, real *phi_out, int p)
{
    int i, j, k;
    real *work;

    if (p == 0) return;

    work = vector(0, p);

    /* Inicializar: polinomio identidad 1 (coefs en work[1..p] = 0) */
    work[0] = 1.0;
    for (k = 1; k <= p; k++) work[k] = 0.0;

    /* Factores AR regulares: P_i(B) = -1 + a₁B + a₂B² + ...
       Convertido a VARMA:  -P_i(B) = 1 - a₁B - a₂B² - ...            */
    for (i = 1; i <= Tm->NumAr1; i++) {
        int ord_i = Tm->p1[i];
        for (k = p; k >= 0; k--) {
            real acc = work[k];
            for (j = 1; j <= ord_i && j <= k; j++)
                acc -= Tm->Ar1[i][j] * work[k - j];
            work[k] = acc;
        }
    }

    /* Factores AR ANUALES: 1 - a₁B^s - a₂B^2s - … (retardos múltiplos de sper) */
    for (i = 1; i <= Tm->NumAr2; i++) {
        int ord_i = Tm->p2[i];
        for (k = p; k >= 0; k--) {
            real acc = work[k];
            for (j = 1; j <= ord_i; j++) {
                int lag = j * Tm->sper;
                if (lag <= k) acc -= Tm->Ar2[i][j] * work[k - lag];
            }
            work[k] = acc;
        }
    }

    /* Factores AR de FRECUENCIA FIJA (irreducibles). fue los parametriza con un
       ÚNICO coeficiente libre c₂ (<0); el término en B se DERIVA de él y de la
       frecuencia (fue.c:4064):
           c₁ = 2·cos(2πf/s)·sqrt(−c₂)     →   1 − c₁B − c₂B²
       que es (1 − 2r·cos(ω)B + r²B²) con r = sqrt(−c₂).                     */
    for (i = 1; i <= Tm->NumAr1f; i++) {
        real c2 = Tm->Ar1f[i][2];
        real r  = (c2 < 0.0) ? sqrt(-c2) : 0.0;
        real c1 = 2.0 * cos(2.0 * M_PI * Tm->pfre1[i] / Tm->sper) * r;

        Tm->Ar1f[i][1] = c1;   /* fue guarda el término en B junto al factor */

        for (k = p; k >= 0; k--) {
            real acc = work[k];
            if (k >= 1) acc -= c1 * work[k - 1];
            if (k >= 2) acc -= c2 * work[k - 2];
            work[k] = acc;
        }
    }

    /* Copiar a phi_out[1..p] (negar porque work almacena P(B),
       pero VARMA usa Φ(B)=1-φ₁B-φ₂B²-... = 1 - work[1]B - work[2]B²-...) */
    for (k = 1; k <= p; k++) phi_out[k] = -work[k];

    free_vector(work, 0, p);
}

/* Expande todos los factores MA en un único polinomio Θ(B)=1-θ₁B-θ₂B²-...
   Misma lógica que expand_ar_factors.                                   */
void expand_ma_factors(struct Tusmodel *Tm, real *theta_out, int q)
{
    int i, j, k;
    real *work;

    if (q == 0) return;

    work = vector(0, q);
    work[0] = 1.0;
    for (k = 1; k <= q; k++) work[k] = 0.0;

    for (i = 1; i <= Tm->NumMa1; i++) {
        int ord_i = Tm->q1[i];
        for (k = q; k >= 0; k--) {
            real acc = work[k];
            for (j = 1; j <= ord_i && j <= k; j++)
                acc -= Tm->Ma1[i][j] * work[k - j];
            work[k] = acc;
        }
    }

    /* Factores MA ANUALES: 1 - θ₁B^s - θ₂B^2s - … (retardos múltiplos de sper) */
    for (i = 1; i <= Tm->NumMa2; i++) {
        int ord_i = Tm->q2[i];
        for (k = q; k >= 0; k--) {
            real acc = work[k];
            for (j = 1; j <= ord_i; j++) {
                int lag = j * Tm->sper;
                if (lag <= k) acc -= Tm->Ma2[i][j] * work[k - lag];
            }
            work[k] = acc;
        }
    }

    /* Factores MA de FRECUENCIA FIJA: igual que los AR (fue.c:4202) */
    for (i = 1; i <= Tm->NumMa1f; i++) {
        real c2 = Tm->Ma1f[i][2];
        real r  = (c2 < 0.0) ? sqrt(-c2) : 0.0;
        real c1 = 2.0 * cos(2.0 * M_PI * Tm->qfre1[i] / Tm->sper) * r;

        Tm->Ma1f[i][1] = c1;

        for (k = q; k >= 0; k--) {
            real acc = work[k];
            if (k >= 1) acc -= c1 * work[k - 1];
            if (k >= 2) acc -= c2 * work[k - 2];
            work[k] = acc;
        }
    }

    /* Copiar a theta_out[1..q] (misma convención de signos que AR) */
    for (k = 1; k <= q; k++) theta_out[k] = -work[k];

    free_vector(work, 0, q);
}
