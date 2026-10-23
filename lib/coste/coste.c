/* coste.c -- lo que cuesta cada corrida de un motor. Ver coste.h. */

#include "coste.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#  include <windows.h>
#  include <io.h>
#else
#  include <unistd.h>
#  include <sys/file.h>
#  include <sys/resource.h>
#endif

#ifndef O_BINARY
#  define O_BINARY 0
#endif

#define COSTE_TDP_W   15.0     /* un portatil: el defecto de §7.7          */
#define COSTE_CAMPOS  12

static const char *const cabecera =
    "fecha,quien,por,objeto,paso,motor,via,cpu_s,pared_s,energia_j,metodo,sha_pre\n";


/* ------------------------------------------------------------------------ */
/* El contexto                                                               */
/* ------------------------------------------------------------------------ */

/* UNO POR PROCESO, Y CON CERROJO. La madre lanza desde el hilo de la
 * interfaz, pero atsw-cli o una bateria pueden hacerlo desde otro: el
 * contexto se copia entero bajo el cerrojo, y nadie ve uno a medias.    */
typedef struct {
    char    *raiz, *quien, *por, *objeto, *paso;
    gboolean retiene;           /* la linea no se escribe: se guarda        */
} Contexto;

static Contexto g_ctx;
G_LOCK_DEFINE_STATIC( g_ctx );

/* LA LINEA RETENIDA (coste_contexto_retiene). Una por proceso. */
static CosteLinea *g_retenida;
G_LOCK_DEFINE_STATIC( g_retenida );

static void ctx_libera( Contexto *c )
{
    g_free( c->raiz ); g_free( c->quien ); g_free( c->por );
    g_free( c->objeto ); g_free( c->paso );
    memset( c, 0, sizeof *c );
}

void coste_contexto( const char *raiz, const char *quien, const char *por,
                     const char *objeto, const char *paso )
{
    G_LOCK( g_ctx );
    ctx_libera( &g_ctx );
    if ( raiz && *raiz )
        {
        g_ctx.raiz   = g_strdup( raiz );
        g_ctx.quien  = g_strdup( quien && *quien ? quien : "madre" );
        g_ctx.por    = g_strdup( por && *por ? por : "analista" );
        g_ctx.objeto = g_strdup( objeto ? objeto : "" );
        g_ctx.paso   = g_strdup( paso ? paso : "" );
        }
    G_UNLOCK( g_ctx );
}

void coste_contexto_modelo( const char *raiz, const char *serie,
                            const char *muestra, const char *id,
                            const char *paso )
{
    gchar *obj;

    if ( !serie || !*serie )   obj = g_strdup( "" );
    else if ( !id || !*id )    obj = g_strdup( serie );
    else if ( muestra && *muestra )
                               obj = g_strdup_printf( "%s/%s/%s", serie, muestra, id );
    else                       obj = g_strdup_printf( "%s/%s", serie, id );
    coste_contexto( raiz, "madre", "analista", obj, paso );
    g_free( obj );
}

void coste_contexto_retiene( void )
{
    G_LOCK( g_ctx );
    if ( g_ctx.raiz ) g_ctx.retiene = TRUE;
    G_UNLOCK( g_ctx );
}

CosteLinea *coste_retenida( void )
{
    CosteLinea *l;

    G_LOCK( g_retenida );
    l = g_retenida;
    g_retenida = NULL;
    G_UNLOCK( g_retenida );
    return l;
}

void coste_contexto_limpia( void )
{
    G_LOCK( g_ctx );
    ctx_libera( &g_ctx );
    G_UNLOCK( g_ctx );
}


/* ------------------------------------------------------------------------ */
/* La energia, que es una ESTIMACION y lo dice                               */
/* ------------------------------------------------------------------------ */

/* NO HAY FORMA PORTATIL DE MEDIRLA POR PROCESO: RAPL mide el paquete entero
 * con los demas procesos dentro, y en muchas distribuciones solo lo lee root;
 * powermetrics pide sudo; Windows no tiene API publica. Por eso se estima con
 * el tiempo de CPU, que si se mide en todas partes, y el metodo va escrito
 * en cada linea. ATSW_TDP_W lo cambia mientras no haya preferencias.    */
double coste_energia( double cpu_s, char *metodo, size_t n )
{
    const char *e = g_getenv( "ATSW_TDP_W" );
    double      tdp = COSTE_TDP_W;
    guint       nucleos = g_get_num_processors();
    char        t[G_ASCII_DTOSTR_BUF_SIZE];

    if ( e && *e )
        {
        char  *fin;
        double v = g_ascii_strtod( e, &fin );

        if ( fin != e && *fin == '\0' && v > 0.0 && isfinite( v ) ) tdp = v;
        }
    if ( nucleos < 1 ) nucleos = 1;
    if ( metodo && n )
        {
        g_ascii_formatd( t, sizeof t, "%g", tdp );
        g_snprintf( metodo, n, "cpu\xc3\x97TDP/n\xc3\xba" "cleos: %s W, %u", t, nucleos );
        }
    return cpu_s > 0.0 ? cpu_s * tdp / (double) nucleos : 0.0;
}


/* ------------------------------------------------------------------------ */
/* Escribir                                                                  */
/* ------------------------------------------------------------------------ */

/* UN CAMPO DE CSV. Entre comillas si lleva coma, comillas o un blanco en
 * los bordes, con las comillas dobladas. Los saltos de linea se cambian por
 * un espacio: UNA CORRIDA, UNA LINEA, que es lo que deja leer el registro
 * con un grep y lo que hace que un escritor a medias no parta a otro.   */
static void campo( GString *s, const char *v, gboolean ultimo )
{
    const char *p;
    gboolean    comillas = FALSE;

    if ( v == NULL ) v = "";
    for ( p = v; *p; p++ )
        if ( *p == ',' || *p == '"' || *p == '\n' || *p == '\r' ) { comillas = TRUE; break; }
    if ( *v && ( v[0] == ' ' || v[strlen( v ) - 1] == ' ' ) ) comillas = TRUE;

    if ( comillas ) g_string_append_c( s, '"' );
    for ( p = v; *p; p++ )
        {
        if ( *p == '"' )                     g_string_append( s, "\"\"" );
        else if ( *p == '\n' || *p == '\r' ) g_string_append_c( s, ' ' );
        else                                 g_string_append_c( s, *p );
        }
    if ( comillas ) g_string_append_c( s, '"' );
    g_string_append_c( s, ultimo ? '\n' : ',' );
}

/* LOS NUMEROS CON PUNTO, siempre: con el español, printf escribe «0,21»,
 * y en un CSV esa coma parte el campo en dos.                          */
static void numero( GString *s, double v )
{
    char t[G_ASCII_DTOSTR_BUF_SIZE];

    g_ascii_formatd( t, sizeof t, "%.3f", v );
    g_string_append( s, t );
    g_string_append_c( s, ',' );
}

/* EL CERROJO DEL FICHERO, y la linea de UNA SOLA ESCRITURA. La madre y el
 * MTM (por atsw-cli) pueden apuntar a la vez en el mismo proyecto. Con el
 * cerrojo, ademas, la cabecera sale una vez: se mira si el fichero esta
 * vacio CON EL CERROJO PUESTO, y la cabecera va en la misma escritura que
 * la primera linea.                                                     */
static int escribe( const char *ruta, const char *linea, char *why, size_t n )
{
    int         fd, rc = 0;
    gint64      fin;
    GString    *todo;
    gssize      w;

    fd = g_open( ruta, O_WRONLY | O_CREAT | O_APPEND | O_BINARY, 0644 );
    if ( fd < 0 )
        {
        g_snprintf( why, n, "no pude abrir %s: %s", ruta, g_strerror( errno ) );
        return 1;
        }

#ifdef G_OS_WIN32
    {
    HANDLE     h = (HANDLE) _get_osfhandle( fd );
    OVERLAPPED ov;

    memset( &ov, 0, sizeof ov );
    if ( !LockFileEx( h, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &ov ) )
        {
        g_snprintf( why, n, "no pude poner el cerrojo a %s", ruta );
        close( fd );
        return 1;
        }
    fin = _lseeki64( fd, 0, SEEK_END );
#else
    while ( flock( fd, LOCK_EX ) != 0 )
        if ( errno != EINTR )
            {
            g_snprintf( why, n, "no pude poner el cerrojo a %s: %s", ruta,
                        g_strerror( errno ) );
            close( fd );
            return 1;
            }
    fin = (gint64) lseek( fd, 0, SEEK_END );
#endif

    todo = g_string_new( fin == 0 ? cabecera : "" );
    g_string_append( todo, linea );
    w = write( fd, todo->str, (unsigned) todo->len );
    if ( w < 0 || (gsize) w != todo->len )
        {
        g_snprintf( why, n, "no pude escribir en %s: %s", ruta,
                    w < 0 ? g_strerror( errno ) : "escritura incompleta" );
        rc = 1;
        }
    g_string_free( todo, TRUE );

#ifdef G_OS_WIN32
    UnlockFileEx( h, 0, MAXDWORD, MAXDWORD, &ov );
    }
#else
    flock( fd, LOCK_UN );
#endif
    close( fd );
    return rc;
}

static gchar *ahora( void )
{
    GDateTime *t = g_date_time_new_now_local();
    gchar     *s = g_date_time_format( t, "%Y-%m-%dT%H:%M:%S" );

    g_date_time_unref( t );
    return s;
}

int coste_apunta( const char *raiz, const CosteLinea *l, char *why, size_t n )
{
    char     met[128], dummy[8];
    double   e;
    gchar   *fecha = NULL, *ruta;
    GString *s;
    int      rc;

    if ( why == NULL || n == 0 ) { why = dummy; n = sizeof dummy; }
    why[0] = '\0';
    if ( raiz == NULL || *raiz == '\0' || l == NULL )
        { g_snprintf( why, n, "falta la raiz del proyecto" ); return 1; }
    if ( !g_file_test( raiz, G_FILE_TEST_IS_DIR ) )
        { g_snprintf( why, n, "%s no es un directorio", raiz ); return 1; }
    if ( !isfinite( l->cpu_s ) || !isfinite( l->pared_s ) ||
         l->cpu_s < 0.0 || l->pared_s < 0.0 )
        { g_snprintf( why, n, "los tiempos tienen que ser numeros >= 0" ); return 1; }

    /* UNA ENERGIA SIN SU METODO NO SE ESCRIBE. Sin energia y sin metodo, se
       estima con el de siempre; con energia, el metodo es obligatorio.   */
    if ( l->energia_j >= 0.0 )
        {
        if ( l->metodo == NULL || *l->metodo == '\0' )
            { g_snprintf( why, n, "una energia sin su metodo no se apunta" ); return 1; }
        e = l->energia_j;
        g_snprintf( met, sizeof met, "%s", l->metodo );
        }
    else if ( l->metodo == NULL || *l->metodo == '\0' )
        e = coste_energia( l->cpu_s, met, sizeof met );
    else
        { g_snprintf( why, n, "un metodo sin energia no dice nada" ); return 1; }

    if ( l->fecha == NULL || *l->fecha == '\0' ) fecha = ahora();

    s = g_string_new( NULL );
    campo( s, fecha ? fecha : l->fecha, FALSE );
    campo( s, l->quien, FALSE );
    campo( s, l->por, FALSE );
    campo( s, l->objeto, FALSE );
    campo( s, l->paso, FALSE );
    campo( s, l->motor, FALSE );
    campo( s, l->via, FALSE );
    numero( s, l->cpu_s );
    numero( s, l->pared_s );
    numero( s, e );
    campo( s, met, FALSE );
    campo( s, l->sha_pre, TRUE );

    ruta = g_build_filename( raiz, COSTE_FICHERO, NULL );
    rc = escribe( ruta, s->str, why, n );
    g_free( ruta );
    g_string_free( s, TRUE );
    g_free( fecha );
    return rc;
}


/* ------------------------------------------------------------------------ */
/* Medir una corrida                                                         */
/* ------------------------------------------------------------------------ */

struct CosteCorrida {
    Contexto ctx;               /* la copia de cuando se lanzo              */
    char    *programa;          /* el nombre, sin ruta ni .exe              */
    char    *pre;               /* el .pre que dejaria, o NULL              */
    gint64   t0;                /* g_get_monotonic_time al lanzar           */
    time_t   lanzado;           /* para no tomar un .pre de antes           */
    double   cpu0;              /* RUSAGE_CHILDREN al lanzar (Unix)         */
    double   cpu_s, pared_s;
    gboolean medida;
};

#ifndef G_OS_WIN32
static double cpu_hijos( void )
{
    struct rusage r;

    if ( getrusage( RUSAGE_CHILDREN, &r ) != 0 ) return 0.0;
    return (double) r.ru_utime.tv_sec + r.ru_utime.tv_usec / 1e6 +
           (double) r.ru_stime.tv_sec + r.ru_stime.tv_usec / 1e6;
}
#endif

/* EL NOMBRE DEL MOTOR, no su ruta: la madre lanza «/home/.../engines/fue/
 * bin/fue» y en el registro tiene que poner «fue».                      */
static char *nombre_programa( const char *programa )
{
    char *b = g_path_get_basename( programa ? programa : "?" );
    size_t l = strlen( b );

    if ( l > 4 && g_ascii_strcasecmp( b + l - 4, ".exe" ) == 0 ) b[l - 4] = '\0';
    return b;
}

/* EL .pre QUE LA CORRIDA DEJARIA. fue y fuf se llaman con la base del
 * modelo («fue IPC_m04», «fue IPC_m04 -f»), y fue escribe <base>.pre en el
 * directorio de trabajo. Se toma el primer argumento que no es una opcion,
 * sin su .inp; si el fichero no aparece --fuf, art--, no hay .pre.       */
static char *pre_de( const char *workdir, const char *const *argv )
{
    int    i;
    gchar *base, *pre;

    for ( i = 0; argv && argv[i]; i++ )
        if ( argv[i][0] != '-' ) break;
    if ( !argv || !argv[i] || !*argv[i] ) return NULL;

    base = g_strdup( argv[i] );
    if ( g_str_has_suffix( base, ".inp" ) || g_str_has_suffix( base, ".pre" ) )
        base[strlen( base ) - 4] = '\0';
    pre = g_strconcat( base, ".pre", NULL );
    g_free( base );
    if ( !g_path_is_absolute( pre ) && workdir && *workdir )
        {
        gchar *t = g_build_filename( workdir, pre, NULL );

        g_free( pre );
        pre = t;
        }
    return pre;
}

CosteCorrida *coste_empieza( const char *workdir, const char *programa,
                             const char *const *argv )
{
    CosteCorrida *c;

    G_LOCK( g_ctx );
    if ( g_ctx.raiz == NULL )
        { G_UNLOCK( g_ctx ); return NULL; }
    c = g_new0( CosteCorrida, 1 );
    c->ctx.raiz   = g_strdup( g_ctx.raiz );
    c->ctx.quien  = g_strdup( g_ctx.quien );
    c->ctx.por    = g_strdup( g_ctx.por );
    c->ctx.objeto = g_strdup( g_ctx.objeto );
    c->ctx.paso   = g_strdup( g_ctx.paso );
    c->ctx.retiene = g_ctx.retiene;
    G_UNLOCK( g_ctx );

    c->programa = nombre_programa( programa );
    c->pre      = pre_de( workdir, argv );
    c->lanzado  = time( NULL );
#ifndef G_OS_WIN32
    c->cpu0     = cpu_hijos();
#endif
    c->t0       = g_get_monotonic_time();
    return c;
}

void coste_hijo_acabo( CosteCorrida *c, GPid pid )
{
    if ( c == NULL || c->medida ) return;
    c->pared_s = ( g_get_monotonic_time() - c->t0 ) / 1e6;
#ifdef G_OS_WIN32
    /* EL PROCESO DEL MOTOR, ANTES DE CERRAR SU MANEJADOR: despues ya no hay
       a quien preguntar. Los tiempos van en unidades de 100 ns.          */
    {
    FILETIME cr, ex, k, u;

    if ( GetProcessTimes( (HANDLE) pid, &cr, &ex, &k, &u ) )
        {
        ULARGE_INTEGER a, b;

        a.LowPart = k.dwLowDateTime; a.HighPart = k.dwHighDateTime;
        b.LowPart = u.dwLowDateTime; b.HighPart = u.dwHighDateTime;
        c->cpu_s = (double) ( a.QuadPart + b.QuadPart ) / 1e7;
        }
    }
#else
    /* LO RECOGE GLib, NO NOSOTROS: cuando llega aqui el hijo ya esta
       recogido y su rusage se ha sumado a RUSAGE_CHILDREN, asi que se toma
       la diferencia desde el lanzamiento. ES EXACTA MIENTRAS CORRA UN MOTOR
       A LA VEZ, que es como trabajan la madre y los contrastes (uno detras
       de otro); si otro hijo se recoge en medio --dos motores a la vez, una
       ventana hija que se cierra--, su CPU entra aqui. Para medir solo este
       habria que recogerlo con wait4, y eso es quitarle el hijo a GLib.  */
    (void) pid;
    c->cpu_s = cpu_hijos() - c->cpu0;
    if ( c->cpu_s < 0.0 ) c->cpu_s = 0.0;
#endif
    c->medida = TRUE;
}

/* LA VERSION, DE LO QUE EL MOTOR DICE AL EMPEZAR: «FUE 1.14: Copyright...».
 * Se busca el nombre del programa seguido de un numero en lo primero de la
 * salida. Si no esta --art, drtran no la escriben--, solo el nombre: una
 * version inventada es peor que ninguna.                              */
static gchar *motor_de( const char *programa, const char *salida )
{
    gchar      *pre, *nom;
    const char *p;
    gsize       l;
    gchar      *r = NULL;

    if ( !salida || !*salida ) return g_strdup( programa );
    pre = g_ascii_strdown( salida, MIN( strlen( salida ), 4096 ) );
    nom = g_ascii_strdown( programa, -1 );
    l   = strlen( nom );
    for ( p = strstr( pre, nom ); p && !r; p = strstr( p + 1, nom ) )
        {
        const char *v = p + l, *f;

        if ( p > pre && g_ascii_isalnum( p[-1] ) ) continue;
        if ( *v != ' ' || !g_ascii_isdigit( v[1] ) ) continue;
        v++;
        for ( f = v; g_ascii_isdigit( *f ) || *f == '.'; f++ ) ;
        while ( f > v && f[-1] == '.' ) f--;
        r = g_strdup_printf( "%s %.*s", programa, (int) ( f - v ), v );
        }
    g_free( pre );
    g_free( nom );
    return r ? r : g_strdup( programa );
}

static gchar *sha_de( const CosteCorrida *c )
{
    GStatBuf st;
    gchar   *datos = NULL, *sha;
    gsize    n = 0;

    if ( !c->pre || g_stat( c->pre, &st ) != 0 ) return NULL;
    /* UN .pre DE ANTES NO ES DE ESTA CORRIDA. Un segundo de holgura por los
       sistemas de ficheros que redondean la fecha.                      */
    if ( st.st_mtime < c->lanzado - 1 ) return NULL;
    if ( !g_file_get_contents( c->pre, &datos, &n, NULL ) ) return NULL;
    sha = g_compute_checksum_for_data( G_CHECKSUM_SHA256, (const guchar *) datos, n );
    g_free( datos );
    return sha;
}

static void corrida_libera( CosteCorrida *c )
{
    ctx_libera( &c->ctx );
    g_free( c->programa );
    g_free( c->pre );
    g_free( c );
}

void coste_descarta( CosteCorrida *c )
{
    if ( c ) corrida_libera( c );
}

void coste_termina( CosteCorrida *c, gboolean escribio, const char *salida )
{
    CosteLinea l;
    char       why[512];
    gchar     *motor, *sha = NULL;

    if ( c == NULL ) return;
    if ( !c->medida ) coste_hijo_acabo( c, 0 );

    motor = motor_de( c->programa, salida );
    if ( escribio ) sha = sha_de( c );

    memset( &l, 0, sizeof l );
    l.quien     = c->ctx.quien;
    l.por       = c->ctx.por;
    l.objeto    = c->ctx.objeto;
    l.paso      = c->ctx.paso;
    l.motor     = motor;
    l.via       = "C";
    l.cpu_s     = c->cpu_s;
    l.pared_s   = c->pared_s;
    l.energia_j = -1.0;                       /* la estima coste_apunta */
    l.sha_pre   = sha;

    /* RETENIDA: la escribira el que la pidio, cuando sepa que poner. */
    if ( c->ctx.retiene )
        {
        CosteLinea *r = g_new0( CosteLinea, 1 );

        r->fecha     = ahora();
        r->quien     = g_strdup( l.quien );
        r->por       = g_strdup( l.por );
        r->objeto    = g_strdup( l.objeto );
        r->paso      = g_strdup( l.paso );
        r->motor     = g_strdup( l.motor );
        r->via       = g_strdup( l.via );
        r->cpu_s     = l.cpu_s;
        r->pared_s   = l.pared_s;
        r->energia_j = -1.0;
        r->metodo    = g_strdup( "" );
        r->sha_pre   = g_strdup( sha ? sha : "" );
        G_LOCK( g_retenida );
        coste_lineas_libera( g_retenida, g_retenida ? 1 : 0 );
        g_retenida = r;
        G_UNLOCK( g_retenida );
        g_free( motor );
        g_free( sha );
        corrida_libera( c );
        return;
        }

    /* SE DICE, PERO NO PARA NADA: el motor ya ha corrido y sus resultados
       estan escritos. Un registro que no se puede escribir no es motivo
       para que la estimacion falle.                                   */
    if ( coste_apunta( c->ctx.raiz, &l, why, sizeof why ) != 0 )
        g_printerr( "costes.csv: %s\n", why );

    g_free( motor );
    g_free( sha );
    corrida_libera( c );
}


/* ------------------------------------------------------------------------ */
/* Leer y sumar                                                              */
/* ------------------------------------------------------------------------ */

/* UNA LINEA DE CSV, con comillas. Devuelve los campos (g_strfreev) o NULL
 * si las comillas no cierran.                                          */
static gchar **parte( const char *s, gsize len )
{
    GPtrArray *v = g_ptr_array_new();
    GString   *c = g_string_new( NULL );
    gsize      i = 0;
    gboolean   dentro = FALSE;

    for ( ;; )
        {
        if ( i >= len )
            {
            if ( dentro ) break;
            g_ptr_array_add( v, g_string_free( c, FALSE ) );
            g_ptr_array_add( v, NULL );
            return (gchar **) g_ptr_array_free( v, FALSE );
            }
        if ( dentro )
            {
            if ( s[i] == '"' && i + 1 < len && s[i + 1] == '"' ) { g_string_append_c( c, '"' ); i += 2; continue; }
            if ( s[i] == '"' ) { dentro = FALSE; i++; continue; }
            g_string_append_c( c, s[i++] );
            }
        else if ( s[i] == '"' ) { dentro = TRUE; i++; }
        else if ( s[i] == ',' )
            {
            g_ptr_array_add( v, g_string_free( c, FALSE ) );
            c = g_string_new( NULL );
            i++;
            }
        else g_string_append_c( c, s[i++] );
        }
    g_string_free( c, TRUE );
    g_ptr_array_add( v, NULL );
    g_strfreev( (gchar **) g_ptr_array_free( v, FALSE ) );
    return NULL;
}

static gboolean numero_de( const char *s, double *v )
{
    char *fin;

    if ( !s || !*s ) return FALSE;
    *v = g_ascii_strtod( s, &fin );
    return *fin == '\0' && isfinite( *v );
}

int coste_lee( const char *raiz, CosteLinea **lineas, int *n, int *malas,
               char *why, size_t nwhy )
{
    gchar   *ruta, *todo = NULL;
    gsize    len = 0, i = 0;
    GArray  *a;
    GError  *err = NULL;
    int      mal = 0;
    char     dummy[8];

    if ( why == NULL || nwhy == 0 ) { why = dummy; nwhy = sizeof dummy; }
    why[0] = '\0';
    *lineas = NULL;
    *n = 0;
    if ( malas ) *malas = 0;
    if ( raiz == NULL || *raiz == '\0' )
        { g_snprintf( why, nwhy, "falta la raiz del proyecto" ); return 1; }

    ruta = g_build_filename( raiz, COSTE_FICHERO, NULL );
    if ( !g_file_test( ruta, G_FILE_TEST_EXISTS ) ) { g_free( ruta ); return 0; }
    if ( !g_file_get_contents( ruta, &todo, &len, &err ) )
        {
        g_snprintf( why, nwhy, "no pude leer %s: %s", ruta, err ? err->message : "?" );
        if ( err ) g_error_free( err );
        g_free( ruta );
        return 1;
        }
    g_free( ruta );

    a = g_array_new( FALSE, TRUE, sizeof( CosteLinea ) );
    while ( i < len )
        {
        gsize   f = i, l;
        gchar **c;

        while ( f < len && todo[f] != '\n' ) f++;
        l = f - i;
        if ( l > 0 && todo[i + l - 1] == '\r' ) l--;       /* guardado en Windows */
        if ( l > 0 && !( l >= 6 && strncmp( todo + i, "fecha,", 6 ) == 0 ) )
            {
            CosteLinea x;

            c = parte( todo + i, l );
            memset( &x, 0, sizeof x );
            if ( c && g_strv_length( c ) == COSTE_CAMPOS &&
                 numero_de( c[7], &x.cpu_s ) && numero_de( c[8], &x.pared_s ) )
                {
                if ( !numero_de( c[9], &x.energia_j ) ) x.energia_j = -1.0;
                x.fecha  = g_strdup( c[0] ); x.quien = g_strdup( c[1] );
                x.por    = g_strdup( c[2] ); x.objeto = g_strdup( c[3] );
                x.paso   = g_strdup( c[4] ); x.motor = g_strdup( c[5] );
                x.via    = g_strdup( c[6] ); x.metodo = g_strdup( c[10] );
                x.sha_pre = g_strdup( c[11] );
                g_array_append_val( a, x );
                }
            else mal++;
            g_strfreev( c );
            }
        i = f + 1;
        }
    g_free( todo );

    *n = (int) a->len;
    *lineas = (CosteLinea *) g_array_free( a, FALSE );
    if ( *n == 0 ) { g_free( *lineas ); *lineas = NULL; }
    if ( malas ) *malas = mal;
    return 0;
}

void coste_lineas_libera( CosteLinea *l, int n )
{
    int i;

    if ( !l ) return;
    for ( i = 0; i < n; i++ )
        {
        g_free( l[i].fecha ); g_free( l[i].quien ); g_free( l[i].por );
        g_free( l[i].objeto ); g_free( l[i].paso ); g_free( l[i].motor );
        g_free( l[i].via ); g_free( l[i].metodo ); g_free( l[i].sha_pre );
        }
    g_free( l );
}

/* LA CLAVE DE UNA LINEA para el grupo pedido. NULL: el grupo no existe. */
static gchar *clave_de( const CosteLinea *l, const char *por_que )
{
    const char *v, *vacio;

    if ( !por_que || !*por_que )         return g_strdup( "total" );
    if ( !strcmp( por_que, "serie" ) )
        {
        const char *b = strchr( l->objeto, '/' );

        if ( !*l->objeto ) return g_strdup( "(sin objeto)" );
        return b ? g_strndup( l->objeto, (gsize) ( b - l->objeto ) ) : g_strdup( l->objeto );
        }
    if ( !strcmp( por_que, "motor" ) )
        {
        const char *b = strchr( l->motor, ' ' );

        if ( !*l->motor ) return g_strdup( "(sin motor)" );
        return b ? g_strndup( l->motor, (gsize) ( b - l->motor ) ) : g_strdup( l->motor );
        }
    if      ( !strcmp( por_que, "objeto" ) ) { v = l->objeto; vacio = "(sin objeto)"; }
    else if ( !strcmp( por_que, "via" ) )    { v = l->via;    vacio = "(sin via)"; }
    else if ( !strcmp( por_que, "por" ) )    { v = l->por;    vacio = "(sin por)"; }
    else if ( !strcmp( por_que, "paso" ) )   { v = l->paso;   vacio = "(sin paso)"; }
    else if ( !strcmp( por_que, "quien" ) )  { v = l->quien;  vacio = "(sin quien)"; }
    else return NULL;
    return g_strdup( *v ? v : vacio );
}

int coste_resumen( const char *raiz, const char *por_que, CosteTotal **t,
                   int *n, char *why, size_t nwhy )
{
    CosteLinea *l = NULL;
    int         nl = 0, i;
    GArray     *a;
    GHashTable *idx;
    char        dummy[8];

    if ( why == NULL || nwhy == 0 ) { why = dummy; nwhy = sizeof dummy; }
    *t = NULL;
    *n = 0;
    {
    CosteLinea prueba;

    memset( &prueba, 0, sizeof prueba );
    prueba.objeto = prueba.motor = prueba.via = prueba.por = prueba.paso =
    prueba.quien = "";
    {
    gchar *k = clave_de( &prueba, por_que );

    if ( k == NULL )
        {
        g_snprintf( why, nwhy, "no sé agrupar por «%s»: serie, objeto, motor, "
                    "via, por, paso o quien", por_que );
        return 1;
        }
    g_free( k );
    }
    }
    if ( coste_lee( raiz, &l, &nl, NULL, why, nwhy ) != 0 ) return 1;

    a   = g_array_new( FALSE, TRUE, sizeof( CosteTotal ) );
    idx = g_hash_table_new_full( g_str_hash, g_str_equal, g_free, NULL );
    for ( i = 0; i < nl; i++ )
        {
        gchar     *k = clave_de( &l[i], por_que );
        gpointer   pos;
        CosteTotal *x;

        if ( g_hash_table_lookup_extended( idx, k, NULL, &pos ) )
            g_free( k );
        else
            {
            CosteTotal nuevo;

            memset( &nuevo, 0, sizeof nuevo );
            nuevo.clave = g_strdup( k );
            g_array_append_val( a, nuevo );
            pos = GINT_TO_POINTER( (int) a->len - 1 );
            g_hash_table_insert( idx, k, pos );
            }
        x = &g_array_index( a, CosteTotal, GPOINTER_TO_INT( pos ) );
        x->corridas++;
        x->cpu_s   += l[i].cpu_s;
        x->pared_s += l[i].pared_s;
        if ( l[i].energia_j >= 0.0 ) x->energia_j += l[i].energia_j;
        }
    g_hash_table_destroy( idx );
    coste_lineas_libera( l, nl );

    *n = (int) a->len;
    *t = (CosteTotal *) g_array_free( a, FALSE );
    if ( *n == 0 ) { g_free( *t ); *t = NULL; }
    return 0;
}

void coste_totales_libera( CosteTotal *t, int n )
{
    int i;

    if ( !t ) return;
    for ( i = 0; i < n; i++ ) g_free( t[i].clave );
    g_free( t );
}
