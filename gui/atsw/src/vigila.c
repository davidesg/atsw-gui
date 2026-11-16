/*
 * vigila.c -- ver lo que hace el asistente, en vivo
 * (docs/ESTUDIO-terminal-mcp.md §7.3).
 *
 * La madre ya relee al recuperar el foco (main.c, on_foco). Con el asistente
 * en una terminal al lado, el analista MIRA la madre sin pulsar en ella, y el
 * foco no vuelve nunca: el modelo que el asistente acaba de derivar no
 * aparecia hasta tocar la ventana.
 *
 * Asi que se vigilan los ficheros: el directorio del proyecto (donde esta
 * proyecto.yaml) y las carpetas de los modelos (sus .out y .pre). GLib lo
 * hace en los tres sistemas (inotify, ReadDirectoryChangesW, kqueue/FSEvents)
 * y un GFileMonitor no es recursivo, por eso una carpeta por cada sitio con
 * modelos; al releer el proyecto se rehace la lista, que puede haber
 * carpetas nuevas.
 *
 * LOS AVISOS SE AGRUPAN, medio segundo: fue escribe el .out, luego el .pre y
 * luego el manifiesto, y releer tres veces seguidas es trabajo que nadie mira.
 * Y se relee con LA MISMA FUNCION que el foco (atsw_relee y atsw_refresca),
 * con sus dos reglas: no se pisa lo que funciona y no se avisa dos veces de lo
 * mismo. Lo que escribe la propia madre tambien dispara el aviso, y no cuesta
 * nada: la huella del manifiesto es la de lo que acaba de escribir.
 *
 * LO NUEVO DEL ASISTENTE SE DICE en la barra de estado --«m04 de IPC, del
 * asistente»--, porque es lo que el analista viene a ver. Lo que hace a mano
 * otro programa (otra madre, un editor) se dice como en el foco.
 */

#include <string.h>

#include "atsw.h"

void barra_pub( Atsw *a, const char *s );

#define VIGIA_MS 500

typedef struct {
    Atsw      *a;
    GPtrArray *mon;     /* los GFileMonitor de ahora                      */
    guint      t;       /* el aviso agrupado pendiente, o 0               */
} Vigia;

static Vigia V;

static void rehace( Atsw *a );

/* Lo que interesa: el manifiesto, y los .out y .pre de los modelos. Una
 * carpeta nueva tambien, que puede ser la de una serie o muestra nueva. */
static gboolean interesa( GFile *f, GFileMonitorEvent ev )
{
    gchar   *b;
    gboolean si;

    if ( ev == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED ||
         ev == G_FILE_MONITOR_EVENT_PRE_UNMOUNT ||
         ev == G_FILE_MONITOR_EVENT_UNMOUNTED ) return FALSE;
    if ( f == NULL ) return FALSE;
    b = g_file_get_basename( f );
    si = b && ( !strcmp( b, "proyecto.yaml" ) || g_str_has_suffix( b, ".out" ) ||
                g_str_has_suffix( b, ".pre" ) || ev == G_FILE_MONITOR_EVENT_CREATED );
    g_free( b );
    return si;
}

/* «m04 de IPC», con la muestra si no es la completa. */
static void nombre( const PrModelo *m, char *s, size_t n )
{
    if ( m->muestra[0] ) g_snprintf( s, n, "%s de %s (%s)", m->id, m->serie, m->muestra );
    else                 g_snprintf( s, n, "%s de %s", m->id, m->serie );
}

static gchar *clave( const PrModelo *m )
{
    return g_strdup_printf( "%s\n%s\n%s", m->serie, m->muestra, m->id );
}

/* Lo que el asistente ha hecho entre ANTES (las claves de los modelos que
 * habia) y el proyecto de ahora: los modelos suyos que no estaban, y las
 * elecciones suyas nuevas. NULL si nada.                               */
static gchar *lo_del_asistente( const Proyecto *p, GHashTable *antes,
                                GHashTable *eleg_antes )
{
    GString *t = g_string_new( NULL );
    int      i, nuevos = 0, elegidos = 0;
    char     uno[PR_ID * 3 + 16] = "", otro[PR_ID * 3 + 16] = "";

    for ( i = 0; i < p->nm; i++ )
        {
        const PrModelo *m = &p->m[i];
        gchar          *k = clave( m );

        if ( !strcmp( m->por, "asistente" ) && !g_hash_table_contains( antes, k ) )
            { if ( !nuevos++ ) nombre( m, uno, sizeof uno ); }
        if ( m->elegido && !strcmp( m->por_elegido, "asistente" ) &&
             !g_hash_table_contains( eleg_antes, k ) )
            { if ( !elegidos++ ) nombre( m, otro, sizeof otro ); }
        g_free( k );
        }

    if ( nuevos == 1 )
        g_string_append_printf( t, "%s, del asistente", uno );
    else if ( nuevos > 1 )
        g_string_append_printf( t, "%s y %d más, del asistente", uno, nuevos - 1 );
    if ( elegidos )
        g_string_append_printf( t, "%s%s%s elegido por el asistente", t->len ? "; " : "",
                                otro, elegidos > 1 ? " (y otros)" : "" );
    if ( t->len == 0 ) { g_string_free( t, TRUE ); return NULL; }
    g_string_append( t, "." );
    return g_string_free( t, FALSE );
}

static gboolean al_rato( gpointer d )
{
    Atsw       *a = d;
    GHashTable *antes, *eleg;
    char        why[512];
    int         i;

    V.t = 0;
    if ( !a->hay || a->p == NULL || a->solo_ver ) return G_SOURCE_REMOVE;

    antes = g_hash_table_new_full( g_str_hash, g_str_equal, g_free, NULL );
    eleg  = g_hash_table_new_full( g_str_hash, g_str_equal, g_free, NULL );
    for ( i = 0; i < a->p->nm; i++ )
        {
        g_hash_table_add( antes, clave( &a->p->m[i] ) );
        if ( a->p->m[i].elegido ) g_hash_table_add( eleg, clave( &a->p->m[i] ) );
        }

    if ( atsw_relee( a, why, sizeof why ) )
        {
        gchar *t = lo_del_asistente( a->p, antes, eleg );

        barra_pub( a, t ? t : "El proyecto ha cambiado fuera de aquí: releído." );
        g_free( t );
        rehace( a );
        }
    else if ( why[0] )
        barra_pub( a, why );

    /* Los .out se releen aqui, por su huella: si no se movieron, nada. */
    atsw_refresca( a );
    /* Y lo ultimo del asistente, en su ventana (§7.6). */
    atsw_asistente_refresca( a );
    g_hash_table_destroy( antes );
    g_hash_table_destroy( eleg );
    return G_SOURCE_REMOVE;
}

static void on_cambio( GFileMonitor *mo, GFile *f, GFile *otro,
                       GFileMonitorEvent ev, gpointer d )
{
    (void) mo; (void) otro;
    if ( !interesa( f, ev ) ) return;
    if ( V.t ) g_source_remove( V.t );
    V.t = g_timeout_add( VIGIA_MS, al_rato, d );
}

static void vigila_dir( Atsw *a, GHashTable *ya, const char *dir )
{
    GFile        *f;
    GFileMonitor *mo;

    if ( !dir || !*dir || g_hash_table_contains( ya, dir ) ) return;
    if ( !g_file_test( dir, G_FILE_TEST_IS_DIR ) ) return;
    g_hash_table_add( ya, g_strdup( dir ) );
    f  = g_file_new_for_path( dir );
    mo = g_file_monitor_directory( f, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL );
    g_object_unref( f );
    if ( mo == NULL ) return;        /* sin vigia sigue el foco: no es un error */
    g_signal_connect( mo, "changed", G_CALLBACK(on_cambio), a );
    g_ptr_array_add( V.mon, mo );
}

void atsw_vigila_para( void )
{
    if ( V.t ) { g_source_remove( V.t ); V.t = 0; }
    if ( V.mon ) { g_ptr_array_free( V.mon, TRUE ); V.mon = NULL; }
}

static void rehace( Atsw *a )
{
    GHashTable *ya;
    char        raiz[PR_RUTA], f[PR_RUTA];
    int         i;

    atsw_vigila_para();
    if ( !a->hay || a->p == NULL || a->solo_ver ) return;
    V.a   = a;
    V.mon = g_ptr_array_new_with_free_func( g_object_unref );
    ya    = g_hash_table_new_full( g_str_hash, g_str_equal, g_free, NULL );

    if ( pr_raiz( a->p, raiz, sizeof raiz ) == 0 ) vigila_dir( a, ya, raiz );
    for ( i = 0; i < a->p->nm; i++ )
        {
        const PrModelo *m = &a->p->m[i];

        if ( pr_ruta( a->p, m->serie, m->muestra, m->id, ".out", f, sizeof f ) == 0 )
            {
            gchar *d = g_path_get_dirname( f );
            vigila_dir( a, ya, d );
            g_free( d );
            }
        }
    g_hash_table_destroy( ya );
}

void atsw_vigila( Atsw *a ) { rehace( a ); }
