/*
 * prevision.c -- la pagina 7: LA PREVISION, LA EVALUACION, LA RESPUESTA AL
 * IMPULSO Y LA DESCOMPOSICION (nodos N5 y N6 de sima: forecast, evaluate,
 * impulse_response, variance_decomposition).
 *
 * HAY DOS COSAS DISTINTAS Y LA PAGINA NO LAS MEZCLA (como lib/outfcst en
 * drtran_gui):
 *
 *   la PREVISION de cada serie en su nivel, con la banda del 95 %, que es
 *   TEORICA: lo que el modelo implica;
 *
 *   la EVALUACION fuera de muestra (-estwin N): los parametros estimados una
 *   vez sobre las N primeras observaciones y clavados mientras el origen
 *   rueda. Es la unica respuesta empirica, y la vara de medir es el sistema
 *   diagonal --los univariantes-- con la misma ventana: ¿prevé mejor el
 *   sistema que las series solas? (sima evaluate).
 *
 * drvarma preve en la pasada en que estima, asi que prever es volver a
 * correr EL MISMO modelo, a <modelo>_prev.*. Con evaluacion, dos corridas
 * mas con -estwin --que preve desde el final de la ventana, no de la
 * muestra, y por eso no sirve para la prevision--: el sistema a
 * <modelo>_eval.* y el diagonal a <modelo>_diag.*. Ninguna toca la
 * estimacion de la pagina 5.
 *
 * La respuesta al impulso y la descomposicion ya estan en el .out de la
 * estimacion (lib/outvarma): se ensenan sin correr nada.
 */

/* Antes que GTK: main.h de drvarma define TRUE y FALSE. */
#include "fue_pre_reader.h"
#include "sim.h"

#include <math.h>
#include <string.h>
#include <glib/gstdio.h>

#include "fugdraw.h"
#include "fugplot.h"
#include "preview.h"

enum { E_SER, E_H, E_N, E_SIS, E_UNI, E_RAZ, E_N_ };

/* --- lanzar ---------------------------------------------------------------- */

static gboolean pulso( gpointer d )
{
    Sim *m = d;
    if (!m->pv.corriendo || !m->avance) return G_SOURCE_REMOVE;
    gtk_progress_bar_pulse( GTK_PROGRESS_BAR(m->avance) );
    return G_SOURCE_CONTINUE;
}

static VarmaOut *lee( const char *base )
{
    gchar *out = g_strdup_printf( "%s.out", base );
    VarmaOut *v = g_new0( VarmaOut, 1 );
    if (ov_parse_file( out, v ) != 0) { g_free( v ); v = NULL; }
    g_free( out );
    return v;
}

static gboolean lanza( Sim *m, int paso );

static const char *sufijo( int paso ) { return paso == 1 ? "" : paso == 2 ? "_eval" : "_diag"; }

static void on_done( const EngineResult *r, gpointer data )
{
    Sim *m = data;
    PagPrev *P = &m->pv;
    gchar *base = g_strdup_printf( "%s%s", P->base, sufijo( P->paso ) );
    VarmaOut *v = lee( base );

    g_free( base );
    P->trabajo = NULL;
    if (P->paso == 1)      { g_free( P->v );  P->v = v; }
    else if (P->paso == 2) { g_free( P->ve ); P->ve = v; }
    else                   { g_free( P->vd ); P->vd = v; }

    /* Con evaluacion, el sistema con -estwin y despues el diagonal. */
    if (P->paso < 3 && r->status == 0 && v && m->ventana && P->c_eval &&
        gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(P->c_eval) ) && lanza( m, P->paso + 1 ))
        return;

    P->corriendo = FALSE;
    if (!m->ventana || !P->b_calc) return;
    gtk_widget_hide( m->avance );
    P->version = m->pe.version;
    sim_refresca( m );
    if (r->status != 0)
        sim_estado( m, "drvarma salió con %d al prever: %s", r->status, r->message );
    else
        sim_estado( m, "Previsión lista%s.", P->vd ? ", y evaluada contra los univariantes" : "" );
}

/* paso 1: el modelo de la pagina 5 con -forecast; 2: el mismo con -estwin;
 * 3: el diagonal (0 0 -diagcov) con la misma ventana.                    */
static gboolean lanza( Sim *m, int paso )
{
    PagPrev *P = &m->pv;
    const char *av[SIS_MAX_SER + 32];
    char buf[2048], hor[16], win[16];
    gchar *salida, *dir;
    SisModelo mo = m->mod;
    int k;

    snprintf( hor, sizeof hor, "%d", gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(P->s_hor) ) );
    snprintf( win, sizeof win, "%d", gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(P->s_win) ) );
    if (paso == 3) { mo.p = mo.q = 0; mo.diagcov = 1; }
    salida = g_strdup_printf( "%s%s", P->base, sufijo( paso ) );
    k = sis_orden_modelo( &m->sis, &mo, salida, buf, sizeof buf, av, G_N_ELEMENTS(av) - 6 );
    if (k < 2) { g_free( salida ); return FALSE; }
    /* -forecast H [-estwin N], delante de «-o SALIDA» (los dos ultimos). */
    k -= 2;
    av[k++] = "-forecast";
    av[k++] = hor;
    if (paso > 1) { av[k++] = "-estwin"; av[k++] = win; }
    av[k++] = "-o";
    av[k++] = salida;
    av[k] = NULL;

    dir = g_path_get_dirname( salida );
    P->paso = paso;
    P->trabajo = engine_start( dir, "drvarma", av, NULL, NULL, on_done, m );
    g_free( dir );
    g_free( salida );
    return P->trabajo != NULL;
}

gboolean prevision_calcular( Sim *m )
{
    PagPrev *P = &m->pv;
    gchar *mot, *nom;

    if (P->corriendo || m->pe.corriendo || m->pp.corriendo) return FALSE;
    if (!m->pe.v || m->pe.v->fallo) {
        sim_estado( m, "Antes, un sistema estimado («5 · Estimación»)." );
        return FALSE;
    }
    if (!( mot = sim_hermano( "drvarma" ) )) {
        sim_estado( m, "No pude lanzar drvarma: ni al lado de sim_gui ni en el PATH." );
        return FALSE;
    }
    g_free( mot );
    if (m->libro) gtk_notebook_set_current_page( GTK_NOTEBOOK(m->libro), PG_PREV );
    g_free( P->base );
    nom = g_strdup_printf( "%s_prev", m->pe.corrida[0] ? m->pe.corrida : "modelo" );
    P->base = sim_artefacto( m, nom );
    g_free( nom );
    g_free( P->v ); P->v = NULL;
    g_free( P->ve ); P->ve = NULL;
    g_free( P->vd ); P->vd = NULL;
    if (!lanza( m, 1 )) { sim_estado( m, "No pude lanzar drvarma." ); return FALSE; }
    P->corriendo = TRUE;
    if (m->avance) { gtk_widget_show( m->avance ); g_timeout_add( 120, pulso, m ); }
    sim_estado( m, "drvarma prevé%s…", gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(P->c_eval) )
                ? " y evalúa (después, los univariantes con la misma ventana)" : "" );
    sim_refresca( m );
    return TRUE;
}

static void on_calc( GtkButton *b, Sim *m ) { (void) b; prevision_calcular( m ); }

/* --- el grafico ------------------------------------------------------------ */

static gchar *latin1( const char *s )
{
    gchar *r = g_convert_with_fallback( s, -1, "ISO-8859-1", "UTF-8", "?", NULL, NULL, NULL );
    return r ? r : g_strdup( s );
}

/* Dos paneles, como drtran_gui: el nivel y la variacion anual, con el tramo
 * observado delante. La variacion lleva su d.t. del .out. En el nivel, con
 * logaritmos la banda del .out es asimetrica y fugdraw solo la dibuja
 * simetrica: no se dibuja, y la nota manda a la tabla.                 */
gboolean prevision_grafico( Sim *m, int i )
{
    PagPrev *P = &m->pv;
    const OvPrev *x = NULL;
    struct Tusmodel tm;
    struct Tseries ts;
    real **dm = NULL;
    int k, j, nh, nf, f, s, es_log;
    double *y[2], *sd[2], rf;
    const char **fe;
    gchar **fechas, *t[2], *nota, *base, *eps, *u;
    FPPanel pn[2];
    FDFig *fig;
    gboolean ok = FALSE;

    if (!P->v || i < 0 || i >= m->sis.n) return FALSE;
    for (k = 0; k < P->v->nprev; k++)
        if (!strcmp( P->v->prev[k].nombre, m->sis.s[i].nombre )) x = &P->v->prev[k];
    if (!x || x->nf < 1) return FALSE;
    memset( &tm, 0, sizeof tm );
    memset( &ts, 0, sizeof ts );
    if (read_fue_pre( m->sis.s[i].ruta, &tm, &ts, &dm ) != 0) return FALSE;

    f = ts.freq > 0 ? ts.freq : 1;
    es_log = fabs( tm.boxlam ) < 1e-8;
    rf = 1.0;                        /* 100*delta/refactor sobre refactor*y */
    nh = ts.nobs < 3 * f + f ? ts.nobs - f : 3 * f > 12 ? 3 * f : 12;
    if (nh > ts.nobs - f) nh = ts.nobs - f;
    if (nh < 1) nh = 1;
    nf = nh + x->nf;
    fe = g_new0( const char *, nf );
    fechas = g_new0( gchar *, nf + 1 );
    for (j = 0; j < 2; j++) { y[j] = g_new0( double, nf ); sd[j] = g_new0( double, nf ); }
    for (j = 0; j < nh; j++) {
        int o = ts.nobs - nh + j + 1;               /* 1..nobs */
        long a = (long) ts.begyear * f + ts.begtime - 1 + o - 1;
        double yt = ts.data[o], ys = ts.data[o - f];
        fechas[j] = f == 1 ? g_strdup_printf( "%ld", a ) : g_strdup_printf( "%ld/%ld", a % f + 1, a / f );
        y[0][j] = yt;
        y[1][j] = es_log ? ( yt > 0 && ys > 0 ? 100.0 * log( yt / ys ) : 0.0 )
                         : 100.0 * ( yt - ys ) * rf;
    }
    for (j = 0; j < x->nf; j++) {
        s = nh + j;
        fechas[s] = g_strdup( f == 1 && !strncmp( x->fecha[j], "1/", 2 ) ? x->fecha[j] + 2 : x->fecha[j] );
        y[0][s] = x->nivel[j];
        /* Con logaritmos la banda del nivel es asimetrica y fugdraw solo
         * sabe dibujarla simetrica (bajaria de cero): no se dibuja, y la
         * nota manda a la tabla. Sin ellos es simetrica y es esta.      */
        sd[0][s] = es_log ? 0.0 : ( x->hi95[j] - x->lo95[j] ) / ( 2 * 1.96 );
        y[1][s] = x->var_anu[j];
        sd[1][s] = x->sd_anu[j];
    }
    for (j = 0; j < nf; j++) fe[j] = fechas[j];

    u = g_strdup_printf( "%s: el nivel, desde %s", x->nombre,
                         f == 1 && !strncmp( x->origen, "1/", 2 ) ? x->origen + 2 : x->origen );
    t[0] = latin1( u ); g_free( u );
    t[1] = latin1( f > 1 ? "La variación anual (%)" : "La variación (%)" );
    nota = latin1( es_log ? "Bandas de ±2 d.t., TEÓRICAS. En el nivel, la del 95 % es asimétrica "
                         "(logaritmos) y no se dibuja: está en la tabla de la previsión."
                       : "Bandas de ±2 d.t., TEÓRICAS: lo que el modelo implica, no lo que "
                         "pasa fuera de muestra." );
    for (j = 0; j < 2; j++) { pn[j].title = t[j]; pn[j].y = y[j]; pn[j].sd = sd[j]; }
    base = sim_artefacto( m, x->nombre );
    fig = fp_forecast( pn, 2, nf, x->nf, fe, nota, base );
    if (fig) {
        gchar *dir = g_path_get_dirname( base ), *nom = g_strdup_printf( "prev_%s.eps", x->nombre );
        eps = g_build_filename( dir, nom, NULL );
        fd_fig_free( fig );
        ok = g_file_test( eps, G_FILE_TEST_EXISTS ) && preview_show( m, eps );
        if (!ok) sim_estado( m, "No pude dibujar %s", eps );
        g_free( eps ); g_free( dir ); g_free( nom );
    }
    for (j = 0; j < 2; j++) { g_free( y[j] ); g_free( sd[j] ); g_free( t[j] ); }
    g_strfreev( fechas );
    g_free( fe ); g_free( nota ); g_free( base );
    free_fue_pre( &tm, &ts, dm );
    return ok;
}

static void on_grafico( GtkButton *b, Sim *m )
{
    prevision_grafico( m, GPOINTER_TO_INT(g_object_get_data( G_OBJECT(b), "serie" )) );
}

/* --- las tablas ------------------------------------------------------------ */

static void texto_prev( Sim *m )
{
    PagPrev *P = &m->pv;
    GString *g = g_string_new( NULL );
    int k, j;

    for (k = 0; P->v && k < P->v->nprev; k++) {
        const OvPrev *x = &P->v->prev[k];
        g_string_append_printf( g, "%s, desde %s (banda del 95 %%, teórica):\n", x->nombre, x->origen );
        g_string_append_printf( g, "  %-10s %14s %14s %14s %9s %8s\n", "fecha", "nivel", "inferior",
                                "superior", "var.%", "d.t." );
        for (j = 0; j < x->nf; j++)
            g_string_append_printf( g, "  %-10s %14.4f %14.4f %14.4f %9.3f %8.3f\n", x->fecha[j],
                                    x->nivel[j], x->lo95[j], x->hi95[j], x->var_anu[j], x->sd_anu[j] );
        g_string_append_c( g, '\n' );
    }
    gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(P->t_prev) ), g->str, -1 );
    g_string_free( g, TRUE );
}

/* La evaluacion: el RMSE del sistema y el de los univariantes, por serie y
 * horizonte, y cuantas veces gana el sistema.                            */
static void evaluacion( Sim *m )
{
    PagPrev *P = &m->pv;
    GtkListStore *st = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(P->l_eval) ));
    int k, j, gana = 0, total = 0;

    gtk_list_store_clear( st );
    if (!P->ve || P->ve->nev == 0) {
        gtk_label_set_text( GTK_LABEL(P->ver_eval), P->v ? "Sin evaluación: marca «Evaluar fuera de "
                            "muestra» y calcula." : "" );
        return;
    }
    for (k = 0; k < P->ve->nev; k++) {
        const OvEvalSerie *e = &P->ve->ev[k], *d = NULL;
        for (j = 0; P->vd && j < P->vd->nev; j++)
            if (!strcmp( P->vd->ev[j].nombre, e->nombre )) d = &P->vd->ev[j];
        for (j = 0; j < e->nh; j++) {
            GtkTreeIter it;
            const OvHoriz *h = &e->h[j], *hd = d && j < d->nh ? &d->h[j] : NULL;
            gchar *a = g_strdup_printf( "%.6g", h->rmse );
            gchar *b = hd ? g_strdup_printf( "%.6g", hd->rmse ) : g_strdup( "" );
            gchar *c = hd && hd->rmse > 0 ? g_strdup_printf( "%.3f", h->rmse / hd->rmse ) : g_strdup( "" );
            if (hd) { total++; gana += h->rmse < hd->rmse; }
            gtk_list_store_append( st, &it );
            gtk_list_store_set( st, &it, E_SER, e->nombre, E_H, h->h, E_N, h->n, E_SIS, a, E_UNI, b,
                                E_RAZ, c, -1 );
            g_free( a ); g_free( b ); g_free( c );
        }
    }
    if (total)
        sim_veredicto( P->ver_eval, gana * 2 > total ? SIM_VERDE : SIM_AMBAR,
                       "Fuera de muestra (%d orígenes; n, los que tienen dato con que comparar), "
                       "el sistema prevé mejor que los univariantes en %d de %d series y "
                       "horizontes (RMSE sistema / univariantes < 1).",
                       P->ve->ev_origenes, gana, total );
    else
        sim_veredicto( P->ver_eval, SIM_AMBAR, "La evaluación del sistema, sin la de los univariantes "
                       "con la que compararla." );
}

/* La IRF y la FEVD, del .out de la estimacion: no se corre nada. */
static void irf_fevd( Sim *m )
{
    PagPrev *P = &m->pv;
    const VarmaOut *v = m->pe.v;
    GString *g = g_string_new( NULL ), *h = g_string_new( NULL );
    int a, b, k, n = m->sis.n;

    if (v && !v->fallo && v->nh_irf > 0) {
        g_string_append( g, "Respuesta al impulso ortogonalizada (Cholesky, en el orden del sistema):\n"
                            "un choque de una desviación típica en la innovación de cada serie.\n\n" );
        for (a = 0; a < n; a++) {
            g_string_append_printf( g, "Choque en %s:\n  %3s", m->sis.s[a].nombre, "h" );
            for (b = 0; b < n; b++) g_string_append_printf( g, " %12s", m->sis.s[b].nombre );
            g_string_append_c( g, '\n' );
            for (k = 0; k < v->nh_irf; k++) {
                g_string_append_printf( g, "  %3d", k );
                for (b = 0; b < n; b++) g_string_append_printf( g, " %12.4f", v->irf[a][k][b] );
                g_string_append_c( g, '\n' );
            }
            g_string_append_c( g, '\n' );
        }
    }
    if (v && !v->fallo && v->nh_fevd > 0) {
        g_string_append( h, "Descomposición de la varianza del error de previsión (%, Cholesky):\n"
                            "de qué innovación viene el error de prever cada serie.\n\n" );
        for (a = 0; a < n; a++) {
            g_string_append_printf( h, "%s:\n  %3s", m->sis.s[a].nombre, "h" );
            for (b = 0; b < n; b++) g_string_append_printf( h, " %10s", m->sis.s[b].nombre );
            g_string_append_c( h, '\n' );
            for (k = 0; k < v->nh_fevd; k++) {
                g_string_append_printf( h, "  %3d", k + 1 );
                for (b = 0; b < n; b++) g_string_append_printf( h, " %10.2f", v->fevd[a][k][b] );
                g_string_append_c( h, '\n' );
            }
            g_string_append_c( h, '\n' );
        }
    }
    gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(P->t_irf) ), g->str, -1 );
    gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(P->t_fevd) ), h->str, -1 );
    g_string_free( g, TRUE );
    g_string_free( h, TRUE );
}

void prevision_refresca( Sim *m )
{
    PagPrev *P = &m->pv;
    GList *l, *q;
    int i;

    if (!P->b_calc) return;
    gtk_widget_set_sensitive( P->b_calc, !P->corriendo && m->pe.v && !m->pe.v->fallo && !m->pe.corriendo );
    gtk_widget_set_sensitive( P->s_win, gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(P->c_eval) ) );

    /* Una prevision de otra estimacion ya no es la de esta. */
    if (P->v && P->version != m->pe.version && !P->corriendo) {
        g_free( P->v ); P->v = NULL;
        g_free( P->ve ); P->ve = NULL;
        g_free( P->vd ); P->vd = NULL;
    }
    if (m->pe.v && m->pe.v->nobs > 0 && !P->corriendo) {
        int nobs = m->pe.v->nobs, w = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(P->s_win) );
        gtk_spin_button_set_range( GTK_SPIN_BUTTON(P->s_win), 10, nobs > 11 ? nobs - 1 : 11 );
        if (w <= 10 || w >= nobs) gtk_spin_button_set_value( GTK_SPIN_BUTTON(P->s_win), (int)( 0.75 * nobs ) );
    }

    if (P->corriendo)
        sim_veredicto( P->ver_pide, SIM_AMBAR, "drvarma está previendo…" );
    else if (m->pe.vec && !m->pe.corriendo)
        sim_veredicto( P->ver_pide, SIM_AMBAR, "Con un VEC, la previsión, la evaluación y la respuesta "
                       "al impulso todavía no están en sim_gui: las hace sima (forecast, evaluate, "
                       "impulse_response). La respuesta al impulso del VEC, en niveles, está en "
                       "«6 · Diagnosis ▸ Salida»." );
    else if (!m->pe.v || m->pe.v->fallo)
        sim_veredicto( P->ver_pide, SIM_AMBAR, "Pide un sistema estimado («5 · Estimación»)." );
    else if (!P->v)
        sim_veredicto( P->ver_pide, SIM_AMBAR, "La respuesta al impulso y la descomposición ya están "
                       "(abajo). La previsión: «Calcular»." );
    else
        sim_veredicto( P->ver_pide, SIM_VERDE, "Previsión %s%s, %d periodos. Las bandas son TEÓRICAS; "
                       "lo empírico es la evaluación.", m->pe.corrida[0] ? "del modelo " : "del sistema",
                       m->pe.corrida, P->v->horizonte );

    texto_prev( m );
    evaluacion( m );
    irf_fevd( m );

    l = gtk_container_get_children( GTK_CONTAINER(P->graficos) );
    for (q = l; q; q = q->next) gtk_widget_destroy( q->data );
    g_list_free( l );
    for (i = 0; P->v && i < m->sis.n; i++) {
        gchar *t = g_strdup_printf( "Gráfico de %s", m->sis.s[i].nombre );
        GtkWidget *b = gtk_button_new_with_label( t );
        g_object_set_data( G_OBJECT(b), "serie", GINT_TO_POINTER(i) );
        g_signal_connect( b, "clicked", G_CALLBACK(on_grafico), m );
        gtk_box_pack_start( GTK_BOX(P->graficos), b, FALSE, FALSE, 0 );
        g_free( t );
    }
    gtk_widget_show_all( P->graficos );
}

/* --- la pagina ------------------------------------------------------------- */

static GtkWidget *veredicto_new( void )
{
    GtkWidget *l = gtk_label_new( "" );
    gtk_label_set_xalign( GTK_LABEL(l), 0 );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    return l;
}

static GtkWidget *texto_new( GtkWidget **tv )
{
    GtkWidget *sw = gtk_scrolled_window_new( NULL, NULL );
    *tv = gtk_text_view_new();
    gtk_text_view_set_editable( GTK_TEXT_VIEW(*tv), FALSE );
    gtk_text_view_set_monospace( GTK_TEXT_VIEW(*tv), TRUE );
    gtk_container_add( GTK_CONTAINER(sw), *tv );
    return sw;
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

static void on_eval( GtkToggleButton *t, Sim *m ) { (void) t; prevision_refresca( m ); }

GtkWidget *prevision_pagina_new( Sim *m )
{
    PagPrev *P = &m->pv;
    GtkWidget *caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    GtkWidget *fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
    GtkWidget *pg, *sw;
    GtkListStore *st;

    gtk_container_set_border_width( GTK_CONTAINER(caja), 12 );
    P->ver_pide = veredicto_new();
    gtk_box_pack_start( GTK_BOX(caja), P->ver_pide, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX(fila), gtk_label_new( "Horizonte" ), FALSE, FALSE, 0 );
    P->s_hor = gtk_spin_button_new_with_range( 1, 120, 1 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(P->s_hor), 10 );
    gtk_box_pack_start( GTK_BOX(fila), P->s_hor, FALSE, FALSE, 0 );
    P->c_eval = gtk_check_button_new_with_label( "Evaluar fuera de muestra, estimando con las primeras" );
    gtk_widget_set_tooltip_text( P->c_eval, "-estwin N: los parámetros se estiman una vez con las N "
                                 "primeras observaciones y se clavan mientras el origen avanza. Después, "
                                 "los univariantes con la misma ventana, para comparar" );
    gtk_box_pack_start( GTK_BOX(fila), P->c_eval, FALSE, FALSE, 0 );
    P->s_win = gtk_spin_button_new_with_range( 10, 2000, 1 );
    gtk_box_pack_start( GTK_BOX(fila), P->s_win, FALSE, FALSE, 0 );
    g_signal_connect( P->c_eval, "toggled", G_CALLBACK(on_eval), m );
    P->b_calc = gtk_button_new_with_label( "Calcular" );
    g_signal_connect( P->b_calc, "clicked", G_CALLBACK(on_calc), m );
    gtk_box_pack_start( GTK_BOX(fila), P->b_calc, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), fila, FALSE, FALSE, 0 );

    P->libreta = gtk_notebook_new();
    gtk_box_pack_start( GTK_BOX(caja), P->libreta, TRUE, TRUE, 0 );

    pg = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(pg), 6 );
    P->graficos = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    gtk_box_pack_start( GTK_BOX(pg), P->graficos, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(pg), texto_new( &P->t_prev ), TRUE, TRUE, 0 );
    gtk_notebook_append_page( GTK_NOTEBOOK(P->libreta), pg, gtk_label_new( "Previsión" ) );

    pg = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(pg), 6 );
    P->ver_eval = veredicto_new();
    gtk_box_pack_start( GTK_BOX(pg), P->ver_eval, FALSE, FALSE, 0 );
    st = gtk_list_store_new( E_N_, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING );
    P->l_eval = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    g_object_unref( st );
    columna( P->l_eval, "Serie", E_SER, "" );
    columna( P->l_eval, "h", E_H, "El horizonte" );
    columna( P->l_eval, "n", E_N, "Cuántas previsiones a ese horizonte" );
    columna( P->l_eval, "RMSE sistema", E_SIS, "El del modelo de «5 · Estimación»" );
    columna( P->l_eval, "RMSE univariantes", E_UNI, "El del sistema diagonal, con la misma ventana" );
    columna( P->l_eval, "Cociente", E_RAZ, "Menor que 1: el sistema prevé mejor" );
    sw = gtk_scrolled_window_new( NULL, NULL );
    gtk_container_add( GTK_CONTAINER(sw), P->l_eval );
    gtk_box_pack_start( GTK_BOX(pg), sw, TRUE, TRUE, 0 );
    gtk_notebook_append_page( GTK_NOTEBOOK(P->libreta), pg, gtk_label_new( "Evaluación" ) );

    gtk_notebook_append_page( GTK_NOTEBOOK(P->libreta), texto_new( &P->t_irf ),
                              gtk_label_new( "Respuesta al impulso" ) );
    gtk_notebook_append_page( GTK_NOTEBOOK(P->libreta), texto_new( &P->t_fevd ),
                              gtk_label_new( "Descomposición" ) );
    return caja;
}
