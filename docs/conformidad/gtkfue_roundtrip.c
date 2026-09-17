/* gtkfue_roundtrip.c -- lee un .inp con el lector de gtk_fue y lo vuelve a
 * escribir con su escritor. Es el camino exacto que recorre un fichero cuando
 * el usuario lo abre en el GUI y le da a Guardar (o a Run, que guarda antes).
 *
 *     gtkfue_roundtrip <entrada.inp> <directorio> <nombre>
 *
 * deja <directorio>/<nombre>.inp. Se compara con comparar.py, que lee los dos
 * con fue.load() y mira el MODELO, no los bytes.
 *
 * Levanta la ventana pero no la ensena: el escritor lee del contexto, y el
 * contexto se llena al cargar y al sincronizar desde la interfaz, asi que sin
 * widgets no hay round trip que valga.
 */

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

#include "fue_context.h"
#include "main_window.h"
#include "fue_globals.h"
#include "data_handling.h"
#include "model_spec.h"
#include "file_io.h"
#include "inpcheck.h"

int main(int argc, char **argv) {
    FueContext *ctx;
    GtkApplication *app;

    if (argc < 2 || (argc < 4 && strcmp(argv[1], "--cabecera"))) {
        fprintf(stderr, "uso: gtkfue_roundtrip <entrada.inp> <directorio> <nombre>\n"
                        "     gtkfue_roundtrip --cabecera <fichero>\n");
        return 2;
    }
    if (!gtk_init_check(&argc, &argv)) {
        fprintf(stderr, "no hay servidor grafico\n");
        return 3;
    }

    /* Modo cabecera: no reescribe nada, solo dice lo que el lector en C
     * ENTIENDE de la linea de fecha. Sirve para contrastarlo con lo que
     * entiende el lector de Python del mismo fichero -- que es una promesa
     * distinta de que el fichero sea legible, y la unica forma de ver una
     * divergencia entre LECTORES cuando el escritor es fiel.            */
    if (!strcmp(argv[1], "--cabecera")) {
        char why[512];

        if (argc < 3) { fprintf(stderr, "uso: --cabecera <fichero>\n"); return 2; }
        if (inp_check_fue(argv[2], why, sizeof why) != 0) { printf("no\n"); return 4; }
        load_input_fue(argv[2]);
        printf("nobs=%d freq=%d year=%d period=%d name=%s\n",
               Ts.nobs, Ts.freq, Ts.begyear, Ts.begtime, Ts.name ? Ts.name : "");
        return 0;
    }

    ctx = g_new0(FueContext, 1);
    app = gtk_application_new("org.atsw.roundtrip", G_APPLICATION_FLAGS_NONE);
    g_application_register(G_APPLICATION(app), NULL, NULL);
    ctx->main_window = create_main_window(app, ctx);
    gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(ctx->main_window)));

    /* Como lo carga el usuario: por la misma puerta que el GUI. Sin ella un
     * fichero de fuf lee el horizonte donde el lector espera el numero de
     * deterministas, y sigue reservando hasta que se acaba la memoria.   */
    {
        char why[512];

        if (inp_check_fue(argv[1], why, sizeof why) != 0) {
            printf("RECHAZADO %s\n", why);
            return 4;
        }
    }
    load_input_fue(argv[1]);
    update_ui_from_model(ctx);

    /* y como lo guarda */
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(ctx->workspace_file_chooser), argv[2]);
    gtk_entry_set_text(GTK_ENTRY(ctx->input_name_entry), argv[3]);
    sync_all_from_ui(ctx);
    save_inp_file(ctx);

    return 0;
}
