/*
 * restricciones.c -- S10c: LAS RESTRICCIONES DEL VEC (nodo C3 de sima,
 * test_restriction), en «6 · Diagnosis ▸ Estructura».
 *
 * La primera, la de Johansen y Juselius: la DEBIL EXOGENEIDAD de cada serie.
 * Su α a cero: la serie no se ajusta al equilibrio, solo lo empuja. Una
 * corrida de drvec por serie,
 *
 *   drvec Y2... Y1... p q r -case C -weakex i -name <modelo>_wx<i>
 *
 * que ajusta el VEC libre y el restringido y escribe la LR (lib/outvec la
 * lee). Con LA NORMALIZACION DEL VEC ESTIMADO: la logL exacta cambia con ella
 * (decision del analista, 2026-11-11), asi que si «4 · Modelo» tiene otra,
 * la pagina lo dice y no contrasta -- la LR seria de otra linea.
 *
 * i es la fila de la serie en la orden de drvec (nabla Y2 y luego Y1), no en
 * el sistema.
 */

#include <string.h>
#include <glib/gstdio.h>

#include "sim.h"

/* Lo de «4 · Modelo» es el VEC estimado: misma normalizacion, r, p, q, caso. */
static gboolean mismo_vec( Sim *m, char *why, size_t n )
{
    const VecOut *v = m->pe.vec;
    char nb[256];

    if (!v || !v->hay_vec) { snprintf( why, n, "no hay un VEC estimado" ); return FALSE; }
    sis_normalizacion( &m->sis, &m->mod, nb, sizeof nb );
    if (m->mod.clase != SIS_VEC || m->mod.r != v->r || m->mod.p != v->p || m->mod.q != v->q ||
        strcmp( nb, m->pe.vec_norm ) != 0) {
        snprintf( why, n, "«4 · Modelo» ya no es el VEC estimado (normalizado sobre %s, r = %d): "
                  "las restricciones se contrastan con ese, con su normalización. Vuelve a "
                  "estimar, o deja el modelo como estaba", m->pe.vec_norm, v->r );
        return FALSE;
    }
    return TRUE;
}

static gboolean lanza( Sim *m );

static void on_done( const EngineResult *r, gpointer data )
{
    Sim *m = data;
    PagDiag *D = &m->pd;
    gchar *out = g_strdup_printf( "%s%d.out", D->wx_base, D->wx_cur + 1 );
    VecOut *o = g_new0( VecOut, 1 );

    D->wx_trabajo = NULL;
    D->wx_ok[D->wx_cur] = 0;
    if (r->status == 0 && ovec_parse_file( out, o ) == 0 && o->nlr > 0) {
        D->wx_lr[D->wx_cur] = o->lr[0].lr;
        D->wx_gl[D->wx_cur] = o->lr[0].gl;
        D->wx_p[D->wx_cur] = o->lr[0].p;
        D->wx_ok[D->wx_cur] = 1;
    }
    g_free( o );
    g_free( out );
    if (!m->ventana) { D->wx_corriendo = FALSE; return; }
    D->wx_cur++;
    if (D->wx_cur < D->wx_n && lanza( m )) { sim_refresca( m ); return; }
    D->wx_corriendo = FALSE;
    D->wx_version = m->pe.version;
    if (m->avance) gtk_widget_hide( m->avance );
    sim_estado( m, "Débil exogeneidad: %d contrastes hechos.", D->wx_n );
    sim_refresca( m );
}

static gboolean lanza( Sim *m )
{
    PagDiag *D = &m->pd;
    const char *av[SIS_MAX_SER + 24];
    char buf[256], idx[16];
    gchar *base = g_strdup_printf( "%s%d", D->wx_base, D->wx_cur + 1 );
    gchar *nom = g_path_get_basename( base ), *dir = g_path_get_dirname( base );
    int k;

    k = sis_orden_vec( &m->sis, &m->mod, nom, buf, sizeof buf, av, G_N_ELEMENTS(av) - 4 );
    if (k < 2) { g_free( base ); g_free( nom ); g_free( dir ); return FALSE; }
    /* -weakex i delante de «-name NOMBRE» */
    snprintf( idx, sizeof idx, "%d", D->wx_cur + 1 );
    k -= 2;
    av[k++] = "-weakex";
    av[k++] = idx;
    av[k++] = "-name";
    av[k++] = nom;
    av[k] = NULL;
    D->wx_trabajo = engine_start( dir, "drvec", av, NULL, NULL, on_done, m );
    g_free( base ); g_free( nom ); g_free( dir );
    return D->wx_trabajo != NULL;
}

gboolean restricciones_weakex( Sim *m )
{
    PagDiag *D = &m->pd;
    const VecOut *v = m->pe.vec;
    char why[512];
    gchar *nom, *mot;
    int i;

    if (D->wx_corriendo || m->pe.corriendo) return FALSE;
    if (!mismo_vec( m, why, sizeof why )) { sim_estado( m, "Sin contraste: %s.", why ); return FALSE; }
    if (!( mot = sim_hermano( "drvec" ) )) { sim_estado( m, "No pude lanzar drvec." ); return FALSE; }
    g_free( mot );
    /* Las series en el orden de drvec: el del .out del VEC. */
    D->wx_n = v->m;
    for (i = 0; i < v->m; i++) {
        snprintf( D->wx_serie[i], sizeof D->wx_serie[i], "%s", v->serie[i] );
        D->wx_ok[i] = 0;
    }
    g_free( D->wx_base );
    nom = g_strdup_printf( "%s_wx", m->pe.corrida[0] ? m->pe.corrida : "modelo" );
    D->wx_base = sim_artefacto( m, nom );
    g_free( nom );
    D->wx_cur = 0;
    if (!lanza( m )) { sim_estado( m, "No pude lanzar drvec." ); return FALSE; }
    D->wx_corriendo = TRUE;
    if (m->avance) gtk_widget_show( m->avance );
    sim_estado( m, "Débil exogeneidad: %d corridas de drvec, una por serie…", D->wx_n );
    sim_refresca( m );
    return TRUE;
}

void restricciones_refresca( Sim *m )
{
    PagDiag *D = &m->pd;
    char why[512];
    GString *g, *ex;
    int i, hechos = 0;

    if (!D->b_weakex) return;
    gtk_widget_set_sensitive( D->b_weakex, !D->wx_corriendo && !m->pe.corriendo &&
                              m->pe.vec && m->pe.vec->hay_vec );
    if (D->wx_corriendo) {
        sim_veredicto( D->ver_wx, SIM_AMBAR, "Débil exogeneidad: contrastando %d de %d…",
                       D->wx_cur + 1, D->wx_n );
        gtk_label_set_text( GTK_LABEL(D->t_wx), "" );
        return;
    }
    if (D->wx_version != m->pe.version || D->wx_n == 0) {
        if (!mismo_vec( m, why, sizeof why ) && m->pe.vec)
            sim_veredicto( D->ver_wx, SIM_AMBAR, "Débil exogeneidad: %s.", why );
        else
            sim_veredicto( D->ver_wx, SIM_AMBAR, "Débil exogeneidad: para cada serie, su α a cero "
                           "(no se ajusta al equilibrio), contra el VEC libre, con la normalización "
                           "del VEC estimado (%s).", m->pe.vec_norm );
        gtk_label_set_text( GTK_LABEL(D->t_wx), "" );
        return;
    }
    g = g_string_new( NULL );
    ex = g_string_new( NULL );
    g_string_append_printf( g, "%-12s %10s %4s %8s\n", "α a cero en", "LR", "gl", "p" );
    for (i = 0; i < D->wx_n; i++) {
        if (!D->wx_ok[i]) { g_string_append_printf( g, "%-12s   (falló la corrida)\n", D->wx_serie[i] ); continue; }
        hechos++;
        g_string_append_printf( g, "%-12s %10.4f %4d %8.4f   %s\n", D->wx_serie[i], D->wx_lr[i],
                                D->wx_gl[i], D->wx_p[i],
                                D->wx_p[i] > 0.05 ? "débilmente exógena" : "se ajusta al equilibrio" );
        if (D->wx_p[i] > 0.05) g_string_append_printf( ex, "%s%s", ex->len ? ", " : "", D->wx_serie[i] );
    }
    gtk_label_set_text( GTK_LABEL(D->t_wx), g->str );
    if (!hechos)
        sim_veredicto( D->ver_wx, SIM_ROJO, "Débil exogeneidad: ninguna corrida salió." );
    else if (ex->len)
        sim_veredicto( D->ver_wx, SIM_VERDE, "Débilmente exógenas al 5 %%: %s. Empujan el equilibrio "
                       "y no se ajustan a él; contrastes uno a uno, con la normalización sobre %s.",
                       ex->str, m->pe.vec_norm );
    else
        sim_veredicto( D->ver_wx, SIM_VERDE, "Ninguna es débilmente exógena al 5 %%: todas se ajustan "
                       "al equilibrio (normalizado sobre %s).", m->pe.vec_norm );
    g_string_free( g, TRUE );
    g_string_free( ex, TRUE );
}
