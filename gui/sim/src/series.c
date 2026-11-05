/*
 * series.c -- la pagina 1: las series del sistema, en orden (nodo N0 de
 * sima, load_pre).
 *
 * Cada fila es un fichero de fue con su modelo univariante entero: lo que el
 * VARMA conserva en su DIAGONAL. El ORDEN importa: es el de Cholesky de la
 * respuesta al impulso y de la descomposicion de la varianza.
 *
 * Lo que se lee y la ventana comun salen de lib/sistema, que lee con el
 * lector de drvarma: lo que la pagina acepta es lo que acepta el motor.
 */

#include <string.h>

/* Antes que GTK: main.h de drvarma define TRUE y FALSE sin mirar, y glib
 * solo si no estan.                                                    */
#include "fue_pre_reader.h"
#include "sim.h"

/* El lector de .pre (lib/fuepre, con el nlatools de drvarma) usa macheps, y
 * en el motor la define su main con cmacheps(). El GUI no estima: basta el
 * de la doble precision (como drtran_gui, series.c). lib/sistema, ENLACE. */
real macheps = 2.220446049250313e-16;

enum { C_NUM, C_SERIE, C_FICH, C_OBS, C_VENTANA, C_LAMBDA, C_DIF, C_AR, C_MA, C_RECORTE, C_N };

static int marcada( Sim *m )
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(m->ps.lista) );
    GtkTreeModel *mod;
    GtkTreeIter it;
    int i = -1;

    if (gtk_tree_selection_get_selected( sel, &mod, &it ))
        gtk_tree_model_get( mod, &it, C_NUM, &i, -1 );
    return i - 1;
}

static void marca( Sim *m, int i )
{
    GtkTreePath *p = gtk_tree_path_new_from_indices( i, -1 );
    gtk_tree_selection_select_path( gtk_tree_view_get_selection( GTK_TREE_VIEW(m->ps.lista) ), p );
    gtk_tree_path_free( p );
}

gboolean series_anade( Sim *m, const char *ruta_dada )
{
    char why[1024];
    /* ABSOLUTA: drvarma se lanza en el directorio de la salida, no aqui. */
    gchar *ruta = g_canonicalize_filename( ruta_dada, NULL );
    int r = sis_anade( &m->sis, ruta, why, sizeof why );

    g_free( ruta );
    if (r != 0) {
        sim_estado( m, "%s", why );
        return FALSE;
    }
    sim_refresca( m );
    sim_estado( m, "%s entra como serie %d.", m->sis.s[m->sis.n - 1].nombre, m->sis.n );
    return TRUE;
}

static void on_abrir( GtkButton *b, Sim *m )
{
    GtkWidget *d = gtk_file_chooser_dialog_new( "Abrir los ficheros de fue de las series",
        GTK_WINDOW(m->ventana), GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancelar", GTK_RESPONSE_CANCEL, "_Abrir", GTK_RESPONSE_ACCEPT, NULL );
    GtkFileFilter *f = gtk_file_filter_new();

    (void) b;
    gtk_file_filter_set_name( f, "Ficheros de fue (.pre, .inp)" );
    gtk_file_filter_add_pattern( f, "*.pre" );
    gtk_file_filter_add_pattern( f, "*.inp" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(d), f );
    gtk_file_chooser_set_select_multiple( GTK_FILE_CHOOSER(d), TRUE );
    if (gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT) {
        GSList *l = gtk_file_chooser_get_filenames( GTK_FILE_CHOOSER(d) ), *p;
        for (p = l; p; p = p->next) series_anade( m, p->data );
        g_slist_free_full( l, g_free );
    }
    gtk_widget_destroy( d );
}

static void on_quitar( GtkButton *b, Sim *m )
{
    int i = marcada( m );
    (void) b;
    if (i < 0) { sim_estado( m, "Marca la serie que quieres quitar." ); return; }
    sim_estado( m, "%s sale del sistema.", m->sis.s[i].nombre );
    sis_quita( &m->sis, i );
    sim_refresca( m );
}

static void mueve( Sim *m, int paso )
{
    int i = marcada( m ), j = i + paso;

    if (i < 0) { sim_estado( m, "Marca la serie que quieres mover." ); return; }
    if (j < 0 || j >= m->sis.n) return;
    sis_mueve( &m->sis, i, j );
    sim_refresca( m );
    marca( m, j );
    sim_estado( m, "%s pasa a ser la serie %d: el orden de Cholesky cambia.",
                m->sis.s[j].nombre, j + 1 );
}

static void on_subir( GtkButton *b, Sim *m ) { (void) b; mueve( m, -1 ); }
static void on_bajar( GtkButton *b, Sim *m ) { (void) b; mueve( m, +1 ); }

void series_refresca( Sim *m )
{
    GtkListStore *st;
    SisVentana v;
    char why[1024], a[32], b[32];
    int i, hay;
    gboolean con_caso = m->caso[0] != 0;

    if (!m->ps.lista) return;
    hay = sis_ventana( &m->sis, &v, why, sizeof why ) == 0;

    st = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(m->ps.lista) ));
    m->recolocando = TRUE;
    gtk_list_store_clear( st );
    for (i = 0; i < m->sis.n; i++) {
        const SisSerie *x = &m->sis.s[i];
        GtkTreeIter it;
        gchar *base = g_path_get_basename( x->ruta );
        gchar *ven, *lam, *dif, *rec;
        char d1[32], d2[32];

        sis_fecha( x->freq, (long) x->ini_anio * x->freq + x->ini_per - 1, d1, sizeof d1 );
        sis_fecha( x->freq, (long) x->fin_anio * x->freq + x->fin_per - 1, d2, sizeof d2 );
        ven = g_strdup_printf( "%s – %s", d1, d2 );
        lam = g_strdup_printf( "%g", x->lambda );
        dif = x->D ? g_strdup_printf( "d=%d D=%d", x->d, x->D ) : g_strdup_printf( "d=%d", x->d );
        rec = !hay ? g_strdup( "" )
            : v.recorta[i] ? g_strdup_printf( "pierde %d", v.recorta[i] ) : g_strdup( "" );
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it, C_NUM, i + 1, C_SERIE, x->nombre,
                            C_FICH, base, C_OBS, x->nobs, C_VENTANA, ven, C_LAMBDA, lam,
                            C_DIF, dif, C_AR, x->ar, C_MA, x->ma, C_RECORTE, rec, -1 );
        g_free( base ); g_free( ven ); g_free( lam ); g_free( dif ); g_free( rec );
    }
    m->recolocando = FALSE;

    if (m->sis.n < 2)
        sim_veredicto( m->ps.ver_ventana, SIM_AMBAR,
                       "Un sistema necesita al menos dos series: Abrir… sus .pre, en orden." );
    else if (!hay)
        sim_veredicto( m->ps.ver_ventana, SIM_ROJO, "Sin ventana común: %s", why );
    else {
        sis_fecha( v.freq, v.desde, a, sizeof a );
        sis_fecha( v.freq, v.hasta, b, sizeof b );
        sim_veredicto( m->ps.ver_ventana, SIM_VERDE,
                       "Ventana común: %s – %s, %d observaciones estacionarias.", a, b, v.nobs );
    }

    if (m->sis.n >= 2)
        sim_veredicto( m->ps.ver_orden, SIM_VERDE,
                       "El orden es el de Cholesky: %s recibe en el mismo periodo los "
                       "choques de las demás; %s, solo el suyo. Muévelas para cambiarlo.",
                       m->sis.s[m->sis.n - 1].nombre, m->sis.s[0].nombre );
    else
        gtk_label_set_text( GTK_LABEL(m->ps.ver_orden), "" );

    /* CON UN SISTEMA DEL PROYECTO, LAS SERIES SON LAS SUYAS: se cambian en la
     * madre (el caso guarda las entradas y su orden), no aqui.          */
    gtk_widget_set_visible( m->ps.b_abrir, !con_caso );
    gtk_widget_set_visible( m->ps.b_quitar, !con_caso );
    gtk_widget_set_visible( m->ps.b_subir, !con_caso );
    gtk_widget_set_visible( m->ps.b_bajar, !con_caso );
    if (con_caso && m->sis.n >= 2)
        sim_veredicto( m->ps.ver_orden, SIM_VERDE,
                       "Las series y su orden son los del sistema %s; se cambian en la "
                       "madre. El orden es el de Cholesky.", m->caso );
}

static void columna( GtkWidget *tv, const char *t, int c, const char *globo )
{
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes( t, r, "text", c, NULL );
    GtkWidget *l = gtk_label_new( t );

    gtk_widget_set_tooltip_text( l, globo );
    gtk_widget_show( l );
    gtk_tree_view_column_set_widget( col, l );
    gtk_tree_view_append_column( GTK_TREE_VIEW(tv), col );
}

GtkWidget *series_pagina_new( Sim *m )
{
    GtkWidget *caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    GtkWidget *fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    GtkWidget *sw = gtk_scrolled_window_new( NULL, NULL );
    GtkListStore *st = gtk_list_store_new( C_N, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING,
                                           G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING,
                                           G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                           G_TYPE_STRING );

    gtk_container_set_border_width( GTK_CONTAINER(caja), 12 );

    m->ps.b_abrir  = gtk_button_new_with_label( "Abrir…" );
    m->ps.b_quitar = gtk_button_new_with_label( "Quitar" );
    m->ps.b_subir  = gtk_button_new_with_label( "Subir" );
    m->ps.b_bajar  = gtk_button_new_with_label( "Bajar" );
    gtk_widget_set_tooltip_text( m->ps.b_abrir, "Los .pre de las series (o .inp: una "
                                 "especificación), en el orden del sistema" );
    gtk_widget_set_tooltip_text( m->ps.b_subir, "La serie marcada, un puesto antes "
                                 "en el orden de Cholesky" );
    gtk_widget_set_tooltip_text( m->ps.b_bajar, "La serie marcada, un puesto después" );
    g_signal_connect( m->ps.b_abrir, "clicked", G_CALLBACK(on_abrir), m );
    g_signal_connect( m->ps.b_quitar, "clicked", G_CALLBACK(on_quitar), m );
    g_signal_connect( m->ps.b_subir, "clicked", G_CALLBACK(on_subir), m );
    g_signal_connect( m->ps.b_bajar, "clicked", G_CALLBACK(on_bajar), m );
    gtk_box_pack_start( GTK_BOX(fila), m->ps.b_abrir, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(fila), m->ps.b_quitar, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(fila), m->ps.b_subir, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(fila), m->ps.b_bajar, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), fila, FALSE, FALSE, 0 );

    m->ps.lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    g_object_unref( st );
    columna( m->ps.lista, "#", C_NUM, "El puesto en el sistema: el orden de Cholesky" );
    columna( m->ps.lista, "Serie", C_SERIE, "El nombre, del propio fichero" );
    columna( m->ps.lista, "Fichero", C_FICH, "El .pre (un óptimo) o .inp (una especificación)" );
    columna( m->ps.lista, "Obs", C_OBS, "Observaciones de la serie" );
    columna( m->ps.lista, "Muestra", C_VENTANA, "La primera y la última observación" );
    columna( m->ps.lista, "λ", C_LAMBDA, "Box-Cox: 0 es el logaritmo, 1 sin transformar" );
    columna( m->ps.lista, "Diferencias", C_DIF, "Regulares (d) y anuales (D)" );
    columna( m->ps.lista, "AR", C_AR, "Los órdenes de sus factores AR" );
    columna( m->ps.lista, "MA", C_MA, "Los órdenes de sus factores MA" );
    columna( m->ps.lista, "Ventana", C_RECORTE,
             "Cuántas observaciones estacionarias pierde al cruzarse: la ventana "
             "común empieza en la serie que empieza más tarde" );
    gtk_container_add( GTK_CONTAINER(sw), m->ps.lista );
    gtk_box_pack_start( GTK_BOX(caja), sw, TRUE, TRUE, 0 );

    m->ps.ver_ventana = gtk_label_new( "" );
    m->ps.ver_orden   = gtk_label_new( "" );
    gtk_label_set_xalign( GTK_LABEL(m->ps.ver_ventana), 0 );
    gtk_label_set_xalign( GTK_LABEL(m->ps.ver_orden), 0 );
    gtk_label_set_line_wrap( GTK_LABEL(m->ps.ver_ventana), TRUE );
    gtk_label_set_line_wrap( GTK_LABEL(m->ps.ver_orden), TRUE );
    gtk_box_pack_start( GTK_BOX(caja), m->ps.ver_ventana, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), m->ps.ver_orden, FALSE, FALSE, 0 );
    return caja;
}
