/***************************************************************************/
/*  DESEASON.H                                                            */
/*  CLI harmonic seasonal adjustment for DRVARMA.                          */
/*  Reuses the tested harmonic routines in gui/seasonal_detection.c.       */
/***************************************************************************/
#ifndef DESEASON_H
#define DESEASON_H

#include "main.h"

/*-------------------------------------------------------------------------*/
/*  deseasonalize_raw                                                       */
/*                                                                          */
/*  Seasonally adjusts the RAW level data in place using the harmonic       */
/*  method (same algorithm as the GUI): the seasonal pattern is ESTIMATED   */
/*  on the first differences (d=1, stationary) and the resulting LEVEL      */
/*  seasonal dummies (recovered via the A0 transform) are subtracted from   */
/*  the level series.                                                       */
/*                                                                          */
/*    raw        : data matrix 1..nobs x 1..m (modified in place)           */
/*    s          : seasonal period (= data frequency)                       */
/*    start_sub  : starting subperiod of obs 1 (1..s)                       */
/*    mode       : 0 = auto (only series with significant seasonality,      */
/*                 F-test p<0.05); 1 = force all                            */
/*    dummies    : output, 1..m x 0..s-1, the level seasonal dummies that   */
/*                 were subtracted (0 for series left unadjusted). Kept for  */
/*                 later re-seasonalization of the forecasts.               */
/*    logf       : optional log stream (may be NULL)                        */
/*-------------------------------------------------------------------------*/
void deseasonalize_raw(real **raw, int nobs, int m, int s, int start_sub,
                       int mode, real **dummies, FILE *logf);

#endif /* DESEASON_H */
