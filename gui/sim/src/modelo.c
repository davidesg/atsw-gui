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
 * LA ORDEN SE VE ANTES DE LANZAR, copiable: lo que dice es lo que se
 * lanzara (lib/sistema la arma, y atsw-cli y el asistente la armaran igual).
 *
 * LA CLASE (S10c): con series I(1) el modelo puede ser un VEC, con drvec: r
 * relaciones de cointegracion, el caso determinista y LA NORMALIZACION --las
 * r series del bloque Y1, sobre las que se normaliza beta--. La logL exacta
 * del VEC cambia con ella (decision del analista, 2026-11-11): se fija antes
 * de comparar, y dos VEC solo se comparan si la comparten.
 */

#include <string.h>

#include "sim.h"

static const struct { const char *id, *rotulo; } CASOS[] = {
    { "pre", "el de cada .pre" },
    { "1",   "1 · sin constante" },
    { "2",   "2 · la constante en la relación" },
    { "3",   "3 · con deriva" } };

/* La salida que tendra la proxima estimacion: con un sistema, su proxima
 * corrida (c00, c01... en el orden en que lib/proyecto las numera); sin
 * el, el nombre fijo de la cache. g_free.                               */
static gchar *salida( Sim *m )
{
    if (m->proy && m->caso[0]) {
        int i, k = 0;
        gchar *id, *r;
        for (i = 0; i < m->proy->nco; i++) k += !strcmp( m->proy->co[i].caso, m->caso );
        id = g_strdup_printf( "c%02d", k );
        r = sim_artefacto( m, id );
        g_free( id );
        return r;
    }
    return sim_artefacto( m, "modelo" );
}

gchar *modelo_orden_texto( Sim *m )
{
    const char *av[SIS_MAX_SER + 24];
    char buf[2048];
    GString *g = g_string_new( m->mod.clase == SIS_VEC ? "drvec" : "drvarma" );
    gchar *sal = salida( m );
    int k, i;

    if (m->mod.clase == SIS_VEC) {
        gchar *base = g_path_get_basename( sal );
        k = sis_orden_vec( &m->sis, &m->mod, base, buf, sizeof buf, av, G_N_ELEMENTS(av) );
        for (i = 0; i < k; i++) {
            gchar *b = i < m->sis.n ? g_path_get_basename( av[i] ) : g_strdup( av[i] );
            g_string_append_printf( g, " %s", b );
            g_free( b );
        }
        g_free( base );
        g_free( sal );
        if (k < 0) { g_string_free( g, TRUE ); return g_strdup( "" ); }
        return g_string_free( g, FALSE );
    }
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
    {
        int k = gtk_combo_box_get_active( GTK_COMBO_BOX(M->cb_caso) );
        int r = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(M->s_r) );
        m->mod.clase = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(M->rb_vec) ) ? SIS_VEC : SIS_VARMA;
        if (k < 0 || k >= (int) G_N_ELEMENTS(CASOS)) k = 0;
        snprintf( m->mod.caso, sizeof m->mod.caso, "%s", CASOS[k].id );
        /* Otro r: la normalizacion de por omision para ese r (las r ultimas). */
        if (r != m->mod.r) { m->mod.r = r; sis_vec_y1_defecto( &m->mod, m->sis.n ); }
    }
}

static void textos( Sim *m );

/* LOS CONTROLES NO SE REHACEN al tocar uno (se destruiria el que avisa):
 * solo la orden y los veredictos.                                      */
static void on_cambia( GtkWidget *w, Sim *m ) { (void) w; lee( m ); textos( m ); }

/* La clase o r cambian lo que se ve (y las casillas de Y1): eso si se rehace,
 * en un idle, despues de que el control que avisa termine.             */
static gboolean rehaz( gpointer d ) { sim_refresca( d ); return G_SOURCE_REMOVE; }
static void on_cambia_forma( GtkWidget *w, Sim *m )
{
    (void) w;
    if (m->pm.leyendo) return;
    lee( m );
    g_idle_add( rehaz, m );
}

static void on_y1( GtkToggleButton *t, Sim *m )
{
    int i = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(t), "i" ));
    if (m->pm.leyendo) return;
    m->mod.y1[i] = gtk_toggle_button_get_active( t );
    textos( m );
}

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

static void on_sin_ceros( Sim *m ) { m->mod.zero[0] = 0; modelo_refresca( m ); }

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

    /* LA CLASE: el VEC solo con series I(1) (lo que admite R · Rango). */
    {
        char why[512];
        gboolean vec_ok = rango_aplica( m, why, sizeof why );
        int k;
        if (!vec_ok && m->mod.clase == SIS_VEC) m->mod.clase = SIS_VARMA;
        gtk_widget_set_sensitive( M->rb_vec, vec_ok );
        gtk_widget_set_tooltip_text( M->rb_vec, vec_ok
            ? "Un VEC con drvec: r relaciones de cointegración entre las series en niveles"
            : "El VEC pide series I(1) (d = 1, D = 0): mira «R · Rango»" );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(m->mod.clase == SIS_VEC ? M->rb_vec : M->rb_varma), TRUE );
        gtk_widget_set_visible( M->caja_varma, m->mod.clase == SIS_VARMA );
        gtk_widget_set_visible( M->c_diag, m->mod.clase == SIS_VARMA );
        gtk_widget_set_visible( M->caja_vec, m->mod.clase == SIS_VEC );
        gtk_spin_button_set_range( GTK_SPIN_BUTTON(M->s_p), m->mod.clase == SIS_VEC ? 1 : 0, 12 );
        gtk_spin_button_set_range( GTK_SPIN_BUTTON(M->s_r), 1, n > 1 ? n - 1 : 1 );
        if (m->mod.r < 1) m->mod.r = 1;
        if (n > 1 && m->mod.r > n - 1) { m->mod.r = n - 1; sis_vec_y1_defecto( &m->mod, n ); }
        /* Y1 VA POR POSICION: si las series cambian (otra, otro orden), la
           posicion ya no dice la misma serie, y vuelve a la de por omision. */
        {
            GString *f = g_string_new( NULL );
            for (k = 0; k < n; k++) { g_string_append( f, m->sis.s[k].nombre ); g_string_append_c( f, '\n' ); }
            if (g_strcmp0( f->str, M->series_y1 ) != 0) {
                if (M->series_y1 && n > 1) sis_vec_y1_defecto( &m->mod, n );
                g_free( M->series_y1 );
                M->series_y1 = g_string_free( f, FALSE );
            } else
                g_string_free( f, TRUE );
        }
        if (n > 1 && sis_vec_ny1( &m->mod, n ) == 0) sis_vec_y1_defecto( &m->mod, n );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(M->s_r), m->mod.r );
        for (k = 0; k < (int) G_N_ELEMENTS(CASOS); k++)
            if (!strcmp( CASOS[k].id, m->mod.caso )) gtk_combo_box_set_active( GTK_COMBO_BOX(M->cb_caso), k );
        vacia( M->y1 );
        for (k = 0; k < n; k++) {
            GtkWidget *c = gtk_check_button_new_with_label( m->sis.s[k].nombre );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(c), m->mod.y1[k] );
            g_object_set_data( G_OBJECT(c), "i", GINT_TO_POINTER(k) );
            g_signal_connect( c, "toggled", G_CALLBACK(on_y1), m );
            gtk_widget_set_tooltip_text( c, "En el bloque Y1: beta se normaliza sobre ella" );
            gtk_box_pack_start( GTK_BOX(M->y1), c, FALSE, FALSE, 0 );
        }
        gtk_widget_show_all( M->y1 );
    }

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
    int n = m->sis.n, ne, tot, nz = 0;
    gchar *ord;

    if (!M->orden) return;
    estima_refresca( m );       /* la orden de «5 · Estimación» es esta misma */

    if (n < 2) {
        gtk_label_set_text( GTK_LABEL(M->orden), "" );
        sim_veredicto( M->ver_cuenta, SIM_AMBAR, "Pide al menos dos series («1 · Series»)." );
        gtk_label_set_text( GTK_LABEL(M->ver_forma), "" );
        return;
    }
    ord = modelo_orden_texto( m );
    gtk_label_set_text( GTK_LABEL(M->orden), ord );
    g_free( ord );

    if (m->mod.clase == SIS_VEC) {
        char nb[256];
        int ny = sis_vec_ny1( &m->mod, n );
        gtk_label_set_text( GTK_LABEL(M->ver_ceros), "" );
        gtk_widget_set_visible( M->b_sin_ceros, FALSE );
        gtk_label_set_text( GTK_LABEL(M->ver_forma), "" );
        sis_normalizacion( &m->sis, &m->mod, nb, sizeof nb );
        if (ny != m->mod.r)
            sim_veredicto( M->ver_vec, SIM_ROJO, "La normalización tiene que llevar %d serie%s "
                           "(r = %d); ahora lleva %d.", m->mod.r, m->mod.r == 1 ? "" : "s", m->mod.r, ny );
        else
            sim_veredicto( M->ver_vec, SIM_VERDE, "β se normaliza sobre %s: %s. La verosimilitud "
                           "exacta del VEC cambia con la normalización, así que dos VEC solo se "
                           "comparan si la comparten.", nb,
                           m->mod.r == 1 ? "la relación se lee como esa serie en función de las demás"
                                         : "cada relación se lee como una de ellas en función de las demás" );
        sim_veredicto( M->ver_cuenta, SIM_VERDE,
                       "VEC con r = %d relaci%s de cointegración, VARMA(%d, %d) en niveles (en las "
                       "diferencias, p − 1 = %d retardos), caso %s.", m->mod.r,
                       m->mod.r == 1 ? "ón" : "ones", m->mod.p, m->mod.q, m->mod.p - 1, m->mod.caso );
        return;
    }
    gtk_label_set_text( GTK_LABEL(M->ver_vec), "" );

    ne = sis_enlaces( &m->mod, n );
    tot = sis_parametros_cruzados( &m->mod, n );
    {
        char z[2048];
        nz = sis_ceros_validos( &m->sis, &m->mod, z, sizeof z );
        gtk_widget_set_visible( M->b_sin_ceros, nz > 0 );
        if (nz > 0) {
            sim_veredicto( M->ver_ceros, SIM_AMBAR, "%d coeficiente%s cruzado%s a cero (-zero, de "
                           "Simplificar): %s.", nz, nz == 1 ? "" : "s", nz == 1 ? "" : "s", z );
        } else
            gtk_label_set_text( GTK_LABEL(M->ver_ceros), "" );
    }
    if (ne == 0 || m->mod.p + m->mod.q == 0)
        sim_veredicto( M->ver_cuenta, SIM_AMBAR,
                       "Sin dinámica cruzada (%s): es el sistema diagonal, el de la puerta%s.",
                       ne == 0 ? "ningún enlace" : "p = q = 0",
                       m->mod.diagcov ? "" : ", con Σ completa" );
    else {
        int bruto = ne * ( m->mod.p + m->mod.q ), cov = tot - bruto, cruz = bruto - nz;
        gchar *mas = m->mod.diagcov ? g_strdup( "" )
                   : g_strdup_printf( ", más %d covarianza%s", cov, cov == 1 ? "" : "s" );
        gchar *ceros = nz ? g_strdup_printf( " − %d a cero", nz ) : g_strdup( "" );
        sim_veredicto( M->ver_cuenta, SIM_VERDE,
                       "%d enlace%s × (p + q = %d)%s = %d coeficientes cruzados%s; la diagonal es "
                       "la de cada fichero, %s.",
                       ne, ne == 1 ? "" : "s", m->mod.p + m->mod.q, ceros, cruz, mas,
                       m->mod.fixarma ? "fija en el .pre (-fixarma)" : "reestimada con el resto" );
        g_free( mas );
        g_free( ceros );
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

    /* LA CLASE */
    fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 12 );
    M->rb_varma = gtk_radio_button_new_with_label( NULL, "VARMA en diferencias (drvarma)" );
    M->rb_vec = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(M->rb_varma), "VEC (drvec)" );
    gtk_widget_set_tooltip_text( M->rb_varma, "Cada serie con su modelo de fue; lo que se añade "
                                 "es la dinámica cruzada" );
    g_signal_connect( M->rb_varma, "toggled", G_CALLBACK(on_cambia_forma), m );
    gtk_box_pack_start( GTK_BOX(fila), M->rb_varma, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(fila), M->rb_vec, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), fila, FALSE, FALSE, 0 );
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
    gtk_widget_show( M->c_diag );
    gtk_widget_set_no_show_all( M->c_diag, TRUE );   /* solo en el VARMA */
    g_signal_connect( M->s_p, "value-changed", G_CALLBACK(on_cambia), m );
    g_signal_connect( M->s_q, "value-changed", G_CALLBACK(on_cambia), m );
    g_signal_connect( M->c_diag, "toggled", G_CALLBACK(on_cambia), m );
    gtk_box_pack_start( GTK_BOX(caja), g, FALSE, FALSE, 0 );

    M->caja_varma = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    gtk_box_pack_start( GTK_BOX(caja), M->caja_varma, FALSE, FALSE, 0 );
    t = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(t), "<b>Enlaces</b>   (A ← B: B entra en la ecuación de A)" );
    gtk_widget_set_halign( t, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX(M->caja_varma), t, FALSE, FALSE, 0 );
    M->enlaces = gtk_box_new( GTK_ORIENTATION_VERTICAL, 2 );
    gtk_box_pack_start( GTK_BOX(M->caja_varma), M->enlaces, FALSE, FALSE, 0 );
    fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
    b = gtk_button_new_with_label( "Todos" );
    gtk_widget_set_tooltip_text( b, "Todos los pares: la orden no lleva -links" );
    g_signal_connect( b, "clicked", G_CALLBACK(on_todos), m );
    gtk_box_pack_start( GTK_BOX(fila), b, FALSE, FALSE, 0 );
    b = gtk_button_new_with_label( "Ninguno" );
    g_signal_connect( b, "clicked", G_CALLBACK(on_ninguno), m );
    gtk_box_pack_start( GTK_BOX(fila), b, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(M->caja_varma), fila, FALSE, FALSE, 0 );

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
    gtk_box_pack_start( GTK_BOX(M->caja_varma), exp, FALSE, FALSE, 0 );
    /* LA VISIBILIDAD DE LAS DOS CAJAS la lleva modelo_refresca: se ensenan
     * enteras aqui y despues el show_all de la ventana no las toca (con
     * no_show_all puesto ANTES, show_all no haria nada).               */
    gtk_widget_show_all( M->caja_varma );
    gtk_widget_set_no_show_all( M->caja_varma, TRUE );

    /* EL VEC: r, el caso y la normalizacion */
    M->caja_vec = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    {
        GtkWidget *f = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 ), *l;
        guint k;
        l = etiqueta( "r", "Cuántas relaciones de cointegración («R · Rango» lo propone)" );
        gtk_box_pack_start( GTK_BOX(f), l, FALSE, FALSE, 0 );
        M->s_r = gtk_spin_button_new_with_range( 1, 15, 1 );
        g_signal_connect( M->s_r, "value-changed", G_CALLBACK(on_cambia_forma), m );
        gtk_box_pack_start( GTK_BOX(f), M->s_r, FALSE, FALSE, 0 );
        l = etiqueta( "Caso", "Lo determinista: sin constante (1), la constante dentro de la "
                              "relación (2), con deriva (3), o el de cada .pre" );
        gtk_box_pack_start( GTK_BOX(f), l, FALSE, FALSE, 0 );
        M->cb_caso = gtk_combo_box_text_new();
        for (k = 0; k < G_N_ELEMENTS(CASOS); k++)
            gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(M->cb_caso), CASOS[k].rotulo );
        gtk_combo_box_set_active( GTK_COMBO_BOX(M->cb_caso), 0 );
        g_signal_connect( M->cb_caso, "changed", G_CALLBACK(on_cambia), m );
        gtk_box_pack_start( GTK_BOX(f), M->cb_caso, FALSE, FALSE, 0 );
        gtk_box_pack_start( GTK_BOX(M->caja_vec), f, FALSE, FALSE, 0 );
        l = gtk_label_new( NULL );
        gtk_label_set_markup( GTK_LABEL(l), "<b>Normalizar β sobre</b>   (el bloque Y1: r series)" );
        gtk_widget_set_halign( l, GTK_ALIGN_START );
        gtk_box_pack_start( GTK_BOX(M->caja_vec), l, FALSE, FALSE, 0 );
        M->y1 = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 10 );
        gtk_box_pack_start( GTK_BOX(M->caja_vec), M->y1, FALSE, FALSE, 0 );
        M->ver_vec = veredicto_new();
        gtk_box_pack_start( GTK_BOX(M->caja_vec), M->ver_vec, FALSE, FALSE, 0 );
    }
    gtk_widget_show_all( M->caja_vec );
    gtk_widget_set_no_show_all( M->caja_vec, TRUE );
    gtk_box_pack_start( GTK_BOX(caja), M->caja_vec, FALSE, FALSE, 0 );

    t = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(t), "<b>La orden</b>   (la que corre «Estimar»)" );
    gtk_widget_set_halign( t, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX(caja), t, FALSE, FALSE, 0 );
    M->orden = sim_mono( "" );
    gtk_label_set_line_wrap( GTK_LABEL(M->orden), TRUE );
    gtk_box_pack_start( GTK_BOX(caja), M->orden, FALSE, FALSE, 0 );

    M->ver_cuenta = veredicto_new();
    M->ver_forma = veredicto_new();
    M->ver_ceros = veredicto_new();
    M->b_sin_ceros = gtk_button_new_with_label( "Quitar los ceros" );
    gtk_widget_set_tooltip_text( M->b_sin_ceros, "Que todos los coeficientes de los enlaces vuelvan "
                                 "a estar libres" );
    gtk_widget_set_halign( M->b_sin_ceros, GTK_ALIGN_START );
    gtk_widget_set_no_show_all( M->b_sin_ceros, TRUE );
    g_signal_connect_swapped( M->b_sin_ceros, "clicked", G_CALLBACK(on_sin_ceros), m );
    gtk_box_pack_start( GTK_BOX(caja), M->ver_ceros, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), M->b_sin_ceros, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), M->ver_cuenta, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), M->ver_forma, FALSE, FALSE, 0 );
    return caja;
}
