/***************************************************************************
 *   Copyright (C) 2009-2026 by Arthur B. Treadway & David E. Guerrero     *
 *   abtreadway@telefonica.net                                             *
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

/*****************************************************************************/
/*  FUG: Free Univariate Graphics. High resolution graphics (EPS + PDF) for  */
/*  the identification of univariate time series models.                    */
/*                                                                           */
/*  1.13: single source for Linux, macOS and Windows (see ChangeLog).        */
/*  1.14: own vector graphics (fugdraw): no gnuplot and no pdflatex.         */
/*  1.15: reads the .inp of fue (shared by fug and fue) and writes           */
/*        <input>_fug.out and <input>_fug.pdf.                               */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fug.h"
#include "delop.h"
#include "port.h"
#include "fugplot.h"
#include "inpfile.h"

/* Minimum number of observations left after differencing */
#define MIN_OBS 4

FILE   *outputv;                      /* .out file (used by diagnose.c)     */

struct Tseries Ts;
struct Tusmodel Tm;
int case_a=0, case_b=0, case_c=0, case_d=0, case_e=0, case_h=0;

/*****************************************************************************/
/* Figures of the PDF (graphs -a, -c and -e), in the order they are made    */
/*****************************************************************************/

#define MAX_FIGURES 64

static FDFig *pdf_fig[MAX_FIGURES];
static double pdf_scale[MAX_FIGURES];
static int    npdf = 0;

/* Keep fig (it may be NULL) for the PDF, drawn at the given scale. */
static void keep_figure( FDFig *fig, double scale )
{
   if ( fig == NULL ) return;
   if ( npdf >= MAX_FIGURES ) { fd_fig_free( fig ); return; }
   pdf_fig[npdf]   = fig;
   pdf_scale[npdf] = scale;
   npdf++;
}

/*****************************************************************************/
/* Command line and input file                                               */
/*****************************************************************************/

static void usage( void )
{
   printf( "Usage: fug input [one | set r a] [options]\n" );
   printf( "       input      : input file name (the extension .inp may be omitted): the\n" );
   printf( "                    .inp of fue (its model is not used) or of older fug.\n" );
   printf( "       one        : graphs of the series with the differences of the input\n" );
   printf( "                    file (default).\n" );
   printf( "       set r a    : identification set: graphs for 0..r regular differences\n" );
   printf( "                    and 0..a annual differences.\n" );
   printf( "\n" );
   printf( "Options (flags can be combined, e.g. -ce):\n" );
   printf( "  -a          plot the time series.\n" );
   printf( "  -b          plot the acf/pacf.\n" );
   printf( "  -c          plot the time series and the acf/pacf in one graph.\n" );
   printf( "  -d          plot the histogram.\n" );
   printf( "  -e          plot the mean - standard deviation graph (series in level).\n" );
   printf( "  -h          set: include the original series (lambda = 1) and its\n" );
   printf( "              mean - standard deviation graph.\n" );
   printf( "  -l n        number of acf/pacf lags (default depends on the data).\n" );
   printf( "  -m n        observations per group of the mean - std. dev. graph.\n" );
   printf( "  -f x        scale of the acf/pacf plots (default: automatic).\n" );
   printf( "  -g n        number of estimated ARMA parameters (degrees of freedom of Q).\n" );
   printf( "  -x [min max step]\n" );
   printf( "              estimate the Box-Cox lambda by maximum likelihood\n" );
   printf( "              (default search: -2 2 0.1).\n" );
   printf( "  -v          print the version and exit.\n" );
   printf( "\n" );
   printf( "Example (identification set with the mean - std. dev. graph):\n" );
   printf( "  fug PU set 2 1 -c -h -e\n" );
   printf( "\n" );
   printf( "Results: <input>_fug.out (text), one EPS file for each graph and, with -a\n" );
   printf( "or -c, <input>_fug.pdf with the -a, -c and -e graphs.\n" );
}

static int arg_int( const char *arg, const char *option )
{
   char *end;
   long  v;

   if ( arg == NULL ) { fprintf( stderr, "Error: option %s requires a number.\n", option ); exit( 1 ); }
   v = strtol( arg, &end, 10 );
   if ( end == arg || *end != '\0' || v < 0 || v > 100000 )
      {
      fprintf( stderr, "Error: invalid value '%s' for option %s.\n", arg, option );
      exit( 1 );
      }
   return( (int)v );
}

static double arg_real( const char *arg, const char *option )
{
   char  *end;
   double v;

   if ( arg == NULL ) { fprintf( stderr, "Error: option %s requires a number.\n", option ); exit( 1 ); }
   v = strtod( arg, &end );
   if ( end == arg || *end != '\0' )
      {
      fprintf( stderr, "Error: invalid value '%s' for option %s.\n", arg, option );
      exit( 1 );
      }
   return( v );
}

static int is_number( const char *s )
{
   char *end;

   if ( s == NULL || *s == '\0' ) return( 0 );
   strtod( s, &end );
   return( *end == '\0' );
}

static void input_error( const char *inputf, const char *what )
{
   fprintf( stderr, "Error reading input file %s: %s\n", inputf, what );
   exit( 1 );
}

/* Read the input file: the .inp of fue (its model is skipped) or the .inp
 * of fug <= 1.14 (see inpfile.c). Returns the acf/pacf scale of the file. */
static double read_input( const char *inputf, const char *basename )
{
   InpFile inp;
   char    error[256];
   int     i, n;

   if ( inp_read( inputf, &inp, error, sizeof( error ) ) != 0 )
      {
      if ( strcmp( error, "can not open the file" ) == 0 )
         {
         fprintf( stderr, "Error opening input file: %s\n", inputf );
         exit( 1 );
         }
      input_error( inputf, error );
      }
   Ts.freq    = inp.freq;
   Ts.nobs    = inp.nobs;
   Ts.begtime = inp.begtime;
   Ts.begyear = inp.begyear;
   Ts.outyear = inp.outyear;
   Ts.name    = fug_strdup( inp.name[0] != '\0' ? inp.name : basename );
   Tm.boxlam  = inp.boxlam;
   Tm.boxm    = inp.boxm;
   Tm.boxgeom = inp.boxgeom;
   Tm.nrdiff  = inp.nrdiff;
   Tm.nadiff  = inp.nadiff;
   Tm.ifadf   = NULL;
   if ( Ts.freq > 1 )
      {
      n = Ts.freq / 2;
      Tm.ifadf = ivector( 0, n );
      for ( i = 0; i <= n; i++ ) Tm.ifadf[i] = (i < 7) ? inp.ifadf[i] : 0;
      }
   Ts.data = vector( 1, Ts.nobs );
   for ( i = 1; i <= Ts.nobs; i++ ) Ts.data[i] = inp.data[i - 1];
   inp_free( &inp );
   return( inp.cbands );
}

/*****************************************************************************/
/* Main program                                                              */
/*****************************************************************************/

int main(int argc, char **argv)
{
int i, j, nparma, grapht, m_dt_nog=0, lags =0, status = 0, need;
int max_nrdiff=0, max_nadiff=0;
char *base, *inputf, *outputf, *pdff, *slash;
/* -B SE QUITO (2026-09-20, decision del analista).
 *
 * Daba la transformacion --lambda, m, d, D-- por la LINEA DE ORDENES, pisando
 * la del .inp. Y el .inp ES COMUN A fug Y A fue: hacerle eso convierte el
 * fichero compartido en un fichero que MIENTE. El GUI llego a dejar de
 * actualizarlo a proposito, confiando en -B, asi que se podia cambiar lambda
 * en la ventana y que el .inp de disco --el que fue lee despues-- siguiera
 * diciendo lo de antes.
 *
 * La forma es la del fug 1.13: la linea de ordenes dice QUE GRAFICOS y el
 * barrido "set r a"; la ESPECIFICACION sale del .inp. Un barrido no es una
 * especificacion -- set recorre seis transformaciones y un fichero no puede
 * llevar seis.
 *
 * -B entro el 2026-09-15 con el trabajo de la 1.15, dos dias antes de que
 * este codigo tuviera control de versiones: no hay commit que revisar.    */
int auto_lambda = 0, set_cbands = 0;
double lambda_min = -2.0, lambda_max = 2.0, lambda_step = 0.1;
double file_cbands;

nparma    = 0;
Tm.cbands = 0.0;

   printf( "\n" );
   printf( "FUG %s: CopyLEFT (C) 2009-2026  David E. Guerrero & Arthur B. Treadway\n", FUG_VERSION );
   printf( "Please report bugs <davidesg@ucm.es>\n" );
   printf( "\n" );

   if ( argc == 1 || strcmp( argv[1], "--help" ) == 0 )
      {
      usage();
      return( argc == 1 ? 1 : 0 );
      }
   if ( strcmp( argv[1], "-v" ) == 0 || strcmp( argv[1], "--version" ) == 0 )
      return( 0 );

/*****************************************************************************/
/* A.  Command line                                                          */
/*****************************************************************************/

   grapht = 0;
   for ( i = 2; i <= argc-1; i++ )
       if ( strcmp( argv[i], "one" ) == 0 )
          grapht = 0;
       else if ( strcmp( argv[i], "set" ) == 0 )
          {
          if ( i + 2 >= argc )
             {
             fprintf( stderr, "Error: 'set' requires two arguments: max regular and annual differences.\n" );
             return( 1 );
             }
          max_nrdiff = arg_int( argv[i+1], "set" );
          max_nadiff = arg_int( argv[i+2], "set" );
          grapht = 1;
          i += 2;
          }
       else if ( argv[i][0] == '-' && argv[i][1] != '\0' )
          {
          char *opt = argv[i] + 1;
          switch ( *opt )
             {
             case 'l': lags = arg_int( (i + 1 < argc) ? argv[++i] : NULL, "-l" ); break;
             case 'm': m_dt_nog = arg_int( (i + 1 < argc) ? argv[++i] : NULL, "-m" ); break;
             case 'f': Tm.cbands = arg_real( (i + 1 < argc) ? argv[++i] : NULL, "-f" );
                       set_cbands = 1;
                       break;
             case 'g': nparma = arg_int( (i + 1 < argc) ? argv[++i] : NULL, "-g" ); break;
             case 'x':
                  auto_lambda = 1;
                  if ( i + 3 < argc && is_number( argv[i+1] ) &&
                       is_number( argv[i+2] ) && is_number( argv[i+3] ) )
                     {
                     lambda_min  = arg_real( argv[++i], "-x" );
                     lambda_max  = arg_real( argv[++i], "-x" );
                     lambda_step = arg_real( argv[++i], "-x" );
                     if ( lambda_step <= 0.0 || lambda_max < lambda_min )
                        {
                        fprintf( stderr, "Error: invalid search range for -x.\n" );
                        return( 1 );
                        }
                     }
                  break;
             default:                          /* flags, possibly combined */
                  for ( ; *opt != '\0'; opt++ )
                      switch ( *opt )
                         {
                         case 'a': case_a = 1; break;
                         case 'b': case_b = 1; break;
                         case 'c': case_c = 1; break;
                         case 'd': case_d = 1; break;
                         case 'e': case_e = 1; break;
                         case 'h': case_h = 1; break;
                         default:
                              fprintf( stderr, "Unknown option: -%c (run fug --help)\n", *opt );
                              return( 1 );
                         }
             }
          }
       else
          {
          fprintf( stderr, "Unknown argument: %s (run fug --help)\n", argv[i] );
          return( 1 );
          }

/* File names: fug works in the folder of the input file                     */

   base = fug_strdup( argv[1] );
   slash = strrchr( base, '/' );
#ifdef _WIN32
   {
   char *bslash = strrchr( base, '\\' );
   if ( bslash != NULL && (slash == NULL || bslash > slash) ) slash = bslash;
   }
#endif
   if ( slash != NULL )
      {
      *slash = '\0';
      if ( fug_chdir( (*base != '\0') ? base : "/" ) != 0 )
         {
         fprintf( stderr, "Error: can not change to the folder %s\n", base );
         return( 1 );
         }
      memmove( base, slash + 1, strlen( slash + 1 ) + 1 );
      }
   if ( strlen( base ) > 4 && strcmp( base + strlen( base ) - 4, ".inp" ) == 0 )
      base[strlen( base ) - 4] = '\0';
   if ( *base == '\0' )
      {
      fprintf( stderr, "Error: missing input file name.\n" );
      return( 1 );
      }
   /* The .inp is shared with fue: fug writes <input>_fug.out/.pdf, not the
    * <input>.out/.pdf of fue */
   need = (int)strlen( base ) + 9;
   inputf  = (char *)malloc( need );
   outputf = (char *)malloc( need );
   pdff    = (char *)malloc( need );
   sprintf( inputf,  "%s.inp", base );
   sprintf( outputf, "%s_fug.out", base );
   sprintf( pdff,    "%s_fug.pdf", base );

/*****************************************************************************/
/* B.  Read input file (allocate memory - read model + data ):               */
/*****************************************************************************/

   file_cbands = read_input( inputf, base );
   if ( !set_cbands && file_cbands > 0.0 ) Tm.cbands = file_cbands;

   if ( NULL == (outputv = fopen( outputf, "w" )) )
      {
      fprintf( stderr, "Error opening output file: %s\n", outputf );
      return( 1 );
      }
   fprintf( outputv, "\n");
   fprintf( outputv, "FUG %s: CopyLEFT (C) 2009-2026  Arthur B. Treadway & David E. Guerrero\n", FUG_VERSION );
   fprintf( outputv, "Input file             : %s\n", inputf );
   fprintf( outputv, "Output file            : %s\n", outputf );
   if ( grapht == 1 )
      fprintf( outputv, "Option                 : Identification Plots Set\n");
   else
      fprintf( outputv, "Option                 : Series Plots\n");

   if ( auto_lambda )
      {
      fprintf( outputv, "\n=== AUTOMATIC LAMBDA ESTIMATION ===\n" );
      Tm.boxlam = estimate_lambda_mle( &Ts, Tm.boxm, lambda_min, lambda_max, lambda_step, 1 );
      fprintf( outputv, "Using estimated lambda: %.2f\n", Tm.boxlam );
      printf( "Estimated Box-Cox lambda: %.2f\n", Tm.boxlam );
      }

/* Enough observations for the largest differencing operator?                */

   i = (grapht == 1) ? max_nrdiff : Tm.nrdiff;
   j = (grapht == 1 && Ts.freq > 1) ? max_nadiff : Tm.nadiff;
   if ( Ts.nobs - (i + Ts.freq * j + ifadf_order( Ts.freq, Tm.ifadf )) < MIN_OBS )
      {
      fprintf( stderr, "Error: %d observations are not enough for %d regular and %d annual differences.\n",
               Ts.nobs, i, j );
      return( 1 );
      }
   if ( m_dt_nog <= 0 ) m_dt_nog = (Ts.freq == 12) ? 12 : 8;

/*****************************************************************************/
/* C. Make the graphs and the PDF                                           */
/*****************************************************************************/

   if ( grapht == 0 )
      {
      DiffGraph ( &Ts, nparma, Tm.boxlam, Tm.boxm, Tm.nrdiff, Tm.nadiff, Tm.ifadf, lags, Tm.cbands, m_dt_nog, base );
      if ( case_e == 1 ) interface_m_dt ( &Ts, m_dt_nog, Tm.boxlam, Tm.boxm, base );
      }
   else
      {
      /* Original series (lambda = 1) before the transformed ones */
      if ( ( Tm.boxlam != 1 ) && ( case_h == 1 ) )
         {
         DiffGraph ( &Ts, nparma, 1, 0, 0, 0, Tm.ifadf, lags, Tm.cbands, m_dt_nog, base );
         if ( case_e == 1 ) interface_m_dt ( &Ts, m_dt_nog, 1, 0, base );
         }
      if ( Ts.freq == 1 ) max_nadiff = 0;
      for ( j = 0; j <= max_nadiff; j++ )
          for ( i = 0; i <= max_nrdiff; i++ )
              {
              DiffGraph ( &Ts, nparma, Tm.boxlam, Tm.boxm, i, (Ts.freq == 1) ? Tm.nadiff : j,
                          Tm.ifadf, lags, Tm.cbands, m_dt_nog, base );
              if ( ( i == 0 ) && ( j == 0 ) && ( case_e == 1 ) )
                 interface_m_dt ( &Ts, m_dt_nog, Tm.boxlam, Tm.boxm, base );
              }
      }

   fclose( outputv );

   if ( (case_a == 1) || (case_c == 1) )
      {
      /* A4 pages, landscape for monthly or long series */
      if ( npdf > 0 && fp_write_pdf( pdff, pdf_fig, pdf_scale, npdf,
                                     (Ts.freq == 12) || (Ts.nobs > 200) ) == 0 )
         printf( "Created %s\n", pdff );
      else
         {
         fprintf( stderr, "Warning: %s not created\n", pdff );
         if ( grapht == 1 ) status = 3;  /* the identification set needs the PDF */
         }
      }

   for ( i = 0; i < npdf; i++ ) fd_fig_free( pdf_fig[i] );
   free( inputf );
   free( outputf );
   free( pdff );
   free( base );
   free( Ts.name );
   if ( Tm.ifadf != NULL ) free_ivector( Tm.ifadf, 0, Ts.freq / 2 );
   free_vector( Ts.data, 1, Ts.nobs );
   return( status );
}

/*****************************************************************************/

/* Order of the individual factors of the annual difference (see DelOp()):   */
/* frequency 0 and 6 (or 2 for quarterly data) have order 1, the others 2.  */
int ifadf_order ( int freq, int *ifadf )
{
   int i, order = 0;

   if ( ifadf == NULL ) return( 0 );
   if ( freq == 12 )
      for ( i = 0; i <= 6; i++ ) order += ifadf[i] ? ((i == 0 || i == 6) ? 1 : 2) : 0;
   else if ( freq == 4 )
      for ( i = 0; i <= 2; i++ ) order += ifadf[i] ? ((i == 0 || i == 2) ? 1 : 2) : 0;
   return( order );
}

void DiffGraph ( struct Tseries *ser, int nparma, double boxlam, double boxm, int nrdiff, int nadiff, int *ifadf, int lags, double cbands, int nog, char *outx11 )
{
int i, j,  ornsop, timeout, tsby;
double tmp1, *rnsop, *DataMat;
STRING file_output;
struct Tseries res;

   memset( &res, 0, sizeof( res ) );
   ornsop = nrdiff + ser->freq * nadiff + ifadf_order( ser->freq, ifadf );
   if ( ser->nobs - ornsop < MIN_OBS )
      {
      fprintf( stderr, "Warning: too few observations for %d regular and %d annual differences\n",
               nrdiff, nadiff );
      return;
      }

/* Transform Data */
   DataMat = vector( 1, ser->nobs );
   BoxCox ( ser->data, DataMat, boxlam, boxm, ser->nobs, 1, Tm.boxgeom );

   rnsop = vector( 0, ornsop );
   res.data = vector( 1, ser->nobs - ornsop );

   DelOp( ser->freq, nrdiff, nadiff, ifadf, ornsop, rnsop );

   for ( j = ornsop + 1; j <= ser->nobs; j++ )  /* Apply non-stat. factors: */
       {
       tmp1 = 0.0;
       for ( i = 1; i <= ornsop; i++ ) tmp1 -= rnsop[i] * DataMat[j-i];
       res.data[j-ornsop] = DataMat[j] + tmp1;
       }

   res.name = ser->name;
   res.nobs = ser->nobs - ornsop;
   res.freq = ser->freq;
   ObsToDate( ser->begyear, ser->begtime, ornsop + 1, res.freq,
              &res.begyear, &res.begtime );

   fprintf( outputv, "\n" );
   fprintf( outputv, "________________________________________________________________________________ \n"  );
   fprintf( outputv, "Series Name                : %2s\n", ser->name  );
   fprintf( outputv, "Seasonal period            : %2d\n", ser->freq  );
   fprintf( outputv, "Regular differences        : %2d\n", nrdiff );
   fprintf( outputv, "Annual differences         : %2d\n", nadiff );
   if ( ifadf_order( ser->freq, ifadf ) > 0 )
      {
      fprintf( outputv, "Individual annual factors  :" );
      for ( i = 0; i <= ser->freq / 2; i++ ) fprintf( outputv, " %d", ifadf[i] );
      fprintf( outputv, "\n" );
      }
   fprintf( outputv, "Box-Cox Transformation     : %2.1f\n", boxlam );
   if ( boxlam != 1 )
      {
      fprintf( outputv, "Box-Cox m                  : %g\n", boxm );
      fprintf( outputv, "Box-Cox Geometric          : %2d\n", Tm.boxgeom );
      }
   fprintf( outputv, "________________________________________________________________________________ \n"  );

   File_StatSer( &res );
   File_PlotSer( &res );
   File_HistSer( &res );
   File_CorrSer( &res, nparma, lags );
   if ((nrdiff == 0) && (nadiff == 0 )) DesvMed ( &res, nog );

   if ( ser->begtime == 1 && ser->freq > 1 ) timeout = ornsop;
   else if ( ser->freq > 1 ) timeout = (ornsop+(ser->begtime-1));
   else timeout = (ornsop + ser->outyear);

/* High resolution graphs: <file_output>.eps (+ acf_, hist_ prefixes)       */

   file_output = file_plot ( nrdiff, nadiff,  boxlam, ser->freq, outx11 );
   tsby = (ser->freq > 1) ? ser->begyear : ser->begyear - ser->outyear;

   /* -a and -c write the same file: the PDF gets the one of -c */
   if ( case_a == 1 )
      {
      FDFig *fig = fp_PlotSer( &res, ser->nobs, timeout, tsby, boxlam, nrdiff, nadiff,
                               file_output, ser->name );
      if ( case_c == 1 ) fd_fig_free( fig ); else keep_figure( fig, 0.90 );
      }
   if ( case_b == 1 )
      fd_fig_free( fp_CorrSer( &res, nparma, lags, cbands, file_output, ser->name ) );
   if ( case_c == 1 )
      keep_figure( fp_PlotSer_CorrSer( &res, nparma, ser->nobs, timeout, tsby, boxlam,
                                       nrdiff, nadiff, lags, cbands, file_output, ser->name ), 0.90 );
   if ( case_d == 1 )
      fd_fig_free( fp_histogram( &res, nrdiff, nadiff, boxlam, file_output, ser->name ) );

free( file_output );
free_vector( rnsop, 0, ornsop );
free_vector( res.data, 1, ser->nobs - ornsop);
free_vector( DataMat, 1, ser->nobs );
}


void BoxCox  ( double *DataInput, double *DataOutput, double boxlam, double boxm, int nobs, double refactor, int geometric)

/* Box-Cox transformation of DataInput + boxm. lambda = 1 means no           */
/* transformation (as in FUG <= 1.12.01): the data are used as they are.     */
/* With geometric = 1 the transformation is divided by J^(lambda-1), J being */
/* the geometric mean of the data (J * log(x) for lambda = 0).               */
{
int i;
double Jacob, logsum;

   if ( boxlam == 1.0 )
      {
      for ( i = 1; i <= nobs; i++ ) DataOutput[i] = refactor * DataInput[i];
      return;
      }

   for ( i = 1; i <= nobs; i++ )
       if ( DataInput[i] + boxm <= 0.0 )
          {
          fprintf( stderr, "Error: the Box-Cox transformation (lambda = %g) requires data + m > 0,\n"
                           "but observation %d + m = %g. Use a larger m or lambda = 1.\n",
                   boxlam, i, DataInput[i] + boxm );
          exit( 1 );
          }

   if ( geometric == 1 )
      {
      logsum = 0.0;                   /* geometric mean without overflow */
      for ( i = 1; i <= nobs; i++ ) logsum += log( DataInput[i] + boxm );
      Jacob = exp( logsum / nobs );
      }
   else
      Jacob = 1.0;

   if ( fabs( boxlam ) < 1.0e-10 )
      for ( i = 1; i <= nobs; i++ ) DataOutput[i] = refactor * Jacob * log( DataInput[i] + boxm );
   else
      for ( i = 1; i <= nobs; i++ )
          DataOutput[i] = refactor * (pow( DataInput[i] + boxm, boxlam ) - 1.0) /
                          (boxlam * pow( Jacob, boxlam - 1.0 ));
}


void interface_m_dt ( struct Tseries *ser, int nog, double boxlam, double boxm, char *outx11 )
{
STRING file_output;
double *DataMat;

DataMat = vector( 1, ser->nobs );                      /* Allocate Transf. Time series data: */
BoxCox ( ser->data, DataMat, boxlam, boxm, ser->nobs, 1, Tm.boxgeom );

file_output = file_plot ( 0, 0,  boxlam, ser->freq, outx11 );
keep_figure( fp_graph_m_dt( DataMat, ser->nobs, (nog > 0) ? nog : 8, ser->freq, boxlam,
                            file_output, ser->name ), 0.70 );

free_vector( DataMat, 1, ser->nobs );
free( file_output );
}

STRING file_plot ( int nrdiff, int nadiff, double boxlam, int freq, char *outx11 )

/* Base name of the plots (without extension). The result must be free()d.  */
{
STRING file_output = (STRING)malloc( strlen( outx11 ) + 40 );

	(void)freq;
	if ( nadiff == 0 ) {
		if (boxlam == 1) sprintf ( file_output, "d%d%s", nrdiff, outx11 );
		else if (boxlam == 0) sprintf ( file_output, "d%dln%s", nrdiff, outx11 );
		else sprintf ( file_output, "d%dl%.1f%s", nrdiff, boxlam, outx11 );
		}
	else {
		if (boxlam == 1) sprintf ( file_output, "d%dD%d%s", nrdiff, nadiff, outx11 );
		else if (boxlam == 0) sprintf ( file_output, "d%dD%dln%s", nrdiff, nadiff,  outx11);
		else sprintf ( file_output, "d%dD%dl%.1f%s", nrdiff, nadiff, boxlam, outx11 );
	}

return ( file_output );
}

/*****************************************************************************/

/*****************************************************************************/
/* Box-Cox lambda estimated by maximum likelihood                            */
/*****************************************************************************/

double boxcox_log_likelihood( double *data, int nobs, double boxm, double lambda, int use_geometric )
/* Profile log-likelihood of the Box-Cox transformation of data + boxm. With */
/* the geometric normalization the Jacobian cancels: -n/2 log(variance).     */
{
    double *transformed, x;
    double sum_squares = 0.0, mean_transformed = 0.0, variance, log_sum = 0.0;
    double geometric_mean;
    int i;

    for ( i = 1; i <= nobs; i++ )
        {
        x = data[i] + boxm;
        if ( x <= 0.0 ) return( -1e308 );
        log_sum += log( x );
        }
    geometric_mean = exp( log_sum / nobs );

    transformed = vector( 1, nobs );
    for ( i = 1; i <= nobs; i++ )
        {
        x = data[i] + boxm;
        if ( fabs( lambda ) < 1e-10 )
           transformed[i] = use_geometric ? geometric_mean * log( x ) : log( x );
        else if ( use_geometric )
           transformed[i] = (pow( x, lambda ) - 1.0) / (lambda * pow( geometric_mean, lambda - 1.0 ));
        else
           transformed[i] = (pow( x, lambda ) - 1.0) / lambda;
        mean_transformed += transformed[i];
        }
    mean_transformed /= nobs;
    for ( i = 1; i <= nobs; i++ )
        sum_squares += (transformed[i] - mean_transformed) * (transformed[i] - mean_transformed);
    variance = sum_squares / nobs;
    free_vector( transformed, 1, nobs );

    if ( variance <= 0.0 || variance != variance ) return( -1e308 );   /* 0 or NaN */
    return( -0.5 * nobs * log( variance ) );
}

double estimate_lambda_mle( struct Tseries *ser, double boxm, double lambda_min, double lambda_max, double step, int use_geometric )
{
    double best_lambda = 1.0, best_loglik = -1e308, lambda, loglik;
    int    k, nsteps;

    fprintf( outputv, "\nEstimating optimal Box-Cox lambda using MLE:\n" );
    fprintf( outputv, "Search range: [%.2f, %.2f] with step %.2f\n", lambda_min, lambda_max, step );
    fprintf( outputv, "Geometric transformation: %s\n", use_geometric ? "YES" : "NO" );
    fprintf( outputv, "Box-Cox m: %g\n", boxm );
    fprintf( outputv, "Number of observations: %d\n", ser->nobs );

    for ( k = 1; k <= ser->nobs; k++ )
        if ( ser->data[k] + boxm <= 0.0 )
           {
           fprintf( outputv, "Warning: data + m must be positive. Using lambda=1.0 (no transformation).\n" );
           return( 1.0 );
           }

    nsteps = (int)floor( (lambda_max - lambda_min) / step + 0.5 );
    for ( k = 0; k <= nsteps; k++ )
        {
        lambda = floor( (lambda_min + k * step) * 1000.0 + 0.5 ) / 1000.0;   /* no drift */
        loglik = boxcox_log_likelihood( ser->data, ser->nobs, boxm, lambda, use_geometric );
        if ( loglik > -1e300 )
           {
           fprintf( outputv, "Lambda: %6.2f, Log-Likelihood: %12.6f\n", lambda, loglik );
           if ( loglik > best_loglik )
              {
              best_loglik = loglik;
              best_lambda = lambda;
              }
           }
        else
           fprintf( outputv, "Lambda: %6.2f, Log-Likelihood: %12s\n", lambda, "invalid" );
        }

    if ( best_loglik <= -1e300 )
       {
       fprintf( outputv, "Warning: No valid lambda found in search range. Using lambda=1.0.\n" );
       return( 1.0 );
       }
    best_lambda = floor( best_lambda * 100.0 + 0.5 ) / 100.0;    /* as in the .inp */
    if ( best_lambda == 0.0 ) best_lambda = 0.0;                 /* no -0.00 */
    fprintf( outputv, "OPTIMAL LAMBDA: %.2f with Log-Likelihood: %.6f\n", best_lambda, best_loglik );
    return( best_lambda );
}
