/*
 * puerta.c -- la pagina 2: LA PUERTA DIAGONAL (nodo N1 de sima, run_gate).
 *
 *   drvarma A.pre B.pre ... 0 0 -diagcov -o <sistema>_puerta
 *
 * Con p = q = 0 y Sigma diagonal el sistema se parte en sus univariantes, y
 * la logL conjunta tiene que ser la suma de las suyas. Si no lo es, el cast
 * conjunto no reproduce los modelos de fue y nada de lo que se construya
 * encima se puede creer: drvarma sale con 5 (escalera.c). Es la pagina que
 * drtran_gui no tiene, porque la transferencia no reestima la diagonal.
 *
 * Lo que dice la pagina sale de lib/outvarma: la tabla de la puerta, y POR
 * QUE se movio cada serie de su .pre --la ventana comun le quita datos (y
 * moverse es legitimo), o no es un optimo (es un .inp, una especificacion)--.
 *
 * Y la pregunta de la cointegracion (N1c): con dos o mas series
 * diferenciadas, ¿se mueven juntas a largo plazo? Es la del rango, y llega
 * con el VEC (fase S10).
 */

#include <string.h>

#include "sim.h"

enum { C_SERIE, C_LOGL, C_SIGMA2, C_MOV, C_POR, C_N };

/* Las rutas del sistema, en orden: si cambian, la puerta es de otro. */
static gchar *firma( Sim *m )
{
    GString *g = g_string_new( NULL );
    int i;
    for (i = 0; i < m->sis.n; i++) { g_string_append( g, m->sis.s[i].ruta ); g_string_append_c( g, '\n' ); }
    return g_string_free( g, FALSE );
}

int puerta_estado( Sim *m )
{
    PagPuerta *P = &m->pp;
    gchar *f;
    int vale;

    if (!P->hecha || !P->firma) return 0;
    f = firma( m );
    vale = strcmp( f, P->firma ) == 0;
    g_free( f );
    if (!vale) return 0;
    return P->v && P->v->puerta.hay && P->v->puerta.pasa && P->rc == 0 ? 1 : -1;
}

static gchar *orden_texto( Sim *m, const char *salida )
{
    const char *av[SIS_MAX_SER + 8];
    GString *g = g_string_new( "drvarma" );
    int k, i;

    k = sis_orden_puerta( &m->sis, salida, av, G_N_ELEMENTS(av) );
    for (i = 0; i < k; i++) {
        gchar *b = i < m->sis.n || i == k - 1 ? g_path_get_basename( av[i] ) : g_strdup( av[i] );
        g_string_append_c( g, ' ' );
        g_string_append( g, b );
        g_free( b );
    }
    return g_string_free( g, FALSE );
}

static void para_avance( Sim *m )
{
    gtk_widget_hide( m->avance );
}

static gboolean pulso( gpointer d )
{
    Sim *m = d;
    if (!m->pp.corriendo) return G_SOURCE_REMOVE;
    gtk_progress_bar_pulse( GTK_PROGRESS_BAR(m->avance) );
    return G_SOURCE_CONTINUE;
}

static void on_done( const EngineResult *r, gpointer data )
{
    Sim *m = data;
    PagPuerta *P = &m->pp;
    gchar *out = g_strdup_printf( "%s.out", P->out );

    P->corriendo = FALSE;
    P->trabajo = NULL;
    /* LA VENTANA SE CERRO con el motor corriendo: se le paro al cerrarla
     * (ventana.c), y lo que acaba de llegar ya no tiene donde ensenarse. */
    if (!m->ventana) { g_free( out ); return; }
    para_avance( m );
    P->rc = r->status;
    P->hecha = TRUE;
    P->version++;
    if (!P->v) P->v = g_new0( VarmaOut, 1 );
    if (ov_parse_file( out, P->v ) != 0) {
        memset( P->v, 0, sizeof *P->v );
        P->hecha = FALSE;
        sim_estado( m, "drvarma no escribió la puerta (%s): %s", out, r->message );
    } else if (r->status == 0 && P->v->puerta.pasa)
        sim_estado( m, "La puerta pasa: el sistema diagonal reproduce los univariantes." );
    else if (r->status == 5 || (P->v->puerta.hay && !P->v->puerta.pasa))
        sim_estado( m, "La puerta FALLA: el sistema diagonal no reproduce los univariantes." );
    else
        sim_estado( m, "drvarma salió con %d: %s", r->status, r->message );
    g_free( out );
    sim_refresca( m );
}

gboolean puerta_pasar( Sim *m )
{
    PagPuerta *P = &m->pp;
    const char *av[SIS_MAX_SER + 8];
    SisVentana v;
    char why[1024];
    gchar *dir;
    int k;

    if (P->corriendo) return FALSE;
    if (m->sis.n < 2) { sim_estado( m, "La puerta necesita al menos dos series («1 · Series»)." ); return FALSE; }
    if (sis_ventana( &m->sis, &v, why, sizeof why ) != 0) { sim_estado( m, "Sin ventana común: %s", why ); return FALSE; }
    /* EL MOTOR, ANTES: lib/engine, si no lo encuentra, prueba el nombre a
     * secas en el PATH. Aqui se dice ya, y donde se busco.              */
    {
        gchar *mot = sim_hermano( "drvarma" );
        if (!mot) {
            sim_veredicto( P->ver_puerta, SIM_ROJO,
                           "No pude lanzar drvarma: ni al lado de sim_gui ni en el PATH "
                           "(«make» en engines/drvarma)." );
            sim_estado( m, "No pude lanzar drvarma." );
            return FALSE;
        }
        g_free( mot );
    }

    g_free( P->out );
    P->out = sim_artefacto( m, "puerta" );
    k = sis_orden_puerta( &m->sis, P->out, av, G_N_ELEMENTS(av) - 1 );
    av[k] = NULL;
    dir = g_path_get_dirname( P->out );
    g_free( P->firma );
    P->firma = firma( m );
    P->hecha = FALSE;

    P->trabajo = engine_start( dir, "drvarma", av, NULL, NULL, on_done, m );
    g_free( dir );
    if (!P->trabajo) {
        sim_veredicto( P->ver_puerta, SIM_ROJO,
                       "No pude lanzar drvarma: ni al lado de sim_gui ni en el PATH "
                       "(«make» en engines/drvarma)." );
        sim_estado( m, "No pude lanzar drvarma." );
        return FALSE;
    }
    P->corriendo = TRUE;
    gtk_widget_show( m->avance );
    g_timeout_add( 120, pulso, m );
    sim_estado( m, "drvarma pasa la puerta…" );
    sim_refresca( m );
    return TRUE;
}

static void on_pasar( GtkButton *b, Sim *m ) { (void) b; puerta_pasar( m ); }

static const char *por_que( const OvPuertaSerie *x )
{
    if (x->no_optimo) return "se movió en su propia muestra: no es un óptimo";
    if (x->recortada) return "se movió porque la ventana común le quita datos";
    return "";
}

void puerta_refresca( Sim *m )
{
    PagPuerta *P = &m->pp;
    GtkListStore *st;
    gchar *ord, *sal;
    int e, i, dif;

    if (!P->lista) return;

    sal = sim_artefacto( m, "puerta" );
    ord = m->sis.n >= 2 ? orden_texto( m, sal ) : g_strdup( "" );
    gtk_label_set_text( GTK_LABEL(P->orden), ord );
    g_free( ord ); g_free( sal );
    gtk_widget_set_sensitive( P->b_pasar, !P->corriendo );

    st = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(P->lista) ));
    gtk_list_store_clear( st );
    e = puerta_estado( m );

    if (P->corriendo)
        sim_veredicto( P->ver_puerta, SIM_AMBAR, "drvarma está pasando la puerta…" );
    else if (m->sis.n < 2)
        sim_veredicto( P->ver_puerta, SIM_AMBAR,
                       "La puerta necesita al menos dos series: «1 · Series», Abrir…" );
    else if (e == 0 && P->hecha)
        sim_veredicto( P->ver_puerta, SIM_AMBAR,
                       "El sistema cambió después de la última puerta: hay que volver a pasarla." );
    else if (e == 0)
        sim_veredicto( P->ver_puerta, SIM_AMBAR,
                       "Todavía no se ha pasado la puerta: «Pasar la puerta»." );
    else {
        const OvPuerta *g = &P->v->puerta;
        GString *notas = g_string_new( NULL );

        for (i = 0; i < g->ns; i++) {
            GtkTreeIter it;
            gchar *a = g_strdup_printf( "%.6f", g->s[i].logl );
            gchar *b = g_strdup_printf( "%.6g", g->s[i].sigma2 );
            gchar *c = g_strdup_printf( "%.3g", g->s[i].mov );
            gtk_list_store_append( st, &it );
            gtk_list_store_set( st, &it, C_SERIE, g->s[i].nombre, C_LOGL, a, C_SIGMA2, b,
                                C_MOV, c, C_POR, por_que( &g->s[i] ), -1 );
            g_free( a ); g_free( b ); g_free( c );
            if (g->s[i].no_optimo)
                g_string_append_printf( notas, "%s%s se movió %.3g de su fichero en su propia "
                    "muestra: es una especificación, no un óptimo. Su modelo es el que drvarma "
                    "acaba de estimar, no el del fichero; si es lo que querías, sigue; si no, "
                    "estímalo en fue.", notas->len ? "\n" : "", g->s[i].nombre, g->s[i].mov );
        }
        if (e > 0)
            sim_veredicto( P->ver_puerta, SIM_VERDE,
                           "PUERTA SUPERADA: el sistema diagonal reproduce los univariantes "
                           "(suma %.6f, conjunta %.6f, diferencia %.2g).",
                           g->suma, g->conjunta, g->diferencia );
        else
            sim_veredicto( P->ver_puerta, SIM_ROJO,
                           "LA PUERTA FALLA (diferencia %.3g): el cast conjunto no reproduce "
                           "los modelos de fue, y nada de lo que se construya encima se "
                           "puede creer. Revisa en fue el fichero que se movió.",
                           g->diferencia );
        gtk_label_set_text( GTK_LABEL(P->ver_notas), notas->str );
        g_string_free( notas, TRUE );
    }
    if (e == 0) gtk_label_set_text( GTK_LABEL(P->ver_notas), "" );

    /* N1c: la pregunta de la cointegracion, con dos o mas diferenciadas. */
    dif = sis_diferenciadas( &m->sis );
    if (m->sis.n >= 2 && dif >= 2)
        sim_veredicto( P->ver_rango, SIM_AMBAR,
                       "%d de las series están diferenciadas. ¿Se mueven juntas a largo "
                       "plazo? Esa pregunta es la del rango de cointegración, y llega con "
                       "el VEC (fase S10). Mientras tanto, la hace sima (prepare_vec y "
                       "rank_test).", dif );
    else
        gtk_label_set_text( GTK_LABEL(P->ver_rango), "" );
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

static GtkWidget *veredicto_new( void )
{
    GtkWidget *l = gtk_label_new( "" );
    gtk_label_set_xalign( GTK_LABEL(l), 0 );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    return l;
}

GtkWidget *puerta_pagina_new( Sim *m )
{
    PagPuerta *P = &m->pp;
    GtkWidget *caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    GtkWidget *fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
    GtkWidget *expl = gtk_label_new( "Cada serie sola, con su modelo de fue y en la ventana "
        "común; luego el sistema diagonal (sin dinámica cruzada, Σ diagonal) evaluado en "
        "esos óptimos. Las dos logL tienen que coincidir." );
    GtkListStore *st = gtk_list_store_new( C_N, G_TYPE_STRING, G_TYPE_STRING,
                                           G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );

    gtk_container_set_border_width( GTK_CONTAINER(caja), 12 );
    gtk_label_set_line_wrap( GTK_LABEL(expl), TRUE );
    gtk_label_set_xalign( GTK_LABEL(expl), 0 );
    gtk_box_pack_start( GTK_BOX(caja), expl, FALSE, FALSE, 0 );

    P->b_pasar = gtk_button_new_with_label( "Pasar la puerta" );
    gtk_widget_set_tooltip_text( P->b_pasar, "drvarma con p = q = 0 y Σ diagonal" );
    g_signal_connect( P->b_pasar, "clicked", G_CALLBACK(on_pasar), m );
    gtk_box_pack_start( GTK_BOX(fila), P->b_pasar, FALSE, FALSE, 0 );
    P->orden = sim_mono( "" );
    gtk_widget_set_tooltip_text( P->orden, "La orden que se lanza (se puede copiar)" );
    gtk_label_set_ellipsize( GTK_LABEL(P->orden), PANGO_ELLIPSIZE_MIDDLE );
    gtk_box_pack_start( GTK_BOX(fila), P->orden, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), fila, FALSE, FALSE, 0 );

    P->lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    g_object_unref( st );
    columna( P->lista, "Serie", C_SERIE, "La serie" );
    columna( P->lista, "logL", C_LOGL, "Su logL sola, en la ventana común" );
    columna( P->lista, "σ²", C_SIGMA2, "La varianza de su innovación" );
    columna( P->lista, "Se movió", C_MOV,
             "Cuánto cambiaron sus parámetros respecto al fichero. Un .pre de verdad es "
             "un punto fijo: se mueve ~1e-5, el redondeo de sus seis decimales" );
    columna( P->lista, "Por qué", C_POR, "La razón del movimiento, si se movió" );
    gtk_box_pack_start( GTK_BOX(caja), P->lista, FALSE, FALSE, 0 );

    P->ver_puerta = veredicto_new();
    P->ver_notas  = veredicto_new();
    P->ver_rango  = veredicto_new();
    gtk_box_pack_start( GTK_BOX(caja), P->ver_puerta, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), P->ver_notas, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), P->ver_rango, FALSE, FALSE, 0 );
    return caja;
}
