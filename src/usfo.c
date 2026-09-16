
/*****************************************************************************/
/*  usfo.c                                                                   */
/*  Computation of forecasts from a US ARMA(p,q) model.                      */
/*  Copyright (C) 2004 Arthur B. Treadway & David E. Guerrero                */
/*                                                                           */
/*  This program is free software; you can redistribute it and/or modify    */
/*  it under the terms of the GNU General Public License as published by     */
/*  the Free Software Foundation; either version 2 of the License, or       */
/*  (at your option) any later version.                                      */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,         */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the             */
/*  GNU General Public License for more details.                             */
/*                                                                           */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program; if not, write to the Free Software Foundation,  */
/*  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.     */
/*****************************************************************************/

#include "usfo.h"              /* Header file (prototype declarations)       */
#include "nlatools.h"
#include "fuf.h"
#include "fufplot.h"

/*****************************************************************************/
/*****************************************************************************/

void varphi(int ornsop, int p, double ***phi0, double ***phi, double *rnsop)
/*Compute coefficients of the Autoregressive and Non-stationary  operator:   */
{
int i, j;
       for ( i = 1; i <= (ornsop + p) ; i++ ) phi0[i][1][1] = 0.0;
                phi0[0][1][1] = -1.0;
                rnsop[0]=-1;
		phi[0][1][1]=-1;
       for ( i = 0; i <= ornsop; i++ )
       for ( j = 0; j <= p; j++ ) phi0[j+i][1][1] -= phi[j][1][1] * rnsop[i];

                phi0[0][1][1] = 0.0;
		phi[0][1][1]= 0.0;
}


void forecast( int m, int n, int ornsop, int p, int q, double *mu, double ***phi,
               double ***theta, double sigma2, double **w, double **a,
               double **f1, double ***v1, double ***v2, double ***v3, int b, int L, int f,
               double **xius, int has_deterministic, double *drift )
{
   int  i, i1, j, j1, k, l;
   double ***psi1, ***psi2, ***psi3, **mtmp1, **mtmp2, *vtmp1, *vtmp2, s1, s2;

   psi1  = tensor( 0, L, 1, m, 1, m );
   psi2  = tensor( 0, L, 1, m, 1, m );
   psi3  = tensor( 0, L, 1, m, 1, m );
   mtmp1 = matrix( 1, m, 1, m );
   mtmp2 = matrix( 1, m, 1, m );
   vtmp1 = vector( 1, m );
   vtmp2 = vector( 1, m );


/*****************************************************************************/
/* [1]: Compute point forecasts for the level series (f1):                   */
/*****************************************************************************/

   for ( l = 1; l <= L; l++ )
       {
       for ( i = 1; i <= m; i++ ) vtmp1[i] = 0.0;
       for ( i = 1; i <= p; i++ )
           if ( l > i )
              for ( i1 = 1; i1 <= m; i1++ )
                  {
                  s1 = 0.0;
                  for ( k = 1; k <= m; k++ )
                      s1 += phi[i][i1][k] * f1[k][l-i] ;
                  vtmp1[i1] += s1;
                  }
           else
              for ( i1 = 1; i1 <= m; i1++ )
                  {
                  s1 = 0.0;
                  for ( k = 1; k <= m; k++ )
                      s1 += phi[i][i1][k] * w[k][n-b-i+l];
                  vtmp1[i1] += s1;
                  }

       for ( j = 1; j <= m; j++ ) vtmp2[j] = 0.0;
       for ( j = 1; j <= q; j++ )
           if ( l <= j )
              for ( i1 = 1; i1 <= m; i1++ )
                  {
                  s1 = 0.0;
                  for ( k = 1; k <= m; k++ )
                      s1 += theta[j][i1][k] * a[k][n-b-j+l-ornsop];
                  vtmp2[i1] += s1;
                  }

       for ( i1 = 1; i1 <= m; i1++ )
           f1[i1][l] = vtmp1[i1] - vtmp2[i1] + drift[i1];   /* + intercepto mu*phi(1) */
       }


/*****************************************************************************/
/* [2]: Adjust level forecasts by deterministic (intervention) effects (f1): */
/*****************************************************************************/
/* CORRECCION DEL BUG DE LA MEDIA: la deriva NO se anade aqui como l*mu sobre */
/* un transitorio homogeneo (eso duplicaba la deriva -> exceso mu*phi/(1-phi));*/
/* el intercepto correcto mu*phi(1) ya va DENTRO de la recursion en [1].       */
/* Aqui solo se anade el efecto determinista (intervenciones/armonicos).       */

   if (has_deterministic) {
       for ( l = 1; l <= L; l++ )
           for ( k = 1; k <= m; k++ )
               f1[k][l] += xius[k][l+n];
   }
   (void) s2; (void) mu;

/*
s2 = 0.0;

  for ( l = 1; l <= L; l++ )
       {

       for ( k = 1; k <= m; k++ ) 
	 {	 
     if  (mu[k]  == 0)
         f1[k][l] += xius[k][l+n];
     else {
         f1[k][l] += xius[k][l+n] + mu[k];

         s2 += xius[k][l+n] + mu[k];
	     f1[k][l] += s2;
        }
    }
	 }
       
*/


/*****************************************************************************/
/* [2]: Adjust level forecasts by deterministic (intervention) effects (f1): */
/*****************************************************************************/
/*
  for ( l = 1; l <= L; l++ )
       {
  //      s2 = 0.0; // Inicializar s2 para cada l
       for ( k = 1; k <= m; k++ )
         {
           if (mu[k] == 0) {
               if (xius[k][l+n] != 0.0) {
                   f1[k][l] += xius[k][l+n];
               }
               else {
                    s2 = 0.0;
                    f1[k][l] += s2;}


           } else {
               if (xius[k][l+n] != 0.0) {
                   s2 += xius[k][l+n] + mu[k];
                   f1[k][l] += s2;}
                else {
                    s2 += mu[k];
                    f1[k][l] += s2; }

           }
         }
       }

/*****************************************************************************/
/* [3]: Compute sequence of psi-weights for the level series (psi1):         */
/*****************************************************************************/

   for ( l = 0; l <= L; l++ )
       for ( i = 1; i <= m; i++ )
           for ( j = 1; j <= m; j++ ) psi1[l][i][j] = 0.0;
   for ( i = 1; i <= m; i++ ) psi1[0][i][i] = 1.0;

   for ( j = 1; j <= L; j++ )
       {
       for ( i = 1; i <= j; i++ )
           if ( i <= p )
              for ( i1 = 1; i1 <= m; i1++ )
                  for ( j1 = 1; j1 <= m; j1++ )
                      {
                      s1 = 0.0;
                      for ( k = 1; k <= m; k++ )
                          s1 += phi[i][i1][k] * psi1[j-i][k][j1];
                      psi1[j][i1][j1] += s1;
                      }
       if ( j <= q )
          for ( i1 = 1; i1 <= m; i1++ )
              for ( j1 = 1; j1 <= m; j1++ ) psi1[j][i1][j1] -= theta[j][i1][j1];
       }


/*****************************************************************************/
/* [4]: Compute sequence of variances for the level series (v1):             */
/*****************************************************************************/

   for ( l = 1; l <= L; l++ )
       for ( i = 1; i <= m; i++ )
           for ( j = 1; j <= m; j++ ) v1[l][i][j] = 0.0;

   for ( l = 1; l <= L; l++ )
       for ( j = 0; j <= l-1; j++ )
           {
           for ( i1 = 1; i1 <= m; i1++ )
               for ( j1 = 1; j1 <= m; j1++ )
                   {
                   s1 = 0.0;
                   for ( k = 1; k <= m; k++ )
                       s1 += psi1[j][i1][k] * sigma2;
                   mtmp1[i1][j1] = s1;
                   }
           for ( i1 = 1; i1 <= m; i1++ )
               for ( j1 = 1; j1 <= m; j1++ )
                   {
                   s1 = 0.0;
                   for ( k = 1; k <= m; k++ )
                       s1 += mtmp1[i1][k] * psi1[j][j1][k];
                   mtmp2[i1][j1] = s1;
                   }
           for ( i1 = 1; i1 <= m; i1++ )
               for ( j1 = 1; j1 <= m; j1++ ) v1[l][i1][j1] += mtmp2[i1][j1];
           }


/*****************************************************************************/
/* [5]: Compute sequence of psi-weights for the d-level series (psi2):       */
/*****************************************************************************/

   for ( i = 1; i <= m; i++ )
       for ( j = 1; j <= m; j++ ) psi2[0][i][j] = psi1[0][i][j];

   for ( l = 1; l <= L; l++ )
       for ( i = 1; i <= m; i++ )
           for ( j = 1; j <= m; j++ )
               psi2[l][i][j] = psi1[l][i][j] - psi1[l-1][i][j];


/*****************************************************************************/
/* [6]: Compute sequence of variances for the d-level series (v2):           */
/*****************************************************************************/

   for ( l = 1; l <= L; l++ )
       for ( i = 1; i <= m; i++ )
           for ( j = 1; j <= m; j++ ) v2[l][i][j] = 0.0;

   for ( l = 1; l <= L; l++ )
       for ( j = 0; j <= l-1; j++ )
           {
           for ( i1 = 1; i1 <= m; i1++ )
               for ( j1 = 1; j1 <= m; j1++ )
                   {
                   s1 = 0.0;
                   for ( k = 1; k <= m; k++ )
                       s1 += psi2[j][i1][k] * sigma2 ;
                   mtmp1[i1][j1] = s1;
                   }
           for ( i1 = 1; i1 <= m; i1++ )
               for ( j1 = 1; j1 <= m; j1++ )
                   {
                   s1 = 0.0;
                   for ( k = 1; k <= m; k++ )
                       s1 += mtmp1[i1][k] * psi2[j][j1][k];
                   mtmp2[i1][j1] = s1;
                   }
           for ( i1 = 1; i1 <= m; i1++ )
               for ( j1 = 1; j1 <= m; j1++ ) v2[l][i1][j1] += mtmp2[i1][j1];
           }


/*****************************************************************************/
/* [7]: Compute sequence of psi-weights for the d4-level series (psi3):      */
/*****************************************************************************/

   for ( l = 0; l <= (f-1); l++ )
       for ( i = 1; i <= m; i++ )
           for ( j = 1; j <= m; j++ ) psi3[l][i][j] = psi1[l][i][j];

   for ( l = f; l <= L; l++ )
       for ( i = 1; i <= m; i++ )
           for ( j = 1; j <= m; j++ )
               psi3[l][i][j] = psi1[l][i][j] - psi1[l-f][i][j];


/*****************************************************************************/
/* [8]: Compute sequence of variances for the d4-level series (v3):          */
/*****************************************************************************/

   for ( l = 1; l <= L; l++ )
       for ( i = 1; i <= m; i++ )
           for ( j = 1; j <= m; j++ ) v3[l][i][j] = 0.0;

   for ( l = 1; l <= L; l++ )
       for ( j = 0; j <= l-1; j++ )
           {
           for ( i1 = 1; i1 <= m; i1++ )
               for ( j1 = 1; j1 <= m; j1++ )
                   {
                   s1 = 0.0;
                   for ( k = 1; k <= m; k++ )
                       s1 += psi3[j][i1][k] * sigma2;
                   mtmp1[i1][j1] = s1;
                   }
           for ( i1 = 1; i1 <= m; i1++ )
               for ( j1 = 1; j1 <= m; j1++ )
                   {
                   s1 = 0.0;
                   for ( k = 1; k <= m; k++ )
                       s1 += mtmp1[i1][k] * psi3[j][j1][k];
                   mtmp2[i1][j1] = s1;
                   }
           for ( i1 = 1; i1 <= m; i1++ )
               for ( j1 = 1; j1 <= m; j1++ ) v3[l][i1][j1] += mtmp2[i1][j1];
           }

/*****************************************************************************/

   free_vector( vtmp2, 1, m );
   free_vector( vtmp1, 1, m );
   free_matrix( mtmp2, 1, m, 1, m );
   free_matrix( mtmp1, 1, m, 1, m );
   free_tensor( psi3, 0, L, 1, m, 1, m );
   free_tensor( psi2, 0, L, 1, m, 1, m );
   free_tensor( psi1, 0, L, 1, m, 1, m );
}

/*****************************************************************************/

void point_forecast ( int m, int freq, int L, int nobs, double *data,
			 double **f1, double **f2, double **f3)
{
/*   Compute point forecasts for the d-level and df-level series:       */
int i, j;

   f2[1][1] = f1[1][1] -  data[nobs];

   for ( j = 2; j <= L; j++ )
       for ( i = 1; i <= m; i++ ) f2[i][j] = f1[i][j] - f1[i][j-1];

   for ( j = 1; j <= freq; j++ )
       for ( i = 1; i <= m; i++ )
           f3[i][j] = f1[i][j] - data[nobs-freq+j];

   for ( j = freq+1; j <= L; j++ )
       for ( i = 1; i <= m; i++ ) f3[i][j] = f1[i][j] - f1[i][j-freq];

}

void forecast_table_acii ( FILE *outputv,int nobs, int freq, int begyear, int begtime, int ornsop, int L,
	double *data, double **a, double **f1, double **f2, double **f3, double ***v1, 
	double ***v2, double ***v3, double boxlam, double refactor, char * variable_name)

{
int i, j, Aper1, Asub1;

if ( freq > 1 ){
          fprintf( outputv, "FORECAST REPORT: \n" );
          fprintf( outputv, "VARIABLE NAME: %s\n", variable_name );
          ObsToDate( begyear, begtime, nobs, freq, &Aper1, &Asub1 );
          fprintf( outputv, "FORECAST ORIGIN : %2d/%4d\n", Asub1, Aper1);
          fprintf( outputv, "LEAD TIME FOR FORECASTING: %d\n", L);
          fprintf( outputv, "\n" );
          fprintf( outputv, " +---------------------------------------------------------------+\n" );
          fprintf( outputv, " |      |     LEVEL      |           VARIATION           |       |\n" );
          fprintf( outputv, " | DATE +----------------+-------------------------------+  ERR  |\n" );
          fprintf( outputv, " |      |  VALUE   | STD | TRIMEST | STD |  ANUAL  | STD |       |\n" );
          fprintf( outputv, " |      |          |     | /MONT   |     |         |     |       |\n" );
          fprintf( outputv, " +---------------------------------------------------------------+\n" );
	  
       for ( i = nobs - L; i <= nobs; i++ )   /* Print data and errors:     */
           {
	     ObsToDate( begyear, begtime, i, freq, &Aper1, &Asub1 );
	     fprintf( outputv," %2d/%4d", Asub1, Aper1);
	     if (boxlam==0) fprintf( outputv,"%9.4f", exp(data[i]/refactor) );
             else fprintf( outputv,"%9.4f",  data[i]/refactor );
	     fprintf( outputv,"     - ");
	     fprintf( outputv,"%8.4f ", 100*(data[i] - data[i-1] )/refactor);
	     fprintf( outputv,"     - ");
	     fprintf( outputv,"%9.4f  ",100*(data[i] - data[i-freq])/refactor );
	     fprintf( outputv,"   - ");
	     fprintf( outputv,"%8.4f \n", 100*a[1][i-ornsop]/refactor );
	   }
       for ( i = 1; i <= L; i++ )              /* Print forecasts and sds:   */
           {
	     ObsToDate( begyear, begtime, nobs+i, freq, &Aper1, &Asub1 );
	     fprintf( outputv," %2d/%4d", Asub1, Aper1);
	     if (boxlam==0)fprintf( outputv,"%9.4f", exp(f1[1][i]/refactor) );
             else fprintf( outputv,"%9.4f",  f1[1][i]/refactor );
	     fprintf( outputv,"%7.4Lf", 100*sqrtl( v1[i][1][1] )/refactor );
	     fprintf( outputv,"%8.4f", 100*f2[1][i]/refactor );
	     fprintf( outputv,"%8.4Lf", 100*sqrtl( v2[i][1][1] )/refactor );
	     fprintf( outputv,"%9.4f", 100*f3[1][i]/refactor );
	     fprintf( outputv,"%7.4Lf \n", 100*sqrtl( v3[i][1][1] )/refactor );
	   }

}
if ( boxlam < 0.0 ){
            fprintf( outputv, "FORECAST REPORT: \n" );
          fprintf( outputv, "VARIABLE NAME: %s\n", variable_name );
          ObsToDate( begyear, begtime, nobs, freq, &Aper1, &Asub1 );
          fprintf( outputv, "FORECAST ORIGIN : %2d/%4d\n", Asub1, Aper1);
          fprintf( outputv, "LEAD TIME FOR FORECASTING: %d\n", L);
          fprintf( outputv, "\n" );
          fprintf( outputv, " +----------------------------------------------------------------------+\n" );
          fprintf( outputv, " |      |        LEVEL          |   BC   TRANSFORMATION         |       |\n" );
          fprintf( outputv, " | DATE +-------+-------+-------+-------------------------------+  ERR  |\n" );
          fprintf( outputv, " |      |  LOW  |       | UPER  | LEVEL |  STD  |  DIF  |  STD  |       |\n" );
          fprintf( outputv, " |      |  BAND |       | BAND  |       |       |       |       |       |\n" );
          fprintf( outputv, " +----------------------------------------------------------------------+\n" );
         for ( i = nobs - L; i <= nobs; i++ )   /* Print data and errors:     */
           {
	     ObsToDate( begyear, begtime, i, freq, &Aper1, &Asub1 );
	     fprintf( outputv,"  %4d", Asub1, Aper1);
	     fprintf( outputv,"     - ");	     
	     fprintf( outputv,"%9.4f",  pow (((data[i]/refactor) *boxlam + 1), (1/boxlam))) ;
	     fprintf( outputv,"     - ");
	     fprintf( outputv,"%8.4f ", 100*data[i]/refactor);
	     fprintf( outputv,"     - ");
	     fprintf( outputv,"%9.4f  ",100*(data[i] - data[i-freq])/refactor );
	     fprintf( outputv,"   - ");
	     fprintf( outputv,"%8.4f \n", 100*a[1][i-ornsop]/refactor );
	   }
       for ( i = 1; i <= L; i++ )              /* Print forecasts and sds:   */
           {
	     ObsToDate( begyear, begtime, nobs+i, freq, &Aper1, &Asub1 );
	     fprintf( outputv," %2d/%4d", Asub1, Aper1);
	     fprintf( outputv,"%7.4f", pow ((((f1[1][i]- 2*sqrt ( v1[i][1][1] ))/refactor )*boxlam + 1)  , (1/boxlam)));	     
	     fprintf( outputv,"%9.4f", pow (((f1[1][i]/refactor) *boxlam + 1), (1/boxlam)));	    
	     fprintf( outputv,"%7.4f", pow ((((f1[1][i]+ 2*sqrt ( v1[i][1][1] ))/refactor )*boxlam + 1)  , (1/boxlam)));	     
	     fprintf( outputv,"%8.4f",  100*f1[1][i]/refactor );
	     fprintf( outputv,"%8.4Lf", 100*sqrtl( v1[i][1][1] )/refactor );
	     fprintf( outputv,"%9.4f", 100*f3[1][i]/refactor );
	     fprintf( outputv,"%7.4Lf \n", 100*sqrtl( v3[i][1][1] )/refactor );
	   }
	  
	  
	  
}
  
  

fprintf( outputv, "\n" );
}

/* The table of the forecasts. When the model is in logarithms (boxlam = 0)
 * the changes are rates and are written in per cent; when it is not, they
 * are changes in the units of the series and multiplying them by a hundred
 * only filled the table with numbers that did not fit.                     */

void forecast_table_latex ( FILE *prevputv, int nobs, int freq, int begyear, int begtime, int ornsop, int L, double *data, double **a, double **f1, double **f2, double **f3, double ***v1, double ***v2, double ***v3, double boxlam, double refactor)

{
int i, Aper1, Asub1;
double pct = ( boxlam == 0 ) ? 100.0 : 1.0;
const char *head = ( boxlam == 0 ) ? "LOG RATE OF CHANGE" : "CHANGE";
const char *unit = ( boxlam == 0 ) ? "($\\%$)" : " ";


   fprintf( prevputv,"\\begin{tabular}{rccrcrcr} \n");
   fprintf( prevputv,"\\hline \n " );
   fprintf( prevputv,"\\multicolumn{1}{|c|}{}  &  \\multicolumn{2}{c|}{} & \n");
   fprintf( prevputv,"\\multicolumn{4}{c}{} & \\multicolumn{1}{|c|}{}\\vspace{-.10in}\\\\ \n");
/*   fprintf( prevputv,"\\multicolumn{1}{|c|}{}  &  \\multicolumn{2}{c|}{NIVEL} & \n"); */
   fprintf( prevputv,"\\multicolumn{1}{|c|}{}  &  \\multicolumn{2}{c|}{LEVEL} & \n"); 
/*   fprintf( prevputv,"\\multicolumn{4}{c}{TASAS LOG DE VARIACI�N} & \\multicolumn{1}{|c|}{}    \\\\  \n"); */
   fprintf( prevputv,"\\multicolumn{4}{c}{%s} & \\multicolumn{1}{|c|}{}    \\\\  \n", head ); 
   fprintf( prevputv,"\\multicolumn{1}{|c|}{}  &  \\multicolumn{2}{c|}{} & \n");
   fprintf( prevputv,"\\multicolumn{4}{c}{} & \\multicolumn{1}{|c|}{}\\vspace{-.10in}\\\\ \\cline{2-7} \n");
   fprintf( prevputv,"\\multicolumn{1}{|c|}{ } &\\multicolumn{1}{c|}{ } & \\multicolumn{1}{c|}{} & \\multicolumn{1}{c|}{} & \\multicolumn{1}{c|}{}     & \\multicolumn{1}{c|}{} & \\multicolumn{1}{c|}{}  & \\multicolumn{1}{c|}{}  \\vspace{-.05in}\\\\  \n");

/*   fprintf( prevputv,"\\multicolumn{1}{|c|}{FECHA } &\\multicolumn{1}{c|}{ VALOR} & \\multicolumn{1}{c|}{DT} & \\multicolumn{1}{c|}{MENS} & \\multicolumn{1}{c|}{DT}     & \\multicolumn{1}{c|}{ANUAL} & \\multicolumn{1}{c|}{DT}  & \\multicolumn{1}{c|}{ERR}\\\\  \n"); */

   if (freq==12)fprintf( prevputv,"\\multicolumn{1}{|c|}{DATE } &\\multicolumn{1}{c|}{ VALUE} & \\multicolumn{1}{c|}{Std} & \\multicolumn{1}{c|}{MONT} & \\multicolumn{1}{c|}{Std}     & \\multicolumn{1}{c|}{ANUAL} & \\multicolumn{1}{c|}{Std}  & \\multicolumn{1}{c|}{ERR}\\\\  \n"); 
   if (freq==4) fprintf( prevputv,"\\multicolumn{1}{|c|}{DATE } &\\multicolumn{1}{c|}{ VALUE} & \\multicolumn{1}{c|}{Std} & \\multicolumn{1}{c|}{QUART} & \\multicolumn{1}{c|}{Std}     & \\multicolumn{1}{c|}{ANUAL} & \\multicolumn{1}{c|}{Std}  & \\multicolumn{1}{c|}{ERR}\\\\  \n");

   fprintf( prevputv,"\\multicolumn{1}{|c|}{ }      & \\multicolumn{1}{c|}{  }    & \\multicolumn{1}{c|}{%s}&  \\multicolumn{1}{c|}{%s  }  & \\multicolumn{1}{c|}{%s} & \\multicolumn{1}{c|}{%s }     & \\multicolumn{1}{c|}{ %s }  &  \\multicolumn{1}{c|}{ %s } \\\\  \n ", unit, unit, unit, unit, unit, unit );
   fprintf( prevputv,"\\hline \n \\\\");
   for ( i = nobs - L/2; i <= nobs; i++ )   /* Print data and errors:     */
	{
	ObsToDate( begyear, begtime, i, freq, &Aper1, &Asub1 );
	fprintf( prevputv,"$\\mathsf{%8d/%4d}$ &", Asub1, Aper1);
	if (boxlam==0) fprintf( prevputv,"$\\mathsf{%9.2f}$ &", exp(data[i]/refactor) );
        else  fprintf( prevputv,"$\\mathsf{%9.2f}$ &",  data[i]/refactor );
	fprintf( prevputv,"     - &");
	fprintf( prevputv,"$\\mathsf{%8.2f}$ &", pct*(data[i] - data[i-1])/refactor );
	fprintf( prevputv,"     - &");
	fprintf( prevputv,"$\\mathsf{%9.2f }$ &", pct*(data[i] - data[i-freq])/refactor );
	fprintf( prevputv,"     - &");
	fprintf( prevputv,"$\\mathsf{%7.2f} $ \\vspace{-.005in}\\\\ \n", pct*a[1][i-ornsop]/refactor );
	}
   for ( i = 1; i <= L/2; i++ )              /* Print forecasts and sds:   */
	{
	ObsToDate( begyear, begtime, nobs+i, freq, &Aper1, &Asub1 );
	fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}r}{$\\mathsf{ %8d/%4d}$} &", Asub1, Aper1);
	if (boxlam==0) fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}c}{$\\mathsf{%9.2f}$} &", exp(f1[1][i]/refactor) );
	else fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}c}{$\\mathsf{%9.2f}$} &", f1[1][i]/refactor );
	fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}c}{$\\mathsf{%7.2Lf}$} &", pct*sqrtl( v1[i][1][1] )/refactor );
	fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}r}{$\\mathsf{%7.2f}$} &", pct*f2[1][i]/refactor );
	fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}r}{$\\mathsf{%7.2Lf}$} &", pct*sqrtl( v2[i][1][1] )/refactor );
	fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}r}{$\\mathsf{%8.2f}$} &", pct*f3[1][i]/refactor );
	fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}r}{$\\mathsf{%7.2Lf}$} & \\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}c}{  - }\\vspace{-.005in} \\\\ \n ", pct*sqrtl( v3[i][1][1] )/refactor );
	}
   fprintf( prevputv," \\\\ \n ");
   for ( i = L/2 + 1; i <= L; i++ )
	{
	ObsToDate( begyear, begtime, nobs+i, freq, &Aper1, &Asub1 );
	if(Asub1 == freq)
	      {
	       fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}c}{$\\mathsf{ %8d/%4d}$} &", Asub1, Aper1);
	       if (boxlam==0) fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}c}{$\\mathsf{%9.2f}$} &", exp(f1[1][i]/refactor) );
	       else fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}c}{$\\mathsf{%9.2f}$} &", f1[1][i]/refactor );
	       fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}r}{$\\mathsf{%7.2Lf}$} &", pct*sqrtl( v1[i][1][1] )/refactor );
	       fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}r}{$\\mathsf{%7.2f }$}&", pct*f2[1][i]/refactor );
	       fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}r}{$\\mathsf{%7.2Lf}$} &", pct*sqrtl( v2[i][1][1] )/refactor );
	       fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}r}{$\\mathsf{%8.2f}$} &", pct*f3[1][i]/refactor );
	       fprintf( prevputv,"\\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}r}{$\\mathsf{%7.2Lf}$} & \\multicolumn{1}{>{\\columncolor [rgb]{0.95, 0.95, 0.95}}c}{  - }\\vspace{-.005in} \\\\  \n ", pct*sqrtl( v3[i][1][1] )/refactor );
	     }
	 }
    fprintf( prevputv,"\\end{tabular} \n\n\n\n\n");

}

void forecast_table_latex_BC ( FILE *prevputv, int nobs, int freq, int begyear, int begtime, int ornsop, int L, double *data, double **a, double **f1, double **f2, double **f3, double ***v1, double ***v2, double ***v3, double boxlam, double refactor)

{
int i, Aper1, Asub1;


   fprintf( prevputv,"\\begin{tabular}{crccrcrcr} \n");
   fprintf( prevputv,"\\hline \n " );
   fprintf( prevputv,"\\multicolumn{1}{|c|}{}  &  \\multicolumn{3}{c|}{} & \n");
   fprintf( prevputv,"\\multicolumn{4}{c}{} & \\multicolumn{1}{|c|}{}\\vspace{-.10in}\\\\ \n");
   fprintf( prevputv,"\\multicolumn{1}{|c|}{}  &  \\multicolumn{3}{c|}{LEVEL} & \n"); 
   fprintf( prevputv,"\\multicolumn{4}{c}{B-C TRANSFORMATION} & \\multicolumn{1}{|c|}{}    \\\\  \n"); 
   fprintf( prevputv,"\\multicolumn{1}{|c|}{}  &  \\multicolumn{3}{c|}{} & \n");
   fprintf( prevputv,"\\multicolumn{4}{c}{} & \\multicolumn{1}{|c|}{}\\vspace{-.10in}\\\\ \\cline{2-8} \n");
   fprintf( prevputv,"\\multicolumn{1}{|c|}{ }     &\\multicolumn{1}{c|}{ }     & \\multicolumn{1}{c|}{} & \\multicolumn{1}{c|}{}    & \\multicolumn{1}{c|}{}           & \\multicolumn{1}{c|}{} &        \\multicolumn{1}{c|}{}  & \\multicolumn{1}{c|}{}  \\vspace{-.05in}\\\\  \n");
   fprintf( prevputv,"\\multicolumn{1}{|c|}{DATE } &\\multicolumn{1}{c|}{LOW}     &\\multicolumn{1}{c|}{} & \\multicolumn{1}{c|}{UP} & \\multicolumn{1}{c|}{LEVEL}      & \\multicolumn{1}{c|}{Std}     & \\multicolumn{1}{c|}{DIF} & \\multicolumn{1}{c|}{Std}  & \\multicolumn{1}{c|}{ERR}\\\\  \n"); 
   fprintf( prevputv,"\\multicolumn{1}{|c|}{     } &\\multicolumn{1}{c|}{BAND  } & \\multicolumn{1}{c|}{}& \\multicolumn{1}{c|}{BAND}&  \\multicolumn{1}{c|}{($\\%%$)  }& \\multicolumn{1}{c|}{($\\%%$)} & \\multicolumn{1}{c|}{($\\%%$) }     & \\multicolumn{1}{c|}{ ($\\%%$) }  &  \\multicolumn{1}{c|}{ ($\\%%$) } \\\\  \n ");
   fprintf( prevputv,"\\hline \n \\\\");
   for ( i = nobs - L/2; i <= nobs; i++ )   /* Print data and errors:     */
	{
	ObsToDate( begyear, begtime, i, freq, &Aper1, &Asub1 );
	fprintf( prevputv,"$\\mathsf{%4d}$ &", Aper1);
	fprintf( prevputv,"     - &");   
        fprintf( prevputv,"$\\mathsf{%9.2f}$ &", pow (((data[i]/refactor) *boxlam + 1), (1/boxlam)));
	fprintf( prevputv,"     - &");
	fprintf( prevputv,"$\\mathsf{%8.2f}$ &", 100*data[i]/refactor );
	fprintf( prevputv,"     - &");
	fprintf( prevputv,"$\\mathsf{%9.2f }$ &", 100*(data[i] - data[i-freq])/refactor );
	fprintf( prevputv,"     - &");
	fprintf( prevputv,"$\\mathsf{%7.2f} $ \\vspace{-.005in}\\\\ \n", 100*a[1][i-ornsop]/refactor );
	}
   for ( i = 1; i <= L/2; i++ )              /* Print forecasts and sds:   */
	{
	ObsToDate( begyear, begtime, nobs+i, freq, &Aper1, &Asub1 );
	fprintf( prevputv,"\\rowcolor[gray]{.9}$\\mathsf{ %4d}$ &", Aper1);
	fprintf( prevputv,"$\\mathsf{%7.2f}$ &", pow ((((f1[1][i]- 2*sqrt ( v1[i][1][1] ))/refactor )*boxlam + 1), (1/boxlam)) );   
	fprintf( prevputv,"$\\mathsf{%9.2f}$ &", pow (((f1[1][i]/refactor) *boxlam + 1), (1/boxlam)) );
	fprintf( prevputv,"$\\mathsf{%7.2f}$ &", pow ((((f1[1][i]+ 2*sqrt ( v1[i][1][1] ))/refactor )*boxlam + 1), (1/boxlam)) );
	fprintf( prevputv,"$\\mathsf{%7.2f}$ &", 100*f1[1][i]/refactor );
	fprintf( prevputv,"$\\mathsf{%7.2Lf}$ &", 100*sqrtl( v1[i][1][1] )/refactor );
	fprintf( prevputv,"$\\mathsf{%8.2f}$ &", 100*f2[1][i]/refactor );
	fprintf( prevputv,"$\\mathsf{%7.2Lf}$ &   -  \\vspace{-.005in} \\\\ \n ", 100*sqrtl( v2[i][1][1] )/refactor );
	}
   fprintf( prevputv," \\\\ \n ");
   fprintf( prevputv,"\\end{tabular} \n\n\n\n\n");

}

/* The graph of the forecasts, drawn by fuf itself (src/fufplot.c): the same
 * two panels that gnuplot drew. The series is the annual change of the last
 * L observations and of the L forecasts: a rate, in per cent, when the model
 * is in logarithms (boxlam = 0), and a change in the units of the series
 * when it is not -- multiplying that one by a hundred only took the graph
 * off its scale. sigma2 is the variance of the residuals, in the units of
 * the estimation; the panel of the errors shows them in the same units as
 * the residuals it draws.                                                  */

FDFig *forecast_graphic ( double *data, double **res, double **f3, double ***v3, int ornsop, double sigma2, int begyear, int begtime, int nobs, int L, int freq, double boxlam, char *x11out, double refactor )

{
int i, Asub1, Aper1, Asub0, Aper0;
double *y, *y1, *y2, *a;
double pct = ( boxlam == 0.0 ) ? 100.0 : 1.0;
char name[4096];
FDFig *fig;

y  = vector (0, 2*L);
y1 = vector (0, 2*L);
y2 = vector (0, 2*L);
a  = vector (0, L-1);

       for(i=0; i < L; i++)
	 {
	   y[i]= pct*(data[i + 1 + nobs - L] - data[i + 1 + nobs - L - freq])/refactor;	    
	   y1[i]= 0.0;
	   y2[i]= 0.0;
	   a[i]= pct*(res[1][(i+1) +(nobs - ornsop - L)])/refactor;
	 }

       for(i=0; i < L; i++)
	 {
	   y[i+L]= pct*f3[1][i+1]/refactor;
	   y1[i+L]= pct*(f3[1][i+1]+sqrtl( v3[i+1][1][1] ))/refactor;
	   y2[i+L]= pct*(f3[1][i+1]-sqrtl( v3[i+1][1][1] ))/refactor;
	 } 

  ObsToDate( begyear, begtime, nobs+1, freq, &Aper1, &Asub1 );
  ObsToDate( begyear, begtime, nobs+1-L, freq, &Aper0, &Asub0 );
  snprintf( name, sizeof( name ), "prev%s.%d%d.eps", x11out, Asub1, Aper1 );

  fig = fp_forecast( y, y1, y2, a, L, pct * sqrt( sigma2 ) / refactor, freq,
                     Aper0, Asub0,
                     ( boxlam == 0.0 ) ? "LRC anual (%)" : "Annual change" );
  if ( fig != NULL && fd_write_eps( fig, name ) != 0 )
     fprintf( stderr, "Warning: %s was not written\n", name );

free_vector( y, 0, 2*L );
free_vector( y1, 0, 2*L );
free_vector( y2, 0, 2*L );
free_vector( a, 0, L-1 );
return( fig );                    /* the report puts it on its page */
}

/* The same graph for a series transformed with a Box-Cox lambda other than
 * 0: the level is the transformation undone, and the band is of two standard
 * deviations (src/fufplot.c).                                              */

FDFig *forecast_graphic_BC ( double *data, double **res, double **f1, double ***v1, int ornsop, double sigma, int begyear, int begtime, int nobs, int L, int freq, double boxlam, char *x11out, double refactor )

{
int i, Asub1, Aper1, Asub0, Aper0;
double *y, *y1, *y2, *a;
char name[4096];
FDFig *fig;

y  = vector (0, 2*L);
y1 = vector (0, 2*L);
y2 = vector (0, 2*L);
a  = vector (0, L-1);

       for(i=0; i < L; i++)
	 {
	   y[i]= pow (((data[i+ 1 + nobs - L]/refactor) *boxlam + 1), (1/boxlam)) ;	    
	   y1[i]= 0.0;
	   y2[i]= 0.0;
	   a[i]= 100*(res[1][(i+1) +(nobs - ornsop - L)])/refactor;
	 }

       for(i=0; i < L; i++)
	 {
	   y[i+L]= pow (((f1[1][i+1]/refactor) *boxlam + 1), (1/boxlam));
	   y1[i+L]= pow ((((f1[1][i+1]+ 2*sqrt ( v1[i+1][1][1] ))/refactor )*boxlam + 1)  , (1/boxlam));
	   y2[i+L]= pow ((((f1[1][i+1]- 2*sqrt ( v1[i+1][1][1] ))/refactor )*boxlam + 1)  , (1/boxlam));
	 } 

  ObsToDate( begyear, begtime, nobs+1, freq, &Aper1, &Asub1 );
  ObsToDate( begyear, begtime, nobs+1-L, freq, &Aper0, &Asub0 );
  snprintf( name, sizeof( name ), "prev%s.%d%d.eps", x11out, Asub1, Aper1 );

  fig = fp_forecast( y, y1, y2, a, L, sigma / 100.0, freq, Aper0, Asub0, "LEVEL" );
  if ( fig != NULL && fd_write_eps( fig, name ) != 0 )
     fprintf( stderr, "Warning: %s was not written\n", name );

free_vector( y, 0, 2*L );
free_vector( y1, 0, 2*L );
free_vector( y2, 0, 2*L );
free_vector( a, 0, L-1 );
return( fig );                    /* the report puts it on its page */
}

void make_latex_forecast (FILE *texputv, char *prevputf, int Aper, int Asub, int freq, char *name )

{

/* 10pt with \\footnotesize and single spacing: the size of the published
 * report, where the table and the graph at scale .60 end together.        */
fprintf(texputv, "\\documentclass[a4paper,10pt]{article}\n");
fprintf(texputv, "\\usepackage[latin1]{inputenc}\n");
fprintf(texputv, "\\usepackage{color}\n");
fprintf(texputv, "\\usepackage{colortbl}\n");
fprintf(texputv, "\\usepackage{lscape}\n");
fprintf(texputv, "\\usepackage{multicol}\n");
fprintf(texputv, "\\usepackage{graphicx}\n");
fprintf(texputv, "\\usepackage{setspace}\n");
fprintf(texputv, "\\setlength{\\columnsep}{2.5pc}\n");
fprintf(texputv, "\\oddsidemargin -.25 in \\textwidth 6.60in \\topmargin -.10in \n");
fprintf(texputv, "\\headheight 0in \\textheight 24.06cm \n");
fprintf(texputv, "\\renewcommand{\\baselinestretch}{.98} \n");
fprintf(texputv, "\n");
fprintf(texputv, "\\begin{document}\n");
fprintf(texputv, "\\thispagestyle{empty}\n");
fprintf(texputv, "\\begin{landscape}\n");
fprintf(texputv, "\\begin{center}\n");
fprintf(texputv, "\n");
fprintf(texputv, "\\textbf{%s} \\\\ \n", name);
fprintf(texputv, "\n");
fprintf(texputv, "\\textbf{Series brief description} \\\\ \n");
fprintf(texputv, "\n");
fprintf(texputv, "Base/Unit: \n");
fprintf(texputv, "\\space \\space \\space Data Source:  \n");
if ( freq == 1) fprintf(texputv, "\\space \\space \\space Forecast Origin: %d \\\\ \n",  Aper );
else fprintf(texputv, "\\space \\space \\space Forecast Origin: %d/%d \\\\ \n", Asub, Aper );
fprintf(texputv, "\\end{center}\n");
fprintf(texputv, "\n");
fprintf(texputv, "\\begin{footnotesize} \\singlespacing \\input{%s} \\end{footnotesize} \n", prevputf);
fprintf(texputv, "\n");
fprintf(texputv, "\\end{landscape}\n");
fprintf(texputv, "\\end{document}\n");
}

