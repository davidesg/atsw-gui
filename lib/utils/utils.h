/* utils.h */
#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <glib.h>
#include <gtk/gtk.h>

const char *getExt(const char *fspec);
/* default_lags se quito: la de lib/fugplot/plotsupport.c es la del motor
   y ademas acota. Ver utils.c.                                      */
int default_nog(int freq);
char *sanitize_to_utf8(const char *input);
/* The name as a single token: fue and fuf read it with %s, so a space in it
 * would cut the line of the .inp in two. Letters, digits and _ - . survive;
 * everything else is dropped. Returns a new string, never NULL.           */
char *token_name(const char *input);

/* Un numero escrito de modo que vuelva a leerse EXACTAMENTE igual: prueba de
 * 6 decimales en adelante y se queda con el primero que cumple. Es la misma
 * funcion que fug tiene en src/inpfile.c; el GUI escribia la serie con "%lf"
 * -- seis decimales -- y le quitaba cifras a los datos del usuario.        */
char *inp_format(char *buf, size_t size, double v);

/* El nombre tal como va en el .inp: los motores lo leen con %s, asi que lo
 * unico que no puede llevar es un espacio. Un nombre como "PE/PU" es
 * legitimo y hay que respetarlo -- token_name() es para el nombre de
 * FICHERO, que es otra cosa.                                              */
char *single_token(const char *input);

/* Un .inp que el motor no podria leer: se dice por que, en una ventana, y
 * se devuelve FALSE. forecast != 0 para los ficheros de fuf.              */
/* 1 si el fichero cabe en los vectores estaticos del GUI, 0 con el motivo en
 * msg[size]. Es un limite del GUI, no del formato: el motor lee ficheros mas
 * grandes. inp_ok_to_load() lo llama despues de inpcheck.                  */
int inp_fits_gui( const char *path, char *msg, size_t size );

gboolean inp_ok_to_load(GtkWidget *parent, const char *path, int forecast);
void reload_number_int(GtkTreeModel *model, GtkTreeIter iter);
/* possibly other string functions */

#endif
