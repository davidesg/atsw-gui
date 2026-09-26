/*
 * fuepre_forecast.c -- from a forecast of the stationary series back to the
 * level, with the univariate model of a .pre.
 *
 * An engine that works on the stationary series w of a .pre (drtran, drvarma
 * in ladder mode) forecasts w. What the analyst wants is the LEVEL, and the
 * road back is the .pre's own model, in reverse:
 *
 *     y*_t = refactor * BoxCox(y_t)             the transformed level
 *     b_t  = y*_t - det_t                       without the deterministic part
 *     b_t  = w_t - SUM_k rnsop_k b_{t-k}        integrated with the operator
 *     y*_t = b_t + det_t                        the deterministic part back:
 *                                               a known function of time
 *     y_t  = BoxCox^-1(y*_t / refactor)
 *
 * and, for the forecast-error variance, the psi weights of w integrated with
 * the same operator: psi_level(B) = psi_w(B) / rnsop(B).
 *
 * This was written inside drtran (forecast_levels, transfer_forecast). It is
 * here because drvarma 5.0 forecasts in ladder mode and needs exactly the same
 * steps: one source, as for the reader.
 *
 * Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 * GPL v2 or later; see COPYING.
 */

#include "main.h"
#include "fue_pre_reader.h"
#include <math.h>

real fuepre_bc(const struct Tusmodel *Tm, const struct Tseries *Ts, real y)
{
    return (fabs(Tm->boxlam) < 1e-8)
           ? log(y) * Ts->refactor
           : ((pow(y, Tm->boxlam) - 1.0) / Tm->boxlam) * Ts->refactor;
}

real fuepre_bc_inverse(const struct Tusmodel *Tm, const struct Tseries *Ts,
                       real ystar, real *jac)
{
    real lam = Tm->boxlam, lvl;
    if (fabs(lam) < 1e-8) {
        lvl = exp(ystar / Ts->refactor);
        if (jac) *jac = lvl / Ts->refactor;
    } else {
        real base = lam * (ystar / Ts->refactor) + 1.0;
        lvl = pow(base, 1.0 / lam);
        if (jac) *jac = pow(base, 1.0 / lam - 1.0) / Ts->refactor;
    }
    return lvl;
}

void fuepre_level_forecast(struct Tusmodel *Tm, struct Tseries *Ts, int nb,
                           const real *wf, int L, real *ystar, real *lvl,
                           real *jac)
{
    real *det = vector(1, nb + L), *b = vector(1, nb + L);
    int   t, k, l, ord = Tm->ornsop;

    build_det_component(Tm, Ts, nb + L, det);
    for (t = 1; t <= nb; t++) b[t] = fuepre_bc(Tm, Ts, Ts->data[t]) - det[t];
    for (l = 1; l <= L; l++) {
        real acc = wf[l];
        int  tt  = nb + l;
        for (k = 1; k <= ord; k++) acc -= (-Tm->rnsop[k]) * b[tt - k];
        b[tt] = acc;
    }
    for (t = 1; t <= nb + L; t++) ystar[t] = b[t] + det[t];
    for (l = 1; l <= L; l++)
        lvl[l] = fuepre_bc_inverse(Tm, Ts, ystar[nb + l], jac ? &jac[l] : NULL);

    free_vector(b, 1, nb + L);
    free_vector(det, 1, nb + L);
}

void fuepre_integrator(const struct Tusmodel *Tm, int L, real *uu)
{
    int t, k, ord = Tm->ornsop;
    uu[0] = 1.0;
    for (t = 1; t <= L; t++) {
        real sum = 0.0;
        for (k = 1; k <= ord && k <= t; k++)
            sum += (-Tm->rnsop[k]) * uu[t - k];
        uu[t] = -sum;
    }
}
