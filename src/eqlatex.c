/*****************************************************************************/
/* eqlatex.c -- the equation (include/equation.h) written as the LaTeX file  */
/* of fue. Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 or later.*/
/*                                                                           */
/* This writes the same file, byte for byte, as fue 1.13.1 wrote in its      */
/* section [11], so the spacing of each part is the one it used, quirks      */
/* included (they are written down in the table below and in the comments).  */
/*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include "equation.h"

/*****************************************************************************/
/* How each kind of coefficient is written                                   */
/*****************************************************************************/

typedef struct {
   const char *first;        /* before the first coefficient of a term       */
   const char *minus;
   const char *plus;
   int         value_width;  /* %<width>.<decimals>f of the value            */
   const char *tail;         /* after the value                              */
   int         est_if_fixed; /* a fixed coefficient also uses \est{}         */
} CoefLatex;

/* The five decimal levels; -1: the level does not exist for that style.
 * The widths and the spaces are those of fue 1.13.1.                        */
static const CoefLatex coef_latex[5][5] = {
   /* CS_OMEGA: 2, 3, 4, 5 and 6 decimals                                    */
   { { "    ", "  - ", "  + ", 2, " ", 0 },
     { "    ", "  - ", "  + ", 2, "",  0 },
     { "    ", "  - ", "  + ", 2, "",  0 },
     { "    ", "  - ", "  + ", 6, "",  0 },
     { "    ", "  - ", "  + ", 6, "",  0 } },
   /* CS_DELTA: 2, 3 and 4 decimals; a fixed delta also uses \est{}          */
   { { "  - ", "  - ", "  + ", 2, " ", 1 },
     { "  - ", "  - ", "  + ", 2, " ", 1 },
     { "  - ", "  - ", "  + ", 2, " ", 1 },
     { NULL,   NULL,   NULL,   0, "",  0 },
     { NULL,   NULL,   NULL,   0, "",  0 } },
   /* CS_PHI: always 2 decimals                                              */
   { { " - ", " - ", " + ", 6, "", 0 },
     { NULL,  NULL,  NULL,  0, "", 0 },
     { NULL,  NULL,  NULL,  0, "", 0 },
     { NULL,  NULL,  NULL,  0, "", 0 },
     { NULL,  NULL,  NULL,  0, "", 0 } },
   /* CS_PHIF: the term in B^2 of a factor of fixed frequency                */
   { { "- ", "- ", "+ ", 6, "", 0 },
     { NULL, NULL, NULL, 0, "", 0 },
     { NULL, NULL, NULL, 0, "", 0 },
     { NULL, NULL, NULL, 0, "", 0 },
     { NULL, NULL, NULL, 0, "", 0 } },
   /* CS_MU                                                                  */
   { { "- ", "- ", "+ ", 4, "", 0 },
     { "- ", "- ", "+ ", 4, "", 0 },
     { "- ", "- ", "+ ", 4, "", 0 },
     { "- ", "- ", "+ ", 4, "", 0 },
     { "- ", "- ", "+ ", 4, "", 0 } }
};

/* The standard error under the value. With 4 decimals the omegas put the
 * parentheses inside \mathsf and the others outside: it was so in 1.13.1. */
static void write_se( FILE *f, const EqItem *it, int decimals )
{
   char fmt[64];

   if ( it->style == CS_OMEGA && decimals == 4 )
      snprintf( fmt, sizeof( fmt ), "{\\mathsf{(%%6.%df)}}", decimals );
   else if ( it->style == CS_OMEGA || it->style == CS_DELTA )
      snprintf( fmt, sizeof( fmt ), "{(\\mathsf{%%%d.%df})}%s", 2, decimals,
                ( decimals == 2 || it->style == CS_DELTA ) ? " " : "" );
   else if ( it->style == CS_MU )
      snprintf( fmt, sizeof( fmt ), "  {(\\mathsf{%%4.%df})}} \n ", decimals );
   else
      snprintf( fmt, sizeof( fmt ), "{(\\mathsf{%%6.%df})}", decimals );
   fprintf( f, fmt, it->se );
}

/* fue 1.13.1 chose the sign by comparing the coefficient with zero, so a
 * coefficient that is exactly zero was written with neither sign nor value
 * (its standard error was written all the same). Only the first omega of a
 * term, which carries no sign, was written when it was zero.               */
static void write_coef( FILE *f, const EqItem *it )
{
   int             decimals = eq_decimals( it );
   const CoefLatex *c = &coef_latex[it->style][decimals - 2];
   char            fmt[96];

   if ( it->value != 0.0 || it->sign == ' ' )
      {
      fprintf( f, "%s", ( it->sign == ' ' ) ? c->first :
                        ( it->sign == '-' ) ? c->minus : c->plus );
      if ( it->has_se || c->est_if_fixed )
         snprintf( fmt, sizeof( fmt ), "\\est{\\mathsf{%%%d.%df}}%s", c->value_width,
                   decimals, c->tail );
      else
         snprintf( fmt, sizeof( fmt ), "\\mathsf{%%%d.%df}%s", c->value_width,
                   decimals, c->tail );
      fprintf( f, fmt, it->value );
      }
   if ( it->has_se ) write_se( f, it, decimals );
}

/*****************************************************************************/
/* The deterministic part                                                    */
/*****************************************************************************/

/* xi_t of an intervention, with its kind and its date. The impulses were
 * written without the three spaces of the steps and the ramps, and easter
 * and trend were not written at all: fue 1.13.1 had no branch for them.    */
static void write_dummy( FILE *f, const EqItem *it )
{
   const char *space = "   ";

   if ( strcmp( it->text, "season" ) == 0 )
      { fprintf( f, "%s\\xi_t^{%d}", space, it->a ); return; }
   if ( it->text[0] == '\0' )                           /* non-standard      */
      { fprintf( f, "%s\\xi_t", space ); return; }
   if ( strcmp( it->text, "I" ) != 0 && strcmp( it->text, "CI" ) != 0 &&
        strcmp( it->text, "S" ) != 0 && strcmp( it->text, "R" ) != 0 )
      return;                                           /* easter, trend    */
   if ( it->text[0] == 'I' || it->text[0] == 'C' ) space = "";
   if ( it->a == 0 )                                    /* annual: the year */
      fprintf( f, "%s\\xi_{t}^{%s,\\mathsf{%d}}", space, it->text, it->b );
   else
      fprintf( f, "%s\\xi_{t}^{%s,\\mathsf{%d/%d}}", space, it->text, it->a, it->b );
}

static void write_harmonic( FILE *f, const EqItem *it )
{
   const char *fn = ( it->text[0] == 'c' ) ? "\\cos" : "\\sin";

   /* the third harmonic of a sine carried a space in 1.13.1 */
   if ( it->text[0] == 's' && it->value == 3 )
      fprintf( f, "%s\\frac{\\pi}{\\mathsf{2} }t", fn );
   else if ( it->a == 1 )
      fprintf( f, "%s\\frac{\\pi}{\\mathsf{%d}}t", fn, it->b );
   else
      fprintf( f, "%s\\frac{\\mathsf{%d}\\pi}{\\mathsf{%d}}t", fn, it->a, it->b );
}

static void write_deter( FILE *f, const Equation *eq )
{
   const EqItem *item = eq->item[EQ_DETER];
   int           i, n = eq->n[EQ_DETER], nomega;

   fprintf( f, "\\newcommand{\\deter}{" );
   if ( eq->is_log ) fprintf( f, "\\ln " );
   fprintf( f, "\\mathsf{%s_{t}} =\n", eq->name );

   for ( i = 2; i < n; i++ )              /* [0] and [1]: the name and the = */
       {
       const EqItem *it = &item[i];

       switch ( it->kind )
          {
          case EI_OP:
               if ( it->sign == '/' )
                  { fprintf( f, " \\,  / ( 1 " ); i++; i++; }  /* ( and "1"  */
               else if ( it->sign == '+' && i + 1 < n && item[i+1].kind == EI_TEXT &&
                         strcmp( item[i+1].text, "N" ) == 0 )
                  { fprintf( f, "+ \\mathsf{N_t}}" ); i++; }
               else
                  {
                  /* the sign of a term, with its parenthesis if it has one  */
                  nomega = ( i + 1 < n && item[i+1].kind == EI_OPEN );
                  if ( !it->a )                  /* a: the omega was zero    */
                     fprintf( f, "%c %s", it->sign, nomega ? "(" : "" );
                  if ( nomega ) i++;
                  }
               break;
          case EI_COEF:   write_coef( f, it ); break;
          case EI_B:      if ( it->a == 1 ) fprintf( f, " B" );
                          else fprintf( f, " B^{\\mathsf{%d}}", it->a );
                          break;
          case EI_CLOSE:  fprintf( f, ")" ); break;
          case EI_DUMMY:  write_dummy( f, it ); break;
          case EI_HARMONIC: write_harmonic( f, it ); break;
          case EI_ALTER:  fprintf( f, "(\\mathsf{-1})^{t}" ); break;
          case EI_TEXT:   if ( strcmp( it->text, "t" ) == 0 ) fprintf( f, " t" );
                          break;
          default:        break;
          }
       /* every deterministic variable ends its line */
       if ( it->kind == EI_DUMMY || it->kind == EI_HARMONIC || it->kind == EI_ALTER ||
            ( it->kind == EI_TEXT && strcmp( it->text, "t" ) == 0 ) )
          fprintf( f, "\n" );
       }
   fprintf( f, "\n" );
}

/*****************************************************************************/
/* The AR and MA factors                                                     */
/*****************************************************************************/

/* Each part opened its parenthesis with its own spacing in fue 1.13.1, and
 * the annual AR factors wrote the 1 without \mathsf.                       */
static void write_factors( FILE *f, const Equation *eq, EqPart part, const char *macro,
                           const char *open )
{
   const EqItem *item = eq->item[part];
   int           i, n = eq->n[part];

   fprintf( f, "\\newcommand{\\%s}{ ", macro );
   for ( i = 0; i < n; i++ )
       {
       const EqItem *it = &item[i];

       switch ( it->kind )
          {
          case EI_OPEN:  fprintf( f, "%s", open ); i++;          /* and the 1 */
                         break;
          case EI_COEF:  write_coef( f, it ); break;
          case EI_B:     if ( it->a == 1 && !it->b ) fprintf( f, " B" );
                         else fprintf( f, " B^{\\mathsf{%d}}", it->a );
                         break;
          case EI_CLOSE: fprintf( f, " ) " ); break;
          default:       break;
          }
       }
   fprintf( f, "}\n" );
}

/* underset{f = k}{(1 - phi B - phi B^2)}: the B^2 term, its standard error
 * and the closing brace were written together in 1.13.1.                    */
static void write_fixed_factors( FILE *f, const Equation *eq, EqPart part, const char *macro,
                                 const char *one )
{
   const EqItem *item = eq->item[part];
   int           i, n = eq->n[part];

   fprintf( f, "\\newcommand{\\%s}{ ", macro );
   for ( i = 0; i < n; i++ )
       {
       const EqItem *it = &item[i];

       if ( it->kind == EI_OPEN )
          {
          fprintf( f, "\\underset{f = \\mathsf{%2d}}{(%s", it->a, one );
          i++;                                            /* the "1"        */
          }
       else if ( it->kind == EI_COEF && !it->has_se && i + 1 < n &&
                 item[i+1].kind == EI_B && item[i+1].a == 1 )
          {
          if ( it->value != 0.0 )
             fprintf( f, "%s\\mathsf{%6.2f} B", ( it->sign == '-' ) ? " - " : " + ",
                      it->value );
          i++;                                            /* its B          */
          }
       else if ( it->kind == EI_COEF )
          {
          if ( it->value != 0.0 )
             fprintf( f, "%s%s\\mathsf{%6.2f}%s", ( it->sign == '-' ) ? "- " : "+ ",
                      it->has_se ? "\\est{" : "", it->value, it->has_se ? "}" : "" );
          if ( it->has_se ) fprintf( f, "{(\\mathsf{%6.2f})}", it->se );
          fprintf( f, "B^\\mathsf{2})}" );
          i += 2;                                         /* its B^2 and )  */
          }
       }
   fprintf( f, "}\n" );
}

/*****************************************************************************/
/* The non-stationary factors                                                */
/*****************************************************************************/

/* The individual factors of the annual difference, one for each frequency */
static void write_ifadf_factor( FILE *f, int i, int freq )
{
   if ( i == 0 ) { fprintf( f, "\\nabla " ); return; }
   if ( freq == 4 )
      fprintf( f, "\\underset{f=\\mathsf{%d}}{(\\mathsf{1} %s)}\n", i,
               ( i == 1 ) ? "+ B^\\mathsf{2}" : "+ B" );
   else if ( i == 6 )
      fprintf( f, "\\underset{f=\\mathsf{6}}{(\\mathsf{1} + B)}\n" );
   else
      {
      static const char *term[6] = { "", "- \\mathsf{\\sqrt{3}}B + B^\\mathsf{2}",
         "- B + B^\\mathsf{2}", "+ B^\\mathsf{2}", "+ B + B^\\mathsf{2}",
         "+ \\mathsf{\\sqrt{3}}B + B^\\mathsf{2}" };
      fprintf( f, "\\underset{f=\\mathsf{%d}}{(\\mathsf{1} %s)}\n", i, term[i] );
      }
}

/*****************************************************************************/

void eq_write_latex( FILE *f, const Equation *eq, const char *x11out )
{
   int i;

/* [11.1]: the preamble                                                      */

   fprintf( f, "\\documentclass[12pt,a4paper,landscape]{article}\n" );
   fprintf( f, "\\usepackage[latin1]{inputenc}\n" );
   fprintf( f, "\\usepackage{amsmath}\n" );
   fprintf( f, "\\usepackage{amsfonts}\n" );
   fprintf( f, "\\usepackage{amssymb}\n" );
   fprintf( f, "\\usepackage{graphicx}\n" );
   fprintf( f, "\\usepackage{lscape}\n" );
   fprintf( f, "\\usepackage{multicol}\n" );
   fprintf( f, "\\usepackage{setspace}\n" );
   fprintf( f, "\\usepackage[margin=2.5cm]{geometry}\n" );
   fprintf( f, "\\newcommand{\\est}[2]{\\begin{array}[t]{c}\n" );
   fprintf( f, "\\hspace{-.125in}#1\\hspace{-.125in}\\vspace{-.13in}\\\\ \n" );
   fprintf( f, "\\hspace{-.125in}#2\\hspace{-.125in}\n" );
   fprintf( f, "\\end{array}}\n" );
   fprintf( f, "\\linespread{1.6}\n" );

/* [11.2] - [11.9]: one macro for each part of the equation                  */

   if ( eq->used[EQ_DETER] ) write_deter( f, eq );
   if ( eq->used[EQ_ARR] ) write_factors( f, eq, EQ_ARR, "arr", " (\\mathsf{1}  " );
   if ( eq->used[EQ_ARA] ) write_factors( f, eq, EQ_ARA, "ara", "(1  " );
   if ( eq->used[EQ_MAR] ) write_factors( f, eq, EQ_MAR, "mar", "(\\mathsf{1}  " );
   if ( eq->used[EQ_MAA] ) write_factors( f, eq, EQ_MAA, "maa", "(\\mathsf{1}  " );
   if ( eq->used[EQ_ARF] ) write_fixed_factors( f, eq, EQ_ARF, "arf", "1" );
   if ( eq->used[EQ_MAF] ) write_fixed_factors( f, eq, EQ_MAF, "maf", "\\mathsf{1}" );

   if ( eq->used[EQ_MU] )
      {
      const EqItem *it = &eq->item[EQ_MU][0];

      fprintf( f, "\\newcommand{\\ifmu}{" );
      write_coef( f, it );                     /* its \n is in the s.e.      */
      }

/* [10.6]: the non-stationary factors                                        */

   if ( eq->used[EQ_NRDIFF] )
      {
      int d = eq->item[EQ_NRDIFF][0].a;

      if ( d == 1 ) fprintf( f, "\\newcommand{\\nrdiff}{\\nabla} \n" );
      else fprintf( f, "\\newcommand{\\nrdiff}{\\nabla^{\\mathsf{%2d}}}\n", d );
      }
   if ( eq->used[EQ_NADIFF] )
      fprintf( f, "\\newcommand{\\nadiff}{\\nabla_{\\mathsf{%2d}}}\n",
               eq->item[EQ_NADIFF][0].b );
   if ( eq->used[EQ_IFADF] )
      {
      fprintf( f, "\\newcommand{\\ifadf}{ " );
      for ( i = 0; i < eq->n[EQ_IFADF]; i++ )
          write_ifadf_factor( f, ( eq->item[EQ_IFADF][i].kind == EI_NABLA ) ? 0
                                 : eq->item[EQ_IFADF][i].a, eq->freq );
      fprintf( f, "}\n" );
      }

/* [11]: the document: the graph, the equation and the table of residuals    */

   fprintf( f, "\\begin{document}\n" );
   fprintf( f, "\\begin{flushleft}\n" );
   fprintf( f, "\\thispagestyle{empty}\n" );
   if ( eq->freq == 1 && eq->nobs > 200 )
      fprintf( f, "\\includegraphics[scale=.90]{A%s.eps}\n", x11out );
   else
      fprintf( f, "\\includegraphics[scale=1]{A%s.eps}\n", x11out );
   fprintf( f, "\\begin{footnotesize}\n" );

   if ( eq->used[EQ_DETER] ) fprintf( f, "$\\deter$ \\\\ \n" );
   fprintf( f, " $ " );
   if ( eq->used[EQ_ARR] ) fprintf( f, "\\arr \n" );
   if ( eq->used[EQ_ARA] ) fprintf( f, "\\ara \n" );
   if ( eq->used[EQ_ARF] ) fprintf( f, "\\arf \n" );
   if ( eq->used[EQ_MU] )  fprintf( f, "\\left[ \n" );
   if ( eq->used[EQ_NRDIFF] ) fprintf( f, "\\nrdiff \n" );
   if ( eq->used[EQ_NADIFF] ) fprintf( f, "\\nadiff \n" );
   if ( eq->used[EQ_IFADF] )  fprintf( f, "\\ifadf " );
   if ( eq->used[EQ_DETER] )
      fprintf( f, "\\mathsf{N_t} \n" );
   else if ( eq->is_log )
      fprintf( f, "\\ln \\mathsf{%s}_{t}", eq->name );
   else
      fprintf( f, "\\mathsf{%s}_{t}", eq->name );
   if ( eq->used[EQ_MU] ) fprintf( f, "\\ifmu \\right] \n" );
   fprintf( f, " = " );
   if ( eq->used[EQ_MAR] ) fprintf( f, "\\mar \n" );
   if ( eq->used[EQ_MAA] ) fprintf( f, "\\maa \n" );
   if ( eq->used[EQ_MAF] ) fprintf( f, "\\maf \n" );
   fprintf( f, "\\mathsf{%s_{t}} ; \\quad \n", eq->residuals );
   fprintf( f, "\\hat{\\sigma}_{\\mathsf{%s}}=  \\mathsf{ %2.2f } \\%%  $ \n",
            eq->residuals, eq->sigma );
   fprintf( f, "\\input {%s_res}\n", x11out );
   fprintf( f, "\\end{footnotesize}\n" );
   fprintf( f, "\\end{flushleft}\n" );
   fprintf( f, "\\end{document}\n" );
}
