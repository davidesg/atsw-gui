
/*****************************************************************************/
/*  DIAGNOSE.C                                                               */
/*  Sample statistics and plots for a single time series.                    */
/*  Copyright (C) José Alberto Mauricio, 1996.                               */
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

#include "fue.h"            /* Header file (prototype declarations)        */
#include <stdlib.h>
#include "nlatools.h"            /* Header file (prototype declarations)        */

#include <gsl/gsl_fit.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_multifit.h>

extern real macheps;          /* Machine epsilon (global: declared in DRV.C) */
extern FILE *outputv;         /* Output file (global: declared in DRV.C)     */
extern FILE *distputv;         /* Output file (global: declared in DRV.C)     */
extern FILE *restputv;

/*****************************************************************************/

void ObsToDate( int beg_per, int beg_sub, int obs_no, int freq,
                int *per, int *sub )
{
   div_t cad;

   if ( obs_no + beg_sub - 1 <= freq )
      {
      *per = beg_per;
      *sub = beg_sub + obs_no - 1;
      }
   else
      {
      cad = div( obs_no - (freq - beg_sub + 1), freq );
      if ( cad.rem > 0 )
         {
         *per = beg_per + cad.quot + 1;
         *sub = cad.rem;
         }
      else
         {
         *per = beg_per + cad.quot;
         *sub = freq;
         }
      }
}

/*****************************************************************************/

void DateToObs( int beg_per, int beg_sub, int per, int sub, int freq,
                int *obs_no )
{
   int srest, pcad, sad;

   srest = freq - beg_sub + 1;
   if ( sub == freq )
      {
      pcad = per - beg_per;
      *obs_no = srest + freq * pcad;
      }
   else
      {
      pcad = per - beg_per - 1;
      sad  = sub;
      *obs_no = srest + freq * pcad + sad;
      }
}

/*****************************************************************************/

real Mean( real *data, int nobs )

{
   int  i;
   real sum;

   sum = 0.0;
   for ( i = 1; i <= nobs; i++ ) sum += data[i];
   return( sum / nobs );
}

/*****************************************************************************/

real Stdev( real *data, int nobs )

{
   int  i;
   real sum, ave;

   ave = Mean( data, nobs );
   sum = 0.0;
   for ( i = 1; i <= nobs; i++ )
       sum += (data[i] - ave) * (data[i] - ave);
   return( sqrt( sum / nobs ) );
}

/*****************************************************************************/

int MaxVal( real *data, int nobs )

{
   int  i, max;
   real maximum;

   max     = 1;
   maximum = data[1];

   for ( i = 2; i <= nobs; i++ )
       if ( data[i] >= maximum )
          {
          maximum = data[i];
          max     = i;
          }
   return( max );
}

/*****************************************************************************/

int MinVal( real *data, int nobs )

{
   int  i, min;
   real minimum;

   min     = 1;
   minimum = data[1];

   for ( i = 2; i <= nobs; i++ )
       if ( data[i] <= minimum )
          {
          minimum = data[i];
          min     = i;
          }
   return( min );
}

/*****************************************************************************/

real Skew( real *data, int nobs )

{
   int  i;
   real sum, ave, std;

   ave = Mean( data, nobs );
   std = Stdev( data, nobs );
   if ( std < 1.0e-20 )
      return( 0.0 );
   else
      {
      sum = 0.0;
      for ( i = 1; i <= nobs; i++ )
          sum += ( (data[i]-ave) * (data[i]-ave) * (data[i]-ave) ) /
                 (std * std * std);
      return( sum / nobs );
      }
}

/*****************************************************************************/

real Kurt( real *data, int nobs )

{
   int  i;
   real sum, ave, std;

   ave = Mean( data, nobs );
   std = Stdev( data, nobs );
   if ( std < 1.0e-20 )
      return( 0.0 );
   else
      {
      sum = 0.0;
      for ( i = 1; i <= nobs; i++ )
          sum += ((data[i]-ave) * (data[i]-ave) * (data[i]-ave) * (data[i]-ave)) /
                 ( std * std * std * std );
      return( sum / nobs - 3.0 );
      }
}

/*****************************************************************************/

real JarqueBera( real Skew, real Kurt, int nobs)

{

return ( nobs / 6 * ( Skew*Skew + Kurt*Kurt/4));

}
/*****************************************************************************/

void Acf( struct Tseries *ser, int lags, real *corr )

{
   int  i, j;
   real rtmp1, rtmp2;

   for ( i = 1; i <= lags; i++ ) corr[i] = 0.0;

   rtmp1 = ser->mean;
   rtmp2 = ser->var;

   for ( j = 1; j <= lags; j++ )
     for ( i = 1; i <= ser->nobs-j; i++ )
       corr[j] += (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
}

/*****************************************************************************/

void Pacf( int lags, real *pcorr )

{
   int  i, j;
   real sum1, sum2, **MatPacf, *corr;

   MatPacf = matrix( 1, lags, 1, lags );
   corr    = vector( 1, lags );

   for ( i = 1; i <= lags; i++ )
       {
       for ( j = 1; j <= lags; j++ )
           MatPacf[i][j] = 0.0;
       corr[i] = pcorr[i];            /* Note that pcorr is input as the acf */
       }                              /* and output as the pacf.             */

   MatPacf[1][1] = corr[1];
   for ( i = 2; i <= lags; i++ )
       {
       sum1 = 0.0;
       sum2 = 0.0;
       for ( j = 1; j <= i-1; j++ )
           {
           sum1 += MatPacf[i-1][j] * corr[i-j];
           sum2 += MatPacf[i-1][j] * corr[j];
           }
       MatPacf[i][i] = (corr[i]-sum1) / (1.0-sum2);
       for ( j = 1; j <= i-1; j++ )
           MatPacf[i][j] = MatPacf[i-1][j] - MatPacf[i][i] * MatPacf[i-1][i-j];
       }
   for ( i = 1; i <= lags; i++ ) pcorr[i] = MatPacf[i][i];

   free_vector( corr, 1, lags );
   free_matrix( MatPacf, 1, lags, 1, lags );
}

/*****************************************************************************/

/*DG 06/28/04 ****************************************************************/

void Acf_dist( struct Tseries *ser, int lags )

{
   int  i, j, Aper1, Asub1, Aper2, Asub2, Aper3, Asub3, Aper4, Asub4, Aper5, Asub5, Aper6, Asub6, Aper7, Asub7, Aper8, Asub8;
   real rtmp1, rtmp2, AbsMax1, AbsMax2, AbsMax3, AbsMax4;
   real *corr;

   corr = vector( 1, lags );
   Acf( ser, lags, corr );

   rtmp1 = ser->mean;
   rtmp2 = ser->var;
   
/* Table of contributions                                                     */

/* Table of outliers:                                                        */

   fprintf( outputv, "      +------------------------------------------------------+\n" );
   fprintf( outputv, "      |         Calibration of distortions of the ACF        |\n" );
   fprintf( outputv, "      |                                                      |\n" );
   fprintf( outputv, "      +------------------------------------------------------+\n" );
   fprintf( outputv, "      |                                                      |\n" );
if (ser->numbering == 0)
   fprintf( outputv, "      |     Lag                 Dates          Contribution  |\n" );
else 
   fprintf( outputv, "      |     Lag             Observations       Contribution  |\n" );

   fprintf( outputv, "      |                                                      |\n" );

   for ( j = 1; j <= lags; j++ )
     {
	   AbsMax1=0;
	   AbsMax2=0;
	   AbsMax3=0;
	   AbsMax4=0;

       if ( corr[j] > 0 )
	 {
	   for ( i = 1; i <= ser->nobs-j; i++ )
	     {

	       if ((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2)>= AbsMax1 )
		 {
		   AbsMax1 = (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
		   ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper1, &Asub1 );
		   ObsToDate( ser->begyear, ser->begtime, i+j, ser->freq, &Aper2, &Asub2 );
		 }
	     }
	   for ( i = 1; i <= ser->nobs-j; i++ )
	     {

	       if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) <  AbsMax1*0.9999 ) )		 {
		   if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) >=  AbsMax2 ) )
		     {
		       AbsMax2 = (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
		       ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper3, &Asub3 );
		       ObsToDate( ser->begyear, ser->begtime, i+j, ser->freq, &Aper4, &Asub4 );	
		   }	 
		 }
	     }

	   for ( i = 1; i <= ser->nobs-j; i++ )
	     {

	       if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) <  AbsMax2*0.9999 ) )		 {
		   if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) >=  AbsMax3 ) )
		     {
		       AbsMax3 = (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
		       ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper5, &Asub5 );
		       ObsToDate( ser->begyear, ser->begtime, i+j, ser->freq, &Aper6, &Asub6 );	
		   }	 
		 }
	     }	     

	   for ( i = 1; i <= ser->nobs-j; i++ )
	     {

	       if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) <  AbsMax3*0.9999 ) )		 {
		   if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) >=  AbsMax4 ) )
		     {
		       AbsMax4 = (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
		       ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper7, &Asub7 );
		       ObsToDate( ser->begyear, ser->begtime, i+j, ser->freq, &Aper8, &Asub8 );	
		   }	 
		 }
	     }

	   fprintf( outputv, "      |" );
	   fprintf( outputv, "  r(%d) = %2.3f", j, corr[j] );
	   if ( ser->freq == 1 )
	     {
	       fprintf( outputv, "%12d", Aper1);
               fprintf( outputv, " -" );
               fprintf( outputv, " %4d    ", Aper2 );
	     }
	   else
	     {
	       fprintf( outputv, "%9d/%4d - %2d/%4d", Asub1, Aper1, Asub2, Aper2 );
	     }
	   fprintf( outputv, "%13.3f    |\n", AbsMax1 );
	 	   fprintf( outputv, "      |" );
	   fprintf( outputv, "             " );
	   if ( ser->freq == 1 )
	     {
	       fprintf( outputv, "%13d", Aper3);
               fprintf( outputv, " -" );
               fprintf( outputv, " %4d    ", Aper4 );
	     }
	   else
	     {
	       fprintf( outputv, "%9d/%4d - %2d/%4d", Asub3, Aper3, Asub4, Aper4 );
	     }
	   fprintf( outputv, "%13.3f    |\n", AbsMax2 );
	 	   fprintf( outputv, "      |" );
	   fprintf( outputv, "             " );

	   if ( ser->freq == 1 )
	     {
	       fprintf( outputv, "%13d", Aper5 );
               fprintf( outputv, " -" );
               fprintf( outputv, " %4d    ", Aper6 );
	     }
	   else
	     {
	       fprintf( outputv, "%9d/%4d - %2d/%4d", Asub5, Aper5, Asub6, Aper6 );
	     }
	   fprintf( outputv, "%13.3f    |\n", AbsMax3 );
	 	   fprintf( outputv, "      |" );
	   fprintf( outputv, "             " );
	   if ( ser->freq == 1 )
	     {
	       fprintf( outputv, "%13d", Aper7  );
               fprintf( outputv, " -" );
               fprintf( outputv, " %4d    ",  Aper8  );
	     }
	   else
	     {
	       fprintf( outputv, "%9d/%4d - %2d/%4d", Asub7, Aper7, Asub8, Aper8 );
	     }
	   fprintf( outputv, "%13.3f    |\n", AbsMax4 );
	   fprintf( outputv, "      |                                                      |\n" );

	 }
       if ( corr[j] < 0 )
	 {
	   for ( i = 1; i <= ser->nobs-j; i++ )
	     {
	       if ((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2)<= AbsMax1 )
		 {
		   AbsMax1 = (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
		   ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper1, &Asub1 );
		   ObsToDate( ser->begyear, ser->begtime, i+j, ser->freq, &Aper2, &Asub2 );
		 }
	     }

	   for ( i = 1; i <= ser->nobs-j; i++ )
	     {

	       if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) >  AbsMax1*0.9999 ) )		 {
		   if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) <=  AbsMax2 ) )
		     {
		       AbsMax2 = (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
		       ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper3, &Asub3 );
		       ObsToDate( ser->begyear, ser->begtime, i+j, ser->freq, &Aper4, &Asub4 );	
		   }	 
		 }
	     }

	   for ( i = 1; i <= ser->nobs-j; i++ )
	     {

	       if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) >  AbsMax2*0.9999 ) )		 {
		   if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) <=  AbsMax3 ) )
		     {
		       AbsMax3 = (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
		       ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper5, &Asub5 );
		       ObsToDate( ser->begyear, ser->begtime, i+j, ser->freq, &Aper6, &Asub6 );	
		   }	 
		 }
	     }
	     
	   for ( i = 1; i <= ser->nobs-j; i++ )
	     {

	       if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) >  AbsMax3*0.9999 ) )		 {
		   if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) <=  AbsMax4 ) )
		     {
		       AbsMax4 = (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
		       ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper7, &Asub7 );
		       ObsToDate( ser->begyear, ser->begtime, i+j, ser->freq, &Aper8, &Asub8 );	
		   }	 
		 }
	     }

	   fprintf( outputv, "      |" );
	   fprintf( outputv, "  r(%d) = %2.3f", j, corr[j] );
	   if ( ser->freq == 1 )
	     {
	       fprintf( outputv, "%12d", Aper1);
               fprintf( outputv, " -" );
               fprintf( outputv, " %4d   ", Aper2 );

	     }
	   else
	     {
	       fprintf( outputv, "%8d/%4d - %2d/%4d", Asub1, Aper1, Asub2, Aper2 );
	     }
	   fprintf( outputv, "%13.3f    |\n", AbsMax1 );
	 	   fprintf( outputv, "      |" );
	   fprintf( outputv, "              ");

	   if ( ser->freq == 1 )
	     {
	       fprintf( outputv, "%13d", Aper3);
               fprintf( outputv, " -" );
               fprintf( outputv, " %4d   ", Aper4 );

	     }
	   else
	     {
	       fprintf( outputv, "%8d/%4d - %2d/%4d", Asub3, Aper3, Asub4, Aper4 );
	     }
	   fprintf( outputv, "%13.3f    |\n", AbsMax2 );
	 	   fprintf( outputv, "      |" );
	   fprintf( outputv, "              ");

	   if ( ser->freq == 1 )
	     {
	       fprintf( outputv, "%13d", Aper5 );
               fprintf( outputv, " -" );
               fprintf( outputv, " %4d   ", Aper6 );
	     }
	   else
	     {
	       fprintf( outputv, "%8d/%4d - %2d/%4d", Asub5, Aper5, Asub6, Aper6 );
	     }
	   fprintf( outputv, "%13.3f    |\n", AbsMax3 );
	 	   fprintf( outputv, "      |" );
	   fprintf( outputv, "              ");

	   if ( ser->freq == 1 )
	     {
	       fprintf( outputv, "%13d", Aper7  );
               fprintf( outputv, " -" );
               fprintf( outputv, " %4d   ",  Aper8  );
	     }
	   else
	     {
	       fprintf( outputv, "%8d/%4d - %2d/%4d", Asub7, Aper7, Asub8, Aper8 );
	     }
	   fprintf( outputv, "%13.3f    |\n", AbsMax4 );
	   fprintf( outputv, "      |                                                      |\n" );

	 }
     }
   fprintf( outputv, "      +------------------------------------------------------+\n" );
   free_vector( corr, 1, lags );
}

/*END DG *************************************************************************/


/*DG 06/30/04 ****************************************************************/

void Acf_disttex( struct Tseries *ser, char *distputf  )

{
   int  i, j, Aper1, Asub1, Aper2, Asub2, Aper3, Asub3, Aper4, Asub4;
   real rtmp1, rtmp2, AbsMax1, AbsMax2, Atip1, Atip2, lags;
   real *corr;

   if ( ser->nobs < 3 * (ser->freq + 1) )
      lags = ser->nobs - ser->freq / 2;
   else if (ser->freq == 1)
     lags = 9; 
   else
      lags = 3 * (ser->freq + 1);

   corr = vector( 1, lags );
   Acf( ser, lags, corr );

   Atip1 = 0;
   Atip2 = 0;

   rtmp1 = ser->mean;
   rtmp2 = ser->var;
   
/* Table of contributions                                                     */


   if ( NULL == (distputv = fopen( distputf, "w" )) )
      {
      printf( "\nError opening output file: %s\n", distputf );
      printf( "... Exiting to system ...\n" );
      exit( 1 );
      }
   fprintf( distputv, "\n" );


   fprintf(distputv, "\\thispagestyle{empty}\n");

   fprintf( distputv, "\\begin{multicols}{2}{ \n");
   fprintf( distputv, "\\begin{tabular}{ccc}\n" );
   fprintf( distputv, "\\multicolumn{3}{c}{\\textbf{Contributions of pairs of the extreme values} }  \\vspace{-.10in}  \\\\ \n" );
  fprintf( distputv, "\\multicolumn{3}{c}{ \\textbf{to the values of the $acf$'s coefficients}} \\\\ \n" );
   fprintf( distputv, "\\hline \n" );
   fprintf( distputv, "\\hline                  Coefficient  &  Dates &  Contribution \\\\ \n" );
   fprintf( distputv, "\\hline \n" );


/* Table of outliers:                                                        */


   for ( j = 1; j <= lags-(lags/3); j++ )
     {
	   AbsMax1=0;
	   AbsMax2=0;


	   if (corr[j] > (1.0 / sqrt(ser-> nobs )) )
	 {
	   for ( i = 1; i <= ser->nobs-j; i++ )
	     {

	       if ((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2)>= AbsMax1 )
		 {
		   AbsMax1 = (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
		   ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper1, &Asub1 );
		   ObsToDate( ser->begyear, ser->begtime, i+j, ser->freq, &Aper2, &Asub2 );
		 }
	     }
	   for ( i = 1; i <= ser->nobs-j; i++ )
	     {

	       if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) <  AbsMax1*0.9999 ) )		 {
		   if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) >=  AbsMax2 ) )
		     {
		       AbsMax2 = (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
		       ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper3, &Asub3 );
		       ObsToDate( ser->begyear, ser->begtime, i+j, ser->freq, &Aper4, &Asub4 );	
		   }	 
		 }
	     }	     
	   fprintf( distputv, "  $r_{%2d}$ = %2.2f(%2.2f) &", j, corr[j], (1.0 / sqrt(ser-> nobs )) );
	   if ( ser->freq == 1 )
	     {
	       fprintf( distputv, "%13d - %d &", Aper1, Aper2 );
	     }
	   else
	     {
	       fprintf( distputv, "%8d/%4d - %2d/%4d &", Asub1, Aper1, Asub2, Aper2 );
	     }
	   fprintf( distputv, "%13.3f   \\vspace{-.10in} \\\\  \n", AbsMax1 );

	   if ( ser->freq == 1 )
	     {
	       fprintf( distputv, "& %13d - %d &", Aper3, Aper4 );
	     }
	   else
	     {
	       fprintf( distputv, "& %8d/%4d - %2d/%4d &", Asub3, Aper3, Asub4, Aper4 );
	     }
	   fprintf( distputv, "%13.3f  \\vspace{-.02in} \\\\ \\hline \n", AbsMax2 );

	 }
       if ( corr[j] < -(1.0 / sqrt(ser-> nobs ))  )
	 {
	   for ( i = 1; i <= ser->nobs-j; i++ )
	     {
	       if ((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2)<= AbsMax1 )
		 {
		   AbsMax1 = (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
		   ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper1, &Asub1 );
		   ObsToDate( ser->begyear, ser->begtime, i+j, ser->freq, &Aper2, &Asub2 );
		 }
	     }
	   for ( i = 1; i <= ser->nobs-j; i++ )
	     {

	       if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) >  AbsMax1*0.9999 ) )		 {
		   if (((ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2) <=  AbsMax2 ) )
		     {
		       AbsMax2 = (ser->data[i]-rtmp1)*(ser->data[i+j]-rtmp1)/(ser->nobs*rtmp2);
		       ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper3, &Asub3 );
		       ObsToDate( ser->begyear, ser->begtime, i+j, ser->freq, &Aper4, &Asub4 );	
		   }	 
		 }
	     }	     
	   fprintf( distputv, "  $r_{%2d}$ = %2.2f(%2.2f) &", j, corr[j], (1.0 / sqrt(ser-> nobs ))  );
	   if ( ser->freq == 1 )
	     {
	       fprintf( distputv, "%13d - %d &", Aper1, Aper2 );
	     }
	   else
	     {
	       fprintf( distputv, "%8d/%4d - %2d/%4d &", Asub1, Aper1, Asub2, Aper2 );
	     }
	   fprintf( distputv, "%13.3f    \\vspace{-.10in} \\\\ \n ", AbsMax1 );

	   if ( ser->freq == 1 )
	     {
	       fprintf( distputv, " & %13d - %d & ", Aper3, Aper4 );
	     }
	   else
	     {
	       fprintf( distputv, " & %8d/%4d - %2d/%4d &", Asub3, Aper3, Asub4, Aper4 );
	     }
	   fprintf( distputv, "%13.3f  \\vspace{-.02in}   \\\\ \\hline \n  ", AbsMax2 );
	 }
     }
   for ( i = 1; i <= lags; i++ ) 
       {
       if ( fabs( corr[i] ) >= (2.0 / sqrt(ser-> nobs )) )
          Atip2 += 1;
       if ( fabs( corr[i]  ) >= (1.0 / sqrt(ser-> nobs )) )
          Atip1 += 1;
       }

   AbsMax1 = ((Atip1 * 100.0) / lags );
   AbsMax2 = ((Atip2 * 100.0) / lags );
   fprintf( distputv, "\\multicolumn{3}{c}{%16.0f values outside (-1,+1): %5.2f ", Atip1, AbsMax1 ); 
   fprintf( distputv, " \\%% (31.74  \\%% expected)}\\vspace{-.10in} \\\\ \n");

   fprintf( distputv, "\\multicolumn{3}{c}{%16.0f values outside (-2,+2): %5.2f \\%%  (4.56 \\%% expected) } \\\\\n",   Atip2, AbsMax2 );
     

   fprintf( distputv, " \\end{tabular} \\\\ \n" );

    


   fprintf( distputv, " } \n");
   fprintf( distputv, "\\end{multicols} \n");
   //   fprintf( distputv, "\\end{footnotesize}\n");
   //   fprintf( distputv, "\\end{flushleft}\n");
   //   fprintf( distputv, "\\end{landscape}\n");
   //   fprintf( distputv, "\\end{document}\n");
   fclose( distputv );





   free_vector( corr, 1, lags );
}

/*END DG *************************************************************************/


real ChiTest( real *corr, int lags, int nobs )

{
   int  i;
   real chisqr;

   chisqr = 0.0;
   for ( i = 1; i <= lags; i++ )
       chisqr += (corr[i] * corr[i]) / (nobs-i);
   chisqr *= nobs;
   chisqr *= (nobs + 2);
   return( chisqr );
}

/*****************************************************************************/

void File_StatSer( struct Tseries *ser )

{
   int  Maxy, Maxt, Miny, Mint, Aper, Asub;
   real tmp;

/* Compute sample statistics for time series serk:                           */

   ObsToDate( ser->begyear, ser->begtime, ser->nobs, ser->freq, &Aper, &Asub );
   ser->endtime = Asub;
   ser->endyear = Aper;
   ser->mean = Mean( ser->data, ser->nobs );
   tmp = Stdev( ser->data, ser->nobs );
   ser->var  = tmp * tmp;
   ser->skew = Skew( ser->data, ser->nobs );
   ser->kurt = Kurt( ser->data, ser->nobs );
   ser->jarquebera = JarqueBera (ser->skew, ser->kurt, ser->nobs );
   ser->max  = MaxVal( ser->data, ser->nobs );
   ser->min  = MinVal( ser->data, ser->nobs );

/* Write to output file:                                                     */

// fprintf( outputv, "%s", ser->name );
   fprintf( outputv, "Unconditional residuals " );
   fprintf( outputv, "(seasonal period: %d)\n", ser->freq );
   fprintf( outputv, "%d observations: ", ser->nobs );
   if ( ser->freq > 1 )
      fprintf( outputv, "from %d/%d to %d/%d\n",
               ser->begtime, ser->begyear, ser->endtime, ser->endyear );
   else
      fprintf( outputv, "from %d to %d\n", ser->begyear, ser->endyear );
   fprintf( outputv, "\n" );

   fprintf( outputv, "                  Mean: %18.6f\n", ser->mean );
   fprintf( outputv, "Standard error of mean: %18.6f\n", tmp / sqrt( ser->nobs ) );
   fprintf( outputv, "              Variance: %18.6f\n", tmp * tmp );
   fprintf( outputv, "    Standard deviation: %18.6f\n", tmp );
   fprintf( outputv, "              Skewness: %18.6f\n", ser->skew );
   fprintf( outputv, "              Kurtosis: %18.6f\n", ser->kurt );
   fprintf( outputv, "           Jarque-Bera: %18.6f\n", ser->jarquebera );


   ObsToDate( ser->begyear, ser->begtime, ser->max, ser->freq, &Maxy, &Maxt );
   ObsToDate( ser->begyear, ser->begtime, ser->min, ser->freq, &Miny, &Mint );

   if ( ser->freq > 1 )
      {
      fprintf( outputv, "               Minimum: %18.6f at %2d/%d (observation %3d)\n",
               ser->data[ser->min], Mint, Miny, ser->min );
      fprintf( outputv, "               Maximum: %18.6f at %2d/%d (observation %3d)\n",
               ser->data[ser->max], Maxt, Maxy, ser->max );
      }
   else
      {
      fprintf( outputv, "               Minimum: %18.6f at %d (observation %3d)\n",
               ser->data[ser->min], Miny, ser->min );
      fprintf( outputv, "               Maximum: %18.6f at %d (observation %3d)\n",
               ser->data[ser->max], Maxy, ser->max );
      }
   fprintf( outputv, "\n" );
}

/*****************************************************************************/

void File_PlotSer( struct Tseries *ser )

{
   int  i, Aper, Asub, itmp1, itmp2, iround( real );
   real BandPos1, BandPos2, Pos, AbsMax, HorInc, rtmp1, rtmp2, rtmp3, rtmp4;
   STRING Guions, Marcas, Tmpstr;

   Guions = NEW_STR( 80 );
   Marcas = NEW_STR( 80 );
   Tmpstr = NEW_STR( 80 );

   strcpy( Guions, "-------------+-------------------------+-------------------------+--------------" );
   strcpy( Marcas, "                                       0                          " );

   Aper = 0;
   Asub = 0;

/* Maximum value to plot (if < 2.0, then force 3.0):                         */

   itmp1 = ser->max;
   itmp2 = ser->min;
   rtmp1 = ser->data[itmp1];
   rtmp2 = ser->data[itmp2];
   rtmp3 = ser->mean;
   rtmp4 = sqrt( ser->var );

   AbsMax = fabs( (rtmp1 - rtmp3) / rtmp4 );
   if ( fabs( (rtmp2 - rtmp3) / rtmp4 ) > AbsMax )
      AbsMax = fabs( (rtmp2 - rtmp3) / rtmp4 );
   if ( AbsMax <= 2.0 ) AbsMax = 3.0;

   if ( AbsMax > 8.0 )
      {
      fprintf( outputv, "Warning: at least one observation above 8 sigmas\n" );
      goto p1;
      }

/* Serie DEGENERADA (varianza cero): no hay nada que dibujar, y ademas AbsMax   */
/* sale NaN -- 0/0 -- que NO atrapa ninguna de las dos guardas de arriba,       */
/* porque toda comparacion con NaN es falsa.  De ahi HorInc = 25/NaN,           */
/* BandPos = NaN, iround(NaN) = basura, y Tmpstr[27 +- basura] mata el proceso. */
/* Pasa de verdad: un ajuste degenerado deja los residuos a cero y el programa  */
/* muere al ir a dibujarlos (BUG-0008).  Se compara con !(x > 0) y x == x en    */
/* vez de con <= y isnan para que el NaN quede atrapado sin depender de flags   */
/* del compilador.                                                             */

   if ( !(rtmp4 > 0.0) || !(AbsMax == AbsMax) )
      {
      fprintf( outputv, "Warning: series with zero variance; plot skipped\n" );
      goto p1;
      }

/* The value of each character + positions of � and 2� bands:                */

   HorInc   = 25.0 / AbsMax;
   BandPos1 = HorInc;
   BandPos2 = 2.0 * HorInc;

   for ( i = 1; i <= 8; i++ ) if ( AbsMax >= i )
       {
       sprintf( Tmpstr, "%d", i);                                 
       Guions[39 - iround( i * HorInc )]     = '+';
       Marcas[39 - iround( i * HorInc )]     = Tmpstr[0];
       Marcas[39 - iround( i * HorInc ) - 1] = '-';
       Guions[39 + iround( i * HorInc )]     = '+';
       Marcas[39 + iround( i * HorInc )]     = Tmpstr[0];
       Marcas[39 + iround( i * HorInc ) - 1] = '+';
       }

   fprintf( outputv, "Standardized time series plot " );
   fprintf( outputv, "(original values on right-side column):\n" );
   fprintf( outputv, "\n" );
   fprintf( outputv, "%s\n", Marcas );
   fprintf( outputv, "%s\n", Guions );

/* Standardized time series plot:                                            */

   for ( i = 1; i <= ser->nobs; i++ )
       {
       if (ser->numbering == 0) fprintf( outputv, "%4d", i );
	else fprintf( outputv, "    ", i );
       ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper, &Asub );
       if ( ser->freq == 1 )
          fprintf( outputv, "%7d ", Aper );
       else
          fprintf( outputv, "%3d/%4d", Asub, Aper );
       Tmpstr[0] = '\0';
       while ( strlen( Tmpstr ) <= 54 ) strcat( Tmpstr, " " );
       if ( (ser->freq != 1) && (Asub == ser->freq) )
          {
          Tmpstr[1]  = '+';
          Tmpstr[53] = '+';
          }
       else
          {
          Tmpstr[1]  = '|';
          Tmpstr[53] = '|';
          }
       if ( fabs( (ser->data[i] - rtmp3) / rtmp4 ) >= 2.0 )
          {
          Tmpstr[0]  = '@';
          Tmpstr[54] = '@';
          }
       Pos = (ser->data[i] - rtmp3) / rtmp4 * HorInc;
       { int idx = 27 + iround( Pos );
         if ( idx >= 0 && idx <= 54 ) Tmpstr[idx] = '*'; }
       if ( Tmpstr[27] == ' ' )
          Tmpstr[27] = '|';
       /* Las bandas, con la MISMA guarda de rango que el marcador '*' de       */
       /* arriba: escribir en Tmpstr[27 +- iround(BandPos)] sin comprobar el    */
       /* indice es lo que reventaba (BUG-0008).  La guarda de la serie         */
       /* degenerada ya evita el NaN; esto es el cinturon.                      */
       { int k;
         for ( k = 0; k < 4; k++ )
             {
             real bp  = (k < 2) ? BandPos1 : BandPos2;
             int  idx = (k % 2 == 0) ? 27 + iround( bp ) : 27 - iround( bp );
             if ( idx >= 0 && idx <= 54 && Tmpstr[idx] == ' ' ) Tmpstr[idx] = ':';
             } }
       fprintf( outputv, "%s", Tmpstr );
       fprintf( outputv, "%13.10f\n", ser->data[i] );
       }
   fprintf( outputv, "%s\n", Guions );
   fprintf( outputv, "%s\n", Marcas );
   fprintf( outputv, "\n" );


/* Table of outliers:                                                        */

   fprintf( outputv, "                 +------------------------------------------+\n" );
   fprintf( outputv, "                 |       Table of standardized values       |\n" );
   fprintf( outputv, "                 |       greater than or equal to 2.0       |\n" );
   fprintf( outputv, "                 +------------------------------------------+\n" );
   fprintf( outputv, "                 |                                          |\n" );
   if (ser->numbering == 0)
   fprintf( outputv, "                 | Observation    Date   Standardized value |\n" );
   else 
   fprintf( outputv, "                 | Observation Number    Standardized value |\n" );

   fprintf( outputv, "                 |                                          |\n" );

   for ( i = 1; i <= ser->nobs; i++ )
       if ( fabs( (ser->data[i] - rtmp3) / rtmp4 ) >= 2.0 )
          {
          fprintf( outputv, "                 |" );
          if (ser->numbering == 0) fprintf( outputv, "%7d", i );
          ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper, &Asub );
          if ( ser->freq == 1 )
             fprintf( outputv, "%13d ", Aper );
          else
             fprintf( outputv, "%9d/%4d", Asub, Aper );
          if (ser->numbering == 1) fprintf( outputv, "%20.2f", (ser->data[i]-rtmp3)/rtmp4 );
          else fprintf( outputv, "%13.2f", (ser->data[i]-rtmp3)/rtmp4 );
          fprintf( outputv, "        |\n" );
          }
   fprintf( outputv, "                 +------------------------------------------+\n" );
   fprintf( outputv, "\n" );


 /* DG 16/07/04 *********************************************************************/
int Atip2;

   for ( i = 1; i <= ser->nobs; i++ )
	{
    if ( fabs( (ser->data[i] - rtmp3) / rtmp4 ) >= 3.0 )
    Atip2 += 1;
	}

   fprintf( restputv, "\\bigskip \n" );
   fprintf( restputv, "\\begin{multicols}{2}{ \n");
   if (ser->numbering == 0)
	{
        fprintf( restputv, "\\begin{tabular}{ccc}\n" );
	fprintf( restputv, "\\hline  Obs.  &  Date &  Std. Val. \\\\ \n" );	
	}
   else {
	fprintf( restputv, "\\begin{tabular}{cc}\n" );
	fprintf( restputv, "\\hline  Obs. \\#  &  Std. Val. \\\\ \n" );
	}
   fprintf( restputv, "\\hline \n" );
   Atip2 = 0;
   for ( i = 1; i <= ser->nobs; i++ )
       if ( fabs( (ser->data[i] - rtmp3) / rtmp4 ) >= 3.0 )
          {
	  Atip2 += 1;
          fprintf( restputv, "                 " );
          if (ser->numbering == 0) fprintf( restputv, " %7d &", i );
          ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper, &Asub );
          if ( ser->freq == 1 )
             fprintf( restputv, "%13d &", Aper );
          else
             fprintf( restputv, "%9d/%4d & ", Asub, Aper );
          fprintf( restputv, "%13.2f \\vspace{-.12in} \\\\ ", (ser->data[i]-rtmp3)/rtmp4 );
          fprintf( restputv, " \n" );
          if ((Atip2 == 11) && (ser->freq == 1) && (ser->nobs > 200))
		{
		fprintf( restputv, " \\end{tabular} \n" );
   		if (ser->numbering == 0) 
			{
			fprintf( restputv, "\\begin{tabular}{ccc}\n" );
			fprintf( restputv, "\\hline  Obs.  &  Date &  Std. Val. \\\\ \n" );		
			}
   		else 
			{
			fprintf( restputv, "\\begin{tabular}{cc}\n" );
			fprintf( restputv, "\\hline  Obs. \\#  &  Std. Val. \\\\ \n" );
			}
   		fprintf( restputv, "\\hline \n" );		
		}

          }
	if ((Atip2 < 22) && (ser->freq == 1) && (ser->nobs > 200))
		{
		for ( i = Atip2+1; i <= 22; i++ ){
			if (ser->numbering == 0) fprintf( restputv, "  & &  \\vspace{-.12in} \\\\ "  );
			else fprintf( restputv, "  &  \\vspace{-.12in} \\\\ " );
			fprintf( restputv, " \n" );
		}
		}	


          fprintf( restputv, " \\end{tabular} \n" );
	  fprintf( restputv, "\\\\ \n" );
          fprintf( restputv, " \\columnbreak \n");
          fprintf( restputv, " \\singlespacing \n");
          fprintf( restputv, " \\underline{\\textbf{Comments:}} \\\\ \n");
          fprintf( restputv, " \\bigskip       \n\n");

          fprintf( restputv, "%% \\underline{Action:} \n");

          fprintf( restputv, " } \n");
          fprintf( restputv, "\\end{multicols} \n");

	  // fprintf( outputv, "------------------------------------------------- \n" );



/* END DG *******************************************************************************/

p1:FREE_STR( Tmpstr );
   FREE_STR( Marcas );
   FREE_STR( Guions );

}



/*****************************************************************************/

void File_HistSer( struct Tseries *ser )

{
   const int NumFil = 17;
   const int NumCol = 64;

   int  itmp1, itmp2, Atip1, Atip2, NumCat, i, j, nphor, fmax, *freqs, *chk;
   real fmax1, rtmp1, rtmp2, xmax, num, ObsPerFil, *breakk;
   STRING *shist, *aux, base1, base2, no, yes, s1, s2;

/* Allocate workspace and initialize:                                        */

   freqs  = ivector( 1, 50 );
   chk    = ivector( 1, 50 );
   breakk = vector( 1, 50 );
   shist  = (STRING *)malloc( (size_t)(NumFil) * sizeof( STRING ) );
   aux    = (STRING *)malloc( (size_t)(NumFil) * sizeof( STRING ) );
   for ( i = 0; i <= NumFil-1; i++ )
       {
       shist[i] = NEW_STR( NumCol );
       aux[i]   = NEW_STR( NumCol );
       }
   base1 = NEW_STR( 80 );
   base2 = NEW_STR( 80 );
   no    = NEW_STR( 80 );
   yes   = NEW_STR( 80 );
   s1    = NEW_STR( 80 );
   s2    = NEW_STR( 80 );

   Atip1 = 0;
   Atip2 = 0;

/* Find maximum absolute value of standardized series:                       */

   itmp1 = ser->max;
   itmp2 = ser->min;
   rtmp1 = ser->mean;
   rtmp2 = sqrt( ser->var );

   xmax = fabs( (ser->data[itmp1]-rtmp1) / rtmp2 );
   if ( fabs( (ser->data[itmp2]-rtmp1) / rtmp2 ) > xmax )
      xmax = fabs( (ser->data[itmp2]-rtmp1) / rtmp2 );

/* Set maximum absolute value to either 4.0 or 8.0:                          */

   if ( xmax > 8.0 )
      {
      fprintf( outputv, "Warning: at least one observation above 8 sigmas\n" );
      goto h1;
      }
   xmax = ( xmax <= 4.0 ) ? 4.0 : 8.0;

/* Number of horizontal characters per category:                             */

   if ( xmax == 4.0 )
      {
      nphor = 4;
      strcpy( no, "    " );
      strcpy( yes, "...." );
      strcpy( base1, "        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+" );
      strcpy( base2, "       -4      -3      -2      -1       0      +1      +2      +3      +4" );
      }
   else
      {
      nphor = 2;
      strcpy( no, "  " );
      strcpy( yes, ".." );
      strcpy( base1, "        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+" );
      strcpy( base2, "       -8  -7  -6  -5  -4  -3  -2  -1   0  +1  +2  +3  +4  +5  +6  +7  +8" );
      }

/* Create vector of "breakpoints":                                           */

   for ( i = 1; i <= 50; i++ ) breakk[i] = 0.0;
   NumCat = 1;
   breakk[NumCat] = -xmax + 0.5;              /* Note that "bandwidth" = 0.5 */
   while ( breakk[NumCat] < xmax )
      {
      NumCat += 1;
      breakk[NumCat] = breakk[NumCat-1] + 0.5;
      }

/* Create vector of frequencies and set number of outliers:                  */

   for ( i = 1; i <= 50; i++ ) freqs[i] = 0;

   for ( i = 1; i <= ser->nobs; i++ )
       {
       num = (ser->data[i]-rtmp1) / rtmp2;
       if ( num <= breakk[1] )
          freqs[1] += 1;
       else
          for ( j = 2; j <= NumCat; j++ )
              if ( (num > breakk[j-1]) && (num <= breakk[j]) ) freqs[j] += 1;
       if ( fabs( num ) >= 2.0 )
          Atip2 += 1;
       if ( fabs( num ) >= 1.0 )
          Atip1 += 1;
       }

/* Maximum frequency = maximum to draw vertically (fills NumFil-1 rows):     */

   fmax = freqs[1];
   for ( i = 2; i <= NumCat; i++ )
       if ( freqs[i] > fmax ) fmax = freqs[i];

/* Number of observations represented by one row of dots:                    */

   fmax1     = fmax;
/* ObsPerFil = fmax1 / (NumFil - 1); (const int Numfil = 17)                 */
   ObsPerFil = fmax1 / 16.0;

/* Fill the NumFil rows that make up the histogram:                          */

   for ( j = 2; j <= NumFil; j++ )
       {
       for ( i = 1; i <= NumCat; i++ )
           if ( freqs[i] > ObsPerFil * (NumFil-j) )
              strcat( shist[j-1], yes );
           else
              strcat( shist[j-1], no );
       strcpy( s2, shist[j-1] );
       COPY_STR( s2, 0, strlen( s2 )-1, shist[j-1] );
       strcat( shist[j-1], "|" );
       }

   for ( i = 1; i <= 50; i++ ) chk[i] = 0;

   for ( j = 2; j <= NumFil; j++ )
       for ( i = 1; i <= NumCat; i++ )
           if ( (freqs[i] > ObsPerFil * (NumFil-j)) && (chk[i] == 0) )
              {
              if ( nphor == 2 )
                 {
                 sprintf(s1, "%d", freqs[i]); 
                 if ( strlen( s1 ) == 1 ) strcat( s1, " " );
                 }
              else
                 {
                 sprintf(s1, "%d", freqs[i]); 
                 if ( strlen( s1 ) == 2 )
                    {
                    strcpy( s2, " " );
                    strcat( s1, s2 );
                    strcat( s2, s1 );
                    strcpy( s1, s2 );
                    }
                 else if ( strlen( s1 ) == 1 )
                    {
                    strcpy( s2, " " );
                    strcat( s1, s2 );
                    strcpy( s2, "  " );
                    strcat( s2, s1 );
                    strcpy( s1, s2 );
                    }
                 else if ( strlen( s1 ) == 3 )
                    {
                    strcpy( s2, " " );
                    strcat( s1, s2 );
                    }
                 }
              strcat( aux[j-2], s1 );
              chk[i] = 1;
              }
           else
              strcat( aux[j-2], no );

   strcpy( shist[0], aux[0] );
   shist[0][NumCol-1] = '|';
   shist[0][NumCol]   = '\0';

   for ( j = 2; j <= NumFil-1; j++ )
       for ( i = 1; i <= strlen( aux[j-1] ); i++ )
           if ( aux[j-1][i-1] != ' ' ) shist[j-1][i-1] = aux[j-1][i-1];

/* Write to output file:                                                     */

   fprintf( outputv, "Standardized time series histogram:\n" );
   fprintf( outputv, "\n" );
/*
   for ( i = 1; i <= NumCat; i++ )
       {
       fprintf( outputv, " Breakpoint[%2d] = %5.1f", i, breakk[i] );
       fprintf( outputv, " Frequency[%2d] = %4d\n", i, freqs[i] );
       }
   fprintf( outputv, "\n" );
*/
   fprintf( outputv, "%s\n", base2 );
   fprintf( outputv, "%s\n", base1 );
   for ( i = 1; i <= NumFil; i++ )
       fprintf( outputv, "        |%s\n", shist[i-1] );
   fprintf( outputv, "%s\n", base1 );
   fprintf( outputv, "%s\n", base2 );
   fprintf( outputv, "\n" );

   fprintf( outputv, "%16d values outside (-1,+1): %5.2f %% (31.74 %% expected)\n",
            Atip1, (Atip1 * 100.0) / ser->nobs );
   fprintf( outputv, "%16d values outside (-2,+2): %5.2f %% ( 4.56 %% expected)\n",
            Atip2, (Atip2 * 100.0) / ser->nobs );

   fprintf( outputv, "\n" );

h1:FREE_STR( s2 );
   FREE_STR( s1 );
   FREE_STR( yes );
   FREE_STR( no );
   FREE_STR( base2 );
   FREE_STR( base1 );
   for ( i = NumFil-1; i >= 0; i-- )
       {
       FREE_STR( aux[i] );
       FREE_STR( shist[i] );
       }
   free( (FREE_ARG)aux );
   free( (FREE_ARG)shist );
   free_vector( breakk, 1, 50 );
   free_ivector( chk, 1, 50 );
   free_ivector( freqs, 1, 50 );
}

/*****************************************************************************/

void File_CorrSer( struct Tseries *ser, int npar )

{
   real *corr;
   int  lags;

   void PlotCor( real *, int, int, struct Tseries *, int );

   if ( ser->nobs < 3 * (ser->freq + 1) )
      lags = ser->nobs - ser->freq / 2;
   /* DG 290708 ***************/
   else if ((ser->freq==1) && (ser->nobs>200)) lags =45;
   else if (ser->freq==1) lags =9;
     
   /*********************************/
   else
      lags = 3 * (ser->freq + 1);

   corr = vector( 1, lags );

   Acf( ser, lags, corr );
   PlotCor( corr, lags, 1, ser, npar );
   Pacf( lags, corr );
   PlotCor( corr, lags, 0, ser, npar );
/* DG 06/28/04 **************************************************************/
      Acf_dist( ser, lags );
/****************************************************************************/
   free_vector( corr, 1, lags );
}

/*****************************************************************************/

void PlotCor( real *corr, int lags, int isacf, struct Tseries *ser, int npar )

{
   int  nobs, freq, i, j, posi, symbol, iround( real );
   real pos, HorInc;
   STRING Guions, Marcas, TmpStr;


   nobs = ser->nobs;
   freq = ser->freq;

   Guions = NEW_STR( 80 );
   Marcas = NEW_STR( 80 );
   TmpStr = NEW_STR( 80 );

   strcpy( Guions, "-------------+-------------------------+-------------------------+--------------" );
   HorInc = 25.0;
   if ( isacf )
      {
      strcpy( Marcas, "            -1                         0                         1  L-B Q  DF" );
      fprintf( outputv, "Autocorrelation function (acf " );
      }
   else
      {
      strcpy( Marcas, "            -1                         0                         1" );
      fprintf( outputv, "Partial autocorrelation function (pacf " );
      }
   fprintf( outputv, "bands = +- %5.3f):\n", 2.0 / sqrt( nobs ) );
   fprintf( outputv, "\n" );
   fprintf( outputv, "%s\n", Marcas );
   fprintf( outputv, "%s\n", Guions );

   for ( i = 1; i <= lags; i++ )
       {
       TmpStr[0] = '\0';
       while ( strlen( TmpStr ) <= 52 ) strcat( TmpStr, " " );
       if ( (freq != 1) && ( i % freq == 0) )
          {
          fprintf( outputv, "%4d %7.3f +", i, corr[i] );
          symbol     = '+';
          TmpStr[51] = '+';
          }
       else
          {
          fprintf( outputv, "%4d %7.3f |", i, corr[i] );
          symbol     = '*';
          TmpStr[51] = '|';
          }
       pos  = corr[i] * HorInc;
       /* Una correlacion NO FINITA -- la de una serie de varianza cero, 0/0 --
          da posi basura, y el bucle de abajo escribe una tirada de esa longitud
          fuera de TmpStr: el compilador la convierte en memset y el proceso
          muere (BUG-0008).  Una correlacion vive en [-1,1], asi que 25 es el
          maximo legitimo; se acota y se sigue dibujando.                      */
       if ( !(pos == pos) ) pos = 0.0;
       posi = abs( iround( pos ) );
       if ( posi > 25 ) posi = 25;
       if ( pos <= 0.0 )
          for ( j = 25 - posi; j <= 25; j++ ) TmpStr[j] = symbol;
       else
          for ( j = 25; j <= 25 + posi; j++ ) TmpStr[j] = symbol;
       TmpStr[25] = '|';
       pos  = 2.0 / sqrt( nobs ) * HorInc;
       posi = iround( pos );
       if ( posi >= 0 && posi <= 25 )
          {
          if ( TmpStr[25 + posi] == ' ' ) TmpStr[25 + posi] = ':';
          if ( TmpStr[25 - posi] == ' ' ) TmpStr[25 - posi] = ':';
          }
       fprintf( outputv, "%s", TmpStr );

       if ( (freq != 1) && (i % freq == 0) && (isacf) && (i - npar >= 1) )
          {
          fprintf( outputv, "%6.2f ", ChiTest( corr, i, nobs ) );
          fprintf( outputv, "%3d", i - npar );
          }
       if ( (i % freq != 0) && (isacf) && (i == lags) && (i - npar >= 1) )
          {
          fprintf( outputv, "%6.2f ", ChiTest( corr, i, nobs ) );
          fprintf( outputv, "%3d", i - npar );
          }
       if ( (freq == 1) && (isacf) && (i == lags) && (i - npar >= 1) )
          {
          fprintf( outputv, "%6.2f ", ChiTest( corr, i, nobs ) );
          fprintf( outputv, "%3d", i - npar );
          }
       fprintf( outputv, "\n" );

       }

   fprintf( outputv, "%s\n", Guions );
   fprintf( outputv, "%s\n", Marcas );
   fprintf( outputv, "\n" );


   FREE_STR( TmpStr );
   FREE_STR( Marcas );
   FREE_STR( Guions );
}

/*****************************************************************************/



/* File_PlotSer_X11() (the residual graph with gnuplot, no longer called) was
 * removed in fue 1.14: the graph is fp_PlotSer_CorrSer() of fugplot.c.     */

/*****************************************************************************/
/*
double Acf_Pacf_Max ( struct Tseries *ser,  int lags )

{
int i;
double cmax, *corr;
corr = vector( 1, lags );

Acf( ser, lags, corr );

 cmax=0.40;

 for ( i = 1; i <= lags; i++ )
      if ( fabs(corr[i]) >= cmax )
          {
          cmax = fabs(corr[i]);
          }

Pacf_New (ser->data ,   ser->nobs,  corr,  lags);

 for ( i = 1; i <= lags; i++ )
      if ( fabs(corr[i]) >= cmax )
          {
          cmax = fabs(corr[i]);
          }


 if ((cmax > 0.40) & (cmax <= 0.60))
      cmax = 0.60;
 if ((cmax > 0.60) & (cmax <= 0.80))
      cmax = 0.80;
 if ((cmax > 0.80) & (cmax <= 1))
      cmax=1;

return (cmax);

}
*/

void Pacf_New (double *ser , int nobs, double *pcorr, int lags)
{
int  i, j, k;
double chisq;
double *corr;
gsl_matrix *X, *cov;
gsl_vector *y, *c;
gsl_multifit_linear_workspace * work;

corr  = vector( 1, lags );
/* The first coeficient for the acf and pacf are the same */
corr[1] = pcorr [1];

/*The rest of the coeficients of the pacf will be calculated employed regretions */
for ( k = 2; k <= lags; k++ )
 	{
  	if ( ( nobs - k -1 ) > k ) /* Degrees of freedom  condition */
		{
		
		y     =   gsl_vector_alloc (  nobs - k  );                            /* y    = dependent variable */
		X     =   gsl_matrix_alloc ( nobs - k ,  k + 1  );                    /* X    = vector of independent variables */
                work  =   gsl_multifit_linear_alloc ( nobs - k,  k + 1  );            /* work = workspace for gsl procedure */
		c     =   gsl_vector_alloc ( k + 1 );                                 /* c    = parameters vector */
                cov   =   gsl_matrix_alloc ( k + 1 , k + 1  );                        /* cov  = covariance matrix */

		for ( i = 0; i <  nobs - k ; i++ ){
			gsl_vector_set (y, i,  ser [i + 1 + k] );                     /* Allocate Y vector */

			for ( j = 0; j <  k + 1; j++) {                               /* Allocate X matrix */
				if (j == 0 ) gsl_matrix_set (X, i, j, 1.0);           
				else gsl_matrix_set (X, i, j, ser[ k + 1 - j + i ] );

				}

			}
/*The best-fit is found by singular value decomposition of the matrix X using the modified Golub-Reinsch SVD algorithm, with column scaling to improve the accuracy of the singular values. Any components which have zero singular value (to machine precision) are discarded from the fit. See for more details:
http://www.gnu.org/software/gsl/manual/html_node/Multi_002dparameter-fitting.html
*/
		gsl_multifit_linear ( X,  y,  c,  cov, &chisq,  work);
		corr[k] = gsl_vector_get (c, k);


		gsl_matrix_free ( cov ); 
		gsl_vector_free ( c );
		gsl_multifit_linear_free (work);
		gsl_matrix_free ( X ); 
		gsl_vector_free ( y );

		}
	else nrerror("There isn't enough observations to calculate so many lags");
	}

for ( i = 1; i <= lags; i++ ) pcorr[i] =  corr[i];
}
