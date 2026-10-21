/*****************************************************************************/
/* equation.h -- the estimated model as an equation, apart from how it is    */
/* written. Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 later.  */
/*                                                                           */
/* src/equation.c walks the model once and builds this; src/eqlatex.c writes */
/* it as the LaTeX file of fue (byte for byte the one of fue 1.13.1).        */
/* Drawing it directly, without LaTeX, is another writer over the same       */
/* structure.                                                                */
/*****************************************************************************/

#ifndef EQUATION_H
#define EQUATION_H

#include <stdio.h>

/* How a coefficient is written: which decimals, and the spacing that fue    */
/* 1.13.1 used in each part of the equation.                                 */
typedef enum {
   CS_OMEGA,        /* omegas of a deterministic variable                    */
   CS_DELTA,        /* deltas (denominator) of a deterministic variable      */
   CS_PHI,          /* AR and MA factors                                     */
   CS_PHIF,         /* AR and MA factors with fixed frequency                */
   CS_MU            /* mean parameter                                        */
} CoefStyle;

typedef enum {
   EI_TEXT,         /* text as it is: a name, "1", "t", "N", "A"             */
   EI_COEF,         /* a coefficient, with its standard error under it       */
   EI_B,            /* the operator B, or B^k                                */
   EI_NABLA,        /* nabla, with its power and its period                  */
   EI_HARMONIC,     /* cos or sin of (num/den) pi t                          */
   EI_DUMMY,        /* an intervention variable: xi_t, with kind and date    */
   EI_ALTER,        /* (-1)^t                                                */
   EI_OPEN,         /* ( or [, and the (1 of a factor of fixed frequency     */
   EI_CLOSE,        /* ) or ]                                                */
   EI_OP,           /* an operator between terms: '+', '-', '=', '/'         */
   EI_INPUT         /* the input of a transfer: text the name, a 0 the level,*/
                    /* 1 its log, 2 Box-Cox with value lambda                */
} EqItemKind;

typedef struct {
   EqItemKind kind;
   char       text[96];   /* EI_TEXT, EI_DUMMY (the date), EI_HARMONIC (cos) */
   char       sign;       /* EI_COEF: ' ' (the first one), '+' or '-'        */
   double     value;      /* EI_COEF: the value, already scaled and positive */
   double     se;         /* EI_COEF: its standard error (0 if it is fixed)  */
   int        has_se;     /* EI_COEF: it was estimated                        */
   int        style;      /* EI_COEF: CoefStyle                              */
   int        a, b;       /* EI_B: the power. EI_HARMONIC: num, den of pi.   */
                          /* EI_NABLA: power, period. EI_DUMMY: season, year */
                          /* EI_OPEN/EI_CLOSE: the frequency f of the factor */
   char       bracket;    /* EI_OPEN, EI_CLOSE: '(', '[' or 'f' (fixed freq) */
} EqItem;

/* The parts of the equation, each one a list of items. The equation is
 *
 *    trans deter                    (if there are inputs or det. vars)
 *    arr ara arf [ nrdiff nadiff ifadf N_t mu ] = mar maa maf A_t ; sigma
 *
 * EQ_TRANS is the transfer of a model with inputs (drtran): its terms go in
 * the first line before the deterministic ones, written as fue writes a
 * deterministic variable, with the input in the place of xi. fue does not
 * use it: the univariate model leaves it empty.                            */
typedef enum {
   EQ_DETER, EQ_ARR, EQ_ARA, EQ_ARF, EQ_MAR, EQ_MAA, EQ_MAF,
   EQ_MU, EQ_NRDIFF, EQ_NADIFF, EQ_IFADF, EQ_TRANS, EQ_NPARTS
} EqPart;

#define EQ_MAX_ITEMS 4096

typedef struct {
   EqItem *item[EQ_NPARTS];
   int     n[EQ_NPARTS];
   int     used[EQ_NPARTS];        /* the part has something to write        */

   char    name[96];               /* name of the series                     */
   char    residuals[96];          /* name of the residuals (A<input>)       */
   int     is_log;                 /* Box-Cox lambda = 0                     */
   int     boxcox;                 /* another lambda (not 0 nor 1): lambda   */
   double  lambda;
   int     freq, nobs;
   double  sigma;                  /* sqrt(sigma2) / refactor, x 100 if pct  */
   int     sigma_pct;              /* sigma in %: logs, no geometric mean    */
} Equation;

/* Build the equation of the model that has just been estimated. inputv is
 * the .inp file, open and positioned at its first line: the kinds and the
 * dates of the deterministic variables are only there. dev holds the
 * standard errors in the order in which the parameters were estimated, and
 * name is the name of the series. Fill in eq->sigma afterwards.            */
void eq_build( Equation *eq, FILE *inputv, const double *dev, const char *name );
void eq_free( Equation *eq );

/* The decimals a coefficient is written with (both writers use it) */
int  eq_decimals( const EqItem *it );

/* Write the LaTeX file of fue: the same file as fue 1.13.1 (src/eqlatex.c) */
void eq_write_latex( FILE *texputv, const Equation *eq, const char *x11out );

#endif
