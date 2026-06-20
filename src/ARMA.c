#include <stdio.h>
#include <stdlib.h>
#include <math.h>
//#include "gnuplot_i.h"
#include "ARMA.h"
//#include "utils.h"


#define MAX_LAG 40
#define M 2000

void calcular_coeficientes_psi(int p, double *phi, int q, double *theta,
                               int P, double *Phi, int Q, double *Theta,
                               int s, double *psi, int max_psi) {
    // Inicializar PSI
    for (int i = 0; i <= max_psi; i++) psi[i] = 0.0;
    psi[0] = 1.0;

    // For pure MA models (no AR components), we can directly compute the coefficients
    if (p == 0 && P == 0) {
        // Non-seasonal MA components
        for (int i = 1; i <= q && i <= max_psi; i++) {
            psi[i] = -theta[i-1];
        }

        // Seasonal MA components
        for (int i = 1; i <= Q && i*s <= max_psi; i++) {
            psi[i*s] = -Theta[i-1];
        }

        // Multiplicative terms (cross terms between non-seasonal and seasonal)
        for (int i = 1; i <= q; i++) {
            for (int j = 1; j <= Q; j++) {
                int lag = i + j*s;
                if (lag <= max_psi) {
                    psi[lag] += theta[i-1] * Theta[j-1];
                }
            }
        }
        return;
    }

    // Original code for ARMA models remains unchanged below
    // Calcular coeficientes PSI recursivamente
    for (int j = 1; j <= max_psi; j++) {
        double ma_part = 0.0;

        // Parte no estacional MA
        if (j <= q) {
            ma_part -= theta[j - 1];
        }

        // Parte estacional MA
        if (Q > 0 && j % s == 0) {
            int seasonal_index = j / s;
            if (seasonal_index <= Q) {
                ma_part -= Theta[seasonal_index - 1];
            }
        }

        psi[j] = ma_part;

        // Parte no estacional AR
        for (int i = 1; i <= p && i <= j; i++) {
            psi[j] += phi[i - 1] * psi[j - i];
        }

        // Parte estacional AR - CORREGIDA
        for (int i = 1; i <= P; i++) {
            int lag = s * i;
            if (lag <= j) {
                psi[j] += Phi[i - 1] * psi[j - lag];

                // Handle multiplicative terms for seasonal AR
                for (int k = 1; k <= p; k++) {
                    if (lag + k <= j) {
                        psi[j] -= phi[k - 1] * Phi[i - 1] * psi[j - lag - k];
                    }
                }
            }
        }
    }
}

// The rest of the code remains unchanged...




// Función para calcular ACF y PACF teóricas
void calcular_ACF_PACF_SARIMA(int p, double *phi, int q, double *theta,
                             int P, double *Phi, int Q, double *Theta,
                             int s, double *acf, double *pacf, int lags) {
    double psi[M + 1];
    calcular_coeficientes_psi(p, phi, q, theta, P, Phi, Q, Theta, s, psi, M);

    // Calcular autocovarianzas
    double gamma[MAX_LAG + 1] = {0.0};
    for (int k = 0; k <= lags; k++) {
        for (int j = 0; j <= M - k; j++) {
            gamma[k] += psi[j] * psi[j + k];
        }
    }

    // Calcular ACF
    if (gamma[0] > 1e-10) {
        for (int k = 0; k <= lags; k++) {
            acf[k] = gamma[k] / gamma[0];
        }
    } else {
        for (int k = 0; k <= lags; k++) {
            acf[k] = 0.0;
        }
    }

    // Calcular PACF usando el algoritmo de Durbin-Levinson
    double prev_phi[MAX_LAG + 1] = {0.0};
    double curr_phi[MAX_LAG + 1] = {0.0};

    pacf[0] = 1.0;
    if (lags >= 1) {
        pacf[1] = acf[1];
        prev_phi[1] = acf[1];
    }

    for (int n = 2; n <= lags; n++) {
        double num = acf[n];
        double den = 1.0;
        for (int j = 1; j < n; j++) {
            num -= prev_phi[j] * acf[n - j];
            den -= prev_phi[j] * acf[j];
        }
        if (fabs(den) < 1e-10) {
            curr_phi[n] = 0.0;
        } else {
            curr_phi[n] = num / den;
        }
        pacf[n] = curr_phi[n];

        for (int j = 1; j < n; j++) {
            curr_phi[j] = prev_phi[j] - curr_phi[n] * prev_phi[n - j];
        }

        for (int j = 1; j <= n; j++) {
            prev_phi[j] = curr_phi[j];
        }
    }
}

