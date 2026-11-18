/*
 * asistente_ventana.c -- la ventana Asistente, acoplable
 * (docs/ESTUDIO-terminal-mcp.md §7.6).
 *
 * UN SOLO CONTENIDO, a->asis: una barra fina arriba y un GtkStack debajo.
 * Es lo que se mueve; la ventana suelta (a->asis_win) y el lado derecho de la
 * madre (a->fuera, un GtkPaned por fuera de todo lo de siempre) son solo dos
 * sitios donde se pone. Se mueve con g_object_ref, gtk_container_remove,
 * el otro sitio y g_object_unref: nada se destruye al moverlo, y cuando la
 * pagina «terminal» llegue (fase T1, con VTE) su proceso seguira vivo.
 *
 * GTK3 NO TIENE ACOPLAMIENTO, y arrastrar para acoplar seria fragil (la
 * posicion de las ventanas la decide el gestor de ventanas; en Wayland, ni
 * se conoce): por eso un conmutador, «Acoplar a la madre».
 *
 * CERRAR LA VENTANA SUELTA LA ESCONDE. Y su estado --visible o no, suelta o
 * pegada, el tamaño, la posicion y el separador-- es POR PROYECTO y del
 * usuario: va en ventanas.ini (g_get_user_config_dir()/atsw/), nunca en
 * proyecto.yaml, que se comparte.
 *
 * LA PAGINA «ESTADO», la unica hasta que haya terminal empotrada: el boton
 * grande de la terminal del sistema, lo instalado (servidores, uv, clientes),
 * si el proyecto esta preparado, y lo ultimo que ha hecho el asistente,
 * leido del manifiesto (por: asistente) y al dia con vigila.c.
 */

#include <string.h>

#include <glib/gstdio.h>

#include "atsw.h"

#define ANCHO_ACOPLADO 420      /* una terminal pide ~80 columnas        */
#define ULTIMOS        8        /* los pasos del asistente que se enseñan */

/* ------------------------------------------------------------------------ */
/* VENTANAS.INI                                                              */
/* ------------------------------------------------------------------------ */

static gchar *ini_ruta( void )
{
    const char *e = g_getenv( "ATSW_VENTANAS_INI" );

    if ( e && *e ) return g_strdup( e );
    return g_build_filename( g_get_user_config_dir(), "atsw", "ventanas.ini", NULL );
}

/* La seccion: la raiz del proyecto. NULL sin proyecto. */
static gchar *seccion( Atsw *a )
{
    char raiz[PR_RUTA];

    if ( !a->hay || a->p == NULL || pr_raiz( a->p, raiz, sizeof raiz ) != 0 ) return NULL;
    return g_canonicalize_filename( raiz, NULL );
}

void atsw_asistente_guarda_estado( Atsw *a )
{
    gchar    *f = ini_ruta(), *s = seccion( a ), *d;
    GKeyFile *k;

    if ( s == NULL || a->asis == NULL ) { g_free( f ); return; }
    k = g_key_file_new();
    g_key_file_load_from_file( k, f, G_KEY_FILE_KEEP_COMMENTS, NULL );
    /* UN PROYECTO EN EL QUE NUNCA SE ABRIO NO SE APUNTA: abrir proyectos no
       tiene por que ir llenando la configuracion del usuario.          */
    if ( !a->asis_visible && !g_key_file_has_group( k, s ) )
        { g_key_file_free( k ); g_free( f ); g_free( s ); return; }
    g_key_file_set_boolean( k, s, "asistente_visible", a->asis_visible );
    g_key_file_set_boolean( k, s, "asistente_acoplado", a->asis_acoplado );
    if ( a->fuera && a->asis_acoplado && a->asis_visible )
        g_key_file_set_integer( k, s, "asistente_separador",
                                gtk_paned_get_position( GTK_PANED(a->fuera) ) );
    if ( a->asis_win && gtk_widget_get_visible( a->asis_win ) )
        {
        gint x, y, w, h;

        gtk_window_get_position( GTK_WINDOW(a->asis_win), &x, &y );
        gtk_window_get_size( GTK_WINDOW(a->asis_win), &w, &h );
        g_key_file_set_integer( k, s, "asistente_x", x );
        g_key_file_set_integer( k, s, "asistente_y", y );
        g_key_file_set_integer( k, s, "asistente_ancho", w );
        g_key_file_set_integer( k, s, "asistente_alto", h );
        }
    d = g_path_get_dirname( f );
    g_mkdir_with_parents( d, 0700 );
    g_key_file_save_to_file( k, f, NULL );
    g_key_file_free( k );
    g_free( d ); g_free( f ); g_free( s );
}

static int entero( GKeyFile *k, const char *s, const char *c, int def )
{
    GError *e = NULL;
    int     v = g_key_file_get_integer( k, s, c, &e );

    if ( e ) { g_error_free( e ); return def; }
    return v;
}

static gboolean logico( GKeyFile *k, const char *s, const char *c, gboolean def )
{
    GError  *e = NULL;
    gboolean v = g_key_file_get_boolean( k, s, c, &e );

    if ( e ) { g_error_free( e ); return def; }
    return v;
}

/* ------------------------------------------------------------------------ */
/* MOVERLA                                                                   */
/* ------------------------------------------------------------------------ */

static void separador_inicial( Atsw *a, int pos )
{
    int ancho = gtk_widget_get_allocated_width( a->fuera );

    if ( pos <= 0 )
        pos = ancho > ANCHO_ACOPLADO + 300 ? ancho - ANCHO_ACOPLADO : 600;
    gtk_paned_set_position( GTK_PANED(a->fuera), pos );
}

/* Pone el contenido donde dicen asis_acoplado y asis_visible. */
static void coloca( Atsw *a, int sep )
{
    GtkWidget *padre;

    if ( a->asis == NULL || a->fuera == NULL ) return;
    padre = gtk_widget_get_parent( a->asis );

    g_object_ref( a->asis );
    if ( a->asis_acoplado )
        {
        if ( padre != a->fuera )
            {
            if ( padre ) gtk_container_remove( GTK_CONTAINER(padre), a->asis );
            gtk_paned_pack2( GTK_PANED(a->fuera), a->asis, FALSE, FALSE );
            separador_inicial( a, sep );
            }
        gtk_widget_hide( a->asis_win );
        gtk_widget_set_visible( a->asis, a->asis_visible );
        }
    else
        {
        if ( padre != a->asis_win )
            {
            if ( padre ) gtk_container_remove( GTK_CONTAINER(padre), a->asis );
            gtk_container_add( GTK_CONTAINER(a->asis_win), a->asis );
            }
        gtk_widget_show( a->asis );
        gtk_widget_set_visible( a->asis_win, a->asis_visible );
        if ( a->asis_visible ) gtk_window_present( GTK_WINDOW(a->asis_win) );
        }
    g_object_unref( a->asis );

    if ( a->asis_linea )
        gtk_button_set_label( GTK_BUTTON(g_object_get_data( G_OBJECT(a->asis), "conmuta" )),
                              a->asis_acoplado ? "Desacoplar" : "Acoplar" );
    if ( a->asis_visible ) atsw_asistente_refresca( a );
}

gboolean atsw_asistente_visible( Atsw *a )  { return a->asis_visible; }
gboolean atsw_asistente_acoplado( Atsw *a ) { return a->asis_acoplado; }

void atsw_asistente_muestra( Atsw *a, gboolean si )
{
    if ( a->asis == NULL ) return;
    a->asis_visible = si;
    coloca( a, 0 );
    atsw_asistente_guarda_estado( a );
}

void atsw_asistente_acopla( Atsw *a, gboolean si )
{
    if ( a->asis == NULL ) return;
    /* Lo de antes, guardado antes de moverla: el tamaño de la suelta. */
    atsw_asistente_guarda_estado( a );
    a->asis_acoplado = si;
    a->asis_visible  = TRUE;          /* quien la mueve quiere verla */
    coloca( a, 0 );
    atsw_asistente_guarda_estado( a );
}

void atsw_asistente_restaura( Atsw *a )
{
    gchar    *f = ini_ruta(), *s = seccion( a );
    GKeyFile *k = g_key_file_new();
    int       sep = 0;

    if ( a->asis == NULL ) { g_key_file_free( k ); g_free( f ); g_free( s ); return; }
    /* SIN NADA GUARDADO, escondida y acoplada: aparece donde se espera la
       primera vez que se pide.                                         */
    a->asis_visible = FALSE;
    a->asis_acoplado = TRUE;
    if ( s && g_key_file_load_from_file( k, f, G_KEY_FILE_NONE, NULL ) &&
         g_key_file_has_group( k, s ) )
        {
        int w = entero( k, s, "asistente_ancho", 0 ), h = entero( k, s, "asistente_alto", 0 );

        a->asis_visible  = logico( k, s, "asistente_visible", FALSE );
        a->asis_acoplado = logico( k, s, "asistente_acoplado", TRUE );
        sep = entero( k, s, "asistente_separador", 0 );
        if ( w > 0 && h > 0 ) gtk_window_resize( GTK_WINDOW(a->asis_win), w, h );
        if ( g_key_file_has_key( k, s, "asistente_x", NULL ) )
            gtk_window_move( GTK_WINDOW(a->asis_win), entero( k, s, "asistente_x", 0 ),
                             entero( k, s, "asistente_y", 0 ) );
        }
    g_key_file_free( k );
    g_free( f ); g_free( s );
    a->asis_inst_hecha = FALSE;       /* otro proyecto: se vuelve a mirar */
    coloca( a, sep );
}

/* ------------------------------------------------------------------------ */
/* LA PAGINA «ESTADO»                                                        */
/* ------------------------------------------------------------------------ */

void atsw_asistente_refresca( Atsw *a )
{
    GtkListStore *st, *ult;
    GtkTreeIter   it;
    int           n, i, desde;

    if ( a->asis == NULL || !a->asis_visible ) return;

    /* LO INSTALADO pregunta la version a cada servidor: se mira una vez
       por proyecto y con «Comprobar de nuevo», no en cada refresco.     */
    if ( !a->asis_inst_hecha )
        {
        gchar *t = atsw_asistente_informe( a );

        gtk_label_set_text( GTK_LABEL(a->asis_inst), t );
        g_free( t );
        a->asis_inst_hecha = TRUE;
        }

    /* EL PROYECTO, preparado o no: unos stat, en cada refresco. */
    if ( a->hay )
        {
        char   raiz[PR_RUTA] = "";
        gchar *ag = NULL;

        pr_raiz( a->p, raiz, sizeof raiz );
        ag = g_build_filename( raiz, "AGENTS.md", NULL );
        gtk_label_set_markup( GTK_LABEL(a->asis_proy), g_file_test( ag, G_FILE_TEST_EXISTS )
            ? "El proyecto está <b>preparado</b> para el asistente (AGENTS.md y la "
              "configuración de los clientes)."
            : "El proyecto <b>no está preparado</b>: «Preparar…» escribe AGENTS.md y la "
              "configuración de cada cliente." );
        g_free( ag );
        }
    else
        gtk_label_set_text( GTK_LABEL(a->asis_proy), "No hay proyecto abierto." );
    /* «Preparar…» como en el menu: sin servidores, la configuracion
       apuntaria a programas que no estan.                           */
    {
    GtkWidget *b = g_object_get_data( G_OBJECT(a->asis), "preparar" );
    gboolean   inst = atsw_asistente_instalado();

    gtk_widget_set_sensitive( b, a->hay && inst );
    gtk_widget_set_tooltip_text( b, !a->hay ? "Abre un proyecto antes."
        : !inst ? "El asistente no está instalado: Asistente ▸ Instalar el asistente…"
        : "Asistente ▸ Preparar el proyecto para el asistente…" );
    }

    /* LO ULTIMO DEL ASISTENTE: los pasos del manifiesto, los mas nuevos
       arriba (el manifiesto los tiene en el orden en que se dieron).    */
    st = gtk_list_store_new( QH_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING );
    n  = a->hay ? atsw_asistente_pasos( a->p, st ) : 0;
    ult = gtk_list_store_new( QH_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                              G_TYPE_STRING, G_TYPE_STRING );
    desde = n > ULTIMOS ? n - ULTIMOS : 0;
    for ( i = n - 1; i >= desde; i-- )
        {
        GtkTreeIter  src;
        gchar       *v[QH_COLS];
        int          c;

        if ( !gtk_tree_model_iter_nth_child( GTK_TREE_MODEL(st), &src, NULL, i ) ) continue;
        for ( c = 0; c < QH_COLS; c++ ) gtk_tree_model_get( GTK_TREE_MODEL(st), &src, c, &v[c], -1 );
        gtk_list_store_append( ult, &it );
        gtk_list_store_set( ult, &it, QH_SERIE, v[QH_SERIE], QH_MUESTRA, v[QH_MUESTRA],
                            QH_MODELO, v[QH_MODELO], QH_QUE, v[QH_QUE], QH_RAZON, v[QH_RAZON], -1 );
        for ( c = 0; c < QH_COLS; c++ ) g_free( v[c] );
        }
    gtk_tree_view_set_model( GTK_TREE_VIEW(a->asis_pasos), GTK_TREE_MODEL(ult) );
    g_object_unref( ult );
    g_object_unref( st );

    {
    gchar *t = !a->hay ? g_strdup( "Sin proyecto." )
             : n ? g_strdup_printf( "%d paso%s del asistente en este proyecto.", n, n == 1 ? "" : "s" )
             : g_strdup( "Aún no ha hecho nada en este proyecto." );
    gtk_label_set_text( GTK_LABEL(a->asis_linea), t );
    g_free( t );
    }
}

static void on_conmuta( GtkButton *b, Atsw *a )
     { (void) b; atsw_asistente_acopla( a, !a->asis_acoplado ); }
static void on_esconde( GtkButton *b, Atsw *a )
     { (void) b; atsw_asistente_muestra( a, FALSE ); }
static void on_terminal_b( GtkButton *b, Atsw *a )
     { (void) b; atsw_asistente_terminal( a ); }
static void on_prepara_b( GtkButton *b, Atsw *a )
     { (void) b; atsw_asistente_preparar( a ); atsw_asistente_refresca( a ); }
static void on_todo_b( GtkButton *b, Atsw *a )
     { (void) b; atsw_asistente_que_ha_hecho( a ); }
static void on_otra_vez( GtkButton *b, Atsw *a )
     { (void) b; a->asis_inst_hecha = FALSE; atsw_asistente_refresca( a ); }

/* Cerrar la suelta la esconde: la sesion es del contenido, no de la ventana. */
static gboolean on_cierra_suelta( GtkWidget *w, GdkEvent *e, Atsw *a )
{
    (void) w; (void) e;
    atsw_asistente_muestra( a, FALSE );
    return TRUE;
}

static GtkWidget *titulo( const char *t )
{
    GtkWidget *l = gtk_label_new( NULL );
    gchar     *m = g_markup_printf_escaped( "<b>%s</b>", t );

    gtk_label_set_markup( GTK_LABEL(l), m );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_widget_set_margin_top( l, 8 );
    g_free( m );
    return l;
}

static GtkWidget *pagina_estado( Atsw *a )
{
    GtkWidget       *sc = gtk_scrolled_window_new( NULL, NULL );
    GtkWidget       *v = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    GtkWidget       *b, *h, *sp;
    GtkCellRenderer *r;
    static const char *const tit[] = { "Serie", "Muestra", "Modelo", "Qué", "Razón" };
    int              i;

    gtk_container_set_border_width( GTK_CONTAINER(v), 10 );
    gtk_container_add( GTK_CONTAINER(sc), v );

    /* EL BOTON GRANDE: sin terminal empotrada, es lo primero (§7.6). */
    b = gtk_button_new_with_label( "Abrir la terminal del sistema" );
    gtk_widget_set_tooltip_text( b, "En la carpeta del proyecto. Ahí se lanza el cliente: "
                                    "claude, gemini, codex o codewhale." );
    gtk_widget_set_size_request( b, -1, 44 );
    g_signal_connect( b, "clicked", G_CALLBACK(on_terminal_b), a );
    gtk_box_pack_start( GTK_BOX(v), b, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(v), titulo( "El proyecto" ), FALSE, FALSE, 0 );
    a->asis_proy = gtk_label_new( "" );
    gtk_label_set_line_wrap( GTK_LABEL(a->asis_proy), TRUE );
    gtk_label_set_xalign( GTK_LABEL(a->asis_proy), 0.0 );
    gtk_box_pack_start( GTK_BOX(v), a->asis_proy, FALSE, FALSE, 0 );
    h = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    b = gtk_button_new_with_label( "Preparar…" );
    gtk_widget_set_tooltip_text( b, "Asistente ▸ Preparar el proyecto para el asistente…" );
    g_signal_connect( b, "clicked", G_CALLBACK(on_prepara_b), a );
    g_object_set_data( G_OBJECT(a->asis), "preparar", b );
    gtk_box_pack_start( GTK_BOX(h), b, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(v), h, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(v), titulo( "Lo último del asistente" ), FALSE, FALSE, 0 );
    a->asis_pasos = gtk_tree_view_new();
    for ( i = 0; i < QH_COLS; i++ )
        {
        r = gtk_cell_renderer_text_new();
        if ( i == QH_RAZON )
            g_object_set( r, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
        gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(a->asis_pasos), -1, tit[i],
                                                     r, "text", i, NULL );
        }
    gtk_tree_view_column_set_expand( gtk_tree_view_get_column( GTK_TREE_VIEW(a->asis_pasos),
                                                               QH_RAZON ), TRUE );
    sp = gtk_scrolled_window_new( NULL, NULL );
    gtk_widget_set_size_request( sp, -1, 170 );
    gtk_container_add( GTK_CONTAINER(sp), a->asis_pasos );
    gtk_box_pack_start( GTK_BOX(v), sp, FALSE, FALSE, 0 );
    b = gtk_button_new_with_label( "Todo lo que ha hecho…" );
    g_signal_connect( b, "clicked", G_CALLBACK(on_todo_b), a );
    h = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(h), b, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(v), h, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(v), titulo( "La instalación" ), FALSE, FALSE, 0 );
    a->asis_inst = gtk_label_new( "" );
    gtk_label_set_selectable( GTK_LABEL(a->asis_inst), TRUE );
    gtk_label_set_xalign( GTK_LABEL(a->asis_inst), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(a->asis_inst), TRUE );
    {
    PangoAttrList *at = pango_attr_list_new();
    pango_attr_list_insert( at, pango_attr_family_new( "monospace" ) );
    pango_attr_list_insert( at, pango_attr_scale_new( PANGO_SCALE_SMALL ) );
    gtk_label_set_attributes( GTK_LABEL(a->asis_inst), at );
    pango_attr_list_unref( at );
    }
    gtk_box_pack_start( GTK_BOX(v), a->asis_inst, FALSE, FALSE, 0 );
    b = gtk_button_new_with_label( "Comprobar de nuevo" );
    g_signal_connect( b, "clicked", G_CALLBACK(on_otra_vez), a );
    h = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(h), b, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(v), h, FALSE, FALSE, 0 );
    return sc;
}

/* EL CONTENIDO Y SUS DOS SITIOS. Devuelve el GtkPaned de fuera, con madre
 * a la izquierda; quien llama lo pone donde iba madre.                  */
GtkWidget *atsw_asistente_fuera( Atsw *a, GtkWidget *madre )
{
    GtkWidget *barra, *b, *pila;

    a->fuera = gtk_paned_new( GTK_ORIENTATION_HORIZONTAL );
    gtk_paned_pack1( GTK_PANED(a->fuera), madre, TRUE, FALSE );

    a->asis = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_widget_set_size_request( a->asis, 320, -1 );
    gtk_widget_set_name( a->asis, "atsw-asistente" );

    /* LA BARRA FINA: el nombre y los dos gestos de la ventana; debajo, el
       estado en una linea, que acoplada no cabria al lado de los botones. */
    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(barra), 4 );
    {
    GtkWidget *t = gtk_label_new( NULL );

    gtk_label_set_markup( GTK_LABEL(t), "<b>Asistente</b>" );
    gtk_box_pack_start( GTK_BOX(barra), t, FALSE, FALSE, 4 );
    }
    a->asis_linea = gtk_label_new( "" );
    gtk_label_set_ellipsize( GTK_LABEL(a->asis_linea), PANGO_ELLIPSIZE_END );
    gtk_label_set_xalign( GTK_LABEL(a->asis_linea), 0.0 );
    gtk_widget_set_margin_start( a->asis_linea, 8 );
    gtk_widget_set_margin_bottom( a->asis_linea, 4 );
    b = gtk_button_new_with_label( "Desacoplar" );
    gtk_widget_set_tooltip_text( b, "Suelta, en su propia ventana; o pegada al lado derecho "
                                    "de la madre. Lo que tenga dentro no se pierde." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_conmuta), a );
    g_object_set_data( G_OBJECT(a->asis), "conmuta", b );
    gtk_box_pack_end( GTK_BOX(barra), b, FALSE, FALSE, 0 );
    b = gtk_button_new_with_label( "Esconder" );
    gtk_widget_set_tooltip_text( b, "Asistente ▸ Ventana del asistente la vuelve a enseñar." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_esconde), a );
    gtk_box_pack_end( GTK_BOX(barra), b, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(a->asis), barra, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(a->asis), a->asis_linea, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(a->asis), gtk_separator_new( GTK_ORIENTATION_HORIZONTAL ),
                        FALSE, FALSE, 0 );

    /* LA PILA: «estado» hoy; «terminal» cuando haya terminal empotrada. */
    pila = gtk_stack_new();
    gtk_stack_add_named( GTK_STACK(pila), pagina_estado( a ), "estado" );
    gtk_box_pack_start( GTK_BOX(a->asis), pila, TRUE, TRUE, 0 );
    a->asis_estado = pila;
    gtk_widget_show_all( a->asis );

    /* LA SUELTA, sin contenido hasta que se suelte. */
    a->asis_win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_title( GTK_WINDOW(a->asis_win), "ATSW — Asistente" );
    gtk_window_set_default_size( GTK_WINDOW(a->asis_win), 520, 640 );
    gtk_window_set_transient_for( GTK_WINDOW(a->asis_win), GTK_WINDOW(a->ventana) );
    g_signal_connect( a->asis_win, "delete-event", G_CALLBACK(on_cierra_suelta), a );

    /* Acoplada y escondida hasta que se lea el estado del proyecto. */
    a->asis_acoplado = TRUE;
    a->asis_visible  = FALSE;
    gtk_paned_pack2( GTK_PANED(a->fuera), a->asis, FALSE, FALSE );
    gtk_widget_set_no_show_all( a->asis, TRUE );
    gtk_widget_hide( a->asis );
    return a->fuera;
}
