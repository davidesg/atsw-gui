#ifndef ARMA_H
#define ARMA_H
#include <stdbool.h>

void calcular_coeficientes_psi(int p, double *phi, int q, double *theta,
                               int P, double *Phi, int Q, double *Theta,
                               int s, double *psi, int max_psi);

void calcular_ACF_PACF_SARIMA(int p, double *phi, int q, double *theta,
                             int P, double *Phi, int Q, double *Theta,
                             int s, double *acf, double *pacf, int lags);

bool check_ar_roots(double *phi, int p);
#endif
