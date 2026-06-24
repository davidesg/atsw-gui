/*****************************************************************************/
/*  forecast.h -- part of drvarma (multivariate VARMA modelling).
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

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
void forecast_model(int m, int n, int p, int q, real *mu,
                    real ***phi, real ***theta, real **sigma,
                    real **w, real **a,
                    real **f1, real ***v1, real ***v2, real ***v3,
                    int b, int L, int s, real **datamat);

/**
 * @brief Forecast-error variance of the (undifferenced) LEVEL series.
 *
 * The engine models the stationary series w = (1-B)^d (1-B^s)^D y, where y is
 * the (scale*Box-Cox) level.  This routine integrates the model's psi-weights
 * through the differencing operator delta(B) = (1-B)^d (1-B^s)^D to obtain the
 * MA(inf) weights of y, and returns V_y(l) = sum_{j<l} psi*_j Sigma psi*_j'
 * (the level forecast-error covariance on the scale*Box-Cox scale).
 *
 * @param v_level  output tensor [1..L][1..m][1..m].
 */
void forecast_level_variances(int m, int p, int q, real ***phi, real ***theta,
                              real **sigma, int L, int d, int D, int s,
                              real ***v_level, real ***v_mon, real ***v_ann);

#endif
