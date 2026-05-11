/* utils.h */
#ifndef UTILS_H
#define UTILS_H

#include <glib.h>
#include <gtk/gtk.h>

const char *getExt(const char *fspec);
int default_lags(int nobs, int freq);
int default_nog(int freq);
char *sanitize_to_utf8(const char *input);
void reload_number_int(GtkTreeModel *model, GtkTreeIter iter);
/* possibly other string functions */

#endif
