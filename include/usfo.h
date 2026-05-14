/***************************************************************************
 *   Copyright (C) 2009 by Arthur B. Treadway and David E. Guerrero        *
 *   davidesg@ucm.es                                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef __USFO_H__
#define __USFO_H__

#include <stdio.h>
#include <math.h>



/****************************************************************************************/

struct Forecast                   /* forecast data  structure:                          */
    {
     int L;                       /* Lead time for forecasting (N§ of forecasts):       */
     int b;                       /* Dead time for first point forecast                 */
     int Aper;                     /* Last obseverved year                              */
     int Asub;                     /* Last observed season  (Origen: Asub/aper)         */
     double sigma2;               /* Maximun Likelihood Estimation of Std.               */
     double **f1, **f2, **f3;     /* Point forecast                                     */
     double ***v1, ***v2, ***v3;  /* Forecast variances                                 */
     double ***phi0;              /* Coefficients of the Autoregressive  operator:      */
     double *data;                /* Transformed time series                            */
     char * latex_file;            /* Latex file for forecasting                        */
    };                                                                                  
/****************************************************************************************/

void varphi(int ornsop, int p, double ***phi0, double ***phi, double *rnsop);

void forecast( int m, int n, int ornsop, int p, int q, double *mu, double ***phi,
               double ***theta, double sigma2, double **w, double **a,
               double **f1, double ***v1, double ***v2, double ***v3, int b, int L, int f,
               double **xius, int has_deterministic );

void point_forecast ( int m, int freq, int L, int nobs, double *data,
			 double **f1, double **f2, double **f3);

void forecast_table_acii (FILE *outputv,int nobs, int freq, int begyear, int begtime, int ornsop, int L,
	double *data, double **a, double **f1, double **f2, double **f3, double ***v1, 
	double ***v2, double ***v3, double boxlam, double refactor, char *variable_name);

void forecast_table_latex ( FILE *prevputv, int nobs, int freq, int begyear, int begtime, int ornsop, int L, double *data, double **a, double **f1, double **f2, double **f3, double ***v1, double ***v2, double ***v3, double boxlam, double refactor);
void forecast_table_latex_BC ( FILE *prevputv, int nobs, int freq, int begyear, int begtime, int ornsop, int L, double *data, double **a, double **f1, double **f2, double **f3, double ***v1, double ***v2, double ***v3, double boxlam, double refactor);


void forecast_graphic ( double *data, double **res, double **f3, double ***v3, int ornsop, double sigma2, int begyear, int begtime, int nobs, int L, int freq, char *x11out, double refactor );
void forecast_graphic_BC ( double *data, double **res, double **f3, double ***v3, int ornsop, double sigma, int begyear, int begtime, int nobs, int L, int freq, double boxlam, char *x11out, double refactor );


void make_latex_forecast (FILE *textputv, char *prevputf, int Aper, int Asub, int freq, char *name );


#endif /* __USFO_H__ */
/*****************************************************************************/
/*****************************************************************************/
