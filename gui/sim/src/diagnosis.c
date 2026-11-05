/*
 * diagnosis.c -- la pagina 6: LA DIAGNOSIS (nodo N4 de sima, estimate y
 * check_residuals).
 *
 * AQUI NO SE CALCULA NADA: drvarma ya hace toda la diagnosis y la escribe en
 * el .out; lib/outvarma y lib/outdiag la leen. Las pestañas van EN EL ORDEN
 * DE LAS PREGUNTAS, de lo que invalida el sistema a lo que lo matiza:
 *
 *   Ajuste      ¿aporta algo la dinamica cruzada? (LR contra los
 *               univariantes); como acabo el optimizador; las raices
 *   Residuos    ¿son ruido blanco? Hosking y Jarque-Bera multivariantes, y
 *               los de cada serie
 *   Parametros  la tabla, con la t de cada uno
 *   Σ           la covarianza de las innovaciones y sus correlaciones
 *   Salida      el .out entero
 *
 * Simplificar (los |t| < 1 a cero) y «¿es una red?» llegan con la fase S7.
 */

#include <math.h>
#include <string.h>

#include "sim.h"

enum { P_NOM, P_EST, P_SE, P_T, P_P, P_N };
enum { R_SER, R_MEDIA, R_SD, R_ASIM, R_CURT, R_FUERA2, R_LB, R_N };

static GtkWidget *veredicto_new( void )
{
    GtkWidget *l = gtk_label_new( "" );
    gtk_label_set_xalign( GTK_LABEL(l), 0 );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    return l;
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

static GtkWidget *con_scroll( GtkWidget *w )
{
    GtkWidget *sw = gtk_scrolled_window_new( NULL, NULL );
    gtk_container_add( GTK_CONTAINER(sw), w );
    return sw;
}

static void ajuste( Sim *m, const VarmaOut *v )
{
    PagDiag *D = &m->pd;

    if (v->tiene_lr)
        sim_veredicto( D->ver_lr, v->lr_p < 0.05 ? SIM_VERDE : SIM_AMBAR,
                       "Contra los univariantes (el sistema diagonal): logL %.6f frente a %.6f, "
                       "LR = %.4g con %d gl, p = %.4f. %s", v->lr_logl_full, v->lr_logl_diag,
                       v->lr, v->lr_df, v->lr_p,
                       v->lr_p < 0.05 ? "La dinámica cruzada aporta: el sistema mejora a las series solas."
                                      : "La dinámica cruzada NO aporta de forma significativa: los "
                                        "univariantes bastan, o sobran enlaces." );
    else
        sim_veredicto( D->ver_lr, SIM_AMBAR, "Sin LR: el modelo no tiene dinámica cruzada que contrastar." );

    if (v->optim == OV_OPT_PARED_MA)
        sim_veredicto( D->ver_optim, SIM_AMBAR, "El optimizador se paró en la pared MA (%d de %d raíces): "
                       "lo de abajo no es la diagnosis de un óptimo.", v->pared_n, v->pared_raices );
    else if (v->optim == OV_OPT_PARADO)
        sim_veredicto( D->ver_optim, SIM_AMBAR, "El optimizador se paró sin converger (%s).", sim_criterio( v->criterio ) );
    else if (v->optim == OV_OPT_CONVERGIO)
        sim_veredicto( D->ver_optim, SIM_VERDE, "El optimizador convergió en %d iteraciones (%s).",
                       v->iteraciones, sim_criterio( v->criterio ) );
    else
        gtk_label_set_text( GTK_LABEL(D->ver_optim), "" );

    if (v->n_raices_ar || v->n_raices_ma) {
        gboolean ar_ok = v->raiz_ar_max < 1, ma_ok = v->raiz_ma_max < 1;
        sim_veredicto( D->ver_raices, ar_ok && ma_ok ? SIM_VERDE : SIM_ROJO,
                       "Raíces inversas: |φ(B)| la mayor %.3f (%d), |θ(B)| la mayor %.3f (%d). %s",
                       v->raiz_ar_max, v->n_raices_ar, v->raiz_ma_max, v->n_raices_ma,
                       ar_ok && ma_ok ? "Estacionario e invertible." :
                       !ar_ok ? "No estacionario." : "No invertible." );
    } else
        gtk_label_set_text( GTK_LABEL(D->ver_raices), "" );
}

static void residuos( Sim *m, const VarmaOut *v )
{
    PagDiag *D = &m->pd;
    GtkListStore *st = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(D->l_res) ));
    const Diagnosis *d = &v->diag;
    int i;

    gtk_list_store_clear( st );
    if (!v->tiene_diag) {
        sim_veredicto( D->ver_hosking, SIM_AMBAR, "El .out no trae la diagnosis de los residuos." );
        gtk_label_set_text( GTK_LABEL(D->ver_jb), "" );
        return;
    }
    if (d->hosking.hay)
        sim_veredicto( D->ver_hosking, d->hosking_blanco ? SIM_VERDE : SIM_ROJO,
                       "Hosking (retardo %d): Q = %.4g con %d gl, p = %.4f. %s", d->hosking_lag,
                       d->hosking.q, d->hosking.df, d->hosking.p,
                       d->hosking_blanco ? "Los residuos del sistema parecen ruido blanco."
                                         : "Los residuos del sistema NO son ruido blanco: falta dinámica." );
    if (d->jb.hay)
        sim_veredicto( D->ver_jb, d->jb_normal ? SIM_VERDE : SIM_AMBAR,
                       "Jarque-Bera multivariante: %.4g con %d gl, p = %.4f. %s", d->jb.q, d->jb.df,
                       d->jb.p, d->jb_normal ? "Compatible con la normal."
                                             : "No normales: mira los residuos grandes de cada serie." );
    for (i = 0; i < d->ns; i++) {
        const OdSerie *x = &d->s[i];
        GtkTreeIter it;
        gchar *a = g_strdup_printf( "%.4g", x->media ), *b = g_strdup_printf( "%.4g", x->sd );
        gchar *c = g_strdup_printf( "%.3f", x->skew ), *e = g_strdup_printf( "%.3f", x->kurt );
        gchar *f = x->tiene_hist ? g_strdup_printf( "%.1f%% (%.1f%%)", x->fuera2, x->esp2 ) : g_strdup( "" );
        gchar *g = x->lb.hay ? g_strdup_printf( "Q(%d) = %.2f", x->lb.df, x->lb.q ) : g_strdup( "" );
        const char *nom = i < m->sis.n ? m->sis.s[i].nombre : x->nombre;
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it, R_SER, nom, R_MEDIA, a, R_SD, b, R_ASIM, c, R_CURT, e,
                            R_FUERA2, f, R_LB, g, -1 );
        g_free( a ); g_free( b ); g_free( c ); g_free( e ); g_free( f ); g_free( g );
    }
}

static void parametros( Sim *m, const VarmaOut *v )
{
    GtkListStore *st = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(m->pd.l_par) ));
    int i;

    gtk_list_store_clear( st );
    for (i = 0; i < v->npar; i++) {
        const OvParam *x = &v->par[i];
        GtkTreeIter it;
        gchar *a = g_strdup_printf( "%.6f", x->est );
        gchar *b = x->tiene_se ? g_strdup_printf( "%.6f", x->se ) : g_strdup( "" );
        gchar *c = x->tiene_se ? g_strdup_printf( "%.3f", x->t ) : g_strdup( "" );
        gchar *e = x->tiene_se ? g_strdup_printf( "%.4f", x->p ) : g_strdup( "" );
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it, P_NOM, x->nombre, P_EST, a, P_SE, b, P_T, c, P_P, e, -1 );
        g_free( a ); g_free( b ); g_free( c ); g_free( e );
    }
}

static void sigma( Sim *m, const VarmaOut *v )
{
    GString *g = g_string_new( NULL );
    int i, j, n = m->sis.n;

    if (v->tiene_sigma) {
        g_string_append( g, "Σ (covarianza de las innovaciones):\n" );
        for (i = 0; i < n; i++) {
            g_string_append_printf( g, "  %-12s", m->sis.s[i].nombre );
            for (j = 0; j < n; j++) g_string_append_printf( g, " %12.5g", v->sigma[i][j] );
            g_string_append_c( g, '\n' );
        }
        g_string_append( g, "\nCorrelaciones:\n" );
        for (i = 0; i < n; i++) {
            g_string_append_printf( g, "  %-12s", m->sis.s[i].nombre );
            for (j = 0; j < n; j++) g_string_append_printf( g, " %8.4f", v->correl[i][j] );
            g_string_append_c( g, '\n' );
        }
    }
    gtk_label_set_text( GTK_LABEL(m->pd.t_sigma), g->str );
    g_string_free( g, TRUE );
}

void diagnosis_refresca( Sim *m )
{
    PagDiag *D = &m->pd;
    const VarmaOut *v = m->pe.v;
    gchar *t = NULL;

    if (!D->libreta) return;
    estructura_refresca( m );          /* S7: su estado cambia sin estimar */
    if (!v || m->pe.corriendo) {
        sim_veredicto( D->ver_pide, SIM_AMBAR, "Pide un sistema estimado («5 · Estimación»)." );
        gtk_widget_set_visible( D->libreta, FALSE );
        D->version = -1;
        return;
    }
    if (D->version == m->pe.version) return;
    D->version = m->pe.version;
    if (v->fallo) {
        sim_veredicto( D->ver_pide, SIM_ROJO, "La estimación falló: no hay diagnosis («5 · Estimación»)." );
        gtk_widget_set_visible( D->libreta, FALSE );
        return;
    }
    gtk_widget_set_visible( D->libreta, TRUE );
    sim_veredicto( D->ver_pide, SIM_VERDE, "La diagnosis de %s, leída del .out de drvarma.",
                   m->pe.corrida[0] ? m->pe.corrida : "la última estimación" );
    ajuste( m, v );
    residuos( m, v );
    parametros( m, v );
    sigma( m, v );
    if (m->pe.out && g_file_get_contents( m->pe.out, &t, NULL, NULL )) {
        gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(D->t_out) ), t, -1 );
        g_free( t );
    }
}

static void on_simpl( Sim *m ) { estructura_simplificar( m, 1.0 ); }
static void on_red( Sim *m )   { estructura_red( m, 0.05 ); }
static void on_pasar_mt( Sim *m )
{
    char why[512];
    estructura_pasar_mt( m, why, sizeof why );
    sim_estado( m, "%s", why );
}

GtkWidget *diagnosis_pagina_new( Sim *m )
{
    PagDiag *D = &m->pd;
    GtkWidget *caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    GtkWidget *pg;
    GtkListStore *st;

    gtk_container_set_border_width( GTK_CONTAINER(caja), 12 );
    D->version = -1;
    D->ver_pide = veredicto_new();
    gtk_box_pack_start( GTK_BOX(caja), D->ver_pide, FALSE, FALSE, 0 );
    D->libreta = gtk_notebook_new();
    /* Se ensena solo con un sistema estimado: el show_all de la ventana no
     * la toca, y sus paginas se ensenan aqui una vez.                   */
    gtk_widget_set_no_show_all( D->libreta, TRUE );
    gtk_box_pack_start( GTK_BOX(caja), D->libreta, TRUE, TRUE, 0 );

    pg = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    gtk_container_set_border_width( GTK_CONTAINER(pg), 8 );
    D->ver_lr = veredicto_new(); D->ver_optim = veredicto_new(); D->ver_raices = veredicto_new();
    gtk_box_pack_start( GTK_BOX(pg), D->ver_lr, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(pg), D->ver_optim, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(pg), D->ver_raices, FALSE, FALSE, 0 );
    gtk_notebook_append_page( GTK_NOTEBOOK(D->libreta), pg, gtk_label_new( "Ajuste" ) );

    pg = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    gtk_container_set_border_width( GTK_CONTAINER(pg), 8 );
    D->ver_hosking = veredicto_new(); D->ver_jb = veredicto_new();
    gtk_box_pack_start( GTK_BOX(pg), D->ver_hosking, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(pg), D->ver_jb, FALSE, FALSE, 0 );
    st = gtk_list_store_new( R_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );
    D->l_res = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    g_object_unref( st );
    columna( D->l_res, "Serie", R_SER, "Los residuos de su ecuación" );
    columna( D->l_res, "Media", R_MEDIA, "" );
    columna( D->l_res, "Desv. típica", R_SD, "" );
    columna( D->l_res, "Asimetría", R_ASIM, "" );
    columna( D->l_res, "Curtosis", R_CURT, "" );
    columna( D->l_res, "Fuera de ±2σ", R_FUERA2, "El porcentaje observado y, entre paréntesis, el esperado con la normal" );
    columna( D->l_res, "Ljung-Box", R_LB, "El último de la ACF de sus residuos" );
    gtk_box_pack_start( GTK_BOX(pg), D->l_res, FALSE, FALSE, 0 );
    gtk_notebook_append_page( GTK_NOTEBOOK(D->libreta), pg, gtk_label_new( "Residuos" ) );

    st = gtk_list_store_new( P_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );
    D->l_par = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    g_object_unref( st );
    columna( D->l_par, "Parámetro", P_NOM, "Como lo nombra drvarma: AR1[A<-B] es B en la ecuación de A" );
    columna( D->l_par, "Estimación", P_EST, "" );
    columna( D->l_par, "Error típico", P_SE, "" );
    columna( D->l_par, "t", P_T, "" );
    columna( D->l_par, "p", P_P, "" );
    gtk_notebook_append_page( GTK_NOTEBOOK(D->libreta), con_scroll( D->l_par ), gtk_label_new( "Parámetros" ) );

    D->t_sigma = sim_mono( "" );
    gtk_widget_set_valign( D->t_sigma, GTK_ALIGN_START );
    gtk_widget_set_margin_start( D->t_sigma, 8 );
    gtk_widget_set_margin_top( D->t_sigma, 8 );
    gtk_notebook_append_page( GTK_NOTEBOOK(D->libreta), D->t_sigma, gtk_label_new( "Σ" ) );

    /* S7: simplificar y «¿es una red?» */
    pg = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    gtk_container_set_border_width( GTK_CONTAINER(pg), 8 );
    {
        GtkWidget *fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
        D->b_simpl = gtk_button_new_with_label( "Simplificar (|t| < 1)" );
        gtk_widget_set_tooltip_text( D->b_simpl, "Tiao y Box: los coeficientes cruzados con |t| < 1 a "
                                     "cero (drvarma -zero) y una estimación nueva, hija de ésta" );
        D->b_red = gtk_button_new_with_label( "¿Es una red?" );
        gtk_widget_set_tooltip_text( D->b_red, "Cada par a cero y cada orden triangular, contra este "
                                     "modelo, por LR. Si un orden se sostiene, es una red de transferencia" );
        D->b_pasar_mt = gtk_button_new_with_label( "Pasar a MT" );
        g_signal_connect_swapped( D->b_simpl, "clicked", G_CALLBACK(on_simpl), m );
        g_signal_connect_swapped( D->b_red, "clicked", G_CALLBACK(on_red), m );
        g_signal_connect_swapped( D->b_pasar_mt, "clicked", G_CALLBACK(on_pasar_mt), m );
        gtk_box_pack_start( GTK_BOX(fila), D->b_simpl, FALSE, FALSE, 0 );
        gtk_box_pack_start( GTK_BOX(fila), D->b_red, FALSE, FALSE, 0 );
        gtk_box_pack_start( GTK_BOX(fila), D->b_pasar_mt, FALSE, FALSE, 0 );
        gtk_box_pack_start( GTK_BOX(pg), fila, FALSE, FALSE, 0 );
    }
    D->ver_simpl = veredicto_new();
    D->ver_red = veredicto_new();
    D->t_red = sim_mono( "" );
    gtk_widget_set_valign( D->t_red, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX(pg), D->ver_simpl, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(pg), D->ver_red, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(pg), D->t_red, FALSE, FALSE, 0 );
    gtk_notebook_append_page( GTK_NOTEBOOK(D->libreta), pg, gtk_label_new( "Estructura" ) );

    D->t_out = gtk_text_view_new();
    gtk_text_view_set_editable( GTK_TEXT_VIEW(D->t_out), FALSE );
    gtk_text_view_set_monospace( GTK_TEXT_VIEW(D->t_out), TRUE );
    gtk_notebook_append_page( GTK_NOTEBOOK(D->libreta), con_scroll( D->t_out ), gtk_label_new( "Salida" ) );
    {
        int i;
        for (i = 0; i < gtk_notebook_get_n_pages( GTK_NOTEBOOK(D->libreta) ); i++)
            gtk_widget_show_all( gtk_notebook_get_nth_page( GTK_NOTEBOOK(D->libreta), i ) );
    }
    gtk_widget_set_no_show_all( D->b_pasar_mt, TRUE );
    gtk_widget_hide( D->b_pasar_mt );
    return caja;
}
