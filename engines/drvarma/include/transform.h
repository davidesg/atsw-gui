/*****************************************************************************/
/*  transform.h -- part of drvarma (multivariate VARMA modelling).
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

/***************************************************************************/
/*  TRANSFORM.H                                                            */
/*  Box-Cox transformation and regular/seasonal differencing for DRVARMA. */
/*  Global (common) lambda, d and D applied to every series.              */
/***************************************************************************/
#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "main.h"

/* Box-Cox (scalar). lambda == 0 => natural logarithm.                     */
real boxcox_fwd( real x, real lambda );
real boxcox_inv( real y, real lambda );

/*-------------------------------------------------------------------------*/
/*  Transformation pipeline.                                               */
/*                                                                         */
/*  transform_series() takes the raw level data (nobs_raw x m, 1-based),   */
/*  applies the Box-Cox transform and then d regular differences (lag 1)   */
/*  followed by D seasonal differences (lag s=freq).  It returns the       */
/*  stationary matrix used for estimation (length nobs_eff = nobs_raw -    */
/*  d - D*s) and, through *bc_out, the Box-Cox series BEFORE differencing  */
/*  (nobs_raw x m), needed later to integrate the forecasts back to levels.*/
/*                                                                         */
/*  On error (e.g. non-positive value with lambda!=1, or not enough data)  */
/*  it returns NULL and sets *ifault > 0.                                  */
/*-------------------------------------------------------------------------*/
real **transform_series( real **raw, int nobs_raw, int m,
                         real lambda, real scale, int d, int D, int s,
                         int *nobs_eff, real ***bc_out, int *ifault );

/*-------------------------------------------------------------------------*/
/*  Forecast inversion.                                                    */
/*                                                                         */
/*  Given the Box-Cox series history bc (nobs_raw x m) and the forecasts   */
/*  of the fully-differenced series wf (L x m, 1-based: wf[l][i]), it      */
/*  reconstructs the forecasts of the original LEVEL series and stores     */
/*  them in level_out (m x L, 1-based: level_out[i][l]) to match the       */
/*  layout used by forecast_model.                                         */
/*-------------------------------------------------------------------------*/
void integrate_forecast( real **bc, int nobs_raw, int m,
                         real **wf, int L,
                         real lambda, real scale, int d, int D, int s,
                         real **level_out );

#endif /* TRANSFORM_H */
