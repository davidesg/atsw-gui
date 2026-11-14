/*
 * rango.c -- la pagina R · Rango: EL RANGO DE COINTEGRACION (nodos N1c y C1
 * de sima, prepare_vec y rank_test), con drvec.
 *
 *   drvec A.pre B.pre ... p q 0 -case C -lrtest [-bootstrap B] -name <sistema>_rango
 *
 * Solo aparece cuando la pregunta existe (DISENO-sim.md §3.2): dos o mas
 * series, todas I(1) (d = 1, D = 0). Entonces, antes de un VARMA en
 * diferencias, ¿se mueven juntas a largo plazo? Si hay r relaciones de
 * cointegracion, el sistema es un VEC; con r = 0, el VARMA en diferencias
 * de siempre.
 *
 * drvec ajusta el sistema para cada r = 0 .. M-1 y contrasta r frente a r+1
 * por LR. Los valores criticos asintoticos son los de Johansen, para su
 * verosimilitud CONDICIONAL; esta es la EXACTA, y en muestras cortas solo
 * el BOOTSTRAP calibra lo que de verdad se calcula. El bootstrap tarda
 * (minutos): corre en el fondo, con su avance contado en el .boot, y se
 * puede detener.
 *
 * Lo que dice la pagina sale de lib/outvec: la tabla por rango, la de las
 * comparaciones y el rango que propone la regla de sima (propose_rank): el
 * primer r que no se rechaza al 5 %.
 *
 * El caso mixto (alguna serie I(0) dentro) no es el de por omision en sima
 * (prepare_vec, mixed=True) y aqui no se ofrece: la pagina dice por que.
 */

#include <math.h>
#include <string.h>

#include <glib/gstdio.h>

#include "sim.h"

enum { R_R, R_NPAR, R_LOGL, R_AIC, R_BIC, R_N };
enum { T_H0, T_LR, T_A5, T_B5, T_P, T_REPS, T_VER, T_N };

static const struct { const char *id, *rotulo; } CASOS[] = {
    { "pre", "el de cada .pre" },
    { "1",   "1 · sin constante" },
    { "2",   "2 · la constante en la relación" },
    { "3",   "3 · con deriva" } };

/* Las rutas y la configuracion: si cambian, la prueba es de otro sistema. */
static gchar *firma( Sim *m )
{
    PagRango *R = &m->pr;
    GString *g = g_string_new( NULL );
    int i;

    for (i = 0; i < m->sis.n; i++) {
        /* la ruta Y EL CONTENIDO: un .pre reestimado tiene la misma ruta */
        gchar *h = sim_sha( m->sis.s[i].ruta );
        g_string_append( g, m->sis.s[i].ruta );
        g_string_append_c( g, '@' );
        g_string_append( g, h ? h : "?" );
        g_string_append_c( g, '\n' );
        g_free( h );
    }
    g_string_append_printf( g, "p=%d q=%d caso=%s B=%d", R->cfg.p, R->cfg.q, R->cfg.caso, R->cfg.B );
    return g_string_free( g, FALSE );
}

gboolean rango_aplica( Sim *m, char *why, size_t n )
{
    return sis_rango_admite( &m->sis, why, n ) != 0;
}

int rango_estado( Sim *m )
{
    PagRango *R = &m->pr;
    gchar *f;
    int vale;

    if (!R->hecha || !R->firma || !R->v) return 0;
    f = firma( m );
    vale = strcmp( f, R->firma ) == 0;
    g_free( f );
    if (!vale) return 0;
    return R->v->nrango > 0 && (R->v->nasint > 0 || R->v->nboot > 0) ? 1 : -1;
}

/* Lo que hay en los controles, a cfg. */
static void lee_controles( Sim *m )
{
    PagRango *R = &m->pr;
    int k;

    if (!R->s_p) return;
    R->cfg.p = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(R->s_p) );
    R->cfg.q = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(R->s_q) );
    R->cfg.B = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(R->s_B) );
    k = gtk_combo_box_get_active( GTK_COMBO_BOX(R->c_caso) );
    if (k < 0 || k >= (int) G_N_ELEMENTS(CASOS)) k = 0;
    snprintf( R->cfg.caso, sizeof R->cfg.caso, "%s", CASOS[k].id );
}

static gchar *orden_texto( Sim *m )
{
    PagRango *R = &m->pr;
    const char *av[SIS_MAX_SER + 16];
    char buf[64];
    gchar *nom = sim_artefacto( m, "rango" ), *base = g_path_get_basename( nom );
    GString *g = g_string_new( "drvec" );
    int k, i;

    k = sis_orden_rango( &m->sis, &R->cfg, base, buf, sizeof buf, av, G_N_ELEMENTS(av) );
    for (i = 0; i < k; i++) {
        gchar *b = i < m->sis.n ? g_path_get_basename( av[i] ) : g_strdup( av[i] );
        g_string_append_c( g, ' ' );
        g_string_append( g, b );
        g_free( b );
    }
    g_free( nom ); g_free( base );
    return g_string_free( g, FALSE );
}

/* --- el motor ------------------------------------------------------------ */

static gboolean avance( gpointer d )
{
    Sim *m = d;
    PagRango *R = &m->pr;
    gchar *boot;
    int hechas, total;

    if (!R->corriendo || !m->ventana) { R->tic = 0; return G_SOURCE_REMOVE; }
    total = R->cfg.B * ( m->sis.n - 1 );
    if (R->cfg.B <= 0 || total <= 0) {
        gtk_progress_bar_pulse( GTK_PROGRESS_BAR(R->barra) );
        return G_SOURCE_CONTINUE;
    }
    boot = g_strdup_printf( "%s.boot", R->out );
    hechas = ovec_boot_hechas( boot );
    g_free( boot );
    if (hechas < 0) gtk_progress_bar_pulse( GTK_PROGRESS_BAR(R->barra) );
    else {
        gchar *t = g_strdup_printf( "bootstrap: %d de %d réplicas", hechas, total );
        gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR(R->barra), (double) hechas / total );
        gtk_progress_bar_set_text( GTK_PROGRESS_BAR(R->barra), t );
        g_free( t );
    }
    return G_SOURCE_CONTINUE;
}

static void on_done( const EngineResult *r, gpointer data )
{
    Sim *m = data;
    PagRango *R = &m->pr;
    gchar *out = g_strdup_printf( "%s.out", R->out );
    gboolean parado = R->parando;

    R->corriendo = FALSE;
    R->parando = FALSE;
    R->trabajo = NULL;
    if (!m->ventana) { g_free( out ); return; }
    gtk_widget_hide( R->barra );
    R->rc = r->status;
    if (!R->v) R->v = g_new0( VecOut, 1 );
    if (parado) {
        R->hecha = FALSE;
        sim_estado( m, "Prueba del rango detenida." );
    } else if (ovec_parse_file( out, R->v ) != 0 || R->v->nrango == 0) {
        R->hecha = FALSE;
        sim_estado( m, "drvec no escribió la prueba del rango (%s): %s", out, r->message );
    } else {
        R->hecha = TRUE;
        R->version++;
        sim_estado( m, "La prueba del rango, lista." );
    }
    g_free( out );
    sim_refresca( m );
}

gboolean rango_contrastar( Sim *m )
{
    PagRango *R = &m->pr;
    const char *av[SIS_MAX_SER + 16];
    char buf[64], why[512];
    gchar *dir, *base, *mot;
    int k;

    if (R->corriendo) return FALSE;
    if (!sim_caso_al_dia( m, why, sizeof why )) { sim_estado( m, "%s", why ); return FALSE; }
    if (why[0]) sim_estado( m, "%s", why );
    if (!rango_aplica( m, why, sizeof why )) { sim_estado( m, "Sin prueba del rango: %s.", why ); return FALSE; }
    mot = sim_hermano( "drvec" );
    if (!mot) {
        sim_veredicto( R->ver_propuesta, SIM_ROJO, "No pude lanzar drvec: ni al lado de sim_gui "
                       "ni en el PATH («make» en engines/drvec)." );
        sim_estado( m, "No pude lanzar drvec." );
        return FALSE;
    }
    g_free( mot );
    lee_controles( m );

    g_free( R->out );
    R->out = sim_artefacto( m, "rango" );
    dir  = g_path_get_dirname( R->out );
    base = g_path_get_basename( R->out );
    /* el .boot de una prueba anterior contaria como avance de esta */
    {
        gchar *b = g_strdup_printf( "%s.boot", R->out );
        g_remove( b );
        g_free( b );
    }
    k = sis_orden_rango( &m->sis, &R->cfg, base, buf, sizeof buf, av, G_N_ELEMENTS(av) - 1 );
    av[k] = NULL;
    g_free( R->firma );
    R->firma = firma( m );
    R->hecha = FALSE;

    R->trabajo = engine_start( dir, "drvec", av, NULL, NULL, on_done, m );
    g_free( dir ); g_free( base );
    if (!R->trabajo) {
        sim_veredicto( R->ver_propuesta, SIM_ROJO, "No pude lanzar drvec." );
        sim_estado( m, "No pude lanzar drvec." );
        return FALSE;
    }
    R->corriendo = TRUE;
    gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR(R->barra), 0.0 );
    gtk_progress_bar_set_text( GTK_PROGRESS_BAR(R->barra), "ajustando cada rango…" );
    gtk_widget_show( R->barra );
    if (!R->tic) R->tic = g_timeout_add( 1000, avance, m );
    if (R->cfg.B > 0)
        sim_estado( m, "drvec contrasta el rango, con %d réplicas de bootstrap (tarda minutos)…",
                    R->cfg.B );
    else
        sim_estado( m, "drvec contrasta el rango, sin bootstrap…" );
    sim_refresca( m );
    return TRUE;
}

static void on_contrastar( GtkButton *b, Sim *m ) { (void) b; rango_contrastar( m ); }

gboolean rango_al_modelo( Sim *m )
{
    PagRango *R = &m->pr;
    int al, r;

    if (rango_estado( m ) != 1) return FALSE;
    r = ovec_rango_propuesto( R->v, &al );
    if (r < 0) return FALSE;
    m->mod.p = R->cfg.p;
    m->mod.q = R->cfg.q;
    snprintf( m->mod.caso, sizeof m->mod.caso, "%s", R->cfg.caso );
    if (r >= 1) {
        m->mod.clase = SIS_VEC;
        m->mod.r = r;
        sis_vec_y1_defecto( &m->mod, m->sis.n );
        sim_estado( m, "Al modelo: un VEC con r = %d, normalizado sobre la%s última%s serie%s "
                    "(cámbialo en «4 · Modelo» antes de comparar).", r, r == 1 ? "" : "s",
                    r == 1 ? "" : "s", r == 1 ? "" : "s" );
    } else {
        /* r = 0: no hay relaciones; el VARMA en diferencias de siempre, con
           p - 1 retardos en las diferencias.                            */
        m->mod.clase = SIS_VARMA;
        m->mod.p = R->cfg.p > 1 ? R->cfg.p - 1 : 1;
        sim_estado( m, "Al modelo: r = 0, el VARMA en diferencias. Un VEC con r de la teoría se "
                    "elige en «4 · Modelo»." );
    }
    modelo_refresca( m );
    if (m->libro) gtk_notebook_set_current_page( GTK_NOTEBOOK(m->libro), PG_MODELO );
    sim_refresca( m );
    return TRUE;
}

static void on_al_modelo( GtkButton *b, Sim *m ) { (void) b; rango_al_modelo( m ); }

static void on_parar( GtkButton *b, Sim *m )
{
    (void) b;
    if (m->pr.trabajo) { m->pr.parando = TRUE; engine_stop( m->pr.trabajo ); }
}

static void on_cambia( GtkWidget *w, Sim *m ) { (void) w; lee_controles( m ); rango_refresca( m ); }

gboolean rango_desde( Sim *m, const char *out )
{
    PagRango *R = &m->pr;
    gchar *base;

    if (!R->v) R->v = g_new0( VecOut, 1 );
    if (ovec_parse_file( out, R->v ) != 0 || R->v->nrango == 0) return FALSE;
    /* La configuracion de la prueba que se lee, a los controles. */
    R->cfg.p = R->v->p > 0 ? R->v->p : R->cfg.p;
    R->cfg.q = R->v->q;
    if (R->v->caso[0]) snprintf( R->cfg.caso, sizeof R->cfg.caso, "%s", R->v->caso );
    R->cfg.B = R->v->B;
    base = g_strdup( out );
    if (g_str_has_suffix( base, ".out" )) base[strlen( base ) - 4] = 0;
    g_free( R->out );
    R->out = base;
    if (R->s_p) {
        int k;
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(R->s_p), R->cfg.p );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(R->s_q), R->cfg.q );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(R->s_B), R->cfg.B );
        for (k = 0; k < (int) G_N_ELEMENTS(CASOS); k++)
            if (!strcmp( CASOS[k].id, R->cfg.caso )) gtk_combo_box_set_active( GTK_COMBO_BOX(R->c_caso), k );
    }
    g_free( R->firma );
    R->firma = firma( m );
    R->rc = 0;
    R->hecha = TRUE;
    R->version++;
    return TRUE;
}

/* --- lo que se ve -------------------------------------------------------- */

static void tablas( Sim *m )
{
    PagRango *R = &m->pr;
    GtkListStore *a = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(R->l_rangos) ));
    GtkListStore *b = GTK_LIST_STORE(gtk_tree_view_get_model( GTK_TREE_VIEW(R->l_pruebas) ));
    const VecOut *v = R->v;
    int i, j, al, rp;

    gtk_list_store_clear( a );
    gtk_list_store_clear( b );
    if (rango_estado( m ) != 1) return;
    rp = ovec_rango_propuesto( v, &al );
    for (i = 0; i < v->nrango; i++) {
        GtkTreeIter it;
        gchar *r = g_strdup_printf( "%d", v->rango[i].r ), *n = g_strdup_printf( "%d", v->rango[i].npar );
        gchar *l = g_strdup_printf( "%.4f", v->rango[i].logL );
        gchar *x = g_strdup_printf( "%.4f", v->rango[i].aic ), *y = g_strdup_printf( "%.4f", v->rango[i].bic );
        gtk_list_store_append( a, &it );
        gtk_list_store_set( a, &it, R_R, r, R_NPAR, n, R_LOGL, l, R_AIC, x, R_BIC, y, -1 );
        g_free( r ); g_free( n ); g_free( l ); g_free( x ); g_free( y );
    }
    for (i = 0; i < v->nasint; i++) {
        const OvecPrueba *t = &v->asint[i], *o = NULL;
        GtkTreeIter it;
        gchar *h, *lr, *a5, *b5, *p, *reps;
        const char *ver;

        for (j = 0; j < v->nboot; j++) if (v->boot[j].r == t->r) o = &v->boot[j];
        h  = g_strdup_printf( "r = %d → %d", t->r, t->r + 1 );
        lr = g_strdup_printf( "%.2f", t->lr );
        a5 = t->hay_cv ? g_strdup_printf( "%.2f", t->cv5 ) : g_strdup( "—" );
        b5 = o && o->hay_cv ? g_strdup_printf( "%.2f", o->cv5 ) : g_strdup( "" );
        p  = o && o->p >= 0 ? g_strdup_printf( "%.3f", o->p ) : g_strdup( "" );
        reps = o ? g_strdup_printf( "%d", o->reps ) : g_strdup( "" );
        {
            const OvecPrueba *c = o ? o : t;
            /* LA PRUEBA ES SECUENCIAL: se para en el primer r que no se
               rechaza; lo de despues no cuenta.                       */
            ver = rp >= 0 && !al && t->r > rp ? "(la secuencia ya se paró)"
                : !c->hay_cv ? "sin valores críticos"
                : t->lr > c->cv5 ? "se rechaza r al 5 %" : "no se rechaza: aquí se para";
        }
        gtk_list_store_append( b, &it );
        gtk_list_store_set( b, &it, T_H0, h, T_LR, lr, T_A5, a5, T_B5, b5, T_P, p, T_REPS, reps,
                            T_VER, ver, -1 );
        g_free( h ); g_free( lr ); g_free( a5 ); g_free( b5 ); g_free( p ); g_free( reps );
    }
}

void rango_refresca( Sim *m )
{
    PagRango *R = &m->pr;
    char why[512];
    gboolean aplica;
    gchar *ord;
    int e;

    if (!R->pagina) return;
    aplica = rango_aplica( m, why, sizeof why );
    /* LA PAGINA SOLO ESTA CUANDO LA PREGUNTA EXISTE (§3.2). */
    gtk_widget_set_visible( R->pagina, aplica || R->corriendo );
    if (!aplica) return;

    ord = orden_texto( m );
    gtk_label_set_text( GTK_LABEL(R->orden), ord );
    g_free( ord );
    gtk_widget_set_sensitive( R->b_contrastar, !R->corriendo );
    gtk_widget_set_sensitive( R->b_parar, R->corriendo );
    gtk_widget_set_sensitive( R->s_p, !R->corriendo );
    gtk_widget_set_sensitive( R->s_q, !R->corriendo );
    gtk_widget_set_sensitive( R->s_B, !R->corriendo );
    gtk_widget_set_sensitive( R->c_caso, !R->corriendo );
    gtk_widget_set_sensitive( R->b_al_modelo, !R->corriendo && rango_estado( m ) == 1 );

    tablas( m );
    e = rango_estado( m );
    gtk_label_set_text( GTK_LABEL(R->ver_notas), "" );
    gtk_label_set_text( GTK_LABEL(R->ver_puerta), "" );

    if (R->corriendo && R->cfg.B > 0)
        sim_veredicto( R->ver_propuesta, SIM_AMBAR,
                       "drvec ajusta cada rango y calibra la LR con %d réplicas de bootstrap. "
                       "Tarda minutos; «Detener» la para.", R->cfg.B );
    else if (R->corriendo)
        sim_veredicto( R->ver_propuesta, SIM_AMBAR, "drvec ajusta cada rango…" );
    else if (e == 0 && R->hecha)
        sim_veredicto( R->ver_propuesta, SIM_AMBAR,
                       "El sistema o la configuración cambiaron después de la última prueba: "
                       "hay que volver a contrastar." );
    else if (e == 0)
        sim_veredicto( R->ver_propuesta, SIM_AMBAR,
                       "%d series I(1). ¿Se mueven juntas a largo plazo? «Contrastar el rango»: "
                       "drvec ajusta el sistema con r = 0 … %d relaciones y contrasta cada r "
                       "frente a r + 1.", m->sis.n, m->sis.n - 1 );
    else {
        const VecOut *v = R->v;
        int al, r = ovec_rango_propuesto( v, &al );
        const OvecPrueba *t = NULL;
        int i;

        for (i = 0; i < (v->nboot ? v->nboot : v->nasint); i++) {
            const OvecPrueba *x = v->nboot ? &v->boot[i] : &v->asint[i];
            if (x->r == r) t = x;
        }
        if (r < 0)
            sim_veredicto( R->ver_propuesta, SIM_ROJO,
                           "No se puede leer un rango: falta una comparación o sus valores "
                           "críticos. Mira la salida de drvec." );
        else if (al)
            sim_veredicto( R->ver_propuesta, SIM_AMBAR,
                           "Todas las comparaciones se rechazan al 5 %%: el rango es AL MENOS %d "
                           "(drvec no contrasta el rango completo).", r );
        else if (r == 0)
            sim_veredicto( R->ver_propuesta, SIM_VERDE,
                           "La prueba propone r = 0: no se rechaza que no haya cointegración "
                           "(LR %.2f ≤ %.2f, %s). El sistema es un VARMA en diferencias.",
                           t ? t->lr : 0.0, t ? t->cv5 : 0.0,
                           v->nboot ? "el 5 % del bootstrap" : "el 5 % asintótico" );
        else
            sim_veredicto( R->ver_propuesta, SIM_VERDE,
                           "La prueba propone r = %d: %d relaci%s de cointegración. La primera "
                           "comparación que no se rechaza al 5 %% es r = %d → %d (LR %.2f ≤ %.2f).",
                           r, r, r == 1 ? "ón" : "ones", r, r + 1,
                           t ? t->lr : 0.0, t ? t->cv5 : 0.0 );

        if (v->puerta == 1)
            sim_veredicto( R->ver_puerta, SIM_VERDE,
                           "La escalera de drvec parte de los univariantes: con r = 0 y todo "
                           "diagonal, la conjunta es la suma (diferencia %.2g).", v->puerta_dif );
        else if (v->puerta == 0)
            sim_veredicto( R->ver_puerta, SIM_ROJO,
                           "La puerta de drvec NO se verifica (diferencia %.3g): la prueba no "
                           "parte de los modelos de fue. Revisa la puerta y los .pre.",
                           v->puerta_dif );

        {
            GString *n = g_string_new( NULL );
            if (!v->nboot)
                g_string_append( n, "Sin bootstrap: los valores críticos son los asintóticos de "
                                    "Johansen, para su verosimilitud condicional; esta es la exacta, "
                                    "y en muestras cortas la prueba sale pequeña. Para calibrarla, "
                                    "réplicas de bootstrap (199 o más)." );
            else if (v->B < 199)
                g_string_append_printf( n, "Con %d réplicas el bootstrap es una exploración: el "
                                        "p-valor tiene un error de Monte Carlo de ±%.2f cerca del 5 %%. "
                                        "Para decidir, 199 o más.", v->B,
                                        2.0 * 0.218 / sqrt( (double) v->B ) );
            if (v->n_parado > 0)
                g_string_append_printf( n, "%s%d ajuste%s de la prueba se par%s sin converger: "
                                        "mira «Salida» antes de fiarte de su LR.", n->len ? "\n" : "",
                                        v->n_parado, v->n_parado == 1 ? "" : "s",
                                        v->n_parado == 1 ? "ó" : "aron" );
            gtk_label_set_text( GTK_LABEL(R->ver_notas), n->str );
            g_string_free( n, TRUE );
        }
    }
}

/* --- la pagina ----------------------------------------------------------- */

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

static GtkWidget *giro( GtkWidget *fila, const char *rotulo, const char *globo,
                        int min, int max, int val, Sim *m )
{
    GtkWidget *l = gtk_label_new( rotulo ), *s = gtk_spin_button_new_with_range( min, max, 1 );

    gtk_widget_set_tooltip_text( l, globo );
    gtk_widget_set_tooltip_text( s, globo );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(s), val );
    g_signal_connect( s, "value-changed", G_CALLBACK(on_cambia), m );
    gtk_box_pack_start( GTK_BOX(fila), l, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(fila), s, FALSE, FALSE, 0 );
    return s;
}

GtkWidget *rango_pagina_new( Sim *m )
{
    PagRango *R = &m->pr;
    GtkWidget *caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    GtkWidget *fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
    GtkWidget *fila2 = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
    GtkWidget *expl = gtk_label_new( "Si las series I(1) se mueven juntas a largo plazo, hay r "
        "relaciones de cointegración y el sistema es un VEC: las diferencias corrigen la "
        "distancia a ese equilibrio. drvec ajusta el sistema con r = 0, 1, … y contrasta cada r "
        "frente a r + 1 por razón de verosimilitudes." );
    GtkListStore *a = gtk_list_store_new( R_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                          G_TYPE_STRING, G_TYPE_STRING );
    GtkListStore *b = gtk_list_store_new( T_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                          G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );
    GtkWidget *l;
    guint k;

    sis_rango_defecto( &R->cfg );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 12 );
    gtk_label_set_line_wrap( GTK_LABEL(expl), TRUE );
    gtk_label_set_xalign( GTK_LABEL(expl), 0 );
    gtk_box_pack_start( GTK_BOX(caja), expl, FALSE, FALSE, 0 );

    R->s_p = giro( fila, "p", "El orden AR del VARMA sobre las series en niveles (en las "
                   "diferencias es p − 1)", 1, 12, R->cfg.p, m );
    R->s_q = giro( fila, "q", "El orden MA", 0, 6, R->cfg.q, m );
    l = gtk_label_new( "Caso" );
    gtk_widget_set_tooltip_text( l, "Lo determinista: sin constante (1), la constante dentro de "
                                    "la relación (2), con deriva (3), o el de cada .pre" );
    gtk_box_pack_start( GTK_BOX(fila), l, FALSE, FALSE, 0 );
    R->c_caso = gtk_combo_box_text_new();
    for (k = 0; k < G_N_ELEMENTS(CASOS); k++)
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(R->c_caso), CASOS[k].rotulo );
    gtk_combo_box_set_active( GTK_COMBO_BOX(R->c_caso), 0 );
    g_signal_connect( R->c_caso, "changed", G_CALLBACK(on_cambia), m );
    gtk_box_pack_start( GTK_BOX(fila), R->c_caso, FALSE, FALSE, 0 );
    R->s_B = giro( fila, "Réplicas", "Las del bootstrap paramétrico, que calibra la LR exacta. "
                   "0: sin bootstrap (los valores asintóticos, en segundos). 199 o más para "
                   "decidir; tarda minutos", 0, 9999, R->cfg.B, m );
    gtk_box_pack_start( GTK_BOX(caja), fila, FALSE, FALSE, 0 );

    R->b_contrastar = gtk_button_new_with_label( "Contrastar el rango" );
    gtk_widget_set_tooltip_text( R->b_contrastar, "drvec -lrtest, con el bootstrap si hay réplicas" );
    g_signal_connect( R->b_contrastar, "clicked", G_CALLBACK(on_contrastar), m );
    gtk_box_pack_start( GTK_BOX(fila2), R->b_contrastar, FALSE, FALSE, 0 );
    R->b_parar = gtk_button_new_with_label( "Detener" );
    g_signal_connect( R->b_parar, "clicked", G_CALLBACK(on_parar), m );
    gtk_box_pack_start( GTK_BOX(fila2), R->b_parar, FALSE, FALSE, 0 );
    R->orden = sim_mono( "" );
    gtk_widget_set_tooltip_text( R->orden, "La orden que se lanza (se puede copiar)" );
    gtk_label_set_ellipsize( GTK_LABEL(R->orden), PANGO_ELLIPSIZE_MIDDLE );
    gtk_box_pack_start( GTK_BOX(fila2), R->orden, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), fila2, FALSE, FALSE, 0 );

    R->barra = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text( GTK_PROGRESS_BAR(R->barra), TRUE );
    gtk_box_pack_start( GTK_BOX(caja), R->barra, FALSE, FALSE, 0 );

    R->ver_propuesta = veredicto_new();
    gtk_box_pack_start( GTK_BOX(caja), R->ver_propuesta, FALSE, FALSE, 0 );
    R->b_al_modelo = gtk_button_new_with_label( "Rango al modelo" );
    gtk_widget_set_tooltip_text( R->b_al_modelo, "El rango propuesto, p, q y el caso a «4 · Modelo»: "
                                 "un VEC con r ≥ 1, o el VARMA en diferencias con r = 0" );
    gtk_widget_set_halign( R->b_al_modelo, GTK_ALIGN_START );
    g_signal_connect( R->b_al_modelo, "clicked", G_CALLBACK(on_al_modelo), m );
    gtk_box_pack_start( GTK_BOX(caja), R->b_al_modelo, FALSE, FALSE, 0 );

    R->l_pruebas = gtk_tree_view_new_with_model( GTK_TREE_MODEL(b) );
    g_object_unref( b );
    columna( R->l_pruebas, "Contraste", T_H0, "H0: el rango es r, frente a r + 1" );
    columna( R->l_pruebas, "LR", T_LR, "2 [L(r + 1) − L(r)]" );
    columna( R->l_pruebas, "5 % asint.", T_A5, "El valor crítico asintótico de Johansen al 5 %" );
    columna( R->l_pruebas, "5 % bootstrap", T_B5, "El valor crítico al 5 % del bootstrap" );
    columna( R->l_pruebas, "p (bootstrap)", T_P, "El p-valor del bootstrap" );
    columna( R->l_pruebas, "Réplicas", T_REPS, "Las réplicas que salieron" );
    columna( R->l_pruebas, "", T_VER, "Lo que dice, al 5 %" );
    gtk_box_pack_start( GTK_BOX(caja), R->l_pruebas, FALSE, FALSE, 0 );

    R->l_rangos = gtk_tree_view_new_with_model( GTK_TREE_MODEL(a) );
    g_object_unref( a );
    columna( R->l_rangos, "r", R_R, "El rango: cuántas relaciones de cointegración" );
    columna( R->l_rangos, "Parámetros", R_NPAR, "Cuántos parámetros tiene el ajuste" );
    columna( R->l_rangos, "logL", R_LOGL, "La verosimilitud exacta del ajuste" );
    columna( R->l_rangos, "AIC", R_AIC, "Akaike, por observación" );
    columna( R->l_rangos, "BIC", R_BIC, "Schwarz, por observación" );
    gtk_box_pack_start( GTK_BOX(caja), R->l_rangos, FALSE, FALSE, 0 );

    R->ver_puerta = veredicto_new();
    R->ver_notas  = veredicto_new();
    gtk_box_pack_start( GTK_BOX(caja), R->ver_puerta, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), R->ver_notas, FALSE, FALSE, 0 );

    /* LA VISIBILIDAD LA LLEVA rango_refresca, no show_all: la pagina solo
     * esta cuando la pregunta existe.                                   */
    gtk_widget_show_all( caja );
    gtk_widget_hide( R->barra );
    gtk_widget_set_no_show_all( caja, TRUE );
    gtk_widget_set_no_show_all( R->barra, TRUE );
    R->pagina = caja;
    return caja;
}
