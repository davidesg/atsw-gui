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
 * DOS GESTOS, Y SON DISTINTOS:
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
 * El .inp del vistazo es TEMPORAL y vive en la cache: no se toca el del
 * proyecto. Un vistazo no es una decision, y no deja rastro en el manifiesto.
 */

#include <string.h>
#include <stdlib.h>

#include <glib/gstdio.h>

#include "atsw.h"
#include "preview.h"
#include "inpfile.h"
#include "sitio.h"

void barra_pub( Atsw *a, const char *s );

typedef struct {
    Atsw      *a;
    char       origen[PR_RUTA];     /* el .inp de la serie                 */
    char       base[PR_RUTA];       /* <cache>/vistazo                     */
    char       eps[PR_RUTA];        /* el que se enseña                    */
    GtkWidget *s_lam, *s_d, *s_D;
    gboolean   armando;             /* poniendo valores: no redibujar      */
    int        modo;                /* 0 serie+acf/pacf, 1 media-dt        */
} Vistazo;

static Vistazo V;

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

/* Llama al motor y devuelve TRUE si el EPS esta. */
static gboolean dibuja( Vistazo *v, char *why, size_t n )
{
    gchar  *dir = atsw_cache_dir();
    gchar  *exe = NULL;
    gchar  *sal = NULL, *err = NULL;
    GError *e = NULL;
    gint    st;
    gboolean ok;
    double  lam = gtk_spin_button_get_value( GTK_SPIN_BUTTON(v->s_lam) );
    /* En media - desviacion tipica no hay d ni D, y no es una omision: ese
     * grafico es de la serie EN NIVEL. Comprobado -- con d=1 D=1 en el .inp,
     * "fug -e" sigue escribiendo m_dt_d0...                            */
    int     d   = v->s_d ? gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(v->s_d) ) : 0;
    int     D   = v->s_D ? gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(v->s_D) ) : 0;
    gchar  *argv[6];
    char    inp[PR_RUTA + 8];

    snprintf( inp, sizeof inp, "%s.inp", v->base );
    if ( escribe_inp( v->origen, inp, lam, d, D, why, n ) != 0 )
        { g_free( dir ); return FALSE; }

    /* El motor: el mismo que usa fug_gui. Se busca al lado antes que en el
     * PATH, por lo mismo que en gestos.c.                              */
    exe = g_build_filename( dir, "..", NULL );
    g_free( exe );
    {
    static const char *const sitio[] = { "../../engines/fug/%s", NULL };

    exe = sitio_busca( "fug", sitio );
    }
    if ( exe == NULL )
        { snprintf( why, n, "No encuentro el motor fug." ); g_free( dir );
          return FALSE; }

    argv[0] = exe;
    argv[1] = v->base;
    argv[2] = (gchar *) ( v->modo ? "-e" : "-c" );
    argv[3] = NULL;

    ok = g_spawn_sync( dir, argv, NULL, 0, NULL, NULL, &sal, &err, &st, &e );
    if ( ok && st != 0 ) ok = FALSE;

    if ( !ok )
        snprintf( why, n, "fug: %s", ( err && *err ) ? err
                  : ( e ? e->message : "no pudo dibujar" ) );

    if ( e ) g_error_free( e );
    g_free( sal ); g_free( err ); g_free( exe ); g_free( dir );
    return ok;
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

static void repinta( Vistazo *v )
{
    char why[512], eps[PR_RUTA];

    if ( v->armando ) return;

    {
    gchar *dir = atsw_cache_dir();
    gchar *sal;

    limpia_eps( dir );
    if ( !dibuja( v, why, sizeof why ) )
        { barra_pub( v->a, why ); g_free( dir ); return; }

    sal = busca_eps( dir );
    g_free( dir );
    if ( sal == NULL )
        {
        barra_pub( v->a, "El motor no dejó ningún gráfico. ¿Esa "
                         "transformación tiene sentido para esta serie?" );
        return;
        }
    snprintf( eps, sizeof eps, "%s", sal );
    g_free( sal );
    }

    /* La MISMA ruta siempre que se pueda: preview reutiliza la ventana y la
     * recarga, asi que el pie no parpadea. Como el nombre lo pone la
     * transformacion, se copia al fijo.                                */
    {
    gchar *d2 = atsw_cache_dir();
    gchar *fijo = g_build_filename( d2, "vistazo.eps", NULL );
    gchar *c = NULL;
    gsize  ln = 0;

    if ( g_file_get_contents( eps, &c, &ln, NULL ) )
        { g_file_set_contents( fijo, c, (gssize) ln, NULL ); g_free( c ); }
    snprintf( v->eps, sizeof v->eps, "%s", fijo );
    g_free( fijo ); g_free( d2 );
    }

    preview_show( (PreviewApp *) v->a, v->eps );
    barra_pub( v->a, "" );
}

static void on_cambio( GtkWidget *w, Vistazo *v ) { (void) w; repinta( v ); }

/* ------------------------------------------------------------------------ */

/* EL PIE, Y LO QUE LLEVA DEPENDE DEL GRAFICO.
 *
 *   serie + acf/pacf   lambda, d y D: la pregunta es cuantas diferencias.
 *   media - desv. tip. SOLO lambda: ese grafico es de la serie EN NIVEL, asi
 *                      que d y D no le dicen nada. Poner unos mandos que no
 *                      hacen nada seria peor que no ponerlos.
 *
 * Lo que los dos comparten es lo que importa: la transformacion se toca AL
 * PIE DEL DIBUJO y el dibujo se rehace ahi mismo.                       */
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

    if ( v->modo == 0 )
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
    GtkWidget *l = gtk_label_new( v->modo
        ? "  La serie EN NIVEL: d y D no entran aquí. El dibujo lo hace fug."
        : "  El dibujo lo hace fug: es el mismo que verías en la herramienta "
          "completa." );

    gtk_label_set_ellipsize( GTK_LABEL(l), PANGO_ELLIPSIZE_END );
    gtk_box_pack_start( GTK_BOX(caja), l, TRUE, TRUE, 0 );
    }

    g_signal_connect( v->s_lam, "value-changed", G_CALLBACK(on_cambio), v );
    if ( v->s_d ) g_signal_connect( v->s_d, "value-changed", G_CALLBACK(on_cambio), v );
    if ( v->s_D ) g_signal_connect( v->s_D, "value-changed", G_CALLBACK(on_cambio), v );

    v->armando = FALSE;
    return caja;
}

/* ------------------------------------------------------------------------ */

/* modo 0: serie + acf/pacf, con controles. modo 1: media - desviacion tipica,
 * con la lambda que se pida y sin controles.                            */
void atsw_vistazo( Atsw *a, const char *inp, int modo, double lam )
{
    gchar *dir;
    char   why[512];
    InpFile f;
    char    msg[256];
    int     d = 0, D = 0;

    if ( inp == NULL || !*inp )
        { barra_pub( a, "Marca una serie." ); return; }

    memset( &V, 0, sizeof V );
    V.a = a;
    V.modo = modo;
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
        if ( modo == 0 ) lam = f.boxlam;
        d = f.nrdiff;
        D = f.nadiff;
        inp_free( &f );
        }

    /* El pie se arma antes de dibujar para que dibuja() lea sus valores. */
    {
    GtkWidget *pie = pie_nuevo( &V, lam, d, D );

    repinta( &V );
    /* El pie va en LOS DOS graficos: el de media - desviacion tipica tambien
     * deja tocar lambda, que es justo la pregunta que contesta.        */
    if ( !preview_set_footer( V.eps, pie ) )
        gtk_widget_destroy( pie );
    }
    (void) why;
}
