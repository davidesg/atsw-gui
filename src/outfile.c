/* outfile.c -- lo que se lee del .out que escribe el motor.
 *
 * Aparte para poder probarlo: file_io.c entero habla con GTK, y esto no.
 */

#include <string.h>
#include <glib.h>
#include "outfile.h"

/* Como acabo la estimacion. El optimizador (qnewtopt.c, report()) deja en
 * el .out dos lineas que empiezan por ****: el criterio de parada que se
 * cumplio -- gradiente, parametros, el paso no encontro un punto mejor,
 * limite de iteraciones, o cinco pasos seguidos de longitud maxima -- y en
 * cuantas iteraciones, con la norma del gradiente.                        */
gchar *convergence_of(const char *out_path) {
    gchar  *text = NULL, **lines, *how = NULL, *many = NULL, *result = NULL;
    gsize   len = 0;
    int     i;

    if (out_path == NULL) return NULL;
    if (!g_file_get_contents(out_path, &text, &len, NULL)) return NULL;
    lines = g_strsplit(text, "\n", -1);
    for (i = 0; lines[i] != NULL; i++) {
        if (!g_str_has_prefix(lines[i], "****")) continue;
        if (strstr(lines[i], "CONVERGENCE OBTAINED")) {
            g_free(many);
            many = g_strdup(g_strstrip(lines[i] + 4));
        } else {
            g_free(how);
            how = g_strdup(g_strstrip(lines[i] + 4));
        }
    }
    if (many != NULL && how != NULL) result = g_strdup_printf("%s; %s", many, how);
    else if (many != NULL) result = g_strdup(many);
    else if (how != NULL)  result = g_strdup(how);
    g_free(how);
    g_free(many);
    g_strfreev(lines);
    g_free(text);
    return result;
}
