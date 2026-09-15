/*****************************************************************************/
/* plotsupport.c -- the functions fugplot.c needs that fue did not have      */
/* (they are in diagnose.c of fug): the axes of annual data, the default    */
/* number of lags and the groups of the mean - standard deviation graph.    */
/* Copyright (C) 2009-2026 A.B. Treadway & D.E. Guerrero. GPL v2 or later.   */
/*****************************************************************************/

#include "plothost.h"

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
