/***************************************************************************
 *   port.h -- portability helpers for FUG (Linux, macOS, Windows)          *
 *   Copyright (C) 2026 by David E. Guerrero & Arthur B. Treadway          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef FUG_PORT_H
#define FUG_PORT_H

#include <stdio.h>

/* Change the working directory; returns 0 on success. */
int fug_chdir( const char *dir );

/* Newly allocated copy of a string (strdup is not ISO C). */
char *fug_strdup( const char *s );

#endif
