/*
 * presentacion.c -- la ventana de presentacion al arrancar, con los
 * creditos, y la comprobacion de la instalacion que corre mientras tanto.
 *
 * Lo pidio el analista (docs/TODO.md, «el arranque, la presentacion y el
 * icono»). Antes, si faltaba un motor o un GUI, se sabia al primer clic y
 * por la barra de estado: «No encuentro fue_gui». Ahora se sabe al
 * arrancar, con QUE falta y COMO se arregla en esta plataforma.
 *
 * COMO SE BUSCA: donde los busca la madre al lanzarlos (atsw_programa, que
 * es lib/sitio: primero al lado, en el arbol de compilacion o en el paquete,
 * y despues el PATH). Comprobar en otro sitio diria que esta lo que luego
 * no se encuentra.
 *
 * QUE NO PARA: lo opcional (drvarma y drvec, que aun no van en el paquete;
 * el asistente) se dice en gris y no es un error. Con algo obligatorio en
 * falta la presentacion se queda abierta con el informe; si no, se cierra
 * sola y abre la madre.
 *
 * Los creditos, con la redaccion del analista (la misma que «Acerca de»,
 * el README y la web).
 */

#include <string.h>

#include "atsw.h"
#include "iconos.h"

#ifndef ATSW_VERSION
#define ATSW_VERSION "en desarrollo"
#endif

#define CREDITOS \
    "Diseño y construcción: Arthur B. Treadway y David E. Guerrero.\n" \
    "Librerías y motores: José Alberto Mauricio, con Arthur B. Treadway y " \
    "David E. Guerrero."
#define COPYRIGHT "Copyright © 1995-2026 A.B. Treadway, J.A. Mauricio y D.E. Guerrero"

/* ------------------------------------------------------------------------ */
/* LA COMPROBACION                                                          */
/* ------------------------------------------------------------------------ */

/* Como se arregla un programa que falta, en esta plataforma. */
static void arreglo_programa( char *b, size_t n, const char *prog )
{
#ifdef G_OS_WIN32
    g_snprintf( b, n, "Falta %s.exe en la carpeta del programa. Vuelve a instalar ATSW "
                "con su instalador.", prog );
#elif defined(__APPLE__)
    g_snprintf( b, n, "Falta %s dentro de ATSW.app. Vuelve a copiar ATSW.app desde el "
                ".dmg a Aplicaciones.", prog );
#else
    g_snprintf( b, n, "Falta %s. Con el paquete: sudo apt install --reinstall atsw. En "
                "el árbol de fuentes: make en la raíz.", prog );
#endif
}

static void programa( AtComprob *c, const char *grupo, const char *prog,
                      gboolean opcional, const char *si_falta )
{
    gchar *r = atsw_programa( prog );

    memset( c, 0, sizeof *c );
    g_snprintf( c->grupo, sizeof c->grupo, "%s", grupo );
    g_snprintf( c->que, sizeof c->que, "%s", prog );
    c->opcional = opcional;
    c->ok = r && g_file_test( r, G_FILE_TEST_IS_EXECUTABLE );
    if ( c->ok ) g_snprintf( c->ruta, sizeof c->ruta, "%s", r );
    else if ( si_falta ) g_snprintf( c->arreglo, sizeof c->arreglo, "%s", si_falta );
    else arreglo_programa( c->arreglo, sizeof c->arreglo, prog );
    g_free( r );
}

/* El selector de ficheros aborta sin sus esquemas («No GSettings schemas
 * are installed»): lo vio la CI de Windows. */
static void esquemas( AtComprob *c )
{
    GSettingsSchemaSource *src = g_settings_schema_source_get_default();
    GSettingsSchema       *sc = src ? g_settings_schema_source_lookup(
                                    src, "org.gtk.Settings.FileChooser", TRUE ) : NULL;

    memset( c, 0, sizeof *c );
    g_snprintf( c->grupo, sizeof c->grupo, "GTK" );
    g_snprintf( c->que, sizeof c->que, "los esquemas de GSettings" );
    c->ok = sc != NULL;
    if ( sc ) g_settings_schema_unref( sc );
    if ( !c->ok )
#ifdef G_OS_WIN32
        g_snprintf( c->arreglo, sizeof c->arreglo, "Sin ellos, abrir un fichero cierra el "
                    "programa. Vuelve a instalar ATSW con su instalador." );
#elif defined(__APPLE__)
        g_snprintf( c->arreglo, sizeof c->arreglo, "Sin ellos, abrir un fichero cierra el "
                    "programa. Vuelve a copiar ATSW.app desde el .dmg." );
#else
        g_snprintf( c->arreglo, sizeof c->arreglo, "Sin ellos, abrir un fichero cierra el "
                    "programa: sudo apt install gsettings-desktop-schemas." );
#endif
}

/* Los cargadores de imagenes de gdk-pixbuf: PNG para los graficos, SVG para
 * los iconos de las barras (sin el, salen los del tema: opcional). */
static void cargador( AtComprob *c, const char *formato, const char *que, gboolean opcional )
{
    GSList *l, *f = gdk_pixbuf_get_formats();

    memset( c, 0, sizeof *c );
    g_snprintf( c->grupo, sizeof c->grupo, "GTK" );
    g_snprintf( c->que, sizeof c->que, "%s", que );
    c->opcional = opcional;
    for ( l = f; l; l = l->next )
        {
        gchar *nom = gdk_pixbuf_format_get_name( l->data );

        c->ok |= !g_ascii_strcasecmp( nom, formato );
        g_free( nom );
        }
    g_slist_free( f );
    if ( !c->ok )
#if defined(G_OS_WIN32) || defined(__APPLE__)
        g_snprintf( c->arreglo, sizeof c->arreglo, "Falta el cargador de %s de gdk-pixbuf. "
                    "Vuelve a instalar ATSW.", formato );
#else
        g_snprintf( c->arreglo, sizeof c->arreglo, "Falta el cargador de %s de gdk-pixbuf: "
                    "sudo apt install %s.", formato,
                    !strcmp( formato, "svg" ) ? "librsvg2-common" : "libgdk-pixbuf2.0-0" );
#endif
}

int atsw_comprueba( AtComprob *c, int max )
{
    static const char *const motores[] = { "fue", "fuf", "fug", "drtran", "art" };
    static const char *const guis[]    = { "fue_gui", "gtk_fmg", "drtran_gui" };
    int n = 0;
    guint i;

    for ( i = 0; i < G_N_ELEMENTS(motores) && n < max; i++ )
        programa( &c[n++], "Motores", motores[i], FALSE, NULL );
    for ( i = 0; i < G_N_ELEMENTS(guis) && n < max; i++ )
        programa( &c[n++], "Programas", guis[i], FALSE, NULL );
    if ( n < max ) esquemas( &c[n++] );
    if ( n < max ) cargador( &c[n++], "png", "las imágenes PNG", FALSE );
    if ( n < max ) cargador( &c[n++], "svg", "los iconos SVG de las barras", TRUE );
    /* LO QUE TODAVIA NO VA EN EL PAQUETE: se dice y no es un error. */
    if ( n < max ) programa( &c[n++], "Sistemas", "drvarma", TRUE,
                             "Todavía no va en el paquete: VARMA llega en una versión posterior." );
    if ( n < max ) programa( &c[n++], "Sistemas", "drvec", TRUE,
                             "Todavía no va en el paquete: VECM llega en una versión posterior." );
    /* EL ASISTENTE (ESTUDIO-terminal-mcp §6.3): opcional; se instala con uv. */
    if ( n < max ) programa( &c[n++], "Asistente", "uv", TRUE,
                             "El asistente es opcional y no está instalado: hace falta uv "
                             "(https://docs.astral.sh/uv/)." );
    return n;
}

int atsw_comprueba_faltan( const AtComprob *c, int n )
{
    int i, k = 0;

    for ( i = 0; i < n; i++ ) k += !c[i].ok && !c[i].opcional;
    return k;
}

/* El informe en Pango: una linea por cosa, y debajo de lo que falta, como
 * se arregla. En el globo de cada linea, donde se encontro.            */
static gchar *informe( const AtComprob *c, int n )
{
    GString    *t = g_string_new( "" );
    const char *grupo = "";
    int         i;

    for ( i = 0; i < n; i++ )
        {
        gchar *que = g_markup_escape_text( c[i].que, -1 );

        if ( strcmp( grupo, c[i].grupo ) )
            {
            grupo = c[i].grupo;
            g_string_append_printf( t, "%s<b>%s</b>\n", i ? "\n" : "", grupo );
            }
        if ( c[i].ok )
            g_string_append_printf( t, "  <span foreground='#26a269'>✓</span> %s\n", que );
        else if ( c[i].opcional )
            {
            gchar *a = g_markup_escape_text( c[i].arreglo, -1 );

            g_string_append_printf( t, "  <span foreground='#888888'>– %s: %s</span>\n", que, a );
            g_free( a );
            }
        else
            {
            gchar *a = g_markup_escape_text( c[i].arreglo, -1 );

            g_string_append_printf( t, "  <span foreground='#e01b24'>✗ <b>%s</b></span>: %s\n",
                                    que, a );
            g_free( a );
            }
        g_free( que );
        }
    return g_string_free( t, FALSE );
}

/* El mismo informe en texto llano, con las rutas: para copiarlo y pegarlo
 * en un correo cuando algo no va.                                        */
static gchar *informe_llano( const AtComprob *c, int n )
{
    GString *t = g_string_new( "ATSW " ATSW_VERSION "\n" );
    int      i;

    for ( i = 0; i < n; i++ )
        g_string_append_printf( t, "%-10s %-30s %s\n", c[i].grupo, c[i].que,
                                c[i].ok ? c[i].ruta : c[i].opcional ? "(opcional, no está)"
                                                                    : "FALTA" );
    return g_string_free( t, FALSE );
}

/* ------------------------------------------------------------------------ */
/* LA VENTANA DE PRESENTACION                                               */
/* ------------------------------------------------------------------------ */

typedef struct {
    Atsw      *a;
    GtkWidget *win, *madre, *l_estado, *l_informe, *sw, *barra;
    AtComprob  c[AT_MAX_COMPROB];
    int        n;
    gboolean   listo, minimo, cerrada;
} Pr;

static void abre_madre( Pr *g )
{
    if ( g->cerrada ) return;
    g->cerrada = TRUE;
    gtk_widget_show_all( g->madre );
    gtk_widget_destroy( g->win );
}

static void on_continuar( GtkButton *b, Pr *g ) { (void) b; abre_madre( g ); }

static void on_copiar( GtkButton *b, Pr *g )
{
    gchar *t = informe_llano( g->c, g->n );

    (void) b;
    gtk_clipboard_set_text( gtk_clipboard_get( GDK_SELECTION_CLIPBOARD ), t, -1 );
    gtk_label_set_text( GTK_LABEL(g->l_estado), "El informe está en el portapapeles." );
    g_free( t );
}

static gboolean on_tecla( GtkWidget *w, GdkEventKey *ev, Pr *g )
{
    (void) w;
    if ( ev->keyval == GDK_KEY_Escape || ev->keyval == GDK_KEY_Return )
        { if ( g->listo ) abre_madre( g ); return TRUE; }
    return FALSE;
}

static void on_destruida( GtkWidget *w, Pr *g ) { (void) w; g_free( g ); }

/* Las dos condiciones para irse solo: la comprobacion acabada sin faltas y
 * el tiempo minimo para leer los creditos. */
static void quiza_se_va( Pr *g )
{
    if ( g->listo && g->minimo && atsw_comprueba_faltan( g->c, g->n ) == 0 )
        abre_madre( g );
}

static gboolean on_minimo( gpointer d )
{
    Pr *g = d;

    g->minimo = TRUE;
    quiza_se_va( g );
    return G_SOURCE_REMOVE;
}

static gboolean comprueba( gpointer d )
{
    Pr    *g = d;
    int    faltan;
    gchar *m;

    g->n = atsw_comprueba( g->c, AT_MAX_COMPROB );
    g->listo = TRUE;
    faltan = atsw_comprueba_faltan( g->c, g->n );
    if ( faltan == 0 )
        {
        gtk_label_set_text( GTK_LABEL(g->l_estado), "Todo está instalado." );
        quiza_se_va( g );
        return G_SOURCE_REMOVE;
        }

    /* FALTA ALGO: la presentacion se queda con el informe y los botones. */
    m = informe( g->c, g->n );
    gtk_label_set_markup( GTK_LABEL(g->l_informe), m );
    g_free( m );
    gtk_widget_set_no_show_all( g->sw, FALSE );
    gtk_widget_show_all( g->sw );
    m = g_strdup_printf( faltan == 1 ? "Falta %d cosa: abajo, cómo se arregla."
                                     : "Faltan %d cosas: abajo, cómo se arregla.", faltan );
    gtk_label_set_text( GTK_LABEL(g->l_estado), m );
    g_free( m );
    gtk_widget_set_no_show_all( g->barra, FALSE );
    gtk_widget_show_all( g->barra );
    gtk_window_resize( GTK_WINDOW(g->win), 620, 560 );
    return G_SOURCE_REMOVE;
}

void atsw_presentacion( Atsw *a, GtkWidget *madre )
{
    Pr        *g = g_new0( Pr, 1 );
    GtkWidget *caja, *fila, *txt, *l, *sw, *b;
    GdkPixbuf *logo;

    g->a = a;
    g->madre = madre;
    g->win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_title( GTK_WINDOW(g->win), "ATSW" );
    gtk_window_set_decorated( GTK_WINDOW(g->win), FALSE );
    gtk_window_set_position( GTK_WINDOW(g->win), GTK_WIN_POS_CENTER );
    gtk_window_set_type_hint( GTK_WINDOW(g->win), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN );
    gtk_window_set_default_size( GTK_WINDOW(g->win), 620, 300 );
    g_signal_connect( g->win, "key-press-event", G_CALLBACK(on_tecla), g );
    g_signal_connect( g->win, "destroy", G_CALLBACK(on_destruida), g );

    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 12 );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 24 );
    gtk_container_add( GTK_CONTAINER(g->win), caja );

    fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 20 );
    gtk_box_pack_start( GTK_BOX(caja), fila, FALSE, FALSE, 0 );
    logo = at_logo( 96 * gtk_widget_get_scale_factor( g->win ) );
    {
    cairo_surface_t *s = gdk_cairo_surface_create_from_pixbuf(
                             logo, gtk_widget_get_scale_factor( g->win ), NULL );

    gtk_box_pack_start( GTK_BOX(fila), gtk_image_new_from_surface( s ), FALSE, FALSE, 0 );
    cairo_surface_destroy( s );
    }
    g_object_unref( logo );

    txt = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_box_pack_start( GTK_BOX(fila), txt, TRUE, TRUE, 0 );
    l = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(l), "<span size='xx-large' weight='bold'>ATSW</span>  "
                          "<span size='large'>" ATSW_VERSION "</span>" );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_box_pack_start( GTK_BOX(txt), l, FALSE, FALSE, 0 );
    l = gtk_label_new( "A Time Series Workshop: el taller de series temporales de la "
                       "escuela Box-Jenkins-Treadway." );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    gtk_box_pack_start( GTK_BOX(txt), l, FALSE, FALSE, 0 );
    l = gtk_label_new( CREDITOS );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    gtk_label_set_max_width_chars( GTK_LABEL(l), 60 );
    gtk_box_pack_start( GTK_BOX(txt), l, FALSE, FALSE, 6 );
    l = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(l), "<small>" COPYRIGHT ". Software libre: GPL v2 o "
                          "posterior.</small>" );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_box_pack_start( GTK_BOX(txt), l, FALSE, FALSE, 0 );

    g->l_estado = gtk_label_new( "Comprobando la instalación…" );
    gtk_label_set_xalign( GTK_LABEL(g->l_estado), 0.0 );
    gtk_box_pack_start( GTK_BOX(caja), g->l_estado, FALSE, FALSE, 0 );

    /* el informe, escondido hasta que haga falta */
    g->l_informe = gtk_label_new( "" );
    gtk_label_set_xalign( GTK_LABEL(g->l_informe), 0.0 );
    gtk_label_set_yalign( GTK_LABEL(g->l_informe), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(g->l_informe), TRUE );
    gtk_label_set_selectable( GTK_LABEL(g->l_informe), TRUE );
    sw = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sw), g->l_informe );
    gtk_box_pack_start( GTK_BOX(caja), sw, TRUE, TRUE, 0 );
    gtk_widget_set_no_show_all( sw, TRUE );
    g->sw = sw;

    g->barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    b = gtk_button_new_with_mnemonic( "_Continuar" );
    gtk_widget_set_tooltip_text( b, "Abre el taller de todas formas: lo que falta "
                                    "fallará cuando se use." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_continuar), g );
    gtk_box_pack_end( GTK_BOX(g->barra), b, FALSE, FALSE, 0 );
    b = gtk_button_new_with_mnemonic( "Co_piar el informe" );
    gtk_widget_set_tooltip_text( b, "Al portapapeles, con las rutas de lo que sí está: "
                                    "para pegarlo en un correo." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_copiar), g );
    gtk_box_pack_end( GTK_BOX(g->barra), b, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), g->barra, FALSE, FALSE, 0 );
    gtk_widget_set_no_show_all( g->barra, TRUE );

    gtk_widget_show_all( g->win );
    /* la comprobacion, cuando la ventana ya se ha pintado; y un minimo de
       dos segundos y medio para leer los creditos                      */
    g_timeout_add( 150, comprueba, g );
    g_timeout_add( 2500, on_minimo, g );
}

/* ------------------------------------------------------------------------ */
/* AYUDA ▸ «COMPROBAR LA INSTALACION…»: el mismo informe, a mano            */
/* ------------------------------------------------------------------------ */

static void on_respuesta( GtkDialog *d, gint r, gpointer txt )
{
    if ( r == 1 )
        {
        gtk_clipboard_set_text( gtk_clipboard_get( GDK_SELECTION_CLIPBOARD ), txt, -1 );
        return;
        }
    gtk_widget_destroy( GTK_WIDGET(d) );
}

void atsw_informe_instalacion( Atsw *a )
{
    AtComprob  c[AT_MAX_COMPROB];
    int        n = atsw_comprueba( c, AT_MAX_COMPROB ), faltan = atsw_comprueba_faltan( c, n );
    GtkWidget *d, *l, *area, *sw;
    gchar     *m;

    d = gtk_dialog_new_with_buttons( "Comprobar la instalación", GTK_WINDOW(a->ventana),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     "Co_piar el informe", 1, "_Cerrar", GTK_RESPONSE_CLOSE,
                                     NULL );
    gtk_window_set_default_size( GTK_WINDOW(d), 620, 520 );
    area = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(area), 12 );
    l = gtk_label_new( NULL );
    m = faltan ? g_strdup_printf( "<b>Falta%s %d cosa%s.</b> Debajo de cada una, cómo se "
                                  "arregla.", faltan == 1 ? "" : "n", faltan,
                                  faltan == 1 ? "" : "s" )
               : g_strdup( "<b>Todo está instalado.</b> Lo gris es opcional." );
    gtk_label_set_markup( GTK_LABEL(l), m );
    g_free( m );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_box_pack_start( GTK_BOX(area), l, FALSE, FALSE, 6 );
    m = informe( c, n );
    l = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(l), m );
    g_free( m );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_label_set_yalign( GTK_LABEL(l), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    gtk_label_set_selectable( GTK_LABEL(l), TRUE );
    gtk_widget_set_can_focus( l, FALSE );
    sw = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sw), l );
    gtk_box_pack_start( GTK_BOX(area), sw, TRUE, TRUE, 0 );
    /* el texto llano vive lo que el dialogo */
    g_signal_connect_data( d, "response", G_CALLBACK(on_respuesta), informe_llano( c, n ),
                           (GClosureNotify) g_free, 0 );
    gtk_widget_show_all( d );
}
