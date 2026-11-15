/* trabajos.c -- LOS TRABAJOS APARTE, EN LA BARRA DE ESTADO.
 *
 * Lo que corre suelto y tarda --el bootstrap del rango, que el asistente lanza
 * con atsw-cli caso-rango --aparte o el analista en sim_gui-- deja un
 * .trabajo junto a sus ficheros (lib/trabajo). La madre los mira cada dos
 * segundos en el proyecto abierto y enseña el primero en una barra de avance
 * a la derecha de la barra de estado: «C1/rango · bootstrap del rango · 24/57
 * · quedan ~27 s · asistente». Con varios, el globo los lista todos. Un clic
 * abre el caso en sim_gui. Cuando uno termina o se para, lo dice una vez en la
 * barra de estado.
 *
 * Lo pidio el analista (15-nov-2026): «asi el analista sabe lo que esta
 * pasando». Mirar no cuesta: leer un fichero corto y contar las lineas del
 * .boot. La madre no sabe nada de drvec: todo lo dice el .trabajo.
 */

#include <string.h>
#include <time.h>

#include "atsw.h"
#include "trabajo.h"

#define TR_MAX 16

void barra_pub( Atsw *a, const char *s );     /* main.c, como en anfitrion.c */

static gchar *raiz_de( Atsw *a )
{
    return ( a->hay && a->p && a->p->path[0] ) ? g_path_get_dirname( a->p->path ) : NULL;
}

/* el caso de un objeto «C1/rango» (g_free), o NULL */
static gchar *caso_de( const char *objeto )
{
    const char *bar = strchr( objeto, '/' );

    return bar && bar > objeto ? g_strndup( objeto, (gsize) ( bar - objeto ) ) : NULL;
}

static gboolean mira( gpointer data )
{
    Atsw   *a = data;
    gchar  *raiz = raiz_de( a );
    char    (*rutas)[TR_RUTA] = NULL;
    int     n = 0, i, primero = -1;
    GString *globo = g_string_new( "" );
    char     lin[512], elegida[512] = "";
    double   frac = -1.0, f;
    long     ahora = (long) time( NULL );

    if ( !a->trab_barra ) { g_free( raiz ); g_string_free( globo, TRUE ); return G_SOURCE_REMOVE; }
    if ( raiz )
        {
        rutas = g_malloc0( sizeof( char[TR_RUTA] ) * TR_MAX );
        n = trabajo_lista( raiz, rutas, TR_MAX );
        }
    for ( i = 0; i < n; i++ )
        {
        Trabajo  t;
        TrEstado e;
        gpointer antes;

        if ( trabajo_lee( rutas[i], &t ) != 0 ) continue;
        e = trabajo_estado( rutas[i], &t );
        trabajo_linea( rutas[i], &t, ahora, lin, sizeof lin, &f );
        g_string_append_printf( globo, "%s%s", globo->len ? "\n" : "", lin );
        /* EL AVISO, UNA VEZ: lo que estaba en marcha y ya no */
        antes = g_hash_table_lookup( a->trab_visto, rutas[i] );
        if ( antes && GPOINTER_TO_INT( antes ) - 1 == TR_EN_MARCHA && e != TR_EN_MARCHA )
            {
            gchar *m = e == TR_TERMINADO
                ? g_strdup_printf( "%s: %s, terminado. Ábrelo en sim_gui, o pídele el resultado al asistente.",
                                   t.objeto, t.que )
                : g_strdup_printf( "%s: %s, se paró sin terminar (¿se cerró o falló?). Vuelve a lanzarlo.",
                                   t.objeto, t.que );
            barra_pub( a, m );
            g_free( m );
            }
        g_hash_table_replace( a->trab_visto, g_strdup( rutas[i] ), GINT_TO_POINTER( e + 1 ) );
        if ( primero < 0 || ( e == TR_EN_MARCHA && frac < 0 ) )
            {
            if ( e == TR_EN_MARCHA || primero < 0 )
                { primero = i; g_strlcpy( elegida, lin, sizeof elegida ); frac = f;
                  g_free( a->trab_caso ); a->trab_caso = caso_de( t.objeto ); }
            }
        }
    if ( primero < 0 )
        gtk_widget_hide( a->trab_barra );
    else
        {
        GtkProgressBar *pb = GTK_PROGRESS_BAR( gtk_bin_get_child( GTK_BIN( a->trab_barra ) ) );
        gchar *txt = n > 1 ? g_strdup_printf( "%s  (y %d más)", elegida, n - 1 ) : g_strdup( elegida );
        gchar *gl = g_strdup_printf( "%s\n\nClic: el caso en sim_gui.", globo->str );

        if ( frac >= 0 ) gtk_progress_bar_set_fraction( pb, frac > 1 ? 1 : frac );
        else gtk_progress_bar_pulse( pb );
        gtk_progress_bar_set_text( pb, txt );
        gtk_widget_set_tooltip_text( a->trab_barra, gl );
        gtk_widget_show( GTK_WIDGET( pb ) );        /* no_show_all: a mano */
        gtk_widget_show( a->trab_barra );
        g_free( txt ); g_free( gl );
        }
    g_free( rutas ); g_free( raiz ); g_string_free( globo, TRUE );
    return G_SOURCE_CONTINUE;
}

static void on_clic( GtkButton *b, Atsw *a )
{
    (void) b;
    if ( a->trab_caso && a->hay )
        {
        gchar *que = g_strdup_printf( "El caso %s en sim_gui, con su trabajo aparte", a->trab_caso );
        atsw_lanza_caso( a, "sim_gui", a->trab_caso, NULL, que );
        g_free( que );
        }
}

GtkWidget *atsw_trabajos_widget( Atsw *a )
{
    GtkWidget *pb = gtk_progress_bar_new();

    gtk_progress_bar_set_show_text( GTK_PROGRESS_BAR( pb ), TRUE );
    gtk_progress_bar_set_ellipsize( GTK_PROGRESS_BAR( pb ), PANGO_ELLIPSIZE_END );
    gtk_widget_set_size_request( pb, 420, -1 );
    a->trab_barra = gtk_button_new();
    gtk_button_set_relief( GTK_BUTTON( a->trab_barra ), GTK_RELIEF_NONE );
    gtk_container_add( GTK_CONTAINER( a->trab_barra ), pb );
    gtk_widget_set_no_show_all( a->trab_barra, TRUE );
    g_signal_connect( a->trab_barra, "clicked", G_CALLBACK( on_clic ), a );
    a->trab_visto = g_hash_table_new_full( g_str_hash, g_str_equal, g_free, NULL );
    a->trab_tic = g_timeout_add_seconds( 2, mira, a );
    return a->trab_barra;
}

void atsw_trabajos_mira( Atsw *a )
{
    if ( a->trab_barra ) mira( a );
}
