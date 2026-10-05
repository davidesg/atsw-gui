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
 * EL BOTON ES EL ESTADO. «Calibrar» se queda pulsado mientras se ve el
 * gráfico sin los anómalos, y se suelta para volver al de siempre: un solo
 * mando para una sola pregunta, y la posición del botón dice cuál de los dos
 * dibujos estás mirando. Eran dos --un botón que calculaba y una casilla que
 * enseñaba-- y esa separación sobraba: calcular sin enseñar no sirve de nada.
 *
 * REDIBUJA, NO AÑADE. Lo que hay que ver es la MISMA figura moviéndose:
 * «¿corto el AR en el 2?» se responde mirando UN dibujo. Dos barras por
 * retardo, una al lado de otra, cambiarían la lectura de siempre; y un
 * degradé con «lo que aporta el anómalo» afirmaría una descomposición que no
 * existe, porque r_con y r_sin son dos cocientes con distinto denominador.
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

#include "analisis.h"
#include "preview.h"
#include "fugplot.h"


typedef struct {
    AnHost     h;
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
    gboolean   calibrado;          /* ya se calibró alguna vez             */
    gboolean   sin_anomalos;       /* el botón está pulsado                */
    gboolean   armando;            /* no reentrar al mover el botón a mano */

    char       eps[PR_RUTA];
    GtkWidget *b_cal, *b_sug, *l_q, *l_jb, *l_pie;
} An;


/* ------------------------------------------------------------------------ */
/* El eje de tiempo, de las fechas de los residuos                           */
/* ------------------------------------------------------------------------ */

/* EL PARTIDOR ES EL DE lib/outfile, y no uno de aquí.
 *
 * El primero que escribí leía «3/2022» como año 3, período 2022 --al revés
 * de como el motor las escribe-- y no reventó: se coló hasta la fecha con la
 * que se iba a escribir la intervención, y el botón «Derivar» se negaba sin
 * decir por qué. Quien lee el .out es quien sabe leer sus fechas.      */
static void parte_fecha( const char *f, int *anno, int *per )
{
    if ( fo_fecha_parte( f, per, anno ) != 0 ) { *per = 1; *anno = 0; }
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
    (void) anno;
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
    gchar *dir = an_cache_dir();

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

    if ( !preview_show( g->h.preview, g->eps ) )
        an_di( &g->h, "No pude mostrar el gráfico." );
}


/* ------------------------------------------------------------------------ */
/* El pie: lo que el dibujo no dice                                          */
/* ------------------------------------------------------------------------ */

/* LA NORMALIDAD, CON Y SIN, AL LADO DEL Q.
 *
 * Es la otra mitad de la diagnosis que un anómalo mueve, y la mueve MAS: la
 * curtosis va a la cuarta potencia, así que un extremo la dispara mientras
 * apenas toca el Q. Quien mira sólo la autocorrelación no ve la mayor parte
 * de lo que el suceso está haciendo.
 *
 * Sin histograma: lo que se pregunta aquí es cuánto del rechazo es del
 * suceso, y eso es un número.                                          */
static void di_jb( An *g )
{
    AnNormal con, sinellos;
    gchar   *t;

    if ( an_normalidad( g->z, g->o.nres, g->nomit ? g->omit : NULL, g->nomit,
                        &con, &sinellos ) != 0 )
        return;

    if ( g->nomit == 0 )
        t = g_markup_printf_escaped(
            "<tt>JB</tt>  <b>%.1f</b> (p = %.3f)   ·   asimetría %+.2f, "
            "curtosis %+.2f", con.jb, chisq_cola( con.jb, 2 ), con.skew,
            con.kurt );
    else
        t = g_markup_printf_escaped(
            "<tt>JB</tt>  con <b>%.1f</b> (p = %.3f)   ·   sin <b>%.1f</b> "
            "(p = %.3f)   ·   curtosis %+.2f → %+.2f, sobre %d observaciones",
            con.jb, chisq_cola( con.jb, 2 ), sinellos.jb,
            chisq_cola( sinellos.jb, 2 ), con.kurt, sinellos.kurt,
            sinellos.n );
    gtk_label_set_markup( GTK_LABEL(g->l_jb), t );
    g_free( t );
}

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
            "<b>Calibrar</b>: el botón se queda hundido mientras ves el "
            "gráfico sin ellos, y al soltarlo vuelve el de siempre. Puedes "
            "probarlos de uno en uno o varios a la vez: no es la misma "
            "pregunta.</small>" );
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

static int marcados( const An *g )
{
    int i, n = 0;

    for ( i = 0; i < g->nep; i++ ) if ( g->marcado[i] ) n++;
    return n;
}

static void hay_marcas( An *g )
{
    if ( g->b_sug ) gtk_widget_set_sensitive( g->b_sug, marcados( g ) > 0 );
}

/* DE CALIBRAR A INTERVENIR, QUE ES EL PASO SIGUIENTE Y NO EL MISMO.
 *
 * Calibrar contesta «¿cambia la identificación si quito esto?». Si la
 * respuesta es que sí, queda la otra pregunta: qué se le pone. Y se le pone
 * AQUI porque aquí están los extremos con su signo, que es de donde sale la
 * forma -- en cualquier otro sitio habría que volver a buscarlos.
 *
 * Se mandan los EXTREMOS, no el rango del episodio: la forma la decide la
 * firma que dejan, no cuántos períodos dura.                            */
static void on_sugerir( GtkButton *b, An *g )
{
    AnSuceso s[AN_MAX_SUC];
    int      i, ns = 0;

    (void) b;
    for ( i = 0; i < g->nep && ns < AN_MAX_SUC; i++ )
        {
        AnSuceso *x = &s[ns];
        int       t, anno, per;

        if ( !g->marcado[i] ) continue;
        memset( x, 0, sizeof *x );
        x->desde = g->ep[i].desde;
        x->hasta = g->ep[i].hasta;
        /* EL SUCESO ES EL TRAMO ENTERO, no los períodos que pasan el umbral
           de declarar.
        
           Este filtro era de la regla vieja, cuando un episodio se construía
           juntando puntos que cruzaban un listón. Con el escáner el tramo YA
           es la unidad --se declara entero y es sólido por construcción--, y
           volver a filtrarlo aquí deshacía justo lo que la detección acababa
           de decidir: en el caso de m04 se marcaban los círculos sobre
           1-3/2021 y la intervención se fechaba en 2/2021, porque 1/2021
           (z = 3,01) no llegaba al umbral de DECLARAR. La ventana decía una
           cosa y proponía otra.                                        */
        for ( t = g->ep[i].desde; t <= g->ep[i].hasta && x->next < AN_MAX_EXT; t++ )
            { x->obs[x->next] = t; x->z[x->next] = g->z[t]; x->next++; }
        if ( x->next == 0 ) continue;      /* no puede pasar, pero no se fía */

        /* LA FECHA ES LA DEL PRIMER PERIODO DEL TRAMO, y se lee del .out: la
           fecha contra la que el motor va a construir el regresor es la
           suya, no una que compongamos nosotros.                       */
        g_snprintf( x->fecha, sizeof x->fecha, "%s", g->o.res_fecha[x->obs[0]] );
        parte_fecha( x->fecha, &anno, &per );
        x->anno = anno;
        x->per  = ( g->freq > 1 ) ? per : 1;

        /* LA VENTANA LLEGA MAS ALLA DE LA DIFERENCIACION: con D = 1 la huella
           de la forma reaparece a s períodos, y si la ventana se cortara
           antes, lo que la forma NO explica caería fuera del dibujo.   */
        {
        int cola = g->o.d + g->freq * g->o.D + 6;
        int fin;

        x->base = g->ep[i].desde - 6;
        if ( x->base < 0 ) x->base = 0;
        fin = g->ep[i].hasta + cola;
        if ( fin > g->o.nres - 1 ) fin = g->o.nres - 1;
        x->nwin = fin - x->base + 1;
        if ( x->nwin > AN_VENTANA_Z ) x->nwin = AN_VENTANA_Z;
        for ( t = 0; t < x->nwin; t++ ) x->zwin[t] = g->z[x->base + t];
        /* EL RESTO SE JUZGA CON EL UMBRAL DEL VECINO, no con el de
           declarar. La pregunta de Treadway --«¿queda un anómalo al lado
           de la forma?»-- es condicional: ya sabemos que ahí hay un
           suceso. Con el umbral alto se queda ciega justo en el tramo
           (2, 3) sigma, que es donde mas sensible es (art, BUG-0087). */
        x->umbral = AN_UMBRAL_VECINO;
        }
        ns++;
        }
    if ( ns == 0 ) { an_di( &g->h, "Marca primero el suceso que quieras "
                                      "intervenir." ); return; }

    an_sugerir( &g->h, g->serie, g->muestra, g->id, g->o.d, g->o.D,
                  g->freq, s, ns );
}

/* EL BOTON HUNDIDO ES «ESTOY VIENDO EL CALIBRADO».
 *
 * Al hundirlo se calibra con lo que esté marcado y se enseña; al soltarlo se
 * vuelve al gráfico de siempre SIN perder la calibración: el veredicto por
 * retardo sigue en el pie, que es justo lo que se quiere leer mientras se
 * mira el original.
 *
 * Y si no hay nada marcado, el botón NO se queda hundido: quedarse hundido
 * enseñando el dibujo de siempre sería decir que eso es el calibrado.   */
static void on_calibrar( GtkToggleButton *b, An *g )
{
    if ( g->armando ) return;

    if ( !gtk_toggle_button_get_active( b ) )
        {
        g->sin_anomalos = FALSE;
        pie( g );
        dibuja( g );
        return;
        }

    calibra( g, TRUE );
    if ( g->nomit == 0 )
        {
        g->armando = TRUE;
        gtk_toggle_button_set_active( b, FALSE );
        g->armando = FALSE;
        an_di( &g->h, "Marca primero el episodio que quieras calibrar." );
        return;
        }

    g->calibrado    = TRUE;
    g->sin_anomalos = TRUE;
    di_q( g );
    di_jb( g );
    pie( g );
    dibuja( g );
}

/* UNA MARCA. No recalcula: marcar es decir QUE se quiere probar, y probarlo
 * es pulsar el botón. Separar las dos cosas deja marcar tres episodios y ver
 * el efecto DE LOS TRES JUNTOS, que es una pregunta distinta de la de cada
 * uno por separado -- y es la que no se podía hacer.
 *
 * Con el botón hundido, cambiar una marca RECALIBRA en el sitio: lo que se ve
 * tiene que ser lo que está marcado, y soltar el botón para volver a hundirlo
 * sería un paso de más. Con el botón suelto, sólo se mueve el círculo.   */
static void on_marca( GtkToggleButton *b, An *g )
{
    int i = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(b), "ep" ) );

    if ( i < 0 || i >= g->nep ) return;
    g->marcado[i] = gtk_toggle_button_get_active( b );
    hay_marcas( g );

    if ( g->sin_anomalos )
        {
        calibra( g, TRUE );
        if ( g->nomit == 0 )          /* se quitó la última marca */
            {
            g->armando = TRUE;
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(g->b_cal), FALSE );
            g->armando = FALSE;
            g->sin_anomalos = FALSE;
            g->calibrado    = FALSE;
            calibra( g, FALSE );
            }
        di_q( g );
        di_jb( g );
        }
    pie( g );
    dibuja( g );
}

static void on_cerrar( GtkWidget *w, An *g ) { (void) w; g_free( g ); }


/* ------------------------------------------------------------------------ */

void an_anomalos( const AnHost *h, const char *serie, const char *muestra,
                  const char *id )
{
    An        *g;
    GtkWidget *pie_caja, *fila, *chips, *sc, *b, *cab;
    char       out[PR_RUTA];
    int        i;

    if ( !h || !h->p || !serie || !*serie || !id || !*id ) return;
    if ( pr_ruta( h->p, serie, muestra, id, ".out", out, sizeof out ) != 0 )
        { an_di( h, "No pude componer la ruta." ); return; }

    g = g_new0( An, 1 );
    g->h = *h;
    g_snprintf( g->serie, PR_ID, "%s", serie );
    g_snprintf( g->muestra, PR_ID, "%s", muestra ? muestra : "" );
    g_snprintf( g->id, PR_ID, "%s", id );

    if ( !fueout_read( out, &g->o ) || g->o.nres < 8 )
        {
        gchar *s = g_strdup_printf( "«%s» no está estimado, o su informe no "
                                    "trae los residuos.", id );

        an_di( h, "%s", s ); g_free( s ); g_free( g );
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

    /* SE PUNTUA EL TRAMO, NO LOS PUNTOS. Un 3 sigma con un 2 al lado es,
       en un gaussiano, mucho más improbable que el 3 solo -- y una regla
       punto a punto no sabe verlo. Ver lib/anomalos.             */
    g->nep = an_episodios( g->z, g->o.nres, g->umbral, g->ep, AN_MAX_EP );
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
        "<b>%s / %s</b>%s%s   ·   %d episodio%s sobre %d residuos   ·   "
        "un tramo es un suceso cuando es tan improbable como un extremo "
        "aislado de |z| ≥ %.2f: por eso un 3σ con un 2σ al lado cuenta y un "
        "2σ solo no",
        serie, id,
        ( muestra && *muestra ) ? "   muestra " : "",
        ( muestra && *muestra ) ? muestra : "",
        g->nep, g->nep == 1 ? "" : "s", g->o.nres, g->umbral );

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

    b = gtk_toggle_button_new_with_label( "Calibrar" );
    gtk_widget_set_tooltip_text( b,
        "Redibuja la ACF y la PACF omitiendo los episodios marcados, y dice "
        "qué retardos cambian de lado de la banda.\n\nSe queda hundido "
        "mientras ves el gráfico sin los anómalos; suéltalo para volver al de "
        "siempre y comparar: es la MISMA figura moviéndose.\n\nMarcar varios "
        "y calibrar de una vez NO es lo mismo que calibrarlos uno a uno: son "
        "dos preguntas distintas." );
    g->b_cal = b;
    g_signal_connect( b, "toggled", G_CALLBACK(on_calibrar), g );
    gtk_box_pack_start( GTK_BOX(fila), b, FALSE, FALSE, 0 );

    g->b_sug = gtk_button_new_with_label( "Sugerir intervención…" );
    gtk_widget_set_tooltip_text( g->b_sug,
        "Dice qué FORMA pide cada suceso marcado --escalón, impulso-- y por "
        "qué, y puede derivar un modelo con esas intervenciones ya escritas "
        "en su .inp.\n\nCalibrar contesta «¿cambia la identificación si "
        "quito esto?». Esto contesta la siguiente: qué se le pone." );
    gtk_widget_set_sensitive( g->b_sug, FALSE );
    g_signal_connect( g->b_sug, "clicked", G_CALLBACK(on_sugerir), g );
    gtk_box_pack_start( GTK_BOX(fila), g->b_sug, FALSE, FALSE, 0 );

    g->l_q = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(g->l_q), 0.0 );
    /* Con ajuste de linea: sin el, una etiqueta exige de ancho minimo su
       texto entero, y el Q con y sin, mas sus botones, imponia a la ventana
       un minimo que en una pantalla pequeña no cabe.                   */
    gtk_label_set_line_wrap( GTK_LABEL(g->l_q), TRUE );
    gtk_box_pack_start( GTK_BOX(fila), g->l_q, TRUE, TRUE, 8 );

    g->l_jb = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(g->l_jb), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(g->l_jb), TRUE );
    gtk_widget_set_tooltip_text( g->l_jb,
        "La normalidad de los residuos, con y sin lo marcado. Un extremo "
        "dispara la curtosis --va a la cuarta potencia-- mucho más de lo que "
        "mueve el Q: quien mira sólo la autocorrelación no ve la mayor parte "
        "de lo que el suceso hace.\n\nSe calcula sobre las observaciones que "
        "QUEDAN, sin rellenar los huecos: rellenarlos sería añadir "
        "observaciones que no se observaron, justo donde más pesan." );
    gtk_box_pack_start( GTK_BOX(pie_caja), g->l_jb, FALSE, FALSE, 0 );

    g->l_pie = gtk_label_new( NULL );
    gtk_label_set_xalign( GTK_LABEL(g->l_pie), 0.0 );
    gtk_label_set_line_wrap( GTK_LABEL(g->l_pie), TRUE );
    gtk_box_pack_start( GTK_BOX(pie_caja), g->l_pie, FALSE, FALSE, 0 );

    di_q( g );
    di_jb( g );
    pie( g );
    gtk_widget_show_all( pie_caja );

    dibuja( g );
    if ( !preview_set_footer( g->eps, pie_caja ) )
        {
        gtk_widget_destroy( pie_caja );
        an_di( h, "No pude abrir la ventana del gráfico." );
        g_free( g );
        return;
        }
    g_signal_connect( pie_caja, "destroy", G_CALLBACK(on_cerrar), g );
}
