/***************************************************************************
 *   fue.c                                                                 *
 *   Exact maximum likelihood estimation of time series models:            *
 *   Copyright (C)2009 by Arthur B Treadway & D.E Guerrero & J.A. Mauricio *
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

#include <math.h>
#include "fue.h"                  /* Header file (prototype declarations).  */
#include "nlatools.h"            /* Header file (prototype declarations)    */
#include "fugplot.h"               /* graphs drawn with fugdraw (no gnuplot) */
#include "inpcheck.h"               /* validation of the .inp, exit status */
#include "equation.h"               /* the equation of the model, and its writers */
#include "report.h"                 /* the report in PDF, drawn without LaTeX  */
#include "rutas.h"                  /* prefixes go on the NAME, not on the path */

double macheps;                      /* Machine epsilon: global variable.   */
FILE *outputv;                     /* Output file: global variable.         */
FILE *texputv;
FILE *preputv;
FILE *distputv;
FILE *restputv;

/*****************************************************************************/

struct Tusmodel Tm;                /* These variables interface with (*cast) */
struct Tseries Ts;                 /* CAUTION: do not declare here variables */
double **DataMat;                    /* whose names are equal to the  names of */
                                   /* variables declared in main or (*cast)! */

/*****************************************************************************/
/*****************************************************************************/

/*****************************************************************************/
/* A model without free parameters (npar = 0): est() would call the          */
/* quasi-Newton optimizer with no parameters, which reads out of its         */
/* vectors. The model is evaluated as in the first and last steps of est()  */
/* (drvmlest.c [1] and [4]): log-likelihood, residuals and sigma2.           */
/*****************************************************************************/

static void eval_model( void (*cast)( real *, struct Tvarma *, int *, int, int ),
                        real *par, real xitol, int chkma, real **a, real *sigma2,
                        real *logelf, int *ifault )
{
   const real LOG2PI = 1.837877066;
   struct Tvarma v;
   real pi1, pi2, pi3;
   int  dummy = 0;

   *ifault = 0;
   v.xitol = xitol;
   v.chkma = chkma;
   (*cast)( par, &v, ifault, 1, 0 );                /* allocate the model    */
   if ( *ifault > 0 ) return;
   elf( v.m, v.n, v.p, v.q, v.mu, v.phi, v.theta, v.qq, v.w, 1.0, v.xitol, v.chkma,
        TRUE, a, &pi1, &pi2, &pi3, ifault );
   if ( *ifault == 0 )
      {
      *logelf = -0.5 * v.m * v.n * ( LOG2PI - log( v.m ) - log( v.n ) + 1.0 )
                - 0.5 * v.n * ( v.m * log( pi1 ) + log( pi2 ) );
      *sigma2 = pi1 / (v.n * v.m);
      }
   (*cast)( par, &v, &dummy, 0, 1 );                /* free it               */
}

int main( int argc, char *argv[] )

{
   const  double PI = 3.141592654;
   const  int  NT = 10;                  /* Maximum number of non-standard   */
                                         /* detvars (handle otherwise).      */
   STRING inputf, outputf, texputf, distputf, restputf, x11out, dvif, preputf, namef, dumstrg;
   int    outyear;
   FILE   *inputv;
   /* dviv: the DVI file of latex + dvips, no longer used */

/* The following variables have to do with the time series model and data:   */

   struct Tvarma varma1;                 /* Standard VARMA structure.        */
   struct Tseries res;                   /* Tseries structure for residuals. */
   int  nstdet, *det, met, chk, geom;
   int  i1, i2, i3, i4;
   double r1;
   int forecast_flag = 0;      /* 1 si se pide -f */
   int forecast_horizon = 24;  /* valor por defecto */
   char base_name[4096];
   int  est_fault = 0;         /* ifault of the estimation (exit status 3) */
   Equation equation;          /* the estimated model, written as an equation */
   FDFig *graph_fig = NULL;    /* the graph of the residuals (report.c)       */
   int  latex_flag = 0;        /* -latex: also compile the .tex with pdflatex */

   void cast_us( double *, struct Tvarma *, int *, int, int );
   void CalcNonsOp( int, int, int, int *, int, double * );

/* The following variables have to do with the quasi-Newton optimizer:       */

   double *x, *dev, **cov, gradtol, steptol;
   int  npar, nparma, maxits, nrits, ifault, i, j;

/*****************************************************************************/
/* [1]: Check and process command-line arguments:                            */
/*****************************************************************************/

   /* Sin buffer: el optimizador escribe la iteracion y el valor de la
    * funcion SIN salto de linea (qnewtopt.c), asi que con el buffer de la
    * tuberia un programa que llame a fue no ve nada hasta el final. Asi
    * ve cada iteracion cuando ocurre. No cambia lo que se escribe.      */
   setvbuf( stdout, NULL, _IONBF, 0 );

   printf( "\n" );
   printf( "FUE 1.14: Copyright (C) 2026 A.B. Treadway & D.E. Guerrero \n" );
   printf( "Non-Final version. May contain errors. Please report\n" );
   printf( "\n" );

/* printf( "INITIAL RAM: %lu\n\n", coreleft() );                             */
   inputf   = NEW_STR( 4096 );
   outputf  = NEW_STR( 4096 );
   x11out   = NEW_STR( 4096 );
   texputf  = NEW_STR( 4096 );
   distputf = NEW_STR( 4096 );
   restputf = NEW_STR( 4096 );
   dvif     = NEW_STR( 4096 );
   preputf  = NEW_STR( 4096 );
   namef    = NEW_STR( 4096 );
   Tm.residuals = NEW_STR( 4096 );
   dumstrg  = NEW_STR( MAXSTR );

   if ( argc == 1 )                     /* No command-line arguments (help): */
      {
      printf( "fue input [eml|aml] [chk|nochk]\n");
      printf( "input      : model-data file name (omit extension .inp)\n" );
      printf( "[eml|aml]  : exact | approximate maximum likelihood (default: eml)\n" );
      printf( "[chk|nochk]: check | do not check for invertibility (default: chk)\n" );
      printf("[-f [horizon]] : generate input file for FUF (forecast)\n");
      printf( "[-latex]   : also compile the .tex file with pdflatex\n" );
      printf( "\nExit status: 0 results written; 1 command line or file error; 2 the input\n" );
      printf( "file is not valid (nothing written); 3 the model could not be estimated\n" );
      printf( "(results written with the initial values); 4 run-time error.\n" );
      exit( FUE_ERR_USAGE );
      }

   if ( argc >= 2 )                     /* Process command-line arguments:   */
      {
      strcpy( inputf, argv[1] );        /* Model and data file name.         */
      strcpy( base_name, argv[1] );   /* guardar nombre base sin extensión */
      met = 1;                          /* Default estimation method (eml).  */
      chk = 1;                          /* Default checking strategy (chk).  */
      geom = 0;				/* Default geometric transform */

      for ( i = 2; i <= argc-1; i++ )
          if ( strcmp( argv[i], "eml" ) == 0 )
             met = 1;
          else if ( strcmp( argv[i], "aml" ) == 0 )
             met = 0;
          else if ( strcmp( argv[i], "chk" ) == 0 )
             chk = 1;
          else if ( strcmp( argv[i], "nochk" ) == 0 )
             chk = 0;
          else if ( strcmp( argv[i], "geom" ) == 0 )
             geom = 1;
         else if ( strcmp( argv[i], "-latex" ) == 0 )
             latex_flag = 1;
         else if ( strcmp( argv[i], "-f" ) == 0 ) {
               forecast_flag = 1;
                  if ( i+1 <= argc-1 && argv[i+1][0] != '-' ) {
                  forecast_horizon = atoi( argv[++i] );
                  if ( forecast_horizon <= 0 ) forecast_horizon = 24;
                  }
               }


      strcpy( outputf, inputf );
      strcpy( x11out, inputf );
      strcpy( texputf, inputf );
      strcpy( dvif, inputf );
//      strcpy( preputf, inputf );
      strcpy( distputf, inputf );
      strcpy( restputf, inputf );
      strcat( inputf, ".inp" );
      strcat( outputf, ".out" );
      strcat( texputf, ".tex" );
      strcat( distputf, "_dist.tex" );
      strcat( restputf, "_res.tex" );
      strcat( dvif, ".dvi" );
//      strcat( preputf, ".pre" );

      if ( !forecast_flag ) {
         strcpy( preputf, base_name );
         strcat( preputf, ".pre" );
      } else {
               /* El prefijo va en el NOMBRE. Con "forecast_%s" y una ruta
                  --fue caso/X -f-- salia "forecast_caso/X.inp", un directorio
                  que no existe, y abortaba. Ver lib/rutas.              */
               ruta_componer( base_name, "forecast_", ".inp", preputf, 4096 );
            }

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
/* [1.1]: Check the input file before any file is written (fue 1.14):       */
/*****************************************************************************/

   {
   char msg[600];

   if ( inp_check( inputf, msg, sizeof( msg ) ) != 0 )
      {
      if ( strcmp( msg, "can not open the file" ) == 0 )
         {
         fprintf( stderr, "Error opening input file: %s\n", inputf );
         exit( FUE_ERR_USAGE );
         }
      fprintf( stderr, "Error in the input file %s, %s\n", inputf, msg );
      exit( FUE_ERR_INPUT );
      }
   }

/*****************************************************************************/
/* [2]: Open output and texput files for writing:                                        */
/*****************************************************************************/

   if ( NULL == (outputv = fopen( outputf, "w" )) )
      {
      fprintf( stderr, "Error opening output file: %s\n", outputf );
      printf( "... Exiting to system ...\n" );
      exit( 1 );
      }
   fprintf( outputv, "\n" );

   if ( NULL == (texputv = fopen( texputf, "w" )) )
      {
      fprintf( stderr, "Error opening output file: %s\n", texputf );
      printf( "... Exiting to system ...\n" );
      exit( 1 );
      }
   fprintf( texputv, "\n" );


  if ( NULL == (restputv = fopen( restputf, "w" )) )
      {
      fprintf( stderr, "Error opening output file: %s\n", restputf );
      printf( "... Exiting to system ...\n" );
      exit( 1 );
      }
   fprintf( restputv, "\n" );


   if ( NULL == (preputv = fopen( preputf, "w" )) ) {
    fprintf( stderr, "Error opening file: %s\n", preputf );
    exit(1);
}
/* Escribir cabecera distinta según el modo */
   fprintf(preputv, "************************************************\n");
   if ( !forecast_flag ) {
    fprintf(preputv, "*        Input file for program DRVUS          *\n");
    fprintf(preputv, "* Copyright (C) 1996 Jose Alberto Mauricio     *\n");
   } else {
    fprintf(preputv, "*        Input file for program FUF            *\n");
    fprintf(preputv, "* Copyright (C) 2009 David E. Guerrero         *\n");
   }
   fprintf(preputv, "************************************************\n\n");
   fprintf(preputv, "** Frequency of time series: either 1(A), 4(Q) or 12(M):\n");

/*****************************************************************************/
/* [3]: Read input file (allocate memory - read model + data - update npar): */
/*****************************************************************************/

   if ( NULL == (inputv = fopen( inputf, "r" )) )
      {
      fprintf( stderr, "Error opening input file: %s\n", inputf );
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

/* [3.2]: Read deterministic structure of time series model:                 */

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%d\n", &Tm.NdetVar );               /* N� of detvars:    */

   DataMat = matrix( 0, Tm.NdetVar, 1, Ts.nobs );       /* Working data set: */

   if ( Tm.NdetVar > 0 )
      {

      fgets( dumstrg, MAXSTR, inputv );

   /* [3.2.0]: Read and generate list of detvars (store as DataMat):         */

      nstdet = 0;                   /* Non-standard detvars (read from input */
      det    = ivector( 1, NT );    /* file as indexed by det):              */

      for ( i = 1; i <= Tm.NdetVar; i++ )
          {
          fscanf( inputv, "%s", dumstrg );
          for ( j = 1; j <= Ts.nobs; j++ ) DataMat[i][j]  = 0.0;

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
             if ( (i3 >= 1) && (i3 <= Ts.nobs) ) DataMat[i][i3] = 1.0;
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
             if ( (i3 >= 1) && (i3 <= Ts.nobs) )   DataMat[i][i3]   =  1.0;
             if ( (i3 >= 0) && (i3+1 <= Ts.nobs) ) DataMat[i][i3+1] = -1.0;
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
             if ( (i3 >= 1) && (i3 <= Ts.nobs) )
                for ( j = i3; j <= Ts.nobs; j++ ) DataMat[i][j] = 1.0;
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
             if ( (i3 >= 1) && (i3 <= Ts.nobs) )
                for ( j = i3; j <= Ts.nobs; j++ ) DataMat[i][j] = j - i3 + 1;
             }

   /* Generate a variable representing the Easter holiday:                   */

          else if ( (strcmp( dumstrg, "easter" ) == 0) && (Ts.freq == 12) )
             {
             for ( j = 1; j <= Ts.nobs; j++ )
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
             for ( j = 1; j <= Ts.nobs; j++ ) DataMat[i][j] = j;
             fscanf( inputv, "\n" );
             }

   /* Generate a cosine seasonal component:                                  */

          else if ( strcmp( dumstrg, "cos" ) == 0 )
             {
             fscanf( inputv, "%lf\n", &r1 );
             for ( j = 1; j <= Ts.nobs; j++ )
                 DataMat[i][j] = cos( 2.0 * PI * r1 / Ts.freq * j );
             }

   /* Generate a sine seasonal component:                                    */

          else if ( strcmp( dumstrg, "sin" ) == 0 )
             {
             fscanf( inputv, "%lf\n", &r1 );
             for ( j = 1; j <= Ts.nobs; j++ )
                 DataMat[i][j] = sin( 2.0 * PI * r1 / Ts.freq * j );
             }

   /* Generate an "alternator" seasonal component:                           */

          else if ( strcmp( dumstrg, "alter" ) == 0 )
             {
             for ( j = 1; j <= Ts.nobs; j++ ) DataMat[i][j] = pow( -1.0, j );
             fscanf( inputv, "\n" );
             }

   /* Read non-standard (unknown) deterministic variable from input file:    */

          else
             {
             nstdet += 1;          /* Update number of non-standard detvars: */
             /* det is ivector( 1, NT ): past NT this was writing outside the
              * block. inpcheck stops it before we get here and says so with
              * the line; this is the belt to that pair of braces.          */
             if ( nstdet > NT )
                {
                fprintf( stderr, "\nError in the input file %s: too many non-standard "
                                 "deterministic variables (%d), and fue takes at most %d\n",
                                 inputf, nstdet, NT );
                exit( 2 );
                }
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
      Tm.Ar1S  = (double **)malloc((size_t)Tm.NumAr1S * sizeof(double *)) - 1;
      Tm.Ia1S  = ivector( 1, Tm.NumAr1S );
      }
      for ( i = 1; i <= Tm.NumAr1S; i++ )
        fscanf( inputv, "%lf", &Tm.pSre1[i] );
   fscanf( inputv, "\n" );

/* [3.3.14]: Read theta for each seasonal AR factor (if any):     */
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
      Tm.Ma1S  = (double **)malloc((size_t)Tm.NumMa1S * sizeof(double *)) - 1;
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
      Tm.Ar2f  = (double **)malloc((size_t)Tm.NumAr2f * sizeof(double *)) - 1;
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
      Tm.Ma2f  = (double **)malloc((size_t)Tm.NumMa2f * sizeof(double *)) - 1;
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
/* [3.7]: Read cbands and refactor:*/

   fgets( dumstrg, MAXSTR, inputv );
   fscanf( inputv, "%lf", &Tm.cbands );
   fscanf( inputv, "%lf\n", &Ts.refactor );
	if (Ts.refactor == 0){Ts.refactor=1;}
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
/*   for ( i = 1; i <= Tm.NumAr1S; i++ ) Tm.Ar1S[i][0]  = -1.0;
/* end DG ********************************************************************/
   for ( i = 1; i <= Tm.NumAr2; i++ )  Tm.Ar2[i][0]   = -1.0;
   for ( i = 1; i <= Tm.NumMa1; i++ )  Tm.Ma1[i][0]   = -1.0;
/* DG 06/20/04 ***************************************************************/
/*   for ( i = 1; i <= Tm.NumMa1S; i++ ) Tm.Ma1S[i][0]  = -1.0;
/* end DG ********************************************************************/
   for ( i = 1; i <= Tm.NumMa2; i++ )  Tm.Ma2[i][0]   = -1.0;
   for ( i = 1; i <= Tm.NumAr1f; i++ ) Tm.Ar1f[i][0]  = -1.0;
/*   for ( i = 1; i <= Tm.NumAr2f; i++ ) Tm.Ar2f[i][0]  = -1.0;             */
   for ( i = 1; i <= Tm.NumMa1f; i++ ) Tm.Ma1f[i][0]  = -1.0;
/*   for ( i = 1; i <= Tm.NumMa2f; i++ ) Tm.Ma2f[i][0]  = -1.0;              */

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

BoxCox ( Ts.data, DataMat[0], Tm.boxlam, 0.0, Ts.nobs, Ts.refactor, geom);

/*
   if ( Tm.boxlam == 0.0 )
      for ( i = 1; i <= Ts.nobs; i++ ) DataMat[0][i] = Ts.refactor*log( Ts.data[i] );
   else if ( Tm.boxlam != 1.0 )
    for ( i = 1; i <= Ts.nobs; i++ ) DataMat[0][i] = Ts.refactor*((pow( Ts.data[i], Tm.boxlam )-1) / Tm.boxlam ) ;
   else
      for ( i = 1; i <= Ts.nobs; i++ ) DataMat[0][i] = Ts.refactor*Ts.data[i];
*/
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
/* [5.9]: theta for seasonal  AR factor (if any):                            */
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
   maxits  = 500;
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

   varma1.sigma2 = 0.0;          /* not computed if the estimation fails   */
   varma1.logelf = 0.0;
   if ( npar > 0 )
      est( cast_us, npar, x, dev, cov, maxits, nrits, gradtol, steptol,
           varma1.xitol, varma1.chkma,
           varma1.a, &varma1.sigma2, &varma1.logelf, &ifault );
   else                          /* nothing to estimate: evaluate the model  */
      eval_model( cast_us, x, varma1.xitol, varma1.chkma,
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
   /* The results are still written (with the initial values, as always),
    * but fue ends with exit status 3 and says why (fue 1.14)               */
   est_fault = ifault;
   if ( ifault )
      {
      static const char *why[] = { "", "matrix Q is not positive definite",
         "the AR operator has at least one unit root",
         "the AR operator is strictly non-stationary",
         "the MA operator is strictly non-invertible",
         "unknown numerical problem", "invalid parameters (see cast_us())" };
      fprintf( stderr, "Error: the model could not be estimated from the initial values "
                       "of %s: %s (ifault %d).\n", inputf,
               (ifault >= 1 && ifault <= 6) ? why[ifault] : "unknown problem", ifault );
      fprintf( outputv, "\n*** THE MODEL COULD NOT BE ESTIMATED FROM THE INITIAL VALUES:\n"
                        "*** %s (ifault %d). The results below are not valid.\n",
               (ifault >= 1 && ifault <= 6) ? why[ifault] : "unknown problem", ifault );
      }

/* [7.3]: Put final estimates into the standard VARMA structure:             */

   cast_us( x, &varma1, &ifault, 0, 0 );

/*****************************************************************************/
/* [8]: Write estimation results to output file (seasonal model):            */
/*****************************************************************************/

   npar = 0;

/* [8.1]: Omegas for each deterministic variable (if any):                   */

   for ( i = 1; i <= Tm.NdetVar; i++ )
       {
       fprintf( outputv, "Omegas for deterministic variable %d:\n", i );
       for ( j = 0; j <= Tm.Nomega[i]; j++ )
           {
           fprintf( outputv, "%14.6f", Tm.Omega[i][j] );
           if ( Tm.Imega[i][j] == 1 )
              {
              npar += 1;
              fprintf( outputv, "  (%6.6f) [%2d]", dev[npar], npar );
              }
           fprintf( outputv, "\n" );
           }
       }

/* [8.2]: Deltas for each deterministic variable (if any):                   */

   for ( i = 1; i <= Tm.NdetVar; i++ ) if ( Tm.Ndelta[i] > 0 )
       {
       fprintf( outputv, "Deltas for deterministic variable %d:\n", i );
       for ( j = 1; j <= Tm.Ndelta[i]; j++ )
           {
           fprintf( outputv, "%14.6f", Tm.Delta[i][j] );
           if ( Tm.Ielta[i][j] == 1 )
              {
              npar += 1;
              fprintf( outputv, "  (%6.6f) [%2d]", dev[npar], npar );
              }
           fprintf( outputv, "\n" );
           }
       }

/* [8.3]: Phis for each regular AR factor (if any):                          */

   for ( i = 1; i <= Tm.NumAr1; i++ )
       {
       fprintf( outputv, "Coefficients for regular AR factor %d:\n", i );
       for ( j = 1; j <= Tm.p1[i]; j++ )
           {
           fprintf( outputv, "%14.6f", Tm.Ar1[i][j] );
           if ( Tm.Ia1[i][j] == 1 )
              {
              npar += 1;
              fprintf( outputv, "  (%6.6f) [%2d]", dev[npar], npar );
              }
           fprintf( outputv, "\n" );
           }
       }


/* DG 06/04/21 ***************************************************************/

/* [8.9]: Thetas for seasonal MA factor (if any):                            */
/*
   for ( i = 1; i <= Tm.NumAr1S; i++ )
       {
	 fprintf( outputv, "Coefficients for seasonal AR factor:\n" );
	 for ( j = 1; j <= Ts.freq-2; j++ )
	   {
	     fprintf( outputv, "%14.6f\n", Tm.Ar1S[i][j] );
	   }
	     fprintf( outputv, "%14.6f", Tm.Ar1S[i][Ts.freq-1] );
	     if ( Tm.Ia1S[i] == 1 )
	       {
		 npar += 1;
		 fprintf( outputv, "  (%6.6f) [%2d]", dev[npar], npar );
	       }
	     fprintf( outputv, "\n" );
       }

/* end DG *********************************************************************/

/* [8.4]: Phis for each annual AR factor (if any):                           */

   for ( i = 1; i <= Tm.NumAr2; i++ )
       {
       fprintf( outputv, "Coefficients for annual AR factor %d:\n", i );
       for ( j = 1; j <= Tm.p2[i]; j++ )
           {
           fprintf( outputv, "%14.6f", Tm.Ar2[i][j] );
           if ( Tm.Ia2[i][j] == 1 )
              {
              npar += 1;
              fprintf( outputv, "  (%6.6f) [%2d]", dev[npar], npar );
              }
           fprintf( outputv, "\n" );
           }
       }

/* [8.5]: Thetas for each regular MA factor (if any):                        */

   for ( i = 1; i <= Tm.NumMa1; i++ )
       {
       fprintf( outputv, "Coefficients for regular MA factor %d:\n", i );
       for ( j = 1; j <= Tm.q1[i]; j++ )
           {
           fprintf( outputv, "%14.6f", Tm.Ma1[i][j] );
           if ( Tm.Im1[i][j] == 1 )
              {
              npar += 1;
              fprintf( outputv, "  (%6.6f) [%2d]", dev[npar], npar );
              }
           fprintf( outputv, "\n" );
           }
       }


/* DG 06/04/20 ***************************************************************/

/* [8.9]: Thetas for seasonal MA factor (if any):                            */
/*
   for ( i = 1; i <= Tm.NumMa1S; i++ )
       {
	 fprintf( outputv, "Coefficients for seasonal MA factor:\n" );
	 for ( j = 1; j <= Ts.freq-2; j++ )
	   {
	     fprintf( outputv, "%14.6f\n", Tm.Ma1S[i][j] );
	   }
	     fprintf( outputv, "%14.6f", Tm.Ma1S[i][Ts.freq-1] );
	     if ( Tm.Im1S[i] == 1 )
	       {
		 npar += 1;
		 fprintf( outputv, "  (%6.6f) [%2d]", dev[npar], npar );
	       }
	     fprintf( outputv, "\n" );
       }

/* end DG *********************************************************************/

/* [8.6]: Thetas for each annual MA factor (if any):                         */

   for ( i = 1; i <= Tm.NumMa2; i++ )
       {
       fprintf( outputv, "Coefficients for annual MA factor %d:\n", i );
       for ( j = 1; j <= Tm.q2[i]; j++ )
           {
           fprintf( outputv, "%14.6f", Tm.Ma2[i][j] );
           if ( Tm.Im2[i][j] == 1 )
              {
              npar += 1;
              fprintf( outputv, "  (%6.6f) [%2d]", dev[npar], npar );
              }
           fprintf( outputv, "\n" );
           }
       }

/* [8.7]: Phis for each regular f-fixed AR factor (if any):                  */

   for ( i = 1; i <= Tm.NumAr1f; i++ )
       {
       fprintf( outputv, "Coefficients for regular f-fixed AR factor %d [f = %3.1f]:\n", i, Tm.pfre1[i] );
       fprintf( outputv, "%14.6f\n", Tm.Ar1f[i][1] );
       fprintf( outputv, "%14.6f", Tm.Ar1f[i][2] );
       if ( Tm.Ia1f[i] == 1 )
          {
          npar += 1;
          fprintf( outputv, "  (%6.6f) [%2d]", dev[npar], npar );
          }
       fprintf( outputv, "\n" );
       }

/* [8.8]: Phis for each annual f-fixed AR factor (if any):                   */
/*
   for ( i = 1; i <= Tm.NumAr2f; i++ )
       {
       fprintf( outputv, "Coefficients for annual f-fixed AR factor %d [f = %3.1f]:\n", i, Tm.pfre2[i] );
       fprintf( outputv, "%14.6f\n", Tm.Ar2f[i][1] );
       fprintf( outputv, "%14.6f", Tm.Ar2f[i][2] );
       if ( Tm.Ia2f[i] == 1 )
          {
          npar += 1;
          fprintf( outputv, "  (%6.6f) [%2d]", dev[npar], npar );
          }
       fprintf( outputv, "\n" );
       }

/* [8.9]: Thetas for each regular f-fixed MA factor (if any):                */

   for ( i = 1; i <= Tm.NumMa1f; i++ )
       {
       fprintf( outputv, "Coefficients for regular f-fixed MA factor %d [f = %3.1f]:\n", i, Tm.qfre1[i] );
       fprintf( outputv, "%14.6f\n", Tm.Ma1f[i][1] );
       fprintf( outputv, "%14.6f", Tm.Ma1f[i][2] );
       if ( Tm.Im1f[i] == 1 )
          {
          npar += 1;
          fprintf( outputv, "  (%6.6f) [%2d]", dev[npar], npar );
          }
       fprintf( outputv, "\n" );
       }

/* [8.10]: Thetas for each annual f-fixed MA factor (if any):                */
/*
   for ( i = 1; i <= Tm.NumMa2f; i++ )
       {
       fprintf( outputv, "Coefficients for annual f-fixed MA factor %d [f = %3.1f]:\n", i, Tm.qfre2[i] );
       fprintf( outputv, "%14.6f\n", Tm.Ma2f[i][1] );
       fprintf( outputv, "%14.6f", Tm.Ma2f[i][2] );
       if ( Tm.Im2f[i] == 1 )
          {
          npar += 1;
          fprintf( outputv, "  (%6.6f) [%2d]", dev[npar], npar );
          }
       fprintf( outputv, "\n" );
       }

/* [8.11]: Mean parameter (if present):                                      */

   fprintf( outputv, "Mean parameter (mu):\n%14.6f", Tm.mu );
   if ( Tm.Imu == 1 )
      {
      npar += 1;
      fprintf( outputv, "  (%6.6f) [%2d]", dev[npar], npar );
      }
   fprintf( outputv, "\n" );

/* [8.12]: Write Box-Cox lambda, non-stationary factors and seasonal period: */

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

/* [8.13]: Write standard ARMA structure (taken from varma1):                */

   fprintf( outputv, "Coefficients of the Autoregressive operator: \n" );
   for ( i = 1; i <= varma1.p; i++ )
       fprintf( outputv, "  phi[%2d]   = %15.10f\n", i, varma1.phi[i][1][1] );
   fprintf( outputv, "Coefficients of the Moving-Average operator: \n" );
   for ( i = 1; i <= varma1.q; i++ )
       fprintf( outputv, "  theta[%2d] = %15.10f\n", i, varma1.theta[i][1][1] );
   fprintf( outputv, "\n" );

/* [8.14]: Write filtered series (taken from varma1):                        */


fprintf( outputv, "Transformed-Differenced-Stochastic series: \n" );
   for ( i = 1; i <= varma1.n; i++ )
       fprintf( outputv, "  w[%3d]    = %15.10f\n", i, varma1.w[1][i] );
   fprintf( outputv, "\n" );
                                                                            


/*****************************************************************************/
/* [9]: Write estimation results to output file (standard VARMA model):      */
/*****************************************************************************/

   fprintf( outputv, "sigma2: %14.10f (%15.10f)\n", varma1.sigma2,
                      varma1.sigma2 * sqrt( 2.0 / (varma1.n * varma1.m)) );
   fprintf( outputv, "sigma : %14.10f\n", sqrt( varma1.sigma2 ) );
   fprintf( outputv, "logelf: %14.10f\n", varma1.logelf );

   fprintf( outputv, "\nSelection Model Criterium:\n" );
	fprintf( outputv, "Hannan-Quinn = %6.2f\n", 
		log( varma1.sigma2 ) + 2 * ( 1 + nparma) * log ( log (Ts.nobs - Tm.ornsop)) / (Ts.nobs - Tm.ornsop) );
	fprintf( outputv, "Schwarz   = %6.2f\n", 
		log( varma1.sigma2 ) + 2 * ( 1 + nparma) * log (Ts.nobs - Tm.ornsop) / (Ts.nobs - Tm.ornsop) );

   fprintf( outputv, "\n" );
   fprintf( outputv, "Estimated covariance matrix:\n\n" );
   for ( i = 1; i <= npar; i++ )
       {
       fprintf( outputv, "x[%2d] ->", i );
       for ( j = 1; j <= i; j++ )
           fprintf( outputv, "%13.9f", cov[i][j]  );
       fprintf( outputv, "\n" );
       }
   fprintf( outputv, "\n" );


   fprintf( outputv, "\n" );
   fprintf( outputv, "Estimated correlation matrix:\n\n" );
   for ( i = 1; i <= npar; i++ )
       {
       fprintf( outputv, "x[%2d] ->", i );
       for ( j = 1; j <= i; j++ )
           fprintf( outputv, "%6.2f", cov[i][j] / (dev[i] * dev[j]) );
       fprintf( outputv, "\n" );
       }
   fprintf( outputv, "\n" );

   fprintf( outputv, "Correlations greater than or equal to 0.7 in absolute value:\n\n" );
   for ( i = 1; i <= npar; i++ ) for ( j = 1; j < i; j++ )
       if ( fabs( cov[i][j] / (dev[i] * dev[j]) ) >= 0.7 )
          fprintf( outputv, "corr[%2d][%2d] =%6.2f\n", i, j, (cov[i][j] / (dev[i] * dev[j])) );



   fprintf( outputv, "\n" );

/*****************************************************************************/
/* [10]: Diagnostic checking of model residuals (use new structure res):     */
/*****************************************************************************/

   res.nobs = Ts.nobs - Tm.ornsop;
   res.freq = Tm.sper;
   res.numbering = Ts.numbering;
   /* El nombre con que se TITULA la serie de residuos. Es identidad, no
      ruta: con "A%s" y un x11out con directorio salia "Acaso/X" en el
      titulo del grafico.                                                */
   if ( strcmp( Tm.residuals, "**" ) == 0 )
      {
      char nm[4096];

      /* ruta_base y NO ruta_nombre: en fue el nombre base puede llevar
         puntos que no son extension -- "DE.2" es el nombre entero, y
         quitarle el ".2" renombraba la serie de ADE.2 a ADE.       */
      ruta_base( x11out, nm, sizeof nm );
      snprintf( Tm.residuals, 4096, "A%s", nm );
      }
//   if (Tm.residuals == " ") res.name = namef;      strcat( inputf, ".inp" );
   res.name = Tm.residuals;
   res.data = vector( 1, res.nobs );
   ObsToDate( Ts.begyear, Ts.begtime, Tm.ornsop + 1, res.freq,
              &res.begyear, &res.begtime );
   for ( i = 1; i <= res.nobs; i++ ) res.data[i] = varma1.a[1][i];
   File_StatSer( &res );
   File_PlotSer( &res );
   File_HistSer( &res );
   File_CorrSer( &res, nparma );

/*
       for ( i = 1; i <= res.nobs; i++ )
           fprintf( outputv,"  %15.8f\n", res.data[i]);
       fprintf( outputv, "\n" );
       
/* DG 09/15/05 **************************************************************/
int timeout;
int lags;
STRING file_output;
file_output = NEW_STR( 4096 );
/* EL FICHERO. Con "A%s" y una ruta salia "Acaso/X.eps": el EPS no se
   escribia, fugplot AVISABA y el informe salia SIN el grafico de residuos.
   Un fallo que solo avisa es mas caro que uno que para.                 */
ruta_componer( x11out, "A", NULL, file_output, 4096 );

    if ( Ts.begtime == 1 && Ts.freq > 1 ) timeout = Tm.ornsop;
    else if ( Ts.freq > 1 ) timeout = (Tm.ornsop+(Ts.begtime-1));
    else if ( Ts.freq ==  1) timeout = (Tm.ornsop + outyear);

   if ( Ts.nobs < 3 * (Ts.freq + 1) )
      lags = Ts.nobs - Ts.freq / 2;
   else if ( Ts.freq == 1)
	{
	if (Ts.nobs > 200) lags = 45;
	else lags =9;
	}
 //    lags =9;
 //  else if (( Ts.freq == 1) && (Ts.nobs > 200))
 //    lags = 45;
   else
      lags = 3 * ( Ts.freq + 1);

/* The graph of the residuals, A<input>.eps, drawn by fugdraw (fugplot.c,    */
/* the same graph as fug -c). The residuals are not transformed: lambda 1    */
/* and no differences, so the title is just their name, as in fue 1.13.1.    */
	if ( Ts.freq > 1 ) 
		graph_fig = fp_PlotSer_CorrSer ( &res, nparma, Ts.nobs, timeout, Ts.begyear, 1.0, 0, 0, lags, Tm.cbands, file_output, res.name );

	else
		graph_fig = fp_PlotSer_CorrSer ( &res, nparma, Ts.nobs, timeout, Ts.begyear - outyear, 1.0, 0, 0, lags, Tm.cbands, file_output, res.name );

FREE_STR( file_output );
/*
   if ( Ts.freq > 1 )
       {
       if ( Ts.begtime > 1)
		{
		File_PlotSer_X11( &res, nparma, Ts.nobs, (Tm.ornsop+Ts.begtime-1), Ts.begyear, Tm.cbands, x11out, Tm.boxlam, Ts.refactor );
		}
	else 
		{
		File_PlotSer_X11( &res, nparma, Ts.nobs, Tm.ornsop, Ts.begyear, Tm.cbands, x11out, Tm.boxlam, Ts.refactor );
		}	
       }
   else
   	{
   	File_PlotSer_X11( &res, nparma, Ts.nobs, Tm.ornsop+outyear, Ts.begyear-outyear, Tm.cbands, x11out, Tm.boxlam, Ts.refactor );
	}

FREE_STR( file_output );	
/****************************************************************************/	
/*File_PlotSer_X11( &res, nparma, Ts.nobs, Tm.ornsop, Ts.begyear, Tm.cbands, x11out );  */
/****************************************************************************/
	
/* DG 06/30/04 **************************************************************/
      Acf_disttex( &res, distputf );
/****************************************************************************/


/*****************************************************************************/
/* [11]: Write Latex file  :      DG 18/02/04                                */
/*****************************************************************************/
/* The equation of the estimated model is built once (src/equation.c) and    */
/* then written; src/eqlatex.c writes it as the LaTeX file that fue has      */
/* always written. The kinds and the dates of the deterministic variables    */
/* are only in the .inp file, so it is opened again.                         */

   if ( NULL == (inputv = fopen( inputf, "r" )) )
      {
      fprintf( stderr, "Error opening input file: %s\n", inputf );
      printf( "... Exiting to system ...\n" );
      exit( FUE_ERR_USAGE );
      }

   eq_build( &equation, inputv, dev, namef );
   equation.sigma = 100 * sqrt( varma1.sigma2 ) / Ts.refactor;
   eq_write_latex( texputv, &equation, x11out );

/* The report in PDF, drawn by fue itself (src/report.c): the graph, the
 * equation and the residuals over three standard deviations. With -latex
 * the .tex file is compiled with pdflatex instead.                         */

   if ( !latex_flag )
      {
      char pdfputf[4096 + 8];

      snprintf( pdfputf, sizeof( pdfputf ), "%s.pdf", base_name );
      if ( report_write_pdf( pdfputf, graph_fig, &equation, &res ) != 0 )
         fprintf( stderr, "Warning: %s was not written\n", pdfputf );
      else
         printf( "\nCreated %s\n", pdfputf );
      }
   eq_free( &equation );
   fd_fig_free( graph_fig );
   free_vector( res.data, 1, res.nobs );



/*****************************************************************************/
/* [11]: That's all, folks !!!:                                              */
/*****************************************************************************/

   fclose( outputv );
   fclose( texputv );
   fclose( inputv  );
   fclose( restputv  );

 if ( latex_flag ) {
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


    // Compile to PDF
    char command[4096 + 64];
    char auxfile[4096 + 8];
    char logfile[4096 + 8];
    #ifdef _WIN32
        snprintf(command, sizeof(command), "pdflatex -interaction=nonstopmode %s", texputf);
    #else
        snprintf(command, sizeof(command), "pdflatex -interaction=nonstopmode %s > /dev/null 2>&1", texputf);
    #endif

    system(command);

    // Cleanup auxiliary files generated by pdflatex
    snprintf(auxfile, sizeof(auxfile), "%s.aux", base_name);
    snprintf(logfile, sizeof(logfile), "%s.log", base_name);
    remove(auxfile);
    remove(logfile);


//   h=latex_init();
//   latex_cmd(h, "%s", x11out) ;
//   latex_close(h) ;
   }
 }
/*
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
*/
/*****************************************************************************/
/* [12]: Writing the prestimation file:                                      */
/*****************************************************************************/


   if ( NULL == (inputv = fopen( inputf, "r" )) )
      {
      fprintf( stderr, "Error opening input file: %s\n", inputf );
      printf( "... Exiting to system ...\n" );
      exit( 1 );
      }

   npar   = 0;                /* Total number of parameters:                 */
   nparma = 0;                /* Number of parameters of the ARMA structure: */

/* [11.1]: Read the first six lines (may contain anything):                   */

   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );
   fgets( dumstrg, MAXSTR, inputv );

   if (Ts.numbering == 1) fprintf(preputv, " number \n" );
   else fprintf(preputv, " %d\n", Ts.freq );

   

   fprintf(preputv, "** Number of observations and starting date of time series:\n");
   fprintf(preputv, " %d", Ts.nobs );
   if ( Ts.freq > 1 )
      {
      fprintf( preputv, " %2d", Ts.begtime );
      fprintf( preputv, " %2d ", Ts.begyear );
      fprintf( preputv, "%2s\n", namef );
      }
   else
      {
      fprintf( preputv, " %2d ", outyear );
      fprintf( preputv, " %2d ", Ts.begyear );
      fprintf( preputv, "%2s\n", namef );
      Ts.begtime = 1;
      }

      if ( forecast_flag ) {
      fprintf( preputv, "** Forecast horizon and estimated innovation variance:\n" );
      fprintf( preputv, "%d  %.10f\n", forecast_horizon, varma1.sigma2 );
      }

   fprintf(preputv, "** Number of deterministic variables (including seasonal components):\n");

   if ( Tm.NdetVar > 0 )
      {
       fgets( dumstrg, MAXSTR, inputv );

        fprintf(preputv, "%d\n", Tm.NdetVar );  
        fprintf(preputv, "**\n");
     

   for ( i = 1; i <= Tm.NdetVar; i++ )
       {
       fscanf( inputv, "%s", dumstrg );

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
             fprintf( preputv, "impulse");
	     if ( Ts.freq > 1){
             fprintf( preputv, " %d",i1);
             fprintf( preputv, " %d\n",i2);
	     }
	  else 
             fprintf( preputv, " %d\n",i2);
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
             fprintf( preputv, "compimp");
	     if ( Ts.freq > 1){
             fprintf( preputv, " %d",i1);
             fprintf( preputv, " %d\n",i2);
	     }
	     else
            fprintf( preputv, " %d\n",i2);
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
             fprintf( preputv, "step");
	     if ( Ts.freq > 1){
             fprintf( preputv, " %d",i1);
             fprintf( preputv, " %d\n",i2);
	     }
	     else
             fprintf( preputv, " %d\n",i2);
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
             fprintf( preputv, "ramp");
	     if ( Ts.freq > 1){
             fprintf( preputv, " %d",i1);
             fprintf( preputv, " %d\n",i2);
	     }
	     else
             fprintf( preputv, " %d\n",i2);
             }


         /* Generate a variable representing the Easter holiday:                */
         /* Sin esta rama el .pre salia con la linea del tipo VACIA, y el       */
         /* fichero dejaba de reproducir su propio modelo: al releerlo, la      */
         /* palabra desaparecida se tomaba por variable NO ESTANDAR.  El .pre   */
         /* es el contrato de continuidad del programa (sus semillas son las    */
         /* estimaciones de la ultima iteracion), asi que perderlo no es un     */
         /* detalle de formato.                                                 */

          else if ( strcmp( dumstrg, "easter" ) == 0 )
             {
             fprintf( preputv, "easter\n");
             }

         /* Generate a deterministic linear trend:                                 */
         /* Ojo: esta rama comparaba con "time", palabra que el GENERADOR de    */
         /* regresores no acepta (fue.c:396 espera "trend"), y ademas escribia  */
         /* en texputv --el fichero LaTeX-- en vez de en preputv.  Nada podia   */
         /* producir un "time" que el generador hubiese entendido, asi que la   */
         /* rama estaba muerta y el trend se perdia igual que el easter.        */

          else if ( strcmp( dumstrg, "trend" ) == 0 )
             {
             fprintf( preputv, "trend\n");
             }

         /* Generate a cosine seasonal component:                                  */

          else if ( strcmp( dumstrg, "cos" ) == 0 )
             {
             fscanf( inputv, "%lf\n", &r1 );
             fprintf( preputv, "cos %1.0f\n", r1 );
             }

         /* Generate a sine seasonal component:                                    */

          else if ( strcmp( dumstrg, "sin" ) == 0 )
             {
             fscanf( inputv, "%lf\n", &r1 );
             fprintf( preputv, "sin %1.0f\n", r1 );
             }

        /* Generate an "alternator" seasonal component:                           */

          else if ( strcmp( dumstrg, "alter" ) == 0 )
             {
               fprintf( preputv, "alter\n");
             }

         /* Variable determinista NO ESTANDAR: sus datos viajan en columnas      */
         /* extra del bloque de la serie -- que el .pre SI escribe.  Sin esta    */
         /* rama se perdia la linea del nombre y el fichero no solo dejaba de    */
         /* reproducir su modelo: releerlo hace SEGFAULTAR al programa.          */
         /* Se escribe la palabra TAL CUAL se leyo, no un "non-standard"         */
         /* generico: el generador toma por no estandar cualquier palabra que no */
         /* conoce, asi que conservarla es fiel y ademas no borra la etiqueta    */
         /* que puso el usuario.  El fgets consume el resto de la linea, igual   */
         /* que hace el generador (fue.c:430).                                   */

          else
             {
             fprintf( preputv, "%s\n", dumstrg );
             fgets( dumstrg, MAXSTR, inputv );
             }

       }
        fprintf(preputv, "**\n");

   for ( i = 1; i <= Tm.NdetVar; i++ ){
          fprintf( preputv, "%d ", Tm.Nomega[i] );
   }
          fprintf( preputv, "\n" );
        fprintf(preputv, "**\n");
   for ( i = 1; i <= Tm.NdetVar; i++ )
       {
       for ( j = 0; j <= Tm.Nomega[i]; j++ )
           {
           fprintf( preputv, "%.6f", Tm.Omega[i][j] );
           if ( Tm.Imega[i][j] == 1 )
              {
              fprintf( preputv, "  1" );
              }
           else{
              fprintf( preputv, "  0" );
           }
           fprintf( preputv, "\n" );
           }
           fprintf( preputv, "**\n" );
       }
   for ( i = 1; i <= Tm.NdetVar; i++ )
	fprintf( preputv, "%d ", Tm.Ndelta[i] );
   fprintf( preputv, "\n" );

   for ( i = 1; i <= Tm.NdetVar; i++ ) if ( Tm.Ndelta[i] > 0 )
          {
          fprintf( preputv, "**\n" );
          for ( j = 1; j <= Tm.Ndelta[i]; j++ )
              {
              fprintf (  preputv, "%.4f", Tm.Delta[i][j] );
	      if ( Tm.Ielta[i][j] == 1 )
		fprintf( preputv, "  1" );
	      else
		fprintf( preputv, "  0" );
	      /* Un par por linea, como en la seccion de omega de arriba. Sin
	       * este salto los pares salian pegados -- "1.8400  1-0.8631  1" --
	       * y el .pre no lo podia releer ni el propio motor, que aborta con
	       * "the flag ... must be 0 or 1". Solo se veia con dos o mas
	       * deltas, y no habia ningun fichero asi en los 115 de tests/.   */
	      fprintf( preputv, "\n" );
              }
          }

   }
   else
     {     
       fprintf( preputv, " 0\n" );

     }

   fprintf( preputv, "**Number and orders of regular AR operators:\n");

   fprintf( preputv, "%d", Tm.NumAr1 );
   if ( Tm.NumAr1 > 0 )
     {
       for ( i = 1; i <= Tm.NumAr1; i++ ){
       fprintf( preputv, " %d", Tm.p1[i] );
       }
       fprintf( preputv, "\n**" );
   for ( i = 1; i <= Tm.NumAr1; i++ )
       {
       for ( j = 1; j <= Tm.p1[i]; j++ )
           {
           fprintf( preputv, "\n%.4f", Tm.Ar1[i][j] );
           if ( Tm.Ia1[i][j] == 1 )
              {
              fprintf( preputv, " 1" );
              }
           else
             {
              fprintf( preputv, " 0" );
             }

           }
           fprintf( preputv, "\n**" );
       }

     }

   else{
           fprintf( preputv, "\n**" );
   }


 /* DG 06/22/04 **********************************************************/
 /*
   fprintf( preputv, " Number and frequencies of seasonal AR operators :\n");

   fprintf( preputv, "%d", Tm.NumAr1S );
   if ( Tm.NumAr1S > 0 )
     {
       for ( i = 1; i <= Tm.NumAr1S; i++ ){
       fprintf( preputv, " %1.0f", Tm.pSre1[i] );
       }
       fprintf( preputv, "\n**" );
   for ( i = 1; i <= Tm.NumAr1S; i++ )
       {
           fprintf( preputv, "\n%.4f", Tm.Ar1S[i][Ts.freq-1] );
           if ( Tm.Ia1S[i] == 1 )
              {
              fprintf( preputv, " 1" );
              }
           else
             {
              fprintf( preputv, " 0" );
             }
            fprintf( preputv, "\n**" );
           }
       }

   else{
           fprintf( preputv, "\n**" );
   }

/* END DG ***********************************************************************/


   fprintf( preputv, " Number and orders of annual AR operators:\n");

   fprintf( preputv, "%d", Tm.NumAr2 );
   if ( Tm.NumAr2 > 0 )
     {
       for ( i = 1; i <= Tm.NumAr2; i++ ){
       fprintf( preputv, " %d", Tm.p2[i] );
       }
       fprintf( preputv, "\n**" );
   for ( i = 1; i <= Tm.NumAr2; i++ )
       {
       for ( j = 1; j <= Tm.p2[i]; j++ )
           {
           fprintf( preputv, "\n%.4f", Tm.Ar2[i][j] );
           if ( Tm.Ia2[i][j] == 1 )
              {
              fprintf( preputv, " 1" );
              }
           else
             {
              fprintf( preputv, " 0" );
             }

           }
           fprintf( preputv, "\n**" );
       }

     }
   else{
           fprintf( preputv, "\n**" );
   }

   fprintf( preputv, " Number and orders of regular MA operators:\n");

   fprintf( preputv, "%d", Tm.NumMa1 );
   if ( Tm.NumMa1 > 0 )
     {
       for ( i = 1; i <= Tm.NumMa1; i++ ){
       fprintf( preputv, " %d", Tm.q1[i] );
       }
       fprintf( preputv, "\n**" );
   for ( i = 1; i <= Tm.NumMa1; i++ )
       {
       for ( j = 1; j <= Tm.q1[i]; j++ )
           {
           fprintf( preputv, "\n%.4f", Tm.Ma1[i][j] );
           if ( Tm.Im1[i][j] == 1 )
              {
              fprintf( preputv, " 1" );
              }
           else
             {
              fprintf( preputv, " 0" );
             }

           }
           fprintf( preputv, "\n**" );
       }

     }
   else{
           fprintf( preputv, "\n**" );
   }

 /* DG 06/21/04 **********************************************************/
/*
   fprintf( preputv, " Number and frequencies of seasonal MA  operators :\n");

   fprintf( preputv, "%d", Tm.NumMa1S );
   if ( Tm.NumMa1S > 0 )
     {
       for ( i = 1; i <= Tm.NumMa1S; i++ ){
       fprintf( preputv, " %1.0f", Tm.qSre1[i] );
       }
       fprintf( preputv, "\n**" );
   for ( i = 1; i <= Tm.NumMa1S; i++ )
       {
           fprintf( preputv, "\n%.4f", Tm.Ma1S[i][Ts.freq-1] );
           if ( Tm.Im1S[i] == 1 )
              {
              fprintf( preputv, " 1" );
              }
           else
             {
              fprintf( preputv, " 0" );
             }
            fprintf( preputv, "\n**" );
           }
       }

   else{
           fprintf( preputv, "\n**" );
   }

/* END DG ***********************************************************************/

   fprintf( preputv, " Number and orders of anual MA operators:\n");

   fprintf( preputv, "%d", Tm.NumMa2 );
   if ( Tm.NumMa2 > 0 )
     {
       for ( i = 1; i <= Tm.NumMa2; i++ ){
       fprintf( preputv, " %d", Tm.q2[i] );
       }
       fprintf( preputv, "\n**" );
   for ( i = 1; i <= Tm.NumMa2; i++ )
       {
       for ( j = 1; j <= Tm.q2[i]; j++ )
           {
           fprintf( preputv, "\n%.4f", Tm.Ma2[i][j] );
           if ( Tm.Im2[i][j] == 1 )
              {
              fprintf( preputv, " 1" );
              }
           else
             {
              fprintf( preputv, " 0" );
             }

           }
           fprintf( preputv, "\n**" );
       }

     }
   else{
           fprintf( preputv, "\n**" );
   }


   fprintf( preputv, " Number and frequencies of regular AR(2) operators with fixed frequency:\n");

   fprintf( preputv, "%d", Tm.NumAr1f );
   if ( Tm.NumAr1f > 0 )
     {
       for ( i = 1; i <= Tm.NumAr1f; i++ ){
       fprintf( preputv, " %1.0f", Tm.pfre1[i] );
       }
       fprintf( preputv, "\n**" );
   for ( i = 1; i <= Tm.NumAr1f; i++ )
       {
           fprintf( preputv, "\n%.4f", Tm.Ar1f[i][2] );
           if ( Tm.Ia1f[i] == 1 )
              {
              fprintf( preputv, " 1" );
              }
           else
             {
              fprintf( preputv, " 0" );
             }
            fprintf( preputv, "\n**" );
           }
       }

   else{
           fprintf( preputv, "\n**" );
   }
/*
   fprintf( preputv, " Number and frequencies of anual AR(2) operators with fixed frequency:\n");

   fprintf( preputv, "%d", Tm.NumAr2f );
   if ( Tm.NumAr2f > 0 )
     {
       for ( i = 1; i <= Tm.NumAr2f; i++ ){
       fprintf( preputv, " %1.0f", Tm.pfre2[i] );
       }
       fprintf( preputv, "\n**" );
   for ( i = 1; i <= Tm.NumAr2f; i++ )
       {
           fprintf( preputv, "\n%.4f", Tm.Ar2f[i][2] );
           if ( Tm.Ia2f[i] == 1 )
              {
              fprintf( preputv, " 1" );
              }
           else
             {
              fprintf( preputv, " 0" );
             }
            fprintf( preputv, "\n**" );
           }
       }

   else{
           fprintf( preputv, "\n**" );
   }
*/
   fprintf( preputv, " Number and frequencies of regular MA(2) operators with fixed frequency:\n");

   fprintf( preputv, "%d", Tm.NumMa1f );
   if ( Tm.NumMa1f > 0 )
     {
       for ( i = 1; i <= Tm.NumMa1f; i++ ){
       fprintf( preputv, " %1.0f", Tm.qfre1[i] );
       }
       fprintf( preputv, "\n**" );
   for ( i = 1; i <= Tm.NumMa1f; i++ )
       {
           fprintf( preputv, "\n%.4f", Tm.Ma1f[i][2] );
           if ( Tm.Im1f[i] == 1 )
              {
              fprintf( preputv, " 1" );
              }
           else
             {
              fprintf( preputv, " 0" );
             }
            fprintf( preputv, "\n**" );
           }
       }

   else{
           fprintf( preputv, "\n**" );
   }

/*
   fprintf( preputv, " Number and frequencies of anual MA(2) operators with fixed frequency:\n");

   fprintf( preputv, "%d", Tm.NumMa2f );
   if ( Tm.NumMa2f > 0 )
     {
       for ( i = 1; i <= Tm.NumMa2f; i++ ){
       fprintf( preputv, " %1.0f", Tm.qfre2[i] );
       }
       fprintf( preputv, "\n**" );
   for ( i = 1; i <= Tm.NumMa2f; i++ )
       {
           fprintf( preputv, "\n%.4f", Tm.Ma2f[i][2] );
           if ( Tm.Im2f[i] == 1 )
              {
              fprintf( preputv, " 1" );
              }
           else
             {
              fprintf( preputv, " 0" );
             }
            fprintf( preputv, "\n**" );
           }
       }

   else{
           fprintf( preputv, "\n**" );
   }
*/
   fprintf( preputv, " Mean parameter (mu):\n");
   if ( Tm.Imu == 1 )
      {
        fprintf( preputv, "%.6f 1", Tm.mu );
      }
   else{
        fprintf( preputv, "0" );
   }

   fprintf( preputv, "\n** Box-Cox lambda, regular differences and complete annual differences:\n");

   fprintf( preputv, "%4.2f", Tm.boxlam );
   fprintf( preputv, " %d", Tm.nrdiff );
   fprintf( preputv, " %d", Tm.nadiff );

   fprintf( preputv, "\n** Individual factors of the annual difference (from freq 0.0): \n");

   if ( Ts.freq > 1 )
      {
      for ( i = 0; i <= Ts.freq / 2; i++ )
          fprintf( preputv, " %d", Tm.ifadf[i] );
      }
   else{
          fprintf( preputv, " 0" );
   }

   fprintf( preputv, "\n** ACF/PACF bands (0 Automatic) and reescaling factor: \n");
   fprintf( preputv, " %.2f", Tm.cbands );
   fprintf( preputv, " %.2f", Ts.refactor );
   fprintf( preputv, "\n** Time series (stochastic and non-standard deterministic variables): \n");

   for ( i = 1; i <= Ts.nobs; i++ )
       {
       fprintf( preputv, "%.10f ", Ts.data[i] );
       if ( Tm.NdetVar > 0 )
          for ( j = 1; j <= nstdet; j++ )
              fprintf( preputv, "   %lf", DataMat[det[j]][i] );
       fprintf( preputv, "\n" );
       }


   fclose( preputv );
   fclose( inputv  );

 /* DG 06/30/04 *********************************************************************/


 /***********************************************************************/

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
       free_ivector( Tm.Ielta[i], 1, Tm.Ndelta[i] );
       free_vector( Tm.Delta[i], 1, Tm.Ndelta[i] );
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
   FREE_STR( distputf );
   FREE_STR( restputf );
   FREE_STR( dvif );
   FREE_STR( preputf );
   FREE_STR( namef );
   FREE_STR( Tm.residuals );
/* printf( "\nFINAL RAM  : %lu\n", coreleft() );                             */
   return( est_fault ? FUE_ERR_ESTIMATE : FUE_OK );
}

/*****************************************************************************/
/*****************************************************************************/

void cast_us( double *x, struct Tvarma *armax,
              int *ifaultx, int firstx, int lastx )

{
   int  i, j, k, itmp;
   int  OrderAr, OrderMa, *NuLag;
   double *vtmp1, *vtmp2, **Nu, tmp1, tmp2;

   void unscramble( struct Tusmodel *, int *, int *, double *, double *, int * );
   void calcnu( double *, int, double *, int, double *, int );

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
/*   for ( i = 1; i <= Tm.NumAr1S; i++ ) OrderAr += Ts.freq-1;
/* END DG *******************************************************************/
   for ( i = 1; i <= Tm.NumAr1f; i++ ) OrderAr += 2;
/*   for ( i = 1; i <= Tm.NumAr2f; i++ ) OrderAr += 2 * Tm.sper;  */

   for ( i = 1; i <= Tm.NumMa1; i++ )  OrderMa += Tm.q1[i];
   for ( i = 1; i <= Tm.NumMa2; i++ )  OrderMa += Tm.q2[i] * Tm.sper;
/* DG 06/21/04 **************************************************************/
/*   for ( i = 1; i <= Tm.NumMa1S; i++ ) OrderMa += Ts.freq-1;
/* END DG *******************************************************************/
   for ( i = 1; i <= Tm.NumMa1f; i++ ) OrderMa += 2;
/*   for ( i = 1; i <= Tm.NumMa2f; i++ ) OrderMa += 2 * Tm.sper; */

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
      armax->a     = matrix( 1, armax->m, 1, armax->n );

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

   vtmp1 = vector( 1, Ts.nobs );

   if ( Tm.NdetVar > 0 )
      {
      NuLag = ivector( 1, Tm.NdetVar );
      Nu    = (double **)calloc((size_t)(Tm.NdetVar + 1), sizeof(double *));
      for ( j = 1; j <= Tm.NdetVar; j++ )
          {
          if ( Tm.Ndelta[j] == 0 )
             NuLag[j] = Tm.Nomega[j];
          else
             NuLag[j] = 40;
          Nu[j] = vector( 0, NuLag[j] );
          calcnu( Tm.Omega[j], Tm.Nomega[j], Tm.Delta[j], Tm.Ndelta[j],
                  Nu[j], NuLag[j] );
          }
      for ( i = 1; i <= Ts.nobs; i++ )           /* Calculate noise:         */
          {
          tmp1 = 0.0;
          for ( j = 1; j <= Tm.NdetVar; j++ )
              {
              tmp2 = 0.0;
              for ( k = 0; k <= NuLag[j]; k++ ) if ( i-k >= 1 )
                  tmp2 += Nu[j][k] * DataMat[j][i-k];
              tmp1 += tmp2;
              }
          vtmp1[i] = DataMat[0][i] - tmp1;
          }
      for ( j = Tm.NdetVar; j >= 1; j-- )
          free_vector( Nu[j], 0, NuLag[j] );
      free( Nu );
      free_ivector( NuLag, 1, Tm.NdetVar );
      }
   else
      for ( i = 1; i <= Ts.nobs; i++ ) vtmp1[i] = DataMat[0][i];

   for ( j = Tm.ornsop + 1; j <= Ts.nobs; j++ )  /* Apply non-stat. factors: */
       {
       tmp1 = 0.0;
       for ( i = 1; i <= Tm.ornsop; i++ ) tmp1 -= Tm.rnsop[i] * vtmp1[j-i];
       armax->w[1][j-Tm.ornsop] = vtmp1[j] + tmp1;
       }

   free_vector( vtmp1, 1, Ts.nobs );

/*****************************************************************************/
/* [6]: Last deallocation of the STANDARD VARMA structure:                   */
/*****************************************************************************/

   if ( lastx == 1 )
      {
      free_matrix( armax->a, 1, armax->m, 1, armax->n );
      free_matrix( armax->w, 1, armax->m, 1, armax->n );
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
/*   for ( i = 1; i <= Model->NumAr1S; i++ ) pr += Ts.freq-1;
/* END DG *********************************************************************/
   for ( i = 1; i <= Model->NumAr1f; i++ ) pr += 2;
   for ( i = 1; i <= Model->NumAr2; i++ )  pa += Model->p2[i];
//   for ( i = 1; i <= Model->NumAr2f; i++ ) pa += 2;
   for ( i = 1; i <= Model->NumMa1; i++ )  qr += Model->q1[i];
/* DG 06/21/04 ****************************************************************/
/*   for ( i = 1; i <= Model->NumMa1S; i++ ) qr += Ts.freq-1;
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
/*         {
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

void BoxCox  ( double *DataInput, double *DataOutput, double boxlam, double boxm, int nobs, double refactor, int geometric)
{
int i;
double product=1;
double Jacob; 

if ( geometric == 0 )
	{
   	if ( boxlam == 0.0 )
      		for ( i = 1; i <=  nobs; i++ ) DataOutput[i] = refactor*log( DataInput[i] + boxm );
   		else if ( boxlam != 1.0 )
      		for ( i = 1; i <=  nobs; i++ ) DataOutput[i] = refactor*(pow( (DataInput[i] + boxm), boxlam )-1)/boxlam;
   		else
      		for ( i = 1; i <=   nobs; i++ ) DataOutput[i] =    refactor*DataInput[i];
	}
else if ( geometric == 1 )
	{
//	expon = 1/nobs;

	for ( i = 1; i <=  nobs; i++ ) product = product*( DataInput[i] + boxm );
	
//	fprintf( outputv, "%13.20f\n\n", expon  );

	Jacob = pow ( product ,   1.0 / nobs );

//	fprintf( outputv, "%13.9f\n\n", Jacob  );

   	if ( boxlam == 0.0 )
      		for ( i = 1; i <=  nobs; i++ ) DataOutput[i] = refactor * Jacob * log( DataInput[i] + boxm );
   	else if ( boxlam != 1.0 )
      		for ( i = 1; i <=  nobs; i++ ) DataOutput[i] = refactor * (pow( (DataInput[i] + boxm), boxlam )-1)/ (boxlam *  pow (  Jacob , boxlam - 1.0 ) );
   	else
		for ( i = 1; i <=   nobs; i++ ) DataOutput[i] =    refactor * DataInput[i];

//   	for ( i = 1; i <= nobs; i++ )fprintf( outputv, "%13.9f\n",  DataOutput[i] );
	
	
	
	}
else  
	for ( i = 1; i <=   nobs; i++ ) DataOutput[i] =    refactor * DataInput[i];
/*
   for ( i = 1; i <= nobs; i++ )
	{
	fprintf( outputv, "%13.9f\n",  DataOutput[i] );
	}
*/
}

