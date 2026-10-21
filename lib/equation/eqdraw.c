/*****************************************************************************/
/* eqdraw.c -- the page of an estimated model (see eqdraw.h).                */
/* Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 or later.        */
/*                                                                           */
/* The page is the one that pdflatex makes of fue's LaTeX file (eqlatex.c),  */
/* measured on it: where TeX puts each atom, how far apart the lines are,    */
/* where the table goes. The numbers below are TeX's, in points at the size  */
/* of the equation (\footnotesize of a 12pt article: 10pt):                  */
/*                                                                           */
/*  - between atoms, the spacing of TeX's table in text style: thin (3mu),   */
/*    medium (4mu) and thick (5mu), with a binary operator turned into an    */
/*    ordinary atom after an operator, a relation or an opening;             */
/*  - \est: an array [t]{c} whose entries are trimmed by 0.125in on each     */
/*    side, with \arraycolsep 5pt: the box is 8pt narrower than the wider of */
/*    value and standard error, and the error goes 9.8pt under the value;    */
/*  - the lines: \baselineskip 19.2pt (12pt x 1.6) unless the depth of one   */
/*    and the height of the next leave less, and then 1pt (\lineskip)        */
/*    between them; the struts of the arrays are 13.44pt high, 5.76pt deep;  */
/*  - a line breaks after a binary operator or a relation, as TeX breaks an  */
/*    inline formula, and the next line begins at the left margin.           */
/*                                                                           */
/* The page is drawn twice: once without drawing, to know how high and deep  */
/* each line is, and then at the baselines that gives.                       */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "eqdraw.h"

/*****************************************************************************/
/* TeX's measures, in points at 10pt                                         */
/*****************************************************************************/

#define SIZE      10.0      /* \footnotesize of a 12pt article               */
#define SANS       9.0      /* Helvetica at 9pt has the widths of cmss10     */
#define SCRIPT     7.0      /* \scriptsize of the subscripts and fractions   */
#define SCRIPT_SANS 6.3

#define THIN      ( 3.0 / 18.0 * SIZE )
#define MED       ( 4.0 / 18.0 * SIZE )
#define THICK     ( 5.0 / 18.0 * SIZE )
#define QUAD      SIZE

#define BSKIP     19.2      /* \baselineskip: 12pt x \linespread{1.6}        */
#define LSKIP      1.0      /* \lineskip                                     */
#define STRUT_H   13.44     /* the strut of an array: 0.7 \baselineskip      */
#define STRUT_D    5.76     /*                        0.3 \baselineskip      */
#define SE_DROP    9.8      /* from the value to its standard error          */
#define EST_TRIM   9.0      /* \hspace{-.125in}                              */
#define EST_SEP    5.0      /* \arraycolsep                                  */

#define AXIS       2.5      /* the math axis                                 */
#define RULE       0.4      /* the rule of a fraction                        */
#define NULLDELIM  1.2      /* \nulldelimiterspace                           */
#define NUM_UP     3.94     /* numerator and denominator of a text fraction  */
#define DEN_DOWN   3.44
#define SUP_UP     3.63     /* superscript                                   */
#define SUB_DOWN   1.5      /* subscript alone, and with a superscript       */
#define SUB2_DOWN  2.47
#define LIMIT_DOWN 6.17     /* the label of \underset, under the box         */

#define SLANT      0.21     /* the Greek letters: math italic                */

/* The advance of the Computer Modern glyph that each one takes the place of */
#define CM_OP      7.78     /* + - = (cmr, cmsy)                             */
#define CM_PAREN   3.89
#define CM_SLASH   5.0
#define CM_SEMI    2.78
#define CM_COMMA   2.78
#define CM_PCT     8.33
#define CM_B       7.59     /* cmmi                                          */
#define CM_T       3.61
#define CM_F       4.90
#define CM_I       4.40
#define CM_NABLA   8.33     /* cmsy                                          */
#define CM_PI      5.70
#define CM_SIGMA   5.71
#define CM_XI      4.38
#define CM_DELIM   6.65     /* \left[ and \right] of cmex                    */

/* The page (geometry: margin=2.5cm, landscape)                              */
#define MARGIN    70.866
#define GRAPH_TOP 12.7      /* the graph, under the top margin               */
#define TABLE_GAP 29.3      /* \bigskip, multicols: from the equation        */
#define ROW       19.6      /* a row of the tabular                          */
#define TABCOLSEP  6.0
#define COLSEP    10.0      /* \columnsep of multicols                       */

#define F_SANS    FD_HELV
#define F_ROMAN   FD_TIMES
#define F_ITALIC  FD_TIMES_ITALIC
#define F_GREEK   FD_SYMBOL

#define SYM_PI      "p"
#define SYM_SIGMA   "s"
#define SYM_XI      "x"
#define SYM_MINUS   "-"     /* in Symbol: the minus sign                     */
#define SYM_RADICAL "\326"

/*****************************************************************************/
/* The pen: TeX's atoms, from left to right                                  */
/*****************************************************************************/

enum { A_NONE, A_ORD, A_OP, A_BIN, A_REL, A_OPEN, A_CLOSE, A_PUNCT, A_INNER };

#define MAXLINES 64

typedef struct {
   FDFig *f;                   /* NULL: only measure                         */
   double x, y;                /* y: the baseline of this line               */
   double left, right;
   int    last;                /* the atom before                            */
   int    line, wrap;          /* wrap: lines may break                      */
   double h[MAXLINES], d[MAXLINES], base[MAXLINES];
   int    drawing;             /* the second pass: base[] is known           */
} Pen;

/* TeX's spacing between two atoms, text style: 0, 1 thin, 2 med, 3 thick */
static const int tex_space[9][9] = {
   /*          none ord op bin rel open close punct inner */
   /* none  */ { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   /* ord   */ { 0, 0, 1, 2, 3, 0, 0, 0, 1 },
   /* op    */ { 0, 1, 1, 0, 3, 0, 0, 0, 1 },
   /* bin   */ { 0, 2, 2, 0, 0, 2, 0, 0, 2 },
   /* rel   */ { 0, 3, 3, 0, 0, 3, 0, 0, 3 },
   /* open  */ { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
   /* close */ { 0, 0, 1, 2, 3, 0, 0, 0, 1 },
   /* punct */ { 0, 1, 1, 0, 1, 1, 1, 1, 1 },
   /* inner */ { 0, 1, 1, 2, 3, 1, 0, 1, 1 }
};

/* The next atom: the space TeX puts before it. Returns the kind it took (a
 * binary operator where it cannot be one is an ordinary atom).            */
static int atom( Pen *p, int kind )
{
   static const double amount[4] = { 0.0, THIN, MED, THICK };

   if ( kind == A_BIN && ( p->last == A_NONE || p->last == A_BIN || p->last == A_OP ||
                           p->last == A_REL || p->last == A_OPEN || p->last == A_PUNCT ) )
      kind = A_ORD;
   p->x   += amount[tex_space[p->last][kind]];
   p->last = kind;
   return( kind );
}

static void grow( Pen *p, double h, double d )
{
   if ( h > p->h[p->line] ) p->h[p->line] = h;
   if ( d > p->d[p->line] ) p->d[p->line] = d;
}

static void newline( Pen *p )
{
   if ( p->line < MAXLINES - 1 ) p->line++;
   p->x    = p->left;
   p->last = A_NONE;
   if ( p->drawing ) p->y = p->base[p->line];
}

/* Text in a font, in the advance cm of the glyph of Computer Modern that it
 * replaces (centred in it), or in its own width when cm is 0.             */
static void glyph( Pen *p, int font, double size, double rise, const char *s, double cm )
{
   double w = fd_text_width( font, size, s ), adv = ( cm > 0.0 ) ? cm : w;

   if ( p->f != NULL )
      fd_text( p->f, p->x + ( adv - w ) / 2.0, p->y + rise, font, size, FD_LEFT, s );
   p->x += adv;
}

/* An italic letter. Times Italic is narrower than the math italic of
 * Computer Modern: in its advance it would leave a gap before a ")", so it
 * keeps its own width and the italic correction of TeX (0.05em).          */
static void italic( Pen *p, double size, double rise, const char *s, double cm )
{
   (void) cm;
   if ( p->f != NULL ) fd_text( p->f, p->x, p->y + rise, F_ITALIC, size, FD_LEFT, s );
   p->x += fd_text_width( F_ITALIC, size, s ) + 0.05 * size;
}

/* A Greek letter, slanted */
static void greek( Pen *p, double size, double rise, const char *s, double cm )
{
   double w = fd_text_width( F_GREEK, size, s ), adv = ( cm > w ) ? cm : w;

   if ( p->f != NULL )
      fd_text_slant( p->f, p->x + ( adv - w ) / 2.0 - SLANT * rise, p->y + rise,
                     F_GREEK, size, SLANT, s );
   p->x += adv;
}

static void sans( Pen *p, double size, double rise, const char *s )
{
   glyph( p, F_SANS, size, rise, s, 0.0 );
}

static double sans_width( double size, const char *s )
{
   return( fd_text_width( F_SANS, size, s ) );
}

/* The binary operators and the relation */
static void op_glyph( Pen *p, char op )
{
   if ( op == '-' ) glyph( p, F_GREEK, SIZE, 0.0, SYM_MINUS, CM_OP );
   else if ( op == '+' ) glyph( p, F_ROMAN, SIZE, 0.0, "+", CM_OP );
   else glyph( p, F_ROMAN, SIZE, 0.0, "=", CM_OP );
   grow( p, 5.8, 0.8 );
}

static void paren( Pen *p, const char *s )
{
   glyph( p, F_ROMAN, SIZE, 0.0, s, CM_PAREN );
   grow( p, 7.5, 2.5 );
}

/* A subscript and a superscript after what was just written (both sans
 * serif, as \mathsf{N_t} and B^{\mathsf{2}}).                              */
static void scripts( Pen *p, const char *sub, const char *sup )
{
   double x0 = p->x, ws = 0.0, wp = 0.0;

   if ( sub != NULL )
      {
      double down = ( sup != NULL ) ? SUB2_DOWN : SUB_DOWN;

      ws = sans_width( SCRIPT_SANS, sub );
      if ( p->f != NULL ) fd_text( p->f, x0, p->y - down, F_SANS, SCRIPT_SANS, FD_LEFT, sub );
      grow( p, 0.0, down + 0.3 );
      }
   if ( sup != NULL )
      {
      wp = sans_width( SCRIPT_SANS, sup );
      if ( p->f != NULL ) fd_text( p->f, x0, p->y + SUP_UP, F_SANS, SCRIPT_SANS, FD_LEFT, sup );
      grow( p, SUP_UP + 4.9, 0.0 );
      }
   p->x = x0 + ( ( ws > wp ) ? ws : wp ) + 0.5;      /* \scriptspace */
}

/* A name with its subscript t: \mathsf{X_{t}} */
static void name_t( Pen *p, const char *name )
{
   atom( p, A_ORD );
   sans( p, SANS, 0.0, name );
   grow( p, 6.94, 0.0 );
   scripts( p, "t", NULL );
}

/* Measure what draw() would take, on the line where the pen is, without
 * breaking it. w: the advance, h and d: how high and deep it goes.        */
static void measure( const Pen *p, void (*draw)( Pen *, const void * ), const void *arg,
                     double *w, double *h, double *d )
{
   Pen *m = (Pen *)malloc( sizeof( Pen ) );

   *w = *h = *d = 0.0;
   if ( m == NULL ) return;
   *m = *p;
   m->f = NULL;
   m->drawing = 0;
   m->wrap = 0;
   m->x = 0.0;
   m->line = MAXLINES - 1;
   m->h[m->line] = m->d[m->line] = 0.0;
   draw( m, arg );
   *w = m->x;
   *h = m->h[m->line];
   *d = m->d[m->line];
   free( m );
}

/* TeX breaks a formula after a binary operator or a relation: what follows
 * goes to the next line if it does not fit in this one.                   */
static void fit( Pen *p, void (*draw)( Pen *, const void * ), const void *arg )
{
   double w, h, d;

   if ( !p->wrap || p->x <= p->left ) return;
   measure( p, draw, arg, &w, &h, &d );
   if ( p->x + w > p->right ) newline( p );
}

/*****************************************************************************/
/* The coefficients: \est{value}{(error)}                                    */
/*****************************************************************************/

/* The standard error in parentheses. eqlatex.c put them inside \mathsf with
 * the omegas of 4 decimals and outside in all the others: so they are, sans
 * serif or roman, each in the advance of the parenthesis of cmss or cmr
 * (the same: 0.389em).                                                    */
static double se_width( const EqItem *it, int dec, char *num, size_t n )
{
   (void) it;
   snprintf( num, n, "%.*f", dec, it->se );
   return( sans_width( SANS, num ) + 2.0 * CM_PAREN );
}

static void se_draw( Pen *p, const EqItem *it, int dec, double x, double y )
{
   char num[64];
   int  font = ( it->style == CS_OMEGA && dec == 4 ) ? F_SANS : F_ROMAN;
   double size = ( font == F_SANS ) ? SANS : SIZE;

   snprintf( num, sizeof( num ), "%.*f", dec, it->se );
   if ( p->f == NULL ) return;
   fd_text( p->f, x + ( CM_PAREN - fd_text_width( font, size, "(" ) ) / 2.0, y,
            font, size, FD_LEFT, "(" );
   x += CM_PAREN;
   fd_text( p->f, x, y, F_SANS, SANS, FD_LEFT, num );
   x += sans_width( SANS, num );
   fd_text( p->f, x + ( CM_PAREN - fd_text_width( font, size, ")" ) ) / 2.0, y,
            font, size, FD_LEFT, ")" );
}

static void coef_box( Pen *p, const EqItem *it )
{
   int    dec = eq_decimals( it );
   char   val[64], num[64];
   double wv, ws, wide, x0;

   snprintf( val, sizeof( val ), "%.*f", dec, it->value );
   wv = sans_width( SANS, val );
   atom( p, A_ORD );
   if ( !it->has_se )
      {
      sans( p, SANS, 0.0, val );
      grow( p, 6.94, 0.0 );
      return;
      }
   ws   = se_width( it, dec, num, sizeof( num ) );
   wide = ( wv > ws ) ? wv : ws;
   x0   = p->x + EST_SEP - EST_TRIM;
   if ( p->f != NULL )
      fd_text( p->f, x0 + ( wide - wv ) / 2.0, p->y, F_SANS, SANS, FD_LEFT, val );
   se_draw( p, it, dec, x0 + ( wide - ws ) / 2.0, p->y - SE_DROP );
   p->x += wide - 2.0 * EST_TRIM + 2.0 * EST_SEP;
   grow( p, STRUT_H, SE_DROP + STRUT_D );
}

/* A coefficient with the sign before it: the sign is a binary operator */
static void coef( Pen *p, const EqItem *it )
{
   if ( it->value == 0.0 && it->sign != ' ' && !it->has_se ) return;
   if ( it->sign != ' ' )
      {
      atom( p, A_BIN );
      op_glyph( p, it->sign );
      }
   coef_box( p, it );
}

/* B or B^k, in math italic */
static void bee( Pen *p, const EqItem *it )
{
   char power[16];

   atom( p, A_ORD );
   italic( p, SIZE, 0.0, "B", CM_B );
   grow( p, 6.83, 0.0 );
   if ( it->a != 1 || it->b )
      {
      snprintf( power, sizeof( power ), "%d", it->a );
      scripts( p, NULL, power );
      }
}

/*****************************************************************************/
/* The variables                                                             */
/*****************************************************************************/

/* \frac{num}{den} of a harmonic: an ordinary atom in amsmath */
static void fraction( Pen *p, int mult, int den )
{
   char   n[16], d[16];
   double wn, wd, w, x0, cx;

   atom( p, A_ORD );
   snprintf( n, sizeof( n ), "%d", mult );
   snprintf( d, sizeof( d ), "%d", den );
   wn = ( mult == 1 ? 0.0 : sans_width( SCRIPT_SANS, n ) ) + CM_PI * SCRIPT / SIZE;
   wd = sans_width( SCRIPT_SANS, d );
   w  = ( wn > wd ) ? wn : wd;
   x0 = p->x + NULLDELIM;
   cx = x0 + w / 2.0;
   if ( p->f != NULL )
      {
      Pen q = *p;

      q.x = cx - wn / 2.0;
      q.y = p->y + NUM_UP;
      if ( mult != 1 ) sans( &q, SCRIPT_SANS, 0.0, n );
      greek( &q, SCRIPT, 0.0, SYM_PI, CM_PI * SCRIPT / SIZE );
      fd_text( p->f, cx - wd / 2.0, p->y - DEN_DOWN, F_SANS, SCRIPT_SANS, FD_LEFT, d );
      fd_linewidth( p->f, RULE );
      fd_line( p->f, x0, p->y + AXIS, x0 + w, p->y + AXIS );
      }
   p->x = x0 + w + NULLDELIM;
   grow( p, NUM_UP + 4.9, DEN_DOWN + 0.2 );
}

/* cos and sin of a multiple of pi, in t */
static void harmonic( Pen *p, const EqItem *it )
{
   int c = ( it->text[0] == 'c' );

   atom( p, A_OP );
   glyph( p, F_ROMAN, SIZE, 0.0, c ? "cos" : "sin", c ? 13.38 : 12.28 );
   grow( p, 6.94, 0.0 );
   fraction( p, it->a, it->b );
   atom( p, A_ORD );
   italic( p, SIZE, 0.0, "t", CM_T );
}

/* xi_t^{sup} of an intervention: the kind in math italic, the date sans */
static void xi( Pen *p, const char *kind, const char *date )
{
   double x0, ws, wp = 0.0;

   atom( p, A_ORD );
   greek( p, SIZE, 0.0, SYM_XI, CM_XI );
   grow( p, 6.94, 1.94 );
   x0 = p->x;
   ws = CM_T * SCRIPT / SIZE;
   if ( p->f != NULL )
      fd_text( p->f, x0, p->y - ( kind || date ? SUB2_DOWN : SUB_DOWN ), F_ITALIC,
               SCRIPT, FD_LEFT, "t" );
   if ( kind != NULL || date != NULL )
      {
      Pen q = *p;

      q.x = x0 + SLANT * SUP_UP + 0.3;      /* the italic correction       */
      q.y = p->y + SUP_UP;
      if ( kind != NULL )
         {
         italic( &q, SCRIPT, 0.0, kind, 0.0 );
         glyph( &q, F_ROMAN, SCRIPT, 0.0, ",", CM_COMMA * SCRIPT / SIZE );
         }
      if ( date != NULL ) sans( &q, SCRIPT_SANS, 0.0, date );
      wp = q.x - x0;
      grow( p, SUP_UP + 4.9, 0.0 );
      }
   grow( p, 0.0, SUB2_DOWN + 0.3 );
   p->x = x0 + ( ( ws > wp ) ? ws : wp ) + 0.5;
}

static void dummy( Pen *p, const EqItem *it )
{
   char date[48];

   if ( strcmp( it->text, "season" ) == 0 )
      {
      snprintf( date, sizeof( date ), "%d", it->a );
      xi( p, NULL, date );
      }
   else if ( it->text[0] == '\0' )                       /* non-standard     */
      xi( p, NULL, NULL );
   else if ( it->b == 0 )                        /* easter, trend: its name  */
      {
      atom( p, A_ORD );
      glyph( p, F_ROMAN, SIZE, 0.0, it->text, 0.0 );
      grow( p, 6.94, 0.0 );
      }
   else
      {
      if ( it->a == 0 ) snprintf( date, sizeof( date ), "%d", it->b );
      else snprintf( date, sizeof( date ), "%d/%d", it->a, it->b );
      xi( p, it->text, date );
      }
}

/* (-1)^t */
static void alternating( Pen *p )
{
   atom( p, A_OPEN );
   paren( p, "(" );
   atom( p, A_BIN );                                    /* an ordinary one   */
   op_glyph( p, '-' );
   atom( p, A_ORD );
   sans( p, SANS, 0.0, "1" );
   atom( p, A_CLOSE );
   paren( p, ")" );
   if ( p->f != NULL ) fd_text( p->f, p->x, p->y + SUP_UP, F_ITALIC, SCRIPT, FD_LEFT, "t" );
   p->x += CM_T * SCRIPT / SIZE + 0.5;
   grow( p, SUP_UP + 4.9, 2.5 );
}

/* ln X_t, X_t or X_t^{(lambda)}: the output, or an input of a transfer */
static void series( Pen *p, const char *name, int log, int boxcox, double lambda )
{
   char l[32];

   if ( log )
      {
      atom( p, A_OP );
      glyph( p, F_ROMAN, SIZE, 0.0, "ln", 8.34 );
      grow( p, 6.94, 0.0 );
      }
   if ( boxcox && !log )
      {
      atom( p, A_ORD );
      sans( p, SANS, 0.0, name );
      grow( p, 6.94, 0.0 );
      snprintf( l, sizeof( l ), "(%.2f)", lambda );
      scripts( p, "t", l );
      return;
      }
   name_t( p, name );
}

/*****************************************************************************/
/* The parts                                                                 */
/*****************************************************************************/

typedef struct { const Equation *eq; int part, from, to; } Range;

static void draw_items( Pen *p, const void *arg )
{
   const Range  *r = (const Range *)arg;
   const EqItem *item = r->eq->item[r->part];
   int           i;

   for ( i = r->from; i < r->to; i++ )
       {
       const EqItem *it = &item[i];

       switch ( it->kind )
          {
          case EI_OPEN:  atom( p, A_OPEN ); paren( p, "(" ); break;
          case EI_CLOSE: atom( p, A_CLOSE ); paren( p, ")" ); break;
          case EI_TEXT:
               if ( strcmp( it->text, "1" ) == 0 )
                  {
                  atom( p, A_ORD );
                  /* the annual AR factors wrote the 1 without \mathsf */
                  if ( r->part == EQ_ARA ) glyph( p, F_ROMAN, SIZE, 0.0, "1", 5.0 );
                  else sans( p, SANS, 0.0, "1" );
                  grow( p, 6.94, 0.0 );
                  }
               else if ( strcmp( it->text, "t" ) == 0 )
                  { atom( p, A_ORD ); italic( p, SIZE, 0.0, "t", CM_T ); }
               else if ( strcmp( it->text, "N" ) == 0 )
                  name_t( p, "N" );
               break;
          case EI_COEF:     coef( p, it ); break;
          case EI_B:        bee( p, it ); break;
          case EI_HARMONIC: harmonic( p, it ); break;
          case EI_DUMMY:    dummy( p, it ); break;
          case EI_ALTER:    alternating( p ); break;
          case EI_INPUT:    series( p, it->text, it->a == 1, it->a == 2, it->value ); break;
          case EI_OP:
               if ( it->sign == '/' )                   /* \, / ( 1 - ... )  */
                  {
                  p->x += THIN;
                  atom( p, A_ORD );
                  glyph( p, F_ROMAN, SIZE, 0.0, "/", CM_SLASH );
                  grow( p, 7.5, 2.5 );
                  }
               else
                  {
                  atom( p, A_BIN );
                  op_glyph( p, it->sign );
                  }
               break;
          default: break;
          }
       }
}

static int term_start( const EqItem *it )
{
   return( it->kind == EI_OP && ( it->sign == '+' || it->sign == '-' ) );
}

/* The terms of the first line (EQ_TRANS, EQ_DETER): each one begins with its
 * sign, and the line breaks after the sign when the term does not fit.     */
static void terms( Pen *p, const Equation *eq, int part, int from )
{
   const EqItem *item = eq->item[part];
   int           i, n = eq->n[part], start;
   Range         r;

   r.eq = eq;
   r.part = part;
   for ( i = from; i < n; )
       {
       start = i;
       if ( term_start( &item[i] ) )
          {
          /* fue wrote nothing when the first omega was exactly zero */
          if ( !item[i].a ) { atom( p, A_BIN ); op_glyph( p, item[i].sign ); }
          i++;
          start = i;
          }
       while ( i < n && !term_start( &item[i] ) ) i++;
       r.from = start;
       r.to   = i;
       fit( p, draw_items, &r );
       draw_items( p, &r );
       }
}

/* A factor: (1 - phi B - ...); the line can break after each sign */
static void factor( Pen *p, const Equation *eq, int part, int from, int to )
{
   const EqItem *item = eq->item[part];
   Range         r;
   int           i, start;

   r.eq = eq;
   r.part = part;
   for ( i = from; i < to; )
       {
       start = i;
       if ( item[i].kind == EI_COEF && item[i].sign != ' ' )
          {
          EqItem body = item[i];

          if ( !( body.value == 0.0 && !body.has_se ) )
             {
             atom( p, A_BIN );
             op_glyph( p, body.sign );
             }
          i++;
          while ( i < to && item[i].kind != EI_COEF ) i++;
          r.from = start;
          r.to   = i;
          /* the body without its sign, to measure where it would go */
          {
          Equation e2 = *eq;
          EqItem  *copy = (EqItem *)malloc( ( r.to - r.from ) * sizeof( EqItem ) );

          if ( copy != NULL )
             {
             memcpy( copy, &item[r.from], ( r.to - r.from ) * sizeof( EqItem ) );
             copy[0].sign = ' ';
             e2.item[part] = copy;
             r.eq = &e2;
             r.from = 0;
             r.to = i - start;
             if ( !( body.value == 0.0 && !body.has_se ) ) fit( p, draw_items, &r );
             draw_items( p, &r );
             free( copy );
             r.eq = eq;
             }
          }
          }
       else
          {
          r.from = i;
          r.to = i + 1;
          draw_items( p, &r );
          i++;
          }
       }
}

/* \underset{f = k}{ ... }: an operator atom with its label under it */
typedef struct { const Equation *eq; int part, from, to, f; } Under;

static void under_body( Pen *p, const void *arg )
{
   const Under *u = (const Under *)arg;

   if ( u->part < 0 ) return;
   factor( p, u->eq, u->part, u->from, u->to );
}

static void label_under( Pen *p, double x0, double x1, double depth, int f )
{
   char   num[16];
   double w, x;

   snprintf( num, sizeof( num ), "%d", f );
   w = CM_F * SCRIPT / SIZE + CM_OP * SCRIPT / SIZE + sans_width( SCRIPT_SANS, num );
   x = ( x0 + x1 ) / 2.0 - w / 2.0;
   if ( p->f != NULL )
      {
      double y = p->y - depth - LIMIT_DOWN;

      fd_text( p->f, x, y, F_ITALIC, SCRIPT, FD_LEFT, "f" );
      x += CM_F * SCRIPT / SIZE;
      fd_text( p->f, x + ( CM_OP - fd_text_width( F_ROMAN, SIZE, "=" ) ) * SCRIPT / SIZE / 2.0,
               y, F_ROMAN, SCRIPT, FD_LEFT, "=" );
      x += CM_OP * SCRIPT / SIZE;
      fd_text( p->f, x, y, F_SANS, SCRIPT_SANS, FD_LEFT, num );
      }
   grow( p, 0.0, depth + LIMIT_DOWN + 1.0 );
}

static void fixed_factor( Pen *p, const Under *u )
{
   double w, h, d, x0;
   int    save;

   atom( p, A_OP );
   measure( p, under_body, u, &w, &h, &d );
   x0 = p->x;
   save = p->last;
   p->last = A_NONE;
   factor( p, u->eq, u->part, u->from, u->to );
   p->last = save;
   label_under( p, x0, p->x, d, u->f );
}

/* An individual factor of the annual difference: (1 - sqrt3 B + B^2) */
static void ifadf_factor( Pen *p, int i, int freq )
{
   double x0;
   int    save;

   if ( i == 0 )
      {
      atom( p, A_ORD );
      glyph( p, F_GREEK, SIZE, 0.0, "\321", CM_NABLA );
      grow( p, 6.83, 0.0 );
      return;
      }
   atom( p, A_OP );
   x0 = p->x;
   save = p->last;
   p->last = A_NONE;
   atom( p, A_OPEN ); paren( p, "(" );
   atom( p, A_ORD ); sans( p, SANS, 0.0, "1" );
   if ( freq == 4 || i == 6 || i == 3 )
      {
      atom( p, A_BIN ); op_glyph( p, '+' );
      atom( p, A_ORD ); italic( p, SIZE, 0.0, "B", CM_B );
      if ( ( freq == 4 && i == 1 ) || ( freq != 4 && i == 3 ) ) scripts( p, NULL, "2" );
      }
   else
      {
      atom( p, A_BIN ); op_glyph( p, ( i < 3 ) ? '-' : '+' );
      if ( i == 1 || i == 5 )                                /* sqrt 3       */
         {
         double w = sans_width( SANS, "3" );

         atom( p, A_ORD );
         glyph( p, F_GREEK, SIZE, 0.0, SYM_RADICAL, 0.0 );
         if ( p->f != NULL )
            {
            fd_linewidth( p->f, RULE );
            fd_line( p->f, p->x, p->y + 7.6, p->x + w, p->y + 7.6 );
            }
         sans( p, SANS, 0.0, "3" );
         grow( p, 8.2, 1.0 );
         }
      else atom( p, A_ORD );
      italic( p, SIZE, 0.0, "B", CM_B );
      atom( p, A_BIN ); op_glyph( p, '+' );
      atom( p, A_ORD ); italic( p, SIZE, 0.0, "B", CM_B );
      scripts( p, NULL, "2" );
      }
   atom( p, A_CLOSE ); paren( p, ")" );
   p->last = save;
   label_under( p, x0, p->x, 2.5, i );
}

/* The factors of a part: ( ... ) ( ... ), or with fixed frequency */
static void factors( Pen *p, const Equation *eq, int part )
{
   const EqItem *item = eq->item[part];
   int           i, n = eq->n[part], start;

   if ( !eq->used[part] ) return;
   for ( i = 0; i < n; )
       {
       start = i;
       do { i++; } while ( i < n && item[i].kind != EI_OPEN );
       if ( item[start].kind == EI_OPEN && item[start].bracket == 'f' )
          {
          Under u = { eq, part, start, i, item[start].a };

          fixed_factor( p, &u );          /* its opening, as a "("         */
          }
       else
          factor( p, eq, part, start, i );
       }
}

/*****************************************************************************/
/* The equation                                                              */
/*****************************************************************************/

/* [ nabla ... N_t - mu ]: inside \left[ and \right], or alone without mu */
static void differences( Pen *p, const void *arg )
{
   const Equation *eq = *(const Equation **)arg;
   int             i, noise = eq->used[EQ_DETER] || eq->used[EQ_TRANS];

   if ( eq->used[EQ_NRDIFF] )
      {
      char t[16];

      atom( p, A_ORD );
      glyph( p, F_GREEK, SIZE, 0.0, "\321", CM_NABLA );
      grow( p, 6.83, 0.0 );
      if ( eq->item[EQ_NRDIFF][0].a > 1 )
         {
         snprintf( t, sizeof( t ), "%d", eq->item[EQ_NRDIFF][0].a );
         scripts( p, NULL, t );
         }
      }
   if ( eq->used[EQ_NADIFF] )
      {
      char t[16];

      atom( p, A_ORD );
      glyph( p, F_GREEK, SIZE, 0.0, "\321", CM_NABLA );
      grow( p, 6.83, 0.0 );
      snprintf( t, sizeof( t ), "%d", eq->item[EQ_NADIFF][0].b );
      scripts( p, t, NULL );
      }
   if ( eq->used[EQ_IFADF] )
      for ( i = 0; i < eq->n[EQ_IFADF]; i++ )
          ifadf_factor( p, ( eq->item[EQ_IFADF][i].kind == EI_NABLA ) ? 0
                           : eq->item[EQ_IFADF][i].a, eq->freq );
   if ( noise ) name_t( p, "N" );
   else series( p, eq->name, eq->is_log, eq->boxcox, eq->lambda );
   if ( eq->used[EQ_MU] ) coef( p, &eq->item[EQ_MU][0] );
}

/* \left[ ... \right]: the delimiters grow with what they hold */
static void bracket_group( Pen *p, const void *arg )
{
   double w, h, d, m, T, top, bot, x;
   int    save;

   atom( p, A_INNER );
   measure( p, differences, arg, &w, &h, &d );
   m = ( h - AXIS > d + AXIS ) ? h - AXIS : d + AXIS;
   T = 2.0 * m * 0.901;
   if ( 2.0 * m - 5.0 > T ) T = 2.0 * m - 5.0;
   T = 6.0 * ceil( T / 6.0 );                       /* the pieces of cmex    */
   top = AXIS + T / 2.0;
   bot = AXIS - T / 2.0;

   x = p->x;
   if ( p->f != NULL )
      {
      fd_linewidth( p->f, RULE );
      fd_line( p->f, x + 5.5, p->y + top, x + 2.4, p->y + top );
      fd_line( p->f, x + 2.4, p->y + top, x + 2.4, p->y + bot );
      fd_line( p->f, x + 2.4, p->y + bot, x + 5.5, p->y + bot );
      }
   p->x += CM_DELIM;
   save = p->last;
   p->last = A_NONE;
   differences( p, arg );
   p->last = save;
   x = p->x;
   if ( p->f != NULL )
      {
      fd_line( p->f, x + 1.15, p->y + top, x + 4.25, p->y + top );
      fd_line( p->f, x + 4.25, p->y + top, x + 4.25, p->y + bot );
      fd_line( p->f, x + 4.25, p->y + bot, x + 1.15, p->y + bot );
      }
   p->x += CM_DELIM;
   grow( p, top, -bot );
}

static void noise_left( Pen *p, const void *arg )
{
   const Equation *eq = *(const Equation **)arg;

   if ( eq->used[EQ_MU] ) bracket_group( p, arg );
   else differences( p, arg );
}

/* sigma-hat with its subscript */
static void sigma_hat( Pen *p, const char *sub )
{
   double x0;

   atom( p, A_ORD );
   x0 = p->x;
   greek( p, SIZE, 0.0, SYM_SIGMA, CM_SIGMA );
   if ( p->f != NULL )
      {
      double cx = x0 + CM_SIGMA / 2.0 + SLANT * 4.3 + 0.3, top = p->y + 5.4;

      fd_linewidth( p->f, RULE );
      fd_line( p->f, cx - 1.7, top, cx, top + 1.5 );
      fd_line( p->f, cx, top + 1.5, cx + 1.7, top );
      }
   grow( p, 7.2, 0.0 );
   scripts( p, sub, NULL );
}

static void tail_sigma( Pen *p, const void *arg )
{
   const Equation *eq = *(const Equation **)arg;
   char            text[64];

   sigma_hat( p, eq->residuals );
   atom( p, A_REL );
   op_glyph( p, '=' );
   atom( p, A_ORD );
   if ( eq->sigma_pct )
      {
      snprintf( text, sizeof( text ), "%.2f", eq->sigma );
      sans( p, SANS, 0.0, text );
      glyph( p, F_ROMAN, SIZE, 0.0, "%", CM_PCT );
      }
   else
      {
      snprintf( text, sizeof( text ), "%.4g", eq->sigma );
      sans( p, SANS, 0.0, text );
      }
   grow( p, 7.5, 0.8 );
}

static void tail_a( Pen *p, const void *arg )
{
   const Equation *eq = *(const Equation **)arg;

   name_t( p, eq->residuals );
}

/* The first part of the MA side, to know if the line breaks after "=" */
static void ma_first( Pen *p, const void *arg )
{
   const Equation *eq = *(const Equation **)arg;
   int             part;

   for ( part = EQ_MAR; part <= EQ_MAF; part++ )
       if ( eq->used[part] && eq->n[part] > 0 )
          {
          const EqItem *item = eq->item[part];
          int           i = 0;

          do { i++; } while ( i < eq->n[part] && item[i].kind != EI_OPEN );
          if ( item[0].bracket == 'f' )
             {
             Under u = { eq, part, 0, i, item[0].a };
             fixed_factor( p, &u );
             }
          else factor( p, eq, part, 0, i );
          return;
          }
   tail_a( p, arg );
}

static void draw_all( Pen *p, const Equation *eq )
{
   const Equation *arg = eq;

   /* ln Y_t = transfers + deterministic terms + N_t */
   if ( eq->used[EQ_TRANS] || eq->used[EQ_DETER] )
      {
      series( p, eq->name, eq->is_log, eq->boxcox, eq->lambda );
      atom( p, A_REL );
      op_glyph( p, '=' );
      if ( eq->used[EQ_TRANS] ) terms( p, eq, EQ_TRANS, 0 );
      if ( eq->used[EQ_DETER] ) terms( p, eq, EQ_DETER, 2 );
      else
         {
         EqItem plus[2];
         Equation e2 = *eq;
         Range    r;

         memset( plus, 0, sizeof( plus ) );
         plus[0].kind = EI_OP; plus[0].sign = '+';
         plus[1].kind = EI_TEXT; snprintf( plus[1].text, sizeof( plus[1].text ), "N" );
         e2.item[EQ_DETER] = plus;
         e2.n[EQ_DETER] = 2;
         atom( p, A_BIN );
         op_glyph( p, '+' );
         r.eq = &e2; r.part = EQ_DETER; r.from = 1; r.to = 2;
         fit( p, draw_items, &r );
         draw_items( p, &r );
         }
      newline( p );                                         /* \\           */
      }

   /* phi(B) [ nabla N_t - mu ] = theta(B) A_t ;  sigma */
   factors( p, eq, EQ_ARR );
   factors( p, eq, EQ_ARA );
   factors( p, eq, EQ_ARF );
   fit( p, noise_left, &arg );
   noise_left( p, &arg );
   atom( p, A_REL );
   op_glyph( p, '=' );
   fit( p, ma_first, &arg );
   factors( p, eq, EQ_MAR );
   factors( p, eq, EQ_MAA );
   factors( p, eq, EQ_MAF );
   tail_a( p, &arg );
   atom( p, A_PUNCT );
   glyph( p, F_ROMAN, SIZE, 0.0, ";", CM_SEMI );
   p->x += QUAD;               /* \quad; the thin space after ; comes with sigma */
   if ( p->wrap )
      {
      double w, h, d;

      measure( p, tail_sigma, &arg, &w, &h, &d );
      if ( p->x + w > p->right ) newline( p );
      }
   tail_sigma( p, &arg );
}

double eq_draw( FDFig *f, const Equation *eq, double top, double left,
                double right, double *depth )
{
   Pen   *p = (Pen *)calloc( 1, sizeof( Pen ) );
   double y;
   int    i, n;

   if ( p == NULL ) { if ( depth ) *depth = 0.0; return( top ); }

   /* the first pass: how high and deep each line is */
   p->left = left;
   p->right = right;
   p->x = left;
   p->wrap = 1;
   draw_all( p, eq );
   n = p->line + 1;

   /* the baselines, as TeX sets them */
   y = top;
   for ( i = 0; i < n; i++ )
       {
       double prev = ( i == 0 ) ? 0.0 : p->d[i - 1], gap;

       gap = ( BSKIP - prev - p->h[i] >= 0.0 ) ? BSKIP : prev + LSKIP + p->h[i];
       y -= gap;
       p->base[i] = y;
       }

   /* the second pass, drawing */
   {
   double h[MAXLINES], d[MAXLINES], base[MAXLINES];

   memcpy( h, p->h, sizeof( h ) );
   memcpy( d, p->d, sizeof( d ) );
   memcpy( base, p->base, sizeof( base ) );
   memset( p, 0, sizeof( Pen ) );
   memcpy( p->base, base, sizeof( base ) );
   p->f = f;
   p->left = left;
   p->right = right;
   p->x = left;
   p->y = base[0];
   p->wrap = 1;
   p->drawing = 1;
   draw_all( p, eq );
   if ( depth ) *depth = d[n - 1];
   (void) h;
   }
   y = p->base[n - 1];
   free( p );
   return( y );
}

/*****************************************************************************/
/* The table of the outliers: the tabular of _res.tex                        */
/*****************************************************************************/

int eq_outliers( const double *res, int n, double mean, double sd,
                 void (*date)( int i, char *buf, size_t size, void *data ),
                 void *data, EqOutlier *out, int max )
{
   int i, k = 0;

   for ( i = 0; i < n && k < max; i++ )
       {
       double z = ( sd > 0.0 ) ? ( res[i] - mean ) / sd : 0.0;

       if ( fabs( z ) < 3.0 ) continue;
       out[k].obs = i + 1;
       out[k].z = z;
       out[k].date[0] = '\0';
       if ( date != NULL ) date( i, out[k].date, sizeof( out[k].date ), data );
       k++;
       }
   return( k );
}

static double roman_width( const char *s )
{
   return( fd_text_width( F_ROMAN, SIZE, s ) );
}

static void table( FDFig *f, const EqOutlier *out, int nout, int dates,
                   double left, double base, double comments_x )
{
   const char *head[3] = { "Obs.", "Date", "Std. Val." };
   double      w[3], x[3], right, y;
   char        text[64];
   int         i, c, ncol = dates ? 3 : 2;

   /* the width of each column: the widest of its cells */
   w[0] = roman_width( head[0] );
   w[1] = roman_width( head[1] );
   w[2] = roman_width( head[2] );
   for ( i = 0; i < nout; i++ )
       {
       snprintf( text, sizeof( text ), "%d", out[i].obs );
       if ( roman_width( text ) > w[0] ) w[0] = roman_width( text );
       if ( roman_width( out[i].date ) > w[1] ) w[1] = roman_width( out[i].date );
       snprintf( text, sizeof( text ), "%.2f", out[i].z );
       if ( roman_width( text ) > w[2] ) w[2] = roman_width( text );
       }
   if ( !dates ) { w[1] = w[2]; head[1] = head[2]; }
   x[0] = left + TABCOLSEP;
   for ( c = 1; c < ncol; c++ ) x[c] = x[c - 1] + w[c - 1] + 2.0 * TABCOLSEP;
   right = x[ncol - 1] + w[ncol - 1] + TABCOLSEP;

   fd_linewidth( f, RULE );
   fd_line( f, left, base + STRUT_H + RULE / 2.0, right, base + STRUT_H + RULE / 2.0 );
   for ( c = 0; c < ncol; c++ )
       fd_text( f, x[c] + w[c] / 2.0, base, F_ROMAN, SIZE, FD_CENTER, head[c] );
   fd_line( f, left, base - STRUT_D - RULE / 2.0, right, base - STRUT_D - RULE / 2.0 );

   y = base;
   for ( i = 0; i < nout; i++ )
       {
       y -= ROW;
       snprintf( text, sizeof( text ), "%d", out[i].obs );
       fd_text( f, x[0] + w[0] / 2.0, y, F_ROMAN, SIZE, FD_CENTER, text );
       snprintf( text, sizeof( text ), "%.2f", out[i].z );
       if ( dates )
          {
          fd_text( f, x[1] + w[1] / 2.0, y, F_ROMAN, SIZE, FD_CENTER, out[i].date );
          fd_text( f, x[2] + w[2] / 2.0, y, F_ROMAN, SIZE, FD_CENTER, text );
          }
       else
          fd_text( f, x[1] + w[1] / 2.0, y, F_ROMAN, SIZE, FD_CENTER, text );
       }

   /* \underline{\textbf{Comments:}}, in the second column. There is no bold
    * roman among the standard fonts that fugdraw uses: it is overprinted.  */
   y = base + 1.8;
   for ( c = 0; c < 3; c++ )
       fd_text( f, comments_x + 0.22 * c, y, F_ROMAN, SIZE, FD_LEFT, "Comments:" );
   fd_line( f, comments_x, y - 2.3, comments_x + roman_width( "Comments:" ) + 0.44, y - 2.3 );
}

/*****************************************************************************/
/* The page                                                                  */
/*****************************************************************************/

int eq_page_pdf( const char *pdf, FDFig *graph, const Equation *eq,
                 const EqOutlier *out, int nout, int dates )
{
   const double PW = 841.89, PH = 595.276;          /* A4, landscape         */
   double       s, gw, gh, gtop, top, base, depth, textw, x[2], y[2], scale[2];
   FDFig       *page, *figs[2];
   FDPdf       *doc;
   int          n = 0, status;

   page = fd_fig_new( PW, PH );
   if ( page == NULL ) return( 1 );

   /* \includegraphics[scale=1], or .90 with a long annual series */
   s  = ( eq->freq == 1 && eq->nobs > 200 ) ? 0.90 : 1.0;
   gw = ( graph != NULL ) ? s * graph->w : 0.0;
   gh = ( graph != NULL ) ? s * graph->h : 0.0;
   gtop = PH - MARGIN - GRAPH_TOP;
   (void) gw;

   top  = gtop - gh;
   base = eq_draw( page, eq, top, MARGIN, PW - MARGIN, &depth );

   textw = PW - 2.0 * MARGIN;
   table( page, out, nout, dates, MARGIN, base - depth - TABLE_GAP,
          MARGIN + ( textw - COLSEP ) / 2.0 + COLSEP );

   if ( ( doc = fd_pdf_open( pdf ) ) == NULL )
      {
      fd_fig_free( page );
      return( 1 );
      }
   if ( graph != NULL )
      {
      figs[n] = graph;
      x[n] = MARGIN;
      y[n] = gtop - gh;
      scale[n] = s;
      n++;
      }
   figs[n] = page;
   x[n] = 0.0;
   y[n] = 0.0;
   scale[n] = 1.0;
   n++;
   fd_pdf_page( doc, PW, PH, figs, x, y, scale, n );
   status = fd_pdf_close( doc );
   fd_fig_free( page );
   return( status );
}

/*****************************************************************************/
/* The decimals (fue 1.13.1)                                                 */
/*****************************************************************************/

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
/* Building an equation                                                      */
/*****************************************************************************/

int eq_new( Equation *eq )
{
   int i;

   memset( eq, 0, sizeof( *eq ) );
   for ( i = 0; i < EQ_NPARTS; i++ )
       {
       eq->item[i] = (EqItem *)calloc( EQ_MAX_ITEMS, sizeof( EqItem ) );
       if ( eq->item[i] == NULL ) { eq_release( eq ); return( 1 ); }
       }
   return( 0 );
}

void eq_release( Equation *eq )
{
   int i;

   for ( i = 0; i < EQ_NPARTS; i++ )
       {
       free( eq->item[i] );
       eq->item[i] = NULL;
       eq->n[i] = 0;
       }
}

EqItem *eq_add( Equation *eq, EqPart part, EqItemKind kind )
{
   EqItem *it;

   if ( eq->n[part] >= EQ_MAX_ITEMS ) return( &eq->item[part][EQ_MAX_ITEMS - 1] );
   it = &eq->item[part][eq->n[part]++];
   memset( it, 0, sizeof( *it ) );
   it->kind = kind;
   it->sign = ' ';
   eq->used[part] = 1;
   return( it );
}

void eq_add_text( Equation *eq, EqPart part, const char *text )
{
   snprintf( eq_add( eq, part, EI_TEXT )->text, sizeof( ((EqItem *)0)->text ), "%s", text );
}

void eq_add_op( Equation *eq, EqPart part, char op )
{
   eq_add( eq, part, EI_OP )->sign = op;
}

void eq_add_open( Equation *eq, EqPart part, char bracket, int f )
{
   EqItem *it = eq_add( eq, part, EI_OPEN );

   it->bracket = bracket;
   it->a = f;
}

void eq_add_close( Equation *eq, EqPart part, char bracket )
{
   eq_add( eq, part, EI_CLOSE )->bracket = bracket;
}

void eq_add_coef( Equation *eq, EqPart part, int style, char sign,
                  double value, int estimated, double se )
{
   EqItem *it = eq_add( eq, part, EI_COEF );

   it->style  = style;
   it->sign   = sign;
   it->value  = fabs( value );
   it->has_se = estimated;
   it->se     = se;
}

void eq_add_b( Equation *eq, EqPart part, int power, int always )
{
   EqItem *it = eq_add( eq, part, EI_B );

   it->a = power;
   it->b = always;
}

void eq_harmonic_angle( int h, int s, int *a, int *b )
{
   int num = h, den = ( s > 1 ) ? s / 2 : 1, x = num, y = den, t;

   while ( y != 0 ) { t = x % y; x = y; y = t; }
   if ( x < 1 ) x = 1;
   *a = num / x;
   *b = den / x;
}
