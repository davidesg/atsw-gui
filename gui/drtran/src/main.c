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

#include "gui.h"


static void activate(GtkApplication *app, gpointer data)
{
    Mtram *m = data;

    gtk_widget_show_all(mtram_window_new(app, m));
}

int main(int argc, char **argv)
{
    GtkApplication *app;
    Mtram m = { 0 };
    int rc;

    app = gtk_application_new("org.drtran.gui", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &m);
    rc = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return rc;
}
