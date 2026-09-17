/*
 * prewhiten.c -- ver prewhiten.h.
 *
 * apply_univariate_model viene de engines/drtran/src/drtran.c TAL CUAL: no se
 * ha tocado una linea. prewhiten_ccf son los pasos 1-4 de
 * prewhiten_and_identify, que estaban atados a los globales del motor (lnk, w,
 * phi, theta, p_ord, q_ord, n_stat, Ts) y aqui reciben sus argumentos.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "prewhiten.h"

/* Venia de drtran.c, donde lo usaban los factores AR/MA de frecuencia fija: un
 * factor de frecuencia f es 1 - 2cos(2*pi/f) B + B^2.                     */
#define DRTRAN_PI 3.14159265358979323846

/* Calcula el orden AR total (suma de órdenes de todos los factores) */
int total_ar_order(struct Tusmodel *Tm)
{
    int ord = 0, i;
    for (i = 1; i <= Tm->NumAr1;  i++) ord += Tm->p1[i];
    /* Un factor ANUAL de orden p actúa en los retardos sper, 2·sper, …, p·sper */
    for (i = 1; i <= Tm->NumAr2;  i++) ord += Tm->p2[i] * Tm->sper;
    for (i = 1; i <= Tm->NumAr1f; i++) ord += 2;   /* cada factor fijo es orden 2 */
    return ord;
}

/* Calcula el orden MA total */
int total_ma_order(struct Tusmodel *Tm)
{
    int ord = 0, i;
    for (i = 1; i <= Tm->NumMa1;  i++) ord += Tm->q1[i];
    for (i = 1; i <= Tm->NumMa2;  i++) ord += Tm->q2[i] * Tm->sper;
    for (i = 1; i <= Tm->NumMa1f; i++) ord += 2;
    return ord;
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
        real c1 = 2.0 * cos(2.0 * DRTRAN_PI * Tm->pfre1[i] / Tm->sper) * r;

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
        real c1 = 2.0 * cos(2.0 * DRTRAN_PI * Tm->qfre1[i] / Tm->sper) * r;

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

/* -------------------------------------------------------------------------- */
/* apply_univariate_model: aplica las transformaciones del modelo univariante */
/* (Box-Cox, sustracción de deterministas, diferenciación) y devuelve la      */
/* serie estacionaria w[1..nstat_out].                                        */
/*                                                                           */
/* DataMat se asume con DataMat[0][1..nobs] disponible para la serie          */
/* transformada, y DataMat[1..NdetVar][1..nobs] para las deterministas.       */
/* -------------------------------------------------------------------------- */
void apply_univariate_model(struct Tusmodel *Tm, struct Tseries *Ts,
                            real **DataMat, real **w_out, int *nstat_out)
{
    int nobs = Ts->nobs;
    real lam = Tm->boxlam;
    int i, t, j;
    real *detrended;
    real *filt_num;   /* resultado intermedio del filtro numerador */
    real *filt_out;   /* resultado del filtro completo Ω(B)/Δ(B)  */

    /* 1. Box-Cox: DataMat[0][t] = refactor * (data[t]^λ - 1)/λ  (o log si λ≈0).
       El factor de reescalado de FUE MULTIPLICA la serie transformada: deja las
       varianzas en O(10), que es el rango en el que el optimizador puede
       trabajar (el paso de diferencias finitas de cdgrad es ~6e-6 absoluto).  */
    for (t = 1; t <= nobs; t++) {
        real y = Ts->data[t];
        if (y <= 0.0) {
            fprintf(stderr, "Error: dato no positivo para Box-Cox (t=%d, y=%g)\n",
                    t, y);
            *w_out = NULL;
            *nstat_out = 0;
            return;
        }
        if (fabs(lam) < 1e-8)
            DataMat[0][t] = log(y) * Ts->refactor;
        else
            DataMat[0][t] = ((pow(y, lam) - 1.0) / lam) * Ts->refactor;
    }

    /* 2. Sustraer componentes deterministas */
    detrended = vector(1, nobs);
    for (t = 1; t <= nobs; t++) detrended[t] = DataMat[0][t];

    if (Tm->NdetVar > 0) {
        /* DataMat lo rellena read_fue_pre a partir de la especificación del
           .pre (impulse/compimp/step/ramp/easter/trend/cos/sin/alter).      */
        filt_num = vector(1, nobs);
        filt_out = vector(1, nobs);

        for (i = 1; i <= Tm->NdetVar; i++) {
            int nw = Tm->Nomega[i];
            int nd = Tm->Ndelta[i];

            /* Inicializar a cero */
            for (t = 1; t <= nobs; t++) {
                filt_num[t] = 0.0;
                filt_out[t] = 0.0;
            }

            /* --- Aplicar numerador Ω(B) = ω₀ - ω₁B - ω₂B² - ... (convencion
               Box-Jenkins, como fue: calcnu en fue.c:4505 hace nu[j] = ... - ω[j]).
               El termino lider (j=0) suma; los demas RESTAN.  Antes se sumaban
               todos (+), lo que invertia el signo de los ω no lideres: latente
               porque la homologacion solo tiene Nomega=0; lo destapo m6 con las
               intervenciones compuestas. --- */
            for (t = 1; t <= nobs; t++) {
                real sum = 0.0;
                for (j = 0; j <= nw; j++) {
                    if (t - j >= 1)
                        sum += (j == 0 ? Tm->Omega[i][j] : -Tm->Omega[i][j])
                               * DataMat[i][t - j];
                }
                filt_num[t] = sum;
            }

            /* --- Aplicar denominador 1/Δ(B) = 1/(1-δ₁B-δ₂B²-...) --- */
            /* El filtro recursivo: out[t] = num[t] + Σ δⱼ·out[t-j]   */
            if (nd > 0) {
                for (t = 1; t <= nobs; t++) {
                    real sum = filt_num[t];
                    for (j = 1; j <= nd; j++) {
                        if (t - j >= 1)
                            sum += Tm->Delta[i][j] * filt_out[t - j];
                    }
                    filt_out[t] = sum;
                }
            } else {
                /* Sin denominador: out = num directamente */
                for (t = 1; t <= nobs; t++)
                    filt_out[t] = filt_num[t];
            }

            /* Restar la contribución filtrada */
            for (t = 1; t <= nobs; t++)
                detrended[t] -= filt_out[t];
        }

        free_vector(filt_out, 1, nobs);
        free_vector(filt_num, 1, nobs);
    }

    /* 3. Aplicar operador no estacionario (diferenciación) */
    {
        int ornsop = Tm->ornsop;
        int nstat  = nobs - ornsop;

        if (nstat <= 0) {
            fprintf(stderr, "Error: demasiadas diferencias (ornsop=%d >= nobs=%d)\n",
                    ornsop, nobs);
            free_vector(detrended, 1, nobs);
            *w_out = NULL;
            *nstat_out = 0;
            return;
        }

        *w_out = vector(1, nstat);

        /* w[t] = Σ_{j=0}^{ornsop} (-rnsop[j]) * detrended[t+ornsop-j]
           donde rnsop[0] = -1, así que -rnsop[0] = +1                  */
        for (t = 1; t <= nstat; t++) {
            real sum = 0.0;
            int base = t + ornsop;   /* índice en la serie original */
            for (j = 0; j <= ornsop; j++) {
                sum += (-Tm->rnsop[j]) * detrended[base - j];
            }
            (*w_out)[t] = sum;
        }

        *nstat_out = nstat;
    }

    free_vector(detrended, 1, nobs);
}

/* -------------------------------------------------------------------------- */
/* prewhiten_ccf -- los pasos 1 a 4 de la identificacion                      */
/* -------------------------------------------------------------------------- */

int prewhiten_nlags( int n )
{
    int k = n / 4;

    if ( k > 24 ) k = 24;
    if ( k < 10 ) k = 10;
    return k;
}

int prewhiten_ccf( struct Tusmodel *tm_ent, struct Tseries *ts_ent,
                   real **dm_ent,
                   struct Tusmodel *tm_sal, struct Tseries *ts_sal,
                   real **dm_sal,
                   int nlags, double *ccf, double *nu, int *n,
                   real ***res, char *why, size_t size )
{
    real *wX = NULL, *wY = NULL, *a = NULL, *beta = NULL;
    real *cpos = NULL, *cneg = NULL;
    real  ma, mb, sa, sb;
    int   nX = 0, nY = 0, nn, t, k, lag, rc = 1;

    /* --- las dos series estacionarias, cada una con SU modelo ------------- */
    apply_univariate_model( tm_ent, ts_ent, dm_ent, &wX, &nX );
    apply_univariate_model( tm_sal, ts_sal, dm_sal, &wY, &nY );
    if ( !wX || !wY ) {
        snprintf( why, size, "no pude transformar %s a estacionaria",
                  !wX ? "la entrada" : "la salida" );
        goto fin;
    }

    /* El motor compara nobs y nada mas; aqui, lo mismo, pero DICIENDOLO. */
    if ( nX != nY )
        snprintf( why, size,
                  "las estacionarias no miden igual (%d y %d): se usan las "
                  "ultimas %d, alineadas por el final", nX, nY,
                  nX < nY ? nX : nY );
    nn = nX < nY ? nX : nY;
    if ( nn < 4 * nlags ) nlags = nn / 4;
    if ( nlags < 1 ) {
        snprintf( why, size, "%d observaciones estacionarias no dan para "
                  "una CCF", nn );
        goto fin;
    }

    a    = vector( 1, nn );
    beta = vector( 1, nn );

    /* --- 1 y 2: el MISMO filtro a las dos --------------------------------- */
    /* phi(B) w -> u; luego invertir theta(B): a[t] = u[t] + SUM theta_j a[t-j] */
    {
    /* El modelo viene FACTORIZADO en el .pre; el filtro necesita el polinomio
     * EXPANDIDO, y lo da el mismo expansor que usa el motor.               */
    int   p     = total_ar_order( tm_ent );
    int   q     = total_ma_order( tm_ent );
    real *phi   = p > 0 ? vector( 1, p ) : NULL;
    real *theta = q > 0 ? vector( 1, q ) : NULL;
    int   dX    = nX - nn, dY = nY - nn;   /* alineadas por el final */

    if ( phi )   expand_ar_factors( tm_ent, phi, p );
    if ( theta ) expand_ma_factors( tm_ent, theta, q );

    for ( t = 1; t <= nn; t++ ) {
        real u = wX[t + dX];
        for ( k = 1; k <= p && k < t; k++ ) u -= phi[k] * wX[t + dX - k];
        a[t] = u;
        for ( k = 1; k <= q && k < t; k++ ) a[t] += theta[k] * a[t - k];
    }
    for ( t = 1; t <= nn; t++ ) {
        real u = wY[t + dY];
        for ( k = 1; k <= p && k < t; k++ ) u -= phi[k] * wY[t + dY - k];
        beta[t] = u;
        for ( k = 1; k <= q && k < t; k++ ) beta[t] += theta[k] * beta[t - k];
    }

    if ( theta ) free_vector( theta, 1, q );
    if ( phi )   free_vector( phi, 1, p );
    }

    /* --- 3: la CCF, con la rutina del motor ------------------------------- */
    ma = Mean( a, nn );      sa = Stdev( a, nn );
    mb = Mean( beta, nn );   sb = Stdev( beta, nn );

    if ( sa < 1e-12 || sb < 1e-12 ) {
        snprintf( why, size, "%s no tiene variabilidad tras preblanquear",
                  sa < 1e-12 ? "la entrada" : "la salida" );
        goto fin;
    }

    cpos = vector( 1, nlags + 1 );
    cneg = vector( 1, nlags + 1 );

    /* Ccf(d1,d2) devuelve corr(d1_t, d2_{t+j-1}), o sea k = j-1 >= 0. Asi que
     * la ENTRADA primero da los retardos positivos --la transferencia-- y al
     * reves los negativos --la retroalimentacion.                          */
    Ccf( a, beta, nn, nlags, cpos, ma, mb, sa, sb );
    Ccf( beta, a, nn, nlags, cneg, mb, ma, sb, sa );

    for ( lag = 0; lag <= nlags; lag++ ) {
        ccf[nlags + lag] = cpos[lag + 1];
        ccf[nlags - lag] = cneg[lag + 1];
    }
    /* los dos coinciden en k = 0 por construccion */

    /* --- 4: los pesos de la respuesta impulso ----------------------------- */
    if ( nu )
        for ( lag = -nlags; lag <= nlags; lag++ )
            nu[nlags + lag] = ccf[nlags + lag] * sb / sa;

    /* Los dos preblanqueados, para el portmanteau de Hosking. */
    if ( res ) {
        real **r = matrix( 1, nn, 1, 2 );

        for ( t = 1; t <= nn; t++ ) { r[t][1] = beta[t]; r[t][2] = a[t]; }
        *res = r;
    }

    *n = nn;
    rc = 0;

fin:
    if ( cneg ) free_vector( cneg, 1, nlags + 1 );
    if ( cpos ) free_vector( cpos, 1, nlags + 1 );
    if ( beta ) free_vector( beta, 1, nn );
    if ( a )    free_vector( a, 1, nn );
    if ( wY )   free_vector( wY, 1, nY );
    if ( wX )   free_vector( wX, 1, nX );
    return rc;
}
