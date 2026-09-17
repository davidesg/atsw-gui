/***************************************************************************
 *   port.c -- portability helpers for FUG (Linux, macOS, Windows)          *
 *   Copyright (C) 2026 by David E. Guerrero & Arthur B. Treadway          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

/* POSIX chdir() with -std=c99 */
#if !defined(_WIN32) && !defined(_XOPEN_SOURCE)
#  define _XOPEN_SOURCE 700
#endif

#include <stdlib.h>
#include <string.h>
#include "port.h"

#ifdef _WIN32
#  include <direct.h>
#else
#  include <unistd.h>
#endif

char *fug_strdup( const char *s )
{
   char *d = (char *)malloc( strlen( s ) + 1 );

   if ( d != NULL ) strcpy( d, s );
   return( d );
}

int fug_chdir( const char *dir )
{
#ifdef _WIN32
   return( _chdir( dir ) );
#else
   return( chdir( dir ) );
#endif
}
