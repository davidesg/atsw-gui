/*****************************************************************************/
/* inpcheck.h -- validation of an .inp file before the GUI reads it.         */
/* Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 or later.        */
/*                                                                           */
/* src/inpcheck_fue.c and src/inpcheck_fuf.c are the inpcheck.c of fue 1.14  */
/* and of fuf 1.09, copied as they are: the only change is the name of the   */
/* function, which there is inp_check in both and here has to tell them      */
/* apart. They read the file with the same calls, and in the same order, as  */
/* the engine does, so what they accept is what the engine accepts.          */
/*                                                                           */
/* The GUI has its own reader (load_input_fue), which trusted the file: a    */
/* forecast file given to it read the horizon as the number of              */
/* deterministic variables and went on until the heap was gone. It asks      */
/* here first now.                                                          */
/*****************************************************************************/

#ifndef INPCHECK_H
#define INPCHECK_H

#include <stddef.h>

/* 0 if the engine can read the file at path; 1 with the reason, and the
 * line number, in msg[size]. */
int inp_check_fue( const char *path, char *msg, size_t size );   /* a model    */
int inp_check_fuf( const char *path, char *msg, size_t size );   /* a forecast */

/* Lo que usa el GUI es inp_ok_to_load() (utils.h), que llama a una de las
 * dos y, si el fichero no vale, lo dice en una ventana y no lo carga.     */

#endif
