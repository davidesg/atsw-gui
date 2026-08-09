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
        /* numerador omega(B) = omega_0 - omega_1 B - ... (Box-Jenkins, como fue
           calcnu): el termino lider suma, los demas RESTAN. */
        if (lag >= 0 && lag <= s) sum = (lag == 0) ? omega[0] : -omega[lag];
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

            /* omega_k(B) * B^b, en forma llana.  omega(B) = omega_0 - omega_1 B - ...
               (Box-Jenkins, como fue calcnu): el lider suma, los demas RESTAN. */
            for (l = 0; l <= b + sn; l++) tmp[l] = 0.0;
            for (l = 0; l <= sn; l++)
                tmp[b + l] = (l == 0) ? omega[k][0] : -omega[k][l];
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

    /* Se guarda la Q ESTRUCTURAL: normalize_phi0 va a pisar armax->qq con la
       REDUCIDA, y la descomposicion de la varianza necesita la estructural --
       que es la unica en la que las innovaciones son ortogonales y por tanto la
       unica en la que la descomposicion es UNICA.                              */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) qq_struct[i][j] = armax->qq[i][j];

    /* Las MEDIAS.  mu_i es LA MEDIA de la serie i, no un intercepto: es lo que
       fue estima y escribe en el .pre, y la coherencia con fue exige mantener esa
       lectura.  Box-Jenkins escribe el modelo en DESVIACIONES de la media,

           (w_Y - mu_Y) = nu(B) (w_X - mu_X) + N_t   =>   E[w_Y] = mu_Y,

       asi que la media de la salida NO hereda nada de la entrada -- es lo que
       hace que las transferencias salgan limpias.  Multiplicando por delta(B),

           phi_Y delta (w_Y - mu_Y) - phi_Y omega B^b (w_X - mu_X) = delta theta_Y a_Y

       que es EXACTAMENTE la fila 1 de Phi(B)(w - mu) = Theta(B) a con
       mu = (mu_Y, mu_X).  No hay termino que anadir.

       Antes se hacia mu_i += (SUM_k omega_k / delta(1)) * mu_inp, en orden
       topologico, que corresponde a la parametrizacion con INTERCEPTO
       (w_Y = c + nu(B) w_X + N).  Las dos son la misma familia reparametrizada
       MIENTRAS mu_Y sea libre -- comprobado: con la media de la salida libre dan
       el mismo optimo a 1e-12.  Divergen cuando mu_Y esta FIJADA: en desviaciones
       mu_Y = 0 significa E[w_Y] = 0; con intercepto significa E[w_Y] = nu(1) mu_X.
       Si fue fijo la media en cero es porque la serie no tiene deriva.

       No se veia en el caso canonico porque su entrada (WTI) tiene mu = 0, y
       entonces el termino vale cero con cualquier convencion.                  */
    for (i = 1; i <= m; i++) armax->mu[i] = mu[i];

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
real phi0_inv[MAX_SER + 1][MAX_SER + 1];
real qq_struct[MAX_SER + 1][MAX_SER + 1];   /* la Q ESTRUCTURAL (diagonal)     */
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
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) phi0_inv[i][j] = Fi[i][j];

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

            /* La salida o recibe: sum_k nu_j[k] * x[t-k+1].

               x NO es la columna propia de la entrada cuando los dos operadores
               difieren: es la entrada diferenciada por el operador de la SALIDA
               (BUG-8). w_alt[j] la trae ya calculada y recortada a n_stat, y es
               NULL cuando coinciden -- todo el legacy pasa por la rama de la
               derecha y no se mueve ni un bit.                                */
            real *xin = w_alt[j] ? w_alt[j] : w[in];
            real *pre;
            int   P;

            /* LA MUESTRA PREVIA. La convolucion quiere la entrada antes de
               t=1 y no existe; ponerla a cero es lo que hacia que este cast
               calculase "la verosimilitud exacta de la serie EQUIVOCADA".
               pre[1] es el valor inmediatamente anterior a xin[1].          */
            P = build_pre_sample(j, nu[j], n_stat, &pre);

            for (t = 1; t <= n_stat; t++) {
                real acc = 0.0;
                for (k = 1; k <= t; k++)
                    acc += nu[j][k] * xin[t - k + 1];
                /* Lo que cae antes de t=1. nu[j] esta alojado 1..n_stat, asi
                   que el indice hay que ACOTARLO: sin la cota se lee fuera del
                   vector para t grande. No se pierde nada, porque nu[k] ya es
                   cero para k-1 > K.                                        */
                for (k = t + 1; k <= t + P && k <= n_stat; k++)
                    acc += nu[j][k] * pre[k - t];
                tr[o][t] += acc;
            }
            if (P > 0) free_vector(pre, 1, P);
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
        /* AR: la estacionariedad se lee de las RAICES, no de phi[1].
           El test anterior era |phi[i][1]| >= 0.999 para TODO orden, y phi[1]
           solo es el reciproco de una raiz cuando p_ord[i] == 1.  En un AR(2)
           la region estacionaria es el triangulo |phi2|<1, phi2+phi1<1,
           phi2-phi1<1, en el que phi1 llega a 2: todo AR(2) de raices
           complejas con phi1 > 1 es estacionario y se rechazaba sin llegar a
           evaluar la verosimilitud.  Y no es un rincon exotico -- es donde
           viven los ciclos persistentes.  Peor que el ifault: partiendo por
           debajo de la barrera el optimizador no falla, sube hasta 0.998998 y
           se clava, con error tipico 1e-06 y t = 1.04e+06.
           chekma hace exactamente esta comprobacion (matriz companera y modulo
           de los autovalores) y es generica en el operador, que es por lo que
           ya se usa aqui abajo para el MA y para la estabilidad de delta(B).

           Los vectores de trabajo pasan a ser dinamicos: chekma indexa 1..m*p
           (o 1..m*q), y wr[4*MAX_SER] = wr[32] se queda corto en cuanto
           m*max(p,q) > 32 -- alcanzable con un modelo estacional (p o q ~ 13)
           y pocas series.  El limite ya existia para la llamada del MA. */
        int   nchk = m * (p > q ? p : q);
        real *wr, *wi, *wmod;
        int   ifault_chk = 0, rechaza = 0;

        if (nchk < 1) nchk = 1;
        wr   = vector(1, nchk);
        wi   = vector(1, nchk);
        wmod = vector(1, nchk);

        if (p > 0) {
            chekma(m, p, armax->phi, wr, wi, wmod, &ifault_chk);
            if (ifault_chk != 0) rechaza = 1;
        }

        if (!rechaza && q > 0) {
            chekma(m, q, armax->theta, wr, wi, wmod, &ifault_chk);
            if (ifault_chk != 0) rechaza = 1;
        }

        free_vector(wr,   1, nchk);
        free_vector(wi,   1, nchk);
        free_vector(wmod, 1, nchk);

        if (rechaza) { *ifaultx = 1; goto cleanup; }
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
