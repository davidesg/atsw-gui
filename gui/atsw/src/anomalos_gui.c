/*
 * anomalos_gui.c -- la ventana de anómalos: los correlogramas CAMBIAN.
 *
 * EL INTERRUPTOR REDIBUJA, NO AÑADE.
 *
 * La primera idea fue dibujar las dos barras por retardo, una al lado de
 * otra. No: lo que hay que ver es la MISMA figura moviéndose, porque lo que
 * se compara no son dos números sino dos lecturas del correlograma -- «¿corto
 * el AR en el 2?» se responde mirando UN dibujo, y con dos barras por retardo
 * la lectura deja de ser la de siempre.
 *
 * Y tampoco un degradé con la parte que aporta el anómalo: r(k) con y sin son
 * DOS COCIENTES DISTINTOS --cambia el denominador y cambia n-- así que
 * r_con = r_sin + aportación NO se cumple. Una barra apilada afirmaría una
 * descomposición que no existe.
 *
 * Lo que sí se dibuja son LAS DOS BANDAS cuando difieren: quitar
 * observaciones ENSANCHA la banda, y comparar contra una sola haría parecer
 * que algo sale de banda cuando lo que pasó fue que la banda se movió.
 */

#include <string.h>
#include <math.h>

#include "anomalos.h"
#include "dictamen.h"

#include "atsw.h"

void barra_pub( Atsw *a, const char *s );

typedef struct {
    Atsw      *a;
    char       serie[PR_ID], muestra[PR_ID], id[PR_ID];

    FueOut     o;
    AnCalibra  c;
    AnEpisodio ep[AN_MAX_EP];
    int        nep;
    int        omit[FO_MAX_RES];
    int        nomit;
    double     umbral;

    gboolean   sin_anomalos;      /* el interruptor                       */
    GtkWidget *dib_acf, *dib_pacf, *l_q, *l_res;
} An;


/* ------------------------------------------------------------------------ */
/* El correlograma                                                           */
/* ------------------------------------------------------------------------ */

static gboolean pinta( GtkWidget *w, cairo_t *cr, An *g )
{
    GtkAllocation al;
    const gboolean es_pacf = ( w == g->dib_pacf );
    double  ancho, medio, alto, escala, banda, otra;
    int     k, n = g->c.nlags;

    gtk_widget_get_allocation( w, &al );
    if ( n < 1 ) return FALSE;

    medio = al.height / 2.0;
    alto  = medio - 14.0;
    ancho = (double) al.width / ( n + 1 );

    /* La escala la fija el mayor de LOS DOS estados: si cambiara con el
       interruptor, las barras se moverían por el dibujo y no por los
       datos, que es justo lo que no puede pasar aquí.                 */
    escala = 0.0;
    for ( k = 0; k < n; k++ )
        {
        double v[4];
        int    j;

        v[0] = fabs( g->c.l[k].acf_con );  v[1] = fabs( g->c.l[k].acf_sin );
        v[2] = fabs( g->c.l[k].pacf_con ); v[3] = fabs( g->c.l[k].pacf_sin );
        for ( j = es_pacf ? 2 : 0; j < ( es_pacf ? 4 : 2 ); j++ )
            if ( v[j] > escala ) escala = v[j];
        }
    if ( escala < g->c.banda_sin ) escala = g->c.banda_sin;
    if ( escala <= 0.0 ) return FALSE;
    escala = alto / ( escala * 1.15 );

    /* el eje */
    cairo_set_source_rgb( cr, 0.45, 0.45, 0.45 );
    cairo_set_line_width( cr, 1.0 );
    cairo_move_to( cr, 0, medio );
    cairo_line_to( cr, al.width, medio );
    cairo_stroke( cr );

    /* LAS DOS BANDAS: la de ahora entera, y la del otro estado punteada si
       no coinciden. Que la banda se mueva es parte de la noticia.      */
    banda = g->sin_anomalos ? g->c.banda_sin : g->c.banda_con;
    otra  = g->sin_anomalos ? g->c.banda_con : g->c.banda_sin;

    if ( fabs( otra - banda ) > 1e-9 )
        {
        static const double guion[] = { 2.0, 3.0 };

        cairo_set_source_rgba( cr, 0.55, 0.55, 0.55, 0.8 );
        cairo_set_dash( cr, guion, 2, 0 );
        cairo_move_to( cr, 0, medio - otra * escala );
        cairo_line_to( cr, al.width, medio - otra * escala );
        cairo_move_to( cr, 0, medio + otra * escala );
        cairo_line_to( cr, al.width, medio + otra * escala );
        cairo_stroke( cr );
        cairo_set_dash( cr, NULL, 0, 0 );
        }

    cairo_set_source_rgba( cr, 0.20, 0.40, 0.75, 0.75 );
    cairo_move_to( cr, 0, medio - banda * escala );
    cairo_line_to( cr, al.width, medio - banda * escala );
    cairo_move_to( cr, 0, medio + banda * escala );
    cairo_line_to( cr, al.width, medio + banda * escala );
    cairo_stroke( cr );

    /* las barras */
    for ( k = 0; k < n; k++ )
        {
        const AnLag *l = &g->c.l[k];
        double       v = es_pacf
                       ? ( g->sin_anomalos ? l->pacf_sin : l->pacf_con )
                       : ( g->sin_anomalos ? l->acf_sin  : l->acf_con );
        AnVeredicto  ver = es_pacf ? l->pacf : l->acf;
        double       x = ( k + 1 ) * ancho;

        /* EL COLOR ES DEL VEREDICTO, no del estado: dice si ESTE retardo
           cambia de lado al quitar el anómalo, que es lo que decide.   */
        if ( ver == AN_FABRICADA )        cairo_set_source_rgb( cr, .71,.11,.09 );
        else if ( ver == AN_ENMASCARADA ) cairo_set_source_rgb( cr, .10,.50,.11 );
        else                              cairo_set_source_rgb( cr, .25,.25,.28 );

        cairo_set_line_width( cr, 3.0 );
        cairo_move_to( cr, x, medio );
        cairo_line_to( cr, x, medio - v * escala );
        cairo_stroke( cr );

        if ( ( k + 1 ) % 6 == 0 )
            {
            char b[8];

            cairo_set_source_rgb( cr, 0.40, 0.40, 0.40 );
            cairo_set_font_size( cr, 9.0 );
            snprintf( b, sizeof b, "%d", k + 1 );
            cairo_move_to( cr, x - 4, al.height - 2 );
            cairo_show_text( cr, b );
            }
        }
    return FALSE;
}


/* ------------------------------------------------------------------------ */
/* El interruptor                                                            */
/* ------------------------------------------------------------------------ */

static void di_q( An *g )
{
    double qc = 0.0, qs = 0.0;
    int    m = g->c.nlags;
    gchar *t;

    an_q( &g->c, m, &qc, &qs );

    /* EL Q, CON Y SIN, CON LA MISMA FORMULA Y CADA UNO CON SU n. No se
       compara contra el que imprime el motor: ése sale de su propio
       estimador, y mezclarlos sería restar peras de manzanas.        */
    t = g_markup_printf_escaped(
        "<tt>Q(%d)</tt>  con los anómalos <b>%.1f</b> (p = %.3f)   ·   "
        "sin ellos <b>%.1f</b> (p = %.3f)",
        m, qc, chisq_cola( qc, m ), qs, chisq_cola( qs, m ) );
    gtk_label_set_markup( GTK_LABEL(g->l_q), t );
    g_free( t );
}

static void on_interruptor( GtkToggleButton *b, An *g )
{
    g->sin_anomalos = gtk_toggle_button_get_active( b );
    gtk_widget_queue_draw( g->dib_acf );
    gtk_widget_queue_draw( g->dib_pacf );
}

static void on_cerrar( GtkWidget *w, An *g ) { (void) w; g_free( g ); }


/* ------------------------------------------------------------------------ */

void atsw_anomalos( Atsw *a, const char *serie, const char *muestra,
                    const char *id )
{
    An        *g;
    GtkWidget *win, *raiz, *cab, *barra, *b, *caja;
    char       out[PR_RUTA];
    int        i;

    if ( !a->hay || !serie || !*serie || !id || !*id ) return;
    if ( pr_ruta( a->p, serie, muestra, id, ".out", out, sizeof out ) != 0 )
        { barra_pub( a, "No pude componer la ruta." ); return; }

    g = g_new0( An, 1 );
    g->a = a;
    g_snprintf( g->serie, PR_ID, "%s", serie );
    g_snprintf( g->muestra, PR_ID, "%s", muestra ? muestra : "" );
    g_snprintf( g->id, PR_ID, "%s", id );

    if ( !fueout_read( out, &g->o ) || g->o.nres < 8 )
        {
        gchar *s = g_strdup_printf( "«%s» no está estimado, o su informe no "
                                    "trae los residuos.", id );

        barra_pub( a, s ); g_free( s ); g_free( g );
        return;
        }

    /* LOS EXTREMOS, EN EPISODIOS. Un suceso de tres períodos no son tres
       atípicos sueltos con su forma decidida por adyacencia.          */
    g->umbral = an_umbral( g->o.nres );
    {
    double *z = g_new( double, g->o.nres );
    double  sd = 0.0;

    for ( i = 0; i < g->o.nres; i++ ) sd += g->o.res[i] * g->o.res[i];
    sd = sqrt( sd / g->o.nres );
    for ( i = 0; i < g->o.nres; i++ )
        z[i] = ( sd > 0.0 ) ? g->o.res[i] / sd : 0.0;

    g->nep = an_episodios( z, g->o.nres, g->umbral, AN_VENTANA,
                           g->ep, AN_MAX_EP );
    for ( i = 0; i < g->nep; i++ )
        {
        int t;

        for ( t = g->ep[i].desde; t <= g->ep[i].hasta; t++ )
            if ( g->nomit < FO_MAX_RES ) g->omit[g->nomit++] = t;
        }
    an_calibra( g->o.res, g->o.nres, g->omit, g->nomit,
                g->o.nres / 8 > 24 ? 24 : g->o.nres / 8, &g->c );
    g_free( z );
    }

    win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_transient_for( GTK_WINDOW(win), GTK_WINDOW(a->ventana) );
    gtk_window_set_default_size( GTK_WINDOW(win), 720, 520 );
    {
    gchar *t = g_strdup_printf( "Anómalos — %s / %s", serie, id );

    gtk_window_set_title( GTK_WINDOW(win), t );
    g_free( t );
    }

    raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
    gtk_container_set_border_width( GTK_CONTAINER(raiz), 10 );
    gtk_container_add( GTK_CONTAINER(win), raiz );

    cab = gtk_label_new( NULL );
    {
    GString *s = g_string_new( NULL );

    g_string_append_printf( s, "<b>%d episodio%s</b> con |z| ≥ %.2f "
        "(%d observaciones de %d)", g->nep, g->nep == 1 ? "" : "s",
        g->umbral, g->nomit, g->o.nres );
    for ( i = 0; i < g->nep && i < 6; i++ )
        {
        const char *d = g->ep[i].desde < g->o.nres
                      ? g->o.res_fecha[g->ep[i].desde] : "";
        const char *h = g->ep[i].hasta < g->o.nres
                      ? g->o.res_fecha[g->ep[i].hasta] : "";

        g_string_append_printf( s, "\n<tt>  %-8s %s %-8s</tt>  %d período%s, "
            "z máx %.2f", d, g->ep[i].n > 1 ? "–" : " ",
            g->ep[i].n > 1 ? h : "", g->ep[i].n,
            g->ep[i].n == 1 ? "" : "s", g->ep[i].z_max );
        }
    gtk_label_set_markup( GTK_LABEL(cab), s->str );
    g_string_free( s, TRUE );
    }
    gtk_label_set_xalign( GTK_LABEL(cab), 0.0 );
    gtk_box_pack_start( GTK_BOX(raiz), cab, FALSE, FALSE, 0 );

    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
    gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );

    b = gtk_check_button_new_with_label( "sin los anómalos" );
    gtk_widget_set_tooltip_text( b,
        "Redibuja los dos correlogramas omitiendo los episodios. Lo que hay "
        "que ver es la MISMA figura moviéndose: «¿corto el AR en el 2?» se "
        "responde mirando un dibujo.\n\nEn rojo, lo que el anómalo FABRICA; "
        "en verde, lo que ENMASCARA." );
    g_signal_connect( b, "toggled", G_CALLBACK(on_interruptor), g );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    g->l_q = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(g->l_q), 0.0 );
    gtk_box_pack_start( GTK_BOX(barra), g->l_q, TRUE, TRUE, 8 );

    caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_box_pack_start( GTK_BOX(raiz), caja, TRUE, TRUE, 0 );

    for ( i = 0; i < 2; i++ )
        {
        GtkWidget *l = gtk_label_new( NULL );
        GtkWidget *d = gtk_drawing_area_new();

        gtk_label_set_markup( GTK_LABEL(l),
            i ? "<small><b>PACF</b> — decide el orden AR</small>"
              : "<small><b>ACF</b> — decide el orden MA</small>" );
        gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
        gtk_box_pack_start( GTK_BOX(caja), l, FALSE, FALSE, 0 );

        gtk_widget_set_size_request( d, -1, 150 );
        if ( i ) g->dib_pacf = d; else g->dib_acf = d;
        g_signal_connect( d, "draw", G_CALLBACK(pinta), g );
        gtk_box_pack_start( GTK_BOX(caja), d, TRUE, TRUE, 0 );
        }

    {
    GtkWidget *pie = gtk_label_new( NULL );

    gtk_label_set_markup( GTK_LABEL(pie), g->c.cambia
        ? "<small>Hay retardos que cambian de lado de la banda: quitar el "
          "anómalo cambiaría la identificación.</small>"
        : "<small>Ningún retardo cambia de lado: intervenir esto <b>no compra "
          "nada</b> para la identificación.</small>" );
    gtk_label_set_xalign( GTK_LABEL(pie), 0.0 );
    gtk_box_pack_start( GTK_BOX(raiz), pie, FALSE, FALSE, 0 );
    }

    di_q( g );
    g_signal_connect( win, "destroy", G_CALLBACK(on_cerrar), g );
    gtk_widget_show_all( win );
}
