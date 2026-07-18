/*****************************************************************************/
/*  fuf_graphic.c                                                            */
/*  Modulo GRAFICO de prevision, IMPORTADO VERBATIM de fuf-1.08.1 (usfo.c):  */
/*  forecast_graphic / forecast_graphic_BC.  Dibujan, via la interfaz        */
/*  gnuplot_i, la variacion anual (historia + prevision +/- 1 DT) y los      */
/*  residuos (ERR) con los formatos ya trabajados de fuf.  No reimplementar. */
/*  Copyright (C) 1995-1996 J.A. Mauricio; 2009-2026 A.B. Treadway &         */
/*  D.E. Guerrero.  GNU GPL v2+.                                             */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "main.h"          /* vector, free_vector, ObsToDate */
#include "gnuplot_i.h"

void forecast_graphic ( double *data, double **res, double **f3, double ***v3, int ornsop, double sigma2, int begyear, int begtime, int nobs, int L, int freq, char *x11out, double refactor )

{
int i, Asub1, Aper1, prevby, previndex;
double AbsMax, prevcmax;
double *y, *y1,*y2, *a;
gnuplot_ctrl*h2;
h2=gnuplot_init();

y  = vector (0, 2*L);
y1 = vector (0, 2*L);
y2 = vector (0, 2*L);
a  = vector (0, L-1);

//   for ( i = nobs - L/2; i <= nobs; i++ )   /* Load data and errors:     */


       for(i=0; i < L; i++)
	 {
	   y[i]= 100*(data[i + 1 + nobs - L] - data[i + 1 + nobs - L - freq])/refactor;	    
	   y1[i]= 0.0;
	   y2[i]= 0.0;
	   a[i]= 100*(res[1][(i+1) +(nobs - ornsop - L)])/refactor;
	 }

       for(i=0; i < L; i++)
	 {
	   y[i+L]= 100*f3[1][i+1]/refactor;
	   y1[i+L]= 100*(f3[1][i+1]+sqrtl( v3[i+1][1][1] ))/refactor;
	   y2[i+L]= 100*(f3[1][i+1]-sqrtl( v3[i+1][1][1] ))/refactor;
	 } 


//for(i=0; i < 2*L; i++) printf( "%g   %g    %g \n \n", y[i], y1[i], y2[i]);

/* [1.0]  Configure options for both graphs    */
gnuplot_cmd(h2,"set terminal postscript eps enhance 'Helvetica' 24") ;
  ObsToDate( begyear, begtime, nobs+1, freq, &Aper1, &Asub1 );
gnuplot_cmd(h2, "set output \"prev%s.%d%d.eps\"", x11out, Asub1, Aper1 );
gnuplot_cmd(h2,"set size .9,1.8");

//gnuplot_cmd(h2,"set origin 0.05,0.05");
gnuplot_cmd(h2,"set origin 0.02,0");
gnuplot_cmd(h2,"set multiplot"); /* double graph option */
gnuplot_cmd(h2,"set datafile missing \"NaN\"");



/* [1.1] Forecasts series graph */

/* [1.1.1] Configure options for forecast series graph */

gnuplot_cmd(h2,"set size .9,.9");
gnuplot_cmd(h2,"set origin .016,.89");
gnuplot_setstyle(h2,"linespoints");
gnuplot_cmd(h2,"set pointsize .85");
gnuplot_cmd(h2,"set lmargin 10");
/*gnuplot_cmd(h2,"set title 'TLV anual (%%)' -13.05,.35 font 'bold,32'");*/
gnuplot_cmd(h2,"set title 'LRC anual (%%)' offset -13.05,.35 font 'bold,32'");
gnuplot_cmd(h2,"set border 3 lw 1.6");
gnuplot_cmd(h2,"set xtics  1");
gnuplot_cmd(h2,"set format x \" \"");
gnuplot_cmd(h2,"set style line 1 lt 2 lw 1.4");
gnuplot_cmd(h2,"set style line 2 lt 1 lw 2.8");
gnuplot_cmd(h2,"set style line 3 lt 2 lw 1.5"); 
gnuplot_cmd(h2,"set style line 4 lt 1 lw 2.5");
gnuplot_cmd(h2,"set style line 5 lt 1 lw 9"); 
gnuplot_cmd(h2,"set grid xtics lt 1");
gnuplot_cmd(h2,"set xtics nomirror ");
gnuplot_cmd(h2,"set mxtics %d", freq);
gnuplot_cmd(h2,"set tics out");
gnuplot_cmd(h2,"set ytics nomirror");
/*gnuplot_cmd(h2,"set ticscale 1 .8"); */
gnuplot_cmd(h2,"set tics scale 0.8");
gnuplot_cmd(h2,"set ticslevel  .4");


/* [1.1.2] Allocate X tics for forecast series graph */

for(i=nobs-L+1; i <= nobs-L+freq; i++)
  {
    ObsToDate( begyear, begtime, i, freq, &Aper1, &Asub1 );
    if (Asub1 == 1)
     {
	prevby=Aper1;
	previndex=i-(nobs-L+1);
      }
  }

if (freq == 12)gnuplot_cmd(h2,"set xtics (\"%d\" %d,\"%d\" %d,\"%d\" %d,\"%d\" %d,\"%d\" %d )", prevby, previndex, prevby+1, previndex+ freq,prevby+2, previndex+(2*freq), prevby+3, previndex+3*freq, prevby+4, previndex+4*freq, prevby+5, previndex+5*freq );
if (freq == 4)gnuplot_cmd(h2,"set xtics (\"%d\" %d,\"%d\" %d,\"%d\" %d,\"%d\" %d,\"%d\" %d )", prevby, previndex, prevby+2, previndex+ 2*freq, prevby+4, previndex+(4*freq), prevby+6, previndex+6*freq, prevby+8, previndex+8*freq, prevby+10, previndex+10*freq );
if (freq == 1) gnuplot_cmd(h2,"set xtics (\"%d\" %d,\"%d\" %d,\"%d\" %d,\"%d\" %d,\"%d\" %d )", prevby, previndex, prevby+10, previndex+ 10*freq, prevby+20, previndex+(20*freq), prevby+30, previndex+30*freq, prevby+40, previndex+40*freq, prevby+50, previndex+50*freq );


/* [1.1.3] Make plot: forecast series graph */

gnuplot_plot_df(h2, y, y1, y2, (2*L), L, "") ;

/* [1.2.1] Configure options for ERR series graph */

gnuplot_setstyle(h2,"impulses");
gnuplot_cmd(h2,"set border 3 lw 1.6");
gnuplot_cmd(h2,"set size .615,.9");
gnuplot_cmd(h2,"set origin  0.0,0.0");
//gnuplot_cmd(h2,"set origin -0.0,-0.09");
gnuplot_cmd(h2,"set title 'ERR' offset -8.5,0.35 font 'bold,32'");
/*gnuplot_cmd(h2,"set title 'Errores' -8.5,0.35 font 'bold,32'");*/
 gnuplot_cmd(h2,"set ytics %.1f", 2*sqrt(sigma2));

/* [1.2.2] Determinate maximun Y Value in ERR */

 prevcmax= 4*sqrt (sigma2);
 
for(i=0; i < L; i++)
 {
   if( fabs(a[i]) >= prevcmax ) prevcmax = fabs(a[i]);
 }

 if ((prevcmax > 4*sqrt (sigma2)) & (prevcmax <= 6*sqrt (sigma2))) prevcmax = 6*sqrt (sigma2);
 if ((prevcmax > 6*sqrt (sigma2)) & (prevcmax <= 7*sqrt (sigma2))) prevcmax = 7*sqrt (sigma2); 
 if (prevcmax > 7*sqrt (sigma2)) prevcmax= 10*sqrt (sigma2);

/* [1.2.3] Make plot: ERR series graph */

 gnuplot_plot_err(h2, a, L, sqrt(sigma2), prevcmax+.10*sqrt(sigma2), "");

/* [1.3] Close gnuplot */


free_vector( y, 0, 2*L );
free_vector( y1, 0, 2*L );
free_vector( y2, 0, 2*L );
free_vector( a, 0, L-1 );
gnuplot_close(h2);
}

void forecast_graphic_BC ( double *data, double **res, double **f1, double ***v1, int ornsop, double sigma, int begyear, int begtime, int nobs, int L, int freq, double boxlam, char *x11out, double refactor )

{
int i, Asub1, Aper1, prevby, previndex;
double AbsMax, prevcmax;
double *y, *y1,*y2, *a;
gnuplot_ctrl*h2;
h2=gnuplot_init();

y  = vector (0, 2*L);
y1 = vector (0, 2*L);
y2 = vector (0, 2*L);
a  = vector (0, L-1);

//   for ( i = nobs - L/2; i <= nobs; i++ )   /* Load data and errors:     */


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
	   y1[i+L]= pow ((((f1[1][i+1]- 2*sqrt ( v1[i+1][1][1] ))/refactor )*boxlam + 1)  , (1/boxlam));
	   y2[i+L]=  pow ((((f1[1][i+1]+ 2*sqrt ( v1[i+1][1][1] ))/refactor )*boxlam + 1)  , (1/boxlam));
	 } 
	 
    AbsMax=4*sigma;   

   for ( i = 0; i < L; i++ )
       if ( fabs(a[i]) >= AbsMax )
          {
          AbsMax = fabs(a[i]);
          }


   if ((AbsMax > 4*sigma) & (AbsMax <= 6*sigma))
      AbsMax = 6;
   if ((AbsMax > 6*sigma) & (AbsMax <= 7*sigma))
      AbsMax=7;
   if ((AbsMax > 7*sigma) & (AbsMax <= 10*sigma))
      AbsMax=12*sigma;	 


//for(i=0; i < 2*L; i++) printf( "%g   %g    %g \n \n", y[i], y1[i], y2[i]);

/* [1.0]  Configure options for both graphs    */
gnuplot_cmd(h2,"set terminal postscript eps enhance 'Helvetica' 24") ;
  ObsToDate( begyear, begtime, nobs+1, freq, &Aper1, &Asub1 );
gnuplot_cmd(h2, "set output \"prev%s.%d%d.eps\"", x11out, Asub1, Aper1 );
gnuplot_cmd(h2,"set size .9,1.8");
gnuplot_cmd(h2,"set origin 0.02,0");
gnuplot_cmd(h2,"set multiplot"); /* double graph option */
gnuplot_cmd(h2,"set datafile missing \"NaN\"");



/* [1.1] Forecasts series graph */

/* [1.1.1] Configure options for forecast series graph */

gnuplot_cmd(h2,"set size .9,.9");
gnuplot_cmd(h2,"set origin .016,.94");
gnuplot_setstyle(h2,"linespoints");
gnuplot_cmd(h2,"set pointsize .85");
gnuplot_cmd(h2,"set lmargin 10");
/*gnuplot_cmd(h2,"set title 'TLV anual (%%)' -13.05,.35 font 'bold,32'");*/
gnuplot_cmd(h2,"set title 'LEVEL' offset -13.05,.35 font 'bold,32'");
gnuplot_cmd(h2,"set border 3 lw 1.6");
gnuplot_cmd(h2,"set xtics  1");
gnuplot_cmd(h2,"set format x \" \"");
gnuplot_cmd(h2,"set style line 1 lt 2 lw 1.4");
gnuplot_cmd(h2,"set style line 2 lt 1 lw 2.8");
gnuplot_cmd(h2,"set style line 3 lt 2 lw 1.5"); 
gnuplot_cmd(h2,"set style line 4 lt 1 lw 2.5");
gnuplot_cmd(h2,"set style line 5 lt 1 lw 9"); 
gnuplot_cmd(h2,"set style line 6 lt 5 lw 1"); 
/*gnuplot_cmd(h2,"set style line 6 lt 7");  /*/
gnuplot_cmd(h2,"set grid xtics lt 1");
gnuplot_cmd(h2,"set xtics nomirror ");
gnuplot_cmd(h2,"set mxtics %d", freq);
gnuplot_cmd(h2,"set tics out");
gnuplot_cmd(h2,"set ytics nomirror");
gnuplot_cmd(h2,"set ticscale 1 .8");
gnuplot_cmd(h2,"set ticslevel  .4");


/* [1.1.2] Allocate X tics for forecast series graph */

for(i=nobs-L+1; i <= nobs-L+freq; i++)
  {
    ObsToDate( begyear, begtime, i, freq, &Aper1, &Asub1 );
    if (Asub1 == 1)
     {
	prevby=Aper1;
	previndex=i-(nobs-L+1);
      }
  }

if (freq == 12)gnuplot_cmd(h2,"set xtics (\"%d\" %d,\"%d\" %d,\"%d\" %d,\"%d\" %d,\"%d\" %d )", prevby, previndex, prevby+1, previndex+ freq,prevby+2, previndex+(2*freq), prevby+3, previndex+3*freq, prevby+4, previndex+4*freq, prevby+5, previndex+5*freq );
if (freq == 4)gnuplot_cmd(h2,"set xtics (\"%d\" %d,\"%d\" %d,\"%d\" %d,\"%d\" %d,\"%d\" %d )", prevby, previndex, prevby+2, previndex+ 2*freq, prevby+4, previndex+(4*freq), prevby+6, previndex+6*freq, prevby+8, previndex+8*freq, prevby+10, previndex+10*freq );
if (freq == 1) gnuplot_cmd(h2,"set xtics (\"%d\" %d,\"%d\" %d,\"%d\" %d,\"%d\" %d,\"%d\" %d )", prevby, previndex, prevby+10, previndex+ 10*freq, prevby+20, previndex+(20*freq), prevby+30, previndex+30*freq, prevby+40, previndex+40*freq, prevby+50, previndex+50*freq );


/* [1.1.3] Make plot: forecast series graph */

gnuplot_plot_df(h2, y, y1, y2, (2*L), L, "") ;

/* [1.2.1] Configure options for ERR series graph */

gnuplot_setstyle(h2,"impulses");
gnuplot_cmd(h2,"set border 3 lw 1.6");
gnuplot_cmd(h2,"set size .615,.9");
gnuplot_cmd(h2,"set origin -0.0,-0.09");
gnuplot_cmd(h2,"set title 'ERR (\%)' -8.5,0.35 font 'bold,32'");
/*gnuplot_cmd(h2,"set title 'Errores' -8.5,0.35 font 'bold,32'");*/
gnuplot_cmd(h2,"set ytics %.1f", 2*sigma );

/* [1.2.2] Determinate maximun Y Value in ERR */



/* [1.2.3] Make plot: ERR series graph */

 gnuplot_plot_err(h2, a, L-1, sigma, AbsMax, "");

/* [1.3] Close gnuplot */


free_vector( y, 0, 2*L );
free_vector( y1, 0, 2*L );
free_vector( y2, 0, 2*L );
free_vector( a, 0, L-1 );
gnuplot_close(h2);
}
