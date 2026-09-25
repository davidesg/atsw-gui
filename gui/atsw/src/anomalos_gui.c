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

    /* QUE EPISODIOS SE CALIBRAN. Uno, dos, tres o todos -- la pregunta de
       verdad no es «¿y si no hubiera anómalos?» sino «¿y si no estuviera
       ESTE?», que es lo que se acaba interviniendo.                    */
    gboolean   marcado[AN_MAX_EP];
    int        z_extremo[FO_MAX_RES];   /* 1 si |z| >= umbral            */
    double     z[FO_MAX_RES];           /* los residuos tipificados      */
    gboolean   calibrado;               /* ya se pulso «Calibrar»        */

    gboolean   sin_anomalos;      /* el interruptor                       */
    GtkWidget *dib_res, *dib_acf, *dib_pacf, *l_q, *l_pie, *b_ver, *lista;
} An;

enum { EP_MARCA, EP_DESDE, EP_HASTA, EP_N, EP_Z, EP_I, EP_COLS };


/* ------------------------------------------------------------------------ */
/* Los residuos, con sus anómalos                                            */
/*                                                                           */
/* ES EL MISMO GRAFICO DE SIEMPRE -- la serie tipificada con sus bandas --    */
/* y encima, sombreados, los episodios MARCADOS. Así se ve sobre qué se va a  */
/* calibrar antes de calibrar, que es la mitad de la pregunta.               */
/* ------------------------------------------------------------------------ */

static gboolean pinta_res( GtkWidget *w, cairo_t *cr, An *g )
{
    GtkAllocation al;
    double        medio, alto, escala, dx;
    int           i, n = g->o.nres;

    gtk_widget_get_allocation( w, &al );
    if ( n < 2 ) return FALSE;

    medio = al.height / 2.0;
    alto  = medio - 6.0;
    dx    = (double) al.width / n;

    escala = g->umbral;
    for ( i = 0; i < n; i++ )
        if ( fabs( g->z[i] ) > escala ) escala = fabs( g->z[i] );
    escala = alto / ( escala * 1.05 );

    /* Los episodios MARCADOS, sombreados por detrás. */
    for ( i = 0; i < g->nep; i++ )
        {
        double x0, x1;

        if ( !g->marcado[i] ) continue;
        x0 = g->ep[i].desde * dx;
        x1 = ( g->ep[i].hasta + 1 ) * dx;
        if ( x1 - x0 < 3.0 ) x1 = x0 + 3.0;
        cairo_set_source_rgba( cr, 0.85, 0.55, 0.10, 0.22 );
        cairo_rectangle( cr, x0, 0, x1 - x0, al.height );
        cairo_fill( cr );
        }

    /* El eje y las bandas del umbral. */
    cairo_set_source_rgb( cr, 0.45, 0.45, 0.45 );
    cairo_set_line_width( cr, 1.0 );
    cairo_move_to( cr, 0, medio ); cairo_line_to( cr, al.width, medio );
    cairo_stroke( cr );

    {
    static const double guion[] = { 2.0, 3.0 };

    cairo_set_source_rgba( cr, 0.20, 0.40, 0.75, 0.6 );
    cairo_set_dash( cr, guion, 2, 0 );
    cairo_move_to( cr, 0, medio - g->umbral * escala );
    cairo_line_to( cr, al.width, medio - g->umbral * escala );
    cairo_move_to( cr, 0, medio + g->umbral * escala );
    cairo_line_to( cr, al.width, medio + g->umbral * escala );
    cairo_stroke( cr );
    cairo_set_dash( cr, NULL, 0, 0 );
    }

    /* La serie. Los extremos, en rojo y más gruesos. */
    for ( i = 0; i < n; i++ )
        {
        double x = ( i + 0.5 ) * dx;

        if ( g->z_extremo[i] )
            { cairo_set_source_rgb( cr, .71, .11, .09 );
              cairo_set_line_width( cr, 2.5 ); }
        else
            { cairo_set_source_rgb( cr, .30, .30, .34 );
              cairo_set_line_width( cr, 1.0 ); }
        cairo_move_to( cr, x, medio );
        cairo_line_to( cr, x, medio - g->z[i] * escala );
        cairo_stroke( cr );
        }
    return FALSE;
}


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

/* UNA MARCA. No recalcula: marcar es decir QUE se quiere probar, y probarlo
 * es pulsar el botón. Separar las dos cosas deja marcar tres episodios y ver
 * el efecto DE LOS TRES JUNTOS, que es una pregunta distinta de la de cada
 * uno por separado -- y es la que no se podía hacer.                    */
static void on_marca( GtkCellRendererToggle *r, gchar *ruta, An *g )
{
    GtkTreeModel *mo = gtk_tree_view_get_model( GTK_TREE_VIEW(g->lista) );
    GtkTreeIter   it;
    gboolean      v;
    int           i;

    (void) r;
    if ( !gtk_tree_model_get_iter_from_string( mo, &it, ruta ) ) return;
    gtk_tree_model_get( mo, &it, EP_MARCA, &v, EP_I, &i, -1 );
    gtk_list_store_set( GTK_LIST_STORE(mo), &it, EP_MARCA, !v, -1 );
    if ( i >= 0 && i < g->nep ) g->marcado[i] = !v;

    gtk_widget_queue_draw( g->dib_res );
}

static void pie( An *g )
{
    gtk_label_set_markup( GTK_LABEL(g->l_pie),
        !g->calibrado
          ? "<small>Marca los episodios que quieras probar y pulsa "
            "<b>Calibrar</b>. Puedes probarlos de uno en uno o varios a la "
            "vez: no es la misma pregunta.</small>"
        : g->c.cambia
          ? "<small>Hay retardos que <b>cambian de lado</b> de la banda: "
            "quitar esto cambiaría la identificación.</small>"
          : "<small>Ningún retardo cambia de lado: intervenir esto <b>no "
            "compra nada</b> para la identificación.</small>" );
}

static void on_calibrar( GtkButton *b, An *g )
{
    int i, t, lags;

    (void) b;
    g->nomit = 0;
    for ( i = 0; i < g->nep; i++ )
        {
        if ( !g->marcado[i] ) continue;
        for ( t = g->ep[i].desde; t <= g->ep[i].hasta; t++ )
            if ( g->nomit < FO_MAX_RES ) g->omit[g->nomit++] = t;
        }

    lags = g->o.nres / 8;
    if ( lags > 24 ) lags = 24;
    an_calibra( g->o.res, g->o.nres, g->omit, g->nomit, lags, &g->c );
    g->calibrado = TRUE;

    /* Al calibrar se ENSEÑA el resultado: es lo que se acaba de pedir. El
       interruptor queda para volver al original y comparar.          */
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g->b_ver), TRUE );
    gtk_widget_set_sensitive( g->b_ver, TRUE );

    di_q( g );
    pie( g );
    gtk_widget_queue_draw( g->dib_acf );
    gtk_widget_queue_draw( g->dib_pacf );

    if ( g->nomit == 0 )
        barra_pub( g->a, "Sin ningún episodio marcado, la calibración es el "
                         "correlograma de siempre." );
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
    double sd = 0.0;

    for ( i = 0; i < g->o.nres; i++ ) sd += g->o.res[i] * g->o.res[i];
    sd = sqrt( sd / g->o.nres );
    for ( i = 0; i < g->o.nres; i++ )
        {
        g->z[i] = ( sd > 0.0 ) ? g->o.res[i] / sd : 0.0;
        g->z_extremo[i] = ( fabs( g->z[i] ) >= g->umbral );
        }

    g->nep = an_episodios( g->z, g->o.nres, g->umbral, AN_VENTANA,
                           g->ep, AN_MAX_EP );
    }

    /* SIN NADA MARCADO al abrir: el correlograma que se ve es el de siempre,
       y calibrar es un acto, no un estado de partida.                 */
    {
    int lags = g->o.nres / 8;

    if ( lags > 24 ) lags = 24;
    an_calibra( g->o.res, g->o.nres, NULL, 0, lags, &g->c );
    }

    win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_transient_for( GTK_WINDOW(win), GTK_WINDOW(a->ventana) );
    gtk_window_set_default_size( GTK_WINDOW(win), 760, 700 );
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
    gchar *t = g_markup_printf_escaped(
        "<b>%s / %s</b>%s%s   ·   %d episodio%s con |z| ≥ %.2f, sobre %d "
        "residuos", serie, id,
        ( muestra && *muestra ) ? "   muestra " : "",
        ( muestra && *muestra ) ? muestra : "",
        g->nep, g->nep == 1 ? "" : "s", g->umbral, g->o.nres );

    gtk_label_set_markup( GTK_LABEL(cab), t );
    g_free( t );
    }
    gtk_label_set_xalign( GTK_LABEL(cab), 0.0 );
    gtk_box_pack_start( GTK_BOX(raiz), cab, FALSE, FALSE, 0 );

    /* EL GRAFICO DE RESIDUOS, el de siempre, con los episodios marcados
       sombreados encima: se ve sobre qué se va a calibrar.            */
    g->dib_res = gtk_drawing_area_new();
    gtk_widget_set_size_request( g->dib_res, -1, 110 );
    gtk_widget_set_tooltip_text( g->dib_res,
        "Los residuos tipificados con su umbral. En rojo los extremos; "
        "sombreados, los episodios marcados abajo." );
    g_signal_connect( g->dib_res, "draw", G_CALLBACK(pinta_res), g );
    gtk_box_pack_start( GTK_BOX(raiz), g->dib_res, FALSE, FALSE, 0 );

    /* LA LISTA DE EPISODIOS, CON SUS CASILLAS. Uno, dos o todos: marcar es
       decir QUÉ se quiere probar; probarlo es pulsar el botón.        */
    {
    GtkListStore *st = gtk_list_store_new( EP_COLS, G_TYPE_BOOLEAN,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT );
    GtkCellRenderer *r;
    GtkTreeIter      it;
    GtkWidget       *sc;

    g->lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );
    g_object_unref( st );

    r = gtk_cell_renderer_toggle_new();
    g_signal_connect( r, "toggled", G_CALLBACK(on_marca), g );
    gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(g->lista), -1,
        "", r, "active", EP_MARCA, NULL );

    r = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(g->lista), -1,
        "Desde", r, "text", EP_DESDE, NULL );
    gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(g->lista), -1,
        "Hasta", r, "text", EP_HASTA, NULL );
    gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(g->lista), -1,
        "Períodos", r, "text", EP_N, NULL );
    gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(g->lista), -1,
        "z máx", r, "text", EP_Z, NULL );

    for ( i = 0; i < g->nep; i++ )
        {
        char zb[16];

        g_snprintf( zb, sizeof zb, "%+.2f", g->ep[i].z_max );
        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it,
            EP_MARCA, FALSE,
            EP_DESDE, g->o.res_fecha[g->ep[i].desde],
            EP_HASTA, g->ep[i].n > 1 ? g->o.res_fecha[g->ep[i].hasta] : "",
            EP_N,     g->ep[i].n,
            EP_Z,     zb,
            EP_I,     i, -1 );
        }

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
    gtk_widget_set_size_request( sc, -1, 96 );
    gtk_container_add( GTK_CONTAINER(sc), g->lista );
    gtk_box_pack_start( GTK_BOX(raiz), sc, FALSE, FALSE, 0 );
    }

    barra = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
    gtk_box_pack_start( GTK_BOX(raiz), barra, FALSE, FALSE, 0 );

    b = gtk_button_new_with_label( "Calibrar" );
    gtk_widget_set_tooltip_text( b,
        "Recalcula la ACF y la PACF omitiendo los episodios marcados, y "
        "dice qué retardos cambian de lado de la banda.\n\nMarcar varios y "
        "calibrar de una vez NO es lo mismo que calibrarlos uno a uno: son "
        "dos preguntas distintas." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_calibrar), g );
    gtk_box_pack_start( GTK_BOX(barra), b, FALSE, FALSE, 0 );

    g->b_ver = gtk_check_button_new_with_label( "ver calibrado" );
    gtk_widget_set_tooltip_text( g->b_ver,
        "Quítalo para volver al correlograma original y comparar: es la "
        "MISMA figura moviéndose." );
    gtk_widget_set_sensitive( g->b_ver, FALSE );
    g_signal_connect( g->b_ver, "toggled", G_CALLBACK(on_interruptor), g );
    gtk_box_pack_start( GTK_BOX(barra), g->b_ver, FALSE, FALSE, 0 );

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

    g->l_pie = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(g->l_pie), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(g->l_pie), TRUE );
    gtk_box_pack_start( GTK_BOX(raiz), g->l_pie, FALSE, FALSE, 0 );

    di_q( g );
    pie( g );
    g_signal_connect( win, "destroy", G_CALLBACK(on_cerrar), g );
    gtk_widget_show_all( win );
}
