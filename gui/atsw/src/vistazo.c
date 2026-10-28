/*
 * vistazo.c -- el atajo: los graficos de identificacion sin levantar fug_gui.
 *
 * LO QUE DIBUJA ES fug, NO UNA IMITACION. El vistazo escribe un .inp con la
 * transformacion pedida y LLAMA AL MOTOR. Asi el grafico rapido es byte a byte
 * el que saldria de la herramienta completa -- y sigue valiendo la regla que
 * acaba de fijarse: LA ESPECIFICACION VIVE EN EL .inp, nunca en la orden.
 *
 * Reimplementar aqui el dibujo habria sido abrir una segunda forma de hacer el
 * mismo grafico, que es exactamente como esta suite se llenó de divergencias.
 *
 * TRES GESTOS, Y SON DISTINTOS:
 *
 *   media - desviacion tipica   una pregunta CERRADA: ¿la dispersion crece con
 *                               el nivel? Se contesta mirando dos dibujos, con
 *                               logaritmos y sin ellos. No necesita controles.
 *
 *   serie + acf/pacf            una pregunta ABIERTA: cuantas diferencias. Ahi
 *                               el analista prueba, y por eso lambda, d y D
 *                               van AL PIE DEL GRAFICO y el dibujo se rehace
 *                               sin cerrar nada.
 *
 *   histograma                  la forma de la distribucion de la serie
 *                               transformada: con lambda, d y D al pie, como
 *                               la anterior, porque es de esa serie.
 *
 * El .inp del vistazo es TEMPORAL y vive en la cache: no se toca el del
 * proyecto. Un vistazo no es una decision, y no deja rastro en el manifiesto.
 *
 * FUG CORRE SIN PARAR LA VENTANA (gui/atsw/BUGS.md BUG-0001). Se lanzaba con
 * g_spawn_sync en el hilo de la interfaz, una vez por cada clic en un mando:
 * en Linux no se notaba, pero en Windows cada corrida eran dos procesos (el
 * ayudante de GLib y fug), una ventana de consola que asomaba y el antivirus
 * mirando cada uno, y la ventana se quedaba congelada entre clic y clic.
 * Ahora:
 *
 *   - un cambio en los mandos espera VZ_ESPERA_MS a que no llegue otro antes
 *     de dibujar: tres clics seguidos en λ son UNA corrida, no tres;
 *   - fug corre con engine_start (lib/engine), sin consola en Windows, y la
 *     ventana sigue viva mientras: se ve el dibujo anterior hasta que llega
 *     el nuevo;
 *   - hay UNA corrida a la vez --todas escriben en el mismo directorio--. Si
 *     se pide otra con una en marcha, esa se para y su resultado se tira; al
 *     acabar se lanza la ultima pedida.
 */

#include <string.h>
#include <stdlib.h>

#include <glib/gstdio.h>

#include "atsw.h"
#include "preview.h"
#include "inpfile.h"
#include "sitio.h"
#include "analisis.h"
#include "anfitrion.h"
#include "rutas.h"
#include "engine.h"

void barra_pub( Atsw *a, const char *s );

typedef struct {
    Atsw      *a;
    char       origen[PR_RUTA];     /* el .inp de la serie                 */
    char       base[PR_RUTA];       /* <cache>/vistazo                     */
    char       eps[PR_RUTA];        /* el que se enseña                    */
    GtkWidget *s_lam, *s_d, *s_D;
    gboolean   armando;             /* poniendo valores: no redibujar      */
    int        modo;                /* 0 serie+acf/pacf, 1 media-dt,       */
                                    /* 2 histograma                        */
    char       serie[PR_ID];        /* de quien es, para el cuaderno: la   */
    char       muestra[PR_ID];      /* marcada puede cambiar con la ventana */
                                    /* abierta                             */
    GtkWidget *pie;                 /* el pie, hasta que llegue el primer  */
                                    /* dibujo y haya ventana donde ponerlo */
    double     lam_h;               /* la transformacion del dibujo que SE */
    int        d_h, D_h;            /* VE, que no es la de los mandos      */
} Vistazo;                          /* mientras se espera al nuevo         */

static Vistazo V;

/* LA CORRIDA, FUERA DE V: atsw_vistazo pone V a cero y una corrida del
 * vistazo anterior puede seguir en marcha.                              */
#define VZ_ESPERA_MS 250
static EngineJob *vz_job;           /* fug en marcha, o NULL               */
static gboolean   vz_otra;          /* se pidio otra: esta no vale         */
static guint      vz_reloj;         /* la espera tras tocar un mando       */
static guint      vz_lanzadas;      /* cuantas van (lo mira la prueba)     */
static double     vz_lam;           /* con que transformacion se lanzo la  */
static int        vz_d, vz_D;       /* que esta en marcha                  */

/* El directorio del vistazo, en la cache: es tierra de nadie a proposito. */
gchar *atsw_cache_dir( void )
{
    gchar *d = g_build_filename( g_get_user_cache_dir(), "atsw_gui", NULL );

    g_mkdir_with_parents( d, 0700 );
    return d;
}

/* Escribe el .inp del vistazo con la transformacion pedida. 0 si pudo.
 *
 * Se parte del .inp de la serie --sus datos, su frecuencia, su fecha-- y solo
 * se cambia la transformacion. Nada de esto vuelve al proyecto.        */
static int escribe_inp( const char *origen, const char *destino,
                        double lam, int d, int D, char *why, size_t n )
{
    InpFile inp;
    char    msg[256];

    if ( inp_read( origen, &inp, msg, sizeof msg ) != 0 )
        {
        /* Los dos se recortan a proposito: esto es un mensaje. */
        snprintf( why, n, "%.*s: %.*s", (int) n / 3, origen,
                  (int) n / 3, msg );
        return 1;
        }

    inp.model  = 0;              /* el vistazo no lleva modelo            */
    inp.boxlam = lam;
    inp.boxm   = 0.0;
    inp.nrdiff = d;
    inp.nadiff = D;

    if ( inp_write_bare( destino, &inp ) != 0 )
        { snprintf( why, n, "no pude escribir %.*s", (int) n - 20, destino );
          inp_free( &inp ); return 1; }

    inp_free( &inp );
    return 0;
}

static void vz_acabo( const EngineResult *r, gpointer data );
static void limpia_eps( const char *dir );

/* Lanza el motor con lo que digan ahora los mandos, y vuelve enseguida: el
 * dibujo llega por vz_acabo(). FALSE, con why, si no se pudo lanzar.       */
static gboolean lanza_fug( Vistazo *v, char *why, size_t n )
{
    gchar  *dir = atsw_cache_dir();
    gchar  *exe = NULL;
    double  lam = gtk_spin_button_get_value( GTK_SPIN_BUTTON(v->s_lam) );
    /* En media - desviacion tipica no hay d ni D, y no es una omision: ese
     * grafico es de la serie EN NIVEL. Comprobado -- con d=1 D=1 en el .inp,
     * "fug -e" sigue escribiendo m_dt_d0...                            */
    int     d   = v->s_d ? gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(v->s_d) ) : 0;
    int     D   = v->s_D ? gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(v->s_D) ) : 0;
    const char *argv[3];
    char    inp[PR_RUTA + 8];

    snprintf( inp, sizeof inp, "%s.inp", v->base );
    if ( escribe_inp( v->origen, inp, lam, d, D, why, n ) != 0 )
        { g_free( dir ); return FALSE; }

    /* El motor: el mismo que usa fug_gui. Se busca al lado antes que en el
     * PATH, por lo mismo que en gestos.c.                              */
    {
    static const char *const sitio[] = { "../../engines/fug/%s", "./%s", NULL };

    exe = sitio_busca( "fug", sitio );
    }
    if ( exe == NULL )
        { snprintf( why, n, "No encuentro el motor fug." ); g_free( dir );
          return FALSE; }

    /* Los EPS de la corrida anterior, fuera: el que quede sera el nuevo. */
    limpia_eps( dir );
    argv[0] = v->base;
    argv[1] = v->modo == 1 ? "-e" : v->modo == 2 ? "-d" : "-c";
    argv[2] = NULL;
    vz_lam = lam; vz_d = d; vz_D = D;
    vz_job = engine_start( dir, exe, argv, NULL, NULL, vz_acabo, v );
    if ( vz_job ) vz_lanzadas++;
    if ( vz_job == NULL )
        snprintf( why, n, "fug: no se pudo lanzar el motor." );

    g_free( exe ); g_free( dir );
    return vz_job != NULL;
}

/* EL NOMBRE DEL EPS NO SE COMPONE: SE BUSCA.
 *
 * El motor lo arma con la transformacion --d2lnvistazo.eps, m_dt_d0vistazo.eps
 * y demas-- y la primera version de esto lo reproducia. Duro una prueba: la
 * «d» va SIEMPRE, tambien cuando es 0, y yo la omitia. Un nombre compuesto en
 * dos sitios se separa, y aqui no hay ninguna necesidad.
 *
 * Se limpia el directorio antes de llamar al motor y despues se mira cual
 * aparecio. El que sabe como se llama es el que lo escribe.           */
static gchar *busca_eps( const char *dir )
{
    GDir       *d = g_dir_open( dir, 0, NULL );
    const char *n;
    gchar      *hallado = NULL;

    if ( d == NULL ) return NULL;
    while ( ( n = g_dir_read_name( d ) ) != NULL )
        if ( g_str_has_suffix( n, ".eps" ) && strcmp( n, "vistazo.eps" ) )
            { hallado = g_build_filename( dir, n, NULL ); break; }
    g_dir_close( d );
    return hallado;
}

/* Los EPS de la corrida anterior, fuera: asi el que quede es el nuevo. */
static void limpia_eps( const char *dir )
{
    GDir       *d = g_dir_open( dir, 0, NULL );
    const char *n;

    if ( d == NULL ) return;
    while ( ( n = g_dir_read_name( d ) ) != NULL )
        if ( g_str_has_suffix( n, ".eps" ) && strcmp( n, "vistazo.eps" ) )
            {
            gchar *p = g_build_filename( dir, n, NULL );

            g_unlink( p );
            g_free( p );
            }
    g_dir_close( d );
}

/* El pie que no llego a ponerse, fuera: el primer dibujo no llego. */
static void sin_pie( Vistazo *v )
{
    if ( v->pie ) gtk_widget_destroy( v->pie );
    v->pie = NULL;
}

/* PEDIR UN DIBUJO, YA. Con una corrida en marcha no se lanza otra: se para
 * la que hay y, cuando avise de que acabó, se lanza esta.            */
static void pide( Vistazo *v )
{
    char why[512];

    if ( vz_reloj ) { g_source_remove( vz_reloj ); vz_reloj = 0; }
    if ( vz_job )
        { vz_otra = TRUE; engine_stop( vz_job ); return; }
    if ( !lanza_fug( v, why, sizeof why ) )
        { barra_pub( v->a, why ); sin_pie( v ); }
}

/* El motor acabó: si su dibujo sigue valiendo, a la ventana. */
static void vz_acabo( const EngineResult *r, gpointer data )
{
    Vistazo *v = data;
    char     why[512];
    gchar   *dir, *sal;

    vz_job = NULL;
    if ( vz_otra )                      /* lo pedido ya es otra cosa      */
        { vz_otra = FALSE; pide( v ); return; }

    if ( r->status != 0 )
        {
        snprintf( why, sizeof why, "fug: %s",
                  ( r->output && *r->output ) ? r->output : r->message );
        barra_pub( v->a, why );
        sin_pie( v );
        return;
        }

    dir = atsw_cache_dir();
    sal = busca_eps( dir );
    if ( sal == NULL )
        {
        g_free( dir );
        barra_pub( v->a, "El motor no dejó ningún gráfico. ¿Esa "
                         "transformación tiene sentido para esta serie?" );
        sin_pie( v );
        return;
        }

    /* La MISMA ruta siempre que se pueda: preview reutiliza la ventana y la
     * recarga, asi que el pie no parpadea. Como el nombre lo pone la
     * transformacion, se copia al fijo.                                */
    {
    gchar *fijo = g_build_filename( dir, "vistazo.eps", NULL );
    gchar *c = NULL;
    gsize  ln = 0;

    if ( g_file_get_contents( sal, &c, &ln, NULL ) )
        { g_file_set_contents( fijo, c, (gssize) ln, NULL ); g_free( c ); }
    snprintf( v->eps, sizeof v->eps, "%s", fijo );
    g_free( fijo );
    }
    g_free( sal ); g_free( dir );
    v->lam_h = vz_lam; v->d_h = vz_d; v->D_h = vz_D;

    preview_show( (PreviewApp *) v->a, v->eps );
    /* El pie va con el PRIMER dibujo: antes no habia ventana. Va en LOS DOS
     * graficos: el de media - desviacion tipica tambien deja tocar
     * lambda, que es justo la pregunta que contesta.                   */
    if ( v->pie )
        {
        if ( !preview_set_footer( v->eps, v->pie ) )
            gtk_widget_destroy( v->pie );
        v->pie = NULL;
        }
    barra_pub( v->a, "" );
}

static gboolean al_reloj( gpointer d )
{
    vz_reloj = 0;
    pide( d );
    return G_SOURCE_REMOVE;
}

/* Un clic en un mando: se dibuja cuando dejen de llegar. */
static void on_cambio( GtkWidget *w, Vistazo *v )
{
    (void) w;
    if ( v->armando ) return;
    if ( vz_reloj ) g_source_remove( vz_reloj );
    vz_reloj = g_timeout_add( VZ_ESPERA_MS, al_reloj, v );
}

/* ------------------------------------------------------------------------ */

/* EL PIE, Y LO QUE LLEVA DEPENDE DEL GRAFICO.
 *
 *   serie + acf/pacf   lambda, d y D: la pregunta es cuantas diferencias.
 *   histograma         lambda, d y D tambien: fug dibuja el de la serie
 *                      transformada y diferenciada.
 *   media - desv. tip. SOLO lambda: ese grafico es de la serie EN NIVEL, asi
 *                      que d y D no le dicen nada. Poner unos mandos que no
 *                      hacen nada seria peor que no ponerlos.
 *
 * Lo que los dos comparten es lo que importa: la transformacion se toca AL
 * PIE DEL DIBUJO y el dibujo se rehace ahi mismo.                       */
static void on_identificar( GtkButton *b, Vistazo *v );

static GtkWidget *pie_nuevo( Vistazo *v, double lam, int d, int D )
{
    GtkWidget *caja = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );

    gtk_container_set_border_width( GTK_CONTAINER(caja), 6 );

    v->armando = TRUE;
    v->s_d = v->s_D = NULL;

    gtk_box_pack_start( GTK_BOX(caja), gtk_label_new( "λ" ), FALSE, FALSE, 0 );
    v->s_lam = gtk_spin_button_new_with_range( -2.0, 2.0, 0.5 );
    gtk_spin_button_set_digits( GTK_SPIN_BUTTON(v->s_lam), 2 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(v->s_lam), lam );
    gtk_widget_set_tooltip_text( v->s_lam,
        "Box-Cox. 0 son logaritmos, 1 la serie sin transformar." );
    gtk_box_pack_start( GTK_BOX(caja), v->s_lam, FALSE, FALSE, 0 );

    if ( v->modo != 1 )
        {
        gtk_box_pack_start( GTK_BOX(caja), gtk_label_new( "  d" ), FALSE, FALSE, 0 );
        v->s_d = gtk_spin_button_new_with_range( 0, 3, 1 );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(v->s_d), d );
        gtk_widget_set_tooltip_text( v->s_d, "Diferencias regulares." );
        gtk_box_pack_start( GTK_BOX(caja), v->s_d, FALSE, FALSE, 0 );

        gtk_box_pack_start( GTK_BOX(caja), gtk_label_new( "  D" ), FALSE, FALSE, 0 );
        v->s_D = gtk_spin_button_new_with_range( 0, 2, 1 );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(v->s_D), D );
        gtk_widget_set_tooltip_text( v->s_D, "Diferencias estacionales." );
        gtk_box_pack_start( GTK_BOX(caja), v->s_D, FALSE, FALSE, 0 );
        }

    {
    GtkWidget *l = gtk_label_new( v->modo == 1
        ? "  La serie EN NIVEL: d y D no entran aquí. El dibujo lo hace fug."
        : "  El dibujo lo hace fug: es el mismo que verías en la herramienta "
          "completa." );

    gtk_label_set_ellipsize( GTK_LABEL(l), PANGO_ELLIPSIZE_END );
    gtk_box_pack_start( GTK_BOX(caja), l, TRUE, TRUE, 0 );
    }

    if ( v->modo == 0 )
        {
        GtkWidget *b = gtk_button_new_with_label( "Identificar la estructura ARIMA…" );

        gtk_widget_set_tooltip_text( b,
            "art propone órdenes ARMA para la serie con esta λ, d y D, con su "
            "evidencia: el correlograma teórico de cada candidato sobre éste.\n\n"
            "Propone; no estima. El modelo lo eliges tú." );
        g_signal_connect( b, "clicked", G_CALLBACK(on_identificar), v );
        gtk_box_pack_end( GTK_BOX(caja), b, FALSE, FALSE, 0 );
        }

    g_signal_connect( v->s_lam, "value-changed", G_CALLBACK(on_cambio), v );
    if ( v->s_d ) g_signal_connect( v->s_d, "value-changed", G_CALLBACK(on_cambio), v );
    if ( v->s_D ) g_signal_connect( v->s_D, "value-changed", G_CALLBACK(on_cambio), v );

    v->armando = FALSE;
    return caja;
}

/* IDENTIFICAR, desde el mismo gráfico (E2, docs/ESTUDIO-identificador.md):
 * art propone órdenes para la serie CON LA TRANSFORMACION QUE SE ESTA
 * MIRANDO, y la ventana deja derivar el modelo del nodo de datos. El
 * vistazo sigue sin dejar rastro: lo que se deriva lo decide el analista. */
static void on_identificar( GtkButton *b, Vistazo *v )
{
    InpFile     inp;
    char        msg[256];
    const char *mu = atsw_muestra_actual( v->a );
    const char *datos = pr_datos_de( v->a->p, v->a->serie, mu );
    AnHost      h = atsw_host( v->a );
    AnSerie     s;
    gchar      *que;

    (void) b;
    if ( !*datos ) { barra_pub( v->a, "Esa serie no tiene nodo de datos." ); return; }
    if ( inp_read( v->origen, &inp, msg, sizeof msg ) != 0 )
        { barra_pub( v->a, msg ); return; }
    memset( &s, 0, sizeof s );
    s.x    = inp.data;
    s.n    = inp.nobs;
    s.freq = inp.freq;
    s.per  = inp.begtime;
    s.anio = inp.begyear;
    s.lam  = gtk_spin_button_get_value( GTK_SPIN_BUTTON(v->s_lam) );
    s.d    = v->s_d ? gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(v->s_d) ) : 0;
    s.D    = v->s_D ? gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(v->s_D) ) : 0;
    que = g_strdup_printf( "Gráficos de identificación de %s", v->a->serie );
    s.que  = que;
    an_identifica_serie( &h, v->a->serie, mu, datos, &s );
    g_free( que );
    inp_free( &inp );
}

/* ------------------------------------------------------------------------ */

/* modo 0: serie + acf/pacf, con controles. modo 1: media - desviacion tipica,
 * con la lambda que se pida y sin d ni D. modo 2: histograma, con los mismos
 * controles que el 0.                                                   */
void atsw_vistazo( Atsw *a, const char *inp, int modo, double lam )
{
    gchar *dir;
    InpFile f;
    char    msg[256];
    int     d = 0, D = 0;

    if ( inp == NULL || !*inp )
        { barra_pub( a, "Marca una serie." ); return; }

    /* Lo que quedara del vistazo anterior: la espera de un mando y un pie
       que no llego a ponerse. Su corrida, si la hay, la para pide().  */
    if ( vz_reloj ) { g_source_remove( vz_reloj ); vz_reloj = 0; }
    sin_pie( &V );
    memset( &V, 0, sizeof V );
    V.a = a;
    V.modo = modo;
    snprintf( V.serie, sizeof V.serie, "%s", a->serie );
    snprintf( V.muestra, sizeof V.muestra, "%s", atsw_muestra_actual( a ) );
    snprintf( V.origen, sizeof V.origen, "%s", inp );

    dir = atsw_cache_dir();
    {
    gchar *b = g_build_filename( dir, "vistazo", NULL );

    snprintf( V.base, sizeof V.base, "%s", b );
    g_free( b );
    }
    g_free( dir );

    /* La transformacion de partida sale del .inp -- que es donde vive. */
    if ( inp_read( inp, &f, msg, sizeof msg ) == 0 )
        {
        if ( modo != 1 ) lam = f.boxlam;
        d = f.nrdiff;
        D = f.nadiff;
        inp_free( &f );
        }

    /* El pie se arma antes de dibujar para que lanza_fug() lea sus
       valores; se pone en la ventana cuando llegue el dibujo (vz_acabo). */
    V.pie = pie_nuevo( &V, lam, d, D );
    pide( &V );
}


/* ------------------------------------------------------------------------ */
/* «Al cuaderno» en la ventana de graficos (lib/preview)                     */
/* ------------------------------------------------------------------------ */

gboolean preview_cuaderno_puede( PreviewApp *app )
{
    return app && app->hay && app->p && app->p->path[0];
}

/* DE QUIEN ES UN DIBUJO SE LE PREGUNTA AL MANIFIESTO, como en pr_de_ruta: el
 * resumen de un modelo es <base>.pdf y su grafico A<base>.eps, asi que se
 * prueba con el nombre y con el nombre sin la «A». 0 si es de un modelo.  */
static int de_modelo( Atsw *a, const char *f, char *serie, char *muestra,
                      char *id )
{
    char  sin[PR_RUTA], out[PR_RUTA];
    gchar *dir, *nom;
    int   rc = 1;

    if ( ruta_sin_ext( f, sin, sizeof sin ) != 0 ) return 1;
    snprintf( out, sizeof out, "%s.out", sin );
    if ( pr_de_ruta( a->p, out, serie, PR_ID, muestra, PR_ID, id, PR_ID ) == 0 )
        return 0;
    dir = g_path_get_dirname( sin );
    nom = g_path_get_basename( sin );
    if ( nom[0] == 'A' && nom[1] )
        {
        gchar *o = g_strdup_printf( "%s%c%s.out", dir, G_DIR_SEPARATOR, nom + 1 );

        rc = pr_de_ruta( a->p, o, serie, PR_ID, muestra, PR_ID, id, PR_ID );
        g_free( o );
        }
    g_free( dir ); g_free( nom );
    return rc;
}

void preview_cuaderno( PreviewApp *a, const gchar *path )
{
    AnHost h = atsw_host( a );
    char   serie[PR_ID] = "", muestra[PR_ID] = "", id[PR_ID] = "";
    char   caso[PR_ID], corrida[PR_ID];

    if ( V.eps[0] && !strcmp( path, V.eps ) )
        {
        /* EL VISTAZO: lo que dice el dibujo depende de la transformacion
           con la que se mira, y esa no esta en ningun fichero.        */
        /* La del dibujo que se ve, no la de los mandos: con el dibujo
           nuevo en camino pueden no ser la misma.                     */
        gchar *t = V.s_d
            ? g_strdup_printf( "λ = %.2f, d = %d, D = %d",
                               V.lam_h, V.d_h, V.D_h )
            : g_strdup_printf( "λ = %.2f (la serie en nivel)", V.lam_h );
        const char *datos[] = { "Transformación", t, NULL };

        an_cuaderno_grafico( &h, path, V.modo == 1 ? "Media – desviación típica"
                                     : V.modo == 2 ? "Histograma"
                                                   : "Serie y ACF/PACF",
                             V.serie, V.muestra, NULL, datos );
        g_free( t );
        }
    else if ( de_modelo( a, path, serie, muestra, id ) == 0 )
        an_cuaderno_grafico( &h, path, "Gráfico", serie, muestra, id, NULL );
    else if ( pr_caso_de_ruta( a->p, path, caso, sizeof caso,
                               corrida, sizeof corrida ) == 0 )
        {
        const char *datos[] = { "Caso", caso, "Corrida", corrida, NULL };
        gchar      *que = g_strdup_printf( "Gráfico — caso %s / %s", caso, corrida );

        an_cuaderno_grafico( &h, path, que, NULL, NULL, NULL, datos );
        g_free( que );
        }
    else
        an_cuaderno_grafico( &h, path, "Gráfico", NULL, NULL, NULL, NULL );
}
