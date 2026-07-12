#ifndef FORECAST_H
#define FORECAST_H

#include "main.h"

/**
 * @brief Calcula las previsiones puntuales y las matrices de varianza del error
 *        para un modelo VARMA(p,q) estimado.
 *
 * @param m         Número de series.
 * @param n         Número total de observaciones (incluyendo el período de estimación).
 * @param p         Orden AR.
 * @param q         Orden MA.
 * @param mu        Vector de medias (longitud m).
 * @param phi       Matrices AR (phi[1..p][1..m][1..m]).
 * @param theta     Matrices MA (theta[1..q][1..m][1..m]).
 * @param sigma     Matriz de covarianza del ruido (m x m).
 * @param w         Datos centrados (w[1..n][1..m]).
 * @param a         Residuos (a[1..n][1..m]).
 * @param f1        Matriz de salida para previsiones puntuales (nivel) [1..m][1..L].
 * @param v1        Tensor de salida para varianzas del nivel [1..L][1..m][1..m].
 * @param v2        Tensor de salida para varianzas de diferencias simples [1..L][1..m][1..m].
 * @param v3        Tensor de salida para varianzas de diferencias estacionales [1..L][1..m][1..m].
 * @param b         Origen hacia atrás (generalmente 0).
 * @param L         Horizonte de previsión.
 * @param s         Período estacional (para diferencias estacionales, por defecto 1 = sin estacionalidad).
 * @param datamat   Datos originales (opcional, necesario para calcular diferencias estacionales iniciales).
 * @param nobs      Número de observaciones disponibles (n).
 */
/* Pesos psi del VARMA (respuesta impulso). drtran los necesita ademas para
   propagar el error de prevision de la ENTRADA a traves del filtro nu(B).   */
void compute_psi_weights(int m, int p, int q, real ***phi, real ***theta,
                         int L, real ***psi);

void forecast_model(int m, int n, int p, int q, real *mu,
                    real ***phi, real ***theta, real **sigma,
                    real **w, real **a,
                    real **f1, real ***v1, real ***v2, real ***v3,
                    int b, int L, int s, real **datamat);

#endif
