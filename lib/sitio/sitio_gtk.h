/*
 * sitio_gtk.h -- lo de lib/sitio que necesita una ventana. Solo cabecera: lo
 * incluyen los GUIs hijos, y atsw-cli, que enlaza lib/sitio sin GTK, ni lo
 * ve.
 */
#ifndef SITIO_GTK_H
#define SITIO_GTK_H

#include <gtk/gtk.h>
#include "sitio.h"

/* LA MADRE SE FUE SIN DESPEDIRSE (AUDITORIA-procesos B4): el hijo no se
 * cierra, y lo dice en un dialogo que no bloquea, sobre su ventana. */
static inline void sitio_avisa_madre_caida( GtkWindow *ventana )
{
    GtkWidget *d = gtk_message_dialog_new( ventana, GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
                                           "%s", sitio_madre_caida_titulo() );

    gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG(d), "%s",
                                              sitio_madre_caida_texto() );
    g_signal_connect( d, "response", G_CALLBACK(gtk_widget_destroy), NULL );
    gtk_widget_show( d );
}

/* La ventana activa de la GtkApplication de este proceso, o NULL. */
static inline GtkWindow *sitio_ventana_activa( void )
{
    GApplication *a = g_application_get_default();

    return ( a && GTK_IS_APPLICATION(a) )
           ? gtk_application_get_active_window( GTK_APPLICATION(a) ) : NULL;
}

#endif
