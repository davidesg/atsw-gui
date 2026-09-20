/*
 * main.c -- atsw_gui, la interfaz madre.
 *
 * El nombre: la FAMILIA se llama atsw y el programa atsw_gui, por el mismo
 * convenio que gui/fue -> fue_gui y gui/drtran -> drtran_gui. Un nombre señala
 * una cosa sola, y esa regla ya costo un renombrado.
 */

#include <string.h>

#include "atsw.h"

void atsw_lanza( Atsw *a, const char *programa );
gboolean atsw_itera( Atsw *a, const char *serie, const char *padre,
                     char *why, size_t n );

static Atsw A;

void barra_pub( Atsw *a, const char *s )
{
    gtk_label_set_text( GTK_LABEL(a->estado), s );
    gtk_widget_set_tooltip_text( a->estado, s );
}

/* ------------------------------------------------------------------------ */

static gchar *marcada( GtkWidget *tv, int columna )
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(tv) );
    GtkTreeModel     *mo;
    GtkTreeIter       it;
    gchar            *s = NULL;

    if ( gtk_tree_selection_get_selected( sel, &mo, &it ) )
        gtk_tree_model_get( mo, &it, columna, &s, -1 );
    return s;
}

static void on_serie( GtkTreeSelection *sel, gpointer d )
{
    Atsw  *a = d;
    gchar *s = marcada( a->l_series, 0 );

    (void) sel;
    snprintf( a->serie, sizeof a->serie, "%s", s ? s : "" );
    g_free( s );
    atsw_refresca( a );
}

static void on_abrir( GtkButton *b, Atsw *a )
{
    GtkWidget *d;

    (void) b;
    d = gtk_file_chooser_dialog_new( "Abrir un proyecto",
            GTK_WINDOW(a->ventana), GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Cancelar", GTK_RESPONSE_CANCEL, "_Abrir", GTK_RESPONSE_ACCEPT,
            NULL );
    {
    GtkFileFilter *f = gtk_file_filter_new();

    gtk_file_filter_set_name( f, "Proyecto (*.yaml)" );
    gtk_file_filter_add_pattern( f, "*.yaml" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(d), f );
    }

    if ( gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT )
        {
        gchar *p = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );
        char   why[512];

        if ( !atsw_abre( a, p, why, sizeof why ) )
            /* UN MANIFIESTO ROTO NO SE PISA: se dice y no se abre. */
            barra_pub( a, why );
        else
            atsw_refresca( a );
        g_free( p );
        }
    gtk_widget_destroy( d );
}

static void on_nuevo( GtkButton *b, Atsw *a )
{
    GtkWidget *d;

    (void) b;
    d = gtk_file_chooser_dialog_new( "Proyecto nuevo",
            GTK_WINDOW(a->ventana), GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Cancelar", GTK_RESPONSE_CANCEL, "_Crear", GTK_RESPONSE_ACCEPT,
            NULL );
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(d), TRUE );
    gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(d), "proyecto.yaml" );

    if ( gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT )
        {
        gchar  *p = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );
        gchar  *base = g_path_get_basename( p );
        PrError e;
        char   *dot;

        if ( !a->p ) a->p = g_new0( Proyecto, 1 );
        dot = strrchr( base, '.' ); if ( dot ) *dot = '\0';
        pr_nuevo( a->p, base, "", "." );
        snprintf( a->p->path, sizeof a->p->path, "%s", p );
        a->hay = TRUE; a->nr = 0; a->serie[0] = '\0';

        if ( pr_escribir( a->p, p, &e ) != 0 )
            { char why[512]; pr_error_es( &e, why, sizeof why );
              barra_pub( a, why ); }
        else
            barra_pub( a, "Proyecto nuevo. Abre una serie en fue para empezar." );

        g_free( base ); g_free( p );
        atsw_refresca( a );
        }
    gtk_widget_destroy( d );
}

static void on_fue( GtkButton *b, Atsw *a )    { (void)b; atsw_lanza( a, "fue_gui" ); }
static void on_fug( GtkButton *b, Atsw *a )    { (void)b; atsw_lanza( a, "gtk_fmg" ); }
static void on_drtran( GtkButton *b, Atsw *a ) { (void)b; atsw_lanza( a, "drtran_gui" ); }

/* Un texto en una linea. Devuelve TRUE si se acepto. */
static gboolean pide_texto( Atsw *a, const char *titulo, const char *aviso,
                            const char *previo, char *out, size_t n )
{
    GtkWidget *d, *caja, *e, *l;
    gboolean   si = FALSE;

    d = gtk_dialog_new_with_buttons( titulo, GTK_WINDOW(a->ventana),
            GTK_DIALOG_MODAL, "_Cancelar", GTK_RESPONSE_CANCEL,
            "_Aceptar", GTK_RESPONSE_ACCEPT, NULL );
    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );

    l = gtk_label_new( aviso );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    gtk_widget_set_halign( l, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX(caja), l, FALSE, FALSE, 4 );

    e = gtk_entry_new();
    gtk_entry_set_width_chars( GTK_ENTRY(e), 56 );
    if ( previo ) gtk_entry_set_text( GTK_ENTRY(e), previo );
    gtk_entry_set_activates_default( GTK_ENTRY(e), TRUE );
    gtk_box_pack_start( GTK_BOX(caja), e, FALSE, FALSE, 4 );
    gtk_dialog_set_default_response( GTK_DIALOG(d), GTK_RESPONSE_ACCEPT );

    gtk_widget_show_all( d );
    if ( gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT )
        { snprintf( out, n, "%s", gtk_entry_get_text( GTK_ENTRY(e) ) ); si = TRUE; }
    gtk_widget_destroy( d );
    return si;
}

static void on_iterar( GtkButton *b, Atsw *a )
{
    gchar *padre = marcada( a->l_modelos, 0 );
    char   why[512];

    (void) b;
    if ( !a->serie[0] ) { barra_pub( a, "Marca una serie." ); return; }
    atsw_itera( a, a->serie, padre ? padre : "", why, sizeof why );
    barra_pub( a, why );
    g_free( padre );
    atsw_refresca( a );
}

static void on_elegir( GtkButton *b, Atsw *a )
{
    gchar  *id = marcada( a->l_modelos, 0 );
    char    razon[PR_RAZON] = "";
    PrError e;

    (void) b;
    if ( !id ) { barra_pub( a, "Marca el modelo que eliges." ); return; }

    if ( pide_texto( a, "El modelo elegido",
            "Por qué éste y no otro. Se puede dejar en blanco: sin razón "
            "se verá como sin razón, que es mejor que una inventada.",
            pr_elegido( a->p, a->serie ), razon, sizeof razon ) )
        {
        if ( pr_elige( a->p, a->serie, id, razon, &e ) != 0 ||
             pr_escribir( a->p, a->p->path, &e ) != 0 )
            { char why[512]; pr_error_es( &e, why, sizeof why );
              barra_pub( a, why ); }
        else
            barra_pub( a, "Elegido declarado y guardado." );
        atsw_refresca( a );
        }
    g_free( id );
}

static void on_razon( GtkButton *b, Atsw *a )
{
    gchar  *id = marcada( a->l_modelos, 0 );
    char    razon[PR_RAZON] = "";
    PrError e;
    int     i;

    (void) b;
    if ( !id ) { barra_pub( a, "Marca la iteración." ); return; }
    i = pr_modelo_idx( a->p, a->serie, id );

    /* SE PIDE, NO SE EXIGE, y se puede poner DESPUES -- mirando el .out, que
     * es cuando de verdad se sabe por que.                              */
    if ( pide_texto( a, "El porqué de esta iteración",
            "Qué te hizo pasar del modelo anterior a éste. Se puede dejar en "
            "blanco y ponerlo más tarde.",
            i >= 0 ? a->p->m[i].razon : "", razon, sizeof razon ) )
        {
        if ( pr_razon( a->p, a->serie, id, razon, &e ) != 0 ||
             pr_escribir( a->p, a->p->path, &e ) != 0 )
            { char why[512]; pr_error_es( &e, why, sizeof why );
              barra_pub( a, why ); }
        else
            barra_pub( a, razon[0] ? "Razón guardada."
                                   : "Sin razón: se seguirá viendo como tal." );
        atsw_refresca( a );
        }
    g_free( id );
}

/* ------------------------------------------------------------------------ */

static void columna( GtkWidget *tv, const char *t, int c )
{
    GtkCellRenderer   *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *k = gtk_tree_view_column_new_with_attributes(
                               t, r, "text", c, NULL );

    gtk_tree_view_column_set_resizable( k, TRUE );
    gtk_tree_view_append_column( GTK_TREE_VIEW(tv), k );
}

static GtkWidget *en_scroll( GtkWidget *w )
{
    GtkWidget *s = gtk_scrolled_window_new( NULL, NULL );

    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(s),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(s), w );
    return s;
}

static GtkWidget *boton( GtkWidget *caja, const char *txt, const char *tip,
                         GCallback cb, Atsw *a )
{
    GtkWidget *b = gtk_button_new_with_label( txt );

    if ( tip ) gtk_widget_set_tooltip_text( b, tip );
    g_signal_connect( b, "clicked", cb, a );
    gtk_box_pack_start( GTK_BOX(caja), b, FALSE, FALSE, 0 );
    return b;
}

static void activate( GtkApplication *app, gpointer d )
{
    Atsw         *a = d;
    GtkWidget    *w, *raiz, *barra, *pan, *izq, *der, *vb;
    GtkListStore *st;

    w = gtk_application_window_new( app );
    a->ventana = w;
    gtk_window_set_title( GTK_WINDOW(w), "ATSW — el taller" );
    gtk_window_set_default_size( GTK_WINDOW(w), 1000, 620 );

    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 8 );
    gtk_container_add( GTK_CONTAINER(w), raiz );

    /* --- la barra --- */
    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );

    boton( barra, "Abrir…", "Un proyecto.yaml. Si está roto se dice y NO se "
                            "abre: nunca se pisa.", G_CALLBACK(on_abrir), a );
    boton( barra, "Nuevo…", NULL, G_CALLBACK(on_nuevo), a );

    a->l_proy = gtk_label_new( "(sin proyecto)" );
    gtk_label_set_ellipsize( GTK_LABEL(a->l_proy), PANGO_ELLIPSIZE_MIDDLE );
    gtk_box_pack_start( GTK_BOX(barra), a->l_proy, TRUE, TRUE, 8 );

    a->b_fue = boton( barra, "fue",
        "El escalón univariante. Se lanza con este proyecto.",
        G_CALLBACK(on_fue), a );
    a->b_fug = boton( barra, "fug", "Los gráficos.", G_CALLBACK(on_fug), a );
    a->b_drtran = boton( barra, "drtran",
        "Las transferencias. Parte de los .pre ya estimados.",
        G_CALLBACK(on_drtran), a );

    /* --- las dos listas --- */
    pan = gtk_paned_new( GTK_ORIENTATION_HORIZONTAL );
    gtk_box_pack_start( GTK_BOX(raiz), pan, TRUE, TRUE, 0 );

    izq = gtk_box_new( GTK_ORIENTATION_VERTICAL, 4 );
    st = gtk_list_store_new( S_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
                             G_TYPE_STRING );
    a->l_series = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( a->l_series, "Serie",   S_ID );
    columna( a->l_series, "Elegido", S_ELEGIDO );
    columna( a->l_series, "Modelos", S_NMOD );
    gtk_widget_set_tooltip_text( a->l_series,
        "El modelo ELEGIDO de cada serie. Hoy esa decisión vive en un "
        "diccionario a pelo repetido en tres guiones de cases/." );
    g_signal_connect( gtk_tree_view_get_selection( GTK_TREE_VIEW(a->l_series) ),
                      "changed", G_CALLBACK(on_serie), a );
    gtk_box_pack_start( GTK_BOX(izq), en_scroll( a->l_series ), TRUE, TRUE, 0 );
    gtk_paned_pack1( GTK_PANED(pan), izq, FALSE, FALSE );

    der = gtk_box_new( GTK_ORIENTATION_VERTICAL, 4 );
    {
    GtkWidget *b2 = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );

    a->b_iterar = boton( b2, "Iterar",
        "Copia el .pre del modelo marcado a un .inp nuevo y registra su "
        "linaje.\n\n.pre e .inp son el mismo formato, pero el motor exige "
        "la extensión .inp: es una copia de verdad. Es el paso que cierra "
        "el ciclo y hasta ahora no lo hacía nadie.",
        G_CALLBACK(on_iterar), a );
    a->b_elegir = boton( b2, "Elegir",
        "Declara que éste es EL modelo de la serie.", G_CALLBACK(on_elegir), a );
    a->b_razon = boton( b2, "Razón…",
        "El porqué de esta iteración. Se puede poner después —mirando el "
        ".out, que es cuando de verdad se sabe— o no ponerse.",
        G_CALLBACK(on_razon), a );
    gtk_box_pack_start( GTK_BOX(der), b2, FALSE, FALSE, 0 );
    }

    st = gtk_list_store_new( M_N, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING );
    a->l_modelos = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    columna( a->l_modelos, "",          M_ESTRELLA );
    columna( a->l_modelos, "Modelo",    M_ID );
    columna( a->l_modelos, "Versión",   M_VER );
    columna( a->l_modelos, "Viene de",  M_PADRE );
    columna( a->l_modelos, "d.t. res.", M_SD );
    columna( a->l_modelos, "Hosking",   M_Q );
    columna( a->l_modelos, "Blancos",   M_BLANCO );
    columna( a->l_modelos, "Por qué",   M_RAZON );
    gtk_widget_set_tooltip_text( a->l_modelos,
        "Los números salen del .out, no del manifiesto, y se releen cuando el "
        "fichero cambia. Cachearlos podría mentir: si alguien reestima por "
        "fuera, el número guardado seguiría diciendo lo de antes.\n\nEl "
        "manifiesto guarda linaje y razón, que son DECISIONES." );
    gtk_box_pack_start( GTK_BOX(der), en_scroll( a->l_modelos ), TRUE, TRUE, 0 );
    gtk_paned_pack2( GTK_PANED(pan), der, TRUE, FALSE );
    gtk_paned_set_position( GTK_PANED(pan), 300 );

    /* --- los dos veredictos, y la barra --- */
    vb = gtk_box_new( GTK_ORIENTATION_VERTICAL, 2 );
    a->ver_cuenta = gtk_label_new( "" );
    a->ver_ojo    = gtk_label_new( "" );
    gtk_widget_set_halign( a->ver_cuenta, GTK_ALIGN_START );
    gtk_widget_set_halign( a->ver_ojo,    GTK_ALIGN_START );
    gtk_label_set_ellipsize( GTK_LABEL(a->ver_cuenta), PANGO_ELLIPSIZE_END );
    gtk_label_set_ellipsize( GTK_LABEL(a->ver_ojo),    PANGO_ELLIPSIZE_END );
    gtk_box_pack_start( GTK_BOX(vb), a->ver_cuenta, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vb), a->ver_ojo,    FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(raiz), vb, FALSE, FALSE, 0 );

    a->estado = gtk_label_new( "" );
    gtk_widget_set_halign( a->estado, GTK_ALIGN_START );
    gtk_label_set_ellipsize( GTK_LABEL(a->estado), PANGO_ELLIPSIZE_END );
    gtk_box_pack_start( GTK_BOX(raiz), a->estado, FALSE, FALSE, 0 );

    atsw_refresca( a );
    gtk_widget_show_all( w );
}

int main( int argc, char **argv )
{
    GtkApplication *app;
    const char     *proy = NULL;
    int             rc, i;

    for ( i = 1; i < argc; i++ )
        {
        if ( !strcmp( argv[i], "--proyecto" ) && i + 1 < argc ) proy = argv[++i];
        else if ( !strncmp( argv[i], "--proyecto=", 11 ) ) proy = argv[i] + 11;
        else if ( !strcmp( argv[i], "-h" ) || !strcmp( argv[i], "--help" ) )
            {
            printf( "uso: %s [--proyecto FICHERO]\n\n"
                    "  ATSW GUI: el taller. Gestiona datos, modelos y\n"
                    "  proyectos, y lanza fue, fug y drtran con el proyecto\n"
                    "  abierto.\n", argv[0] );
            return 0;
            }
        else { fprintf( stderr, "%s: no entiendo «%s»\n", argv[0], argv[i] );
               return 2; }
        }

    if ( proy )
        {
        char why[512];

        if ( !atsw_abre( &A, proy, why, sizeof why ) )
            { fprintf( stderr, "%s: %s\n", proy, why ); return 3; }
        }

    argc = 1;
    app = gtk_application_new( "org.atsw.gui", G_APPLICATION_NON_UNIQUE );
    g_signal_connect( app, "activate", G_CALLBACK(activate), &A );
    rc = g_application_run( G_APPLICATION(app), argc, argv );
    g_object_unref( app );
    return rc;
}
