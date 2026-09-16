/***************************************************************************
 *   fugdraw.h -- vector graphics for FUG: EPS and PDF without external     *
 *   programs. Copyright (C) 2026 by David E. Guerrero & Arthur B. Treadway *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

/* A figure is recorded once as a PDF content stream (units: points, origin
 * at the lower left corner). The same stream is written as an EPS file (the
 * EPS prolog defines the PDF operators in PostScript) and placed on the
 * pages of a PDF document, so both outputs are identical.
 * Only the standard PDF/PostScript fonts are used: nothing is embedded and
 * text widths come from the Adobe font metrics (fd_metrics.h).             */

#ifndef FUGDRAW_H
#define FUGDRAW_H

#include <stddef.h>

/* Fonts */
enum { FD_HELV, FD_HELV_BOLD, FD_TIMES, FD_TIMES_ITALIC, FD_SYMBOL };

/* Text alignment */
enum { FD_LEFT, FD_CENTER, FD_RIGHT };

/* Accents of a text run */
enum { FD_ACC_NONE, FD_ACC_BAR, FD_ACC_HAT };

/* Symbol font codes */
#define FD_SYM_NABLA "\321"
#define FD_SYM_SIGMA "s"

typedef struct {
   double w, h;              /* size (points)                  */
   char  *buf;               /* content stream                 */
   size_t len, cap;
   double lw, dash_on, dash_off, gray;   /* current state      */
   int    state_ok;
} FDFig;

/* A piece of text in one font; rise > 0 raises it (superscript). */
typedef struct {
   int         font;
   double      size;
   double      rise;
   const char *text;
   int         accent;
} FDRun;

FDFig *fd_fig_new( double w, double h );
void   fd_fig_free( FDFig *f );

void fd_linewidth( FDFig *f, double w );
void fd_dash( FDFig *f, double on, double off );      /* on <= 0: solid */
void fd_gray( FDFig *f, double g );                   /* 0 black, 1 white */
void fd_line( FDFig *f, double x1, double y1, double x2, double y2 );
void fd_polyline( FDFig *f, const double *x, const double *y, int n );
void fd_disc( FDFig *f, double x, double y, double r );
void fd_clip_begin( FDFig *f, double x, double y, double w, double h );
void fd_clip_end( FDFig *f );

double fd_text_width( int font, double size, const char *s );
double fd_font_xheight( int font, double size );
void   fd_text( FDFig *f, double x, double y, int font, double size, int align, const char *s );
/* Text rotated 90 degrees counterclockwise (vertical axis labels); align
 * is along the text, (x, y) is the base line point. */
void   fd_text_up( FDFig *f, double x, double y, int font, double size, int align, const char *s );
double fd_runs_width( const FDRun *r, int n );
void   fd_runs( FDFig *f, double x, double y, int align, const FDRun *r, int n );

/* Write the figure as an EPS file; returns 0 on success. */
int fd_write_eps( const FDFig *f, const char *filename );

/* PDF document made of pages with figures placed on them. */
typedef struct FDPdf FDPdf;

FDPdf *fd_pdf_open( const char *filename );
/* New page of size pw x ph; figure i is drawn at (x[i], y[i]) scaled by s[i]. */
void   fd_pdf_page( FDPdf *pdf, double pw, double ph, FDFig **figs,
                    const double *x, const double *y, const double *s, int n );
int    fd_pdf_close( FDPdf *pdf );              /* returns 0 on success */

#endif
