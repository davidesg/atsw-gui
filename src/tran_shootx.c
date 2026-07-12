#include "main.h"
#include "drtran.h"
#include "fue_pre_reader.h"   /* para apply_univariate_model */

/* -------------------------------------------------------------------------- */
/* Función auxiliar: calcula la secuencia de pesos nu para un filtro racional */
/* omega(B)/delta(B) con retardo b.                                           */
/* nu[t] = omega[t-1-b] + sum_{j=1..r} delta[j] * nu[t-j]                     */
/* donde t es índice 1-based en la serie estacionaria.                         */
/* omega[0] = ω₀ (contemporáneo), omega[1] = ω₁, ..., omega[s] = ω_s.          */
/* -------------------------------------------------------------------------- */
static void compute_irf(real *omega, int s, real *delta, int r, int b,
                         real *nu, int length)
{
    int t, j;
    for (t = 1; t <= length; t++) nu[t] = 0.0;

    for (t = 1; t <= length; t++) {
        real sum = 0.0;
        /* contribución del numerador: lag 0-based = t-1-b */
        int lag = t - 1 - b;
        if (lag >= 0 && lag <= s) sum = omega[lag];
        /* contribución del denominador */
        for (j = 1; j <= r; j++) {
            if (t > j) sum += delta[j] * nu[t - j];
        }
        nu[t] = sum;
    }
}

/* -------------------------------------------------------------------------- */
/* Recalcula las series estacionarias w_X e w_Y aplicando el modelo           */
/* univariante. Se llama en cada evaluación de shootx.                       */
/* -------------------------------------------------------------------------- */
static void recompute_stationary_series(void)
{
    build_stationary_pair();   /* reconstruye w_X, w_Y y las alinea; fija n_stat */
}

/* -------------------------------------------------------------------------- */
/* Estabilidad del denominador δ(B) de cada variable determinista.            */
/* El filtro 1/δ(B) es recursivo: si δ(B) tiene raíces dentro del círculo     */
/* unidad, la contribución determinista explota y la serie estacionaria se va */
/* a infinito. Se comprueba con chekma, que usa la misma convención de        */
/* polinomio (1 - δ₁B - δ₂B² - …). Devuelve 1 si alguna δ es inestable.       */
/* -------------------------------------------------------------------------- */
static int unstable_delta(struct Tusmodel *Tm)
{
    int i, k;
    real wr[10], wi[10], wmod[10];

    for (i = 1; i <= Tm->NdetVar; i++) {
        int nd = Tm->Ndelta[i];
        int ifault_chk = 0;
        real ***t1;

        if (nd <= 0) continue;

        t1 = tensor(0, nd, 1, 1, 1, 1);
        t1[0][1][1] = 1.0;
        for (k = 1; k <= nd; k++) t1[k][1][1] = Tm->Delta[i][k];

        chekma(1, nd, t1, wr, wi, wmod, &ifault_chk);
        free_tensor(t1, 0, nd, 1, 1, 1, 1);

        if (ifault_chk != 0) return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------------- */
/* shootx: transforma el vector de parámetros en la estructura Tvarma        */
/* -------------------------------------------------------------------------- */
void shootx(real *x, struct Tvarma *armax, int *ifaultx, int firstx, int lastx)
{
    int m = 2;   /* bivariante: salida Y, entrada X */
    int idx = 1;
    int i, j, k;
    int p, q;
    real omega[MAX_S+1], delta[MAX_R+1];
    real *transfer_weights;              /* pesos ν, indexados 1..n_stat */
    real *transfer;                      /* componente de transferencia en cada t */

    *ifaultx = 0;

    /* Los vectores de trabajo se reservan MÁS ABAJO, una vez que
       recompute_stationary_series() ha fijado el n_stat definitivo.          */

    /* 1. Extraer parámetros de la transferencia */
    for (j = 0; j <= s_ord; j++) omega[j] = x[idx++];
    for (j = 1; j <= r_ord; j++) delta[j] = x[idx++];

    /* 2. Extraer parámetros ARMA del ruido de Y (si no fijos) — FACTORES */
    if (!fix_noise) {
        unpack_ar_factors(&TmY, x, &idx);
        unpack_ma_factors(&TmY, x, &idx);
        /* Re-expandir después de actualizar Tm */
        if (p_N > 0) expand_ar_factors(&TmY, phi_N, p_N);
        if (q_N > 0) expand_ma_factors(&TmY, theta_N, q_N);
    }
    /* 3. Extraer parámetros ARMA de X (si no fijos) — FACTORES */
    if (!fix_X) {
        unpack_ar_factors(&TmX, x, &idx);
        unpack_ma_factors(&TmX, x, &idx);
        if (p_X > 0) expand_ar_factors(&TmX, phi_X, p_X);
        if (q_X > 0) expand_ma_factors(&TmX, theta_X, q_X);
    }

    /* 3b/3c. Coeficientes deterministas ω(B)/δ(B) de cada serie: solo los que
              el .pre marca como estimables (Imega/Ielta).                    */
    if (!fix_det_Y) unpack_det_params(&TmY, x, &idx);
    if (!fix_det_X) unpack_det_params(&TmX, x, &idx);

    /* Rechazar denominadores δ(B) inestables: el filtro 1/δ(B) explotaría */
    if (unstable_delta(&TmY) || unstable_delta(&TmX)) {
        *ifaultx = 1;
        return;
    }

    /* Rechazar factores de frecuencia fija con c₂ >= 0 (r = sqrt(−c₂)) */
    if (invalid_fixfreq(&TmY) || invalid_fixfreq(&TmX)) {
        *ifaultx = 1;
        return;
    }

    /* 3d. Extraer medias (cada serie por separado; si está fija conserva el
           valor leído del .pre de FUE) */
    if (!fix_mu_Y) mu_Y = x[idx++];
    if (!fix_mu_X) mu_X = x[idx++];

    /* 4. Covarianza Q (diagonal).
          OJO: est() CONCENTRA un factor de escala sigma2 (Σ = sigma2·Q), así que
          la escala global de Q NO está identificada: meter var_Y y var_X como dos
          parámetros libres deja una dirección exactamente plana en la
          verosimilitud y un hessiano SINGULAR (se veía en SE de Q de 4·10^5).
          Solo la RAZÓN de varianzas es estimable. Se normaliza Q[1,1] = 1 y se
          estima log(var_X/var_Y): un único parámetro, bien escalado y con
          positividad garantizada. La escala la recupera sigma2.               */
    real log_ratio = x[idx++];
    real var_Y = 1.0;
    real var_X = exp(log_ratio);

    /* Re-expandir factores ARMA desde Tm (por si fueron modificados) */
    /* Si los ARMA están fijos, recalcular desde Tm; si libres, ya se leyeron de x[] */
    if (fix_noise) {
        if (p_N > 0) expand_ar_factors(&TmY, phi_N, p_N);
        if (q_N > 0) expand_ma_factors(&TmY, theta_N, q_N);
    }
    if (fix_X) {
        if (p_X > 0) expand_ar_factors(&TmX, phi_X, p_X);
        if (q_X > 0) expand_ma_factors(&TmX, theta_X, q_X);
    }

    /* Recalcular series estacionarias con los parámetros actuales */
    recompute_stationary_series();
    if (n_stat <= 0) {
        *ifaultx = 1;
        return;
    }

    /* Ahora que n_stat es definitivo, reservar los vectores de trabajo.
       (Antes eran un array de pila de 5000 sin comprobar contra n_stat.)     */
    transfer_weights = vector(1, n_stat);
    transfer         = vector(1, n_stat);

    /* Calcular los pesos nu de la transferencia */
    compute_irf(omega, s_ord, delta, r_ord, b_delay, transfer_weights, n_stat);

    /* Construir la serie de transferencia: transfer[t] = sum_{j} nu[j] * w_X[t-j] */
    for (int t = 1; t <= n_stat; t++) {
        real tr = 0.0;
        for (j = 1; j <= t; j++) {
            tr += transfer_weights[j] * w_X[t - j + 1];
        }
        transfer[t] = tr;
    }

    /* Dimensiones del VARMA */
    p = (p_N > p_X) ? p_N : p_X;
    q = (q_N > q_X) ? q_N : q_X;
    /* elf requiere al menos p >= 1 (phi[1] debe existir aunque sea cero) */
    if (p < 1) p = 1;
    armax->m = m;
    armax->n = n_stat;
    armax->p = p;
    armax->q = q;

    /* Alojar memoria en firstx */
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
                for (k = 0; k <= p; k++) armax->phi[k][i][j] = 0.0;
                for (k = 0; k <= q; k++) armax->theta[k][i][j] = 0.0;
                armax->qq[i][j] = 0.0;
            }
            for (j = 1; j <= n_stat; j++) {
                armax->w[j][i] = 0.0;
                armax->a[j][i] = 0.0;
            }
        }
        /* phi[0] y theta[0] = identidad */
        for (i = 1; i <= m; i++) {
            armax->phi[0][i][i] = 1.0;
            armax->theta[0][i][i] = 1.0;
        }
    } else {
        /* Reinicializar a cero antes de rellenar */
        for (k = 1; k <= p; k++)
            for (i = 1; i <= m; i++)
                for (j = 1; j <= m; j++)
                    armax->phi[k][i][j] = 0.0;
        for (k = 1; k <= q; k++)
            for (i = 1; i <= m; i++)
                for (j = 1; j <= m; j++)
                    armax->theta[k][i][j] = 0.0;
    }

    /* Llenar matrices phi y theta (diagonales) */
    for (k = 1; k <= p; k++) {
        if (k <= p_N) armax->phi[k][1][1] = phi_N[k];
        if (k <= p_X) armax->phi[k][2][2] = phi_X[k];
    }
    for (k = 1; k <= q; k++) {
        if (k <= q_N) armax->theta[k][1][1] = theta_N[k];
        if (k <= q_X) armax->theta[k][2][2] = theta_X[k];
    }

    /* ─── Restricciones: estacionariedad, invertibilidad, varianzas > 0 ─── */
    {
        real wr[10], wi[10], wmod[10];
        int ifault_chk = 0;

        /* Varianzas positivas */
        if (var_Y <= 0.0 || var_X <= 0.0) {
            *ifaultx = 1;
            free_vector(transfer, 1, n_stat);
            free_vector(transfer_weights, 1, n_stat);
            return;
        }

        /* Chequeo simple AR(1): |φ₁| < 1 para cada serie */
        if ((p_N >= 1 && fabs(phi_N[1]) >= 0.999) ||
            (p_X >= 1 && fabs(phi_X[1]) >= 0.999)) {
            *ifaultx = 1;
            free_vector(transfer, 1, n_stat);
            free_vector(transfer_weights, 1, n_stat);
            return;
        }

        /* Chequeo multivariante de invertibilidad MA */
        if (q > 0) {
            chekma(m, q, armax->theta, wr, wi, wmod, &ifault_chk);
            if (ifault_chk != 0) {
                *ifaultx = 1;
                free_vector(transfer, 1, n_stat);
                free_vector(transfer_weights, 1, n_stat);
                return;
            }
        }
    }

    /* Matriz de covarianza qq (diagonal, contiene varianzas) */
    armax->qq[1][1] = var_Y;
    armax->qq[2][2] = var_X;
    armax->qq[1][2] = armax->qq[2][1] = 0.0;
    armax->sigma2 = 1.0;   /* ya que qq incorpora las varianzas */

    /* Media */
    armax->mu[1] = mu_Y;
    armax->mu[2] = mu_X;

    /* Construir las series w: w[.,1] = w_Y - transferencia, w[.,2] = w_X */
    for (int t = 1; t <= n_stat; t++) {
        armax->w[t][1] = w_Y[t] - transfer[t];
        armax->w[t][2] = w_X[t];
    }

    free_vector(transfer, 1, n_stat);
    free_vector(transfer_weights, 1, n_stat);

    if (lastx) {
        free_matrix(armax->a, 1, armax->n, 1, armax->m);
        free_matrix(armax->w, 1, armax->n, 1, armax->m);
        free_matrix(armax->qq, 1, armax->m, 1, armax->m);
        free_tensor(armax->theta, 0, armax->q, 1, armax->m, 1, armax->m);
        free_tensor(armax->phi, 0, armax->p, 1, armax->m, 1, armax->m);
        free_vector(armax->mu, 1, armax->m);
    }
}
