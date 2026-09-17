/*****************************************************************************/
/* equation.c -- the estimated model as an equation (see include/equation.h).*/
/* Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 or later.        */
/*                                                                           */
/* This walks the model once and records what the equation says. It follows  */
/* the order in which fue 1.13.1 wrote the LaTeX file (fue.c, section [11]), */
/* because that order is also the order in which the standard errors of dev  */
/* are used: the n-th parameter written is the n-th one estimated.           */
/*                                                                           */
/* The kinds and the dates of the deterministic variables are only in the    */
/* .inp file (the model keeps their values, not what they are), so they are  */
/* read from it again, exactly as fue read them.                             */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fue.h"
#include "nlatools.h"
#include "equation.h"

extern struct Tseries Ts;
extern struct Tusmodel Tm;

/*****************************************************************************/
/* Building the parts                                                        */
/*****************************************************************************/

static EqItem *add( Equation *eq, EqPart part, EqItemKind kind )
{
   EqItem *it;

   if ( eq->n[part] >= EQ_MAX_ITEMS ) return( &eq->item[part][EQ_MAX_ITEMS - 1] );
   it = &eq->item[part][eq->n[part]++];
   memset( it, 0, sizeof( *it ) );
   it->kind = kind;
   it->sign = ' ';
   return( it );
}

static void add_text( Equation *eq, EqPart part, const char *text )
{
   EqItem *it = add( eq, part, EI_TEXT );

   snprintf( it->text, sizeof( it->text ), "%s", text );
}

static void add_op( Equation *eq, EqPart part, char op )
{
   add( eq, part, EI_OP )->sign = op;
}

static void add_open( Equation *eq, EqPart part, char bracket, int f )
{
   EqItem *it = add( eq, part, EI_OPEN );

   it->bracket = bracket;
   it->a = f;
}

static void add_close( Equation *eq, EqPart part, char bracket )
{
   add( eq, part, EI_CLOSE )->bracket = bracket;
}

/* A coefficient: value (already scaled), its standard error if it was
 * estimated, and the sign written before it (' ' for the first one).       */
static void add_coef( Equation *eq, EqPart part, CoefStyle style, char sign,
                      double value, int estimated, double se )
{
   EqItem *it = add( eq, part, EI_COEF );

   it->style  = style;
   it->sign   = sign;
   it->value  = fabs( value );
   it->has_se = estimated;
   it->se     = se;
}

/* B, or B^power. always: write the power even when it is 1 (the annual
 * factors were written as B^{s} in fue 1.13.1, also with s = 1).           */
static void add_b( Equation *eq, EqPart part, int power, int always )
{
   EqItem *it = add( eq, part, EI_B );

   it->a = power;
   it->b = always;
}

/* The decimals of a coefficient: fue 1.13.1 chose them by its size, so that
 * a small coefficient does not print as 0.00.                              */
int eq_decimals( const EqItem *it )
{
   double v = it->value;

   switch ( it->style )
      {
      case CS_PHI:
      case CS_PHIF:
           return( 2 );
      case CS_DELTA:
           if ( v >= .10 ) return( 2 );
           if ( v >= .01 ) return( 3 );
           return( 4 );
      case CS_OMEGA:
      case CS_MU:
      default:
           if ( v >= .10 )   return( 2 );
           if ( v >= .01 )   return( 3 );
           if ( v >= .001 )  return( 4 );
           if ( v >= .0001 ) return( 5 );
           return( 6 );
      }
}

/*****************************************************************************/
/* [11.2]: the deterministic part: ln X_t = sum omega(B)/delta(B) xi + N_t   */
/*****************************************************************************/

/* The variable itself: what it is and when it happened, read from the .inp */
static void deterministic_variable( Equation *eq, FILE *inputv, const char *kind )
{
   EqItem *it;
   int     season, year;
   double  harmonic;

   if ( strcmp( kind, "impulse" ) == 0 || strcmp( kind, "compimp" ) == 0 ||
        strcmp( kind, "step" ) == 0 || strcmp( kind, "ramp" ) == 0 )
      {
      season = 1;
      if ( Ts.freq == 1 )
         { if ( fscanf( inputv, "%d\n", &year ) != 1 ) year = 0; }
      else if ( fscanf( inputv, "%d", &season ) != 1 || fscanf( inputv, "%d\n", &year ) != 1 )
         year = 0;
      it = add( eq, EQ_DETER, EI_DUMMY );
      snprintf( it->text, sizeof( it->text ), "%s",
                ( strcmp( kind, "impulse" ) == 0 ) ? "I" :
                ( strcmp( kind, "compimp" ) == 0 ) ? "CI" :
                ( strcmp( kind, "step" ) == 0 )    ? "S" : "R" );
      it->a = ( Ts.freq == 1 ) ? 0 : season;         /* 0: only the year     */
      it->b = year;
      }
   else if ( strcmp( kind, "time" ) == 0 )
      {
      add_text( eq, EQ_DETER, "t" );
      if ( fscanf( inputv, "\n" ) ) {}
      }
   else if ( strcmp( kind, "non-standard" ) == 0 )
      {
      it = add( eq, EQ_DETER, EI_DUMMY );            /* xi_t, with no date   */
      it->text[0] = '\0';
      it->a = it->b = 0;
      if ( fscanf( inputv, "\n" ) ) {}
      }
   else if ( strcmp( kind, "season" ) == 0 )
      {
      if ( fscanf( inputv, "%d\n", &season ) != 1 ) season = 0;
      it = add( eq, EQ_DETER, EI_DUMMY );
      snprintf( it->text, sizeof( it->text ), "season" );
      it->a = season;
      it->b = 0;
      if ( fscanf( inputv, "\n" ) ) {}
      }
   else if ( strcmp( kind, "cos" ) == 0 || strcmp( kind, "sin" ) == 0 )
      {
      if ( fscanf( inputv, "%lf\n", &harmonic ) != 1 ) harmonic = 0.0;
      it = add( eq, EQ_DETER, EI_HARMONIC );
      snprintf( it->text, sizeof( it->text ), "%s", kind );
      it->value = harmonic;                          /* which harmonic it is */
      /* harmonic f of a series of frequency s: the angle is f*pi/(s/2) */
      it->a = 1;                                     /* numerator of pi      */
      it->b = 0;
      if ( harmonic == 1 ) { it->a = 1; it->b = ( Ts.freq == 4 ) ? 2 : 6; }
      else if ( harmonic == 2 ) { it->a = 1; it->b = 3; }
      else if ( harmonic == 3 ) { it->a = 1; it->b = 2; }
      else if ( harmonic == 4 ) { it->a = 2; it->b = 3; }
      else if ( harmonic == 5 ) { it->a = 5; it->b = 6; }
      }
   else if ( strcmp( kind, "alter" ) == 0 )
      {
      add( eq, EQ_DETER, EI_ALTER );
      if ( fscanf( inputv, "\n" ) ) {}
      }
   else                                    /* easter, trend, or unknown      */
      {
      it = add( eq, EQ_DETER, EI_DUMMY );
      snprintf( it->text, sizeof( it->text ), "%s", kind );
      it->a = it->b = 0;
      if ( fscanf( inputv, "\n" ) ) {}
      }
}

static void build_deterministic( Equation *eq, FILE *inputv, const double *dev, int *npar )
{
   char   kind[MAXSTR];
   int    i, j;
   double omega0;

   add_text( eq, EQ_DETER, eq->name );          /* ln X_t =                  */
   add_op( eq, EQ_DETER, '=' );

   for ( i = 1; i <= Tm.NdetVar; i++ )
       {
       if ( fscanf( inputv, "%s", kind ) != 1 ) break;
       omega0 = Tm.Omega[i][0];

       /* The sign of the term is the sign of its first omega. fue 1.13.1
        * wrote nothing (not even the parenthesis) when that omega was
        * exactly zero; it is marked so that the LaTeX file does not change. */
       add_op( eq, EQ_DETER, ( omega0 < 0 ) ? '-' : '+' );
       eq->item[EQ_DETER][eq->n[EQ_DETER] - 1].a = ( omega0 == 0 );
       if ( Tm.Nomega[i] > 0 ) add_open( eq, EQ_DETER, '(', 0 );

       for ( j = 0; j <= Tm.Nomega[i]; j++ )
           {
           double v = Tm.Omega[i][j] / Ts.refactor;
           char   sign = ' ';
           int    estimated = Tm.Imega[i][j] == 1;

           if ( j > 0 )                 /* relative to the sign of the term  */
              sign = ( (omega0 > 0) == (Tm.Omega[i][j] > 0) ) ? '-' : '+';
           add_coef( eq, EQ_DETER, CS_OMEGA, sign, v, estimated,
                     estimated ? dev[*npar + 1] / Ts.refactor : 0.0 );
           if ( estimated ) (*npar)++;
           if ( j > 0 ) add_b( eq, EQ_DETER, j, 0 );
           }
       if ( Tm.Nomega[i] > 0 ) add_close( eq, EQ_DETER, ')' );

       if ( Tm.Ndelta[i] > 0 )                  /* / ( 1 - delta B ... )     */
          {
          add_op( eq, EQ_DETER, '/' );
          add_open( eq, EQ_DETER, '(', 0 );
          add_text( eq, EQ_DETER, "1" );
          for ( j = 1; j <= Tm.Ndelta[i]; j++ )
              {
              int estimated = Tm.Ielta[i][j] == 1;

              add_coef( eq, EQ_DETER, CS_DELTA, ( Tm.Delta[i][j] > 0 ) ? '-' : '+',
                        Tm.Delta[i][j], estimated,
                        estimated ? dev[*npar + 1] : 0.0 );
              if ( estimated ) (*npar)++;
              add_b( eq, EQ_DETER, j, 0 );
              }
          add_close( eq, EQ_DETER, ')' );
          }

       deterministic_variable( eq, inputv, kind );
       }

   add_op( eq, EQ_DETER, '+' );
   add_text( eq, EQ_DETER, "N" );               /* + N_t                     */
   eq->used[EQ_DETER] = 1;
}

/*****************************************************************************/
/* [11.3] - [11.6]: the AR and MA factors                                    */
/*****************************************************************************/

/* (1 - phi B - phi B^2 ...) for each factor; lag is 1 for the regular
 * factors and the frequency of the series for the annual ones.             */
static void build_factors( Equation *eq, EqPart part, const double *dev, int *npar,
                           int nfactors, const int *order, real **coef, int **flag,
                           int lag )
{
   int i, j;

   for ( i = 1; i <= nfactors; i++ )
       {
       add_open( eq, part, '(', 0 );
       add_text( eq, part, "1" );
       for ( j = 1; j <= order[i]; j++ )
           {
           int estimated = flag[i][j] == 1;

           add_coef( eq, part, CS_PHI, ( coef[i][j] > 0 ) ? '-' : '+',
                     coef[i][j], estimated, estimated ? dev[*npar + 1] : 0.0 );
           if ( estimated ) (*npar)++;
           add_b( eq, part, j * lag, lag > 1 );
           }
       add_close( eq, part, ')' );
       }
   eq->used[part] = 1;
}

/* [11.7], [11.9]: underset{f = k}{(1 - phi B - phi B^2)}, the factors whose
 * frequency is fixed. With f = 3 the term in B is not written (its phi is 0). */
static void build_fixed_factors( Equation *eq, EqPart part, const double *dev, int *npar,
                                 int nfactors, const real *freq, real **coef, const int *flag )
{
   int i;

   for ( i = 1; i <= nfactors; i++ )
       {
       int estimated = flag[i] == 1;

       add_open( eq, part, 'f', (int)freq[i] );
       add_text( eq, part, "1" );
       if ( freq[i] != 3 )
          {
          add_coef( eq, part, CS_PHIF, ( coef[i][1] > 0 ) ? '-' : '+',
                    coef[i][1], 0, 0.0 );         /* never has its own s.e.  */
          add_b( eq, part, 1, 0 );
          }
       add_coef( eq, part, CS_PHIF, ( coef[i][2] > 0 ) ? '-' : '+',
                 coef[i][2], estimated, estimated ? dev[*npar + 1] : 0.0 );
       if ( estimated ) (*npar)++;
       add_b( eq, part, 2, 0 );
       add_close( eq, part, ')' );
       }
   eq->used[part] = 1;
}

/*****************************************************************************/

void eq_build( Equation *eq, FILE *inputv, const double *dev, const char *name )
{
   char dumstrg[MAXSTR];
   int  i, npar = 0;

   memset( eq, 0, sizeof( *eq ) );
   for ( i = 0; i < EQ_NPARTS; i++ )
       {
       eq->item[i] = (EqItem *)calloc( EQ_MAX_ITEMS, sizeof( EqItem ) );
       if ( eq->item[i] == NULL ) nrerror( "ALLOCATION FAILURE in eq_build()" );
       }

   snprintf( eq->name, sizeof( eq->name ), "%s", name ? name : "" );
   snprintf( eq->residuals, sizeof( eq->residuals ), "%s", Tm.residuals ? Tm.residuals : "" );
   eq->is_log = ( Tm.boxlam == 0 );
   eq->freq   = Ts.freq;
   eq->nobs   = Ts.nobs;

/* [11.1]: the eleven lines of the .inp before the deterministic variables   */

   for ( i = 1; i <= 11; i++ )
       if ( fgets( dumstrg, MAXSTR, inputv ) == NULL ) break;

   if ( Tm.NdetVar > 0 )
      {
      if ( fgets( dumstrg, MAXSTR, inputv ) != NULL )     /* the "**" line   */
         build_deterministic( eq, inputv, dev, &npar );
      }

   /* fue 1.13.1 only wrote the regular AR factors when the first phi of the
    * first factor was estimated; the parameters of the others then keep
    * their place in dev, so this is kept as it was.                         */
   if ( Tm.NumAr1 > 0 && Tm.Ia1[1][1] == 1 )
      build_factors( eq, EQ_ARR, dev, &npar, Tm.NumAr1, Tm.p1, Tm.Ar1, Tm.Ia1, 1 );
   if ( Tm.NumAr2 > 0 )
      build_factors( eq, EQ_ARA, dev, &npar, Tm.NumAr2, Tm.p2, Tm.Ar2, Tm.Ia2, Ts.freq );
   if ( Tm.NumMa1 > 0 )
      build_factors( eq, EQ_MAR, dev, &npar, Tm.NumMa1, Tm.q1, Tm.Ma1, Tm.Im1, 1 );
   if ( Tm.NumMa2 > 0 )
      build_factors( eq, EQ_MAA, dev, &npar, Tm.NumMa2, Tm.q2, Tm.Ma2, Tm.Im2, Ts.freq );
   if ( Tm.NumAr1f > 0 )
      build_fixed_factors( eq, EQ_ARF, dev, &npar, Tm.NumAr1f, Tm.pfre1, Tm.Ar1f, Tm.Ia1f );
   if ( Tm.NumMa1f > 0 )
      build_fixed_factors( eq, EQ_MAF, dev, &npar, Tm.NumMa1f, Tm.qfre1, Tm.Ma1f, Tm.Im1f );

/* [8.11]: the mean, inside the bracket: [ ... - mu ]                        */

   if ( Tm.Imu == 1 )
      {
      add_coef( eq, EQ_MU, CS_MU, ( Tm.mu > 0 ) ? '-' : '+', Tm.mu / Ts.refactor,
                1, dev[npar + 1] / Ts.refactor );
      npar++;
      eq->used[EQ_MU] = 1;
      }

/* [10.6]: the non-stationary factors: nabla^d, nabla_s^D and the individual
 * factors of the annual difference                                          */

   if ( Tm.nrdiff > 0 )
      {
      EqItem *it = add( eq, EQ_NRDIFF, EI_NABLA );
      it->a = Tm.nrdiff;                              /* power               */
      eq->used[EQ_NRDIFF] = 1;
      }
   if ( Tm.nadiff > 0 )
      {
      EqItem *it = add( eq, EQ_NADIFF, EI_NABLA );
      it->b = Tm.nadiff * Ts.freq;                    /* period              */
      eq->used[EQ_NADIFF] = 1;
      }
   if ( Ts.freq == 4 || Ts.freq == 12 )
      {
      int nf = Ts.freq / 2;

      for ( i = 0; i <= nf; i++ )
          if ( Tm.ifadf != NULL && Tm.ifadf[i] > 0 )
             {
             EqItem *it;

             if ( i == 0 )                            /* nabla = (1 - B)     */
                {
                it = add( eq, EQ_IFADF, EI_NABLA );
                it->a = 1;
                }
             else                                     /* (1 +- c B + B^2)    */
                {
                it = add( eq, EQ_IFADF, EI_OPEN );
                it->bracket = 'i';                    /* individual factor   */
                it->a = i;                            /* its frequency       */
                it->b = Ts.freq;
                }
             }
      eq->used[EQ_IFADF] = 1;                         /* always written      */
      }
}

void eq_free( Equation *eq )
{
   int i;

   for ( i = 0; i < EQ_NPARTS; i++ )
       {
       free( eq->item[i] );
       eq->item[i] = NULL;
       eq->n[i] = 0;
       }
}
