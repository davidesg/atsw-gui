/*
 * cierre.c -- un modulo de GTK para la prueba de humo (gui-humo.sh): pasados
 * CIERRE segundos, cierra las ventanas del programa como lo haria el usuario
 * (gtk_window_close: el aspa, no un kill), y el programa tiene que salir solo
 * y bien.
 *
 * Existe porque la prueba de humo mataba los GUIs con kill y el cierre no se
 * probaba en ninguna plataforma. En macOS, cerrar fue_gui sacaba un aviso de
 * fallo (TODO.md, 13-nov-2026), y los ejecutores de la CI de macOS son Macs
 * de verdad: aqui se ve lo que alli pasa.
 *
 * Se carga con GTK_MODULES=<ruta de este modulo>. Cierra primero las ventanas
 * secundarias y despues las de la aplicacion; si a los pocos segundos queda
 * alguna (un dialogo «¿guardar?»), la cierra tambien.
 */

#include <stdlib.h>
#include <gtk/gtk.h>

static gboolean cierra( gpointer d )
{
    GList *l = gtk_window_list_toplevels(), *q;
    int    n = 0;

    (void) d;
    for ( q = l; q; q = q->next )
        {
        GtkWindow *w = q->data;

        if ( gtk_widget_get_visible( GTK_WIDGET(w) ) &&
             gtk_window_get_window_type( w ) == GTK_WINDOW_TOPLEVEL )
            {
            g_printerr( "cierre: cierro «%s»\n",
                        gtk_window_get_title( w ) ? gtk_window_get_title( w ) : "" );
            gtk_window_close( w );
            n++;
            }
        }
    g_list_free( l );
    /* si queda algo abierto (un dialogo que salio al cerrar), otra vuelta */
    return n > 0 ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

G_MODULE_EXPORT void gtk_module_init( gint *argc, gchar ***argv )
{
    const char *s = g_getenv( "CIERRE" );
    int         t = s ? atoi( s ) : 10;

    (void) argc; (void) argv;
    g_timeout_add_seconds( t > 0 ? t : 10, cierra, NULL );
}
