
/*****************************************************************************/
/*  DIAGNOSE.C                                                               */
/*  Sample statistics and plots for a single time series.                    */
/*  Copyright (C) Jose Alberto Mauricio, 1996. (except where indicated)       */
/*****************************************************************************/

#include "fug.h"            /* Header file (prototype declarations)        */

extern FILE *outputv;         /* Output file (global: declared in FUG.C)     */

/*****************************************************************************/

/* ObsToDate and DateToObs live in lib/dates (the copy here was identical). */

/*****************************************************************************/


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

return ( nobs / 6.0 * ( Skew*Skew + Kurt*Kurt/4));

}

/*****************************************************************************/

void Acf( struct Tseries *ser, int lags, real *corr )

{
   int  i, j;
   real rtmp1, rtmp2;

   for ( i = 1; i <= lags; i++ ) corr[i] = 0.0;

   rtmp1 = ser->mean;
   rtmp2 = ser->var;

   for ( j = 1; j <= lags; j++ ) for ( i = 1; i <= ser->nobs-j; i++ )
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
       MatPacf[i][i] = ( fabs( 1.0-sum2 ) > 1.0e-12 ) ? (corr[i]-sum1) / (1.0-sum2) : 0.0;
       for ( j = 1; j <= i-1; j++ )
           MatPacf[i][j] = MatPacf[i-1][j] - MatPacf[i][i] * MatPacf[i-1][i-j];
       }
   for ( i = 1; i <= lags; i++ ) pcorr[i] = MatPacf[i][i];

   free_vector( corr, 1, lags );
   free_matrix( MatPacf, 1, lags, 1, lags );
}

/*****************************************************************************/

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

 /*  fprintf( outputv, "%s", ser->name ); */
   fprintf( outputv, "\n\nTransformed Time Series Data " );
   fprintf( outputv, "(seasonal period: %d)\n", ser->freq );
   fprintf( outputv, "%d observations: ", ser->nobs );
   if ( ser->freq > 1 )
      fprintf( outputv, "from %d/%d to %d/%d\n",
               ser->begtime, ser->begyear, ser->endtime, ser->endyear );
   else
      fprintf( outputv, "from %d to %d\n", ser->begyear, ser->endyear );
   fprintf( outputv, "\n" );

/*  debug lines  
   int i;
   for (i=1; i<=ser->nobs; i++)
   fprintf( outputv, "%13.10f\n", ser->data[i] );


  */
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

   if ( rtmp4 <= 0.0 )
      {
      fprintf( outputv, "Warning: the series is constant (standard deviation 0): no plot\n" );
      goto p1;
      }

   AbsMax = fabs( (rtmp1 - rtmp3) / rtmp4 );
   if ( fabs( (rtmp2 - rtmp3) / rtmp4 ) > AbsMax )
      AbsMax = fabs( (rtmp2 - rtmp3) / rtmp4 );
   if ( AbsMax <= 2.0 ) AbsMax = 3.0;

   if ( AbsMax > 8.0 )
      {
      fprintf( outputv, "Warning: at least one observation above 8 sigmas\n" );
      goto p1;
      }

/* The value of each character + positions of sigma and 2 sigma bands:      */

   HorInc   = 25.0 / AbsMax;
   BandPos1 = HorInc;
   BandPos2 = 2.0 * HorInc;

   for ( i = 1; i <= 8; i++ ) if ( AbsMax >= i )
       {
/*   itoa( i, Tmpstr, 10 );                                     */
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
       fprintf( outputv, "%4d", i );
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
          Tmpstr[0]  = '>';
          Tmpstr[54] = '<';
          }
       Pos = (ser->data[i] - rtmp3) / rtmp4 * HorInc;
       Tmpstr[27 + iround( Pos )] = '*';
       if ( Tmpstr[27] == ' ' )
          Tmpstr[27] = '|';
       if ( Tmpstr[27 + iround( BandPos1 )] == ' ' )
          Tmpstr[27 + iround( BandPos1 )] = ':';
       if ( Tmpstr[27 - iround( BandPos1 )] == ' ' )
          Tmpstr[27 - iround( BandPos1 )] = ':';
       if ( Tmpstr[27 + iround( BandPos2 )] == ' ' )
          Tmpstr[27 + iround( BandPos2 )] = ':';
       if ( Tmpstr[27 - iround( BandPos2 )] == ' ' )
          Tmpstr[27 - iround( BandPos2 )] = ':';
       fprintf( outputv, "%s", Tmpstr );
       fprintf( outputv, "%13.10f\n", ser->data[i] );
       }
   fprintf( outputv, "%s\n", Guions );
   fprintf( outputv, "%s\n", Marcas );
   fprintf( outputv, "\n" );

//   h=gnuplot_init();
//     for ( i = 1; i <= ser->nobs; i++ )
//         gnuplot_cmd(h, "plot '%s'", ser->data[i] );


/* Table of outliers:                                                        */

   fprintf( outputv, "                 +------------------------------------------+\n" );
   fprintf( outputv, "                 |       Table of standardized values       |\n" );
   fprintf( outputv, "                 |       greater than or equal to 2.0       |\n" );
   fprintf( outputv, "                 +------------------------------------------+\n" );
   fprintf( outputv, "                 |                                          |\n" );
   fprintf( outputv, "                 | Observation    Date   Standardized value |\n" );
   fprintf( outputv, "                 |                                          |\n" );

   for ( i = 1; i <= ser->nobs; i++ )
       if ( fabs( (ser->data[i] - rtmp3) / rtmp4 ) >= 2.0 )
          {
          fprintf( outputv, "                 |" );
          fprintf( outputv, "%7d", i );
          ObsToDate( ser->begyear, ser->begtime, i, ser->freq, &Aper, &Asub );
          if ( ser->freq == 1 )
             fprintf( outputv, "%13d ", Aper );
          else
             fprintf( outputv, "%9d/%4d", Asub, Aper );
          fprintf( outputv, "%13.2f", (ser->data[i]-rtmp3)/rtmp4 );
          fprintf( outputv, "        |\n" );
          }
   fprintf( outputv, "                 +------------------------------------------+\n" );
   fprintf( outputv, "\n" );

 
p1:FREE_STR( Tmpstr );
   FREE_STR( Marcas );
   FREE_STR( Guions );

}

/*****************************************************************************/
void DesvMed (struct Tseries *ser,  int nog)
{

   const int NumFil = 17;
   const int NumCol = 40;

   int   i, f, ng , *PosX, *PosY;
   int Aper, Asub, Aper1, Asub1;
   real MaxRange, MaxMean, AbsMax, *ms, *dts;
   STRING *RmPlt ,  Tmpstr;
   if ( nog < 1 ) nog = 1;
   ng=  ser->nobs/nog;
   if ( ng < 2 )
      {
      fprintf( outputv, "\nMean-Standard Deviation Plot: not available (less than 2 groups of %d observations)\n\n", nog );
      return;
      }
/* Allocate workspace and initialize:                                        */

   PosX  = ivector( 1, ng );
   PosY    = ivector( 1, ng );
   ms = vector( 1, ng );
   dts = vector( 1, ng );
    RmPlt   = (STRING *)malloc( (size_t)(NumFil) * sizeof( STRING ) );
 
   for ( i = 0; i <= NumFil-1; i++ )
       {
       RmPlt [i] = NEW_STR( NumCol );
 
       }
   Tmpstr = NEW_STR( 80 );


   if ( meandv(  ser->data, dts, ms, nog, ng ) != 0 )
      {
      fprintf( outputv, "\nMean-Standard Deviation Plot: not available (the group means or standard\n"
                        "deviations are all equal, or there are less than 2 groups of %d observations)\n\n", nog );
      goto d1;
      }

   AbsMax = 0.0;
   for ( i = 1; i <= ng; i++ )
	{
        if ( fabs( ms[i]) >= AbsMax ) AbsMax =  fabs( ms[i]);
	if ( fabs( dts[i]) >= AbsMax ) AbsMax =  fabs( dts[i]);
	}
   AbsMax += .1*AbsMax;

  MaxRange = 2*AbsMax / NumFil; /* 15 = Characteres in vertical axis */
  MaxMean  = 2*AbsMax / NumCol;/* 37 = Characteres in horizontal axis */

/*fprintf( outputv, "%f %f  %f\n\n",  AbsMax,  MaxRange,  MaxMean  ); */

  for (i = 1; i <= ng; i++ )
      {
       PosX [i] =   iround (( ms[i] + AbsMax ) / MaxMean )     ;
       PosY [i] =   iround ((AbsMax - dts[i]  ) /  MaxRange )  ;
      }

/* Debug lines 
for ( i = 1; i <= ng; i++ )
	{
fprintf( outputv, "%d  %d\n",  PosY[i],   PosX[i] );
        }
                 */


  for (i = 0; i < NumFil; i++ ) 
       {
       RmPlt[i][0] = '\0';
       Tmpstr[0] = '\0';
       
		for (f = 1; f <= ng; f++ )
			{
			if ( i == (PosY [f]-1)) {
                          while ( strlen( Tmpstr ) <= (size_t)PosX[f] ) strcat( Tmpstr, " " );
			  Tmpstr [ PosX[f] ]  = '*';
			  while ( strlen( Tmpstr ) <=  NumCol-2 ) strcat( Tmpstr, " " );
//                          strcat( Tmpstr, "|" );
                          strcpy( RmPlt[i], Tmpstr);
                         }
			}
       }

  fprintf( outputv, "\n\n" );
  fprintf( outputv,"Mean-Standard Deviation Plot Standardized, Observations Number for Each Group: %d \n\n", nog);
  fprintf( outputv,"                      %.1f +-------------------+-------------------+\n", AbsMax);
  for (i = 0; i <  NumFil; i++ ) {
  if (i==8) {RmPlt[i][NumCol-1] = '+';}
  else {RmPlt[i][NumCol-1] = '|';}
  
  RmPlt[i][NumCol] = '\0';

  if (i==8) {fprintf( outputv, " Standard Deviation   0.0 +");}
       else {fprintf( outputv, "                          |");}
  if (strlen(RmPlt[i]) > 1) fprintf( outputv, "%s\n",  RmPlt[i] );
  else  fprintf( outputv,"                                       |\n");

  }
  fprintf( outputv,"                     -%.1f +-------------------+-------------------+\n", AbsMax);
  fprintf( outputv,"                        -%.1f                 0.0                 %.1f\n", AbsMax, AbsMax);
  fprintf( outputv,"                                             Mean                    '\n");
/* */ 
fprintf( outputv, "\n\n" );
if ( ser->freq == 1 )
   {
   fprintf( outputv, "                 +--------------------------------------------------------+\n" );
   fprintf( outputv, "                 |              Table of standardized values              |\n" );
   fprintf( outputv, "                 |   for each Mean and correspondent Standard Deviation   |\n" );
   fprintf( outputv, "                 +--------------------------------------------------------+\n" );
   fprintf( outputv, "                 |                                                        |\n" );
   fprintf( outputv, "                 |   Group        Dates       Mean      Standard          |\n" );
   fprintf( outputv, "                 |                                      Deviation         |\n" );
   fprintf( outputv, "                 +--------------------------------------------------------+\n" );
   }
else   {
   fprintf( outputv, "                 +--------------------------------------------------------+\n" );
   fprintf( outputv, "                 |              Table of standardized values              |\n" );
   fprintf( outputv, "                 |   for each Mean and correspondent Standard Deviation   |\n" );
   fprintf( outputv, "                 +--------------------------------------------------------+\n" );
   fprintf( outputv, "                 |                                                        |\n" );
   fprintf( outputv, "                 |   Group          Dates            Mean      Standard   |\n" );
   fprintf( outputv, "                 |                                             Deviation  |\n" );
   fprintf( outputv, "                 +--------------------------------------------------------+\n" );
   }


   for ( i = 1; i <= ng; i++ )
	{
          fprintf( outputv, "                 |" );
          fprintf( outputv, "%7d", i );
          ObsToDate( ser->begyear, ser->begtime, i*nog-(nog-1), ser->freq, &Aper, &Asub );
          ObsToDate( ser->begyear, ser->begtime, i*nog, ser->freq, &Aper1, &Asub1 );
          if ( ser->freq == 1 )
             fprintf( outputv, "%9d - %3d", Aper, Aper1 );
          else
             fprintf( outputv, "%7d/%4d - %2d/%4d", Asub, Aper, Asub1, Aper1 );

          fprintf( outputv, "      %4.1f        %4.1f ",  ms[i],   dts[i] );
if ( ser->freq == 1 )          fprintf( outputv, "           |\n" );
else fprintf( outputv, "    |\n" );
	}
   fprintf( outputv, "                 +--------------------------------------------------------+\n" );
   fprintf( outputv, "\n\n" );

d1:FREE_STR(  Tmpstr );
   for ( i = NumFil-1; i >= 0; i-- )
       {
 
       FREE_STR( RmPlt [i] );
       }
 
   free( (FREE_ARG)RmPlt  );
   free_vector( dts, 1, ng );
   free_vector( ms, 1, ng );
   free_ivector( PosY, 1, ng );
   free_ivector( PosX, 1, ng );

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

   if ( rtmp2 <= 0.0 )
      {
      fprintf( outputv, "Warning: the series is constant (standard deviation 0): no histogram\n" );
      goto h1;
      }
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
/*               itoa( freqs[i], s1, 10 );                */
                 sprintf(s1, "%d", freqs[i]);
                 if ( strlen( s1 ) == 1 ) strcat( s1, " " );
                 }
              else
                 {
/*               itoa( freqs[i], s1, 10 );                  */
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
/*            A count wider than its cell (100+ with nphor = 2, 10000+ with 4)
              wrote past the row: heap overflow (drvarma BUGS.md, found in
              drvec). The cell keeps its width and says the count did not fit. */
              if ( (int) strlen( s1 ) > nphor )
                 {
                 memset( s1, '*', nphor );
                 s1[nphor] = '\0';
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
       for ( i = 1; i <= (int)strlen( aux[j-1] ); i++ )
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

void File_CorrSer( struct Tseries *ser, int npar, int lags )

{
   real *corr;
//   int  lags;

   void PlotCor( real *, int, int, struct Tseries *, int );

   if ( ser->var <= 0.0 )
      {
      fprintf( outputv, "Warning: the series is constant (variance 0): no acf/pacf\n\n" );
      return;
      }
   if ( lags <= 0 ) lags = default_lags( ser->nobs, ser->freq );
   if ( lags > ser->nobs - 2 ) lags = ser->nobs - 2;

   corr = vector( 1, lags );

   Acf( ser, lags, corr );
   PlotCor( corr, lags, 1, ser, npar );
   Pacf( lags, corr );
   PlotCor( corr, lags, 0, ser, npar );

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
   fprintf( outputv, "bands = +/- %5.3f):\n", 2.0 / sqrt( nobs ) );
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
       posi = abs( iround( pos ) );
       if ( pos <= 0.0 )
          for ( j = 25 - posi; j <= 25; j++ ) TmpStr[j] = symbol;
       else
          for ( j = 25; j <= 25 + posi; j++ ) TmpStr[j] = symbol;
       TmpStr[25] = '|';
       pos  = 2.0 / sqrt( nobs ) * HorInc;
       posi = iround( pos );
       if ( TmpStr[25 + posi] == ' ' )
          TmpStr[25 + posi] = ':';
       if ( TmpStr[25 - posi] == ' ' )
          TmpStr[25 - posi] = ':';
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

int annual_step( int years, double width )

/* Years between the vertical lines and labels of annual data: every 5 years
 * (as GraphMaker), or 10, 20, 25, 50... when the 4-digit labels would not
 * fit. years: span of the x axis, width: its length in points.              */
{
   static const int steps[] = { 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000 };
   int i;

   if ( years < 1 ) years = 1;
   for ( i = 0; i < (int)(sizeof( steps ) / sizeof( steps[0] )); i++ )
       if ( steps[i] * width / years >= 28.0 ) return( steps[i] );
   return( 1000 );
}

int first_multiple( int year, int step )

/* First multiple of step that is >= year (also for negative years) */
{
   int r = ((year % step) + step) % step;

   return( (r == 0) ? year : year + step - r );
}

/*****************************************************************************/

int default_lags( int nobs, int freq )

/* Default number of lags of the acf and pacf (same rule for the .out file,
 * the plots and the GUI). At most nobs - 2 lags are used.                   */
{
   int lags;

   if ( nobs < 3 * (freq + 1) )
      lags = nobs - freq / 2;
   else if ( (freq == 1) && (nobs > 200) )
      lags = 45;
   else if ( freq == 1 )
      lags = 9;
   else
      lags = 3 * (freq + 1);
   if ( lags > nobs - 2 ) lags = nobs - 2;
   if ( lags < 1 ) lags = 1;
   return( lags );
}

/*****************************************************************************/

/*****************************************************************************/

int meandv(double *y,double *dts, double *ms, int nog, int ng)
/* input  --> double *y = data vector */
/* output <-- double *dts = standarized standard deviation vector */
/* output <-- double *ms  = standarized means vector */
/* input  --> int nog = number observation per group */
/* input  --> int ng = number groups         */
{
int i=1,j,h=1;  //define variables de tipo entero
double dta,mea; //define variables de tipo double

double *bb = vector( 1, nog ); //define vectores double
double *m  = vector( 1, ng  );
double *dt = vector( 1, ng  );

while (i <= ng )//para todos los grupos
        {
        for( j=1; j<=nog; j++ )//coja "nog" observaciones y las deposite en "bb"
                {
                bb[j]=y[h];
                h=h+1;
                }
        mea   = Mean (bb, nog); //calcule la media de "bb"
        dta   = Stdev (bb, nog); //calcule la desviación típica de "bb"
        m[i]  = mea;//deposite la media del grupo "i" en la posición "i" del vector "m"
        dt[i] = dta;//deposite la desviación típica del grupo "i" en la posición "i" del vector "dt"
        i=i+1;
        }


double mean_m = Mean( m, ng ), std_m = Stdev( m, ng );
double mean_dt = Mean( dt, ng ), std_dt = Stdev( dt, ng );
int status = 0;

if ( ng < 2 || std_m <= 0.0 || std_dt <= 0.0 )  /* can not standardize */
   status = 1;
else
   for ( i = 1; i <= ng; i++ )
	{
         ms[i] = (m[i] - mean_m)/ std_m;
         dts[i] =(dt[i] - mean_dt)/ std_dt;
	}

free_vector( bb, 1, nog );
free_vector( m, 1, ng );
free_vector( dt, 1, ng );
return( status );
}
