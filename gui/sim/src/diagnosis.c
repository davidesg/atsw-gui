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
 * Simplificar (los |t| < 1 a cero) y «¿es una red?» estan en Estructura (S7).
 *
 * CON UN VEC (S10c) las mismas pestañas leen el .out de drvec (lib/outvec):
 * Ajuste dice la relacion de equilibrio --beta, normalizada sobre el bloque
 * Y1-- y como se ajusta cada serie a ella --alpha, con su t: negativo
 * corrige el error--; Residuos es igual (lib/outdiag lee la diagnosis de
 * drvec); Parametros, Σ y Salida, los del VEC. Estructura es del VARMA.
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

/* La de drvarma y la de drvec son la misma (lib/diagmv): d, y los nombres
 * de las series en el orden del .out (NULL: los del sistema).          */
static void residuos( Sim *m, const Diagnosis *d, gboolean tiene, const char (*nombres)[OVEC_NOMBRE] )
{
    PagDiag *D = &m->pd;
    GtkListStore *st = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(D->l_res) ));
    int i;

    gtk_list_store_clear( st );
    if (!tiene) {
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
        const char *nom = nombres && i < m->sis.n ? nombres[i]
                        : i < m->sis.n ? m->sis.s[i].nombre : x->nombre;
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

/* --- el VEC --------------------------------------------------------------- */

/* «LRM = 1.12 LRY − 5.16 IBO + 4.26 IDE»: la relacion j, despejando su serie
 * de Y1 (beta = [I_r ; beta_2]: la de Y1 lleva 1, las otras de Y1 cero). */
static void relacion( const VecOut *v, int j, GString *g )
{
    int y = v->m - v->r + j, i, primero = 1;

    g_string_append_printf( g, "%s =", v->serie[y] );
    for (i = 0; i < v->m - v->r; i++) {
        double c = -v->beta[i][j];
        g_string_append_printf( g, primero ? " %s%.3g %s" : " %s %.3g %s",
                                c < 0 ? "−" : ( primero ? "" : "+" ), c < 0 ? -c : c, v->serie[i] );
        primero = 0;
    }
}

static void ajuste_vec( Sim *m, const VecOut *v )
{
    PagDiag *D = &m->pd;
    GString *g = g_string_new( NULL ), *a = g_string_new( NULL );
    int i, j, corrige = 0;

    for (j = 0; j < v->nr; j++) {
        if (j) g_string_append( g, "; " );
        relacion( v, j, g );
        if (j < v->new_ && v->r == 1)
            g_string_append_printf( g, ", más un error estacionario de media %.4g", v->ew[j] );
    }
    sim_veredicto( D->ver_lr, SIM_VERDE, "VEC con r = %d, normalizado sobre %s. La relación de "
                   "equilibrio%s: %s.", v->r, m->pe.vec_norm, v->r == 1 ? "" : "es", g->str );

    /* alpha: como se ajusta cada serie (signo de Johansen: negativo corrige) */
    for (j = 0; j < v->nr; j++)
        for (i = 0; i < v->m; i++) {
            gchar *n = g_strdup_printf( "D.%s <- ec%d", v->serie[i], j + 1 );
            const OvecPar *x = ovec_param( v, n );
            double t = x ? x->t : 0.0;
            const char *dice = !x ? "" : fabs( t ) < 2 ? "no reacciona (|t| < 2)"
                             : v->alpha[i][j] < 0 ? "corrige el error" : "se aleja del equilibrio";
            g_string_append_printf( a, "%s%s %+.3g (t %.2f): %s", a->len ? "; " : "", v->serie[i],
                                    v->alpha[i][j], t, dice );
            corrige += x && fabs( t ) >= 2 && v->alpha[i][j] < 0;
            g_free( n );
        }
    sim_veredicto( D->ver_optim, corrige ? SIM_VERDE : SIM_AMBAR, "El ajuste al equilibrio (α): %s.%s",
                   a->str, corrige ? "" : " Ninguna serie corrige el error de forma significativa: "
                                          "la relación no tira del sistema." );
    if (v->puerta == 0)
        sim_veredicto( D->ver_raices, SIM_ROJO, "La puerta de drvec NO se verifica (diferencia %.3g).",
                       v->puerta_dif );
    else
        sim_veredicto( D->ver_raices, v->ultimo_conv == 1 ? SIM_VERDE : SIM_AMBAR,
                       "El optimizador %s en %d iteraciones (%s); %d de %d arranques, logL de %.6f "
                       "a %.6f. La puerta de drvec, verificada. AIC %.4f, BIC %.4f.",
                       v->ultimo_conv == 1 ? "convergió" : "se paró sin converger", v->ultimo_iter,
                       sim_criterio( v->ultimo_criterio ), v->arranques_conv, v->arranques,
                       v->busca_min, v->busca_max, v->aic, v->bic );
    g_string_free( g, TRUE );
    g_string_free( a, TRUE );
}

static void parametros_vec( Sim *m, const VecOut *v )
{
    GtkListStore *st = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(m->pd.l_par) ));
    int i;

    gtk_list_store_clear( st );
    for (i = 0; i < v->npar_t; i++) {
        const OvecPar *x = &v->par[i];
        GtkTreeIter it;
        gchar *a = g_strdup_printf( "%.6f", x->est ), *b = g_strdup_printf( "%.6f", x->se );
        gchar *c = g_strdup_printf( "%.3f", x->t ), *e = g_strdup_printf( "%.4f", x->p );
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it, P_NOM, x->nombre, P_EST, a, P_SE, b, P_T, c, P_P, e, -1 );
        g_free( a ); g_free( b ); g_free( c ); g_free( e );
    }
}

static void sigma_vec( Sim *m, const VecOut *v )
{
    GString *g = g_string_new( "Σ (covarianza de las innovaciones, en el orden del .out):\n" );
    int i, j;

    for (i = 0; i < v->m; i++) {
        g_string_append_printf( g, "  %-12s", v->serie[i] );
        for (j = 0; j < v->m; j++) g_string_append_printf( g, " %12.5g", v->sigma[i][j] );
        g_string_append_c( g, '\n' );
    }
    g_string_append( g, "\nCorrelaciones:\n" );
    for (i = 0; i < v->m; i++) {
        g_string_append_printf( g, "  %-12s", v->serie[i] );
        for (j = 0; j < v->m; j++) {
            double d = v->sigma[i][i] * v->sigma[j][j];
            g_string_append_printf( g, " %8.4f", d > 0 ? v->sigma[i][j] / sqrt( d ) : 0.0 );
        }
        g_string_append_c( g, '\n' );
    }
    gtk_label_set_text( GTK_LABEL(m->pd.t_sigma), g->str );
    g_string_free( g, TRUE );
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
    if (m->pe.vec && !m->pe.corriendo) {
        const VecOut *x = m->pe.vec;
        if (D->version == m->pe.version) return;
        D->version = m->pe.version;
        if (!x->hay_vec) {
            sim_veredicto( D->ver_pide, SIM_ROJO, "La estimación del VEC falló: no hay diagnosis "
                           "(«5 · Estimación»)." );
            gtk_widget_set_visible( D->libreta, FALSE );
            return;
        }
        gtk_widget_set_visible( D->libreta, TRUE );
        sim_veredicto( D->ver_pide, SIM_VERDE, "La diagnosis de %s, leída del .out de drvec.",
                       m->pe.corrida[0] ? m->pe.corrida : "la última estimación" );
        ajuste_vec( m, x );
        {
            /* drvec diagnostica W = (nabla Y2, beta'Y): las ultimas r filas son
               los errores de equilibrio, ec1..ecr, no las series de Y1.  */
            char nom[OVEC_MAX_SER][OVEC_NOMBRE];
            int i;
            for (i = 0; i < x->m && i < OVEC_MAX_SER; i++) {
                if (i < x->m - x->r) snprintf( nom[i], OVEC_NOMBRE, "∇%s", x->serie[i] );
                else                 snprintf( nom[i], OVEC_NOMBRE, "ec%d", i - ( x->m - x->r ) + 1 );
            }
            residuos( m, m->pe.vdiag, m->pe.vdiag != NULL, (const char (*)[OVEC_NOMBRE]) nom );
        }
        parametros_vec( m, x );
        sigma_vec( m, x );
        if (m->pe.out && g_file_get_contents( m->pe.out, &t, NULL, NULL )) {
            gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(D->t_out) ), t, -1 );
            g_free( t );
        }
        return;
    }
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
    residuos( m, &v->diag, v->tiene_diag, NULL );
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
        /* S10c: la del VEC */
        D->b_weakex = gtk_button_new_with_label( "Débil exogeneidad" );
        gtk_widget_set_tooltip_text( D->b_weakex, "Para cada serie, α a cero en su ecuación (drvec "
                                     "-weakex): ¿se ajusta al equilibrio, o solo lo empuja? LR "
                                     "contra el VEC libre, con su misma normalización" );
        g_signal_connect_swapped( D->b_weakex, "clicked", G_CALLBACK(restricciones_weakex), m );
        gtk_box_pack_start( GTK_BOX(fila), D->b_weakex, FALSE, FALSE, 0 );
        gtk_box_pack_start( GTK_BOX(pg), fila, FALSE, FALSE, 0 );
    }
    D->ver_simpl = veredicto_new();
    D->ver_red = veredicto_new();
    D->t_red = sim_mono( "" );
    gtk_widget_set_valign( D->t_red, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX(pg), D->ver_simpl, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(pg), D->ver_red, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(pg), D->t_red, FALSE, FALSE, 0 );
    D->ver_wx = veredicto_new();
    D->t_wx = sim_mono( "" );
    gtk_widget_set_valign( D->t_wx, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX(pg), D->ver_wx, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(pg), D->t_wx, FALSE, FALSE, 0 );
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
