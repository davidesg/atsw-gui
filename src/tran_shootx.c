/*****************************************************************************/
/*  tran_shootx.c -- part of drtran (Box-Jenkins transfer function models).
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

#include "main.h"
#include "drtran.h"
#include "fue_pre_reader.h"

/* -------------------------------------------------------------------------- */
/* compute_irf: pesos nu de un filtro racional omega(B)/delta(B) con retardo b */
/* nu[t] = omega[t-1-b] + sum_{j=1..r} delta[j]*nu[t-j]                        */
/* nu[j] pesa a la entrada en el retardo j-1.                                  */
/* -------------------------------------------------------------------------- */
void compute_irf(real *omega, int s, real *delta, int r, int b,
                 real *nu, int length)
{
    int t, j;

    for (t = 1; t <= length; t++) nu[t] = 0.0;

    for (t = 1; t <= length; t++) {
        real sum = 0.0;
        int  lag = t - 1 - b;
        if (lag >= 0 && lag <= s) sum = omega[lag];
        for (j = 1; j <= r; j++)
            if (t > j) sum += delta[j] * nu[t - j];
        nu[t] = sum;
    }
}

/* -------------------------------------------------------------------------- */
/* Estabilidad del denominador δ(B) de cada variable determinista.            */
/* El filtro 1/δ(B) es recursivo: con raíces dentro del círculo unidad la      */
/* contribución determinista explota. Se comprueba con chekma, que usa la      */
/* misma convención de polinomio (1 - δ₁B - δ₂B² - …).                        */
/* -------------------------------------------------------------------------- */
static int unstable_delta(struct Tusmodel *Tmi)
{
    int i, k;
    real wr[10], wi[10], wmod[10];

    for (i = 1; i <= Tmi->NdetVar; i++) {
        int nd = Tmi->Ndelta[i];
        int ifault_chk = 0;
        real ***t1;

        if (nd <= 0) continue;

        t1 = tensor(0, nd, 1, 1, 1, 1);
        t1[0][1][1] = 1.0;
        for (k = 1; k <= nd; k++) t1[k][1][1] = Tmi->Delta[i][k];

        chekma(1, nd, t1, wr, wi, wmod, &ifault_chk);
        free_tensor(t1, 0, nd, 1, 1, 1, 1);

        if (ifault_chk != 0) return 1;
    }
    return 0;
}


/* -------------------------------------------------------------------------- */
/* Polinomios en forma LLANA: a(B) = a[0] + a[1]B + ... (a[0] = 1 en los ARMA).*/
/* Los ARMA se guardan en la convencion (1 - phi_1 B - ...), asi que al pasar a */
/* forma llana hay que cambiar el signo.                                       */
/* -------------------------------------------------------------------------- */
#define MAX_POLY 80

static void poly_one(real *a, int *deg)
{
    a[0] = 1.0; *deg = 0;
}

/* c = a * b */
static void poly_mul(real *a, int da, real *b, int db, real *c, int *dc)
{
    int i, j;
    real t[MAX_POLY + 1];

    for (i = 0; i <= da + db; i++) t[i] = 0.0;
    for (i = 0; i <= da; i++)
        for (j = 0; j <= db; j++) t[i + j] += a[i] * b[j];
    for (i = 0; i <= da + db; i++) c[i] = t[i];
    *dc = da + db;
}

/* -------------------------------------------------------------------------- */
/* EL CAST EMPOTRADO: la transferencia DENTRO del VARMA.                       */
/*                                                                            */
/* El cast por resta construye el ruido fuera de elf:                          */
/*     N_t = w_Y,t - SUM_k nu_k w_X,{t-k}                                      */
/* y para t = 1 necesita w_X en instantes que NO EXISTEN. Los pone a cero. Eso  */
/* contamina el arranque de la muestra, y elf no puede arreglarlo porque nunca  */
/* ve esas X: recibe el ruido ya contaminado.                                   */
/*                                                                            */
/* La alternativa es no restar nada y escribir la transferencia como           */
/* coeficientes FUERA DE LA DIAGONAL del VARMA. De                             */
/*                                                                            */
/*     delta(B) w_i = omega(B) B^b w_j + delta(B) N_i,   phi_i(B) N_i = theta_i(B) a_i */
/*                                                                            */
/* se obtiene, multiplicando por phi_i(B):                                     */
/*                                                                            */
/*     [phi_i·delta](B) w_i  -  [phi_i·omega·B^b](B) w_j  =  [delta·theta_i](B) a_i */
/*                                                                            */
/* que es un VARMA restringido. Con varios enlaces entrando en i hay que        */
/* multiplicar por TODOS sus denominadores: D_i = PROD_k delta_k, y la fila es  */
/*                                                                            */
/*     [phi_i·D_i] w_i - SUM_k [phi_i·omega_k·B^bk·(D_i/delta_k)] w_inp(k)      */
/*                                                    = [D_i·theta_i] a_i       */
/*                                                                            */
/* A elf se le entregan las series TAL CUAL, sin restar nada, y su verosimilitud*/
/* EXACTA se encarga de la inicializacion pre-muestral. No hay truncamiento ni  */
/* hay que retropredecir nada: el problema no se resuelve, DESAPARECE.          */
/*                                                                            */
/* Es lo que hacen los m6 de Mauricio, cuyo shootx escribe theta1[1][3][4]=-x5, */
/* theta1[2][3][4]=x5*x6 -- fuera de la diagonal y con PRODUCTOS de parametros. */
/*                                                                            */
/* Precio: los ordenes crecen (grado de phi_i·D_i, etc.) y Phi(0) deja de ser I */
/* cuando hay una transferencia contemporanea (b=0), asi que hay que NORMALIZAR */
/* premultiplicando por Phi(0)^-1 -- lo que convierte la Q estructural diagonal  */
/* en una Q reducida NO diagonal. Eso no es un defecto: es la identidad del SVAR,*/
/* y es exactamente la equivalencia observacional entre omega_0 y sigma_12.      */
/* -------------------------------------------------------------------------- */
static void build_embedded_varma(struct Tvarma *armax, int m,
                                 real omega[][MAX_S + 1], real delta[][MAX_R + 1],
                                 real *var, real cov[][MAX_SER + 1],
                                 int firstx, int *ifaultx)
{
    real  P[MAX_SER + 1][MAX_SER + 1][MAX_POLY + 1];   /* fila AR, forma llana */
    int   dP[MAX_SER + 1][MAX_SER + 1];
    real  M[MAX_SER + 1][MAX_POLY + 1];                /* fila MA (diagonal)   */
    int   dM[MAX_SER + 1];
    real  Di[MAX_POLY + 1], tmp[MAX_POLY + 1], acc[MAX_POLY + 1];
    int   dDi, dtmp, dacc;
    int   i, j, k, l, p = 1, q = 0;

    /* --- 1. Cada fila --- */
    for (i = 1; i <= m; i++) {
        real A[MAX_POLY + 1], T[MAX_POLY + 1];
        int  dA, dT;

        for (j = 1; j <= m; j++) { P[i][j][0] = 0.0; dP[i][j] = 0; }

        /* phi_i y theta_i en forma llana */
        A[0] = 1.0; dA = p_ord[i];
        for (k = 1; k <= p_ord[i]; k++) A[k] = -phi[i][k];
        T[0] = 1.0; dT = q_ord[i];
        for (k = 1; k <= q_ord[i]; k++) T[k] = -theta[i][k];

        /* D_i = producto de los denominadores de los enlaces que entran en i */
        poly_one(Di, &dDi);
        for (k = 1; k <= n_link; k++) {
            if (lnk[k].out != i || lnk[k].s < 0 || lnk[k].r <= 0) continue;
            tmp[0] = 1.0;
            for (l = 1; l <= lnk[k].r; l++) tmp[l] = -delta[k][l];
            dtmp = lnk[k].r;
            if (dDi + dtmp > MAX_POLY) { *ifaultx = 1; return; }
            poly_mul(Di, dDi, tmp, dtmp, Di, &dDi);
        }

        /* diagonal:  phi_i * D_i */
        if (dA + dDi > MAX_POLY) { *ifaultx = 1; return; }
        poly_mul(A, dA, Di, dDi, P[i][i], &dP[i][i]);

        /* fuera de la diagonal: -phi_i * omega_k * B^bk * (D_i / delta_k) */
        for (k = 1; k <= n_link; k++) {
            int in = lnk[k].inp, b = lnk[k].b, sn = lnk[k].s, r = lnk[k].r;

            if (lnk[k].out != i || sn < 0) continue;

            /* D_i sin el delta de este enlace */
            poly_one(acc, &dacc);
            for (l = 1; l <= n_link; l++) {
                int l2;
                if (l == k || lnk[l].out != i || lnk[l].s < 0 || lnk[l].r <= 0) continue;
                tmp[0] = 1.0;
                for (l2 = 1; l2 <= lnk[l].r; l2++) tmp[l2] = -delta[l][l2];
                poly_mul(acc, dacc, tmp, lnk[l].r, acc, &dacc);
            }
            (void)r;

            /* omega_k(B) * B^b, en forma llana */
            for (l = 0; l <= b + sn; l++) tmp[l] = 0.0;
            for (l = 0; l <= sn; l++) tmp[b + l] = omega[k][l];
            dtmp = b + sn;

            if (dacc + dtmp > MAX_POLY) { *ifaultx = 1; return; }
            poly_mul(acc, dacc, tmp, dtmp, acc, &dacc);
            if (dA + dacc > MAX_POLY) { *ifaultx = 1; return; }
            poly_mul(A, dA, acc, dacc, acc, &dacc);

            for (l = 0; l <= dacc; l++)
                P[i][in][l] = (l <= dP[i][in] ? P[i][in][l] : 0.0) - acc[l];
            if (dacc > dP[i][in]) dP[i][in] = dacc;
        }

        /* MA de la fila: D_i * theta_i */
        if (dDi + dT > MAX_POLY) { *ifaultx = 1; return; }
        poly_mul(Di, dDi, T, dT, M[i], &dM[i]);

        for (j = 1; j <= m; j++) if (dP[i][j] > p) p = dP[i][j];
        if (dM[i] > q) q = dM[i];
    }

    /* p y q dependen SOLO de los ordenes (p_ord, q_ord, b, r, s), no de los
       VALORES de los parametros: son constantes a lo largo de la optimizacion.
       Asi que se aloja una sola vez, en firstx, y nunca se realoja.           */
    if (firstx) {
        armax->m = m;
        armax->n = n_stat;
        armax->p = p;
        armax->q = q;

        armax->mu    = vector(1, m);
        armax->phi   = tensor(0, p, 1, m, 1, m);
        armax->theta = tensor(0, q, 1, m, 1, m);
        armax->qq    = matrix(1, m, 1, m);
        armax->w     = matrix(1, n_stat, 1, m);
        armax->a     = matrix(1, n_stat, 1, m);
    } else {
        p = armax->p;
        q = armax->q;
    }

    for (k = 0; k <= p; k++)
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) armax->phi[k][i][j] = 0.0;
    for (k = 0; k <= q; k++)
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) armax->theta[k][i][j] = 0.0;

    /* Convencion de elf: Phi(B) = Phi_0 - SUM_{k>=1} Phi_k B^k */
    for (i = 1; i <= m; i++) {
        for (j = 1; j <= m; j++) {
            if (dP[i][j] == 0 && P[i][j][0] == 0.0 && i != j) continue;
            armax->phi[0][i][j] = P[i][j][0];
            for (k = 1; k <= dP[i][j] && k <= p; k++)
                armax->phi[k][i][j] = -P[i][j][k];
        }
        armax->theta[0][i][i] = M[i][0];
        for (k = 1; k <= dM[i] && k <= q; k++)
            armax->theta[k][i][i] = -M[i][k];
    }

    /* Q ESTRUCTURAL: diagonal, mas las covarianzas que el fichero de restricciones
       haya LIBERADO. Se me olvido cablearlas: un q[i,j] libre no llegaba aqui, o sea
       era un parametro SIN EFECTO -- direccion exactamente plana, y un error estandar
       de tres millones. */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) armax->qq[i][j] = (i == j) ? var[i] : 0.0;
    for (i = 2; i <= m; i++)
        for (j = 1; j < i; j++)
            armax->qq[i][j] = armax->qq[j][i] = cov[i][j];
    armax->sigma2 = 1.0;

    /* Las MEDIAS. Aqui w_i es la serie OBSERVADA, no el ruido:
           E[w_i] = mu_i + SUM_{k: out=i} g_k * E[w_inp(k)],   g_k = nu_k(1)
       Se calcula en orden topologico: una salida necesita la media de su entrada. */
    for (i = 1; i <= m; i++) armax->mu[i] = 0.0;
    for (l = 1; l <= m; l++) {
        i = topo[l];
        armax->mu[i] = mu[i];
        for (k = 1; k <= n_link; k++) {
            real w1 = 0.0, d1 = 1.0;
            int  kk;
            if (lnk[k].out != i || lnk[k].s < 0) continue;
            for (kk = 0; kk <= lnk[k].s; kk++) w1 += omega[k][kk];
            for (kk = 1; kk <= lnk[k].r; kk++) d1 -= delta[k][kk];
            if (fabs(d1) < 1e-10) { *ifaultx = 1; return; }
            armax->mu[i] += (w1 / d1) * armax->mu[lnk[k].inp];
        }
    }

    /* Las series, SIN RESTAR NADA: es lo que cambia todo. */
    {
        int t;
        for (t = 1; t <= n_stat; t++)
            for (i = 1; i <= m; i++) armax->w[t][i] = w[i][t];
    }
}

/* Normalizacion de Phi(0): premultiplicar por Phi(0)^-1. Con Theta(0) = I,
     phi[k]   <- Phi0^-1 phi[k]
     theta[k] <- Phi0^-1 theta[k] Phi0
     qq       <- Phi0^-1 qq Phi0^-T
   Es la misma que hace drvarma. Hace falta en cuanto hay una transferencia
   CONTEMPORANEA (b=0), que mete omega_0 en el retardo cero.                   */
/* Phi(0) del ultimo cast empotrado. Se guarda porque los DIAGNOSTICOS necesitan
   los residuos ESTRUCTURALES, no los de la forma reducida: al normalizar,
   a_reducido = Phi(0)^-1 a_estructural, luego a_estructural = Phi(0) a_reducido.
   Sin deshacer esto, la prueba de adecuacion mide la correlacion contemporanea
   que la PROPIA transferencia genera (Sigma_12 = omega_0 sigma_X^2) y la llama
   mala especificacion. */
real phi0_last[MAX_SER + 1][MAX_SER + 1];
int  phi0_is_identity = 1;

static int normalize_phi0(struct Tvarma *armax)
{
    int m = armax->m, p = armax->p, q = armax->q;
    int i, j, k, k1, need = 0;
    real **F, **Fi, **T1, **T2;
    int  *idx;
    real *v;

    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) {
            phi0_last[i][j] = armax->phi[0][i][j];
            if (fabs(armax->phi[0][i][j] - ((i == j) ? 1.0 : 0.0)) > 1e-14) need = 1;
        }
    phi0_is_identity = !need;
    if (!need) return 0;

    F  = matrix(1, m, 1, m);
    Fi = matrix(1, m, 1, m);
    T1 = matrix(1, m, 1, m);
    T2 = matrix(1, m, 1, m);
    idx = ivector(1, m);
    v   = vector(1, m);

    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) F[i][j] = armax->phi[0][i][j];

    ludcp(F, m, idx);
    for (j = 1; j <= m; j++) {
        for (i = 1; i <= m; i++) v[i] = 0.0;
        v[j] = 1.0;
        lusol(F, v, m, idx);
        for (i = 1; i <= m; i++) Fi[i][j] = v[i];
    }

    for (k = 1; k <= p; k++) {
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) {
                T1[i][j] = 0.0;
                for (k1 = 1; k1 <= m; k1++)
                    T1[i][j] += Fi[i][k1] * armax->phi[k][k1][j];
            }
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) armax->phi[k][i][j] = T1[i][j];
    }

    for (k = 1; k <= q; k++) {
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) {
                T1[i][j] = 0.0;
                for (k1 = 1; k1 <= m; k1++)
                    T1[i][j] += Fi[i][k1] * armax->theta[k][k1][j];
            }
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) {
                T2[i][j] = 0.0;
                for (k1 = 1; k1 <= m; k1++)
                    T2[i][j] += T1[i][k1] * armax->phi[0][k1][j];
            }
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) armax->theta[k][i][j] = T2[i][j];
    }

    /* qq <- Fi qq Fi' */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) {
            T1[i][j] = 0.0;
            for (k1 = 1; k1 <= m; k1++)
                T1[i][j] += Fi[i][k1] * armax->qq[k1][j];
        }
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) {
            T2[i][j] = 0.0;
            for (k1 = 1; k1 <= m; k1++)
                T2[i][j] += T1[i][k1] * Fi[j][k1];
        }
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) armax->qq[i][j] = T2[i][j];

    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) armax->phi[0][i][j] = (i == j) ? 1.0 : 0.0;

    free_vector(v, 1, m);
    free_ivector(idx, 1, m);
    free_matrix(T2, 1, m, 1, m);
    free_matrix(T1, 1, m, 1, m);
    free_matrix(Fi, 1, m, 1, m);
    free_matrix(F, 1, m, 1, m);
    return 0;
}

/* -------------------------------------------------------------------------- */
/* shootx — el CAST: vector de parámetros -> estructura VARMA                  */
/*                                                                            */
/* Modelo:   Y_t = SUM_j nu_j(B) X_j,t + N_t                                   */
/*                                                                            */
/* VARMA diagonal de m = 1 + n_inp series:                                     */
/*     w[.][1]   = w_1 - SUM_j transferencia_j     (el ruido N)                */
/*     w[.][i]   = w_i                             (la entrada i-1)            */
/*     phi/theta diagonales: cada serie con su propio ARMA                     */
/*     Q diagonal, con Q[1][1] = 1 (la escala la concentra sigma2)             */
/*                                                                            */
/* Todo el acoplamiento vive en las transferencias restadas a la serie 1.      */
/* -------------------------------------------------------------------------- */
void shootx(real *xfree, struct Tvarma *armax, int *ifaultx, int firstx, int lastx)
{
    /* El optimizador solo ve los parámetros LIBRES. Aquí se expanden a la
       estructura completa, aplicando los fijos y los COMPARTIDOS: un mismo
       grado de libertad puede aparecer en varios sitios de la estructura.  */
    real *x = expand_params(xfree);

    int m = n_ser;
    int idx = 1;
    int i, j, k, t;
    int p, q;

    real omega[MAX_LINK + 1][MAX_S + 1];
    real delta[MAX_LINK + 1][MAX_R + 1];
    real **nu = NULL;          /* nu[k][.] : pesos del enlace k              */
    real **tr = NULL;          /* tr[i][.] : transferencia que RECIBE la serie i */
    real   var[MAX_SER + 1];
    real   cov[MAX_SER + 1][MAX_SER + 1];

    *ifaultx = 0;

    /* --- 1. Parámetros de las transferencias: una por ENLACE --- */
    for (j = 1; j <= n_link; j++) {
        for (k = 0; k <= lnk[j].s; k++) omega[j][k] = x[idx++];
        for (k = 1; k <= lnk[j].r; k++) delta[j][k] = x[idx++];
    }

    /* --- 2. ARMA de cada serie (factores del .pre, no expandidos) --- */
    for (i = 1; i <= n_ser; i++) {
        if (fix_arma[i]) continue;
        unpack_ar_factors(&Tm[i], x, &idx);
        unpack_ma_factors(&Tm[i], x, &idx);
    }

    /* --- 3. Deterministas: solo los coeficientes libres según el .pre --- */
    for (i = 1; i <= n_ser; i++)
        if (!fix_det[i]) unpack_det_params(&Tm[i], x, &idx);

    /* Rechazar deterministas con denominador inestable o factores de
       frecuencia fija inválidos (c2 >= 0) */
    for (i = 1; i <= n_ser; i++) {
        if (unstable_delta(&Tm[i]) || invalid_fixfreq(&Tm[i])) {
            *ifaultx = 1;
            return;
        }
    }

    /* --- 4. Medias (cada serie según su flag) --- */
    for (i = 1; i <= n_ser; i++)
        if (!fix_mu[i]) mu[i] = x[idx++];

    /* --- 5. Covarianza Q.
             est() CONCENTRA un factor de escala sigma2 (Sigma = sigma2*Q). Esa
             descomposicion NO es unica, y Mauricio (1995, ec. 2.1) lo dice: la
             verosimilitud concentrada (su ec. 3.1) depende de Q solo a traves de
             Pi1*Pi2 = (eta'eta - lambda'lambda)^m * |Q| * |D|^(1/n), que es
             EXACTAMENTE invariante ante Q -> cQ. Dejar las m varianzas libres
             (como hacen drvarma y el legacy) deja una direccion plana y un
             hessiano SINGULAR: los errores estandar de Q que da el legacy son
             finitos solo porque salen del hessiano acumulado por BFGS, que nunca
             ve esa direccion. Aqui se normaliza Q[1][1] = 1 y se estima
             log(var_i/var_1); la escala se la queda sigma2.

             Las COVARIANZAS q[i][j] (i>j) son libres solo si el fichero de
             restricciones las libera. Si la Q resultante no es definida positiva,
             elf lo detecta y objcfunc devuelve 1.0: el punto se rechaza. Es la
             estrategia del propio articulo (seccion 3).                       */
    var[1] = 1.0;
    for (i = 2; i <= n_ser; i++) var[i] = exp(x[idx++]);
    for (i = 2; i <= n_ser; i++)
        for (j = 1; j < i; j++) cov[i][j] = x[idx++];

    /* --- 6. Expandir los factores ARMA a polinomios --- */
    for (i = 1; i <= n_ser; i++) {
        if (p_ord[i] > 0) expand_ar_factors(&Tm[i], phi[i],   p_ord[i]);
        if (q_ord[i] > 0) expand_ma_factors(&Tm[i], theta[i], q_ord[i]);
    }

    /* --- 7. Series estacionarias con los parámetros actuales --- */
    build_stationary_series();
    if (n_stat <= 0) { *ifaultx = 1; return; }

    /* --- 8a. EL CAST EMPOTRADO: la transferencia DENTRO del VARMA -----------
       Sin restar nada. elf recibe las series tal cual y su verosimilitud EXACTA
       se ocupa de la inicializacion pre-muestral: no hay truncamiento.        */
    if (embed_varma) {
        build_embedded_varma(armax, m, omega, delta, var, cov, firstx, ifaultx);

        if (*ifaultx == 0) {                       /* invertibilidad del MA */
            real wr[4 * MAX_SER], wi[4 * MAX_SER], wmod[4 * MAX_SER];
            int  ifc = 0;
            if (armax->q > 0) {
                chekma(m, armax->q, armax->theta, wr, wi, wmod, &ifc);
                if (ifc != 0) *ifaultx = 1;
            }
        }
        if (*ifaultx == 0) normalize_phi0(armax);

        /* Un punto RECHAZADO no libera nada: el optimizador volvera a llamar con
           firstx = 0 sobre esta misma estructura. Liberar aqui dejaba punteros
           colgando y reventaba en el free siguiente. Solo se libera en lastx,
           igual que en el cast por resta.                                      */
        if (lastx) {
            free_matrix(armax->a, 1, armax->n, 1, armax->m);
            free_matrix(armax->w, 1, armax->n, 1, armax->m);
            free_matrix(armax->qq, 1, armax->m, 1, armax->m);
            free_tensor(armax->theta, 0, armax->q, 1, armax->m, 1, armax->m);
            free_tensor(armax->phi, 0, armax->p, 1, armax->m, 1, armax->m);
            free_vector(armax->mu, 1, armax->m);
        }
        return;
    }

    /* --- 8. Transferencias de la RED: cada enlace resta a SU salida --- */
    tr = matrix(1, n_ser, 1, n_stat);
    for (i = 1; i <= n_ser; i++)
        for (t = 1; t <= n_stat; t++) tr[i][t] = 0.0;

    if (n_link > 0) {
        nu = matrix(1, n_link, 1, n_stat);

        for (j = 1; j <= n_link; j++) {
            int o = lnk[j].out, in = lnk[j].inp;

            compute_irf(omega[j], lnk[j].s, delta[j], lnk[j].r, lnk[j].b,
                        nu[j], n_stat);

            /* la salida o recibe: sum_k nu_j[k] * w_in[t-k+1] */
            for (t = 1; t <= n_stat; t++) {
                real acc = 0.0;
                for (k = 1; k <= t; k++)
                    acc += nu[j][k] * w[in][t - k + 1];
                tr[o][t] += acc;
            }
        }
    }

    /* --- 9. Dimensiones del VARMA --- */
    p = 1;                       /* elf exige p >= 1 */
    q = 0;
    for (i = 1; i <= n_ser; i++) {
        if (p_ord[i] > p) p = p_ord[i];
        if (q_ord[i] > q) q = q_ord[i];
    }

    armax->m = m;
    armax->n = n_stat;
    armax->p = p;
    armax->q = q;

    /* --- 10. Alojar / reinicializar --- */
    if (firstx) {
        armax->mu    = vector(1, m);
        armax->phi   = tensor(0, p, 1, m, 1, m);
        armax->theta = tensor(0, q, 1, m, 1, m);
        armax->qq    = matrix(1, m, 1, m);
        armax->w     = matrix(1, n_stat, 1, m);
        armax->a     = matrix(1, n_stat, 1, m);

        for (i = 1; i <= m; i++) {
            armax->mu[i] = 0.0;
            for (j = 1; j <= m; j++) {
                for (k = 0; k <= p; k++) armax->phi[k][i][j]   = 0.0;
                for (k = 0; k <= q; k++) armax->theta[k][i][j] = 0.0;
                armax->qq[i][j] = 0.0;
            }
            for (t = 1; t <= n_stat; t++) {
                armax->w[t][i] = 0.0;
                armax->a[t][i] = 0.0;
            }
        }
        for (i = 1; i <= m; i++) {          /* phi[0] = theta[0] = I */
            armax->phi[0][i][i]   = 1.0;
            armax->theta[0][i][i] = 1.0;
        }
    } else {
        for (k = 1; k <= p; k++)
            for (i = 1; i <= m; i++)
                for (j = 1; j <= m; j++) armax->phi[k][i][j] = 0.0;
        for (k = 1; k <= q; k++)
            for (i = 1; i <= m; i++)
                for (j = 1; j <= m; j++) armax->theta[k][i][j] = 0.0;
    }

    /* --- 11. phi y theta DIAGONALES: cada serie con su propio ARMA --- */
    for (i = 1; i <= m; i++) {
        for (k = 1; k <= p_ord[i]; k++) armax->phi[k][i][i]   = phi[i][k];
        for (k = 1; k <= q_ord[i]; k++) armax->theta[k][i][i] = theta[i][k];
    }

    /* --- 12. Restricciones --- */
    {
        real wr[4 * MAX_SER], wi[4 * MAX_SER], wmod[4 * MAX_SER];
        int ifault_chk = 0;

        for (i = 1; i <= m; i++)
            if (p_ord[i] >= 1 && fabs(phi[i][1]) >= 0.999) {
                *ifaultx = 1;
                goto cleanup;
            }

        if (q > 0) {
            chekma(m, q, armax->theta, wr, wi, wmod, &ifault_chk);
            if (ifault_chk != 0) { *ifaultx = 1; goto cleanup; }
        }
    }

    /* --- 13. Q (simetrica) y medias --- */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++)
            armax->qq[i][j] = (i == j) ? var[i] : 0.0;
    for (i = 2; i <= m; i++)
        for (j = 1; j < i; j++)
            armax->qq[i][j] = armax->qq[j][i] = cov[i][j];
    armax->sigma2 = 1.0;        /* la escala la concentra est() */

    for (i = 1; i <= m; i++) armax->mu[i] = mu[i];

    /* --- 14. Cada serie, menos lo que recibe por la red: es su RUIDO --- */
    for (t = 1; t <= n_stat; t++)
        for (i = 1; i <= m; i++)
            armax->w[t][i] = w[i][t] - tr[i][t];

cleanup:
    if (nu) free_matrix(nu, 1, n_link, 1, n_stat);
    free_matrix(tr, 1, n_ser, 1, n_stat);

    if (lastx) {
        free_matrix(armax->a, 1, armax->n, 1, armax->m);
        free_matrix(armax->w, 1, armax->n, 1, armax->m);
        free_matrix(armax->qq, 1, armax->m, 1, armax->m);
        free_tensor(armax->theta, 0, armax->q, 1, armax->m, 1, armax->m);
        free_tensor(armax->phi, 0, armax->p, 1, armax->m, 1, armax->m);
        free_vector(armax->mu, 1, armax->m);
    }
}
