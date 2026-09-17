/***************************************************************************
 *   inpfile.h -- the .inp file (series and model), shared by fug and its  *
 *   GUI. Copyright (C) 2026 by David E. Guerrero & Arthur B. Treadway     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

/* fug and fue share one input file: the .inp of FUE. fug uses its series,
 * its Box-Cox line and its individual factors, and skips the model (read
 * exactly as fue 1.13.1 reads it). The .inp files of older fug versions
 * are still read. fug itself never writes the .inp: its GUI writes a fue
 * .inp without model (inp_write_bare) when there is none.                  */

#ifndef INPFILE_H
#define INPFILE_H

#include <stddef.h>

#define INP_NAME_MAX 80

typedef struct {
   int    fue;              /* 1: .inp of fue, 0: .inp of fug <= 1.14          */
   int    model;            /* fue: the file has a model (deterministic         */
                            /* variables, ARMA operators or mean)               */
   int    freq, nobs;
   int    numbering;        /* fue: "number" instead of the frequency (freq 1)  */
   int    begtime;          /* first season (1 for annual data)                 */
   int    outyear;          /* annual data: the second value of the date line   */
   int    begyear;
   char   name[INP_NAME_MAX + 1];   /* series name ("" if missing)              */
   double boxlam, boxm;     /* m: fug files only (0 in fue files)               */
   int    boxgeom;          /* fug 1.10 - 1.12.01 files only                     */
   int    nrdiff, nadiff;
   int    ifadf[7];         /* individual factors of the annual difference      */
   double cbands, refactor; /* fue: scale of the acf/pacf (0 auto), rescaling   */
   double *data;            /* the series, data[0..nobs-1] (malloc)             */
} InpFile;

/* Read path into inp. Returns 0, or 1 with a message in error[size].       */
int  inp_read( const char *path, InpFile *inp, char *error, size_t size );

/* Write a fue .inp without model: series, Box-Cox line (lambda d D) and
 * individual factors. Values keep at least 6 decimals and are read back
 * exactly. Returns 0 on success.                                           */
int  inp_write_bare( const char *path, const InpFile *inp );

/* Write v with at least 6 decimals and as many more as needed to read back
 * exactly the same number (buf of at least 400 bytes).                    */
char *inp_format( char *buf, size_t size, double v );

void inp_free( InpFile *inp );

#endif
