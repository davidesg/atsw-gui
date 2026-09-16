/* outfile.c -- lo que se lee del .out que escribe el motor.
 *
 * Aparte para poder probarlo: file_io.c entero habla con GTK, y esto no.
 */

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include "outfile.h"

static const struct { const char *mark; ConvKind kind; const char *brief; } kinds[] = {
    { "GRADIENT STOPPING",             CONV_GRADTOL,  "converged (gradtol)"          },
    { "PARAMETER STOPPING",            CONV_STEPTOL,  "converged (steptol)"          },
    { "FAILED TO LOCATE A LOWER POINT",CONV_NO_LOWER, "stopped: no lower point"      },
    { "ITERATION LIMIT",               CONV_MAXITS,   "NOT converged: iteration limit" },
    { "MAX-LENGTH",                    CONV_MAXSTEPS, "stopped: five max-length steps" },
};

gboolean convergence_of(const char *out_path, Convergence *c) {
    gchar  *text = NULL, **lines, *how = NULL, *many = NULL;
    gsize   len = 0;
    int     i, k;

    if (c == NULL) return FALSE;
    c->kind = CONV_NONE;
    c->iterations = -1;
    c->gradient = -1.0;
    c->brief = c->full = NULL;
    if (out_path == NULL) return FALSE;
    if (!g_file_get_contents(out_path, &text, &len, NULL)) return FALSE;

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
    g_strfreev(lines);
    g_free(text);

    if (how != NULL)
        for (k = 0; k < (int)(sizeof(kinds) / sizeof(kinds[0])); k++)
            if (strstr(how, kinds[k].mark) != NULL) {
                c->kind  = kinds[k].kind;
                c->brief = g_strdup(kinds[k].brief);
                break;
            }
    if (many != NULL) {
        const char *p = strstr(many, "AFTER ");
        const char *g = strstr(many, "NORM = ");

        if (p != NULL) c->iterations = atoi(p + 6);
        if (g != NULL) c->gradient   = g_ascii_strtod(g + 7, NULL);
    }
    if (c->brief == NULL && c->iterations >= 0)
        c->brief = g_strdup("finished");
    if (how != NULL && many != NULL) c->full = g_strdup_printf("%s\n%s", how, many);
    else if (many != NULL) c->full = g_strdup(many);
    else if (how != NULL)  c->full = g_strdup(how);
    g_free(how);
    g_free(many);
    return c->brief != NULL || c->full != NULL;
}

void convergence_clear(Convergence *c) {
    if (c == NULL) return;
    g_free(c->brief);
    g_free(c->full);
    c->brief = c->full = NULL;
}

gboolean convergence_is_good(const Convergence *c) {
    return c != NULL && (c->kind == CONV_GRADTOL || c->kind == CONV_STEPTOL);
}
