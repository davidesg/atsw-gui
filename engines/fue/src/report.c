/*****************************************************************************/
/* report.c -- the report of FUE drawn with fugdraw, without LaTeX.          */
/* Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 or later.        */
/*                                                                           */
/* The page has the same three parts as the one that pdflatex made: the      */
/* graph of the residuals, the equation of the model (include/equation.h)    */
/* and the table of the residuals over three standard deviations.            */
/*                                                                           */
/* The equation is laid out here with the primitives of fugdraw (text,       */
/* lines and the accents of fd_runs); what mathematics needs and fugdraw     */
/* does not have -- fractions, radicals, brackets that grow, subscripts and  */
/* superscripts -- is drawn in this file, so fugdraw stays the same file in  */
/* fug and in fue.                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "report.h"

/* The fonts of the equation: numbers, names and the letters of the
 * variables (B, t, N) in sans serif, as \mathsf in the LaTeX one, and the
 * Greek letters in Symbol. The letters were in Times-Italic; the analyst
 * asked for them upright, as the original FUE wrote its models, which reads
 * better next to the sans serif numbers.                                   */
#define F_NUM   FD_HELV
#define F_VAR   FD_HELV
#define F_ROMAN FD_TIMES
#define F_GREEK FD_SYMBOL

#define SYM_XI    "x"          /* Symbol: lowercase xi                       */
#define SYM_PI    "p"
#define SYM_SIGMA "s"
#define SYM_NABLA FD_SYM_NABLA
#define SYM_RADICAL "\326"

#define SUP_RISE   0.42        /* of the size of the text                    */
#define SUB_RISE  -0.22
#define SMALL      0.72        /* size of a subscript or a superscript       */

/*****************************************************************************/
/* The pen: it draws (or only measures, when f is NULL) from left to right   */
/* and wraps when a term does not fit.                                       */
/*****************************************************************************/

typedef struct {
   FDFig *f;                   /* NULL: measure without drawing              */
   double x, y;                /* where the next thing goes                  */
   double size;                /* size of the text                           */
   double left, right;         /* margins                                    */
   double leading;             /* between lines                              */
   double fixed_x0, fixed_y;   /* where the operator of fixed frequency began */
   int    fixed_f;             /* its frequency (0: there is none open)       */
} Pen;

static void put( Pen *p, int font, double size, double rise, const char *s )
{
   if ( p->f != NULL ) fd_text( p->f, p->x, p->y + rise, font, size, FD_LEFT, s );
   p->x += fd_text_width( font, size, s );
}

static void put_num( Pen *p, double value, int decimals )
{
   char text[64];

   snprintf( text, sizeof( text ), "%.*f", decimals, value );
   put( p, F_NUM, p->size, 0.0, text );
}

static void space( Pen *p, double em )
{
   p->x += em * p->size;
}

/* A new line of the equation */
static void newline( Pen *p )
{
   p->x  = p->left;
   p->y -= p->leading;
}

/* "f = k" centred under the operator of fixed frequency that has just been
 * closed (it is not written when the operator was broken over two lines).  */
static void close_fixed( Pen *p )
{
   char label[32];

   if ( p->fixed_f == 0 ) return;
   if ( p->f != NULL && p->y == p->fixed_y )
      {
      snprintf( label, sizeof( label ), "f = %d", p->fixed_f );
      fd_text( p->f, (p->fixed_x0 + p->x) / 2.0, p->y - 2.05 * p->size, F_NUM,
               SMALL * p->size, FD_CENTER, label );
      }
   p->fixed_f = 0;
}

/* Measure what draw() would take, without drawing it */
static double width_of( const Pen *p, void (*draw)( Pen *, const void * ), const void *arg )
{
   Pen m = *p;

   m.f = NULL;
   m.x = 0.0;
   draw( &m, arg );
   return( m.x );
}

/*****************************************************************************/
/* What mathematics needs and fugdraw does not have                          */
/*****************************************************************************/

/* A value with its standard error under it, both centred (the \est of the
 * LaTeX file).                                                              */
static void stacked( Pen *p, double value, double se, int decimals, int has_se )
{
   char   top[64], bottom[64];
   double wt, wb, w, sz = p->size;

   snprintf( top, sizeof( top ), "%.*f", decimals, value );
   snprintf( bottom, sizeof( bottom ), "(%.*f)", decimals, se );
   wt = fd_text_width( F_NUM, sz, top );
   wb = has_se ? fd_text_width( F_NUM, sz, bottom ) : 0.0;
   w  = ( wt > wb ) ? wt : wb;
   if ( p->f != NULL )
      {
      fd_text( p->f, p->x + (w - wt) / 2.0, p->y, F_NUM, sz, FD_LEFT, top );
      if ( has_se )
         fd_text( p->f, p->x + (w - wb) / 2.0, p->y - 1.05 * sz, F_NUM, sz,
                  FD_LEFT, bottom );
      }
   p->x += w;
}

/* num / den with a rule between them, as pi/6 of the harmonics */
static void fraction( Pen *p, const char *num, int num_font, const char *den, int den_font )
{
   double sz = 0.76 * p->size, wn, wd, w;

   wn = fd_text_width( num_font, sz, num );
   wd = fd_text_width( den_font, sz, den );
   w  = ( wn > wd ) ? wn : wd;
   if ( p->f != NULL )
      {
      fd_text( p->f, p->x + (w - wn) / 2.0, p->y + 0.50 * sz, num_font, sz, FD_LEFT, num );
      fd_text( p->f, p->x + (w - wd) / 2.0, p->y - 0.62 * sz, den_font, sz, FD_LEFT, den );
      fd_linewidth( p->f, 0.05 * sz );
      fd_line( p->f, p->x, p->y + 0.33 * sz, p->x + w, p->y + 0.33 * sz );
      }
   p->x += w + 0.10 * p->size;
}

/* The square root of a number: the radical of the Symbol font and its rule */
static void radical( Pen *p, const char *inside )
{
   double sz = p->size, w = fd_text_width( F_NUM, sz, inside );

   put( p, F_GREEK, sz, 0.0, SYM_RADICAL );
   if ( p->f != NULL )
      {
      fd_linewidth( p->f, 0.045 * sz );
      fd_line( p->f, p->x, p->y + 0.72 * sz, p->x + w, p->y + 0.72 * sz );
      }
   put( p, F_NUM, sz, 0.0, inside );
}

/* A bracket that grows: it holds the differences and the mean */
static void bracket( Pen *p, int closing, double half )
{
   double sz = p->size, x = p->x, serif = 0.22 * sz, mid = p->y + 0.30 * sz;

   if ( p->f != NULL )
      {
      fd_linewidth( p->f, 0.06 * sz );
      fd_line( p->f, x, mid - half, x, mid + half );
      if ( closing )
         {
         fd_line( p->f, x, mid + half, x - serif, mid + half );
         fd_line( p->f, x, mid - half, x - serif, mid - half );
         }
      else
         {
         fd_line( p->f, x, mid + half, x + serif, mid + half );
         fd_line( p->f, x, mid - half, x + serif, mid - half );
         }
      }
   p->x += serif + 0.14 * sz;
}

/* A letter with a subscript and a superscript, as N_t or xi_t^{I,2/2008} */
static void with_scripts( Pen *p, int font, const char *base, const char *sub,
                          const char *sup )
{
   double sz = p->size, small = SMALL * sz, ws, wp, x;

   put( p, font, sz, 0.0, base );
   x  = p->x;
   ws = ( sub != NULL ) ? fd_text_width( F_VAR, small, sub ) : 0.0;
   wp = ( sup != NULL ) ? fd_text_width( F_NUM, small, sup ) : 0.0;
   if ( p->f != NULL )
      {
      if ( sub != NULL )
         fd_text( p->f, x, p->y + SUB_RISE * sz, F_VAR, small, FD_LEFT, sub );
      if ( sup != NULL )
         fd_text( p->f, x, p->y + SUP_RISE * sz, F_NUM, small, FD_LEFT, sup );
      }
   p->x = x + ( ( ws > wp ) ? ws : wp );
}

/*****************************************************************************/
/* The items of the equation                                                 */
/*****************************************************************************/

static void draw_coef( Pen *p, const EqItem *it )
{
   int decimals = eq_decimals( it );

   if ( it->value == 0.0 && it->sign != ' ' && !it->has_se ) return;
   if ( it->sign != ' ' )
      {
      space( p, 0.22 );
      put( p, F_ROMAN, p->size, 0.0, ( it->sign == '-' ) ? "-" : "+" );
      space( p, 0.22 );
      }
   stacked( p, it->value, it->se, decimals, it->has_se );
}

static void draw_b( Pen *p, const EqItem *it )
{
   char power[16];

   space( p, 0.10 );
   if ( it->a == 1 && !it->b )
      put( p, F_VAR, p->size, 0.0, "B" );
   else
      {
      snprintf( power, sizeof( power ), "%d", it->a );
      with_scripts( p, F_VAR, "B", NULL, power );
      }
}

/* nabla, nabla^d or nabla_s */
static void draw_nabla( Pen *p, const EqItem *it )
{
   char text[16];

   if ( it->b > 0 )
      {
      snprintf( text, sizeof( text ), "%d", it->b );
      with_scripts( p, F_GREEK, SYM_NABLA, text, NULL );
      }
   else if ( it->a > 1 )
      {
      snprintf( text, sizeof( text ), "%d", it->a );
      with_scripts( p, F_GREEK, SYM_NABLA, NULL, text );
      }
   else
      put( p, F_GREEK, p->size, 0.0, SYM_NABLA );
}

/* cos or sin of an angle that is a multiple of pi */
static void draw_harmonic( Pen *p, const EqItem *it )
{
   char num[16], den[16];

   put( p, F_ROMAN, p->size, 0.0, ( it->text[0] == 'c' ) ? "cos" : "sin" );
   space( p, 0.12 );
   if ( it->a == 1 )
      snprintf( num, sizeof( num ), "%s", SYM_PI );
   else
      snprintf( num, sizeof( num ), "%d%s", it->a, SYM_PI );
   snprintf( den, sizeof( den ), "%d", it->b );
   fraction( p, num, F_GREEK, den, F_NUM );
   put( p, F_VAR, p->size, 0.0, "t" );
}

/* xi_t of an intervention, with its kind and its date */
static void draw_dummy( Pen *p, const EqItem *it )
{
   char sup[48];

   if ( strcmp( it->text, "season" ) == 0 )
      {
      snprintf( sup, sizeof( sup ), "%d", it->a );
      with_scripts( p, F_GREEK, SYM_XI, "t", sup );
      }
   else if ( it->text[0] == '\0' )                       /* non-standard     */
      with_scripts( p, F_GREEK, SYM_XI, "t", NULL );
   else if ( it->b == 0 )                        /* easter, trend: its name  */
      {
      space( p, 0.15 );
      put( p, F_ROMAN, 0.92 * p->size, 0.0, it->text );
      }
   else
      {
      if ( it->a == 0 ) snprintf( sup, sizeof( sup ), "%s,%d", it->text, it->b );
      else snprintf( sup, sizeof( sup ), "%s,%d/%d", it->text, it->a, it->b );
      with_scripts( p, F_GREEK, SYM_XI, "t", sup );
      }
}

/* An individual factor of the annual difference: (1 +- c B + B^2) with the
 * frequency under it                                                        */
static void draw_ifadf_factor( Pen *p, int i, int freq )
{
   double sz = p->size, x0 = p->x;
   char   label[32];

   if ( i == 0 ) { draw_nabla( p, &(EqItem){ .a = 1 } ); return; }
   put( p, F_ROMAN, sz, 0.0, "(" );
   put( p, F_NUM, sz, 0.0, "1" );
   if ( freq == 4 )
      {
      space( p, 0.2 );
      put( p, F_ROMAN, sz, 0.0, "+" );
      space( p, 0.2 );
      if ( i == 1 ) with_scripts( p, F_VAR, "B", NULL, "2" );
      else put( p, F_VAR, sz, 0.0, "B" );
      }
   else if ( i == 6 )
      {
      space( p, 0.2 );
      put( p, F_ROMAN, sz, 0.0, "+" );
      space( p, 0.2 );
      put( p, F_VAR, sz, 0.0, "B" );
      }
   else
      {
      if ( i != 3 )
         {
         space( p, 0.2 );
         put( p, F_ROMAN, sz, 0.0, ( i < 3 ) ? "-" : "+" );
         space( p, 0.2 );
         if ( i == 1 || i == 5 ) radical( p, "3" );
         put( p, F_VAR, sz, 0.0, "B" );
         space( p, 0.2 );
         put( p, F_ROMAN, sz, 0.0, "+" );
         space( p, 0.2 );
         }
      else
         {
         space( p, 0.2 );
         put( p, F_ROMAN, sz, 0.0, "+" );
         space( p, 0.2 );
         }
      with_scripts( p, F_VAR, "B", NULL, "2" );
      }
   put( p, F_ROMAN, sz, 0.0, ")" );
   snprintf( label, sizeof( label ), "f = %d", i );
   if ( p->f != NULL )
      fd_text( p->f, (x0 + p->x) / 2.0, p->y - 2.05 * sz, F_NUM, SMALL * sz,
               FD_CENTER, label );
}

/*****************************************************************************/
/* The parts of the equation                                                 */
/*****************************************************************************/

/* One factor (1 - phi B - phi B^2 ...) or, with a fixed frequency, the same
 * with "f = k" under it                                                     */
typedef struct { const Equation *eq; int part; int from, to; } Range;

static void draw_range( Pen *p, const void *arg )
{
   const Range  *r = (const Range *)arg;
   const EqItem *item = r->eq->item[r->part];
   double        sz = p->size;
   int           i;

   for ( i = r->from; i < r->to; i++ )
       {
       const EqItem *it = &item[i];

       switch ( it->kind )
          {
          case EI_OPEN:
               if ( it->bracket == 'f' )
                  {
                  p->fixed_x0 = p->x;
                  p->fixed_y  = p->y;
                  p->fixed_f  = it->a;
                  }
               put( p, F_ROMAN, sz, 0.0, "(" );
               break;
          case EI_CLOSE:
               put( p, F_ROMAN, sz, 0.0, ")" );
               close_fixed( p );
               break;
          case EI_TEXT:
               if ( strcmp( it->text, "1" ) == 0 ) put( p, F_NUM, sz, 0.0, "1" );
               else if ( strcmp( it->text, "t" ) == 0 )
                  { space( p, 0.15 ); put( p, F_VAR, sz, 0.0, "t" ); }
               else if ( strcmp( it->text, "N" ) == 0 )
                  with_scripts( p, F_VAR, "N", "t", NULL );
               break;
          case EI_COEF:     draw_coef( p, it ); break;
          case EI_B:        draw_b( p, it ); break;
          case EI_NABLA:    draw_nabla( p, it ); break;
          case EI_HARMONIC: draw_harmonic( p, it ); break;
          case EI_DUMMY:    draw_dummy( p, it ); break;
          case EI_ALTER:    put( p, F_ROMAN, sz, 0.0, "(" );
                            put( p, F_ROMAN, sz, 0.0, "-1" );
                            put( p, F_ROMAN, sz, 0.0, ")" );
                            with_scripts( p, F_ROMAN, "", NULL, "t" );
                            break;
          case EI_OP:
               if ( it->sign == '/' ) { space( p, 0.2 ); put( p, F_ROMAN, sz, 0.0, "/" ); space( p, 0.2 ); }
               else if ( it->sign == '=' ) { space( p, 0.35 ); put( p, F_ROMAN, sz, 0.0, "="); space( p, 0.35 ); }
               else { space( p, 0.3 ); put( p, F_ROMAN, sz, 0.0, ( it->sign == '-' ) ? "-" : "+" ); space( p, 0.3 ); }
               break;
          default: break;
          }
       }
}

/* A part, breaking the line before a term that does not fit */
static void draw_part( Pen *p, const Equation *eq, int part, int wrap )
{
   const EqItem *item = eq->item[part];
   int           i, n = eq->n[part], start;
   Range         r;

   if ( !eq->used[part] ) return;
   r.eq = eq;
   r.part = part;
   /* the name of the series and the = are drawn by draw_equation() */
   for ( i = ( part == EQ_DETER ) ? 2 : 0; i < n; )
       {
       start = i;
       /* A chunk goes up to the next term: an operator that starts one, or a
        * coefficient with its sign (inside a factor, so that a long AR or MA
        * operator can also be broken). Each coefficient keeps its B.        */
       do { i++; }
       while ( i < n &&
               !( item[i].kind == EI_OP &&
                  ( item[i].sign == '+' || item[i].sign == '-' ) ) &&
               !( item[i].kind == EI_COEF &&
                  ( item[i].sign == '+' || item[i].sign == '-' ) ) );
       r.from = start;
       r.to   = i;
       if ( wrap && p->x > p->left && p->x + width_of( p, draw_range, &r ) > p->right )
          newline( p );                    /* close_fixed() checks the line  */
       draw_range( p, &r );
       }
}

/*****************************************************************************/
/* The equation, from the deterministic part to sigma                        */
/*****************************************************************************/

/* [ nabla ... N_t - mu ]: the differences of the series, with the mean */
static void draw_bracket_group( Pen *p, const void *arg )
{
   const Equation *eq = *(const Equation **)arg;
   double          sz = p->size, half = 1.15 * sz;
   int             i;

   if ( eq->used[EQ_MU] ) { space( p, 0.2 ); bracket( p, 0, half ); }
   draw_part( p, eq, EQ_NRDIFF, 0 );
   draw_part( p, eq, EQ_NADIFF, 0 );
   if ( eq->used[EQ_IFADF] )
      for ( i = 0; i < eq->n[EQ_IFADF]; i++ )
          draw_ifadf_factor( p, ( eq->item[EQ_IFADF][i].kind == EI_NABLA ) ? 0
                                : eq->item[EQ_IFADF][i].a, eq->freq );
   space( p, 0.15 );
   if ( eq->used[EQ_DETER] )
      with_scripts( p, F_VAR, "N", "t", NULL );
   else
      {
      if ( eq->is_log ) { put( p, F_ROMAN, sz, 0.0, "ln" ); space( p, 0.2 ); }
      with_scripts( p, F_VAR, eq->name, "t", NULL );
      }
   if ( eq->used[EQ_MU] )
      {
      draw_coef( p, &eq->item[EQ_MU][0] );
      space( p, 0.2 );
      bracket( p, 1, half );
      }
}

/* = MA factors A_t ;  sigma-hat_A = x %   */
static void draw_tail( Pen *p, const void *arg )
{
   const Equation *eq = *(const Equation **)arg;
   double          sz = p->size;
   char            text[64];
   FDRun           r[2];

   space( p, 0.35 );
   put( p, F_ROMAN, sz, 0.0, "=" );
   space( p, 0.35 );
   draw_part( p, eq, EQ_MAR, 1 );
   draw_part( p, eq, EQ_MAA, 1 );
   draw_part( p, eq, EQ_MAF, 1 );
   space( p, 0.15 );
   with_scripts( p, F_VAR, eq->residuals, "t", NULL );

   space( p, 0.5 );
   put( p, F_ROMAN, sz, 0.0, ";" );
   space( p, 0.9 );
   r[0] = (FDRun){ F_GREEK, sz, 0.0, SYM_SIGMA, FD_ACC_HAT };
   r[1] = (FDRun){ F_VAR, SMALL * sz, SUB_RISE * sz, eq->residuals, FD_ACC_NONE };
   if ( p->f != NULL ) fd_runs( p->f, p->x, p->y, FD_LEFT, r, 2 );
   p->x += fd_runs_width( r, 2 );
   space( p, 0.3 );
   put( p, F_ROMAN, sz, 0.0, "=" );
   space( p, 0.3 );
   snprintf( text, sizeof( text ), "%2.2f %%", eq->sigma );
   put( p, F_NUM, sz, 0.0, text );
}

/* Break the line if what comes next does not fit in it */
static void fit( Pen *p, void (*draw)( Pen *, const void * ), const void *arg )
{
   if ( p->x > p->left && p->x + width_of( p, draw, arg ) > p->right ) newline( p );
}

static void draw_equation( Pen *p, const Equation *eq )
{
   const Equation *arg = eq;

   if ( eq->used[EQ_DETER] )
      {
      if ( eq->is_log ) { put( p, F_ROMAN, p->size, 0.0, "ln" ); space( p, 0.2 ); }
      with_scripts( p, F_VAR, eq->name, "t", NULL );
      space( p, 0.35 );
      put( p, F_ROMAN, p->size, 0.0, "=" );
      space( p, 0.35 );
      draw_part( p, eq, EQ_DETER, 1 );  /* its first two items are skipped  */
      newline( p );
      newline( p );
      }

   draw_part( p, eq, EQ_ARR, 1 );
   draw_part( p, eq, EQ_ARA, 1 );
   draw_part( p, eq, EQ_ARF, 1 );
   fit( p, draw_bracket_group, &arg );
   draw_bracket_group( p, &arg );
   fit( p, draw_tail, &arg );
   draw_tail( p, &arg );
}

/*****************************************************************************/
/* The table of the residuals over three standard deviations                 */
/*****************************************************************************/

#define OUTLIER_LIMIT 3.0

static double draw_outliers( FDFig *f, struct Tseries *res, double x, double y,
                             double size )
{
   double mean = res->mean, sd = sqrt( res->var ), z, top = y;
   double wobs = 3.2 * size, wdate = 5.0 * size, wval = 4.4 * size, w;
   int    i, rows = 0, period, season;
   char   text[64];

   w = wobs + wdate + wval;
   if ( f != NULL )
      {
      fd_linewidth( f, 0.05 * size );
      fd_line( f, x, y + 1.35 * size, x + w, y + 1.35 * size );
      fd_text( f, x + wobs / 2.0, y + 0.45 * size, F_ROMAN, size, FD_CENTER, "Obs." );
      if ( res->numbering == 0 )
         fd_text( f, x + wobs + wdate / 2.0, y + 0.45 * size, F_ROMAN, size, FD_CENTER, "Date" );
      fd_text( f, x + wobs + wdate + wval / 2.0, y + 0.45 * size, F_ROMAN, size,
               FD_CENTER, "Std. Val." );
      fd_line( f, x, y, x + w, y );
      }
   y -= 1.25 * size;

   for ( i = 1; i <= res->nobs; i++ )
       {
       z = ( sd > 0.0 ) ? ( res->data[i] - mean ) / sd : 0.0;
       if ( fabs( z ) < OUTLIER_LIMIT ) continue;
       rows++;
       if ( f != NULL )
          {
          snprintf( text, sizeof( text ), "%d", i );
          fd_text( f, x + wobs / 2.0, y, F_NUM, size, FD_CENTER, text );
          if ( res->numbering == 0 )
             {
             ObsToDate( res->begyear, res->begtime, i, res->freq, &period, &season );
             if ( res->freq == 1 ) snprintf( text, sizeof( text ), "%d", period );
             else snprintf( text, sizeof( text ), "%d/%d", season, period );
             fd_text( f, x + wobs + wdate / 2.0, y, F_NUM, size, FD_CENTER, text );
             }
          snprintf( text, sizeof( text ), "%.2f", z );
          fd_text( f, x + wobs + wdate + wval / 2.0, y, F_NUM, size, FD_CENTER, text );
          }
       y -= 1.25 * size;
       }
   if ( f != NULL )
      {
      fd_line( f, x, y + 0.85 * size, x + w, y + 0.85 * size );
      fd_text( f, x + w + 4.0 * size, top + 0.45 * size, FD_HELV_BOLD, size,
               FD_LEFT, "Comments:" );
      }
   (void) rows;
   return( top - y );
}

/*****************************************************************************/

int report_write_pdf( const char *filename, FDFig *graph, const Equation *eq,
                      struct Tseries *res )
{
   const double A4_SHORT = 595.276, A4_LONG = 841.89, margin = 36.0;
   double       PW, PH, size = 9.0, x[2], y[2], scale[2], gw, gh, s, left;
   FDFig       *page, *figs[2];
   FDPdf       *pdf;
   Pen          p;
   int          status, n = 0, landscape;

/* [1]: the page is landscape only when the graph does not fit across an A4
 * portrait page: with quarterly or annual data of few observations the graph
 * is the small one (468 points) and the page is portrait.                   */

   gw = ( graph != NULL ) ? graph->w : 0.0;
   gh = ( graph != NULL ) ? graph->h : 0.0;
   landscape = ( gw > A4_SHORT - 2 * margin );
   PW = landscape ? A4_LONG : A4_SHORT;
   PH = landscape ? A4_SHORT : A4_LONG;

   page = fd_fig_new( PW, PH );
   if ( page == NULL ) return( 1 );

   s = 1.0;
   if ( gw > 0.0 && s * gw > PW - 2 * margin ) s = ( PW - 2 * margin ) / gw;

/* [2]: the equation under the graph and the table of residuals under it.
 * The graph is against the left margin and the equation begins at the same
 * point, so that the three parts are aligned.                               */

   left = margin;
   memset( &p, 0, sizeof( p ) );
   p.f       = page;
   p.size    = size;
   p.left    = left;
   p.right   = PW - margin;
   p.leading = 2.9 * size;          /* room for the standard errors and the brackets */
   p.x       = left;
   p.y       = PH - margin - s * gh - 2.6 * size;
   draw_equation( &p, eq );

   /* the table under the equation, leaving room for the "f = k" of a factor
    * of fixed frequency, which hangs below its line                         */
   draw_outliers( page, res, left, p.y - 4.6 * size, size );

/* [3]: the page                                                             */

   if ( ( pdf = fd_pdf_open( filename ) ) == NULL )
      {
      fd_fig_free( page );
      return( 1 );
      }
   if ( graph != NULL )
      {
      figs[n] = graph;
      x[n] = left;
      y[n] = PH - margin - s * gh;
      scale[n] = s;
      n++;
      }
   figs[n] = page;
   x[n] = 0.0;
   y[n] = 0.0;
   scale[n] = 1.0;
   n++;
   fd_pdf_page( pdf, PW, PH, figs, x, y, scale, n );
   status = fd_pdf_close( pdf );
   fd_fig_free( page );
   return( status );
}
