/***************************************************************************
 *   Copyright (C) 2009 by Arthur B. Treadway and David E. Guerrero        *
 *    davidesg@ucm.es                                                      *
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


#include <math.h>
#include "fuf.h"                  /* Header file (prototype declarations). */
#include "nlatools.h"             /* Header file (prototype declarations)  */
#include "gnuplot_i.h"            /* gnuplot interface                     */
#include "usfo.h"

double macheps;                      /* Machine epsilon: global variable.   */
FILE *outputv;                       /* Output file: global variable.         */
FILE *texputv;


/*****************************************************************************/

struct Tusmodel Tm;                /* These variables interface with (*cast) */
struct Tseries Ts;                 /* CAUTION: do not declare here variables */
double **DataMat;                  /* whose names are equal to the  names of */
                                   /* variables declared in main or (*cast)! */

/*****************************************************************************/
/* forecast module global variables:                                         */
struct Forecast Fs;
FILE *prevputv;
/*****************************************************************************/



int main( int argc, char *argv[] )

{
   const  double PI = 3.141592654;
   const  int  NT = 10;                  /* Maximum number of non-standard   */
                                         /* detvars (handle otherwise).      */
   STRING inputf, outputf, texputf, x11out, dvif, namef, dumstrg;
   int    outyear;
   FILE   *inputv;
   FILE   *dviv;

/* The following variables have to do with the time series model and data:   */

   struct Tvarma varma1;                 /* Standard VARMA structure.        */
   struct Tseries res;                   /* Tseries structure for residuals. */
   int  nstdet, *det, met, chk, hdm;
   int  i1, i2, i3, i4;
   double r1;
   char command[256];

   //latex_ctrl*h;
   //dvips_ctrl*h1;

   void cast_us( double *, struct Tvarma *, int *, int, int );
   void CalcNonsOp( int, int, int, int *, int, double * );

/* The following variables have to do with the quasi-Newton optimizer:       */

   double *x, *dev, **cov, gradtol, steptol;
   int  npar, nparma, maxits, nrits, ifault, i, j;

/*****************************************************************************/
/* [1]: Check and process command-line arguments:                            */
/*****************************************************************************/

   printf( "\n" );
   printf( "FUF 1.08: Copyright (C) 2026 Arthur B. Treadway and David E. Guerrero \n" );
   printf( "Non-Final version. May contain errors. Please report\n" );
   printf( "\n" );

/* printf( "INITIAL RAM: %lu\n\n", coreleft() );                             */
   inputf   = NEW_STR( 4096 );
   outputf  = NEW_STR( 4096 );
   x11out   = NEW_STR( 4096 );
   texputf  = NEW_STR( 4096 );
   dvif     = NEW_STR( 4096 );
   namef    = NEW_STR( 4096 );
   Tm.residuals = NEW_STR( 4096 );
   dumstrg  = NEW_STR( MAXSTR );

   if ( argc == 1 )                     /* No command-line arguments (help): */
      {
      printf( "fuf input [eml|aml] [chk|nochk]\n");
      printf( "input      : model-data file name (omit extension .inp)\n" );
      printf( "[eml|aml]  : exact | approximate maximum likelihood (default: eml)\n" );
      printf( "[chk|nochk]: check | do not check for invertibility (default: chk)\n" );
      exit( 1 );
      }

   if ( argc >= 2 )                     /* Process command-line arguments:   */
      {
      strcpy( inputf, argv[1] );        /* Model and data file name.         */
      met = 1;                          /* Default estimation method (eml).  */
      chk = 1;                          /* Default checking strategy (chk).  */
      hdm = 0;                          /* No high definition module         */

      for ( i = 2; i <= argc-1; i++ )
          if ( strcmp( argv[i], "eml" ) == 0 )
             met = 1;
          else if ( strcmp( argv[i], "aml" ) == 0 )
             met = 0;
          else if ( strcmp( argv[i], "chk" ) == 0 )
             chk = 1;
          else if ( strcmp( argv[i], "nochk" ) == 0 )
             chk = 0;

      strcpy( outputf, inputf );
      strcpy( x11out, inputf );
      strcpy( texputf, inputf );
      strcpy( dvif, inputf );
      strcat( inputf, ".inp" );
      strcat( outputf, ".out" );
      strcat( texputf, ".tex" );
      strcat( dvif, ".dvi" );



      printf( "Input file             : %s\n", inputf );
      printf( "Output file            : %s\n", outputf );
      printf( "Estimation method      : " );
      if ( met == 1 )
         printf( "exact maximum likelihood\n" );
      else
         printf( "approximate maximum likelihood\n" );
      printf( "Check for invertibility: " );
      if ( chk == 1 )
         printf( "constrained search\n" );
      else
         printf( "unconstrained search\n" );
      }

/*****************************************************************************/
/* [2]: Open output and texput files for writing:                                        */
/*****************************************************************************/

   if ( NULL == (outputv = fopen( outputf, "w" )) )
      {
      printf( "\nError opening output file: %s\n", outputf );
      printf( "... Exiting to system ...\n" );
      exit( 1 );
      }
   fprintf( outputv, "\n" );

   if ( NULL == (texputv = fopen( texputf, "w" )) )
      {
      printf( "\nError opening output file: %s\n", texputf );
      printf( "... Exiting to system ...\n" );
      exit( 1 );
      }
   fprintf( texputv, "\n" );






/*****************************************************************************/
/* [3]: Read input file (allocate memory - read model + data - update npar): */
/*****************************************************************************/

   if ( NULL == (inputv = fopen( inputf, "r" )) )
      {
      printf( "\nError opening input file: %s\n", inputf );
      printf( "... Exiting to system ...\n" );
      exit( 1 );
      }

   npar   = 0;                /* Total number of parameters:                 */
   nparma = 0;                /* Number of parameters of the ARMA structure: */

/* [3.0]: Read the first six lines (may contain anything):                   */

   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );

/* [3.1]: Read seasonal period, number of observations and starting date:    */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%s\n", dumstrg );
    if ( strcmp( dumstrg, "number" ) == 0 ) {
	Ts.freq = 1;
	Ts.numbering = 1;
	}
    else {
	sscanf( dumstrg, "%u", &Ts.freq); 
	Ts.numbering = 0;
	}
     
/*   fscanf( inputv, "%d\n", &Ts.freq ); */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Ts.nobs );
   if ( Ts.freq > 1 )
      {
      fscanf( inputv, "%d", &Ts.begtime );
      fscanf( inputv, "%d", &Ts.begyear );
      fscanf( inputv, "%s", namef );
      fscanf( inputv, "%s\n", Tm.residuals );
      }
   else
      {
      fscanf( inputv, "%d", &outyear );
      fscanf( inputv, "%d", &Ts.begyear );
      fscanf( inputv, "%s", namef );
      fscanf( inputv, "%s\n", Tm.residuals );
      Ts.begtime = 1;
      }

   Ts.data = vector( 1, Ts.nobs );                      /* Time series data: */
   
/* []: read Lead time and estimated innovation variance:                     */
   
   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Fs.L );
   fscanf( inputv, "%lf\n", &Fs.sigma2 );

/* [3.2]: Read deterministic structure of time series model:                 */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d\n", &Tm.NdetVar );               /* N� of detvars:    */

   DataMat = matrix( 0, Tm.NdetVar, 1, Ts.nobs + Fs.L );       /* Working data set: */

   if ( Tm.NdetVar > 0 )
      {

      fgets( dumstrg, MAXSTR, inputv );

   /* [3.2.0]: Read and generate list of detvars (store as DataMat):         */

      nstdet = 0;                   /* Non-standard detvars (read from input */
      det    = ivector( 1, NT );    /* file as indexed by det):              */

      for ( i = 1; i <= Tm.NdetVar; i++ )
          {
          fscanf( inputv, "%s", dumstrg );
          for ( j = 1; j <= Ts.nobs + Fs.L ; j++ ) DataMat[i][j]  = 0.0;

   /* Generate a unit impulse:                                               */

          if ( strcmp( dumstrg, "impulse" ) == 0 )
             {
             if ( Ts.freq == 1)
                {
                i1 = 1;
                fscanf( inputv, "%d\n", &i2 );
                }
             else
                {
                fscanf( inputv, "%d", &i1 );
                fscanf( inputv, "%d\n", &i2 );
                }
             DateToObs( Ts.begyear, Ts.begtime, i2, i1, Ts.freq, &i3 );
             if ( (i3 >= 1) && (i3 <= Ts.nobs + Fs.L) ) DataMat[i][i3] = 1.0;
             }

   /* Generate a unit compensated impulse:                                   */

          else if ( strcmp( dumstrg, "compimp" ) == 0 )
             {
             if ( Ts.freq == 1)
                {
                i1 = 1;
                fscanf( inputv, "%d\n", &i2 );
                }
             else
                {
                fscanf( inputv, "%d", &i1 );
                fscanf( inputv, "%d\n", &i2 );
                }
             DateToObs( Ts.begyear, Ts.begtime, i2, i1, Ts.freq, &i3 );
             if ( (i3 >= 1) && (i3 <= Ts.nobs + Fs.L) )   DataMat[i][i3]   =  1.0;
             if ( (i3 >= 0) && (i3+1 <= Ts.nobs + Fs.L) ) DataMat[i][i3+1] = -1.0;
             }

   /* Generate a unit step:                                                  */

          else if ( strcmp( dumstrg, "step" ) == 0 )
             {
             if ( Ts.freq == 1)
                {
                i1 = 1;
                fscanf( inputv, "%d\n", &i2 );
                }
             else
                {
                fscanf( inputv, "%d", &i1 );
                fscanf( inputv, "%d\n", &i2 );
                }
             DateToObs( Ts.begyear, Ts.begtime, i2, i1, Ts.freq, &i3 );
             if ( (i3 >= 1) && (i3 <= Ts.nobs + Fs.L) )
                for ( j = i3; j <= Ts.nobs + Fs.L; j++ ) DataMat[i][j] = 1.0;
             }

   /* Generate a unit ramp:                                                  */

          else if ( strcmp( dumstrg, "ramp" ) == 0 )
             {
             if ( Ts.freq == 1)
                {
                i1 = 1;
                fscanf( inputv, "%d\n", &i2 );
                }
             else
                {
                fscanf( inputv, "%d", &i1 );
                fscanf( inputv, "%d\n", &i2 );
                }
             DateToObs( Ts.begyear, Ts.begtime, i2, i1, Ts.freq, &i3 );
             if ( (i3 >= 1) && (i3 <= Ts.nobs + Fs.L) )
                for ( j = i3; j <= Ts.nobs + Fs.L; j++ ) DataMat[i][j] = j - i3 + 1;
             }

   /* Generate a variable representing the Easter holiday:                   */

          else if ( (strcmp( dumstrg, "easter" ) == 0) && (Ts.freq == 12) )
             {
             for ( j = 1; j <= Ts.nobs + Fs.L; j++ )
                 {
                 ObsToDate( Ts.begyear, Ts.begtime, j, Ts.freq, &i1, &i2 );
                 Easter( &i3, &i4, i1 );
                 if ( (i4 == 4) && (i2 == i4) && (i3 >= 4) )
                    DataMat[i][j] = 1.0;
                 else if ( (i4 == 4) && (i2 == i4) && (i3 < 4) )
                    {
                    DataMat[i][j] = 0.5;
                    if ( j > 1 )
                       DataMat[i][j-1] = 0.5;
                    }
                 else if ( (i4 == 3) && (i2 == i4) )
                    DataMat[i][j] = 1.0;
                 }
             fscanf( inputv, "\n" );
             }

   /* Generate a deterministic linear trend:                                 */

          else if ( strcmp( dumstrg, "trend" ) == 0 )
             {
             for ( j = 1; j <= Ts.nobs + Fs.L; j++ ) DataMat[i][j] = j;
             fscanf( inputv, "\n" );
             }

   /* Generate a cosine seasonal component:                                  */

          else if ( strcmp( dumstrg, "cos" ) == 0 )
             {
             fscanf( inputv, "%lf\n", &r1 );
             for ( j = 1; j <= Ts.nobs + Fs.L; j++ )
                 DataMat[i][j] = cos( 2.0 * PI * r1 / Ts.freq * j );
             }

   /* Generate a sine seasonal component:                                    */

          else if ( strcmp( dumstrg, "sin" ) == 0 )
             {
             fscanf( inputv, "%lf\n", &r1 );
             for ( j = 1; j <= Ts.nobs+ Fs.L; j++ )
                 DataMat[i][j] = sin( 2.0 * PI * r1 / Ts.freq * j );
             }

   /* Generate an "alternator" seasonal component:                           */

          else if ( strcmp( dumstrg, "alter" ) == 0 )
             {
             for ( j = 1; j <= Ts.nobs + Fs.L; j++ ) DataMat[i][j] = pow( -1.0, j );
             fscanf( inputv, "\n" );
             }

   /* Read non-standard (unknown) deterministic variable from input file:    */

          else
             {
             nstdet += 1;          /* Update number of non-standard detvars: */
             det[nstdet] = i;
             fgets( dumstrg, MAXSTR, inputv );
             }
          }

   /* [3.2.1]: Allocate workspace for omegas and deltas:                     */

      Tm.Nomega = ivector( 1, Tm.NdetVar );
      Tm.Omega  = (double **)calloc((size_t)(Tm.NdetVar + 1), sizeof(double *));
      Tm.Imega  = (int **)calloc((size_t)(Tm.NdetVar + 1), sizeof(int *));
      Tm.Ndelta = ivector( 1, Tm.NdetVar );
      Tm.Delta  = (double **)calloc((size_t)(Tm.NdetVar + 1), sizeof(double *));
      Tm.Ielta  = (int **)calloc((size_t)(Tm.NdetVar + 1), sizeof(int *));

   /* [3.2.2]: Read omegas for each deterministic variable (if any):         */

      fgets( dumstrg, MAXSTR, inputv );
      for ( i = 1; i <= Tm.NdetVar; i++ )
          fscanf( inputv, "%d", &Tm.Nomega[i] );
      fscanf( inputv, "\n" );

      for ( i = 1; i <= Tm.NdetVar; i++ )
          {
          Tm.Omega[i] = vector( 0, Tm.Nomega[i] );
          Tm.Imega[i] = ivector( 0, Tm.Nomega[i] );
          fgets( dumstrg, MAXSTR, inputv );
          for ( j = 0; j <= Tm.Nomega[i]; j++ )
              {
              fscanf( inputv, "%lf", &Tm.Omega[i][j] );
              fscanf( inputv, "%d\n", &Tm.Imega[i][j] );
              if ( Tm.Imega[i][j] == 1 ) npar += 1;
              }
          }

   /* [3.2.3]: Read deltas for each deterministic variable (if any):         */

      fgets( dumstrg, MAXSTR, inputv );
      for ( i = 1; i <= Tm.NdetVar; i++ )
          fscanf( inputv, "%d", &Tm.Ndelta[i] );
      fscanf( inputv, "\n" );

      for ( i = 1; i <= Tm.NdetVar; i++ ) if ( Tm.Ndelta[i] > 0 )
          {
          Tm.Delta[i] = vector( 1, Tm.Ndelta[i] );
          Tm.Ielta[i] = ivector( 1, Tm.Ndelta[i] );
          fgets( dumstrg, MAXSTR, inputv );
          for ( j = 1; j <= Tm.Ndelta[i]; j++ )
              {
              fscanf( inputv, "%lf", &Tm.Delta[i][j] );
              fscanf( inputv, "%d\n", &Tm.Ielta[i][j] );
              if ( Tm.Ielta[i][j] == 1 ) npar += 1;
              }
          }
      }

/* [3.3.1]: Read number and order for each regular AR factor:                */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumAr1 );
   if ( Tm.NumAr1 > 0 )
      {
      Tm.p1  = ivector( 1, Tm.NumAr1 );
      Tm.Ar1 = (double **)calloc((size_t)(Tm.NumAr1 + 1), sizeof(double *));
      Tm.Ia1 = (int **)calloc((size_t)(Tm.NumAr1 + 1), sizeof(int *));
      }
   for ( i = 1; i <= Tm.NumAr1; i++ )
       fscanf( inputv, "%d", &Tm.p1[i] );
   fscanf( inputv, "\n" );

/* [3.3.2]: Read phis for each regular AR factor (if any):                   */

   for ( i = 1; i <= Tm.NumAr1; i++ )
       {
       Tm.Ar1[i] = vector( 0, Tm.p1[i] );
       Tm.Ia1[i] = ivector( 0, Tm.p1[i] );
       fgets( dumstrg, MAXSTR, inputv );
       for ( j = 1; j <= Tm.p1[i]; j++ )
           {
           fscanf( inputv, "%lf", &Tm.Ar1[i][j] );
           fscanf( inputv, "%d\n", &Tm.Ia1[i][j] );
           if ( Tm.Ia1[i][j] == 1 )
              {
              npar   += 1;
              nparma += 1;
              }
           }
       }


/*DG 06/21/04 ****************************************************/

/* [3.3.13]: Read number and frequency for seasonal AR factor:   */
/*
   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumAr1S );
   if ( Tm.NumAr1S > 0 )
      {
      Tm.pSre1 = vector( 1, Tm.NumAr1S );
      Tm.Ar1S  = (double **)calloc((size_t)(Tm.NumAr1S + 1), sizeof(double *));
      Tm.Ia1S  = ivector( 1, Tm.NumAr1S );
      }
      for ( i = 1; i <= Tm.NumAr1S; i++ )
        fscanf( inputv, "%lf", &Tm.pSre1[i] );
   fscanf( inputv, "\n" );

/* [3.3.14]: Read theta for each seasonal MA factor (if any):     */
/*
   for ( i = 1; i <= Tm.NumAr1S; i++ )
       {
       Tm.Ar1S[i] = vector( 0, Ts.freq-1 );
       fgets( dumstrg, MAXSTR, inputv );
       fscanf( inputv, "%lf", &Tm.Ar1S[i][Ts.freq-1] );
       fscanf( inputv, "%d\n", &Tm.Ia1S[i] );
       if ( Tm.Ia1S[i] == 1 )
          {
          npar   += 1;
          nparma += 1;
          }
       }

/* end DG ********************************************************************/

/* [3.3.3]: Read number and order for each annual AR factor:                 */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumAr2 );
   if ( Tm.NumAr2 > 0 )
      {
      Tm.p2  = ivector( 1, Tm.NumAr2 );
      Tm.Ar2 = (double **)calloc((size_t)(Tm.NumAr2 + 1), sizeof(double *));
      Tm.Ia2 = (int **)calloc((size_t)(Tm.NumAr2 + 1), sizeof(int *));
      }
   for ( i = 1; i <= Tm.NumAr2; i++ )
       fscanf( inputv, "%d", &Tm.p2[i] );
   fscanf( inputv, "\n" );

/* [3.3.4]: Read phis for each annual AR factor (if any):                    */

   for ( i = 1; i <= Tm.NumAr2; i++ )
       {
       Tm.Ar2[i] = vector( 0, Tm.p2[i] );
       Tm.Ia2[i] = ivector( 0, Tm.p2[i] );
       fgets( dumstrg, MAXSTR, inputv );
       for ( j = 1; j <= Tm.p2[i]; j++ )
           {
           fscanf( inputv, "%lf", &Tm.Ar2[i][j] );
           fscanf( inputv, "%d\n", &Tm.Ia2[i][j] );
           if ( Tm.Ia2[i][j] == 1 )
              {
              npar   += 1;
              nparma += 1;
              }
           }
       }

/* [3.3.5]: Read number and order for each regular MA factor:                */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumMa1 );
   if ( Tm.NumMa1 > 0 )
      {
      Tm.q1  = ivector( 1, Tm.NumMa1 );
      Tm.Ma1 = (double **)calloc((size_t)(Tm.NumMa1 + 1), sizeof(double *));
      Tm.Im1 = (int **)calloc((size_t)(Tm.NumMa1 + 1), sizeof(int *));
      }
   for ( i = 1; i <= Tm.NumMa1; i++ )
       fscanf( inputv, "%d", &Tm.q1[i] );
   fscanf( inputv, "\n" );

/* [3.3.6]: Read thetas for each regular MA factor (if any):                 */

   for ( i = 1; i <= Tm.NumMa1; i++ )
       {
       Tm.Ma1[i] = vector( 0, Tm.q1[i] );
       Tm.Im1[i] = ivector( 0, Tm.q1[i] );
       fgets( dumstrg, MAXSTR, inputv );
       for ( j = 1; j <= Tm.q1[i]; j++ )
           {
           fscanf( inputv, "%lf", &Tm.Ma1[i][j] );
           fscanf( inputv, "%d\n", &Tm.Im1[i][j] );
           if ( Tm.Im1[i][j] == 1 )
              {
              npar   += 1;
              nparma += 1;
              }
           }
       }


/*DG 06/20/04 ****************************************************/

/* [3.3.13]: Read number and frequency for seasonal MA factor:   */
/*
   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumMa1S );
   if ( Tm.NumMa1S > 0 )
      {
      Tm.qSre1 = vector( 1, Tm.NumMa1S );
      Tm.Ma1S  = (double **)calloc((size_t)(Tm.NumMa1S + 1), sizeof(double *));
      Tm.Im1S  = ivector( 1, Tm.NumMa1S );
      }
      for ( i = 1; i <= Tm.NumMa1S; i++ )
        fscanf( inputv, "%lf", &Tm.qSre1[i] );
   fscanf( inputv, "\n" );

/* [3.3.14]: Read theta for each seasonal MA factor (if any):     */
/*
   for ( i = 1; i <= Tm.NumMa1S; i++ )
       {
       Tm.Ma1S[i] = vector( 0, Ts.freq-1 );
       fgets( dumstrg, MAXSTR, inputv );
       fscanf( inputv, "%lf", &Tm.Ma1S[i][Ts.freq-1] );
       fscanf( inputv, "%d\n", &Tm.Im1S[i] );
       if ( Tm.Im1S[i] == 1 )
          {
          npar   += 1;
          nparma += 1;
          }
       }

/* end DG *************************************************************/

/* [3.3.7]: Read number and order for each annual MA factor:                 */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumMa2 );
   if ( Tm.NumMa2 > 0 )
      {
      Tm.q2  = ivector( 1, Tm.NumMa2 );
      Tm.Ma2 = (double **)calloc((size_t)(Tm.NumMa2 + 1), sizeof(double *));
      Tm.Im2 = (int **)calloc((size_t)(Tm.NumMa2 + 1), sizeof(int *));
      }
   for ( i = 1; i <= Tm.NumMa2; i++ )
       fscanf( inputv, "%d", &Tm.q2[i] );
   fscanf( inputv, "\n" );

/* [3.3.8]: Read thetas for each annual MA factor (if any):                  */

   for ( i = 1; i <= Tm.NumMa2; i++ )
       {
       Tm.Ma2[i] = vector( 0, Tm.q2[i] );
       Tm.Im2[i] = ivector( 0, Tm.q2[i] );
       fgets( dumstrg, MAXSTR, inputv );
       for ( j = 1; j <= Tm.q2[i]; j++ )
           {
           fscanf( inputv, "%lf", &Tm.Ma2[i][j] );
           fscanf( inputv, "%d\n", &Tm.Im2[i][j] );
           if ( Tm.Im2[i][j] == 1 )
              {
              npar   += 1;
              nparma += 1;
              }
           }
       }

/* [3.3.9]: Read number and frequency for each regular f-fixed AR factor:    */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumAr1f );
   if ( Tm.NumAr1f > 0 )
      {
      Tm.pfre1 = vector( 1, Tm.NumAr1f );
      Tm.Ar1f  = (double **)calloc((size_t)(Tm.NumAr1f + 1), sizeof(double *));
      Tm.Ia1f  = ivector( 1, Tm.NumAr1f );
      }
   for ( i = 1; i <= Tm.NumAr1f; i++ )
       fscanf( inputv, "%lf", &Tm.pfre1[i] );
   fscanf( inputv, "\n" );

/* [3.3.10]: Read 2nd phi for each regular f-fixed AR factor (if any):       */

   for ( i = 1; i <= Tm.NumAr1f; i++ )
       {
       Tm.Ar1f[i] = vector( 0, 2 );
       fgets( dumstrg, MAXSTR, inputv );
       fscanf( inputv, "%lf", &Tm.Ar1f[i][2] );
       fscanf( inputv, "%d\n", &Tm.Ia1f[i] );
       if ( Tm.Ia1f[i] == 1 )
          {
          npar   += 1;
          nparma += 1;
          }
       }

/* [3.3.11]: Read number and frequency for each annual f-fixed AR factor:    */
/*
   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumAr2f );
   if ( Tm.NumAr2f > 0 )
      {
      Tm.pfre2 = vector( 1, Tm.NumAr2f );
      Tm.Ar2f  = (double **)calloc((size_t)(Tm.NumAr2f + 1), sizeof(double *));
      Tm.Ia2f  = ivector( 1, Tm.NumAr2f );
      }
   for ( i = 1; i <= Tm.NumAr2f; i++ )
       fscanf( inputv, "%lf", &Tm.pfre2[i] );
   fscanf( inputv, "\n" );

/* [3.3.12]: Read 2nd phi for each annual f-fixed AR factor (if any):        */
/*
   for ( i = 1; i <= Tm.NumAr2f; i++ )
       {
       Tm.Ar2f[i] = vector( 0, 2 );
       fgets( dumstrg, MAXSTR, inputv );
       fscanf( inputv, "%lf", &Tm.Ar2f[i][2] );
       fscanf( inputv, "%d\n", &Tm.Ia2f[i] );
       if ( Tm.Ia2f[i] == 1 )
          {
          npar   += 1;
          nparma += 1;
          }
       }

/* [3.3.13]: Read number and frequency for each regular f-fixed MA factor:   */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumMa1f );
   if ( Tm.NumMa1f > 0 )
      {
      Tm.qfre1 = vector( 1, Tm.NumMa1f );
      Tm.Ma1f  = (double **)calloc((size_t)(Tm.NumMa1f + 1), sizeof(double *));
      Tm.Im1f  = ivector( 1, Tm.NumMa1f );
      }
   for ( i = 1; i <= Tm.NumMa1f; i++ )
       fscanf( inputv, "%lf", &Tm.qfre1[i] );
   fscanf( inputv, "\n" );

/* [3.3.14]: Read 2nd theta for each regular f-fixed MA factor (if any):     */

   for ( i = 1; i <= Tm.NumMa1f; i++ )
       {
       Tm.Ma1f[i] = vector( 0, 2 );
       fgets( dumstrg, MAXSTR, inputv );
       fscanf( inputv, "%lf", &Tm.Ma1f[i][2] );
       fscanf( inputv, "%d\n", &Tm.Im1f[i] );
       if ( Tm.Im1f[i] == 1 )
          {
          npar   += 1;
          nparma += 1;
          }
       }

/* [3.3.15]: Read number and frequency for each annual f-fixed MA factor:    */
/*
   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d", &Tm.NumMa2f );
   if ( Tm.NumMa2f > 0 )
      {
      Tm.qfre2 = vector( 1, Tm.NumMa2f );
      Tm.Ma2f  = (double **)calloc((size_t)(Tm.NumMa2f + 1), sizeof(double *));
      Tm.Im2f  = ivector( 1, Tm.NumMa2f );
      }
   for ( i = 1; i <= Tm.NumMa2f; i++ )
       fscanf( inputv, "%lf", &Tm.qfre2[i] );
   fscanf( inputv, "\n" );

/* [3.3.16]: Read 2nd theta for each annual f-fixed MA factor (if any):      */
/*
   for ( i = 1; i <= Tm.NumMa2f; i++ )
       {
       Tm.Ma2f[i] = vector( 0, 2 );
       fgets( dumstrg, MAXSTR, inputv );
       fscanf( inputv, "%lf", &Tm.Ma2f[i][2] );
       fscanf( inputv, "%d\n", &Tm.Im2f[i] );
       if ( Tm.Im2f[i] == 1 )
          {
          npar   += 1;
          nparma += 1;
          }
       }

/* [3.4]: Read mean parameter (with flag):                                   */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%lf\n", &Tm.mu );
   fscanf( inputv, "%d\n", &Tm.Imu );
   if ( Tm.Imu == 1 ) npar += 1;

/* [3.5]: Read Box-Cox lambda and differences (regular and complete annual): */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%lf", &Tm.boxlam );
   fscanf( inputv, "%d", &Tm.nrdiff );
   fscanf( inputv, "%d\n", &Tm.nadiff );

/* [3.6]: Read individual factors of the annual difference (from freq 0.0):  */

   if ( Ts.freq > 1 )
      {
      Tm.ifadf = ivector( 0, Ts.freq / 2 );
      fgets( dumstrg, MAXSTR, inputv );
      for ( i = 0; i <= Ts.freq / 2; i++ )
          fscanf( inputv, "%d", &Tm.ifadf[i] );
      fscanf( inputv, "\n" );
      }
   else                                        /* Annual data (no ifadf):    */
      {
      fgets( dumstrg, MAXSTR, inputv );
      fgets( dumstrg, MAXSTR, inputv );
      }
/* [3.7]: Read time series and non-standard detvars data:*/

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%lf", &Tm.cbands );
   fscanf( inputv, "%lf\n", &Ts.refactor );
	if (Ts.refactor == 0){Ts.refactor=1;}
	
/* [3.7]: Read time series and non-standard detvars data:                    */	
	
/* [3.7]: Read time series and non-standard detvars data:                    */

   fgets( dumstrg, MAXSTR, inputv );

   for ( i = 1; i <= Ts.nobs; i++ )
       {
       fscanf( inputv, "%lf", &Ts.data[i] );
       if ( Tm.NdetVar > 0 )
          for ( j = 1; j <= nstdet; j++ )
              fscanf( inputv, "%lf", &DataMat[det[j]][i] );
       fscanf( inputv, "\n" );
       }

   fclose( inputv );

/*****************************************************************************/
/* [4]: Set up additional model and data features for estimation:            */
/*****************************************************************************/

   Tm.sper = Ts.freq;

/* [4.1]: Normalize model AR and MA operators for unscrambling:              */

   for ( i = 1; i <= Tm.NumAr1; i++ )  Tm.Ar1[i][0]   = -1.0;
/* DG 06/21/04 ***************************************************************/
//   for ( i = 1; i <= Tm.NumAr1S; i++ ) Tm.Ar1S[i][0]  = -1.0;
/* end DG ********************************************************************/
   for ( i = 1; i <= Tm.NumAr2; i++ )  Tm.Ar2[i][0]   = -1.0;
   for ( i = 1; i <= Tm.NumMa1; i++ )  Tm.Ma1[i][0]   = -1.0;
/* DG 06/20/04 ***************************************************************/
//   for ( i = 1; i <= Tm.NumMa1S; i++ ) Tm.Ma1S[i][0]  = -1.0;
/* end DG ********************************************************************/
   for ( i = 1; i <= Tm.NumMa2; i++ )  Tm.Ma2[i][0]   = -1.0;
   for ( i = 1; i <= Tm.NumAr1f; i++ ) Tm.Ar1f[i][0]  = -1.0;
//   for ( i = 1; i <= Tm.NumAr2f; i++ ) Tm.Ar2f[i][0]  = -1.0;
   for ( i = 1; i <= Tm.NumMa1f; i++ ) Tm.Ma1f[i][0]  = -1.0;
//   for ( i = 1; i <= Tm.NumMa2f; i++ ) Tm.Ma2f[i][0]  = -1.0;

/* [4.2]: Compute operator containing model non-stationary factors:          */

   Tm.ornsop = Tm.nrdiff + Tm.sper * Tm.nadiff;

   if ( Tm.sper == 12 )
      {
      if ( Tm.ifadf[0] == 1) Tm.ornsop += 1;
      if ( Tm.ifadf[1] == 1) Tm.ornsop += 2;
      if ( Tm.ifadf[2] == 1) Tm.ornsop += 2;
      if ( Tm.ifadf[3] == 1) Tm.ornsop += 2;
      if ( Tm.ifadf[4] == 1) Tm.ornsop += 2;
      if ( Tm.ifadf[5] == 1) Tm.ornsop += 2;
      if ( Tm.ifadf[6] == 1) Tm.ornsop += 1;
      }
   else if ( Tm.sper == 4 )
      {
      if ( Tm.ifadf[0] == 1) Tm.ornsop += 1;
      if ( Tm.ifadf[1] == 1) Tm.ornsop += 2;
      if ( Tm.ifadf[2] == 1) Tm.ornsop += 1;
      }

   Tm.rnsop = vector( 0, Tm.ornsop );
   CalcNonsOp( Tm.sper, Tm.nrdiff, Tm.nadiff, Tm.ifadf, Tm.ornsop, Tm.rnsop );

/* [4.3]: Transform and reescaling stochastic series and store in working data set:         */

   if ( Tm.boxlam == 0.0 )
      for ( i = 1; i <= Ts.nobs; i++ ) DataMat[0][i] = Ts.refactor*log( Ts.data[i] );
   else if ( Tm.boxlam != 1.0 )
    for ( i = 1; i <= Ts.nobs; i++ ) DataMat[0][i] = Ts.refactor*((pow( Ts.data[i], Tm.boxlam )-1) / Tm.boxlam ) ;
   else
      for ( i = 1; i <= Ts.nobs; i++ ) DataMat[0][i] = Ts.refactor*Ts.data[i];

/*****************************************************************************/
/* [5]: Set up parameter vector and preliminary estimates for estimation:    */
/*****************************************************************************/

   x    = vector( 1, npar );            /* Parameter vector (estimates).     */
   dev  = vector( 1, npar );            /* Standard deviations of estimates. */
   cov  = matrix( 1, npar, 1, npar );   /* Covariance matrix of estimates.   */

   npar = 0;                            /* Reinitialize and update ...       */

/* [5.1]: Omegas for each deterministic variable (if any):                   */

   for ( i = 1; i <= Tm.NdetVar; i++ )
       for ( j = 0; j <= Tm.Nomega[i]; j++ )
           if ( Tm.Imega[i][j] == 1 )
              {
              npar += 1;
              x[npar] = Tm.Omega[i][j];
              }

/* [5.2]: Deltas for each deterministic variable (if any):                   */

   for ( i = 1; i <= Tm.NdetVar; i++ )
       for ( j = 1; j <= Tm.Ndelta[i]; j++ )
           if ( Tm.Ielta[i][j] == 1 )
              {
              npar += 1;
              x[npar] = Tm.Delta[i][j];
              }

/* [5.3]: Phis for each regular AR factor (if any):                          */

   for ( i = 1; i <= Tm.NumAr1; i++ )
       for ( j = 1; j <= Tm.p1[i]; j++ )
           if ( Tm.Ia1[i][j] == 1 )
              {
              npar += 1;
              x[npar] = Tm.Ar1[i][j];
              }

/* DG 06/21/04 ***************************************************************/
/* [5.9]: theta for seasonal  MA factor (if any):                            */
/*
   for ( i = 1; i <= Tm.NumAr1S; i++ )
       if ( Tm.Ia1S[i] == 1 )
          {
          npar += 1;
          x[npar] = Tm.Ar1S[i][Ts.freq-1];
          }


/* end DG ********************************************************************/

/* [5.4]: Phis for each annual AR factor (if any):                           */

   for ( i = 1; i <= Tm.NumAr2; i++ )
       for ( j = 1; j <= Tm.p2[i]; j++ )
           if ( Tm.Ia2[i][j] == 1 )
              {
              npar += 1;
              x[npar] = Tm.Ar2[i][j];
              }

/* [5.5]: Thetas for each regular MA factor (if any):                        */

   for ( i = 1; i <= Tm.NumMa1; i++ )
       for ( j = 1; j <= Tm.q1[i]; j++ )
           if ( Tm.Im1[i][j] == 1 )
              {
              npar += 1;
              x[npar] = Tm.Ma1[i][j];
              }
/* DG 06/20/04 ***************************************************************/
/* [5.9]: theta for seasonal  MA factor (if any):                            */
/*
   for ( i = 1; i <= Tm.NumMa1S; i++ )
       if ( Tm.Im1S[i] == 1 )
          {
          npar += 1;
          x[npar] = Tm.Ma1S[i][Ts.freq-1];
          }


/* end DG ********************************************************************/

/* [5.6]: Thetas for each annual MA factor (if any):                         */

   for ( i = 1; i <= Tm.NumMa2; i++ )
       for ( j = 1; j <= Tm.q2[i]; j++ )
           if ( Tm.Im2[i][j] == 1 )
              {
              npar += 1;
              x[npar] = Tm.Ma2[i][j];
              }

/* [5.7]: 2nd phi for each regular f-fixed AR factor (if any):               */

   for ( i = 1; i <= Tm.NumAr1f; i++ )
       if ( Tm.Ia1f[i] == 1 )
          {
          npar += 1;
          x[npar] = Tm.Ar1f[i][2];
          }

/* [5.8]: 2nd phi for each annual f-fixed AR factor (if any):                */
/*
   for ( i = 1; i <= Tm.NumAr2f; i++ )
       if ( Tm.Ia2f[i] == 1 )
          {
          npar += 1;
          x[npar] = Tm.Ar2f[i][2];
          }

/* [5.9]: 2nd theta for each regular f-fixed MA factor (if any):             */

   for ( i = 1; i <= Tm.NumMa1f; i++ )
       if ( Tm.Im1f[i] == 1 )
          {
          npar += 1;
          x[npar] = Tm.Ma1f[i][2];
          }

/* [5.10]: 2nd theta for each annual f-fixed MA factor (if any):            */
/*
   for ( i = 1; i <= Tm.NumMa2f; i++ )
       if ( Tm.Im2f[i] == 1 )
          {
          npar += 1;
          x[npar] = Tm.Ma2f[i][2];
          }

/* [5.11]: Mean parameter (if present):                                     */

   if ( Tm.Imu == 1 )
      {
      npar += 1;
      x[npar] = Tm.mu;
      }

/*****************************************************************************/
/* [6]: Initializations related to the numerical optimizer:                  */
/*****************************************************************************/

   macheps = cmacheps();
   gradtol = pow( macheps, 1.1 / 3.0 );              /* Alternative: . 1.0e-7*/
   steptol = pow( macheps, 2.0 / 3.0 );              /* Alternative: . 0.50e-7*/
   maxits  =   0;
   nrits   =  10;
   ifault  =   0;

   printf( "\n" );
   printf( "Observations: %d.\n", Ts.nobs );
   printf( "Parameters  : %2d.\n\n", npar );

   fprintf( outputv, "Input file             : %s\n", inputf );
   fprintf( outputv, "Output file            : %s\n", outputf );
   fprintf( outputv, "Estimation method      : " );
   if ( met == 1 )
      fprintf( outputv, "exact maximum likelihood\n" );
   else
      fprintf( outputv, "approximate maximum likelihood\n" );
   fprintf( outputv, "Check for invertibility: " );
   if ( chk == 1 )
      fprintf( outputv, "constrained search\n" );
   else
      fprintf( outputv, "unconstrained search\n" );
   fprintf( outputv, "\n" );
   fprintf( outputv, "Observations: %d.\n", Ts.nobs );
   fprintf( outputv, "Parameters  : %2d.\n", npar );

/*****************************************************************************/
/* [7]: Estimate the parameters using the desired method (set by met):       */
/*****************************************************************************/

   varma1.xitol = ( met == 1 ) ? -1.0e-3 : 1.0e-3;
   varma1.chkma = chk;

/* [7.1]: Set up dynamic memory for the (output) standard VARMA structure:   */

   cast_us( x, &varma1, &ifault, 1, 0 );

/* [7.2]: Estimate the model specified in function cast_us:                  */

   est( cast_us, npar, x, dev, cov, maxits, nrits, gradtol, steptol,
        varma1.xitol, varma1.chkma,
        varma1.a, &varma1.sigma2, &varma1.logelf, &ifault );

   if ( ifault ) printf( "Bad initial guess: " );
   switch ( ifault )
      {
      case 1: printf( "Matrix Q is not positive definite.\n" );
              break;
      case 2: printf( "AR operator has at least one unit root.\n" );
              break;
      case 3: printf( "AR operator is strictly non-stationary.\n" );
              break;
      case 4: printf( "MA operator is strictly non-invertible.\n" );
              break;
      case 5: printf( "Unknown numerical problem.\n" );
              break;
      case 6: printf( "See cast_us().\n" );
              break;
      }

/* [7.3]: Put final estimates into the standard VARMA structure:             */

   cast_us( x, &varma1, &ifault, 0, 0 );

/*****************************************************************************/
/* [8]: Forcasting Module   DG 18/02/04                                     */
/*****************************************************************************/

/* [8.1] Allocate memory for the module variables                           */

   Fs.b = 0;
   Fs.f1 = matrix( 1, varma1.m, 1, Fs.L );
   Fs.f2 = matrix( 1, varma1.m, 1, Fs.L );
   Fs.f3 = matrix( 1, varma1.m, 1, Fs.L );
   Fs.v1 = tensor( 1, Fs.L, 1, varma1.m, 1, varma1.m );
   Fs.v2 = tensor( 1, Fs.L, 1, varma1.m, 1, varma1.m );
   Fs.v3 = tensor( 1, Fs.L, 1, varma1.m, 1, varma1.m );
   Fs.phi0   = tensor( 0,(Tm.ornsop + varma1.p) , 1, varma1.m, 1, varma1.m );
   Fs.data = vector( 1, Ts.nobs );
   Fs.latex_file = NEW_STR( 4096 );

   for ( i = 1; i <= Ts.nobs; i++ ) Fs.data[i] = DataMat[0][i];

/* [8.2]:  Compute coefficients of the Autoregressive and Non-stationary  operator:    */

  varphi (Tm.ornsop,  varma1.p, Fs.phi0, varma1.phi, Tm.rnsop);

/* [8.3]: Compute point forecasts for the level series, plus variances for   */
/*        the level, d-level and d4-level series:                            */
// Around line 750 in fuf.c, change the forecast call:
forecast( varma1.m, Ts.nobs, Tm.ornsop, (Tm.ornsop+varma1.p ), varma1.q, varma1.mu, Fs.phi0,
          varma1.theta, Fs.sigma2, varma1.nt, varma1.a, Fs.f1, Fs.v1, Fs.v2, Fs.v3,
          Fs.b, Fs.L, Ts.freq, varma1.xi, (Tm.NdetVar > 0) );  // Add condition

/* [8.4]: Compute point forecasts for the d-level and df-level series:       */

   point_forecast ( varma1.m, Ts.freq, Fs.L, Ts.nobs, Fs.data, Fs.f1, Fs.f2, Fs.f3);


   forecast_table_acii (outputv, Ts.nobs, Ts.freq, Ts.begyear, Ts.begtime, Tm.ornsop, Fs.L,
	Fs.data,  varma1.a,  Fs.f1,  Fs.f2,  Fs.f3,  Fs.v1, Fs.v2,  Fs.v3, Tm.boxlam, Ts.refactor, namef);

/* [8.5] Make Latex File (Table with graphics)                                    */


   ObsToDate( Ts.begyear, Ts.begtime, Ts.nobs+1, Ts.freq, &Fs.Aper, &Fs.Asub );
   snprintf( Fs.latex_file, 4096, "%s_prev.%d.%d.tex", x11out, Fs.Asub, Fs.Aper ); 

   if ( NULL == (prevputv = fopen(  Fs.latex_file, "w" )) )
      {
      printf( "\nError opening input file: %s\n", Fs.latex_file  );
      printf( "... Exiting to system ...\n" );
      exit( 1 );
      }

    fprintf( prevputv,"\\begin{multicols}{2}{ \n");

    if (Tm.boxlam < 0) {forecast_table_latex_BC ( prevputv, Ts.nobs, Ts.freq, Ts.begyear, Ts.begtime, Tm.ornsop, Fs.L,
                           Fs.data,  varma1.a,  Fs.f1,  Fs.f2,  Fs.f3,  Fs.v1, Fs.v2,  Fs.v3, Tm.boxlam, Ts.refactor);}
    else {forecast_table_latex ( prevputv, Ts.nobs, Ts.freq, Ts.begyear, Ts.begtime, Tm.ornsop, Fs.L,
                           Fs.data,  varma1.a,  Fs.f1,  Fs.f2,  Fs.f3,  Fs.v1, Fs.v2,  Fs.v3, Tm.boxlam, Ts.refactor);}
/* [8.6] High definition Graphic Forcasting Module   DG 17/02/04                    */               

    if (Tm.boxlam < 0) forecast_graphic_BC (  Fs.data, varma1.a, Fs.f1, Fs.v1, Tm.ornsop, 100*sqrt(Fs.sigma2), Ts.begyear, Ts.begtime, Ts.nobs, Fs.L, Ts.freq, Tm.boxlam, x11out, Ts.refactor );
    else forecast_graphic (  Fs.data, varma1.a, Fs.f3, Fs.v3, Tm.ornsop, 100*Fs.sigma2/Ts.refactor, Ts.begyear, Ts.begtime, Ts.nobs, Fs.L, Ts.freq, x11out, Ts.refactor );

    
    
   fprintf( prevputv,"\\begin{flushleft} \n");
   if (Fs.L > 20 ) fprintf( prevputv,"\\includegraphics[scale=.90]{prev%s.%d%d.eps} \n", x11out, Fs.Asub, Fs.Aper );
   else fprintf( prevputv,"\\includegraphics[scale=.70]{prev%s.%d%d.eps} \n", x11out, Fs.Asub, Fs.Aper );

   fprintf( prevputv,"\\end{flushleft} \n");
   fprintf( prevputv,"} \n" );
   fprintf( prevputv,"\\end{multicols} \n");
   fclose( prevputv );

/* [8.7]: Deallocate memory for the module variables                            */

   free_matrix( Fs.f1, 1, varma1.m, 1, Fs.L );
   free_matrix( Fs.f2, 1, varma1.m, 1, Fs.L );
   free_matrix( Fs.f3, 1, varma1.m, 1, Fs.L );
   free_tensor( Fs.v1, 1, Fs.L, 1, varma1.m, 1, varma1.m );
   free_tensor( Fs.v2, 1, Fs.L, 1, varma1.m, 1, varma1.m );
   free_tensor( Fs.v3, 1, Fs.L, 1, varma1.m, 1, varma1.m );
   free_tensor( Fs.phi0, 0,(Tm.ornsop + varma1.p), 1, varma1.m, 1, varma1.m );

   free_vector( Fs.data, 1, Ts.nobs );




/*****************************************************************************/
/* [9]: Write estimation results to output file (Optional):                  */
/*****************************************************************************/


/* [9.1]: Write Box-Cox lambda, non-stationary factors and seasonal period: */
/*
   fprintf( outputv, "\n" );
   fprintf( outputv, "Box-Cox lambda     : %4.1f\n", Tm.boxlam );
   fprintf( outputv, "Seasonal period    : %2d\n", Tm.sper );
   fprintf( outputv, "Regular differences: %2d\n", Tm.nrdiff );
   fprintf( outputv, "Annual differences : %2d\n", Tm.nadiff );

   if ( Ts.freq > 1 )
      {
      fprintf( outputv, "Irreducible factors: " );
      for ( i = 0; i <= Tm.sper / 2; i++ )
          fprintf( outputv, "%2d", Tm.ifadf[i] );
      fprintf( outputv, "\n" );
      }
   fprintf( outputv, "\n" );

   if ( Tm.ornsop >= 1 )
      {
      fprintf( outputv, "Coefficients of the non-stationary operator: \n" );
      for ( i = 1; i <= Tm.ornsop; i++ )
          fprintf( outputv, "  u[%2d]     = %15.10f\n", i, Tm.rnsop[i] );
      }

/* [9.2]: Write standard ARMA structure (taken from varma1):                */
/*
   fprintf( outputv, "Coefficients of the Autoregressive operator: \n" );
   for ( i = 1; i <= varma1.p; i++ )
       fprintf( outputv, "  phi[%2d]   = %15.10f\n", i, varma1.phi[i][1][1] );
   fprintf( outputv, "Coefficients of the Moving-Average operator: \n" );
   for ( i = 1; i <= varma1.q; i++ )
       fprintf( outputv, "  theta[%2d] = %15.10f\n", i, varma1.theta[i][1][1] );
   fprintf( outputv, "\n" );





/* [9.3]: Write estimation results to output file (standard VARMA model):      */
/*
   fprintf( outputv, "sigma2: %14.10f (%15.10f)\n", varma1.sigma2,
                      varma1.sigma2 * sqrt( 2.0 / (varma1.n * varma1.m)) );
   fprintf( outputv, "sigma : %14.10f\n", sqrt( varma1.sigma2 ) );
   fprintf( outputv, "logelf: %14.10f\n", varma1.logelf );


/*****************************************************************************/
/* [10]: Diagnostic checking of model residuals (use new structure res):     */
/*****************************************************************************/

   res.nobs = Ts.nobs - Tm.ornsop;
   res.freq = Tm.sper;
   res.numbering = Ts.numbering;
   if ( strcmp( Tm.residuals, "**" ) == 0 )  snprintf(Tm.residuals, 4096, "A%s", x11out);
   res.name = Tm.residuals;
   res.data = vector( 1, res.nobs );
   ObsToDate( Ts.begyear, Ts.begtime, Tm.ornsop + 1, res.freq,
              &res.begyear, &res.begtime );
   for ( i = 1; i <= res.nobs; i++ ) res.data[i] = varma1.a[1][i];
   File_StatSer( &res );
   File_PlotSer( &res );
   File_HistSer( &res );
   File_CorrSer( &res, nparma );


/********************************************************************************/
/* [11] : High definition residuals graphic  (optional. default: no)                                   */
/********************************************************************************/
/*
   if ( Ts.freq > 1 )
       {
       if ( Ts.begtime > 1) File_PlotSer_X11( &res, nparma, Ts.nobs, (Tm.ornsop+Ts.begtime-1), Ts.begyear, Tm.cbands, x11out, Tm.boxlam, Ts.refactor );
       else File_PlotSer_X11( &res, nparma, Ts.nobs, Tm.ornsop, Ts.begyear, Tm.cbands, x11out, Tm.boxlam, Ts.refactor );
       }
   else File_PlotSer_X11( &res, nparma, Ts.nobs, Tm.ornsop+outyear, Ts.begyear-outyear, Tm.cbands, x11out, Tm.boxlam, Ts.refactor );
*/

   free_vector( res.data, 1, res.nobs );


/*****************************************************************************/
/* [12]: Write Latex file  :      DG 18/02/04                                */
/*****************************************************************************/


   ObsToDate( Ts.begyear, Ts.begtime, Ts.nobs, Ts.freq, &Fs.Aper, &Fs.Asub );
   make_latex_forecast (texputv, Fs.latex_file, Fs.Aper, Fs.Asub, Ts.freq, namef );

   FREE_STR(Fs.latex_file);
/*****************************************************************************/
/* [13]: That's all, folks !!!:                                              */
/*****************************************************************************/

   fclose( outputv );
   fclose( texputv );


 printf( "\nOpening LaTex file: %s", texputf );

   if ( NULL == (texputv = fopen( texputf, "r" )) )
      {
      printf( "\nError opening LaTex file: %s\n", texputf );
      printf( "... Exiting to system ...\n" );
      exit( 1 );
      }
   else{
      printf( "...............................................OK\n");
      fclose( texputv );
      printf( "Running LaTex -interaction=batchmode  %s\n", texputf );

		#ifdef _WIN32
        snprintf(command, sizeof(command), "pdflatex -interaction=batchmode %s", texputf);
		#else
        snprintf(command, sizeof(command), "pdflatex -interaction=batchmode %s > /dev/null 2>&1", texputf);
		#endif

        int ret = system(command);
      if (ret != 0) {
      printf( "\nError opening PDF file. Check your pdflatex install!" );
      printf( "... Exiting to system ...\n" );
      exit( 1 );
      }
//		system(command);

		// Cleanup auxiliary files
		{ char auxfile[4096 + 8]; char logfile[4096 + 8];
		  snprintf(auxfile, sizeof(auxfile), "%s.aux", x11out);
		  snprintf(logfile, sizeof(logfile), "%s.log", x11out);
		  remove(auxfile); remove(logfile); }

   }

/*
 *
 * printf( "Running LaTex -interaction=batchmode  %s\n", texputf );

   h=latex_init();
   latex_cmd(h, "%s", x11out) ;
   latex_close(h) ;

 printf( "\nOpening DVI file: %s", dvif );

   if ( NULL == (dviv = fopen( dvif, "r" )) )
      {
      printf( "\nError opening DVI file: %s\n", dvif );
      printf( "... Exiting to system ...\n" );
      exit( 1 );
      }
   else{
   printf( "..................................................OK\n");
   fclose( dviv );
   printf( "\nRunning dvips  %s", dvif );
      h1=dvips_init(x11out);
   //  dvips_cmd(h1, " %s", x11out) ;
      dvips_close(h1) ;
   printf( "......................................................OK\n");
   }



 /* DG 06/30/04 *********************************************************************/


   cast_us( x, &varma1, &ifault, 0, 1 );

   free_matrix( cov, 1, npar, 1, npar );
   free_vector( dev, 1, npar );
   free_vector( x, 1, npar );

   free_vector( Tm.rnsop, 0, Tm.ornsop );
   if ( Ts.freq > 1 ) free_ivector( Tm.ifadf, 0, Ts.freq / 2 );
/*
   for ( i = Tm.NumMa2f; i >= 1; i-- )
       free_vector( Tm.Ma2f[i], 0, 2 );
   if ( Tm.NumMa2f > 0 )
      {
      free_ivector( Tm.Im2f, 1, Tm.NumMa2f );
      free( (FREE_ARG)(Tm.Ma2f + 1) );
      free_vector( Tm.qfre2, 1, Tm.NumMa2f );
      }
*/
   for ( i = Tm.NumMa1f; i >= 1; i-- )
       free_vector( Tm.Ma1f[i], 0, 2 );
   if ( Tm.NumMa1f > 0 )
      {
      free_ivector( Tm.Im1f, 1, Tm.NumMa1f );
      free( Tm.Ma1f );
      free_vector( Tm.qfre1, 1, Tm.NumMa1f );
      }
/*
   for ( i = Tm.NumAr2f; i >= 1; i-- )
       free_vector( Tm.Ar2f[i], 0, 2 );
   if ( Tm.NumAr2f > 0 )
      {
      free_ivector( Tm.Ia2f, 1, Tm.NumAr2f );
      free( (FREE_ARG)(Tm.Ar2f + 1) );
      free_vector( Tm.pfre2, 1, Tm.NumAr2f );
      }
*/
   for ( i = Tm.NumAr1f; i >= 1; i-- )
       free_vector( Tm.Ar1f[i], 0, 2 );
   if ( Tm.NumAr1f > 0 )
      {
      free_ivector( Tm.Ia1f, 1, Tm.NumAr1f );
      free( Tm.Ar1f );
      free_vector( Tm.pfre1, 1, Tm.NumAr1f );
      }

 /* DG 06/21/04  *************************************************************/
/*
   for ( i = Tm.NumMa1S; i >= 1; i-- )
       free_vector( Tm.Ma1S[i], 0, Ts.freq-1 );
   if ( Tm.NumMa1S > 0 )
      {
      free_ivector( Tm.Im1S, 1, Tm.NumMa1S );
      free( (FREE_ARG)(Tm.Ma1S + 1) );
      free_vector( Tm.qSre1, 1, Tm.NumMa1S );
      }
 /* END DG ********************************************************************/ 

   for ( i = Tm.NumMa2; i >= 1; i-- )
       {
       free_ivector( Tm.Im2[i], 0, Tm.q2[i] );
       free_vector( Tm.Ma2[i], 0, Tm.q2[i] );
       }
   if ( Tm.NumMa2 > 0 )
      {
      free( Tm.Im2 );
      free( Tm.Ma2 );
      free_ivector( Tm.q2, 1, Tm.NumMa2 );
      }

   for ( i = Tm.NumMa1; i >= 1; i-- )
       {
       free_ivector( Tm.Im1[i], 0, Tm.q1[i] );
       free_vector( Tm.Ma1[i], 0, Tm.q1[i] );
       }
   if ( Tm.NumMa1 > 0 )
      {
      free( Tm.Im1 );
      free( Tm.Ma1 );
      free_ivector( Tm.q1, 1, Tm.NumMa1 );
      }

   for ( i = Tm.NumAr2; i >= 1; i-- )
       {
       free_ivector( Tm.Ia2[i], 0, Tm.p2[i] );
       free_vector( Tm.Ar2[i], 0, Tm.p2[i] );
       }
   if ( Tm.NumAr2 > 0 )
      {
      free( Tm.Ia2 );
      free( Tm.Ar2 );
      free_ivector( Tm.p2, 1, Tm.NumAr2 );
      }
 /* DG 06/22/04  *************************************************************/
/*
   for ( i = Tm.NumAr1S; i >= 1; i-- )
       free_vector( Tm.Ar1S[i], 0, Ts.freq-1 );
   if ( Tm.NumAr1S > 0 )
      {
      free_ivector( Tm.Ia1S, 1, Tm.NumAr1S );
      free( (FREE_ARG)(Tm.Ar1S + 1) );
      free_vector( Tm.pSre1, 1, Tm.NumAr1S );
      }
 /* END DG ********************************************************************/ 

   for ( i = Tm.NumAr1; i >= 1; i-- )
       {
       free_ivector( Tm.Ia1[i], 0, Tm.p1[i] );
       free_vector( Tm.Ar1[i], 0, Tm.p1[i] );
       }
   if ( Tm.NumAr1 > 0 )
      {
      free( Tm.Ia1 );
      free( Tm.Ar1 );
      free_ivector( Tm.p1, 1, Tm.NumAr1 );
      }

   for ( i = Tm.NdetVar; i >= 1; i-- ) if ( Tm.Ndelta[i] > 0 )
       {
       free_ivector( Tm.Ielta[i], 0, Tm.Ndelta[i] );
       free_vector( Tm.Delta[i], 0, Tm.Ndelta[i] );
       }
   for ( i = Tm.NdetVar; i >= 1; i-- )
       {
       free_ivector( Tm.Imega[i], 0, Tm.Nomega[i] );
       free_vector( Tm.Omega[i], 0, Tm.Nomega[i] );
       }
   if ( Tm.NdetVar > 0 )
      {
      free( Tm.Ielta );
      free( Tm.Delta );
      free_ivector( Tm.Ndelta, 1, Tm.NdetVar );
      free( Tm.Imega );
      free( Tm.Omega );
      free_ivector( Tm.Nomega, 1, Tm.NdetVar );
      }

   if ( Tm.NdetVar > 0 ) free_ivector( det, 1, NT );
   free_matrix( DataMat, 0, Tm.NdetVar, 1, Ts.nobs );
   free_vector( Ts.data, 1, Ts.nobs );

   FREE_STR( dumstrg );
   FREE_STR( outputf );
   FREE_STR( inputf );
   FREE_STR( x11out );
   FREE_STR( texputf );
   FREE_STR( dvif );
   FREE_STR( namef );
   FREE_STR( Tm.residuals );
/* printf( "\nFINAL RAM  : %lu\n", coreleft() );                             */
return 0;
}

/*****************************************************************************/
/*****************************************************************************/

void cast_us( real *x, struct Tvarma *armax,
              int *ifaultx, int firstx, int lastx )

{
   int  i, j, k, itmp;
   int  OrderAr, OrderMa, *NuLag;
   real *vtmp1, *vtmp2, **Nu, tmp1, tmp2, *xius;

   void unscramble( struct Tusmodel *, int *, int *, real *, real *, int * );
   void calcnu( real *, int, real *, int, real *, int );

/*****************************************************************************/
/* [1]: Put parameter vector x into seasonal model:                          */
/*****************************************************************************/

   itmp = 0;

/* [1.1]: Omegas for each deterministic variable (if any):                   */

   for ( i = 1; i <= Tm.NdetVar; i++ )
       for ( j = 0; j <= Tm.Nomega[i]; j++ )
           if ( Tm.Imega[i][j] == 1 )
              {
              itmp += 1;
              Tm.Omega[i][j] = x[itmp];
              }

/* [1.2]: Deltas for each deterministic variable (if any):                   */

   for ( i = 1; i <= Tm.NdetVar; i++ )
       for ( j = 1; j <= Tm.Ndelta[i]; j++ )
           if ( Tm.Ielta[i][j] == 1 )
              {
              itmp += 1;
              Tm.Delta[i][j] = x[itmp];
              }

/* [1.3]: Phis for each regular AR factor (if any):                          */

   for ( i = 1; i <= Tm.NumAr1; i++ )
       for ( j = 1; j <= Tm.p1[i]; j++ )
           if ( Tm.Ia1[i][j] == 1 )
              {
              itmp += 1;
              Tm.Ar1[i][j] = x[itmp];
              }

/* DG 06/22/04 ***************************************************************/

/* [1.9]: 11  theta for each seasonal  AR factor (if any):             */
/*
   for ( i = 1; i <= Tm.NumAr1S; i++ )
       if ( Tm.Ia1S[i] == 1 )
          {
          itmp += 1;
          Tm.Ar1S[i][Ts.freq-1] = x[itmp];
          }

/* END DG ********************************************************************/

/* [1.4]: Phis for each annual AR factor (if any):                           */

   for ( i = 1; i <= Tm.NumAr2; i++ )
       for ( j = 1; j <= Tm.p2[i]; j++ )
           if ( Tm.Ia2[i][j] == 1 )
              {
              itmp += 1;
              Tm.Ar2[i][j] = x[itmp];
              }

/* [1.5]: Thetas for each regular MA factor (if any):                        */

   for ( i = 1; i <= Tm.NumMa1; i++ )
       for ( j = 1; j <= Tm.q1[i]; j++ )
           if ( Tm.Im1[i][j] == 1 )
              {
              itmp += 1;
              Tm.Ma1[i][j] = x[itmp];
              }


/* DG 06/21/04 ***************************************************************/

/* [1.9]: 11  theta for each seasonal  MA factor (if any):             */
/*
   for ( i = 1; i <= Tm.NumMa1S; i++ )
       if ( Tm.Im1S[i] == 1 )
          {
          itmp += 1;
          Tm.Ma1S[i][Ts.freq-1] = x[itmp];
          }

/* END DG ********************************************************************/

/* [1.6]: Thetas for each annual MA factor (if any):                         */

   for ( i = 1; i <= Tm.NumMa2; i++ )
       for ( j = 1; j <= Tm.q2[i]; j++ )
           if ( Tm.Im2[i][j] == 1 )
              {
              itmp += 1;
              Tm.Ma2[i][j] = x[itmp];
              }


/* [1.7]: 2nd phi for each regular f-fixed AR factor (if any):               */

   for ( i = 1; i <= Tm.NumAr1f; i++ )
       if ( Tm.Ia1f[i] == 1 )
          {
          itmp += 1;
          Tm.Ar1f[i][2] = x[itmp];
          }

/* [1.8]: 2nd phi for each annual f-fixed AR factor (if any):                */
/*
   for ( i = 1; i <= Tm.NumAr2f; i++ )
       if ( Tm.Ia2f[i] == 1 )
          {
          itmp += 1;
          Tm.Ar2f[i][2] = x[itmp];
          }

/* [1.9]: 2nd theta for each regular f-fixed MA factor (if any):             */

   for ( i = 1; i <= Tm.NumMa1f; i++ )
       if ( Tm.Im1f[i] == 1 )
          {
          itmp += 1;
          Tm.Ma1f[i][2] = x[itmp];
          }

/* [1.10]: 2nd theta for each annual f-fixed MA factor (if any):             */
/*
   for ( i = 1; i <= Tm.NumMa2f; i++ )
       if ( Tm.Im2f[i] == 1 )
          {
          itmp += 1;
          Tm.Ma2f[i][2] = x[itmp];
          }

/* [1.11]: Mean parameter (if present):                                      */

   if ( Tm.Imu == 1 )
      {
      itmp += 1;
      Tm.mu = x[itmp];
      }

/*****************************************************************************/
/* [2]: Calculate the orders of the complete AR and MA operators:            */
/*****************************************************************************/

   OrderAr = 0;
   OrderMa = 0;

   for ( i = 1; i <= Tm.NumAr1; i++ )  OrderAr += Tm.p1[i];
   for ( i = 1; i <= Tm.NumAr2; i++ )  OrderAr += Tm.p2[i] * Tm.sper;

/* DG 06/21/04 **************************************************************/
//   for ( i = 1; i <= Tm.NumAr1S; i++ ) OrderAr += Ts.freq-1;
/* END DG *******************************************************************/
   for ( i = 1; i <= Tm.NumAr1f; i++ ) OrderAr += 2;
//   for ( i = 1; i <= Tm.NumAr2f; i++ ) OrderAr += 2 * Tm.sper;

   for ( i = 1; i <= Tm.NumMa1; i++ )  OrderMa += Tm.q1[i];
   for ( i = 1; i <= Tm.NumMa2; i++ )  OrderMa += Tm.q2[i] * Tm.sper;
/* DG 06/21/04 **************************************************************/
//   for ( i = 1; i <= Tm.NumMa1S; i++ ) OrderMa += Ts.freq-1;
/* END DG *******************************************************************/
   for ( i = 1; i <= Tm.NumMa1f; i++ ) OrderMa += 2;
//   for ( i = 1; i <= Tm.NumMa2f; i++ ) OrderMa += 2 * Tm.sper;

   armax->m = 1;
   armax->n = Ts.nobs - Tm.ornsop;
   armax->p = OrderAr;
   armax->q = OrderMa;

/*****************************************************************************/
/* [3]: First allocation of the STANDARD VARMA structure:                    */
/*****************************************************************************/

   if ( firstx )
      {
      armax->mu    = vector( 1, armax->m );
      armax->phi   = tensor( 0, armax->p, 1, armax->m, 1, armax->m );
      armax->theta = tensor( 0, armax->q, 1, armax->m, 1, armax->m );
      armax->qq    = matrix( 1, armax->m, 1, armax->m );
      armax->w     = matrix( 1, armax->m, 1, armax->n );
      armax ->nt    = matrix( 1, armax->m, 1, Ts.nobs );
      armax->a     = matrix( 1, armax->m, 1, armax->n );
      armax->xi    = matrix( 1, armax->m, 1, Ts.nobs+Fs.L );

      for ( i = 1; i <= armax->m; i++ )
          {
          armax->mu[i] = 0.0;
          for ( j = 1; j <= armax->m; j++ )
              {
              for ( k = 0; k <= armax->p; k++ ) armax->phi[k][i][j] = 0.0;
              for ( k = 0; k <= armax->q; k++ ) armax->theta[k][i][j] = 0.0;
              armax->qq[i][j] = 0.0;
              }
          for ( j = 1; j <= armax->n; j++ )
              {
              armax->w[i][j] = 0.0;
              armax->a[i][j] = 0.0;
              }
          }
      }

/*****************************************************************************/
/* [4]: Unscrambling the operators:                                          */
/*****************************************************************************/

   *ifaultx = 0;

   vtmp1 = vector( 0, OrderAr );
   vtmp2 = vector( 0, OrderMa );

   unscramble( &Tm, &OrderAr, &OrderMa, vtmp1, vtmp2, ifaultx );
   for ( i = 1; i <= OrderAr; i++ ) armax->phi[i][1][1] = vtmp1[i];
   for ( i = 1; i <= OrderMa; i++ ) armax->theta[i][1][1] = vtmp2[i];

   free_vector( vtmp2, 0, OrderMa );
   free_vector( vtmp1, 0, OrderAr );

   armax->mu[1]    = Tm.mu;
   armax->qq[1][1] = 1.0;

/*****************************************************************************/
/* [5]: Computing and differencing the noise (data for VARMA structure):     */
/*****************************************************************************/

// Allocate arrays for full range including forecast periods
vtmp1 = vector( 1, Ts.nobs + Fs.L );
xius  = vector( 1, Ts.nobs + Fs.L );

// Initialize both arrays to zero for the entire range
for (i = 1; i <= Ts.nobs + Fs.L; i++) {
    vtmp1[i] = 0.0;
    xius[i] = 0.0;
}

// Handle deterministic variables if present
if (Tm.NdetVar > 0) {
    // Deterministic variables present - calculate their effects
    NuLag = ivector( 1, Tm.NdetVar );
    Nu    = (real **)calloc((size_t)(Tm.NdetVar + 1), sizeof(real *));

    // Initialize Nu arrays for each deterministic variable
    for (j = 1; j <= Tm.NdetVar; j++) {
        if (Tm.Ndelta[j] == 0) {
            NuLag[j] = Tm.Nomega[j];
        } else {
            NuLag[j] = 20; // Default lag length for delta components
        }
        Nu[j] = vector( 0, NuLag[j] );
        calcnu( Tm.Omega[j], Tm.Nomega[j], Tm.Delta[j], Tm.Ndelta[j],
                Nu[j], NuLag[j] );
    }

    // Calculate deterministic effects for entire range
    for (i = 1; i <= Ts.nobs + Fs.L; i++) {
        double deterministic_effect = 0.0;

        // Sum effects from all deterministic variables
        for (j = 1; j <= Tm.NdetVar; j++) {
            double var_effect = 0.0;
            for (k = 0; k <= NuLag[j]; k++) {
                if (i - k >= 1) {
                    var_effect += Nu[j][k] * DataMat[j][i - k];
                }
            }
            deterministic_effect += var_effect;
        }

        xius[i] = deterministic_effect;

        // For historical period: data minus deterministic effect
        if (i <= Ts.nobs) {
            vtmp1[i] = DataMat[0][i] - deterministic_effect;
        }
        // For forecast period: we don't have actual data, so just store -deterministic_effect
        // This will be handled properly in the forecast function
        else {
            vtmp1[i] = -deterministic_effect;
        }
    }

    // Clean up Nu arrays
    for (j = Tm.NdetVar; j >= 1; j--) {
        free_vector(Nu[j], 0, NuLag[j]);
    }
    free(Nu);
    free_ivector(NuLag, 1, Tm.NdetVar);
}
else {
    // No deterministic variables - simple case
    for (i = 1; i <= Ts.nobs; i++) {
        // Copy original data for historical period
        vtmp1[i] = DataMat[0][i];
    }
    // For forecast period, set to zero (no data available)
    for (i = Ts.nobs + 1; i <= Ts.nobs + Fs.L; i++) {
        vtmp1[i] = 0.0;
    }
    // xius remains zero for all periods (already initialized above)
}

// Store the original (potentially deterministic-adjusted) series in nt
for (j = 1; j <= Ts.nobs; j++) {
    armax->nt[1][j] = vtmp1[j];
}

// Apply non-stationary transformation to create stationary series w
for (j = Tm.ornsop + 1; j <= Ts.nobs; j++) {
    double nonstat_adjustment = 0.0;
    for (i = 1; i <= Tm.ornsop; i++) {
        nonstat_adjustment -= Tm.rnsop[i] * vtmp1[j - i];
    }
    armax->w[1][j - Tm.ornsop] = vtmp1[j] + nonstat_adjustment;
}

// Set xi values (deterministic/intervention effects) for entire range
for (j = 1; j <= Ts.nobs + Fs.L; j++) {
    armax->xi[1][j] = xius[j];
}

// Free temporary arrays
free_vector(vtmp1, 1, Ts.nobs + Fs.L);
free_vector(xius, 1, Ts.nobs + Fs.L);

/*****************************************************************************/
/* [6]: Last deallocation of the STANDARD VARMA structure:                   */
/*****************************************************************************/

   if ( lastx == 1 )
      {
      free_matrix( armax->a, 1, armax->m, 1, armax->n );
      free_matrix( armax->w, 1, armax->m, 1, armax->n );
/*DG  13/08/04         *******************************************************/
      free_matrix( armax->xi, 1, armax->m, 1, Ts.nobs + Fs.L );
      free_matrix( armax ->nt, 1, armax->m, 1, Ts.nobs );

/*END DG               ******************************************************/
      free_matrix( armax->qq, 1, armax->m, 1, armax->m );
      free_tensor( armax->theta, 0, armax->q, 1, armax->m, 1, armax->m );
      free_tensor( armax->phi, 0, armax->p, 1, armax->m, 1, armax->m );
      free_vector( armax->mu, 1, armax->m );
      }
}

/*****************************************************************************/
/*****************************************************************************/

void unscramble( struct Tusmodel *Model, int *OrderAr, int *OrderMa,
                 double *ArFactor, double *MaFactor, int *ifault )

{
   int i, j, k, pr, pa, qr, qa, p1, p2, q1, q2, itmp1, itmp2, pq;
   double *phir, *phia, *thetar, *thetaa, *tmp, Cos2( double, int );

/*****************************************************************************/
/* [1]: Initialize intermediate operators:                                   */
/*****************************************************************************/

   pr = 0;                              /* Order of the regular AR operator. */
   pa = 0;                              /* Order of the annual AR operator.  */
   qr = 0;                              /* Order of the regular MA operator. */
   qa = 0;                              /* Order of the annual MA operator.  */
   pq = 0;                              /* Maximum of the previous orders.   */

   for ( i = 1; i <= Model->NumAr1; i++ )  pr += Model->p1[i];
/* DG 06/22/04 ****************************************************************/
//   for ( i = 1; i <= Model->NumAr1S; i++ ) pr += Ts.freq-1;
/* END DG *********************************************************************/
   for ( i = 1; i <= Model->NumAr1f; i++ ) pr += 2;
   for ( i = 1; i <= Model->NumAr2; i++ )  pa += Model->p2[i];
//   for ( i = 1; i <= Model->NumAr2f; i++ ) pa += 2;
   for ( i = 1; i <= Model->NumMa1; i++ )  qr += Model->q1[i];
/* DG 06/21/04 ****************************************************************/
//   for ( i = 1; i <= Model->NumMa1S; i++ ) qr += Ts.freq-1;
/* END DG *********************************************************************/
   for ( i = 1; i <= Model->NumMa1f; i++ ) qr += 2;
   for ( i = 1; i <= Model->NumMa2; i++ )  qa += Model->q2[i];
//   for ( i = 1; i <= Model->NumMa2f; i++ ) qa += 2;

   itmp1 = ( pr > pa ) ? pr : pa;
   itmp2 = ( qr > qa ) ? qr : qa;
   pq    = ( itmp1 > itmp2 ) ? itmp1 : itmp2;

   phir   = vector( 0, pr );                         /* Regular AR operator. */
   phia   = vector( 0, pa );                         /* Annual AR operator.  */
   thetar = vector( 0, qr );                         /* Regular MA operator. */
   thetaa = vector( 0, qa );                         /* Annual MA operator.  */
   tmp    = vector( 0, pq );                         /* Temporary operator.  */

   phir[0]   = -1.0;
   phia[0]   = -1.0;
   thetar[0] = -1.0;
   thetaa[0] = -1.0;
   tmp[0]    = -1.0;

   p1 = 0;
   p2 = 0;
   q1 = 0;
   q2 = 0;

/*****************************************************************************/
/* [2]: Calculate the normal-regular AR operator:                            */
/*****************************************************************************/

   for ( k = 1; k <= Model->NumAr1; k++ )
       {
       for ( i = 1; i <= pr; i++ ) phir[i] = 0.0;
       phir[0] = -1;
       tmp[0]  = -1;
       for ( i = 0; i <= p1; i++ )
           for ( j = 0; j <= Model->p1[k]; j++ )
               phir[j+i] -= Model->Ar1[k][j] * tmp[i];
       p1 += Model->p1[k];
       for ( i = 1; i <= p1; i++ ) tmp[i] = phir[i];
       }
   for ( i = 0; i <= p1; i++ ) phir[i] = tmp[i];

/* DG 06/22/04 ***************************************************************/

/*****************************************************************************/
/* [3]: Multiply it by every seasonal AR operator:                           */
/*****************************************************************************/
/*
   for ( k = 1; k <= Model->NumAr1S; k++ )
       {
       if ( Model->Ar1S[k][Ts.freq-1] > 0.0 )                    /* Force negative.  */
/*          {
          *ifault = 1;
          goto u1;
          }

       if(Ts.freq == 12){
	 Model->Ar1S[k][1]  = - pow( -Model->Ar1S[k][11],  ( 1.0 /11.0 )  );
	 Model->Ar1S[k][2]  = - pow( -Model->Ar1S[k][11],  ( 2.0/11.0 )  );
	 Model->Ar1S[k][3]  = - pow( -Model->Ar1S[k][11],  ( 3.0/11.0 )  );
	 Model->Ar1S[k][4]  = - pow( -Model->Ar1S[k][11],  ( 4.0/11.0 ) );
	 Model->Ar1S[k][5]  = - pow( -Model->Ar1S[k][11],  ( 5.0/11.0 ) );
	 Model->Ar1S[k][6]  = - pow( -Model->Ar1S[k][11],  ( 6.0/11.0 ) );
	 Model->Ar1S[k][7]  = - pow( -Model->Ar1S[k][11],  ( 7.0/11.0 ) );
	 Model->Ar1S[k][8]  = - pow( -Model->Ar1S[k][11],  ( 8.0/11.0 ) );
	 Model->Ar1S[k][9]  = - pow( -Model->Ar1S[k][11],  ( 9.0/11.0 ));
	 Model->Ar1S[k][10] = - pow( -Model->Ar1S[k][11],  ( 10.0/11.0) );
       }
       if(Ts.freq == 4){
	 Model->Ar1S[k][1] = - pow( -Model->Ar1S[k][3],  ( 1.0 /3.0 )  );
	 Model->Ar1S[k][2] = - pow( -Model->Ar1S[k][3],  ( 2.0/3.0 )  );
       }

       for ( i = 1; i <= pr; i++ ) phir[i] = 0.0;
       phir[0] = -1;
       tmp[0]  = -1;
       for ( i = 0; i <= p1; i++ )
           for ( j = 0; j <= Ts.freq-1; j++ )
               phir[j+i] -= Model->Ar1S[k][j] * tmp[i];
       p1 += Ts.freq-1;
       for ( i = 1; i <= p1; i++ ) tmp[i] = phir[i];
       }
   for ( i = 0; i <= p1; i++ ) phir[i] = tmp[i];

/* END DG ********************************************************************/

/*****************************************************************************/
/* [3]: Multiply it by by every regular AR(2) operator with fixed frequency: */
/*****************************************************************************/

   for ( k = 1; k <= Model->NumAr1f; k++ )
       {
       if ( Model->Ar1f[k][2] > 0.0 )                    /* Force negative.  */
          {
          *ifault = 1;
          goto u1;
          }
       Model->Ar1f[k][1] = 2.0 * Cos2( Model->pfre1[k], Model->sper ) *
                           sqrt( -Model->Ar1f[k][2] );
       for ( i = 1; i <= pr; i++ ) phir[i] = 0.0;
       phir[0] = -1;
       tmp[0]  = -1;
       for ( i = 0; i <= p1; i++ )
           for ( j = 0; j <= 2; j++ )
               phir[j+i] -= Model->Ar1f[k][j] * tmp[i];
       p1 += 2;
       for ( i = 1; i <= p1; i++ ) tmp[i] = phir[i];
       }
   for ( i = 0; i <= p1; i++ ) phir[i] = tmp[i];

/*****************************************************************************/
/* [4]: Calculate the normal-annual AR operator:                             */
/*****************************************************************************/

   for ( k = 1; k <= Model->NumAr2; k++ )
       {
       for ( i = 1; i <= pa; i++ ) phia[i] = 0.0;
       phia[0] = -1;
       tmp[0]  = -1;
       for ( i = 0; i <= p2; i++ )
           for ( j = 0; j <= Model->p2[k]; j++ )
               phia[j+i] -= Model->Ar2[k][j] * tmp[i];
       p2 += Model->p2[k];
       for ( i = 1; i <= p2; i++ ) tmp[i] = phia[i];
       }
   for ( i = 0; i <= p2; i++ ) phia[i] = tmp[i];

/*****************************************************************************/
/* [5]: Multiply it by by every annual AR(2) operator with fixed frequency:  */
/*****************************************************************************/
/*
   for ( k = 1; k <= Model->NumAr2f; k++ )
       {
       if ( Model->Ar2f[k][2] > 0.0 )                    /* Force negative.  */
/*          {
          *ifault = 1;
          goto u1;
          }
       Model->Ar2f[k][1] = 2.0 * Cos2( Model->pfre2[k], Model->sper ) *
                           sqrt( -Model->Ar2f[k][2] );
       for ( i = 1; i <= pa; i++ ) phia[i] = 0.0;
       phia[0] = -1;
       tmp[0]  = -1;
       for ( i = 0; i <= p2; i++ )
           for ( j = 0; j <= 2; j++ )
               phia[j+i] -= Model->Ar2f[k][j] * tmp[i];
       p2 += 2;
       for ( i = 1; i <= p2; i++ ) tmp[i] = phia[i];
       }
   for ( i = 0; i <= p2; i++ ) phia[i] = tmp[i];
*/
/*****************************************************************************/
/* [6]: Calculate the normal-regular MA operator:                            */
/*****************************************************************************/

   for ( k = 1; k <= Model->NumMa1; k++ )     /* Take invertible for MA(1)s: */

/* CAUTION: "Taking invertible" for MA(1)s may cause problems when computing */
/* standard deviations of  the corresponding  parameter estimate, so it is   */
/* ENABLED  here and within step [8] too. Disabling this should go with good */
/* initial estimates in order to facilitate convergence. One has to play ... */

       {
       if ( (Model->q1[k] == 1) && (fabs( Model->Ma1[k][1] ) > 1.0) )
          Model->Ma1[k][1] = 1.0 / Model->Ma1[k][1];
       for ( i = 1; i <= qr; i++ ) thetar[i] = 0.0;
       thetar[0] = -1;
       tmp[0]    = -1;
       for ( i = 0; i <= q1; i++ )
           for ( j = 0; j <= Model->q1[k]; j++ )
               thetar[j+i] -= Model->Ma1[k][j] * tmp[i];
       q1 += Model->q1[k];
       for ( i = 1; i <= q1; i++ ) tmp[i] = thetar[i];
       }

/* DG 06/21/04 ***************************************************************/


/*****************************************************************************/
/* [7]: Multiply it by by every seasonal MA operator with fixed frequency: */
/*****************************************************************************/
/*
   for ( k = 1; k <= Model->NumMa1S; k++ )
       {
       if ( Model->Ma1S[k][Ts.freq-1] < -1.0 )                   /* Take invertible. */
/*          Model->Ma1S[k][Ts.freq-1] = 1.0 / Model->Ma1S[k][Ts.freq-1];
       if ( Model->Ma1S[k][Ts.freq-1] > 0.0 )                    /* Force negative.  */
/*          {
          *ifault = 1;
          goto u1;
          }
       if(Ts.freq == 12){
	 Model->Ma1S[k][1] = - pow( -Model->Ma1S[k][11],  ( 1.0 /11.0 )  );
	 Model->Ma1S[k][2] = - pow( -Model->Ma1S[k][11],  ( 2.0/11.0 )  );
	 Model->Ma1S[k][3] = - pow( -Model->Ma1S[k][11],  ( 3.0/11.0 )  );
	 Model->Ma1S[k][4] = - pow( -Model->Ma1S[k][11],  ( 4.0/11.0 ) );
	 Model->Ma1S[k][5] = - pow( -Model->Ma1S[k][11],  ( 5.0/11.0 ) );
	 Model->Ma1S[k][6] = - pow( -Model->Ma1S[k][11],  ( 6.0/11.0 ) );
	 Model->Ma1S[k][7] = - pow( -Model->Ma1S[k][11],  ( 7.0/11.0 ) );
	 Model->Ma1S[k][8] = - pow( -Model->Ma1S[k][11],  ( 8.0/11.0 ) );
	 Model->Ma1S[k][9] = - pow( -Model->Ma1S[k][11],  ( 9.0/11.0 ));
	 Model->Ma1S[k][10] = - pow( -Model->Ma1S[k][11], ( 10.0/11.0) );
       }
       if(Ts.freq == 4){
	 Model->Ma1S[k][1] = - pow( -Model->Ma1S[k][3],  ( 1.0 /3.0 )  );
	 Model->Ma1S[k][2] = - pow( -Model->Ma1S[k][3],  ( 2.0/3.0 )  );
       }

       for ( i = 1; i <= qr; i++ ) thetar[i] = 0.0;
       thetar[0] = -1;
       tmp[0]    = -1;
       for ( i = 0; i <= q1; i++ )
           for ( j = 0; j <= Ts.freq-1; j++ )
               thetar[j+i] -= Model->Ma1S[k][j] * tmp[i];
       q1 += Ts.freq-1;
       for ( i = 1; i <= q1; i++ ) tmp[i] = thetar[i];
       }

   //     for ( i = 0; i <= q1; i++ ) thetar[i] = tmp[i];

/* END DG ********************************************************************/

/*****************************************************************************/
/* [7]: Multiply it by by every regular MA(2) operator with fixed frequency: */
/*****************************************************************************/

   for ( k = 1; k <= Model->NumMa1f; k++ )
       {
       if ( Model->Ma1f[k][2] < -1.0 )                   /* Take invertible. */
          Model->Ma1f[k][2] = 1.0 / Model->Ma1f[k][2];
       if ( Model->Ma1f[k][2] > 0.0 )                    /* Force negative.  */
          {
          *ifault = 1;
          goto u1;
          }
       Model->Ma1f[k][1] = 2.0 * Cos2( Model->qfre1[k], Model->sper ) *
                           sqrt( -Model->Ma1f[k][2] );
       for ( i = 1; i <= qr; i++ ) thetar[i] = 0.0;
       thetar[0] = -1;
       tmp[0]    = -1;
       for ( i = 0; i <= q1; i++ )
           for ( j = 0; j <= 2; j++ )
               thetar[j+i] -= Model->Ma1f[k][j] * tmp[i];
       q1 += 2;
       for ( i = 1; i <= q1; i++ ) tmp[i] = thetar[i];
       }
   for ( i = 0; i <= q1; i++ ) thetar[i] = tmp[i];

/*****************************************************************************/
/* [8]: Calculate the normal-annual MA operator:                             */
/*****************************************************************************/

   for ( k = 1; k <= Model->NumMa2; k++ )     /* Take invertible for MA(1)s: */
       {
       if ( (Model->q2[k] == 1) && (fabs( Model->Ma2[k][1] ) > 1.0) )
          Model->Ma2[k][1] = 1.0 / Model->Ma2[k][1];
       for ( i = 1; i <= qa; i++ ) thetaa[i] = 0.0;
       thetaa[0] = -1;
       tmp[0]    = -1;
       for ( i = 0; i <= q2; i++ )
           for ( j = 0; j <= Model->q2[k]; j++ )
               thetaa[j+i] -= Model->Ma2[k][j] * tmp[i];
       q2 += Model->q2[k];
       for ( i = 1; i <= q2; i++ ) tmp[i] = thetaa[i];
       }
   for ( i = 0; i <= q2; i++ ) thetaa[i] = tmp[i];
/*****************************************************************************/
/* [9]: Multiply it by by every annual MA(2) operator with fixed frequency:  */
/*****************************************************************************/
/*
   for ( k = 1; k <= Model->NumMa2f; k++ )
       {
       if ( Model->Ma2f[k][2] < -1.0 )                   /* Take invertible. */
/*          Model->Ma2f[k][2] = 1.0 / Model->Ma2f[k][2];
       if ( Model->Ma2f[k][2] > 0.0 )                    /* Force negative.  */
/*          {
          *ifault = 1;
          goto u1;
          }
       Model->Ma2f[k][1] = 2.0 * Cos2( Model->qfre2[k], Model->sper ) *
                           sqrt( -Model->Ma2f[k][2] );
       for ( i = 1; i <= qa; i++ ) thetaa[i] = 0.0;
       thetaa[0] = -1;
       tmp[0]    = -1;
       for ( i = 0; i <= q2; i++ )
           for ( j = 0; j <= 2; j++ )
               thetaa[j+i] -= Model->Ma2f[k][j] * tmp[i];
       q2 += 2;
       for ( i = 1; i <= q2; i++ ) tmp[i] = thetaa[i];
       }
   for ( i = 0; i <= q2; i++ ) thetaa[i] = tmp[i];

/*****************************************************************************/
/* [10]: Calculate the complete AR and Ma operators:                         */
/*****************************************************************************/

   *OrderAr = pr + Model->sper * pa;
   *OrderMa = qr + Model->sper * qa;

   for ( i = 1; i <= *OrderAr; i++ ) ArFactor[i] = 0.0;
   for ( i = 1; i <= *OrderMa; i++ ) MaFactor[i] = 0.0;

   ArFactor[0] = -1.0;
   MaFactor[0] = -1.0;

   for ( i = 0; i <= pa; i++ )
       for ( j = 0; j <= pr; j++ )
           ArFactor[j+i*Model->sper] -= phir[j] * phia[i];

   for ( i = 0; i <= qa; i++ )
       for ( j = 0; j <= qr; j++ )
           MaFactor[j+i*Model->sper] -= thetar[j] * thetaa[i];

u1:free_vector( tmp, 0, pq );
   free_vector( thetaa, 0, qa );
   free_vector( thetar, 0, qr );
   free_vector( phia, 0, pa );
   free_vector( phir, 0, pr );
}

/*****************************************************************************/

double Cos2( double freq, int sper )

{
   const double PI = 3.141592654;
   double arg;

   arg = 2.0 * PI * freq / sper;
   return( cos( arg ) );
}

/*****************************************************************************/
/*****************************************************************************/

void CalcNonsOp( int sp, int d, int ds, int *ifds, int ord, double *op )

{
   double *pol1, *pol2, *pol3, *pol4;
   int  i, j, k, pp, pp1, pp2;

/*****************************************************************************/
/* [1]: Multiply regular by (complete) annual differences (pol1[pp1]):       */
/*****************************************************************************/

   pp1   = d + ds * sp;
   pol1  = vector( 0, pp1 );
   pol2  = vector( 0, pp1 );
   for ( i = 1; i <= pp1; i++ ) pol1[i] = 0.0;
   pol1[0] = -1.0;
   pp      =    0;

   if ( ds > 0 )
      for ( k = 1; k <= ds; k++ )
          {
          for ( i = 0; i <= pp1; i++ ) pol2[i] = 0.0;
          for ( j = 0; j <= pp + sp; j++ )
              if ( (j >= 0) && (j < sp) )
                 pol2[j] = pol1[j];
              else if ( (j >= sp) && (j <= pp) )
                 pol2[j] = pol1[j] - pol1[j-sp];
              else if ( (j > pp) && (j <= pp + sp) )
                 pol2[j] = -pol1[j-sp];
          pp += sp;
          for ( i = 0; i <= pp; i++ ) pol1[i] = pol2[i];
          }
   if ( d > 0 )
      for ( k = 1; k <= d; k++ )
          {
          for ( i = 0; i <= pp1; i++ ) pol2[i] = 0.0;
          for ( j = 0; j <= pp + 1; j++ )
              if ( (j >= 0) && (j < 1) )
                 pol2[j] = pol1[j];
              else if ( (j >= 1) && (j <= pp) )
                 pol2[j] = pol1[j] - pol1[j-1];
              else if ( (j > pp) && (j <= pp + 1) )
                 pol2[j] = -pol1[j-1];
          pp += 1;
          for ( i = 0; i <= pp; i++ ) pol1[i] = pol2[i];
          }

   free_vector( pol2, 0, pp1 );

/*****************************************************************************/
/* [2]: Multiply individual factors of the annual difference (pol2[pp2]):    */
/*****************************************************************************/

   pp2  = ord - pp1;
   pol2 = vector( 0, pp2 );
   pol3 = vector( 0, pp2 );
   pol4 = vector( 0, 2 );
   for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
   pol3[0] = -1.0;
   pp      =    0;

   if (((sp == 12) && (ifds[0] == 1)) || ((sp == 4) && (ifds[0] == 1)))
      {
      pol4[0] = -1.0;
      pol4[1] =  1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 1; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 1;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }
   if ( (sp == 12) && (ifds[1] == 1) )
      {
      pol4[0] = -1.0;
      pol4[1] = sqrt( 3.0 );
      pol4[2] = -1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 2; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 2;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }
   if ( (sp == 12) && (ifds[2] == 1) )
      {
      pol4[0] = -1.0;
      pol4[1] =  1.0;
      pol4[2] = -1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 2; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 2;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }
   if (((sp == 12) && (ifds[3] == 1)) || ((sp == 4) && (ifds[1] == 1)))
      {
      pol4[0] = -1.0;
      pol4[1] =  0.0;
      pol4[2] = -1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 2; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 2;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }
   if ( (sp == 12) && (ifds[4] == 1) )
      {
      pol4[0] = -1.0;
      pol4[1] = -1.0;
      pol4[2] = -1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 2; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 2;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }
   if ( (sp == 12) && (ifds[5] == 1) )
      {
      pol4[0] = -1.0;
      pol4[1] = -sqrt( 3.0 );
      pol4[2] = -1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 2; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 2;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }
   if (((sp == 12) && (ifds[6] == 1)) || ((sp == 4) && (ifds[2] == 1)))
      {
      pol4[0] = -1.0;
      pol4[1] = -1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 1; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 1;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }

   for ( i = 0; i <= pp; i++ ) pol2[i] = pol3[i];

   free_vector( pol4, 0, 2 );
   free_vector( pol3, 0, pp2 );

/*****************************************************************************/
/* [3]: Multiply pol1 by pol 2 and return non-stationary factors as op:      */
/*****************************************************************************/
/*
   fprintf( outputv, "\n" );
   fprintf( outputv, "Order of the differencing operator: %d\n", pp1 );
   for ( j = 0; j <= pp1; j++ )
       fprintf( outputv, "u1[%2d]: %5.2f\n", j, pol1[j] );
   fprintf( outputv, "\n" );
   fprintf( outputv, "Order of the annual diffs operator: %d\n", pp2 );
   for ( j = 0; j <= pp2; j++ )
       fprintf( outputv, "u2[%2d]: %5.2f\n", j, pol2[j] );
*/
   for ( i = 1; i <= ord; i++ ) op[i] = 0.0;
   op[0] = -1.0;
   for ( i = 0; i <= pp1; i++ )
       for ( j = 0; j <= pp2; j++ ) op[j+i] -= pol2[j] * pol1[i];

   free_vector( pol2, 0, pp2 ); 
   free_vector( pol1, 0, pp1 );
}

/*****************************************************************************/

void calcnu( double *omega, int s, double *delta, int r, double *nu, int lags )

{
   int  i, j;
   double sum1, sum2;

   nu[0] = omega[0];
   for ( j = 1; j <= lags; j++ )
       {
       sum1 = 0.0;
       if ( r > 0 )
          for ( i = 1; i <= j; i++ )
              if ( i <= r ) sum1 = sum1 + delta[i] * nu[j-i];
       sum2 = 0.0;
       if ( s > 0 )
          if ( j <= s ) sum2 = omega[j];
       nu[j] = sum1 - sum2;
       }
}

/*****************************************************************************/

