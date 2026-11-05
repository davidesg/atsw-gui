/*
 * estima.c -- la pagina 5: LA ESTIMACION (nodo N3 de sima, estimate).
 *
 * Lanza drvarma con el modelo de la pagina 4 (la orden la arma lib/sistema)
 * y dice COMO ACABO, que no es lo mismo que «salio con 0»:
 *
 *   convergio                 un optimo: la diagnosis lo juzga
 *   se paro en la PARED MA    raices MA a 5e-5 del circulo unidad: salen
 *                             numeros, pero NO es un optimo (N4b, sima
 *                             study_estimation). No se presenta como uno.
 *   se paro sin converger     el criterio que lo paro
 *   la estimacion fallo       drvarma sale con 3 y lo dice en el .out
 *   la puerta fallo           sale con 5: nada encima se puede creer
 *
 * CON UN SISTEMA DEL PROYECTO, cada estimacion es una CORRIDA del caso
 * (c00, c01...), con su padre --la anterior-- en el linaje; su .out va al
 * directorio del sistema y no se pisa. Sin proyecto, a la cache con nombre
 * fijo: entonces no caben dos.
 */

#include <string.h>
#include <glib/gstdio.h>

#include "sim.h"

static void consola( Sim *m, const char *txt, gssize n )
{
    GtkTextBuffer *b;
    GtkTextIter fin;
    if (!m->pe.consola) return;
    b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(m->pe.consola) );
    gtk_text_buffer_get_end_iter( b, &fin );
    gtk_text_buffer_insert( b, &fin, txt, (gint) n );
}

static void on_salida( const char *txt, gsize len, gpointer data )
{
    consola( data, txt, (gssize) len );
}

static gboolean pulso( gpointer d )
{
    Sim *m = d;
    if (!m->pe.corriendo || !m->avance) return G_SOURCE_REMOVE;
    gtk_progress_bar_pulse( GTK_PROGRESS_BAR(m->avance) );
    return G_SOURCE_CONTINUE;
}

/* El modelo del .out a la pagina 4: p, q, Sigma, los enlaces y como se
 * trato la diagonal. Lo que el .out no dice (-m, -hessian, -lik) se deja. */
static void modelo_del_out( Sim *m, const VarmaOut *v )
{
    m->mod.p = v->p;
    m->mod.q = v->q;
    m->mod.diagcov = v->sigma_diagonal;
    m->mod.redet = v->det_reestimados;
    m->mod.fixarma = v->arma_fijos;
    sis_enlaces_de_texto( &m->sis, &m->mod, v->enlaces );
}

static void lee( Sim *m, const char *out )
{
    PagEstima *E = &m->pe;
    if (!E->v) E->v = g_new0( VarmaOut, 1 );
    if (ov_parse_file( out, E->v ) != 0) {
        g_free( E->v );
        E->v = NULL;
    }
    E->version++;
}

static void on_done( const EngineResult *r, gpointer data )
{
    Sim *m = data;
    PagEstima *E = &m->pe;

    E->corriendo = FALSE;
    E->trabajo = NULL;
    E->rc = r->status;
    if (!m->ventana) return;
    gtk_widget_hide( m->avance );
    lee( m, E->out );
    sim_refresca( m );
    {
        /* El veredicto, sin su punto de color. */
        const char *t = gtk_label_get_text( GTK_LABEL(E->ver_fin) ), *q = strstr( t, "●" );
        if (q) for (q += strlen( "●" ); *q == ' '; q++) ;
        sim_estado( m, "%s", q ? q : t );
    }
}

/* El nombre de la corrida: con un sistema del proyecto, una corrida nueva
 * del caso (y el manifiesto guardado); sin el, la cache. g_free.        */
static gchar *corrida_nueva( Sim *m, char *why, size_t n )
{
    PagEstima *E = &m->pe;

    if (m->proy && m->caso[0]) {
        char id[PR_ID], ruta[PR_RUTA];
        PrError e;
        gchar *dir, *base;

        if (pr_corrida_nueva( m->proy, m->caso, E->previa[0] ? E->previa : NULL, id, sizeof id,
                              ruta, sizeof ruta, &e ) != 0 ||
            pr_escribir( m->proy, m->proy->path, &e ) != 0) {
            pr_error_es( &e, why, n );
            return NULL;
        }
        snprintf( E->corrida, sizeof E->corrida, "%s", id );
        dir = g_path_get_dirname( ruta );
        g_mkdir_with_parents( dir, 0755 );
        g_free( dir );
        /* drvarma pone el .out: -o lleva el nombre sin extension. */
        base = g_strdup( ruta );
        if (g_str_has_suffix( base, ".out" )) base[strlen( base ) - 4] = 0;
        return base;
    }
    E->corrida[0] = 0;
    return sim_artefacto( m, "modelo" );
}

gboolean estima_lanzar( Sim *m )
{
    PagEstima *E = &m->pe;
    const char *av[SIS_MAX_SER + 24];
    char buf[2048], why[512];
    gchar *base, *dir, *mot;
    int k;

    if (E->corriendo || m->pp.corriendo) return FALSE;
    if (m->sis.n < 2) { sim_estado( m, "Hacen falta al menos dos series («1 · Series»)." ); return FALSE; }
    if (puerta_estado( m ) != 1) {
        sim_estado( m, "Antes, la puerta superada («2 · Puerta»): si el sistema diagonal no "
                       "reproduce los univariantes, nada de lo que se estime encima se puede creer." );
        return FALSE;
    }
    if (!( mot = sim_hermano( "drvarma" ) )) {
        sim_veredicto( E->ver_fin, SIM_ROJO, "No pude lanzar drvarma: ni al lado de sim_gui ni en "
                       "el PATH («make» en engines/drvarma)." );
        sim_estado( m, "No pude lanzar drvarma." );
        return FALSE;
    }
    g_free( mot );
    if (!( base = corrida_nueva( m, why, sizeof why ) )) {
        sim_estado( m, "No pude registrar la corrida: %s", why );
        return FALSE;
    }
    k = sis_orden_modelo( &m->sis, &m->mod, base, buf, sizeof buf, av, G_N_ELEMENTS(av) - 1 );
    if (k < 0) { g_free( base ); sim_estado( m, "La orden no cabe." ); return FALSE; }
    av[k] = NULL;

    g_free( E->out );
    E->out = g_strdup_printf( "%s.out", base );
    g_remove( E->out );
    dir = g_path_get_dirname( base );
    if (E->consola)
        gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(E->consola) ), "", -1 );
    E->trabajo = engine_start( dir, "drvarma", av, NULL, on_salida, on_done, m );
    g_free( dir );
    g_free( base );
    if (!E->trabajo) {
        sim_veredicto( E->ver_fin, SIM_ROJO, "No pude lanzar drvarma." );
        return FALSE;
    }
    /* La siguiente colgara de esta. */
    if (E->corrida[0]) snprintf( E->previa, sizeof E->previa, "%s", E->corrida );
    E->corriendo = TRUE;
    if (m->avance) { gtk_widget_show( m->avance ); g_timeout_add( 120, pulso, m ); }
    sim_estado( m, "drvarma estima%s%s…", E->corrida[0] ? " la corrida " : "",
                E->corrida[0] ? E->corrida : "" );
    sim_refresca( m );
    return TRUE;
}

gboolean estima_desde( Sim *m, const char *out )
{
    PagEstima *E = &m->pe;
    gchar *t = NULL;

    g_free( E->out );
    E->out = g_strdup( out );
    E->rc = 0;
    lee( m, out );
    if (!E->v) return FALSE;
    modelo_del_out( m, E->v );
    if (E->consola && g_file_get_contents( out, &t, NULL, NULL )) {
        gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW(E->consola) ), t, -1 );
        g_free( t );
    }
    sim_refresca( m );
    return TRUE;
}

static void on_estimar( GtkButton *b, Sim *m ) { (void) b; estima_lanzar( m ); }
static void on_parar( GtkButton *b, Sim *m )
{
    (void) b;
    if (m->pe.trabajo) engine_stop( m->pe.trabajo );
    sim_estado( m, "Detenido." );
}

/* --- refrescar ----------------------------------------------------------- */

void estima_refresca( Sim *m )
{
    PagEstima *E = &m->pe;
    const VarmaOut *v = E->v;
    gchar *ord;

    if (!E->b_estimar) return;
    gtk_widget_set_sensitive( E->b_estimar, !E->corriendo && !m->pp.corriendo );
    gtk_widget_set_sensitive( E->b_parar, E->corriendo );
    ord = m->sis.n >= 2 ? modelo_orden_texto( m ) : g_strdup( "" );
    gtk_label_set_text( GTK_LABEL(E->orden), ord );
    g_free( ord );

    if (m->proy && m->caso[0])
        sim_veredicto( E->ver_donde, SIM_VERDE, "Cada estimación es un modelo del sistema %s, "
                       "con su padre en el linaje%s%s.", m->caso,
                       E->corrida[0] ? "; el último, " : "", E->corrida );
    else
        sim_veredicto( E->ver_donde, SIM_AMBAR, "Sin proyecto, la estimación va a la caché con "
                       "un nombre fijo: la siguiente pisa a esta." );

    if (E->corriendo)
        sim_veredicto( E->ver_fin, SIM_AMBAR, "drvarma está estimando…" );
    else if (puerta_estado( m ) != 1 && !v)
        sim_veredicto( E->ver_fin, SIM_AMBAR, "Pide la puerta superada («2 · Puerta»)." );
    else if (!v && !E->out)
        sim_veredicto( E->ver_fin, SIM_AMBAR, "Todavía no se ha estimado: «Estimar» (Ctrl+E)." );
    else if (!v)
        sim_veredicto( E->ver_fin, SIM_ROJO, "drvarma salió con %d y no dejó un .out que leer.", E->rc );
    else if (E->rc == 5 || ( v->puerta.hay && !v->puerta.pasa ))
        sim_veredicto( E->ver_fin, SIM_ROJO, "La puerta falló: nada de lo que se estime encima se "
                       "puede creer («2 · Puerta»)." );
    else if (v->fallo || E->rc == 3)
        sim_veredicto( E->ver_fin, SIM_ROJO, "La estimación FALLÓ: %s. No hay resultados; revisa "
                       "el modelo («4 · Modelo»).", v->fallo_msg[0] ? v->fallo_msg : "drvarma salió con 3" );
    else if (v->optim == OV_OPT_PARED_MA)
        sim_veredicto( E->ver_fin, SIM_AMBAR, "Se paró en la PARED MA tras %d iteraciones: %d de %d "
                       "raíces MA a 5e-5 del círculo unidad. Salen números, pero NO es un óptimo; "
                       "prueba con menos q o sin ese enlace.", v->iteraciones, v->pared_n, v->pared_raices );
    else if (v->optim == OV_OPT_PARADO)
        sim_veredicto( E->ver_fin, SIM_AMBAR, "Se paró sin converger tras %d iteraciones (%s): no es "
                       "un óptimo seguro.", v->iteraciones, sim_criterio( v->criterio ) );
    else if (E->rc != 0)
        sim_veredicto( E->ver_fin, SIM_ROJO, "drvarma salió con %d.", E->rc );
    else if (v->tiene_lr)
        sim_veredicto( E->ver_fin, SIM_VERDE, "Convergió en %d iteraciones: logL = %.6f, %d parámetros. "
                       "Contra los univariantes, LR = %.4g (gl %d, p = %.4f). La diagnosis, en «6».",
                       v->iteraciones, v->logl, v->npar, v->lr, v->lr_df, v->lr_p );
    else
        sim_veredicto( E->ver_fin, SIM_VERDE, "Estimado: logL = %.6f, %d parámetros.", v->logl, v->npar );
}

/* --- la pagina ----------------------------------------------------------- */

static GtkWidget *veredicto_new( void )
{
    GtkWidget *l = gtk_label_new( "" );
    gtk_label_set_xalign( GTK_LABEL(l), 0 );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    return l;
}

GtkWidget *estima_pagina_new( Sim *m )
{
    PagEstima *E = &m->pe;
    GtkWidget *caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    GtkWidget *fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
    GtkWidget *sw = gtk_scrolled_window_new( NULL, NULL );

    gtk_container_set_border_width( GTK_CONTAINER(caja), 12 );
    E->b_estimar = gtk_button_new_with_label( "Estimar" );
    gtk_widget_set_tooltip_text( E->b_estimar, "drvarma con el modelo de «4 · Modelo» (Ctrl+E)" );
    E->b_parar = gtk_button_new_with_label( "Detener" );
    g_signal_connect( E->b_estimar, "clicked", G_CALLBACK(on_estimar), m );
    g_signal_connect( E->b_parar, "clicked", G_CALLBACK(on_parar), m );
    gtk_box_pack_start( GTK_BOX(fila), E->b_estimar, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(fila), E->b_parar, FALSE, FALSE, 0 );
    E->orden = sim_mono( "" );
    gtk_label_set_ellipsize( GTK_LABEL(E->orden), PANGO_ELLIPSIZE_MIDDLE );
    gtk_box_pack_start( GTK_BOX(fila), E->orden, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), fila, FALSE, FALSE, 0 );

    E->ver_fin = veredicto_new();
    E->ver_donde = veredicto_new();
    gtk_box_pack_start( GTK_BOX(caja), E->ver_fin, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), E->ver_donde, FALSE, FALSE, 0 );

    E->consola = gtk_text_view_new();
    gtk_text_view_set_editable( GTK_TEXT_VIEW(E->consola), FALSE );
    gtk_text_view_set_monospace( GTK_TEXT_VIEW(E->consola), TRUE );
    gtk_container_add( GTK_CONTAINER(sw), E->consola );
    gtk_box_pack_start( GTK_BOX(caja), sw, TRUE, TRUE, 0 );
    return caja;
}
