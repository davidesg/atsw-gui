/* utils.c */
#include "utils.h"
#include <string.h>
#include <ctype.h>

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

void reload_number_int(GtkTreeModel *model, GtkTreeIter iter) {
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    int row = 1;
    while (valid) {
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, row, -1);
        row++;
        valid = gtk_tree_model_iter_next(model, &iter);
    }
}
