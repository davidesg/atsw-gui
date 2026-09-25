/*
 * anomalos_gui.c -- calibrar anómalos SOBRE EL GRÁFICO DE FUE.
 *
 * EL GRÁFICO NO SE REINVENTA.
 *
 * La primera versión dibujaba con cairo un gráfico de residuos y dos
 * correlogramas «parecidos» a los de fue. Parecidos no sirve: el analista
 * compararía dos dibujos distintos creyendo que compara dos calibraciones, y
 * las diferencias de lienzo, de escala y de bandas se leerían como
 * diferencias de los datos. Así que se dibuja EL DE FUE -- fp_PlotSer_CorrSer,
 * el mismo lienzo, las mismas escalas, el mismo Q -- sobre los residuos que
 * trae el .out.
 *
 * EL INTERRUPTOR REDIBUJA, NO AÑADE. Lo que hay que ver es la MISMA figura
 * moviéndose: «¿corto el AR en el 2?» se responde mirando UN dibujo. Dos
 * barras por retardo, una al lado de otra, cambiarían la lectura de siempre;
 * y un degradé con «lo que aporta el anómalo» afirmaría una descomposición
 * que no existe, porque r_con y r_sin son dos cocientes con distinto
 * denominador.
 *
 * SIN ANÓMALOS ES LA MEDIA DE LOS RETENIDOS, y por eso la figura de fue vale
 * tal cual. El estimador declarado de lib/anomalos es la desviación a cero:
 * mu sobre las retenidas, z~ = z - mu en las retenidas y 0 en las omitidas.
 * Poner mu en las omitidas da exactamente esa serie -- su media ES mu -- así
 * que basta pasarle a fue la serie modificada y su Acf devuelve nuestra r(k),
 * sin tener que pasarle correlaciones ya calculadas.
 *
 * Y EL CÍRCULO. Marcar un episodio en una lista y verlo sombreado no dice
 * cuál es: el círculo va sobre el punto, en el dibujo, donde está la fecha.
 * Lo pinta fugplot (fp_PlotSer_CorrSer_marks); sin marcas, el dibujo es el de
 * fue sin tocar un punto.
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "anomalos.h"
#include "dictamen.h"
#include "outfile.h"

#include "atsw.h"
#include "preview.h"
#include "fugplot.h"

void barra_pub( Atsw *a, const char *s );
gchar *atsw_cache_dir( void );

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
    double     z[FO_MAX_RES];      /* los residuos tipificados             */

    int        freq, tmornsop, tsby;   /* el eje de tiempo de la figura    */
    int        nparma, lags;

    /* QUE EPISODIOS SE CALIBRAN. Uno, dos, tres o todos -- la pregunta de
       verdad no es «¿y si no hubiera anómalos?» sino «¿y si no estuviera
       ESTE?», que es lo que se acaba interviniendo.                    */
    gboolean   marcado[AN_MAX_EP];
    gboolean   calibrado;          /* ya se pulsó «Calibrar»               */
    gboolean   sin_anomalos;       /* el interruptor                       */

    char       eps[PR_RUTA];
    GtkWidget *b_ver, *l_q, *l_pie;
} An;


/* ------------------------------------------------------------------------ */
/* El eje de tiempo, de las fechas de los residuos                           */
/* ------------------------------------------------------------------------ */

/* "1982/03" -> 1982, 3.  "1766" -> 1766, 1 (los anuales no llevan barra). */
static void parte_fecha( const char *f, int *anno, int *per )
{
    const char *b = strchr( f, '/' );

    *anno = atoi( f );
    *per  = b ? atoi( b + 1 ) : 1;
    if ( *per < 1 ) *per = 1;
}

/* LA FRECUENCIA, DE LAS FECHAS ANTES QUE DEL MODELO. El s del .out es el
   período estacional del MODELO, y un mensual sin parte estacional puede
   traerlo a 1; las fechas, en cambio, son las de la serie.              */
static int frecuencia( const An *g )
{
    int i, maxp = 1, anno, per;

    if ( g->o.nres < 1 ) return 1;
    if ( strchr( g->o.res_fecha[0], '/' ) == NULL ) return 1;
    for ( i = 0; i < g->o.nres; i++ )
        {
        parte_fecha( g->o.res_fecha[i], &anno, &per );
        if ( per > maxp ) maxp = per;
        }
    if ( g->o.s > 1 ) return g->o.s;
    if ( maxp > 4 ) return 12;
    if ( maxp > 1 ) return 4;
    return 12;                     /* con barra y un solo período: mensual */
}


/* ------------------------------------------------------------------------ */
/* La figura                                                                 */
/* ------------------------------------------------------------------------ */

/* LA ESCALA DEL CORRELOGRAMA, FIJA EN LOS DOS ESTADOS.
 *
 * fugplot la elige sola (0,4 / 0,6 / 0,8 / 1) con el máximo de la función que
 * va a dibujar. Si la dejáramos elegir en cada estado, al accionar el
 * interruptor las barras se moverían por el dibujo y no por los datos, que es
 * justo lo que aquí no puede pasar. Así que se calcula con el máximo de LOS
 * DOS y se le pasa por cbands -- que es la opción con la que el motor admite
 * una escala dada.                                                        */
static double escala_fija( const An *g )
{
    double cmax = 0.40;
    int    k;

    for ( k = 0; k < g->c.nlags; k++ )
        {
        const AnLag *l = &g->c.l[k];
        double       v[4];
        int          j;

        v[0] = fabs( l->acf_con );  v[1] = fabs( l->acf_sin );
        v[2] = fabs( l->pacf_con ); v[3] = fabs( l->pacf_sin );
        for ( j = 0; j < 4; j++ ) if ( v[j] >= cmax ) cmax = v[j];
        }
    if ( cmax > 0.80 ) return 1.0;
    if ( cmax > 0.60 ) return 0.80;
    if ( cmax > 0.40 ) return 0.60;
    return cmax;
}

static void dibuja( An *g )
{
    struct Tseries ser;
    double        *d, mu = 0.0, var = 0.0;
    int            marks[FO_MAX_RES], nmarks = 0;
    int            i, n = g->o.nres, ret = 0;
    gchar         *base, *nombre;

    if ( n < 8 ) return;

    d = vector( 1, n );
    for ( i = 0; i < n; i++ ) d[i+1] = g->o.res[i];

    /* SIN LOS MARCADOS: la media de los retenidos en el hueco. Ver la
       cabecera -- esto ES la desviación a cero de lib/anomalos.       */
    if ( g->sin_anomalos && g->nomit > 0 )
        {
        char fuera[FO_MAX_RES];

        memset( fuera, 0, sizeof fuera );
        for ( i = 0; i < g->nomit; i++ )
            if ( g->omit[i] >= 0 && g->omit[i] < n ) fuera[g->omit[i]] = 1;
        for ( i = 0; i < n; i++ ) if ( !fuera[i] ) { mu += g->o.res[i]; ret++; }
        mu = ret ? mu / ret : 0.0;
        for ( i = 0; i < n; i++ ) if ( fuera[i] ) d[i+1] = mu;
        }
    else
        {
        for ( i = 0; i < n; i++ ) mu += g->o.res[i];
        mu /= n;
        }

    for ( i = 1; i <= n; i++ ) var += ( d[i] - mu ) * ( d[i] - mu );
    var /= n;

    /* LOS CÍRCULOS: los índices son 1..n, como el vector. */
    for ( i = 0; i < g->nep; i++ )
        {
        int t;

        if ( !g->marcado[i] ) continue;
        for ( t = g->ep[i].desde; t <= g->ep[i].hasta && nmarks < FO_MAX_RES; t++ )
            marks[nmarks++] = t + 1;
        }

    nombre = g_strdup_printf( "residuos %s", g->id );
    memset( &ser, 0, sizeof ser );
    ser.name    = nombre;
    ser.nobs    = n;
    ser.freq    = g->freq;
    ser.begtime = ( g->tmornsop % g->freq ) + 1;
    ser.begyear = g->tsby;
    ser.mean    = mu;
    ser.var     = var;
    ser.data    = d;

    {
    gchar *dir = atsw_cache_dir();

    base = g_build_filename( dir, "anomalos", NULL );
    g_free( dir );
    }
    {
    FDFig *f;

    /* nrdiff, nadiff y lambda a 0, 0 y 1: los residuos no se transforman,
       que es como los pasa fue.                                       */
    f = fp_PlotSer_CorrSer_marks( &ser, g->nparma, n + g->tmornsop, g->tmornsop,
                                  g->tsby, 1.0, 0, 0, g->lags, escala_fija( g ),
                                  base, nombre, marks, nmarks );
    if ( f ) fd_fig_free( f );
    }

    g_snprintf( g->eps, sizeof g->eps, "%s.eps", base );
    g_free( base );
    g_free( nombre );
    free_vector( d, 1, n );

    if ( !preview_show( (PreviewApp *) g->a, g->eps ) )
        barra_pub( g->a, "No pude mostrar el gráfico." );
}


/* ------------------------------------------------------------------------ */
/* El pie: lo que el dibujo no dice                                          */
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
        "sin ellos <b>%.1f</b> (p = %.3f)   ·   banda ±%.3f / ±%.3f",
        m, qc, chisq_cola( qc, m ), qs, chisq_cola( qs, m ),
        g->c.banda_con, g->c.banda_sin );
    gtk_label_set_markup( GTK_LABEL(g->l_q), t );
    g_free( t );
}

/* QUE RETARDOS CAMBIAN DE LADO. En el gráfico de fue las barras no llevan
 * color --es su dibujo, no el nuestro-- así que el veredicto se dice aquí,
 * con nombre y retardo. Que además se pueda copiar a un informe es ganancia.
 */
static void pie( An *g )
{
    GString *s;
    int      k, hay = 0;

    if ( !g->calibrado )
        {
        gtk_label_set_markup( GTK_LABEL(g->l_pie),
            "<small>Marca los episodios que quieras probar y pulsa "
            "<b>Calibrar</b>. Puedes probarlos de uno en uno o varios a la "
            "vez: no es la misma pregunta.</small>" );
        return;
        }

    s = g_string_new( "<small>" );
    for ( k = 0; k < g->c.nlags; k++ )
        {
        const AnLag *l = &g->c.l[k];

        if ( l->acf != AN_IGUAL )
            {
            g_string_append_printf( s, "%sACF %d <b>%s</b>", hay++ ? " · " : "",
                                    l->lag, an_veredicto_es( l->acf ) );
            }
        if ( l->pacf != AN_IGUAL )
            {
            g_string_append_printf( s, "%sPACF %d <b>%s</b>", hay++ ? " · " : "",
                                    l->lag, an_veredicto_es( l->pacf ) );
            }
        }

    if ( !hay )
        g_string_append( s, "Ningún retardo cambia de lado de la banda: "
                            "intervenir esto <b>no compra nada</b> para la "
                            "identificación." );
    else
        g_string_append( s, "  — quitar esto <b>cambiaría la "
                            "identificación</b>." );
    g_string_append( s, "</small>" );
    gtk_label_set_markup( GTK_LABEL(g->l_pie), s->str );
    g_string_free( s, TRUE );
}


/* ------------------------------------------------------------------------ */
/* Los mandos                                                                */
/* ------------------------------------------------------------------------ */

static void calibra( An *g, gboolean con_omisiones )
{
    int i, t;

    g->nomit = 0;
    if ( con_omisiones )
        for ( i = 0; i < g->nep; i++ )
            {
            if ( !g->marcado[i] ) continue;
            for ( t = g->ep[i].desde; t <= g->ep[i].hasta; t++ )
                if ( g->nomit < FO_MAX_RES ) g->omit[g->nomit++] = t;
            }
    an_calibra( g->o.res, g->o.nres, g->nomit ? g->omit : NULL, g->nomit,
                g->lags, &g->c );
}

static void on_interruptor( GtkToggleButton *b, An *g )
{
    g->sin_anomalos = gtk_toggle_button_get_active( b );
    dibuja( g );
}

/* UNA MARCA. No recalcula: marcar es decir QUE se quiere probar, y probarlo
 * es pulsar el botón. Separar las dos cosas deja marcar tres episodios y ver
 * el efecto DE LOS TRES JUNTOS, que es una pregunta distinta de la de cada
 * uno por separado -- y es la que no se podía hacer.
 *
 * Lo que sí hace es DESHACER la calibración anterior: si cambian las marcas,
 * lo que se está viendo ya no es lo que está marcado, y dejarlo en pantalla
 * sería enseñar una cosa diciendo otra.                                  */
static void on_marca( GtkToggleButton *b, An *g )
{
    int i = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(b), "ep" ) );

    if ( i < 0 || i >= g->nep ) return;
    g->marcado[i] = gtk_toggle_button_get_active( b );

    if ( g->calibrado )
        {
        g->calibrado = FALSE;
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g->b_ver), FALSE );
        gtk_widget_set_sensitive( g->b_ver, FALSE );
        g->sin_anomalos = FALSE;
        calibra( g, FALSE );
        di_q( g );
        }
    pie( g );
    dibuja( g );
}

static void on_calibrar( GtkButton *b, An *g )
{
    (void) b;
    calibra( g, TRUE );
    g->calibrado = TRUE;

    /* Al calibrar se ENSEÑA el resultado: es lo que se acaba de pedir. El
       interruptor queda para volver al original y comparar.          */
    gtk_widget_set_sensitive( g->b_ver, TRUE );
    g->sin_anomalos = TRUE;
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g->b_ver), TRUE );

    di_q( g );
    pie( g );
    dibuja( g );

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
    GtkWidget *pie_caja, *fila, *chips, *sc, *b, *cab;
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
        g->z[i] = ( sd > 0.0 ) ? g->o.res[i] / sd : 0.0;

    g->nep = an_episodios( g->z, g->o.nres, g->umbral, AN_VENTANA,
                           g->ep, AN_MAX_EP );
    }

    /* EL EJE DE TIEMPO. tmornsop son los períodos que van de enero (o del
       primer período del año) al primer residuo: con eso las etiquetas de
       año de fugplot caen donde tienen que caer, igual que en fue, que le
       pasa las observaciones que perdió al diferenciar.               */
    {
    int anno, per;

    g->freq = frecuencia( g );
    parte_fecha( g->o.res_fecha[0], &anno, &per );
    if ( per > g->freq ) per = 1;
    g->tsby     = anno;
    g->tmornsop = per - 1;
    }

    g->nparma = g->o.p + g->o.q + g->o.P + g->o.Q;
    g->lags   = default_lags( g->o.nres, g->freq );
    if ( g->lags > AN_MAX_LAG ) g->lags = AN_MAX_LAG;
    if ( g->lags > g->o.nres - 2 ) g->lags = g->o.nres - 2;

    /* SIN NADA MARCADO al abrir: el correlograma que se ve es el de siempre,
       y calibrar es un acto, no un estado de partida.                 */
    calibra( g, FALSE );

    /* ------------------------------------------------------------------ */
    /* EL PIE DEL GRAFICO, que es donde van los mandos.                    */
    /*                                                                     */
    /* No hay una ventana de anómalos aparte: la figura ES la ventana, y   */
    /* debajo van las casillas, el botón y lo que el dibujo no dice. Es el */
    /* mismo sitio donde el vistazo pone lambda, d y D -- el analista      */
    /* trabaja al pie del gráfico, no en otra ventana mirando ésta.        */
    /* ------------------------------------------------------------------ */

    pie_caja = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
    gtk_container_set_border_width( GTK_CONTAINER(pie_caja), 8 );

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
    gtk_box_pack_start( GTK_BOX(pie_caja), cab, FALSE, FALSE, 0 );

    /* LAS CASILLAS, UNA POR EPISODIO, CON SU FECHA. La fecha es la misma que
       va a quedar rodeada en el dibujo: se lee la casilla y se busca el
       círculo, sin tabla intermedia que traducir.                    */
    chips = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 10 );
    for ( i = 0; i < g->nep; i++ )
        {
        gchar     *et;
        GtkWidget *k;

        if ( g->ep[i].n > 1 )
            et = g_strdup_printf( "%s–%s  (%+.1f)", g->o.res_fecha[g->ep[i].desde],
                                  g->o.res_fecha[g->ep[i].hasta], g->ep[i].z_max );
        else
            et = g_strdup_printf( "%s  (%+.1f)", g->o.res_fecha[g->ep[i].desde],
                                  g->ep[i].z_max );
        k = gtk_check_button_new_with_label( et );
        g_free( et );
        g_object_set_data( G_OBJECT(k), "ep", GINT_TO_POINTER(i) );
        g_signal_connect( k, "toggled", G_CALLBACK(on_marca), g );
        gtk_box_pack_start( GTK_BOX(chips), k, FALSE, FALSE, 0 );
        }

    if ( g->nep == 0 )
        gtk_box_pack_start( GTK_BOX(chips),
            gtk_label_new( "Ningún residuo pasa del umbral." ), FALSE, FALSE, 0 );

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER );
    gtk_container_add( GTK_CONTAINER(sc), chips );
    gtk_widget_set_size_request( sc, -1, 38 );
    gtk_box_pack_start( GTK_BOX(pie_caja), sc, FALSE, FALSE, 0 );

    fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 8 );
    gtk_box_pack_start( GTK_BOX(pie_caja), fila, FALSE, FALSE, 0 );

    b = gtk_button_new_with_label( "Calibrar" );
    gtk_widget_set_tooltip_text( b,
        "Redibuja la ACF y la PACF omitiendo los episodios marcados, y dice "
        "qué retardos cambian de lado de la banda.\n\nMarcar varios y "
        "calibrar de una vez NO es lo mismo que calibrarlos uno a uno: son "
        "dos preguntas distintas." );
    g_signal_connect( b, "clicked", G_CALLBACK(on_calibrar), g );
    gtk_box_pack_start( GTK_BOX(fila), b, FALSE, FALSE, 0 );

    g->b_ver = gtk_check_button_new_with_label( "ver calibrado" );
    gtk_widget_set_tooltip_text( g->b_ver,
        "Quítalo para volver al gráfico original y comparar: es la MISMA "
        "figura moviéndose." );
    gtk_widget_set_sensitive( g->b_ver, FALSE );
    g_signal_connect( g->b_ver, "toggled", G_CALLBACK(on_interruptor), g );
    gtk_box_pack_start( GTK_BOX(fila), g->b_ver, FALSE, FALSE, 0 );

    g->l_q = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(g->l_q), 0.0 );
    gtk_box_pack_start( GTK_BOX(fila), g->l_q, TRUE, TRUE, 8 );

    g->l_pie = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(g->l_pie), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(g->l_pie), TRUE );
    gtk_box_pack_start( GTK_BOX(pie_caja), g->l_pie, FALSE, FALSE, 0 );

    di_q( g );
    pie( g );
    gtk_widget_show_all( pie_caja );

    dibuja( g );
    if ( !preview_set_footer( g->eps, pie_caja ) )
        {
        gtk_widget_destroy( pie_caja );
        barra_pub( a, "No pude abrir la ventana del gráfico." );
        g_free( g );
        return;
        }
    g_signal_connect( pie_caja, "destroy", G_CALLBACK(on_cerrar), g );
}
