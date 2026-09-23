/*
 * ventana.c -- la ventana de fuf: prever, la tabla y el gráfico.
 *
 * EL CICLO ES DE DOS MOTORES Y SE VE ENTERO EN LA CONSOLA:
 *
 *     fue <modelo> -f      escribe forecast_<modelo>.inp
 *     fuf forecast_<modelo>  escribe su .out, su .pdf y su .eps
 *
 * Los dos corren en EL DIRECTORIO DEL MODELO, así que todo cae al lado del
 * modelo. Eso es la respuesta a «dónde se guardan las previsiones»: donde
 * está lo que las produjo.
 */

#include <string.h>

#include <glib/gstdio.h>

#include "fufgui.h"

#include "rutas.h"

/* Los ganchos que lib/preview pide al anfitrión. */
void preview_open_external( PreviewApp *app, const gchar *path )
{
    gchar *uri = g_filename_to_uri( path, NULL, NULL );

    (void) app;
    if ( uri ) { g_app_info_launch_default_for_uri( uri, NULL, NULL );
                 g_free( uri ); }
}

void preview_show_status( PreviewApp *app, const gchar *format, ... )
{
    va_list ap;
    gchar  *s;

    va_start( ap, format );
    s = g_strdup_vprintf( format, ap );
    va_end( ap );
    fuf_di( (Fuf *) app, "%s", s );
    g_free( s );
}


/* ------------------------------------------------------------------------ */
/* Prever                                                                    */
/* ------------------------------------------------------------------------ */

static gchar *donde_esta( const char *programa )
{
    static const char *sitio[] = {         /* relativos a gui/fuf/ */
        "../../engines/fuf/bin/%s", "../../engines/fue/bin/%s", "./%s", NULL
    };
    gchar *mio = g_file_read_link( "/proc/self/exe", NULL );
    gchar *dir = mio ? g_path_get_dirname( mio ) : NULL;
    int    i;

    g_free( mio );
    for ( i = 0; dir && sitio[i]; i++ )
        {
        gchar *rel = g_strdup_printf( sitio[i], programa );
        gchar *p   = g_build_filename( dir, rel, NULL );

        g_free( rel );
        if ( g_file_test( p, G_FILE_TEST_IS_EXECUTABLE ) )
            { g_free( dir ); return p; }
        g_free( p );
        }
    g_free( dir );
    return g_find_program_in_path( programa );
}

static void corriendo( Fuf *f, gboolean si )
{
    gtk_widget_set_sensitive( f->b_prever, !si );
    gtk_widget_set_sensitive( f->horizonte, !si );
}

/* EL INFORME ES EL PDF, y existe en cuanto fuf corre con pdflatex.
 *
 * Lo tenia bien el modulo de antes y yo lo reinvente peor: puse un boton
 * "Grafico" que buscaba el EPS. El EPS es SOLO el dibujo; el PDF es el
 * INFORME DE PREVISION completo --la tabla, el grafico y lo que el motor
 * escribe alrededor-- que es lo que uno quiere ver despues de prever.   */
static void informe_hay( Fuf *f )
{
    gchar   *pdf = g_strdup_printf( "%s/%s.pdf", f->dir, f->prev );
    gboolean hay = g_file_test( pdf, G_FILE_TEST_EXISTS );

    g_free( pdf );
    gtk_widget_set_sensitive( f->b_grafico, hay );
}

static void sale( const char *txt, gsize n, gpointer d )
{
    Fuf           *f = d;
    GtkTextBuffer *b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(f->consola) );
    GtkTextIter    fin;

    gtk_text_buffer_get_end_iter( b, &fin );
    gtk_text_buffer_insert( b, &fin, txt, (gint) n );
}

static void acabo_fuf( const EngineResult *r, gpointer d )
{
    Fuf *f = d;

    f->job = NULL;
    corriendo( f, FALSE );
    fuf_consola( f, "\n%s\n\n", r->message ? r->message : "terminó" );

    if ( engine_wrote_results( r ) )
        {
        fuf_trae_out( f );
        informe_hay( f );
        /* La previsión es lo que se iba a mirar: se pone delante sola. */
        gtk_notebook_set_current_page( GTK_NOTEBOOK(f->libro), 0 );
        }
    else fuf_di( f, "%s", r->message ? r->message : "fuf no terminó." );
}

static void on_prever( GtkButton *b, Fuf *f )
{
    gchar      *exe;
    char        inp[FG_RUTA], why[512];
    int         h;
    const char *argv[3];

    (void) b;
    if ( f->job || !f->base[0] ) return;

    /* --- 1. fue -f escribe el .inp de la previsión ---------------------- */
    exe = donde_esta( "fue" );
    if ( exe == NULL )
        { fuf_di( f, "No encuentro «fue»: ni al lado de esta ventana ni en "
                     "el PATH." ); return; }

    fuf_consola( f, "$ cd %s\n$ %s %s -f\n", f->dir, exe, f->base );
    argv[0] = f->base; argv[1] = "-f"; argv[2] = NULL;
    {
    EngineResult r = engine_run( f->dir, exe, f->base, "-f", NULL );

    if ( r.output ) fuf_consola( f, "%s", r.output );
    if ( !engine_wrote_results( &r ) )
        { fuf_di( f, "%s", r.message ); engine_result_clear( &r );
          g_free( exe ); return; }
    engine_result_clear( &r );
    }
    g_free( exe );

    /* --- 2. EL HORIZONTE, EN EL FICHERO --------------------------------- */
    g_snprintf( inp, sizeof inp, "%s/%s.inp", f->dir, f->prev );
    h = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(f->horizonte) );
    if ( fuf_pon_horizonte( inp, h, why, sizeof why ) != 0 )
        { fuf_di( f, "%s", why ); return; }
    fuf_consola( f, "* horizonte %d, escrito en %s.inp\n", h, f->prev );

    /* --- 3. fuf ---------------------------------------------------------- */
    exe = donde_esta( "fuf" );
    if ( exe == NULL )
        { fuf_di( f, "No encuentro «fuf»." ); return; }

    fuf_consola( f, "$ %s %s\n", exe, f->prev );
    argv[0] = f->prev; argv[1] = NULL;
    corriendo( f, TRUE );
    fuf_di( f, "Previendo %d períodos…", h );
    f->job = engine_start( f->dir, exe, argv, NULL, sale, acabo_fuf, f );
    if ( f->job == NULL )
        { corriendo( f, FALSE ); fuf_di( f, "No pude lanzar %s.", exe ); }
    g_free( exe );
}

static void on_informe( GtkButton *b, Fuf *f )
{
    gchar *pdf = g_strdup_printf( "%s/%s.pdf", f->dir, f->prev );

    (void) b;
    if ( !g_file_test( pdf, G_FILE_TEST_EXISTS ) )
        {
        fuf_di( f, "Todavía no hay informe: sale al prever." );
        g_free( pdf );
        return;
        }

    /* Primero la ventana de gráficos, y si no puede con él, el visor del
       sistema. Es lo que hacía el módulo de antes, y por una razón: el PDF
       lo escribe pdflatex y no fugdraw, así que lib/preview puede no saber
       leerlo -- no por estar roto, sino por no ser suyo.               */
    if ( preview_show( (PreviewApp *) f, pdf ) )
        fuf_di( f, "El informe de previsión." );
    else
        {
        preview_open_external( (PreviewApp *) f, pdf );
        fuf_di( f, "El informe, en el visor del sistema." );
        }
    g_free( pdf );
}

static void on_abrir( GtkButton *b, Fuf *f )
{
    GtkWidget     *d;
    GtkFileFilter *fl;

    (void) b;
    d = gtk_file_chooser_dialog_new( "Abrir un modelo", GTK_WINDOW(f->ventana),
            GTK_FILE_CHOOSER_ACTION_OPEN, "Cancelar", GTK_RESPONSE_CANCEL,
            "Abrir", GTK_RESPONSE_ACCEPT, NULL );
    fl = gtk_file_filter_new();
    gtk_file_filter_set_name( fl, "Modelos (*.pre *.inp)" );
    gtk_file_filter_add_pattern( fl, "*.pre" );
    gtk_file_filter_add_pattern( fl, "*.inp" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(d), fl );
    if ( f->dir[0] )
        gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(d), f->dir );

    if ( gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT )
        {
        gchar *p = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );
        gchar *dir = g_path_get_dirname( p );
        gchar *bn  = g_path_get_basename( p );

        g_snprintf( f->inp, sizeof f->inp, "%s", p );
        g_snprintf( f->dir, sizeof f->dir, "%s", dir );
        ruta_nombre( bn, f->base, sizeof f->base );
        g_snprintf( f->prev, sizeof f->prev, "forecast_%s", f->base );
        g_free( p ); g_free( dir ); g_free( bn );

        gtk_label_set_text( GTK_LABEL(f->l_modelo), f->base );
        gtk_widget_set_sensitive( f->b_prever, TRUE );
        fuf_trae_out( f );
        informe_hay( f );
        fuf_consola( f, "* %s\n", f->inp );
        }
    gtk_widget_destroy( d );
}


/* ------------------------------------------------------------------------ */
/* La ventana                                                                */
/* ------------------------------------------------------------------------ */

static void columna( GtkWidget *tv, const char *t, int c )
{
    GtkCellRenderer *r = gtk_cell_renderer_text_new();

    g_object_set( r, "family", "monospace", NULL );
    gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(tv), -1, t, r,
                                                 "text", c, NULL );
}

static GtkWidget *monoespaciado( void )
{
    GtkWidget      *tv = gtk_text_view_new();
    GtkCssProvider *css = gtk_css_provider_new();

    /* REJILLA DE VERDAD: el informe del motor es una tabla dibujada con
       espacios, y con tipografia proporcional deja de estar alineada. */
    gtk_text_view_set_editable( GTK_TEXT_VIEW(tv), FALSE );
    gtk_css_provider_load_from_data( css,
        "textview { font-family: monospace; font-size: 10pt; }", -1, NULL );
    gtk_style_context_add_provider( gtk_widget_get_style_context( tv ),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );
    g_object_unref( css );
    return tv;
}

static GtkWidget *en_scroll( GtkWidget *w )
{
    GtkWidget *s = gtk_scrolled_window_new( NULL, NULL );

    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(s),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(s), w );
    return s;
}

GtkWidget *fuf_ventana_nueva( GtkApplication *app, Fuf *f )
{
    GtkWidget    *raiz, *barra, *pan, *b;
    GtkListStore *st;

    f->ventana = gtk_application_window_new( app );
    gtk_window_set_title( GTK_WINDOW(f->ventana), "fuf — previsión" );
    gtk_window_set_default_size( GTK_WINDOW(f->ventana), 760, 640 );

    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 8 );
    gtk_container_add( GTK_CONTAINER(f->ventana), raiz );

    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );

    b = gtk_button_new_with_label( "Abrir…" );
    gtk_widget_set_tooltip_text( b, "Un .pre o un .inp. Los ficheros de la "
        "previsión se escriben AL LADO del modelo." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_abrir), f );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(barra), gtk_label_new( "Horizonte" ),
                        FALSE, FALSE, 4 );
    f->horizonte = gtk_spin_button_new_with_range( 1, 120, 1 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(f->horizonte), 24 );
    gtk_widget_set_tooltip_text( f->horizonte,
        "Cuántos períodos hacia delante. Se escribe EN EL .inp de la "
        "previsión, que es donde el motor lo lee: la especificación vive en "
        "el fichero, no en la línea de órdenes." );
    gtk_box_pack_start( GTK_BOX(barra), f->horizonte, FALSE, FALSE, 0 );

    f->b_prever = b = gtk_button_new_with_label( "Prever" );
    gtk_widget_set_tooltip_text( b,
        "Corre «fue -f» para escribir el .inp de la previsión y después "
        "«fuf» sobre él. Los dos en el directorio del modelo." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_prever), f );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    f->b_grafico = b = gtk_button_new_with_label( "Informe…" );
    gtk_widget_set_tooltip_text( b,
        "El informe de previsión completo, en PDF: la tabla, el gráfico y lo "
        "que el motor escribe alrededor. Lo hace pdflatex al prever." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_informe), f );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    f->l_modelo = gtk_label_new( f->base[0] ? f->base : "(sin modelo)" );
    gtk_label_set_ellipsize( GTK_LABEL(f->l_modelo), PANGO_ELLIPSIZE_MIDDLE );
    gtk_box_pack_start( GTK_BOX(barra), f->l_modelo, TRUE, TRUE, 8 );

    pan = gtk_paned_new( GTK_ORIENTATION_VERTICAL );
    gtk_box_pack_start( GTK_BOX(raiz), pan, TRUE, TRUE, 0 );

    /* ARRIBA LO QUE SE MIRA, ABAJO LO QUE PASA -- la misma forma que el
     * editor del .inp, y por la misma razón: la tabla y el informe son dos
     * vistas de LO MISMO, así que se alternan; la consola es otra cosa y
     * tiene que verse a la vez que cualquiera de las dos.              */
    f->libro = gtk_notebook_new();
    gtk_paned_pack1( GTK_PANED(pan), f->libro, TRUE, FALSE );

    st = gtk_list_store_new( FC_N, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );
    f->tabla = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    g_object_unref( st );
    columna( f->tabla, "Fecha",   FC_FECHA );
    columna( f->tabla, "Nivel",   FC_NIVEL );
    columna( f->tabla, "d.t.",    FC_SD );
    columna( f->tabla, "Var.",    FC_VAR );
    columna( f->tabla, "Anual",   FC_ANUAL );
    gtk_widget_set_tooltip_text( f->tabla,
        "SÓLO lo previsto, destilado. El motor imprime también las "
        "observaciones anteriores al origen para que el gráfico empalme; "
        "ponerlas aquí sería decir que se previeron.\n\nEl informe entero "
        "está en la otra pestaña.\n\nLas bandas son TEÓRICAS: dicen lo que "
        "el modelo implica si el modelo es cierto." );
    gtk_notebook_append_page( GTK_NOTEBOOK(f->libro),
                              en_scroll( f->tabla ),
                              gtk_label_new( "Previsión" ) );

    /* EL INFORME ENTERO, tal cual lo escribió el motor. La tabla resume, y
     * resumir pierde: la cabecera, los parámetros con que previó, la
     * columna de error y las observaciones con que empalma están aquí. */
    f->salida = monoespaciado();
    gtk_widget_set_tooltip_text( f->salida,
        "El informe de fuf, sin resumir: lo que se lee aquí es lo que hay en "
        "el .out." );
    gtk_notebook_append_page( GTK_NOTEBOOK(f->libro),
                              en_scroll( f->salida ),
                              gtk_label_new( "Output" ) );

    f->consola = monoespaciado();
    gtk_widget_set_tooltip_text( f->consola,
        "Las dos órdenes, con su directorio, y lo que los motores van "
        "diciendo. No se borra entre corridas." );
    gtk_paned_pack2( GTK_PANED(pan), en_scroll( f->consola ), FALSE, TRUE );
    gtk_paned_set_position( GTK_PANED(pan), 400 );

    f->estado = gtk_label_new( "" );
    gtk_label_set_xalign( GTK_LABEL(f->estado), 0.0 );
    gtk_label_set_ellipsize( GTK_LABEL(f->estado), PANGO_ELLIPSIZE_END );
    gtk_box_pack_start( GTK_BOX(raiz), f->estado, FALSE, FALSE, 0 );

    if ( f->base[0] )
        {
        fuf_consola( f, "* %s\n", f->inp );
        fuf_trae_out( f );
        informe_hay( f );
        }
    else
        {
        gtk_widget_set_sensitive( f->b_prever, FALSE );
        gtk_widget_set_sensitive( f->b_grafico, FALSE );
        fuf_di( f, "Abre un modelo: un .pre o un .inp." );
        }
    return f->ventana;
}
