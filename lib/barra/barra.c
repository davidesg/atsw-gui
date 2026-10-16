/*
 * barra.c -- la barra de herramientas de los GUIs de ATSW (ver barra.h).
 */

#include "barra.h"

GtkToolItem *at_boton( GtkWidget *barra, const char *rotulo, const char *icono,
                       const char *respaldo, const char *globo, GCallback cb,
                       gpointer datos )
{
    GtkToolItem *b = gtk_tool_button_new( at_icono( icono, 24, barra, respaldo ), rotulo );

    gtk_widget_set_tooltip_text( GTK_WIDGET(b), globo );
    g_signal_connect( b, "clicked", cb, datos );
    gtk_toolbar_insert( GTK_TOOLBAR(barra), b, -1 );
    return b;
}

GtkToolItem *at_partido( GtkWidget *barra, const char *rotulo, const char *icono,
                         const char *respaldo, GtkWidget *menu, GCallback cb,
                         gpointer datos )
{
    GtkToolItem *b = gtk_menu_tool_button_new( at_icono( icono, 24, barra, respaldo ),
                                               rotulo );
    gchar *mas = g_strdup_printf( "Más: el resto de %s", rotulo );

    gtk_menu_tool_button_set_menu( GTK_MENU_TOOL_BUTTON(b), menu );
    gtk_menu_tool_button_set_arrow_tooltip_text( GTK_MENU_TOOL_BUTTON(b), mas );
    g_free( mas );
    g_signal_connect( b, "clicked", cb, datos );
    gtk_toolbar_insert( GTK_TOOLBAR(barra), b, -1 );
    return b;
}

GtkWidget *at_entrada( GtkWidget *menu, const char *texto, const char *icono,
                       const char *respaldo, GtkWidget *ref, GCallback cb,
                       gpointer datos )
{
    GtkWidget *mi = at_menu_item( texto, icono, ref, respaldo );

    g_signal_connect( mi, "activate", cb, datos );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), mi );
    return mi;
}

static gboolean pulsa( GtkAccelGroup *g, GObject *o, guint k, GdkModifierType m,
                       gpointer b )
{
    (void) g; (void) o; (void) k; (void) m;
    if (gtk_widget_is_sensitive( GTK_WIDGET(b) )) g_signal_emit_by_name( b, "clicked" );
    return TRUE;
}

void at_atajo( GtkAccelGroup *g, guint tecla, GtkToolItem *b )
{
    gtk_accel_group_connect( g, tecla, GDK_CONTROL_MASK, 0,
                             g_cclosure_new( G_CALLBACK(pulsa), b, NULL ) );
}
