/* utils.c */
#include "utils.h"
#include "inpcheck.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

const char *getExt(const char *fspec) {
    const char *dot = strrchr(fspec, '.');
    if (!dot) return "";
    return dot;
}

int default_lags(int nobs, int freq) {
    if (nobs < 3 * (freq + 1)) return nobs - freq / 2;
    if (freq == 1) return (nobs > 200) ? 45 : 9;
    return 3 * (freq + 1);
}

int default_nog(int freq) {
    if (freq == 12) return 12;
    if (freq == 4) return 8;
    return 8;
}

char *sanitize_to_utf8(const char *input) {
    GString *result = g_string_new(NULL);
    const char *p = input;
    while (*p) {
        if (g_utf8_validate(p, -1, NULL)) {
            gunichar c = g_utf8_get_char(p);
            g_string_append_unichar(result, c);
            p = g_utf8_next_char(p);
        } else {
            g_string_append_c(result, '?');
            p++;
        }
    }
    return g_string_free(result, FALSE);
}

/* The name as a single token: fue and fuf read it with %s, so a space in it
 * would cut the line of the .inp in two and the rest would be read as the
 * next field.                                                             */
char *token_name(const char *input) {
    GString *out = g_string_new(NULL);
    const char *p;

    for (p = input ? input : ""; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == '_' || c == '-' || c == '.' || c >= 0x80)
            g_string_append_c(out, (char)c);
    }
    return g_string_free(out, FALSE);
}

void reload_number_int(GtkTreeModel *model, GtkTreeIter iter) {
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    int row = 1;
    while (valid) {
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, row, -1);
        row++;
        valid = gtk_tree_model_iter_next(model, &iter);
    }
}

/* Un numero escrito de modo que vuelva a leerse EXACTAMENTE igual. Copia de
 * fug src/inpfile.c:inp_format(): el mismo fichero en los dos sitios, como
 * fugdraw.                                                                */
char *inp_format(char *buf, size_t size, double v) {
    int decimals;

    for (decimals = 6; decimals <= 17; decimals++) {
        snprintf(buf, size, "%.*f", decimals, v);
        if (fabs(v) < 1e21 && strtod(buf, NULL) == v) return buf;
    }
    snprintf(buf, size, "%.17g", v);        /* muy grande o muy pequeno     */
    return buf;
}

/* Un .inp que el motor no podria leer: se dice por que y no se carga. El
 * aviso va en una ventana porque no cabe en la barra de estado -- la razon
 * de inpcheck lleva la recomendacion al final, que es lo que hace falta.  */
gboolean inp_ok_to_load(GtkWidget *parent, const char *path, int forecast) {
    char why[512];
    GtkWidget *dialog;
    gchar *base;

    if ((forecast ? inp_check_fuf(path, why, sizeof(why))
                  : inp_check_fue(path, why, sizeof(why))) == 0)
        return TRUE;

    base = g_path_get_basename(path);
    dialog = gtk_message_dialog_new(parent ? GTK_WINDOW(parent) : NULL,
                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                    "%s was not loaded", base);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", why);
    gtk_window_set_title(GTK_WINDOW(dialog),
                         forecast ? "Not a forecast input file" : "Not a model input file");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free(base);
    return FALSE;
}
