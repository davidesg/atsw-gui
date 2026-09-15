/*****************************************************************************/
/* inpcheck.h -- validation of the .inp file of FUE before it is read.       */
/* Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 or later.        */
/*****************************************************************************/

#ifndef INPCHECK_H
#define INPCHECK_H

#include <stddef.h>

/* Exit status of fue                                                        */
#define FUE_OK            0   /* results written                             */
#define FUE_ERR_USAGE     1   /* command line, or a file can not be opened   */
#define FUE_ERR_INPUT     2   /* the .inp file is not valid (nothing written)*/
#define FUE_ERR_ESTIMATE  3   /* the model could not be estimated (results   */
                              /* written with the initial values)            */
#define FUE_ERR_INTERNAL  4   /* run-time error (nrerror)                     */

/* Check the .inp file at path: 0 if fue can read it, 1 with a message
 * (with the line number) in msg[size]. It reads the file with the same
 * calls, in the same order, as fue (fue.c, section [3]).                    */
int inp_check( const char *path, char *msg, size_t size );

#endif
