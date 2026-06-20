#ifndef ARMA_H
#define ARMA_H
#include <stdbool.h>

void calcular_coeficientes_psi(int p, double *phi, int q, double *theta,
                               int P, double *Phi, int Q, double *Theta,
                               int s, double *psi, int max_psi);

void calcular_ACF_PACF_SARIMA(int p, double *phi, int q, double *theta,
                             int P, double *Phi, int Q, double *Theta,
                             int s, double *acf, double *pacf, int lags);

void plot_theoretical_acf_pacf(double *acf, double *pacf, int lags, double cmax,
                               int p, int q, int P, int Q, int s, char *title);

bool check_ar_roots(double *phi, int p);
#endif
