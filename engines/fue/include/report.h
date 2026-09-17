/*****************************************************************************/
/* report.h -- the report of FUE (graph, equation and outlying residuals)    */
/* drawn with fugdraw, without LaTeX.                                        */
/* Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 or later.        */
/*****************************************************************************/

#ifndef REPORT_H
#define REPORT_H

#include "fugdraw.h"
#include "equation.h"
#include "plothost.h"

/* Write <filename> (A4, landscape): the graph of the residuals, the equation
 * of the model and the table of the residuals over three standard
 * deviations. graph is the figure of fp_PlotSer_CorrSer(); res, the
 * residuals. Returns 0 if the file was written.                            */
int report_write_pdf( const char *filename, FDFig *graph, const Equation *eq,
                      struct Tseries *res );

#endif
