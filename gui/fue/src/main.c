/* main.c – entry point for FUE GUI */
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "fue_context.h"
#include "main_window.h"
#include "fue_globals.h"       /* provides global structures */
#include "data_handling.h"  /* for load_data_file, etc. */
#include "proyecto.h"
#include "model_spec.h"
#include "deterministic_dialog.h"
#include "operator_dialog.h"
#include "file_io.h"


#ifdef _WIN32
#include <windows.h>
#endif

static void init_global_flags(FueContext *ctx) {
    /* Pass the flags from the context to the global variables */
    ma_order_inc = ctx->ma_order_inc;
    ar_order_inc = ctx->ar_order_inc;
    new_det = ctx->new_det;
    type_op = ctx->type_op;
    new_op = ctx->new_op;

}


/* Definidas mas abajo: activate() las usa. */
const char *fue_abrir(void);

static void activate(GtkApplication *app, gpointer user_data) {
    /* UNA VENTANA POR PROCESO, y el guardia es del PROCESO, no del contexto:
     * el contexto se crea aqui, asi que mirarlo a el no dice nada.
     *
     * La bandera NON_UNIQUE lo garantiza hoy, pero la razon de fondo no es
     * la bandera: EL MODELO ES ESTADO GLOBAL DEL PROCESO -- Ts, Tm, It[50],
     * Arr[20]... en model_globals.c. Dos ventanas aqui dentro compartirian
     * un solo modelo y se pisarian los operadores, que es peor que no
     * abrirse.                                                          */
    static GtkWidget *abierta = NULL;
    FueContext *ctx;

    if (abierta) { gtk_window_present(GTK_WINDOW(abierta)); return; }

    ctx = g_new0(FueContext, 1);

    /* Initialize global flags from context (they start at 0) */
    init_global_flags(ctx);

    /* Build the main window */
    ctx->main_window = create_main_window(app, ctx);
    abierta = ctx->main_window;
    gtk_widget_show_all(ctx->main_window);

    /* Lo que la madre mando. Se hace DESPUES de mostrar la ventana para que,
     * si el fichero tiene algo raro, el aviso salga sobre una ventana que ya
     * esta ahi y no sobre el vacio.                                     */
    if (fue_abrir())
        fue_abre_al_arrancar(ctx, fue_abrir());

    /* Set up status label and text view (already done in create_main_window) */

    /* Cleanup on window close */
    g_signal_connect(ctx->main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
}

/* --proyecto FICHERO: el espacio de trabajo sale de la RAIZ del proyecto.
 *
 * Es lo minimo que fue_gui necesita de la interfaz madre y lo que de verdad
 * le falta hoy: NO GUARDA NADA entre ejecuciones --ni sesion, ni preferencias,
 * ni recientes-- asi que cada arranque empieza preguntando donde esta todo.
 * Con esto arranca sabiendo en que proyecto esta.
 *
 * Sin la opcion funciona como siempre. La madre todavia no existe.     */
/* Lo que la linea de ordenes deja dicho vive en arranque.c, NO AQUI: la
 * ventana lo pregunta, y una prueba que levanta la ventana sin main() se
 * quedaba sin ello. Un dato que la interfaz consulta no puede vivir en el
 * unico fichero que las pruebas no pueden enlazar.                     */
void fue_pon_raiz_proyecto(const char *s);
void fue_pon_abrir(const char *s);

static int lee_opciones(int argc, char *argv[])
{
    const char *proy = NULL;
    Proyecto   *p;
    PrError     e;
    int         i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--proyecto") && i + 1 < argc) proy = argv[++i];
        else if (!strncmp(argv[i], "--proyecto=", 11)) proy = argv[i] + 11;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("uso: %s [--proyecto FICHERO] [FICHERO.inp]\n\n"
                   "  --proyecto F  el espacio de trabajo sale de la raiz\n"
                   "                del proyecto F.\n"
                   "  FICHERO.inp   se abre al arrancar. Es lo que la madre\n"
                   "                manda al decir «estimar esta serie».\n",
                   argv[0]);
            return 1;
        }
        else if (argv[i][0] != '-')
            fue_pon_abrir(argv[i]);
    }
    if (!proy) return 0;

    p = calloc(1, sizeof *p);
    if (!p) return 0;
    if (pr_leer(proy, p, &e) != 0) {
        char why[512];

        /* Un manifiesto roto se dice y se para: arrancar ignorandolo es
           arrancar creyendo que se esta en un sitio y estar en otro.   */
        pr_error_es(&e, why, sizeof why);
        fprintf(stderr, "%s: %s\n", proy, why);
        free(p);
        return 2;
    }
    /* La raiz, resuelta: pr_ruta con id vacio da el directorio.       */
    {
    char raiz[1024] = "";

    pr_ruta(p, "", "", NULL, NULL, raiz, sizeof raiz);
    if (!raiz[0]) {
        char *d = g_path_get_dirname(proy);

        snprintf(raiz, sizeof raiz, "%s", d);
        g_free(d);
    }
    fue_pon_raiz_proyecto(raiz);
    }
    free(p);
    return 0;
}

int main(int argc, char *argv[]) {
    int opt;

    setlocale(LC_ALL, "C");

    opt = lee_opciones(argc, argv);
    if (opt == 1) return 0;
    if (opt == 2) return 3;

    /* Las opciones ya estan leidas: al GTK solo le llega el nombre. Si no,
       g_application_run se las encuentra y dice "Unknown option".      */
    argc = 1;

#ifdef _WIN32
    /* On Windows, set up resource paths for GTK (if needed) */
    wchar_t wpath[MAX_PATH];
    GetModuleFileNameW(NULL, wpath, MAX_PATH);
    char *exe_path = g_utf16_to_utf8(wpath, -1, NULL, NULL, NULL);
    char *exe_dir = g_path_get_dirname(exe_path);
    g_free(exe_path);

    char *data_dir = g_build_filename(exe_dir, "share", NULL);
    char *pixbuf_cache = g_build_filename(exe_dir, "lib", "gdk-pixbuf-2.0", "2.10.0", "loaders.cache", NULL);
    char *schema_dir = g_build_filename(exe_dir, "share", "glib-2.0", "schemas", NULL);

    g_setenv("XDG_DATA_DIRS", data_dir, TRUE);
    g_setenv("GDK_PIXBUF_MODULE_FILE", pixbuf_cache, TRUE);
    g_setenv("GSETTINGS_SCHEMA_DIR", schema_dir, TRUE);
    g_setenv("GTK_THEME", "Windows", TRUE);

    g_free(data_dir);
    g_free(pixbuf_cache);
    g_free(schema_dir);
    g_free(exe_dir);
#endif

    /* NON_UNIQUE: UN PROCESO POR VENTANA, y no es un detalle de arranque.
     *
     * Con la bandera por defecto, GtkApplication es de INSTANCIA UNICA: el
     * segundo "fue_gui ... m02.inp" se encuentra al primero por D-Bus, le
     * manda "activate" y SE MUERE -- con su m02 dentro, que nunca cruza la
     * frontera del proceso porque las opciones se leyeron aqui. El primero
     * abre otra ventana con lo suyo, que era m01. Eso es lo que se veia.
     *
     * Y habria sido peor que ver el fichero equivocado: EL MODELO ES ESTADO
     * GLOBAL DEL PROCESO -- Ts, Tm, It[50], Arr[20]... viven en
     * model_globals.c -- asi que dos ventanas en un proceso comparten un
     * solo modelo y se pisan los operadores. Un proceso por ventana las
     * separa de verdad.
     *
     * Es ademas lo que ya hacia gui/drtran; fue_gui era el raro.        */
    GtkApplication *app = gtk_application_new("org.fue.gui",
                                              G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
