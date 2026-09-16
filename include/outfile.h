/* outfile.h -- lo que se lee del .out que escribe el motor. */

#ifndef OUTFILE_H
#define OUTFILE_H

#include <glib.h>

/* Como acabo la estimacion, de las lineas **** que el optimizador deja en
 * el .out: en cuantas iteraciones, con que norma del gradiente y por que
 * criterio de parada. NULL si el fichero no las tiene. Se libera con
 * g_free().                                                              */
gchar *convergence_of(const char *out_path);

#endif
