/*****************************************************************************/
/* report.c -- the report of FUE drawn with fugdraw, without LaTeX.          */
/* Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 or later.        */
/*                                                                           */
/* The page has the same three parts as the one that pdflatex made: the      */
/* graph of the residuals, the equation of the model (include/equation.h)    */
/* and the table of the residuals over three standard deviations.            */
/*                                                                           */
/* The page itself is lib/equation/eqdraw.c, typeset as pdflatex typesets    */
/* the LaTeX file of fue, and it is the same page that drtran_gui writes for */
/* each equation of a case: here only the outliers are found, with the dates */
/* of the series.                                                            */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "report.h"
#include "eqdraw.h"

static void date_of( int i, char *buf, size_t size, void *data )
{
   struct Tseries *res = (struct Tseries *)data;
   int             period, season;

   ObsToDate( res->begyear, res->begtime, i + 1, res->freq, &period, &season );
   if ( res->freq == 1 ) snprintf( buf, size, "%d", period );
   else snprintf( buf, size, "%d/%d", season, period );
}

int report_write_pdf( const char *filename, FDFig *graph, const Equation *eq,
                      struct Tseries *res )
{
   EqOutlier *out;
   int        nout, status;

   out = (EqOutlier *)calloc( res->nobs + 1, sizeof( EqOutlier ) );
   if ( out == NULL ) return( 1 );
   nout = eq_outliers( res->data + 1, res->nobs, res->mean, sqrt( res->var ),
                       ( res->numbering == 0 ) ? date_of : NULL, res, out, res->nobs );
   status = eq_page_pdf( filename, graph, eq, out, nout, res->numbering == 0 );
   free( out );
   return( status );
}
