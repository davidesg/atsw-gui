/*
 * estructura.c -- S7: SIMPLIFICAR y «¿ES UNA RED?» (nodos N4s y N4t de sima,
 * simplify y structure; docs/DISENO-sim.md §3.2).
 *
 * SIMPLIFICAR es la de Tiao y Box (1981, §4), por coeficiente: los
 * coeficientes cruzados con |t| < 1 se fijan a cero (drvarma -zero) y se
 * estima de nuevo. Es una estimacion mas --un modelo hijo del que se mira,
 * con su razon--, y la diagnosis la contrasta contra su padre por LR. Varias
 * rondas son normales.
 *
 * «¿ES UNA RED?» (Tiao y Box, §3.1 y §5.2): sobre el modelo estimado, la LR
 * de que la serie B no entra en la ecuacion de A (todos sus retardos AR y MA
 * a cero, -links sin ese par), para cada par; y, con hasta cuatro series,
 * la de cada ORDEN TRIANGULAR (cada serie solo recibe de las de antes). Si
 * un orden se sostiene, el sistema es una RED DE TRANSFERENCIA --un VARMA
 * triangular es un modelo de transferencia-- y se ofrece «Pasar a MT» con los
 * mismos ficheros. No se pasa solo. El veredicto vale lo que valga el orden
 * del ajuste: con orden bajo puede salir una retroalimentacion espuria.
 *
 * Las corridas de la estructura no son modelos del sistema: van a
 * <modelo>_est<k>.* y no entran en el linaje. Lo que no necesita ventana
 * --que coeficientes, que modelos restringidos, la chi2-- es de lib/sistema.
 */

#include <math.h>
#include <string.h>
#include <glib/gstdio.h>

#include "sim.h"

/* --- simplificar ---------------------------------------------------------- */

/* Un coeficiente cruzado: «AR1[A<-B]» o «MA2[A<-B]». */
static gboolean cruzado( const char *nom )
{
    char k[3];
    int l;
    return sscanf( nom, "%2[ARM]%d[", k, &l ) == 2 && strstr( nom, "<-" ) &&
           ( !strcmp( k, "AR" ) || !strcmp( k, "MA" ) );
}

gboolean estructura_simplificar( Sim *m, double t )
{
    PagDiag *D = &m->pd;
    const VarmaOut *v = m->pe.v;
    GString *z;
    int i, n = 0;
    double ll;
    int k;
    char co[PR_ID];

    if (!v || v->fallo || m->pe.corriendo || D->corriendo) return FALSE;
    if (v->optim == OV_OPT_PARED_MA) {
        sim_estado( m, "El modelo se paró en la pared MA: sus t no son de un óptimo. Antes, "
                       "otro modelo («4 · Modelo»)." );
        return FALSE;
    }
    z = g_string_new( m->mod.zero );
    for (i = 0; i < v->npar; i++) {
        const OvParam *x = &v->par[i];
        if (!cruzado( x->nombre ) || !x->tiene_se || x->se <= 0) continue;
        if (fabs( x->t ) < t) {
            g_string_append_printf( z, "%s%s", z->len ? "," : "", x->nombre );
            n++;
        }
    }
    if (n == 0) {
        sim_estado( m, "Ningún coeficiente cruzado tiene |t| < %g: no hay qué simplificar.", t );
        g_string_free( z, TRUE );
        return FALSE;
    }
    if (z->len >= sizeof m->mod.zero) {
        sim_estado( m, "Demasiados ceros para una orden." );
        g_string_free( z, TRUE );
        return FALSE;
    }
    ll = v->logl;
    k = v->npar;
    snprintf( co, sizeof co, "%s", m->pe.corrida );
    snprintf( m->mod.zero, sizeof m->mod.zero, "%s", z->str );
    g_string_free( z, TRUE );
    if (!estima_lanzar( m )) return FALSE;

    /* El padre, para la LR de la diagnosis; y la razon, con proyecto. */
    D->hay_ant = TRUE;
    D->ll_ant = ll;
    D->k_ant = k;
    snprintf( D->co_ant, sizeof D->co_ant, "%s", co );
    D->ver_simpl_de = m->pe.version + 1;
    if (m->proy && m->caso[0] && m->pe.corrida[0]) {
        char razon[PR_RAZON];
        PrError e;
        snprintf( razon, sizeof razon, "Simplificación (Tiao y Box): %d coeficiente%s cruzado%s con "
                  "|t| < %g a cero.", n, n == 1 ? "" : "s", n == 1 ? "" : "s", t );
        pr_corrida_razon( m->proy, m->caso, m->pe.corrida, razon, &e );
        pr_escribir( m->proy, m->proy->path, &e );
    }
    sim_estado( m, "Simplificar: %d coeficiente%s cruzado%s con |t| < %g a cero; drvarma estima…",
                n, n == 1 ? "" : "s", n == 1 ? "" : "s", t );
    return TRUE;
}

/* --- «¿es una red?» -------------------------------------------------------- */

static void libera( PagDiag *D )
{
    g_free( D->jobs ); D->jobs = NULL;
    g_free( D->etiq ); D->etiq = NULL;
    g_free( D->orden ); D->orden = NULL;
    g_free( D->perm ); D->perm = NULL;
    g_free( D->ll ); D->ll = NULL;
    g_free( D->kk ); D->kk = NULL;
    D->njobs = D->cur = 0;
}

static gboolean pulso( gpointer d )
{
    Sim *m = d;
    if (!m->pd.corriendo || !m->avance) return G_SOURCE_REMOVE;
    gtk_progress_bar_pulse( GTK_PROGRESS_BAR(m->avance) );
    return G_SOURCE_CONTINUE;
}

static gboolean lanza( Sim *m );

static void on_done( const EngineResult *r, gpointer data )
{
    Sim *m = data;
    PagDiag *D = &m->pd;
    gchar *out = g_strdup_printf( "%s%d.out", D->base, D->cur );
    VarmaOut *v = g_new0( VarmaOut, 1 );

    (void) r;
    D->trabajo = NULL;
    if (D->ll && ov_parse_file( out, v ) == 0 && v->tiene_logl && !v->fallo) {
        D->ll[D->cur] = v->logl;
        D->kk[D->cur] = v->npar;
    }
    g_free( v );
    g_free( out );
    D->cur++;
    if (m->ventana && D->cur < D->njobs && lanza( m )) return;

    D->corriendo = FALSE;
    if (!m->ventana || !D->b_red) return;   /* la ventana se cerro: nada que ensenar */
    gtk_widget_hide( m->avance );
    D->version_red = m->pe.version;
    sim_refresca( m );
    sim_estado( m, "«¿Es una red?»: %d contrastes hechos.", D->njobs );
}

static gboolean lanza( Sim *m )
{
    PagDiag *D = &m->pd;
    const char *av[SIS_MAX_SER + 32];
    char buf[4096];
    gchar *sal = g_strdup_printf( "%s%d", D->base, D->cur ), *dir;
    int k = sis_orden_modelo( &m->sis, &D->jobs[D->cur], sal, buf, sizeof buf, av,
                              G_N_ELEMENTS(av) - 1 );

    if (k < 0) { g_free( sal ); return FALSE; }
    av[k] = NULL;
    dir = g_path_get_dirname( sal );
    D->trabajo = engine_start( dir, "drvarma", av, NULL, NULL, on_done, m );
    g_free( dir );
    g_free( sal );
    return D->trabajo != NULL;
}

/* Las permutaciones de 0..m-1, en orden lexicografico. */
static gboolean siguiente( int *a, int n )
{
    int i = n - 2, j = n - 1, t;
    while (i >= 0 && a[i] >= a[i + 1]) i--;
    if (i < 0) return FALSE;
    while (a[j] <= a[i]) j--;
    t = a[i]; a[i] = a[j]; a[j] = t;
    for (j = n - 1, i++; i < j; i++, j--) { t = a[i]; a[i] = a[j]; a[j] = t; }
    return TRUE;
}

gboolean estructura_red( Sim *m, double alfa )
{
    PagDiag *D = &m->pd;
    const VarmaOut *v = m->pe.v;
    int n = m->sis.n, a, b, max, perm[SIS_MAX_SER], i;
    gchar *nom, *mot;

    (void) alfa;
    if (!v || v->fallo || D->corriendo || m->pe.corriendo) return FALSE;
    if (m->mod.p + m->mod.q == 0 || sis_enlaces( &m->mod, n ) == 0) {
        sim_estado( m, "El modelo no tiene dinámica cruzada: no hay qué contrastar." );
        return FALSE;
    }
    if (!( mot = sim_hermano( "drvarma" ) )) { sim_estado( m, "No pude lanzar drvarma." ); return FALSE; }
    g_free( mot );

    libera( D );
    D->tri = 0;
    max = n * ( n - 1 ) + ( n <= 4 ? 24 : 0 );
    D->jobs = g_new0( SisModelo, max );
    D->etiq = g_malloc0( sizeof *D->etiq * (size_t) max );
    D->orden = g_new0( int, max );
    D->perm = g_malloc0( sizeof *D->perm * (size_t) max );
    D->ll = g_new0( double, max );
    D->kk = g_new0( int, max );
    for (i = 0; i < max; i++) D->ll[i] = NAN;

    /* Cada par enlazado, a cero. */
    for (a = 0; a < n; a++)
        for (b = 0; b < n; b++) {
            if (a == b || !m->mod.enlace[a][b]) continue;
            sis_modelo_sin_par( &m->mod, &D->jobs[D->njobs], a, b );
            snprintf( D->etiq[D->njobs], sizeof D->etiq[0], "%s no entra en %s",
                      m->sis.s[b].nombre, m->sis.s[a].nombre );
            D->njobs++;
        }
    /* Con hasta cuatro series, cada orden triangular que quita algo. */
    if (n <= 4) {
        for (i = 0; i < n; i++) perm[i] = i;
        do {
            SisModelo r;
            GString *g;
            /* Un orden que no quita nada: el modelo YA es triangular. */
            if (sis_modelo_triangular( &m->mod, &r, perm, n ) == 0) {
                if (!D->tri) { D->tri = 1; memcpy( D->perm_tri, perm, sizeof perm ); }
                continue;
            }
            g = g_string_new( "orden " );
            for (i = 0; i < n; i++) g_string_append_printf( g, "%s%s", i ? " → " : "", m->sis.s[perm[i]].nombre );
            D->jobs[D->njobs] = r;
            D->orden[D->njobs] = 1;
            memcpy( D->perm[D->njobs], perm, sizeof perm );
            snprintf( D->etiq[D->njobs], sizeof D->etiq[0], "%s", g->str );
            g_string_free( g, TRUE );
            D->njobs++;
        } while (siguiente( perm, n ));
    }

    g_free( D->base );
    nom = g_strdup_printf( "%s_est", m->pe.corrida[0] ? m->pe.corrida : "modelo" );
    D->base = sim_artefacto( m, nom );
    g_free( nom );
    D->cur = 0;
    D->orden_ok = -1;
    if (!lanza( m )) { sim_estado( m, "No pude lanzar drvarma." ); return FALSE; }
    D->corriendo = TRUE;
    if (m->avance) { gtk_widget_show( m->avance ); g_timeout_add( 120, pulso, m ); }
    sim_estado( m, "«¿Es una red?»: %d modelos restringidos que estimar…", D->njobs );
    sim_refresca( m );
    return TRUE;
}

gboolean estructura_pasar_mt( Sim *m, char *why, size_t n )
{
    PagDiag *D = &m->pd;
    const PrCaso *c;
    PrEntrada en[PR_MAX_ENTRADA];
    char id[PR_ID], razon[PR_RAZON];
    PrError e;
    int i, k, ns = m->sis.n, *perm;

    if (why && n) why[0] = 0;
    if (D->orden_ok < 0 && !D->tri) { snprintf( why, n, "Ningún orden triangular se sostiene." ); return FALSE; }
    if (!m->proy || !m->caso[0] || !( c = pr_caso_ver( m->proy, m->caso ) )) {
        snprintf( why, n, "«Pasar a MT» da de alta un caso del proyecto: abre el sistema desde la madre." );
        return FALSE;
    }
    if (c->nen != ns) { snprintf( why, n, "El sistema no es el del proyecto." ); return FALSE; }
    /* LA SALIDA PRIMERO: la ultima del orden, que recibe de todas. */
    perm = D->tri ? D->perm_tri : D->perm[D->orden_ok];
    for (i = 0; i < ns; i++) en[i] = c->en[perm[ns - 1 - i]];
    snprintf( razon, sizeof razon, "Pasado de %s (SIM): %s; es una red de transferencia.", c->id,
              D->tri ? "el modelo ya es triangular" : D->etiq[D->orden_ok] );
    if (pr_caso_add( m->proy, en, ns, c->muestra, "drtran", c->titulo, razon, id, sizeof id, &e ) != 0) {
        pr_error_es( &e, why, n );
        return FALSE;
    }
    k = pr_caso_idx( m->proy, id );
    if (k >= 0) snprintf( m->proy->ca[k].padre, PR_ID, "%s", c->id );
    if (pr_escribir( m->proy, m->proy->path, &e ) != 0) { pr_error_es( &e, why, n ); return FALSE; }
    snprintf( why, n, "Caso %s (MT): las series en el orden de la red, la salida primero. Se estima "
              "en drtran_gui, desde la madre.", id );
    return TRUE;
}

/* --- la vista ------------------------------------------------------------- */

void estructura_refresca( Sim *m )
{
    PagDiag *D = &m->pd;
    const VarmaOut *v = m->pe.v;
    gboolean hay = v && !v->fallo && !m->pe.corriendo;

    if (!D->b_red) return;
    gtk_widget_set_sensitive( D->b_simpl, hay && !D->corriendo );
    gtk_widget_set_sensitive( D->b_red, hay && !D->corriendo );

    /* CON UN VEC, Simplificar y «¿es una red?» no son suyos: son preguntas
     * sobre los enlaces del VARMA.                                       */
    if (m->pe.vec && !m->pe.corriendo) {
        gtk_widget_hide( D->b_pasar_mt );
        gtk_widget_hide( D->b_simpl );
        gtk_widget_hide( D->b_red );
        gtk_widget_show( D->b_weakex );
        sim_veredicto( D->ver_simpl, SIM_AMBAR, "Simplificar y «¿Es una red?» son del VARMA (sus "
                       "enlaces). En el VEC, las preguntas de estructura son restricciones sobre α "
                       "y β, contrastadas con la normalización del VEC estimado. Las demás "
                       "(β fija, Γ a cero) las hace sima (test_restriction)." );
        gtk_label_set_text( GTK_LABEL(D->ver_red), "" );
        gtk_label_set_text( GTK_LABEL(D->t_red), "" );
        restricciones_refresca( m );
        return;
    }
    gtk_widget_show( D->b_simpl );
    gtk_widget_show( D->b_red );
    gtk_widget_hide( D->b_weakex );
    gtk_label_set_text( GTK_LABEL(D->ver_wx), "" );
    gtk_label_set_text( GTK_LABEL(D->t_wx), "" );

    /* Simplificar: la LR contra el padre, cuando la estimacion es la hija. */
    if (D->hay_ant && v && !v->fallo && m->pe.version == D->ver_simpl_de && v->tiene_logl) {
        double lr = 2.0 * ( D->ll_ant - v->logl );
        int df = D->k_ant - v->npar;
        double p = sis_chi2_p( lr > 0 ? lr : 0, df );
        sim_veredicto( D->ver_simpl, p > 0.05 ? SIM_VERDE : SIM_AMBAR,
                       "Contra %s (antes de simplificar): LR = %.4g con %d gl, p = %.4f; AIC %.2f → %.2f. %s",
                       D->co_ant[0] ? D->co_ant : "el modelo anterior", lr, df, p,
                       -2 * D->ll_ant + 2 * D->k_ant, -2 * v->logl + 2 * v->npar,
                       p > 0.05 ? "Los ceros se sostienen: el modelo simple vale lo mismo." :
                                  "Los ceros NO se sostienen: el modelo simple pierde ajuste." );
    } else if (D->corriendo || !v)
        gtk_label_set_text( GTK_LABEL(D->ver_simpl), "" );
    else
        sim_veredicto( D->ver_simpl, SIM_AMBAR, "Simplificar: los coeficientes cruzados con |t| < 1 a "
                       "cero, y una estimación nueva (hija de ésta), contrastada contra ella." );

    /* «¿Es una red?» */
    gtk_widget_hide( D->b_pasar_mt );
    if (D->corriendo) {
        gchar *t = g_strdup_printf( "Contrastando %d de %d…", D->cur + 1, D->njobs );
        sim_veredicto( D->ver_red, SIM_AMBAR, "%s", t );
        g_free( t );
        return;
    }
    if (!D->ll || D->version_red != m->pe.version || !v || !v->tiene_logl) {
        sim_veredicto( D->ver_red, SIM_AMBAR, "«¿Es una red?»: cada par a cero y cada orden triangular, "
                       "contra el modelo estimado, por LR." );
        gtk_label_set_text( GTK_LABEL(D->t_red), "" );
        return;
    }
    {
        GString *g = g_string_new( NULL );
        int i, mejor = -1;
        double pm = -1;

        g_string_append_printf( g, "%-42s %10s %4s %8s\n", "restricción", "LR", "gl", "p" );
        for (i = 0; i < D->njobs; i++) {
            double lr, p;
            int df;
            if (isnan( D->ll[i] )) {
                g_string_append_printf( g, "%-42s   (falló la estimación)\n", D->etiq[i] );
                continue;
            }
            lr = 2.0 * ( v->logl - D->ll[i] );
            df = v->npar - D->kk[i];
            p = sis_chi2_p( lr > 0 ? lr : 0, df );
            g_string_append_printf( g, "%-42s %10.4f %4d %8.4f%s\n", D->etiq[i], lr, df, p,
                                    p > 0.05 ? "   se sostiene" : "" );
            if (D->orden[i] && p > 0.05 && p > pm) { pm = p; mejor = i; }
        }
        gtk_label_set_text( GTK_LABEL(D->t_red), g->str );
        g_string_free( g, TRUE );
        D->orden_ok = mejor;
        if (D->tri) {
            GString *o = g_string_new( NULL );
            for (i = 0; i < m->sis.n; i++)
                g_string_append_printf( o, "%s%s", i ? " → " : "", m->sis.s[D->perm_tri[i]].nombre );
            sim_veredicto( D->ver_red, SIM_AMBAR, "ES UNA RED: el modelo ya es triangular (orden %s): "
                           "ninguna serie recibe de las que van después. Se puede estimar en MT.", o->str );
            g_string_free( o, TRUE );
            gtk_widget_show( D->b_pasar_mt );
            gtk_widget_set_sensitive( D->b_pasar_mt, m->proy && m->caso[0] );
        } else if (mejor >= 0) {
            sim_veredicto( D->ver_red, SIM_AMBAR, "ES UNA RED: el %s se sostiene (p = %.4f). Un VARMA "
                           "triangular es un modelo de transferencia: se puede estimar en MT.",
                           D->etiq[mejor], pm );
            gtk_widget_show( D->b_pasar_mt );
            gtk_widget_set_sensitive( D->b_pasar_mt, m->proy && m->caso[0] );
            gtk_widget_set_tooltip_text( D->b_pasar_mt, m->proy && m->caso[0]
                ? "Un caso MT hermano, con las mismas series en el orden de la red (la salida "
                  "primero), para estimarlo en drtran_gui desde la madre"
                : "Da de alta un caso del proyecto: abre el sistema desde la madre" );
        } else
            sim_veredicto( D->ver_red, SIM_VERDE, "Es un sistema SIMULTÁNEO: ningún orden triangular se "
                           "sostiene. (El veredicto vale lo que valga el orden del ajuste.)" );
    }
}
