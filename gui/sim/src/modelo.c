/*
 * modelo.c -- la pagina 4: EL MODELO (nodo N3 de sima, estimate).
 *
 * Un candidato son p y q de la dinamica cruzada, Sigma completa o diagonal,
 * y los ENLACES: que serie entra en la ecuacion de cual (-links "A<-B",
 * en AR y en MA, en todos los retardos hasta p y q). Sustituye a las
 * casillas «AR diagonal / MA diagonal» del GUI viejo: es mas fino y es lo
 * que propone la identificacion. La diagonal --el modelo de cada serie--
 * es la de su fichero de fue, reestimada con el resto salvo -fixarma.
 *
 * LA ORDEN SE VE ANTES DE LANZAR, copiable. Estimar llega en la fase S4;
 * lo que la orden dice ya es lo que se lanzara (lib/sistema la arma, y
 * atsw-cli y el asistente la armaran igual).
 */

#include <string.h>

#include "sim.h"

gchar *modelo_orden_texto( Sim *m )
{
    const char *av[SIS_MAX_SER + 24];
    char buf[2048];
    GString *g = g_string_new( "drvarma" );
    gchar *sal = sim_artefacto( m, "c01" );
    int k, i;

    k = sis_orden_modelo( &m->sis, &m->mod, sal, buf, sizeof buf, av, G_N_ELEMENTS(av) );
    for (i = 0; i < k; i++) {
        gchar *b = i < m->sis.n || i == k - 1 ? g_path_get_basename( av[i] ) : g_strdup( av[i] );
        gboolean comillas = strchr( b, '<' ) != NULL;
        g_string_append_printf( g, comillas ? " \"%s\"" : " %s", b );
        g_free( b );
    }
    g_free( sal );
    return g_string_free( g, FALSE );
}

/* --- leer los controles -------------------------------------------------- */

static void lee( Sim *m )
{
    PagModelo *M = &m->pm;
    if (M->leyendo) return;
    m->mod.p = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(M->s_p) );
    m->mod.q = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(M->s_q) );
    m->mod.diagcov = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(M->c_diag) );
    m->mod.redet = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(M->c_redet) );
    m->mod.fixarma = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(M->c_fixarma) );
    m->mod.metodo = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(M->c_m2) ) ? 2 : 1;
    m->mod.hessian = gtk_combo_box_get_active( GTK_COMBO_BOX(M->cb_hessian) );
    m->mod.lik = gtk_combo_box_get_active( GTK_COMBO_BOX(M->cb_lik) );
}

static void textos( Sim *m );

/* LOS CONTROLES NO SE REHACEN al tocar uno (se destruiria el que avisa):
 * solo la orden y los veredictos.                                      */
static void on_cambia( GtkWidget *w, Sim *m ) { (void) w; lee( m ); textos( m ); }

static void on_enlace( GtkToggleButton *t, Sim *m )
{
    int a = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(t), "a" ));
    int b = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(t), "b" ));
    if (m->pm.leyendo) return;
    m->mod.enlace[a][b] = gtk_toggle_button_get_active( t );
    textos( m );
}

static void todos( Sim *m, int valor )
{
    int a, b;
    for (a = 0; a < m->sis.n; a++)
        for (b = 0; b < m->sis.n; b++) m->mod.enlace[a][b] = a != b && valor;
    modelo_refresca( m );
}

static void on_todos( GtkButton *b, Sim *m )   { (void) b; todos( m, 1 ); }
static void on_ninguno( GtkButton *b, Sim *m ) { (void) b; todos( m, 0 ); }

/* --- refrescar ------------------------------------------------------------ */

static void vacia( GtkWidget *caja )
{
    GList *l = gtk_container_get_children( GTK_CONTAINER(caja) ), *p;
    for (p = l; p; p = p->next) gtk_widget_destroy( p->data );
    g_list_free( l );
}

void modelo_refresca( Sim *m )
{
    PagModelo *M = &m->pm;
    int a, b, n = m->sis.n;

    if (!M->s_p) return;
    M->leyendo = TRUE;
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(M->s_p), m->mod.p );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(M->s_q), m->mod.q );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(M->c_diag), m->mod.diagcov );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(M->c_redet), m->mod.redet );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(M->c_fixarma), m->mod.fixarma );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(M->c_m2), m->mod.metodo == 2 );
    gtk_combo_box_set_active( GTK_COMBO_BOX(M->cb_hessian), m->mod.hessian );
    gtk_combo_box_set_active( GTK_COMBO_BOX(M->cb_lik), m->mod.lik );

    /* UNA CASILLA POR PAR ORDENADO, «A ← B»: B entra en la ecuacion de A. */
    vacia( M->enlaces );
    for (a = 0; a < n; a++)
        for (b = 0; b < n; b++) {
            GtkWidget *c;
            gchar *t;
            if (a == b) continue;
            t = g_strdup_printf( "%s ← %s", m->sis.s[a].nombre, m->sis.s[b].nombre );
            c = gtk_check_button_new_with_label( t );
            g_free( t );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(c), m->mod.enlace[a][b] );
            g_object_set_data( G_OBJECT(c), "a", GINT_TO_POINTER(a) );
            g_object_set_data( G_OBJECT(c), "b", GINT_TO_POINTER(b) );
            g_signal_connect( c, "toggled", G_CALLBACK(on_enlace), m );
            gtk_widget_set_tooltip_text( c, "La segunda serie entra en la ecuación de la "
                                            "primera, en AR y MA, hasta p y q" );
            gtk_box_pack_start( GTK_BOX(M->enlaces), c, FALSE, FALSE, 0 );
        }
    gtk_widget_show_all( M->enlaces );
    M->leyendo = FALSE;
    textos( m );
}

static void textos( Sim *m )
{
    PagModelo *M = &m->pm;
    int n = m->sis.n, ne, tot;
    gchar *ord;

    if (!M->orden) return;

    if (n < 2) {
        gtk_label_set_text( GTK_LABEL(M->orden), "" );
        sim_veredicto( M->ver_cuenta, SIM_AMBAR, "Pide al menos dos series («1 · Series»)." );
        gtk_label_set_text( GTK_LABEL(M->ver_forma), "" );
        return;
    }
    ord = modelo_orden_texto( m );
    gtk_label_set_text( GTK_LABEL(M->orden), ord );
    g_free( ord );

    ne = sis_enlaces( &m->mod, n );
    tot = sis_parametros_cruzados( &m->mod, n );
    if (ne == 0 || m->mod.p + m->mod.q == 0)
        sim_veredicto( M->ver_cuenta, SIM_AMBAR,
                       "Sin dinámica cruzada (%s): es el sistema diagonal, el de la puerta%s.",
                       ne == 0 ? "ningún enlace" : "p = q = 0",
                       m->mod.diagcov ? "" : ", con Σ completa" );
    else {
        int cruz = ne * ( m->mod.p + m->mod.q ), cov = tot - cruz;
        gchar *mas = m->mod.diagcov ? g_strdup( "" )
                   : g_strdup_printf( ", más %d covarianza%s", cov, cov == 1 ? "" : "s" );
        sim_veredicto( M->ver_cuenta, SIM_VERDE,
                       "%d enlace%s × (p + q = %d) = %d coeficientes cruzados%s; la diagonal es "
                       "la de cada fichero, %s.",
                       ne, ne == 1 ? "" : "s", m->mod.p + m->mod.q, cruz, mas,
                       m->mod.fixarma ? "fija en el .pre (-fixarma)" : "reestimada con el resto" );
        g_free( mas );
    }
    if (ne == 0)
        gtk_label_set_text( GTK_LABEL(M->ver_forma), "" );
    else if (sis_retroalimenta( &m->mod, n ))
        sim_veredicto( M->ver_forma, SIM_VERDE, "Hay retroalimentación (algún par en los dos sentidos): "
                       "un sistema simultáneo." );
    else
        sim_veredicto( M->ver_forma, SIM_AMBAR, "Todos los enlaces van en un solo sentido: es una red "
                       "de transferencia, que también se puede estimar en MT (drtran)." );
}

/* --- la pagina ----------------------------------------------------------- */

static GtkWidget *veredicto_new( void )
{
    GtkWidget *l = gtk_label_new( "" );
    gtk_label_set_xalign( GTK_LABEL(l), 0 );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    return l;
}

static GtkWidget *etiqueta( const char *t, const char *globo )
{
    GtkWidget *l = gtk_label_new( t );
    gtk_widget_set_halign( l, GTK_ALIGN_START );
    gtk_widget_set_tooltip_text( l, globo );
    return l;
}

GtkWidget *modelo_pagina_new( Sim *m )
{
    PagModelo *M = &m->pm;
    GtkWidget *caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    GtkWidget *g = gtk_grid_new();
    GtkWidget *fila, *exp, *av, *t, *b;

    sis_modelo_defecto( &m->mod, SIS_MAX_SER );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 12 );
    gtk_grid_set_row_spacing( GTK_GRID(g), 6 );
    gtk_grid_set_column_spacing( GTK_GRID(g), 10 );

    M->s_p = gtk_spin_button_new_with_range( 0, 12, 1 );
    M->s_q = gtk_spin_button_new_with_range( 0, 6, 1 );
    M->c_diag = gtk_check_button_new_with_label( "Σ diagonal" );
    gtk_widget_set_tooltip_text( M->c_diag, "Innovaciones sin correlación contemporánea "
                                 "(-diagcov). La identificación lo propone si la k = 0 queda "
                                 "dentro de la banda" );
    gtk_grid_attach( GTK_GRID(g), etiqueta( "p", "El orden AR de la dinámica cruzada" ), 0, 0, 1, 1 );
    gtk_grid_attach( GTK_GRID(g), M->s_p, 1, 0, 1, 1 );
    gtk_grid_attach( GTK_GRID(g), etiqueta( "q", "El orden MA de la dinámica cruzada" ), 2, 0, 1, 1 );
    gtk_grid_attach( GTK_GRID(g), M->s_q, 3, 0, 1, 1 );
    gtk_grid_attach( GTK_GRID(g), M->c_diag, 4, 0, 1, 1 );
    g_signal_connect( M->s_p, "value-changed", G_CALLBACK(on_cambia), m );
    g_signal_connect( M->s_q, "value-changed", G_CALLBACK(on_cambia), m );
    g_signal_connect( M->c_diag, "toggled", G_CALLBACK(on_cambia), m );
    gtk_box_pack_start( GTK_BOX(caja), g, FALSE, FALSE, 0 );

    t = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(t), "<b>Enlaces</b>   (A ← B: B entra en la ecuación de A)" );
    gtk_widget_set_halign( t, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX(caja), t, FALSE, FALSE, 0 );
    M->enlaces = gtk_box_new( GTK_ORIENTATION_VERTICAL, 2 );
    gtk_box_pack_start( GTK_BOX(caja), M->enlaces, FALSE, FALSE, 0 );
    fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    b = gtk_button_new_with_label( "Todos" );
    gtk_widget_set_tooltip_text( b, "Todos los pares: la orden no lleva -links" );
    g_signal_connect( b, "clicked", G_CALLBACK(on_todos), m );
    gtk_box_pack_start( GTK_BOX(fila), b, FALSE, FALSE, 0 );
    b = gtk_button_new_with_label( "Ninguno" );
    g_signal_connect( b, "clicked", G_CALLBACK(on_ninguno), m );
    gtk_box_pack_start( GTK_BOX(fila), b, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), fila, FALSE, FALSE, 0 );

    exp = gtk_expander_new( "Opciones avanzadas" );
    av = gtk_grid_new();
    gtk_grid_set_row_spacing( GTK_GRID(av), 4 );
    gtk_grid_set_column_spacing( GTK_GRID(av), 10 );
    M->c_redet = gtk_check_button_new_with_label( "Reestimar los deterministas (-redet)" );
    gtk_widget_set_tooltip_text( M->c_redet, "Por omisión quedan fijos en el .pre" );
    M->c_fixarma = gtk_check_button_new_with_label( "El ARMA univariante, fijo (-fixarma)" );
    gtk_widget_set_tooltip_text( M->c_fixarma, "Por omisión se reestima con la dinámica cruzada" );
    M->c_m2 = gtk_check_button_new_with_label( "Exacta sin truncar (-m 2)" );
    gtk_widget_set_tooltip_text( M->c_m2, "Por omisión, exacta con xi truncado en 1e-3" );
    M->cb_hessian = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(M->cb_hessian), "fdhess en el óptimo" );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(M->cb_hessian), "BFGS de la búsqueda" );
    M->cb_lik = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(M->cb_lik), "Mauricio (AS 311)" );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(M->cb_lik), "Shea (AS 242)" );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(M->cb_lik), "las dos, comparadas" );
    gtk_grid_attach( GTK_GRID(av), M->c_redet, 0, 0, 2, 1 );
    gtk_grid_attach( GTK_GRID(av), M->c_fixarma, 0, 1, 2, 1 );
    gtk_grid_attach( GTK_GRID(av), M->c_m2, 0, 2, 2, 1 );
    gtk_grid_attach( GTK_GRID(av), etiqueta( "Errores típicos", "-hessian" ), 0, 3, 1, 1 );
    gtk_grid_attach( GTK_GRID(av), M->cb_hessian, 1, 3, 1, 1 );
    gtk_grid_attach( GTK_GRID(av), etiqueta( "Verosimilitud", "-lik" ), 0, 4, 1, 1 );
    gtk_grid_attach( GTK_GRID(av), M->cb_lik, 1, 4, 1, 1 );
    g_signal_connect( M->c_redet, "toggled", G_CALLBACK(on_cambia), m );
    g_signal_connect( M->c_fixarma, "toggled", G_CALLBACK(on_cambia), m );
    g_signal_connect( M->c_m2, "toggled", G_CALLBACK(on_cambia), m );
    g_signal_connect( M->cb_hessian, "changed", G_CALLBACK(on_cambia), m );
    g_signal_connect( M->cb_lik, "changed", G_CALLBACK(on_cambia), m );
    gtk_container_add( GTK_CONTAINER(exp), av );
    gtk_box_pack_start( GTK_BOX(caja), exp, FALSE, FALSE, 0 );

    t = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(t), "<b>La orden</b>   (estimar llega con la fase S4)" );
    gtk_widget_set_halign( t, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX(caja), t, FALSE, FALSE, 0 );
    M->orden = sim_mono( "" );
    gtk_label_set_line_wrap( GTK_LABEL(M->orden), TRUE );
    gtk_box_pack_start( GTK_BOX(caja), M->orden, FALSE, FALSE, 0 );

    M->ver_cuenta = veredicto_new();
    M->ver_forma = veredicto_new();
    gtk_box_pack_start( GTK_BOX(caja), M->ver_cuenta, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), M->ver_forma, FALSE, FALSE, 0 );
    return caja;
}
