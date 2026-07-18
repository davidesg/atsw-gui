/***************************************************************************
 *   gnuplot_i — C interface to gnuplot                                    *
 *   Based on the gnuplot_i library, licensed under the GNU GPL.           *
 *   Modified for FUF: Windows/MSYS2 support, time series plot functions.  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/**
  @file     gnuplot_i.h
  @brief    C header file to gnuplot interface.
*/

#ifndef _GNUPLOT_PIPES_H_
#define _GNUPLOT_PIPES_H_

/*---------------------------------------------------------------------------
  Includes
 ---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <time.h>

/** Maximum number of simultaneous temporary files */
#define GP_MAX_TMP_FILES 64
/** Maximum amount of characters of a temporary file name */
#define GP_TMP_NAME_SIZE 512

/*---------------------------------------------------------------------------
  Gnuplot_i types
 ---------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/**
  @typedef  gnuplot_ctrl
  @brief    gnuplot session handle.
  @param    gnucmd
  @param    nplots
  @param    pstyle
  @param    term
  @param    to_delete
  @param    ntmp

  This structure holds all necessary information to talk to a gnuplot session.
  It is called and returned by gnuplot_init() and later used by all functions
  in this module to communicate with the session, then meant to be closed by
  gnuplot_close().
*/

typedef struct _GNUPLOT_CTRL_ {
  FILE *gnucmd;       /*!< Pipe to gnuplot process. */
  int nplots;         /*!< Number of currently active plots. */
  char pstyle[32];    /*!< Current plotting style. */
  char term[32];      /*!< Save terminal name, used by `gnuplot_hardcopy()` function. */
  char to_delete[GP_MAX_TMP_FILES][GP_TMP_NAME_SIZE];   /*!< Names of temporary files. */
  int ntmp;           /*!< Number of temporary files in the current session. */
} gnuplot_ctrl;

/*-------------------------------------------------------------------------*/
/**
  @typedef  gnuplot_point
  @brief    gnuplot point structure, ie set of [x,y,z] coordinates.
  @param    x
  @param    y
  @param    z

  gnuplot_point is a point struct to allow the return of points to the
  `gnuplot_plot_obj_xy` function by callback functions.
*/

typedef struct _GNUPLOT_POINT_ {
  double x; /*!< X-coordinate */
  double y; /*!< Y-coordinate */
  double z; /*!< Z-coordinate */
} gnuplot_point;

/*---------------------------------------------------------------------------
  Function ANSI C prototypes
 ---------------------------------------------------------------------------*/

/* Auxiliary functions */

char *gnuplot_get_program_path (char *pname);
void print_gnuplot_handle (gnuplot_ctrl *handle);

/* Gnuplot interface handling functions */

gnuplot_ctrl *gnuplot_init (void);
void gnuplot_close (gnuplot_ctrl *handle);
void gnuplot_cmd (gnuplot_ctrl *handle, char *cmd, ...);
void gnuplot_setstyle (gnuplot_ctrl *handle, char *plot_style);
void gnuplot_setterm (gnuplot_ctrl *handle, char *terminal, int width, int height);
void gnuplot_set_axislabel (gnuplot_ctrl *handle, char *axis, char *label);
void gnuplot_resetplot (gnuplot_ctrl *handle);

/* Gnuplot interface plotting functions */

void gnuplot_plot_coordinates (gnuplot_ctrl *handle, double *x, double *y, int n, char *title);
void gnuplot_splot (gnuplot_ctrl *handle, double *x, double *y, double *z, int n, char *title);
void gnuplot_splot_grid (gnuplot_ctrl *handle, double *points, int rows, int cols, char *title);
void gnuplot_contour_plot (gnuplot_ctrl *handle, double *x, double *y, double *z, int nx, int ny, char *title);
void gnuplot_splot_obj (gnuplot_ctrl *handle, void *obj, void (*getPoint)(void *, gnuplot_point *, int, int), int n, char *title);
void gnuplot_plot_obj_xy (gnuplot_ctrl *handle, void *obj, void (*getPoint)(void *, gnuplot_point *, int, int), int n, char *title);
void gnuplot_plot_once (char *style, char *label_x, char *label_y, double *x, double *y, int n, char *title);
void gnuplot_plot_equation (gnuplot_ctrl *handle, char *equation, char *title);
void gnuplot_hardcopy (gnuplot_ctrl *handle, char *filename, char *color);

/* New for AST libraries */

void gnuplot_plot_acf(gnuplot_ctrl * handle, double * d, int n, int b, double cmax,  char * title);
void gnuplot_plot_ser(gnuplot_ctrl * handle, double * d, int n, int tmornsop, double AbsMax, char * title);
void gnuplot_plot_histo (gnuplot_ctrl * handle, double * x, double * y, int n, char * title);
void gnuplot_plot_mdt(gnuplot_ctrl    *   handle, double * x, double * y, int  n, char * title, double  AbsMax ) ;
void gnuplot_plot_err(gnuplot_ctrl * handle, double * d, int n, double sigma, double cmax,  char * title);
void gnuplot_plot_df(gnuplot_ctrl * handle, double * d, double *d1, double *d2, int n, int L, char * title);
#endif
