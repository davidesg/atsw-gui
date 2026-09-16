/* utils.h */
#ifndef UTILS_H
#define UTILS_H

#include <glib.h>
#include <gtk/gtk.h>

const char *getExt(const char *fspec);
int default_lags(int nobs, int freq);
int default_nog(int freq);
char *sanitize_to_utf8(const char *input);
/* The name as a single token: fue and fuf read it with %s, so a space in it
 * would cut the line of the .inp in two. Letters, digits and _ - . survive;
 * everything else is dropped. Returns a new string, never NULL.           */
char *token_name(const char *input);

/* Un .inp que el motor no podria leer: se dice por que, en una ventana, y
 * se devuelve FALSE. forecast != 0 para los ficheros de fuf.              */
gboolean inp_ok_to_load(GtkWidget *parent, const char *path, int forecast);
void reload_number_int(GtkTreeModel *model, GtkTreeIter iter);
/* possibly other string functions */

#endif
