/*****************************************************************************/
/* inpcheck.c -- validation of the .inp file of FUF before it is read.       */
/* Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 or later.        */
/*                                                                           */
/* fuf reads the .inp with fscanf and fgets, and never checked what it read: */
/* a file of fue or fug, a missing value or a word where a number should be  */
/* made fuf corrupt its memory, crash, or write results that mean nothing.   */
/* inp_check() reads the file first with the SAME calls in the SAME order    */
/* as fuf.c [3.0]-[3.7] (so it accepts exactly what fuf reads well), checks  */
/* every value, and says what is wrong and in which line.                    */
/*                                                                           */
/* The .inp of fuf is the one of fue with the forecast horizon and the       */
/* innovation variance after the date: that is what tells them apart.        */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include "inpcheck.h"

#define LABEL_MAX  90             /* fuf reads the labels with fgets( s, 90 )   */
#define MAX_COUNT  1000           /* limit for counts and orders                */

typedef struct {
   FILE   *f;
   char   *msg;
   size_t  size;
} Check;

/* Line number of the current position of the file */
static int line_now( FILE *f )
{
   long pos = ftell( f ), i;
   int  c, line = 1;

   if ( pos < 0 ) return( 0 );
   rewind( f );
   for ( i = 0; i < pos && (c = getc( f )) != EOF; i++ )
       if ( c == '\n' ) line++;
   fseek( f, pos, SEEK_SET );
   return( line );
}

static int fail( Check *k, const char *fmt, ... )
{
   char    text[400];
   va_list ap;

   va_start( ap, fmt );
   vsnprintf( text, sizeof( text ), fmt, ap );
   va_end( ap );
   snprintf( k->msg, k->size, "line %d: %s", line_now( k->f ), text );
   return( 1 );
}

static void skip_space( Check *k )           /* fscanf( f, "\n" )             */
{
   if ( fscanf( k->f, "\n" ) ) {}
}

static int label( Check *k, char *s )
{
   if ( fgets( s, LABEL_MAX, k->f ) == NULL ) return( fail( k, "the file ends too soon" ) );
   return( 0 );
}

/* An integer in [min, max]; then, if nl, the white space after it          */
static int get_int( Check *k, int *v, int min, int max, int nl, const char *what )
{
   if ( fscanf( k->f, "%d", v ) != 1 ) return( fail( k, "%s expected", what ) );
   if ( *v < min || *v > max ) return( fail( k, "%s must be between %d and %d (it is %d)", what, min, max, *v ) );
   if ( nl ) skip_space( k );
   return( 0 );
}

static int get_real( Check *k, double *v, int nl, const char *what )
{
   if ( fscanf( k->f, "%lf", v ) != 1 ) return( fail( k, "%s expected", what ) );
   if ( !isfinite( *v ) ) return( fail( k, "%s is not a finite number", what ) );
   if ( nl ) skip_space( k );
   return( 0 );
}

/* The first character of the next token (after any white space), or EOF   */
static int peek_token( FILE *f )
{
   long pos = ftell( f );
   int  c;

   while ( (c = getc( f )) != EOF && isspace( c ) ) ;
   fseek( f, pos, SEEK_SET );
   return( c );
}

/* A parameter: "value flag", flag 0 (fixed) or 1 (estimated). The flag may
 * be missing at the end of the line: fue then leaves it at 0 (fixed), as the
 * Python port does, but only if the next line is a label: fue would take a
 * number there as the flag, and read the rest of the file wrongly.         */
static int get_param( Check *k, const char *what )
{
   double v;
   int    c, flag;

   if ( get_real( k, &v, 0, what ) ) return( 1 );
   while ( (c = getc( k->f )) == ' ' || c == '\t' || c == '\r' ) ;
   if ( c != EOF ) ungetc( c, k->f );
   if ( c == '\n' || c == EOF )
      {
      c = peek_token( k->f );
      if ( isdigit( c ) || c == '-' || c == '+' || c == '.' )
         return( fail( k, "the flag (0 fixed, 1 estimated) of %s is missing: fue would "
                          "read the next number as the flag", what ) );
      if ( fscanf( k->f, "%d\n", &flag ) ) {}    /* as fue: fails, flag 0 */
      return( 0 );
      }
   if ( get_int( k, &flag, 0, 1, 0, "the flag (0 fixed, 1 estimated)" ) ) return( 1 );
   c = getc( k->f );
   if ( c != EOF ) ungetc( c, k->f );
   if ( c != EOF && !isspace( c ) )
      return( fail( k, "the flag (0 fixed, 1 estimated) of %s must be 0 or 1", what ) );
   skip_space( k );
   return( 0 );
}

/* Numbers in a line of text: count, and whether they are all integers      */
static int count_numbers( const char *line, int *integers )
{
   const char *p = line;
   char       *end;
   int         n = 0;

   *integers = 1;
   for ( ;; )
      {
      double v = strtod( p, &end );
      if ( end == p ) break;
      if ( v != floor( v ) || memchr( p, '.', end - p ) != NULL ) *integers = 0;
      n++;
      p = end;
      }
   while ( isspace( (unsigned char)*p ) ) p++;
   return( *p == '\0' ? n : -1 );            /* -1: something else in the line */
}

static int lower_has( const char *s, const char *word )
{
   char t[LABEL_MAX + 1];
   int  i;

   for ( i = 0; s[i] != '\0' && i < LABEL_MAX; i++ ) t[i] = tolower( (unsigned char)s[i] );
   t[i] = '\0';
   return( strstr( t, word ) != NULL );
}

/* [3.3]: number of operators, their orders, and for each one a label and   */
/* the pairs "value flag"                                                   */
static int check_operators( Check *k, const char *what )
{
   char s[LABEL_MAX];
   int  i, j, num, order[MAX_COUNT + 1];

   if ( label( k, s ) || get_int( k, &num, 0, MAX_COUNT, 0, what ) ) return( 1 );
   for ( i = 1; i <= num; i++ )
       if ( get_int( k, &order[i], 0, MAX_COUNT, 0, "the order of the operator" ) ) return( 1 );
   skip_space( k );
   for ( i = 1; i <= num; i++ )
       {
       if ( label( k, s ) ) return( 1 );
       for ( j = 1; j <= order[i]; j++ )
           if ( get_param( k, "a coefficient of the operator" ) ) return( 1 );
       }
   return( 0 );
}

/* The operators with fixed frequency: number, frequencies, and for each    */
/* one a label and "value flag"                                             */
static int check_fixed_operators( Check *k, const char *what )
{
   char   s[LABEL_MAX];
   int    i, num;
   double x;

   if ( label( k, s ) || get_int( k, &num, 0, MAX_COUNT, 0, what ) ) return( 1 );
   for ( i = 1; i <= num; i++ )
       if ( get_real( k, &x, 0, "the frequency of the operator" ) ) return( 1 );
   skip_space( k );
   for ( i = 1; i <= num; i++ )
       if ( label( k, s ) || get_param( k, "the coefficient of the operator" ) ) return( 1 );
   return( 0 );
}

static int check( Check *k )
{
   char   s[LABEL_MAX], word[256], next[1024];
   int    i, j, freq, nobs, second, year, ndet, nstdet = 0, n, integers, d, D, v;
   int    nomega[MAX_COUNT + 1], ndelta[MAX_COUNT + 1];
   long   pos;
   double x;
   FILE  *f = k->f;

/* [3.0]: five lines that may contain anything                               */

   for ( i = 1; i <= 5; i++ )
       if ( label( k, s ) ) return( 1 );

/* [3.1]: frequency, number of observations, starting date and name         */

   if ( label( k, s ) ) return( 1 );
   if ( fscanf( f, "%255s\n", word ) != 1 ) return( fail( k, "the frequency expected" ) );
   if ( strcmp( word, "number" ) == 0 )
      freq = 1;
   else if ( sscanf( word, "%d", &freq ) != 1 || freq < 1 || strspn( word, "0123456789" ) != strlen( word ) )
      return( fail( k, "the frequency must be 1, 4, 12 or 'number' (it is '%s')", word ) );

   if ( label( k, s ) ) return( 1 );
   if ( get_int( k, &nobs, 1, 10000000, 0, "the number of observations" ) ||
        get_int( k, &second, 0, 100000, 0, (freq > 1) ? "the first season" : "the first observation" ) ||
        get_int( k, &year, -100000, 100000, 0, "the first year" ) )
      return( 1 );
   if ( freq > 1 && (second < 1 || second > freq) )
      return( fail( k, "the first season must be between 1 and %d (it is %d)", freq, second ) );
   if ( fscanf( f, "%255s", word ) != 1 ) return( fail( k, "the name of the series expected" ) );
   if ( fscanf( f, "%255s\n", word ) ) {}                /* name of the residuals */

/* [3.1.1]: forecast horizon and innovation variance. A file of fue has the  */
/* number of deterministic variables here, and one of fug its Box-Cox line.  */

   if ( label( k, s ) ) return( 1 );
   pos = ftell( f );
   if ( fgets( next, sizeof( next ), f ) == NULL ) return( fail( k, "the file ends too soon" ) );
   n = count_numbers( next, &integers );
   fseek( f, pos, SEEK_SET );
   if ( n != 2 )
      {
      if ( n == 1 && integers )
         return( fail( k, "this is an input file of fue (the model, with no forecast "
                          "horizon after the date), not of fuf: estimate it with fue, "
                          "and fue -f writes the input file of fuf" ) );
      if ( lower_has( s, "box-cox" ) || (n >= 3 && n <= 5 && !integers) )
         return( fail( k, "this looks like an input file of fug (Box-Cox line after the "
                          "date), not of fuf" ) );
      return( fail( k, "the forecast horizon and the innovation variance expected" ) );
      }
   if ( get_int( k, &v, 1, 10000, 0, "the forecast horizon" ) ||
        get_real( k, &x, 1, "the innovation variance" ) )
      return( 1 );
   if ( x < 0.0 ) return( fail( k, "the innovation variance can not be negative" ) );

/* [3.2]: deterministic variables                                            */

   if ( label( k, s ) ) return( 1 );
   pos = ftell( f );
   if ( fgets( next, sizeof( next ), f ) == NULL ) return( fail( k, "the file ends too soon" ) );
   n = count_numbers( next, &integers );
   fseek( f, pos, SEEK_SET );
   if ( n != 1 || !integers )
      return( fail( k, "the number of deterministic variables (one integer) expected" ) );
   if ( get_int( k, &ndet, 0, MAX_COUNT, 1, "the number of deterministic variables" ) ) return( 1 );

   if ( ndet > 0 )
      {
      if ( label( k, s ) ) return( 1 );
      for ( i = 1; i <= ndet; i++ )
          {
          if ( fscanf( f, "%255s", word ) != 1 ) return( fail( k, "deterministic variable %d expected", i ) );
          if ( strcmp( word, "impulse" ) == 0 || strcmp( word, "compimp" ) == 0 ||
               strcmp( word, "step" ) == 0 || strcmp( word, "ramp" ) == 0 )
             {
             if ( freq == 1 )
                {
                if ( get_int( k, &v, -100000, 100000, 1, "the year of the intervention" ) ) return( 1 );
                }
             else if ( get_int( k, &v, 1, freq, 0, "the season of the intervention" ) ||
                       get_int( k, &v, -100000, 100000, 1, "the year of the intervention" ) )
                return( 1 );
             }
          else if ( strcmp( word, "easter" ) == 0 && freq == 12 )
             skip_space( k );
          else if ( strcmp( word, "trend" ) == 0 || strcmp( word, "alter" ) == 0 )
             skip_space( k );
          else if ( strcmp( word, "cos" ) == 0 || strcmp( word, "sin" ) == 0 )
             {
             if ( get_real( k, &x, 1, "the harmonic" ) ) return( 1 );
             }
          else if ( isdigit( (unsigned char)word[0] ) || word[0] == '-' || word[0] == '.' || word[0] == '*' )
             return( fail( k, "deterministic variable %d expected (impulse, compimp, step, ramp, "
                              "easter, trend, cos, sin, alter or the name of a variable), not '%s'", i, word ) );
          else                            /* non-standard: one data column    */
             {
             nstdet++;
             if ( fgets( s, LABEL_MAX, f ) == NULL ) return( fail( k, "the file ends too soon" ) );
             }
          }

      if ( label( k, s ) ) return( 1 );                  /* omegas            */
      for ( i = 1; i <= ndet; i++ )
          if ( get_int( k, &nomega[i], 0, MAX_COUNT, 0, "the number of omegas" ) ) return( 1 );
      skip_space( k );
      for ( i = 1; i <= ndet; i++ )
          {
          if ( label( k, s ) ) return( 1 );
          for ( j = 0; j <= nomega[i]; j++ )
              if ( get_param( k, "an omega of a deterministic variable" ) ) return( 1 );
          }
      if ( label( k, s ) ) return( 1 );                  /* deltas            */
      for ( i = 1; i <= ndet; i++ )
          if ( get_int( k, &ndelta[i], 0, MAX_COUNT, 0, "the number of deltas" ) ) return( 1 );
      skip_space( k );
      for ( i = 1; i <= ndet; i++ ) if ( ndelta[i] > 0 )
          {
          if ( label( k, s ) ) return( 1 );
          for ( j = 1; j <= ndelta[i]; j++ )
              if ( get_param( k, "a delta of a deterministic variable" ) ) return( 1 );
          }
      }

/* [3.3]: regular AR, annual AR, regular MA and annual MA operators, and the */
/* AR(2) and MA(2) operators with fixed frequency                            */

   if ( check_operators( k, "the number of regular AR operators" ) ||
        check_operators( k, "the number of annual AR operators" ) ||
        check_operators( k, "the number of regular MA operators" ) ||
        check_operators( k, "the number of annual MA operators" ) ||
        check_fixed_operators( k, "the number of AR(2) operators with fixed frequency" ) ||
        check_fixed_operators( k, "the number of MA(2) operators with fixed frequency" ) )
      return( 1 );

/* [3.4]: mean parameter (its flag may be missing: 0)                        */

   if ( label( k, s ) || get_real( k, &x, 1, "the mean parameter (mu)" ) ) return( 1 );
   if ( fscanf( f, "%d\n", &v ) == 1 && v != 0 && v != 1 )
      return( fail( k, "the flag of the mean must be 0 (fixed) or 1 (estimated)" ) );

/* [3.5]: Box-Cox lambda, regular and annual differences                     */

   if ( label( k, s ) || get_real( k, &x, 0, "the Box-Cox lambda" ) ||
        get_int( k, &d, 0, 100, 0, "the number of regular differences" ) ||
        get_int( k, &D, 0, 100, 1, "the number of annual differences" ) )
      return( 1 );
   if ( nobs - d - freq * D < 3 )
      return( fail( k, "%d observations are not enough for %d regular and %d annual differences",
                    nobs, d, D ) );

/* [3.6]: individual factors of the annual difference                        */

   if ( freq > 1 )
      {
      if ( label( k, s ) ) return( 1 );
      for ( i = 0; i <= freq / 2; i++ )
          if ( get_int( k, &v, 0, 1, 0, "an individual factor of the annual difference (0 or 1)" ) )
             return( 1 );
      skip_space( k );
      }
   else if ( label( k, s ) || label( k, s ) )
      return( 1 );

/* [3.7]: scale of the acf/pacf, rescaling factor and the data               */

   if ( label( k, s ) || get_real( k, &x, 0, "the scale of the acf/pacf (0 automatic)" ) ||
        get_real( k, &x, 1, "the rescaling factor" ) )
      return( 1 );
   if ( label( k, s ) ) return( 1 );
   for ( i = 1; i <= nobs; i++ )
       {
       if ( fscanf( f, "%lf", &x ) != 1 )
          return( fail( k, "observation %d of %d expected", i, nobs ) );
       if ( !isfinite( x ) ) return( fail( k, "observation %d is not a finite number", i ) );
       for ( j = 1; j <= nstdet; j++ )
           if ( get_real( k, &x, 0, "the value of a non-standard deterministic variable" ) )
              return( 1 );
       skip_space( k );
       }
   return( 0 );
}

int inp_check( const char *path, char *msg, size_t size )
{
   Check k;
   int   status;

   k.msg = msg;
   k.size = size;
   if ( size > 0 ) msg[0] = '\0';
   /* EN BINARIO, aunque fue abra en modo texto. Este validador mira
    * adelante y vuelve (ftell/fseek: peek_token, line_now, la linea de
    * despues de la fecha), y en modo texto la biblioteca de Microsoft
    * calcula mal esas posiciones si el fichero NO trae \r\n: en Windows
    * se rechazaban los ficheros con finales LF -- los de la suite en
    * Python, los bajados de GitHub -- con mensajes sin sentido ("the
    * forecast horizon ... (it is 6721390)"). En binario las posiciones son
    * exactas, y el \r de un fichero con \r\n no estorba: fscanf lo salta
    * como espacio. Lo vio la CI de Windows.                             */
   if ( (k.f = fopen( path, "rb" )) == NULL )
      {
      snprintf( msg, size, "can not open the file" );
      return( 1 );
      }
   status = check( &k );
   fclose( k.f );
   return( status );
}
