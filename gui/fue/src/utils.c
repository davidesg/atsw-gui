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

/* El nombre tal como va en el .inp: sin espacios, y lo demas tal cual. */
char *single_token(const char *input) {
    GString *out = g_string_new(NULL);
    const char *p;

    for (p = input ? input : ""; *p; p++)
        if (!g_ascii_isspace((unsigned char) *p))
            g_string_append_c(out, *p);
    return g_string_free(out, FALSE);
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
/* Lo que CABE en el GUI, que es menos de lo que admite el motor.
 *
 * inpcheck dice lo que fue puede leer: hasta 1000 deterministas y hasta diez
 * millones de observaciones. Pero el GUI los guarda en vectores estaticos y
 * CONSECUTIVOS --It[50], Arr[20], ... Data[2000] en model_globals.c-- y los
 * llena sin comprobar, asi que un fichero que el motor lee tan feliz escribe
 * encima de los punteros del vecino, que luego se desreferencian y se
 * liberan.
 *
 * El limite es del GUI, no del formato. Por eso se comprueba aqui, en su
 * propia puerta, y NO en la copia de inpcheck: esa tiene que seguir siendo
 * identica a la del motor, que es lo unico que garantiza que el GUI acepte
 * exactamente lo que el motor acepta.                                     */
#define GUI_MAX_DET   50          /* It[50]  en model_globals.c            */
#define GUI_MAX_NOBS  2000        /* Data[2000]                            */

int inp_fits_gui( const char *path, char *why, size_t size )
{
    FILE *f;
    char  s[512];
    int   i, nobs, ndet;

    if ( ( f = fopen( path, "r" ) ) == NULL ) return 1;   /* ya lo dira otro */

    /* Cinco de banner, etiqueta y valor de la frecuencia, y la etiqueta de la
     * fecha: el fichero ya ha pasado por inpcheck, asi que la cuenta cuadra. */
    for ( i = 0; i < 8; i++ )
        if ( fgets( s, sizeof s, f ) == NULL ) { fclose( f ); return 1; }

    if ( fscanf( f, "%d", &nobs ) == 1 && nobs > GUI_MAX_NOBS ) {
        fclose( f );
        g_snprintf( why, size,
                    "%d observations, and this interface holds %d. The engine "
                    "reads the file: run fue on it from the command line.",
                    nobs, GUI_MAX_NOBS );
        return 0;
    }
    if ( fgets( s, sizeof s, f ) == NULL ) { fclose( f ); return 1; }  /* resto */
    if ( fgets( s, sizeof s, f ) == NULL ) { fclose( f ); return 1; }  /* etiqueta */
    if ( fscanf( f, "%d", &ndet ) == 1 && ndet > GUI_MAX_DET ) {
        fclose( f );
        g_snprintf( why, size,
                    "%d deterministic variables, and this interface holds %d. "
                    "The engine reads the file: run fue on it from the command line.",
                    ndet, GUI_MAX_DET );
        return 0;
    }
    fclose( f );
    return 1;
}

gboolean inp_ok_to_load(GtkWidget *parent, const char *path, int forecast) {
    char why[512];
    GtkWidget *dialog;
    gchar *base;

    if ((forecast ? inp_check_fuf(path, why, sizeof(why))
                  : inp_check_fue(path, why, sizeof(why))) == 0
        && inp_fits_gui(path, why, sizeof(why)))
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
