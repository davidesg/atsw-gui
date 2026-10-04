/*
 * drtran_gui -- el GUI de drtran.
 *
 * SE LLAMA COMO SU MOTOR, no como el asistente. "mtram" es el SERVIDOR MCP de
 * drtran-python, y pip lo pone en ~/.local/bin/mtram: llamar igual al binario
 * de GTK hacia que una instalacion lo machacase sin avisar. El convenio del
 * monorepo ya estaba puesto por gui/fue, que construye fue_gui.
 *
 * Parte de .pre YA ESTIMADOS, no de datos crudos, y eso es deliberado: dejar
 * cargar un CSV aqui seria saltarse el escalon univariante, que es el que la
 * escalera certifica. El modelo de cada serie se hace con fue; aqui se cruzan.
 */

#include <string.h>

#include "gui.h"
#include "previewhost.h"


static void activate(GtkApplication *app, gpointer data)
{
    Mtram *m = data;

    gtk_widget_show_all(mtram_window_new(app, m));

    /* EL CASO SE CARGA CON LA VENTANA YA HECHA: cargarlo es llenar sus
     * paginas --las series, la red, el modelo-- y dice en la barra lo que
     * paso, incluido un caso desfasado.                               */
    if (m->caso[0]) {
        char why[1024];

        if (!mtram_caso_carga(m, why, sizeof why) || why[0])
            preview_show_status(m, "%s", why);
    }
}

/* --proyecto FICHERO -- SIN EL, EL PROGRAMA FUNCIONA COMO SIEMPRE.
 *
 * Es deliberado: la interfaz madre todavia no existe, y obligar a tener un
 * proyecto para estimar una red seria poner la carreta delante. Lo que cambia
 * con el es que cada estimacion pasa a ser una CORRIDA con su nombre y su
 * linaje, en vez de pisar siempre modelo.out -- que es la ranura unica
 * RESIDUOS de TASTE reaparecida por la puerta de atras.            */
static void uso(const char *me)
{
    printf("uso: %s [--proyecto FICHERO [--caso C [--corrida c]]]\n\n"
           "  --proyecto F  abre (o empieza) el proyecto F. Cada estimacion\n"
           "                sera una corrida con su nombre y su linaje, en\n"
           "                un CASO: lo que se cruza. Si las series cargadas\n"
           "                son modelos del proyecto, el caso se da de alta\n"
           "                solo. Sin esto se escribe siempre en los mismos\n"
           "                ficheros de la cache, y NO CABEN DOS MODELOS.\n"
           "  --caso C      carga el caso C: sus series en su orden, y la red\n"
           "                y las restricciones de su corrida elegida.\n"
           "  --corrida c   parte de la corrida c del caso, no de la elegida.\n",
           me);
}

int main(int argc, char **argv)
{
    GtkApplication *app;
    Mtram m = { 0 };
    const char *proy = NULL, *caso = NULL, *corrida = NULL;
    int rc, i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--proyecto") && i + 1 < argc) proy = argv[++i];
        else if (!strncmp(argv[i], "--proyecto=", 11)) proy = argv[i] + 11;
        else if (!strcmp(argv[i], "--caso") && i + 1 < argc) caso = argv[++i];
        else if (!strcmp(argv[i], "--corrida") && i + 1 < argc) corrida = argv[++i];
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
            { uso(argv[0]); return 0; }
        else { fprintf(stderr, "%s: no entiendo «%s»\n", argv[0], argv[i]);
               uso(argv[0]); return 2; }
    }

    if ((caso || corrida) && !proy) {
        fprintf(stderr, "%s: --caso y --corrida son de un proyecto: hace falta "
                        "--proyecto\n", argv[0]);
        return 2;
    }
    if (corrida && !caso) {
        fprintf(stderr, "%s: --corrida es de un caso: hace falta --caso\n",
                argv[0]);
        return 2;
    }

    if (proy) {
        char why[512];

        if (!mtram_proyecto_abre(&m, proy, why, sizeof why)) {
            /* Un manifiesto ROTO no se pisa: se dice y se para. Arrancar
               ignorandolo escribiria encima de el al guardar.          */
            fprintf(stderr, "%s: %s\n", proy, why);
            return 3;
        }
        /* Un caso que no esta se dice YA, antes de abrir una ventana que no
         * podria cargarlo.                                              */
        if (caso && pr_caso_idx(m.proy, caso) < 0) {
            fprintf(stderr, "%s: «%s» no es un caso del proyecto\n", proy, caso);
            return 3;
        }
        if (caso)    snprintf(m.caso, sizeof m.caso, "%s", caso);
        if (corrida) snprintf(m.corrida_ini, sizeof m.corrida_ini, "%s", corrida);
    }

    /* Las opciones ya estan leidas: al GTK solo le llega el nombre. */
    argc = 1;
    app = gtk_application_new("org.drtran.gui", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &m);
    rc = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return rc;
}
