/*
 * iconos.c -- los iconos propios de la barra de fue_gui (ver iconos.h).
 *
 * El estudio que los propuso: la barra tenia tres lupas (View Output,
 * Anomalos, Identificar), la «i» de informacion para la diagnosis, la
 * flecha de recargar para la ganancia y una de «saltar» para prever; y sin
 * texto. Estos dibujan lo que cada ventana ensena.
 *
 * Cada SVG es una plantilla de 24 x 24: @F es el color del texto del tema,
 * @A el acento, y @V, @M y @R el verde, el ambar y el rojo de los
 * veredictos, que son los unicos colores fijos.
 */

#include <string.h>
#include "iconos.h"

#define CAB "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' " \
            "stroke='@F' stroke-width='1.7' stroke-linecap='round' stroke-linejoin='round'>"
#define FINA "stroke-width='1.1' stroke-dasharray='2 2'"

typedef struct { const char *nombre, *cuerpo; } Icono;

static const Icono iconos[] = {
    { "nuevo",
      "<path d='M6 2.8h8.5L19 7.3v13.9H6z'/><path d='M14.5 2.8v4.5H19'/>"
      "<path d='M12.5 11v6M9.5 14h6' stroke='@A'/>" },
    { "abrir",
      "<path d='M3 6.5h6l2 2h10v10.5H3z'/><path d='M3 11h18'/>" },
    { "guardar",
      "<path d='M4 4h13l3 3v13H4z'/><path d='M8 4v5h8V4M8 20v-6h8v6'/>" },
    { "estimar",
      "<path d='M7 4.5v15l12-7.5z' fill='@A' stroke='@A'/>" },
    { "ver-modelo",
      "<path d='M6 2.8h8.5L19 7.3v13.9H6z'/><path d='M14.5 2.8v4.5H19'/>"
      "<path d='M8.2 11.2l1.6-1.8 1.4 1.3 1.6-2.2 1.5 1.6 1.7-1.2' stroke='@A'/>"
      "<path d='M8.3 15h7.6M8.3 18h5'/>" },
    { "diagnosis",
      "<circle cx='5.5' cy='6' r='2' fill='@V' stroke='none'/>"
      "<circle cx='5.5' cy='12' r='2' fill='@V' stroke='none'/>"
      "<circle cx='5.5' cy='18' r='2' fill='@M' stroke='none'/>"
      "<path d='M10 6h10M10 12h10M10 18h7'/>" },
    { "anomalos",
      "<path d='M2.5 9h19M2.5 17h19' " FINA "/>"
      "<path d='M2.5 13.5l2.2-1.4 2 1.8 2-1.2 2 .8 1.8-9.2 1.9 10.6 2-1.6 2 1 2-.8 2.3.6'/>"
      "<circle cx='12.5' cy='4.3' r='1.9' fill='@R' stroke='none'/>" },
    { "ganancia",
      "<path d='M2.5 19.5h5v-5h4v-4h4v-4h6'/><path d='M15.5 6.5h6' stroke='@A'/>"
      "<path d='M2.5 6.5h12' " FINA "/>" },
    { "identificar",
      "<path d='M3 12h18'/><path d='M3 8.5h18M3 15.5h18' " FINA "/>"
      "<path d='M5 12V4M8.5 12V7.5M12 12v2.5M15.5 12V9.8M19 12v1.6' stroke-width='2.4'/>" },
    { "prever",
      "<path d='M12.5 12l9-6.5v13z' fill='@A' fill-opacity='.22' stroke='none'/>"
      "<path d='M2.5 14l2.5-3 2.5 2 2.5-4 2.5 3'/><path d='M12.5 12h9' stroke='@A'/>"
      "<path d='M12.5 12l9-6.5M12.5 12l9 6.5' " FINA "/>" },
    { "integracion",
      "<circle cx='12' cy='12' r='8.5'/><path d='M1.8 12h20.4M12 1.8v20.4' " FINA "/>"
      "<circle cx='20.5' cy='12' r='2.3' fill='@A' stroke='none'/>" },
    { "estacionalidad",
      "<circle cx='12' cy='12' r='8.5'/><path d='M1.8 12h20.4M12 1.8v20.4' " FINA "/>"
      "<g fill='@F' stroke='none'>"
      "<circle cx='20.5' cy='12' r='1.25'/><circle cx='19.36' cy='16.25' r='1.25'/>"
      "<circle cx='16.25' cy='19.36' r='1.25'/><circle cx='12' cy='20.5' r='1.25'/>"
      "<circle cx='7.75' cy='19.36' r='1.25'/><circle cx='4.64' cy='16.25' r='1.25'/>"
      "<circle cx='3.5' cy='12' r='1.25'/><circle cx='4.64' cy='7.75' r='1.25'/>"
      "<circle cx='7.75' cy='4.64' r='1.25'/><circle cx='12' cy='3.5' r='1.25'/>"
      "<circle cx='16.25' cy='4.64' r='1.25'/><circle cx='19.36' cy='7.75' r='1.25'/></g>"
      "<circle cx='16.25' cy='4.64' r='2' fill='@A' stroke='none'/>"
      "<circle cx='16.25' cy='19.36' r='2' fill='@A' stroke='none'/>" },
    { "factores",
      "<circle cx='12' cy='12' r='8.5' " FINA "/><path d='M1.8 12h20.4M12 1.8v20.4' " FINA "/>"
      "<circle cx='16.8' cy='8.2' r='1.7' fill='@A' stroke='none'/>"
      "<circle cx='16.8' cy='15.8' r='1.7' fill='@A' stroke='none'/>"
      "<circle cx='8.5' cy='12' r='1.7' fill='@F' stroke='none'/>" },
    { "exportar",
      "<path d='M4 4.5h11v15H4z'/><path d='M6.5 8.5h6M6.5 11.5h6M6.5 14.5h4'/>"
      "<path d='M15 12h6.5M18.5 9l3 3-3 3' stroke='@A'/>" },
};

static const Icono *busca( const char *nombre )
{
    size_t i;

    for ( i = 0; i < G_N_ELEMENTS(iconos); i++ )
        if ( !strcmp( iconos[i].nombre, nombre ) ) return &iconos[i];
    return NULL;
}

static void hex( char *b, size_t n, const GdkRGBA *c )
{
    g_snprintf( b, n, "#%02x%02x%02x", (int)( c->red * 255 + .5 ),
                (int)( c->green * 255 + .5 ), (int)( c->blue * 255 + .5 ) );
}

/* La plantilla con sus colores. El acento es el azul de Adwaita, un poco
 * mas claro sobre fondo oscuro; el verde, el ambar y el rojo, los suyos. */
static gchar *rellena( const char *cuerpo, GtkWidget *ref )
{
    GdkRGBA  fg = { 0.12, 0.14, 0.16, 1.0 };
    char     f[8];
    gboolean oscuro;
    GString *s = g_string_new( CAB );
    const char *p;

    if ( ref )
        gtk_style_context_get_color( gtk_widget_get_style_context( ref ),
                                     GTK_STATE_FLAG_NORMAL, &fg );
    oscuro = ( fg.red + fg.green + fg.blue ) / 3.0 > 0.5;   /* texto claro */
    hex( f, sizeof f, &fg );
    g_string_append( s, cuerpo );
    g_string_append( s, "</svg>" );
    for ( p = strchr( s->str, '@' ); p; p = strchr( s->str, '@' ) )
        {
        gsize       at = (gsize)( p - s->str );
        const char *c;

        switch ( p[1] )
            {
            case 'F': c = f; break;
            case 'A': c = oscuro ? "#78aeed" : "#1c71d8"; break;
            case 'V': c = oscuro ? "#57e389" : "#26a269"; break;
            case 'M': c = oscuro ? "#f8e45c" : "#e5a50a"; break;
            case 'R': c = oscuro ? "#ff7b63" : "#e01b24"; break;
            default:  c = "#000000"; break;
            }
        g_string_erase( s, at, 2 );
        g_string_insert( s, at, c );
        }
    return g_string_free( s, FALSE );
}

GtkWidget *fue_icono( const char *nombre, int px, GtkWidget *ref,
                      const char *respaldo )
{
    const Icono     *ic = busca( nombre );
    GdkPixbufLoader *l;
    GdkPixbuf       *pb = NULL;
    gchar           *svg;
    int              esc = ref ? gtk_widget_get_scale_factor( ref ) : 1;

    if ( ic == NULL ) goto tema;
    svg = rellena( ic->cuerpo, ref );
    l = gdk_pixbuf_loader_new_with_type( "svg", NULL );
    if ( l )
        {
        gdk_pixbuf_loader_set_size( l, px * esc, px * esc );
        if ( gdk_pixbuf_loader_write( l, (const guchar *) svg, strlen( svg ), NULL ) &&
             gdk_pixbuf_loader_close( l, NULL ) )
            {
            pb = gdk_pixbuf_loader_get_pixbuf( l );
            if ( pb ) g_object_ref( pb );
            }
        else
            gdk_pixbuf_loader_close( l, NULL );
        g_object_unref( l );
        }
    g_free( svg );
    if ( pb )
        {
        cairo_surface_t *sf = gdk_cairo_surface_create_from_pixbuf( pb, esc, NULL );
        GtkWidget       *im = gtk_image_new_from_surface( sf );

        cairo_surface_destroy( sf );
        g_object_unref( pb );
        gtk_widget_set_name( im, nombre );
        return im;
        }
tema:
    return gtk_image_new_from_icon_name( respaldo ? respaldo : "image-missing",
                                         px <= 16 ? GTK_ICON_SIZE_MENU
                                                  : GTK_ICON_SIZE_LARGE_TOOLBAR );
}

GtkWidget *fue_menu_item( const char *texto, const char *icono, GtkWidget *ref,
                          const char *respaldo )
{
    GtkWidget *mi = gtk_menu_item_new();
    GtkWidget *caja = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
    GtkWidget *et = gtk_label_new( texto );

    if ( icono )
        gtk_box_pack_start( GTK_BOX(caja), fue_icono( icono, 16, ref, respaldo ),
                            FALSE, FALSE, 0 );
    else
        gtk_widget_set_margin_start( et, 24 );
    gtk_label_set_xalign( GTK_LABEL(et), 0.0 );
    gtk_box_pack_start( GTK_BOX(caja), et, TRUE, TRUE, 0 );
    gtk_container_add( GTK_CONTAINER(mi), caja );
    return mi;
}
