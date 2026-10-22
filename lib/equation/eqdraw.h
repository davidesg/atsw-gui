/*****************************************************************************/
/* eqdraw.h -- the page of an estimated model, drawn as fue's LaTeX file is  */
/* typeset. Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 later.  */
/*                                                                           */
/* ONE PAGE FOR EVERY MODEL. fue (the univariate model) and drtran_gui (the  */
/* model of a case, one page per equation) write the same page with the same */
/* code: the graph of the residuals, the equation and the table of the       */
/* residuals over three standard deviations. A model with inputs only adds   */
/* its transfer (EQ_TRANS) in the first line, before the deterministic       */
/* terms.                                                                    */
/*                                                                           */
/* THE TYPESETTING IS THAT OF fue -latex (eqlatex.c compiled with pdflatex): */
/* A4 landscape with 2.5 cm margins, the equation in \footnotesize of a 12pt */
/* article with \linespread{1.6}, TeX's spacing between atoms (thin, medium  */
/* and thick), \est as eqlatex.c defines it (the value and its standard      */
/* error trimmed by 0.125in on each side), the line breaks of TeX (after a   */
/* binary operator or a relation), and the table of residuals as the         */
/* tabular of _res.tex. The glyphs are those of the standard PDF fonts       */
/* (nothing is embedded), placed in the advance of the Computer Modern one:  */
/* sans serif for numbers and names (\mathsf), Times for the roman, Times    */
/* Italic for the math italic (B, t, f) and Symbol, slanted, for the Greek.  */
/*****************************************************************************/

#ifndef EQDRAW_H
#define EQDRAW_H

#include "equation.h"
#include "fugdraw.h"

/* A residual over three standard deviations, for the table */
typedef struct {
   int    obs;              /* its number, from 1                            */
   char   date[32];         /* season/year, or "" with numbered data         */
   double z;                /* its standardized value                        */
} EqOutlier;

/* Draw the equation with its first line hanging from top (the y of the line
 * above it; its depth is 0) between left and right. Returns the baseline of
 * its last line and leaves in *depth how far that line goes below it.      */
double eq_draw( FDFig *f, const Equation *eq, double top, double left,
                double right, double *depth );

/* Write the page in pdf: graph (or NULL) at the top, the equation and the
 * table of the outliers (dates: the table has a Date column). Returns 0 if
 * the file was written.                                                    */
int eq_page_pdf( const char *pdf, FDFig *graph, const Equation *eq,
                 const EqOutlier *out, int nout, int dates );

/* The outliers of res[0..n-1]: the residuals whose standardized value is 3
 * or more in absolute value, with the mean and the standard deviation given.
 * date(i) writes the date of residual i (0-based), or is NULL. Fills at most
 * max and returns how many.                                               */
int eq_outliers( const double *res, int n, double mean, double sd,
                 void (*date)( int i, char *buf, size_t size, void *data ),
                 void *data, EqOutlier *out, int max );

/* Building an equation that does not come from fue (eq_build): the same
 * items, appended to a part. eq_new allocates the parts, eq_release frees
 * them.                                                                    */
int     eq_new( Equation *eq );
void    eq_release( Equation *eq );
EqItem *eq_add( Equation *eq, EqPart part, EqItemKind kind );
void    eq_add_text( Equation *eq, EqPart part, const char *text );
void    eq_add_op( Equation *eq, EqPart part, char op );
void    eq_add_open( Equation *eq, EqPart part, char bracket, int f );
void    eq_add_close( Equation *eq, EqPart part, char bracket );
void    eq_add_coef( Equation *eq, EqPart part, int style, char sign,
                     double value, int estimated, double se );
void    eq_add_b( Equation *eq, EqPart part, int power, int always );

/* cos or sin of harmonic h of a series of period s: its angle h*pi/(s/2),
 * as a fraction a/b of pi in lowest terms (fue: pi/6, pi/3, ..., 5pi/6).  */
void    eq_harmonic_angle( int h, int s, int *a, int *b );

#endif
