/***************************************************************************
 *   inpfile.c -- the .inp file (series and model), shared by fug and its  *
 *   GUI. Copyright (C) 2026 by David E. Guerrero & Arthur B. Treadway     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "inpfile.h"

#define LINE_MAX_LEN 1024
#define FUE_MAXSTR   90        /* fue reads its label lines with fgets( s, 90 )  */
#define MAX_COUNT    1000      /* limit for the counts of the model sections     */

static int fail( char *error, size_t size, const char *what )
{
   if ( error != NULL && size > 0 ) snprintf( error, size, "%s", what );
   return( 1 );
}

/* Next line of f that is not blank (as fscanf skips white space). */
static int next_line( FILE *f, char *buf, int size )
{
   char *p;

   while ( fgets( buf, size, f ) != NULL )
      {
      for ( p = buf; *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'; p++ ) ;
      if ( *p != '\0' ) return( 1 );
      }
   return( 0 );
}

/* Numbers at the beginning of line (at most max). */
static int numbers( const char *line, double *v, int max )
{
   const char *p = line;
   char       *end;
   int         n = 0;

   while ( n < max )
      {
      v[n] = strtod( p, &end );
      if ( end == p ) break;
      n++;
      p = end;
      }
   return( n );
}

/*****************************************************************************/
/* Which format: after the date line a fue file has the number of            */
/* deterministic variables (one integer); a fug file has the Box-Cox line    */
/* (3 to 5 values).                                                          */
/*****************************************************************************/

static int is_fue_file( FILE *f )
{
   char   line[LINE_MAX_LEN];
   double v[5];
   int    i;

   for ( i = 1; i <= 5; i++ )
       if ( fgets( line, sizeof( line ), f ) == NULL ) return( 0 );
   if ( fgets( line, sizeof( line ), f ) == NULL ||                /* label */
        !next_line( f, line, sizeof( line ) ) ||                   /* frequency */
        !next_line( f, line, sizeof( line ) ) ||                   /* label */
        !next_line( f, line, sizeof( line ) ) ||                   /* date */
        !next_line( f, line, sizeof( line ) ) ||                   /* label */
        !next_line( f, line, sizeof( line ) ) )
      return( 0 );
   return( numbers( line, v, 5 ) == 1 && v[0] == floor( v[0] ) );
}

/*****************************************************************************/
/* .inp of fug <= 1.14. The Box-Cox line may have the layout of any version: */
/*    lambda nrdiff nadiff            (FUG <= 1.08)                          */
/*    lambda m nrdiff nadiff          (FUG 1.09, 1.12, gtk_fmg)              */
/*    lambda m geom nrdiff nadiff     (FUG 1.10 - 1.12.01)                   */
/*****************************************************************************/

static int read_fug( FILE *f, InpFile *inp, char *error, size_t size )
{
   char   line[LINE_MAX_LEN];
   double v[5];
   int    i, n, nfields, second;
   char  *p, *end;

   for ( i = 1; i <= 5; i++ )
       if ( fgets( line, sizeof( line ), f ) == NULL )
          return( fail( error, size, "the file is too short" ) );

   if ( fgets( line, sizeof( line ), f ) == NULL ||                /* label */
        !next_line( f, line, sizeof( line ) ) ||
        sscanf( line, "%d", &inp->freq ) != 1 || inp->freq < 1 )
      return( fail( error, size, "invalid frequency of the time series" ) );

   if ( !next_line( f, line, sizeof( line ) ) ||                   /* label */
        !next_line( f, line, sizeof( line ) ) ||
        sscanf( line, "%d %d %d %80s", &inp->nobs, &second, &inp->begyear, inp->name ) < 3 ||
        inp->nobs < 1 )
      return( fail( error, size, "invalid number of observations or starting date" ) );
   if ( inp->freq > 1 )
      {
      inp->begtime = second;
      if ( inp->begtime < 1 || inp->begtime > inp->freq )
         return( fail( error, size, "the starting season must be between 1 and the frequency" ) );
      }
   else
      {
      inp->begtime = 1;
      inp->outyear = second;
      }

   if ( !next_line( f, line, sizeof( line ) ) ||                   /* label */
        !next_line( f, line, sizeof( line ) ) )
      return( fail( error, size, "missing Box-Cox and differences line" ) );
   nfields = numbers( line, v, 5 );
   if ( nfields == 3 )
      {
      inp->boxlam = v[0]; inp->nrdiff = (int)v[1]; inp->nadiff = (int)v[2];
      }
   else if ( nfields == 4 )
      {
      inp->boxlam = v[0]; inp->boxm = v[1]; inp->nrdiff = (int)v[2]; inp->nadiff = (int)v[3];
      }
   else if ( nfields == 5 )
      {
      inp->boxlam = v[0]; inp->boxm = v[1]; inp->boxgeom = (int)v[2];
      inp->nrdiff = (int)v[3]; inp->nadiff = (int)v[4];
      }
   else
      return( fail( error, size, "the Box-Cox line needs 3, 4 or 5 values (lambda [m [geom]] d D)" ) );

   if ( inp->freq > 1 )
      {
      n = inp->freq / 2;
      if ( !next_line( f, line, sizeof( line ) ) ||                /* label */
           !next_line( f, line, sizeof( line ) ) )
         return( fail( error, size, "missing individual factors line" ) );
      p = line;
      for ( i = 0; i <= n; i++ )
          {
          long factor = strtol( p, &end, 10 );
          if ( end == p ) break;
          if ( i < 7 ) inp->ifadf[i] = (factor != 0);
          p = end;
          }
      if ( !next_line( f, line, sizeof( line ) ) )                 /* label */
         return( fail( error, size, "missing time series" ) );
      }
   else                                        /* annual data (no factors)   */
      {
      if ( !next_line( f, line, sizeof( line ) ) ||                /* label */
           fgets( line, sizeof( line ), f ) == NULL ||             /* factors */
           fgets( line, sizeof( line ), f ) == NULL )              /* label */
         return( fail( error, size, "missing time series" ) );
      }

   inp->data = (double *)malloc( inp->nobs * sizeof( double ) );
   if ( inp->data == NULL ) return( fail( error, size, "out of memory" ) );
   for ( i = 0; i < inp->nobs; i++ )
       if ( fscanf( f, "%lf", &inp->data[i] ) != 1 )
          {
          snprintf( error, size, "%d observations declared but only %d numbers found",
                    inp->nobs, i );
          return( 1 );
          }
   return( 0 );
}

/*****************************************************************************/
/* .inp of fue, read in the order and with the same calls as fue 1.13.1      */
/* (fue.c, sections [3.0] - [3.7]); the model is skipped. Non-standard       */
/* deterministic variables have one column each after the series.            */
/*****************************************************************************/

/* The ARMA sections: number of operators, their orders, and for each one a
 * label and the pairs "value flag". Returns the number, or -1.            */
static int skip_operators( FILE *f )
{
   char   s[FUE_MAXSTR];
   int    i, j, num, flag, *order;
   double x;

   if ( fgets( s, FUE_MAXSTR, f ) == NULL || fscanf( f, "%d", &num ) != 1 ||
        num < 0 || num > MAX_COUNT )
      return( -1 );
   order = (int *)calloc( num + 1, sizeof( int ) );
   for ( i = 1; i <= num; i++ )
       if ( fscanf( f, "%d", &order[i] ) != 1 || order[i] < 0 || order[i] > MAX_COUNT )
          { free( order ); return( -1 ); }
   if ( fscanf( f, "\n" ) ) {}
   for ( i = 1; i <= num; i++ )
       {
       if ( fgets( s, FUE_MAXSTR, f ) == NULL ) { free( order ); return( -1 ); }
       for ( j = 1; j <= order[i]; j++ )
           if ( fscanf( f, "%lf", &x ) != 1 || fscanf( f, "%d\n", &flag ) != 1 )
              { free( order ); return( -1 ); }
       }
   free( order );
   return( num );
}

/* The sections of the operators with fixed frequency: number of operators,
 * their frequencies, and for each one a label and "value flag".           */
static int skip_fixed_operators( FILE *f )
{
   char   s[FUE_MAXSTR];
   int    i, num, flag;
   double x;

   if ( fgets( s, FUE_MAXSTR, f ) == NULL || fscanf( f, "%d", &num ) != 1 ||
        num < 0 || num > MAX_COUNT )
      return( -1 );
   for ( i = 1; i <= num; i++ )
       if ( fscanf( f, "%lf", &x ) != 1 ) return( -1 );
   if ( fscanf( f, "\n" ) ) {}
   for ( i = 1; i <= num; i++ )
       if ( fgets( s, FUE_MAXSTR, f ) == NULL ||
            fscanf( f, "%lf", &x ) != 1 || fscanf( f, "%d\n", &flag ) != 1 )
          return( -1 );
   return( num );
}

static int read_fue( FILE *f, InpFile *inp, char *error, size_t size )
{
   char   s[FUE_MAXSTR], word[256];
   int    i, j, k, ndet, nstdet = 0, second, i1, i2, flag, num;
   int   *nomega, *ndelta;
   double x, mu;

/* [3.0]: the first five lines may contain anything                          */

   for ( i = 1; i <= 5; i++ )
       if ( fgets( s, FUE_MAXSTR, f ) == NULL )
          return( fail( error, size, "the file is too short" ) );

/* [3.1]: seasonal period, number of observations, starting date and name    */

   if ( fgets( s, FUE_MAXSTR, f ) == NULL || fscanf( f, "%255s\n", word ) != 1 )
      return( fail( error, size, "invalid frequency of the time series" ) );
   if ( strcmp( word, "number" ) == 0 )
      {
      inp->freq = 1;
      inp->numbering = 1;
      }
   else if ( sscanf( word, "%d", &inp->freq ) != 1 || inp->freq < 1 )
      return( fail( error, size, "invalid frequency of the time series" ) );

   if ( fgets( s, FUE_MAXSTR, f ) == NULL ||
        fscanf( f, "%d %d %d", &inp->nobs, &second, &inp->begyear ) != 3 || inp->nobs < 1 )
      return( fail( error, size, "invalid number of observations or starting date" ) );
   if ( fscanf( f, "%80s", inp->name ) != 1 ) inp->name[0] = '\0';
   if ( fscanf( f, "%255s\n", word ) ) {}           /* name of the residuals   */
   if ( inp->freq > 1 )
      {
      inp->begtime = second;
      if ( inp->begtime < 1 || inp->begtime > inp->freq )
         return( fail( error, size, "the starting season must be between 1 and the frequency" ) );
      }
   else
      {
      inp->begtime = 1;
      inp->outyear = second;
      }

/* [3.2]: deterministic variables, their omegas and deltas                   */

   if ( fgets( s, FUE_MAXSTR, f ) == NULL || fscanf( f, "%d\n", &ndet ) != 1 ||
        ndet < 0 || ndet > MAX_COUNT )
      return( fail( error, size, "invalid number of deterministic variables" ) );
   if ( ndet > 0 )
      {
      inp->model = 1;
      if ( fgets( s, FUE_MAXSTR, f ) == NULL )
         return( fail( error, size, "missing deterministic variables" ) );
      for ( i = 1; i <= ndet; i++ )
          {
          if ( fscanf( f, "%255s", word ) != 1 )
             return( fail( error, size, "missing deterministic variables" ) );
          if ( strcmp( word, "impulse" ) == 0 || strcmp( word, "compimp" ) == 0 ||
               strcmp( word, "step" ) == 0 || strcmp( word, "ramp" ) == 0 )
             {
             if ( inp->freq == 1 )
                { if ( fscanf( f, "%d\n", &i2 ) ) {} }
             else
                { if ( fscanf( f, "%d", &i1 ) && fscanf( f, "%d\n", &i2 ) ) {} }
             }
          else if ( strcmp( word, "easter" ) == 0 && inp->freq == 12 )
             { if ( fscanf( f, "\n" ) ) {} }
          else if ( strcmp( word, "trend" ) == 0 || strcmp( word, "alter" ) == 0 )
             { if ( fscanf( f, "\n" ) ) {} }
          else if ( strcmp( word, "cos" ) == 0 || strcmp( word, "sin" ) == 0 )
             { if ( fscanf( f, "%lf\n", &x ) ) {} }
          else                           /* non-standard: one data column    */
             {
             nstdet++;
             if ( fgets( s, FUE_MAXSTR, f ) == NULL )
                return( fail( error, size, "missing deterministic variables" ) );
             }
          }

      nomega = (int *)calloc( ndet + 1, sizeof( int ) );
      ndelta = (int *)calloc( ndet + 1, sizeof( int ) );
      k = ( fgets( s, FUE_MAXSTR, f ) == NULL );
      for ( i = 1; i <= ndet && !k; i++ )
          k = ( fscanf( f, "%d", &nomega[i] ) != 1 || nomega[i] < 0 || nomega[i] > MAX_COUNT );
      if ( fscanf( f, "\n" ) ) {}
      for ( i = 1; i <= ndet && !k; i++ )
          {
          k = ( fgets( s, FUE_MAXSTR, f ) == NULL );
          for ( j = 0; j <= nomega[i] && !k; j++ )
              k = ( fscanf( f, "%lf", &x ) != 1 || fscanf( f, "%d\n", &flag ) != 1 );
          }
      k = k || ( fgets( s, FUE_MAXSTR, f ) == NULL );
      for ( i = 1; i <= ndet && !k; i++ )
          k = ( fscanf( f, "%d", &ndelta[i] ) != 1 || ndelta[i] < 0 || ndelta[i] > MAX_COUNT );
      if ( fscanf( f, "\n" ) ) {}
      for ( i = 1; i <= ndet && !k; i++ ) if ( ndelta[i] > 0 )
          {
          k = ( fgets( s, FUE_MAXSTR, f ) == NULL );
          for ( j = 1; j <= ndelta[i] && !k; j++ )
              k = ( fscanf( f, "%lf", &x ) != 1 || fscanf( f, "%d\n", &flag ) != 1 );
          }
      free( nomega );
      free( ndelta );
      if ( k ) return( fail( error, size, "invalid omegas or deltas of the deterministic variables" ) );
      }

/* [3.3]: regular AR, annual AR, regular MA and annual MA operators, and the */
/* AR(2) and MA(2) operators with fixed frequency                            */

   for ( k = 0; k < 4; k++ )
       {
       if ( (num = skip_operators( f )) < 0 )
          return( fail( error, size, "invalid AR or MA operators" ) );
       if ( num > 0 ) inp->model = 1;
       }
   for ( k = 0; k < 2; k++ )
       {
       if ( (num = skip_fixed_operators( f )) < 0 )
          return( fail( error, size, "invalid operators with fixed frequency" ) );
       if ( num > 0 ) inp->model = 1;
       }

/* [3.4]: mean parameter and its flag                                        */

   flag = 0;
   if ( fgets( s, FUE_MAXSTR, f ) == NULL || fscanf( f, "%lf\n", &mu ) != 1 )
      return( fail( error, size, "missing mean parameter" ) );
   if ( fscanf( f, "%d\n", &flag ) ) {}
   if ( flag == 1 ) inp->model = 1;

/* [3.5]: Box-Cox lambda, regular and annual differences                     */

   if ( fgets( s, FUE_MAXSTR, f ) == NULL || fscanf( f, "%lf", &inp->boxlam ) != 1 ||
        fscanf( f, "%d", &inp->nrdiff ) != 1 || fscanf( f, "%d\n", &inp->nadiff ) != 1 )
      return( fail( error, size, "missing Box-Cox and differences line" ) );

/* [3.6]: individual factors of the annual difference                        */

   if ( inp->freq > 1 )
      {
      if ( fgets( s, FUE_MAXSTR, f ) == NULL )
         return( fail( error, size, "missing individual factors line" ) );
      for ( i = 0; i <= inp->freq / 2; i++ )
          {
          if ( fscanf( f, "%d", &flag ) != 1 )
             return( fail( error, size, "missing individual factors line" ) );
          if ( i < 7 ) inp->ifadf[i] = (flag != 0);
          }
      if ( fscanf( f, "\n" ) ) {}
      }
   else if ( fgets( s, FUE_MAXSTR, f ) == NULL || fgets( s, FUE_MAXSTR, f ) == NULL )
      return( fail( error, size, "missing individual factors line" ) );

/* [3.7]: scale of the acf/pacf, rescaling factor and the data               */

   if ( fgets( s, FUE_MAXSTR, f ) == NULL || fscanf( f, "%lf", &inp->cbands ) != 1 ||
        fscanf( f, "%lf\n", &inp->refactor ) != 1 )
      return( fail( error, size, "missing acf/pacf bands and rescaling factor line" ) );
   if ( inp->refactor == 0.0 ) inp->refactor = 1.0;

   if ( fgets( s, FUE_MAXSTR, f ) == NULL )
      return( fail( error, size, "missing time series" ) );
   inp->data = (double *)malloc( inp->nobs * sizeof( double ) );
   if ( inp->data == NULL ) return( fail( error, size, "out of memory" ) );
   for ( i = 0; i < inp->nobs; i++ )
       {
       if ( fscanf( f, "%lf", &inp->data[i] ) != 1 )
          {
          snprintf( error, size, "%d observations declared but only %d numbers found",
                    inp->nobs, i );
          return( 1 );
          }
       for ( j = 1; j <= nstdet; j++ )
           if ( fscanf( f, "%lf", &x ) != 1 )
              {
              snprintf( error, size, "observation %d: missing the value of the non-standard "
                                     "deterministic variable %d", i + 1, j );
              return( 1 );
              }
       if ( fscanf( f, "\n" ) ) {}
       }
   return( 0 );
}

/*****************************************************************************/

int inp_read( const char *path, InpFile *inp, char *error, size_t size )
{
   FILE *f;
   int   status;

   memset( inp, 0, sizeof( *inp ) );
   inp->refactor = 1.0;
   if ( error != NULL && size > 0 ) error[0] = '\0';
   if ( (f = fopen( path, "r" )) == NULL )
      return( fail( error, size, "can not open the file" ) );
   inp->fue = is_fue_file( f );
   rewind( f );
   status = inp->fue ? read_fue( f, inp, error, size ) : read_fug( f, inp, error, size );
   fclose( f );
   if ( status == 0 && (inp->nrdiff < 0 || inp->nadiff < 0) )
      status = fail( error, size, "negative number of differences" );
   if ( status != 0 ) inp_free( inp );
   return( status );
}

void inp_free( InpFile *inp )
{
   free( inp->data );
   inp->data = NULL;
}

char *inp_format( char *buf, size_t size, double v )
{
   int decimals;

   for ( decimals = 6; decimals <= 17; decimals++ )
       {
       snprintf( buf, size, "%.*f", decimals, v );
       if ( fabs( v ) < 1e21 && strtod( buf, NULL ) == v ) return( buf );
       }
   snprintf( buf, size, "%.17g", v );          /* very large or very small   */
   return( buf );
}

int inp_write_bare( const char *path, const InpFile *inp )
{
   static const char *operators[] = {
      "** Number and orders of regular AR operators:",
      "** Number and orders of annual AR operators:",
      "** Number and orders of regular MA operators:",
      "** Number and orders of anual MA operators:",
      "** Number and frequencies of regular AR(2) operators with fixed frequency:",
      "** Number and frequencies of regular MA(2) operators with fixed frequency:" };
   char  num[512];
   FILE *f;
   int   i;

   if ( (f = fopen( path, "w" )) == NULL ) return( 1 );
   fprintf( f, "************************************************\n" );
   fprintf( f, "* Input file for program FUE                   *\n" );
   fprintf( f, "* DOCTYPE ATSW-interface SYSTEM                *\n" );
   fprintf( f, "************************************************\n\n" );
   fprintf( f, "** Frequency of time series: either 1(A), 4(Q) or 12(M):\n" );
   fprintf( f, " %d\n", inp->freq );
   fprintf( f, "** Number of observations and starting date of time series:\n" );
   fprintf( f, " %d %2d %d %s\n", inp->nobs, (inp->freq > 1) ? inp->begtime : inp->outyear,
            inp->begyear, inp->name );
   fprintf( f, "** Number of deterministic variables (including seasonal components):\n" );
   fprintf( f, "0\n" );
   for ( i = 0; i < 6; i++ ) fprintf( f, "%s\n0\n", operators[i] );
   fprintf( f, "** Mean parameter (mu):\n" );
   fprintf( f, "0.000000 0\n" );
   fprintf( f, "** Box-Cox lambda, regular differences and complete annual differences:\n" );
   fprintf( f, " %s %d %d\n", inp_format( num, sizeof( num ), inp->boxlam ), inp->nrdiff, inp->nadiff );
   fprintf( f, "** Individual factors of the annual difference (starting at freq 0.0):\n" );
   if ( inp->freq > 1 )
      for ( i = 0; i <= inp->freq / 2; i++ ) fprintf( f, " %d", (i < 7) ? inp->ifadf[i] : 0 );
   else
      fprintf( f, " 0" );
   fprintf( f, "\n" );
   fprintf( f, "** ACF/PACF bands (0 Automatic) and reescaling factor:\n" );
   fprintf( f, " %s %.2f\n", inp_format( num, sizeof( num ), inp->cbands ),
            (inp->refactor > 0.0) ? inp->refactor : 1.0 );
   fprintf( f, "** Time series (stochastic and non-standard deterministic variables):\n" );
   for ( i = 0; i < inp->nobs; i++ )
       fprintf( f, "%s\n", inp_format( num, sizeof( num ), inp->data[i] ) );
   return( fclose( f ) != 0 );
}
