/*
 * identifica.c -- la pagina 3: LA IDENTIFICACION (nodo N2 de sima,
 * identify_cross).
 *
 * Lo que los univariantes NO llevan: las correlaciones cruzadas de sus
 * residuos. Son los del sistema diagonal --las innovaciones de cada serie,
 * preblanqueadas por construccion--, asi que una correlacion fuera de la
 * banda es dinamica entre las series. drvarma ya las calcula y las escribe
 * en el .out de la puerta (diagnose.c); aqui se leen (lib/outvarma), no se
 * rehacen.
 *
 * LA REJILLA m x m, como la figura de Jenkins y Alavi: en la fila A,
 * columna B, la CCF de «B entra en la ecuacion de A» para k = 0, 1, ...
 * La k = 0 (gris) es la contemporanea: no es un enlace, apunta a Sigma
 * completa. Pulsar una miniatura abre la CCF entera, por los dos lados,
 * en la ventana de graficos: el mismo EPS que va al papel (lib/ccfplot).
 *
 * LO QUE SE SACA DE LA PAGINA SON LOS ENLACES: que pares tienen dinamica
 * cruzada. La pagina PROPONE (los retardos cortos fuera de la banda) y el
 * analista marca; «Al modelo» lo lleva a la pagina 4. Con un enlace de ida
 * y vuelta hay retroalimentacion, que es lo que trae aqui y no a MT.
 */

#include <math.h>
#include <string.h>

#include "sim.h"
#include "previewhost.h"
#include "preview.h"
#include "ccfplot.h"

/* --- el anfitrion de la ventana de graficos ------------------------------ */

void preview_open_external( PreviewApp *app, const gchar *path )
{
    gchar *uri = g_filename_to_uri( path, NULL, NULL );
    if (uri && app->ventana) {
        gtk_show_uri_on_window( GTK_WINDOW(app->ventana), uri, GDK_CURRENT_TIME, NULL );
    }
    g_free( uri );
}

void preview_show_status( PreviewApp *app, const gchar *format, ... )
{
    va_list ap;
    gchar  *s;
    va_start( ap, format );
    s = g_strdup_vprintf( format, ap );
    va_end( ap );
    sim_estado( app, "%s", s );
    g_free( s );
}

/* --- lo que se lee ------------------------------------------------------- */

static const VarmaOut *ccfs( Sim *m )
{
    return puerta_estado( m ) == 1 && m->pp.v && m->pp.v->nccf > 0 ? m->pp.v : NULL;
}

/* Los retardos «cortos»: los que proponen un enlace. Con datos anuales
 * Jenkins y Alavi miran 1 y 2; con mensuales, hasta el año.            */
static int k_max( Sim *m, const VarmaOut *v )
{
    int f = m->sis.n ? m->sis.s[0].freq : 1, L = v && v->nccf ? v->ccf[0].nlags : 0;
    int k = f > 4 ? f : 4;
    return k < L ? k : L;
}

static void lee( Sim *m )
{
    PagIdent *I = &m->pi;
    const VarmaOut *v = ccfs( m );
    int a, b, k, n = m->sis.n;

    memset( I->marca, 0, sizeof I->marca );
    I->p_prop = 0;
    I->sigma_prop = FALSE;
    I->vale = v != NULL;
    if (!v) return;
    I->k_max = k_max( m, v );
    for (a = 0; a < n; a++)
        for (b = 0; b < n; b++) {
            double banda;
            if (a == b) continue;
            if (fabs( ov_ccf_r( v, a, b, 0, &banda ) ) > banda && banda > 0) I->sigma_prop = TRUE;
            for (k = 1; k <= I->k_max; k++)
                if (fabs( ov_ccf_r( v, a, b, k, &banda ) ) > banda && banda > 0) {
                    I->marca[a][b] = 1;
                    if (k > I->p_prop) I->p_prop = k;
                }
        }
}

/* Los retardos fuera de la banda, como texto: «1, 2». g_free. */
static gchar *retardos( Sim *m, int a, int b )
{
    const VarmaOut *v = ccfs( m );
    GString *g = g_string_new( NULL );
    int k;
    for (k = 1; v && k <= m->pi.k_max; k++) {
        double banda, r = ov_ccf_r( v, a, b, k, &banda );
        if (fabs( r ) > banda && banda > 0) g_string_append_printf( g, "%s%d", g->len ? ", " : "", k );
    }
    return g_string_free( g, FALSE );
}

/* --- la miniatura -------------------------------------------------------- */

typedef struct { Sim *m; int a, b; } Celda;

static gboolean dibuja( GtkWidget *w, cairo_t *cr, Celda *c )
{
    const VarmaOut *v = ccfs( c->m );
    int W = gtk_widget_get_allocated_width( w ), H = gtk_widget_get_allocated_height( w );
    double banda, x0 = 6, x1 = W - 6, y0 = 22, y1 = H - 6, ym = ( y0 + y1 ) / 2, esc = ( y1 - y0 ) / 2 / 0.6;   /* la escala va a ±0,6 */
    int k, K = c->m->pi.k_max;
    gchar *t;

    cairo_set_source_rgb( cr, 1, 1, 1 );
    cairo_paint( cr );
    t = g_strdup_printf( "%s → %s", c->m->sis.s[c->b].nombre, c->m->sis.s[c->a].nombre );
    /* CON PANGO, como el resto de la ventana: la fuente de juguete de cairo
     * no tiene la flecha.                                               */
    {
        PangoLayout *ly = gtk_widget_create_pango_layout( w, t );
        cairo_set_source_rgb( cr, 0.2, 0.2, 0.2 );
        cairo_move_to( cr, 6, 1 );
        pango_cairo_show_layout( cr, ly );
        g_object_unref( ly );
    }
    g_free( t );
    if (!v || K < 1) return FALSE;

    ov_ccf_r( v, c->a, c->b, 0, &banda );
    cairo_set_line_width( cr, 1 );
    cairo_set_source_rgb( cr, 0.6, 0.6, 0.6 );
    cairo_move_to( cr, x0, ym ); cairo_line_to( cr, x1, ym ); cairo_stroke( cr );
    {
        double d[] = { 3, 3 };
        cairo_set_dash( cr, d, 2, 0 );
        cairo_move_to( cr, x0, ym - banda * esc ); cairo_line_to( cr, x1, ym - banda * esc );
        cairo_move_to( cr, x0, ym + banda * esc ); cairo_line_to( cr, x1, ym + banda * esc );
        cairo_stroke( cr );
        cairo_set_dash( cr, NULL, 0, 0 );
    }
    for (k = 0; k <= K; k++) {
        double r = ov_ccf_r( v, c->a, c->b, k, NULL );
        double rr = r > 0.6 ? 0.6 : r < -0.6 ? -0.6 : r;
        double x = x0 + ( k + 0.5 ) * ( x1 - x0 ) / ( K + 1 ), bw = ( x1 - x0 ) / ( K + 1 ) * 0.6;
        if (k == 0) cairo_set_source_rgb( cr, 0.65, 0.65, 0.65 );
        else if (fabs( r ) > banda) cairo_set_source_rgb( cr, 0.70, 0.15, 0.12 );
        else cairo_set_source_rgb( cr, 0.25, 0.35, 0.55 );
        cairo_rectangle( cr, x - bw / 2, rr >= 0 ? ym - rr * esc : ym, bw, fabs( rr ) * esc );
        cairo_fill( cr );
    }
    return FALSE;
}

/* Pulsar: la CCF entera del par, por los dos lados, en la ventana de
 * graficos. Entrada b, salida a: k > 0 es «b entra en a».              */
static gboolean pulsada( GtkWidget *w, GdkEventButton *e, Celda *c )
{
    const VarmaOut *v = ccfs( c->m );
    const OvCcf *x;
    double corr[2 * OV_MAX_LAG + 1], banda;
    gchar *path, *nom;
    int k, L, nobs;

    (void) w; (void) e;
    if (!v || !( x = ov_ccf( v, c->a, c->b ) )) return FALSE;
    L = x->nlags;
    for (k = -L; k <= L; k++)
        corr[k + L] = k >= 0 ? ov_ccf_r( v, c->a, c->b, k, &banda ) : ov_ccf_r( v, c->b, c->a, -k, &banda );
    nobs = banda > 0 ? (int) floor( 4.0 / ( banda * banda ) + 0.5 ) : 0;
    nom = g_strdup_printf( "ccf_%s_%s.eps", c->m->sis.s[c->b].nombre, c->m->sis.s[c->a].nombre );
    path = sim_artefacto( c->m, nom );
    if (ccf_write_eps( path, corr, L, nobs, c->m->sis.s[c->b].nombre, c->m->sis.s[c->a].nombre,
                       c->a == x->i ? x->q_ge0 : x->q_le0, x->df_ge0 ) != 0)
        sim_estado( c->m, "No pude escribir %s", path );
    else if (!preview_show( c->m, path ))
        sim_estado( c->m, "No pude dibujar %s", path );
    g_free( nom ); g_free( path );
    return TRUE;
}

/* --- la pagina ----------------------------------------------------------- */

static void vacia( GtkWidget *caja )
{
    GList *l = gtk_container_get_children( GTK_CONTAINER(caja) ), *p;
    for (p = l; p; p = p->next) gtk_widget_destroy( p->data );
    g_list_free( l );
}

static void on_marca( GtkToggleButton *t, Sim *m )
{
    int a = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(t), "a" ));
    int b = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(t), "b" ));
    m->pi.marca[a][b] = gtk_toggle_button_get_active( t );
}

void identifica_al_modelo( Sim *m )
{
    PagIdent *I = &m->pi;
    int a, b, n = m->sis.n, k = 0;

    if (!I->vale) { sim_estado( m, "Antes, la puerta: «2 · Puerta»." ); return; }
    for (a = 0; a < n; a++)
        for (b = 0; b < n; b++) {
            m->mod.enlace[a][b] = a != b && I->marca[a][b];
            k += m->mod.enlace[a][b];
        }
    if (I->p_prop > 0) m->mod.p = I->p_prop;
    m->mod.diagcov = !I->sigma_prop;
    sim_refresca( m );
    if (m->libro) gtk_notebook_set_current_page( GTK_NOTEBOOK(m->libro), PG_MODELO );
    sim_estado( m, "Al modelo: %d enlace%s, p = %d, Σ %s.", k, k == 1 ? "" : "s", m->mod.p,
                m->mod.diagcov ? "diagonal" : "completa" );
}

static void on_al_modelo( GtkButton *b, Sim *m ) { (void) b; identifica_al_modelo( m ); }

/* Lo que dicen los enlaces marcados: retroalimentacion, una direccion
 * (una red: MT), o nada.                                               */
static void lectura( Sim *m )
{
    PagIdent *I = &m->pi;
    int a, b, n = m->sis.n, ida = 0, vuelta = 0, alguno = 0;

    for (a = 0; a < n; a++)
        for (b = a + 1; b < n; b++) {
            if (I->marca[a][b] || I->marca[b][a]) alguno++;
            if (I->marca[a][b] && I->marca[b][a]) vuelta++;
            else if (I->marca[a][b] || I->marca[b][a]) ida++;
        }
    if (!alguno)
        sim_veredicto( I->ver_lectura, SIM_AMBAR,
                       "Ningún retardo corto sale de la banda: los univariantes llevan ya la "
                       "dinámica de cada serie, y no hay evidencia de dinámica cruzada." );
    else if (vuelta)
        sim_veredicto( I->ver_lectura, SIM_VERDE,
                       "Retroalimentación: %d par%s con enlace de ida y vuelta. Es un sistema "
                       "simultáneo, lo que trae aquí y no a MT.", vuelta, vuelta == 1 ? "" : "es" );
    else
        sim_veredicto( I->ver_lectura, SIM_AMBAR,
                       "Los enlaces van en un solo sentido: puede ser una red de transferencia "
                       "(MT). La estructura se contrasta en la diagnosis («¿Es una red?», S7)." );
    if (I->sigma_prop)
        sim_veredicto( I->ver_sigma, SIM_VERDE,
                       "La correlación contemporánea (k = 0, en gris) sale de la banda: Σ completa." );
    else
        sim_veredicto( I->ver_sigma, SIM_VERDE,
                       "La correlación contemporánea queda dentro de la banda: Σ puede ser diagonal." );
}

void identifica_refresca( Sim *m )
{
    PagIdent *I = &m->pi;
    int a, b, n = m->sis.n;

    if (!I->rejilla) return;
    /* SOLO SE RELEE SI CAMBIO LA PUERTA: rehacer la rejilla en cada refresco
     * borraria lo que el analista desmarco.                             */
    {
        int ver = ccfs( m ) ? m->pp.version : -1;
        if (ver == I->version) return;
        I->version = ver;
    }
    lee( m );
    vacia( I->rejilla );
    vacia( I->enlaces );
    gtk_widget_set_sensitive( I->b_al_modelo, I->vale );

    if (!I->vale) {
        sim_veredicto( I->ver_pide, SIM_AMBAR, "%s",
                       m->sis.n < 2 ? "Pide al menos dos series («1 · Series»)."
                                    : "Pide la puerta superada («2 · Puerta»): las CCF son las de "
                                      "los residuos del sistema diagonal." );
        gtk_label_set_text( GTK_LABEL(I->ver_lectura), "" );
        gtk_label_set_text( GTK_LABEL(I->ver_sigma), "" );
        return;
    }
    sim_veredicto( I->ver_pide, SIM_VERDE,
                   "La CCF de los residuos univariantes, k = 0..%d (en rojo, fuera de la banda "
                   "2/√n; en gris, la contemporánea). Pulsa un par para verla entera.", I->k_max );

    for (a = 0; a < n; a++)
        for (b = 0; b < n; b++) {
            GtkWidget *w;
            if (a == b) {
                gchar *mk = g_markup_printf_escaped( "<b>%s</b>", m->sis.s[a].nombre );
                w = gtk_label_new( NULL );
                gtk_label_set_markup( GTK_LABEL(w), mk );
                g_free( mk );
            } else {
                Celda *c = g_new0( Celda, 1 );
                GtkWidget *da = gtk_drawing_area_new();
                c->m = m; c->a = a; c->b = b;
                gtk_widget_set_size_request( da, 200, 110 );
                g_signal_connect_data( da, "draw", G_CALLBACK(dibuja), c, (GClosureNotify) g_free, 0 );
                w = gtk_event_box_new();
                gtk_container_add( GTK_CONTAINER(w), da );
                g_signal_connect( w, "button-press-event", G_CALLBACK(pulsada), c );
                gtk_widget_set_tooltip_text( w, "Pulsa para ver la CCF entera" );
                g_object_set_data( G_OBJECT(w), "a", GINT_TO_POINTER(a + 1) );
                g_object_set_data( G_OBJECT(w), "b", GINT_TO_POINTER(b + 1) );
            }
            gtk_grid_attach( GTK_GRID(I->rejilla), w, b, a, 1, 1 );
        }

    for (a = 0; a < n; a++)
        for (b = 0; b < n; b++) {
            GtkWidget *c;
            gchar *ks, *t;
            if (a == b || !I->marca[a][b]) continue;
            ks = retardos( m, a, b );
            t = g_strdup_printf( "%s ← %s   (k = %s)", m->sis.s[a].nombre, m->sis.s[b].nombre, ks );
            c = gtk_check_button_new_with_label( t );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(c), TRUE );
            g_object_set_data( G_OBJECT(c), "a", GINT_TO_POINTER(a) );
            g_object_set_data( G_OBJECT(c), "b", GINT_TO_POINTER(b) );
            g_signal_connect( c, "toggled", G_CALLBACK(on_marca), m );
            gtk_widget_set_tooltip_text( c, "Que la segunda serie entre en la ecuación de la primera" );
            gtk_box_pack_start( GTK_BOX(I->enlaces), c, FALSE, FALSE, 0 );
            g_free( ks ); g_free( t );
        }
    lectura( m );
    gtk_widget_show_all( I->rejilla );
    gtk_widget_show_all( I->enlaces );
}

static GtkWidget *veredicto_new( void )
{
    GtkWidget *l = gtk_label_new( "" );
    gtk_label_set_xalign( GTK_LABEL(l), 0 );
    gtk_label_set_line_wrap( GTK_LABEL(l), TRUE );
    return l;
}

GtkWidget *identifica_pagina_new( Sim *m )
{
    PagIdent *I = &m->pi;
    GtkWidget *caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    GtkWidget *fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 18 );
    GtkWidget *der = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    GtkWidget *t = gtk_label_new( NULL );

    gtk_container_set_border_width( GTK_CONTAINER(caja), 12 );
    I->version = -2;
    I->ver_pide = veredicto_new();
    gtk_box_pack_start( GTK_BOX(caja), I->ver_pide, FALSE, FALSE, 0 );

    I->rejilla = gtk_grid_new();
    gtk_grid_set_row_spacing( GTK_GRID(I->rejilla), 6 );
    gtk_grid_set_column_spacing( GTK_GRID(I->rejilla), 6 );
    gtk_box_pack_start( GTK_BOX(fila), I->rejilla, FALSE, FALSE, 0 );

    gtk_label_set_markup( GTK_LABEL(t), "<b>Enlaces propuestos</b>" );
    gtk_widget_set_halign( t, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX(der), t, FALSE, FALSE, 0 );
    I->enlaces = gtk_box_new( GTK_ORIENTATION_VERTICAL, 2 );
    gtk_box_pack_start( GTK_BOX(der), I->enlaces, FALSE, FALSE, 0 );
    I->b_al_modelo = gtk_button_new_with_label( "Al modelo" );
    gtk_widget_set_tooltip_text( I->b_al_modelo, "Los enlaces marcados, p y Σ, a «4 · Modelo»" );
    gtk_widget_set_halign( I->b_al_modelo, GTK_ALIGN_START );
    g_signal_connect( I->b_al_modelo, "clicked", G_CALLBACK(on_al_modelo), m );
    gtk_box_pack_start( GTK_BOX(der), I->b_al_modelo, FALSE, FALSE, 6 );
    gtk_box_pack_start( GTK_BOX(fila), der, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), fila, FALSE, FALSE, 0 );

    I->ver_lectura = veredicto_new();
    I->ver_sigma = veredicto_new();
    gtk_box_pack_start( GTK_BOX(caja), I->ver_lectura, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(caja), I->ver_sigma, FALSE, FALSE, 0 );
    return caja;
}
