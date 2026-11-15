/*
 * atsw-cli -- la puerta sin ventana: lo que la madre hace y calcula, como
 * orden, en texto o en JSON.
 *
 * POR QUE EXISTE (docs/ESTUDIO-terminal-mcp.md §2.2). El asistente maneja
 * los motores en C, y casi todo lo que la madre sabe hacer --derivar un
 * modelo, el dictamen de un .out, la factorizacion del AR-- esta en lib/ y
 * solo se llamaba desde las ventanas. Esto lo llama sin ventana. Son LAS
 * MISMAS FUNCIONES que usa la madre, asi que da los mismos numeros: dos
 * anfitriones, un calculo.
 *
 * UN SOLO ESCRITOR DEL MANIFIESTO (§2.5). El asistente no escribe
 * proyecto.yaml: llama a «atsw-cli proyecto …», que lo escribe con
 * lib/proyecto, el mismo lector y escritor de la madre.
 *
 * LA SALIDA. Texto para una persona; con --json, un objeto por orden con
 * "ok" y lo demas, para un programa. Los errores van a stderr en texto y,
 * con --json, tambien como {"ok": false, "error": "..."} en stdout. El
 * estado de salida: 0 bien, 1 no se pudo, 2 la orden esta mal escrita.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "proyecto.h"
#include "gesto.h"
#include "outfile.h"
#include "dictamen.h"
#include "inpdet.h"
#include "raices.h"
#include "inpcheck.h"
#include "coste.h"
#include "engine.h"
#include "sitio.h"
#include "ficha.h"
#include "outfcst.h"
#include "netfile.h"
#include "outdiag.h"
#include "fcomp.h"
#include "artrun.h"
#include "inpfile.h"
#include "formales.h"
/* el mundo de drtran, para el preblanqueo: lo mismo que enlaza drtran_gui */
/* main.h define TRUE y FALSE a su manera: se le dejan y luego se vuelven a
   los de GLib, que es lo que usa todo lo demas de aqui.                 */
#undef TRUE
#undef FALSE
#include "main.h"
#undef TRUE
#undef FALSE
#define FALSE (0)
#define TRUE  (!FALSE)
#include "fue_pre_reader.h"
#include "prewhiten.h"
#include "residuos.h"
#include "sistema.h"
#include "outvarma.h"

#ifndef ATSW_VERSION
#define ATSW_VERSION "en desarrollo"
#endif

/* ------------------------------------------------------------------------ */
/* Las opciones comunes                                                     */
/* ------------------------------------------------------------------------ */

static gboolean  json;
/* caso-estima */
static const char *k_enlace[NET_MAX_LINK];
static int         k_nenlace = 0;
#define K_MAX_RESTR 64
static const char *k_restr[K_MAX_RESTR];   /* caso-estima --restriccion "NOMBRE = VALOR" */
static int         k_nrestr = 0;
static int         k_p = -1, k_q = -1;  /* caso-estima de SIM: la dinamica cruzada */
static int         k_diagcov = 0;       /* caso-estima de SIM: Sigma diagonal      */
static const char *k_cero[K_MAX_RESTR]; /* caso-estima de SIM: --cero "AR1[A<-B]"  */
static int         k_ncero = 0;
static const char *k_desde = NULL;
static gboolean    k_diagonal = FALSE;
static int         k_prever = 0, k_evaluar = 0;   /* caso-estima --prever H --evaluar W */
static const char *i_resid = NULL;        /* identifica --residuos          */
static gboolean  comprobar;
/* especifica: lo que se cambia del .inp (lib/inpdet) */
static const char *e_arma = NULL, *e_anade = NULL, *e_transf = NULL, *e_media = NULL;
static const char *e_armon = NULL;   /* especifica --armonicos N|todos */
static int         e_pascua = 0;     /* especifica --pascua            */
static const char *e_interv[16];
static int         e_ninterv = 0;
static gboolean  con_sub;                 /* formales --sub                 */
static const char *familia = NULL;        /* formales --familia             */
static int       retardos = 0;            /* prewhiten --retardos           */
static const char *episodios = NULL;      /* anomalos --episodios           */
static const char *muestra = "";
static const char *razon   = NULL;
static const char *titulo  = NULL;

/* las de «coste apunta» y «coste resumen» */
static const char *c_quien, *c_por, *c_objeto, *c_paso, *c_motor, *c_via,
                  *c_cpu, *c_pared, *c_energia, *c_metodo, *c_sha, *c_fecha,
                  *c_por_que;

/* ------------------------------------------------------------------------ */
/* JSON a mano: es poco y no merece una dependencia mas en tres sistemas    */
/* ------------------------------------------------------------------------ */

static void js_cadena( GString *t, const char *s )
{
    const unsigned char *p;

    g_string_append_c( t, '"' );
    for ( p = (const unsigned char *) ( s ? s : "" ); *p; p++ )
        switch ( *p )
            {
            case '"':  g_string_append( t, "\\\"" ); break;
            case '\\': g_string_append( t, "\\\\" ); break;
            case '\n': g_string_append( t, "\\n" ); break;
            case '\r': g_string_append( t, "\\r" ); break;
            case '\t': g_string_append( t, "\\t" ); break;
            default:
                if ( *p < 0x20 ) g_string_append_printf( t, "\\u%04x", *p );
                else g_string_append_c( t, (char) *p );
            }
    g_string_append_c( t, '"' );
}

/* "clave": "valor", */
static void js_par( GString *t, const char *k, const char *v )
{
    js_cadena( t, k );
    g_string_append( t, ": " );
    js_cadena( t, v );
}

/* Un numero; NaN e infinito no existen en JSON: null. */
static void js_num( GString *t, const char *k, double v )
{
    js_cadena( t, k );
    if ( isfinite( v ) ) g_string_append_printf( t, ": %.10g", v );
    else g_string_append( t, ": null" );
}

/* s rellenado hasta ancho CARACTERES (no bytes: «Estimación» lleva una
   tilde y con %-14s descuadraba la columna). */
static void col( GString *t, const char *s, int ancho )
{
    glong n = g_utf8_strlen( s, -1 );

    g_string_append( t, s );
    for ( ; n < ancho; n++ ) g_string_append_c( t, ' ' );
}

/* ------------------------------------------------------------------------ */
/* Errores y el proyecto                                                    */
/* ------------------------------------------------------------------------ */

static int falla( const char *fmt, ... ) G_GNUC_PRINTF( 1, 2 );

static int falla( const char *fmt, ... )
{
    va_list ap;
    gchar  *m;

    va_start( ap, fmt );
    m = g_strdup_vprintf( fmt, ap );
    va_end( ap );
    fprintf( stderr, "atsw-cli: %s\n", m );
    if ( json )
        {
        GString *t = g_string_new( "{\"ok\": false, " );

        js_par( t, "error", m );
        g_string_append( t, "}\n" );
        fputs( t->str, stdout );
        g_string_free( t, TRUE );
        }
    g_free( m );
    return 1;
}

/* ~1,2 MB: nunca en la pila (proyecto.h). */
static Proyecto *abre( const char *path, char *why, size_t n )
{
    Proyecto *p = g_new0( Proyecto, 1 );
    PrError   e;

    if ( pr_leer( path, p, &e ) != 0 )
        {
        pr_error_es( &e, why, n );
        g_free( p );
        return NULL;
        }
    return p;
}

static int guarda( Proyecto *p, char *why, size_t n )
{
    PrError e;

    if ( pr_escribir( p, p->path, &e ) != 0 ) { pr_error_es( &e, why, n ); return 1; }
    return 0;
}

/* QUIEN DIO EL PASO (ESTUDIO-terminal-mcp §7.2): con --por asistente, el
   modelo nuevo lo lleva escrito. El analista no se escribe.            */
static void pon_por( Proyecto *p, const char *serie, const char *id )
{
    PrError e;

    if ( c_por ) pr_pon_por( p, serie, muestra, id, c_por, &e );
}

/* La razon, si se dio, en el modelo recien hecho. */
static void pon_razon( Proyecto *p, const char *serie, const char *id )
{
    PrError e;

    if ( razon && *razon ) pr_razon( p, serie, muestra, id, razon, &e );
}

/* El resultado de un gesto que crea un modelo: texto o JSON. */
static void dice_modelo( const char *serie, const char *id, const char *ruta,
                         const char *frase )
{
    if ( json )
        {
        GString *t = g_string_new( "{\"ok\": true, " );
        /* la ruta, absoluta: quien la lea puede estar en otra carpeta */
        gchar   *abs = g_canonicalize_filename( ruta, NULL );

        js_par( t, "serie", serie );   g_string_append( t, ", " );
        js_par( t, "muestra", muestra ); g_string_append( t, ", " );
        js_par( t, "modelo", id );     g_string_append( t, ", " );
        js_par( t, "inp", abs );       g_string_append( t, ", " );
        g_free( abs );
        js_par( t, "dice", frase );    g_string_append( t, "}\n" );
        fputs( t->str, stdout );
        g_string_free( t, TRUE );
        }
    else printf( "%s\n", frase );
}

/* ------------------------------------------------------------------------ */
/* proyecto                                                                 */
/* ------------------------------------------------------------------------ */

/* proyecto ver P: las series, sus modelos con su linaje y su razon, el
   elegido de cada una, y los casos. */
static int proyecto_ver( const char *path )
{
    char      why[512];
    Proyecto *p = abre( path, why, sizeof why );
    GString  *t;
    int       i, j;

    if ( !p ) return falla( "%s", why );
    t = g_string_new( "" );
    if ( json )
        {
        g_string_append( t, "{\"ok\": true, " );
        js_par( t, "id", p->id ); g_string_append( t, ", " );
        js_par( t, "titulo", p->titulo ); g_string_append( t, ", \"modelos\": [" );
        for ( i = 0; i < p->nm; i++ )
            {
            const PrModelo *m = &p->m[i];

            g_string_append( t, i ? ", {" : "{" );
            js_par( t, "serie", m->serie ); g_string_append( t, ", " );
            js_par( t, "muestra", m->muestra ); g_string_append( t, ", " );
            js_par( t, "id", m->id ); g_string_append( t, ", " );
            js_par( t, "padre", m->padre ); g_string_append( t, ", " );
            js_par( t, "razon", m->razon ); g_string_append( t, ", " );
            js_par( t, "por", m->por[0] ? m->por : "analista" ); g_string_append( t, ", " );
            js_par( t, "por_elegido", !m->elegido ? "" :
                    m->por_elegido[0] ? m->por_elegido : "analista" );
            g_string_append( t, ", " );
            g_string_append_printf( t, "\"datos\": %s, \"elegido\": %s}",
                                    m->rol == PR_DATOS ? "true" : "false",
                                    m->elegido ? "true" : "false" );
            }
        g_string_append( t, "], \"casos\": [" );
        for ( i = 0; i < p->nca; i++ )
            {
            const PrCaso *c = &p->ca[i];

            g_string_append( t, i ? ", {" : "{" );
            js_par( t, "id", c->id ); g_string_append( t, ", " );
            js_par( t, "motor", c->motor ); g_string_append( t, ", " );
            js_par( t, "muestra", c->muestra ); g_string_append( t, ", \"entradas\": [" );
            for ( j = 0; j < c->nen; j++ )
                {
                gchar *e = g_strdup_printf( "%s/%s", c->en[j].serie, c->en[j].modelo );

                if ( j ) g_string_append( t, ", " );
                js_cadena( t, e );
                g_free( e );
                }
            g_string_append( t, "]}" );
            }
        g_string_append( t, "]}\n" );
        }
    else
        {
        g_string_append_printf( t, "%s — %s\n", p->id, p->titulo );
        for ( i = 0; i < p->nm; i++ )
            {
            const PrModelo *m = &p->m[i];

            g_string_append_printf( t, "  %s/%s%s%s%s", m->serie, m->id,
                                    m->muestra[0] ? "  [" : "", m->muestra,
                                    m->muestra[0] ? "]" : "" );
            /* LA MARCA DEL ASISTENTE, solo cuando la hay (§7.2) */
            if ( m->por[0] ) g_string_append_printf( t, "  ✦ %s", m->por );
            if ( m->elegido ) g_string_append( t, "  * elegido" );
            if ( m->elegido && m->por_elegido[0] )
                g_string_append_printf( t, " ✦ %s", m->por_elegido );
            g_string_append_printf( t, "%s\n", m->rol == PR_DATOS ? "  (datos)" : "" );
            if ( m->padre[0] || m->razon[0] )
                g_string_append_printf( t, "      de %s: %s\n", m->padre[0] ? m->padre : "—",
                                        m->razon[0] ? m->razon : "(sin razón)" );
            }
        for ( i = 0; i < p->nca; i++ )
            {
            const PrCaso *c = &p->ca[i];

            g_string_append_printf( t, "  caso %s (%s):", c->id, c->motor );
            for ( j = 0; j < c->nen; j++ )
                g_string_append_printf( t, " %s/%s", c->en[j].serie, c->en[j].modelo );
            g_string_append_c( t, '\n' );
            }
        }
    fputs( t->str, stdout );
    g_string_free( t, TRUE );
    g_free( p );
    return 0;
}

/* proyecto itera P SERIE PADRE / proyecto nuevo P SERIE */
static int proyecto_deriva( const char *path, const char *serie, const char *padre )
{
    char      why[512], id[PR_ID], ruta[PR_RUTA], frase[512];
    Proyecto *p = abre( path, why, sizeof why );
    int       rc;

    if ( !p ) return falla( "%s", why );
    rc = padre ? ge_itera( p, serie, muestra, padre, id, sizeof id, ruta, sizeof ruta,
                           frase, sizeof frase )
               : ge_modelo_nuevo( p, serie, muestra, id, sizeof id, ruta, sizeof ruta,
                                  frase, sizeof frase );
    if ( rc != 0 ) { g_free( p ); return falla( "%s", frase ); }
    pon_razon( p, serie, id );
    pon_por( p, serie, id );
    if ( guarda( p, why, sizeof why ) != 0 )
        { g_free( p ); return falla( "El .inp está, pero no pude guardar el proyecto: %s", why ); }
    dice_modelo( serie, id, ruta, frase );
    g_free( p );
    return 0;
}

/* proyecto razon P SERIE ID TEXTO / proyecto elige P SERIE ID RAZON */
static int proyecto_decide( const char *path, const char *serie, const char *id,
                            const char *texto, gboolean elige )
{
    char      why[512];
    Proyecto *p = abre( path, why, sizeof why );
    PrError   e;
    int       rc;

    if ( !p ) return falla( "%s", why );
    rc = elige ? pr_elige( p, serie, muestra, id, texto, &e )
               : pr_razon( p, serie, muestra, id, texto, &e );
    /* QUIEN ELIGIO, despues de pr_elige, que lo borra al mover la marca. */
    if ( rc == 0 && elige ) rc = pr_pon_por_elegido( p, serie, muestra, id, c_por, &e );
    if ( rc != 0 ) { pr_error_es( &e, why, sizeof why ); g_free( p ); return falla( "%s", why ); }
    if ( guarda( p, why, sizeof why ) != 0 ) { g_free( p ); return falla( "%s", why ); }
    if ( json )
        {
        GString *t = g_string_new( "{\"ok\": true, " );

        js_par( t, "serie", serie ); g_string_append( t, ", " );
        js_par( t, "modelo", id ); g_string_append( t, ", " );
        js_par( t, elige ? "elegido" : "razon", texto ); g_string_append( t, "}\n" );
        fputs( t->str, stdout );
        g_string_free( t, TRUE );
        }
    else printf( elige ? "%s/%s es el elegido de su serie.\n" : "%s/%s: razón apuntada.\n",
                 serie, id );
    g_free( p );
    return 0;
}

/* proyecto caso-nuevo P SERIE/MODELO...: la salida primero, como en la madre.
   El sha de cada .pre lo pone quien llama (lib/proyecto no lee ficheros). */
static int proyecto_caso( const char *path, char **en, int nen )
{
    char      why[512], id[PR_ID];
    Proyecto *p = abre( path, why, sizeof why );
    PrEntrada e[PR_MAX_ENTRADA];
    PrError   pe;
    int       i;

    if ( !p ) return falla( "%s", why );
    if ( nen < 2 ) { g_free( p ); return falla( "Un caso cruza dos series o más." ); }
    if ( nen > PR_MAX_ENTRADA ) { g_free( p ); return falla( "Caben %d entradas.", PR_MAX_ENTRADA ); }
    memset( e, 0, sizeof e );
    for ( i = 0; i < nen; i++ )
        {
        char *bar = strchr( en[i], '/' ), pre[PR_RUTA];
        char *h;

        if ( !bar ) { g_free( p ); return falla( "«%s»: se escribe SERIE/MODELO.", en[i] ); }
        g_snprintf( e[i].serie, sizeof e[i].serie, "%.*s", (int) ( bar - en[i] ), en[i] );
        g_snprintf( e[i].modelo, sizeof e[i].modelo, "%s", bar + 1 );
        if ( pr_ruta( p, e[i].serie, muestra, e[i].modelo, ".pre", pre, sizeof pre ) == 0 &&
             ( h = ge_sha( pre ) ) != NULL )
            { g_snprintf( e[i].sha, sizeof e[i].sha, "%s", h ); g_free( h ); }
        else
            { g_free( p ); return falla( "%s/%s no está estimado: un caso entra con su .pre.",
                                         e[i].serie, e[i].modelo ); }
        }
    /* EL MOTOR DEL CASO: drtran (MT, si no se dice) o drvarma (SIM), como la
       madre con «Caso nuevo» y con el menú SIM.                          */
    if ( c_motor && *c_motor && strcmp( c_motor, "drtran" ) && strcmp( c_motor, "drvarma" ) )
        { g_free( p ); return falla( "--motor %s: un caso es de drtran (MT) o de drvarma (SIM).",
                                     c_motor ); }
    if ( pr_caso_add( p, e, nen, muestra, c_motor, titulo, razon, id, sizeof id, &pe ) != 0 )
        { pr_error_es( &pe, why, sizeof why ); g_free( p ); return falla( "%s", why ); }
    if ( guarda( p, why, sizeof why ) != 0 ) { g_free( p ); return falla( "%s", why ); }
    if ( json )
        {
        GString *t = g_string_new( "{\"ok\": true, " );

        js_par( t, "caso", id ); g_string_append( t, ", " );
        js_par( t, "motor", ( c_motor && *c_motor ) ? c_motor : "drtran" );
        g_string_append( t, "}\n" );
        fputs( t->str, stdout );
        g_string_free( t, TRUE );
        }
    else if ( c_motor && !strcmp( c_motor, "drvarma" ) )
        printf( "Caso %s (SIM) dado de alta con %d series, en este orden.\n", id, nen );
    else printf( "Caso %s dado de alta con %d series; la salida es %s.\n", id, nen, e[0].serie );
    g_free( p );
    return 0;
}

/* ------------------------------------------------------------------------ */
/* dictamen: la diagnosis del .out, la misma que la ventana de la madre     */
/* ------------------------------------------------------------------------ */

static int ruta_de( const char *path, const char *serie, const char *id, const char *ext,
                    char *out, size_t n, Proyecto **pp )
{
    char why[512];

    *pp = abre( path, why, sizeof why );
    if ( !*pp ) return falla( "%s", why );
    if ( pr_modelo_idx( *pp, serie, muestra, id ) < 0 )
        return falla( "«%s» no es un modelo de %s en esa muestra.", id, serie );
    if ( pr_ruta( *pp, serie, muestra, id, ext, out, n ) != 0 )
        return falla( "No pude componer la ruta." );
    return 0;
}

static int dictamen( const char *path, const char *serie, const char *id )
{
    char        out[PR_RUTA];
    Proyecto   *p = NULL;
    Dictamen    d;
    GString    *t;
    int         i;

    if ( ruta_de( path, serie, id, ".out", out, sizeof out, &p ) != 0 ) { g_free( p ); return 1; }
    /* EL MISMO QUE LA FICHA (lib/ficha), que es el de la ventana */
    if ( fi_dictamen( p, serie, muestra, id, &d ) != 0 )
        { g_free( p ); return falla( "«%s» no está estimado: no hay .out.", id ); }

    t = g_string_new( "" );
    if ( json )
        {
        g_string_append( t, "{\"ok\": true, " );
        js_par( t, "serie", serie ); g_string_append( t, ", " );
        js_par( t, "modelo", id ); g_string_append( t, ", " );
        js_par( t, "peor", dx_estado_es( d.peor ) ); g_string_append( t, ", \"lineas\": [" );
        for ( i = 0; i < d.n; i++ )
            {
            g_string_append( t, i ? ", {" : "{" );
            js_par( t, "titulo", d.l[i].titulo ); g_string_append( t, ", " );
            js_par( t, "estado", dx_estado_es( d.l[i].estado ) ); g_string_append( t, ", " );
            js_par( t, "dato", d.l[i].dato ); g_string_append( t, ", " );
            js_par( t, "dice", d.l[i].dice ); g_string_append( t, "}" );
            }
        g_string_append( t, "]}\n" );
        }
    else
        {
        g_string_append_printf( t, "Diagnosis de %s/%s: %s\n", serie, id, dx_estado_es( d.peor ) );
        for ( i = 0; i < d.n; i++ )
            {
            g_string_append( t, "  " );
            col( t, d.l[i].titulo, 16 );
            col( t, dx_estado_es( d.l[i].estado ), 10 );
            g_string_append_printf( t, " %s\n  %27s%s\n", d.l[i].dato, "", d.l[i].dice );
            }
        }
    fputs( t->str, stdout );
    g_string_free( t, TRUE );
    g_free( p );
    return 0;
}

/* proyecto ruta P SERIE MODELO EXT: la ruta de un fichero del modelo, la que
   compone lib/proyecto (pr_ruta) y no otra. Es para el extra Python del
   asistente, que llama a art con rutas: asi la regla de donde vive cada
   fichero sigue teniendo un solo dueno. MODELO «datos» es el nodo de datos
   de la serie en esa muestra. EXT es .inp, .pre o .out. No lee ni escribe
   nada; dice si el fichero existe.                                       */
static int proyecto_ruta( const char *path, const char *serie, const char *id, const char *ext )
{
    char      why[512], r[PR_RUTA];
    Proyecto *p = abre( path, why, sizeof why );
    int       hay;

    if ( !p ) return falla( "%s", why );
    if ( strcmp( ext, ".inp" ) && strcmp( ext, ".pre" ) && strcmp( ext, ".out" ) )
        { g_free( p ); return falla( "«%s»: la extensión es .inp, .pre o .out.", ext ); }
    if ( !strcmp( id, "datos" ) )
        {
        id = pr_datos_de( p, serie, muestra );
        if ( !id || !*id ) { g_free( p ); return falla( "%s no tiene datos en esa muestra.", serie ); }
        }
    if ( pr_modelo_idx( p, serie, muestra, id ) < 0 )
        { g_free( p ); return falla( "«%s» no es un modelo de %s en esa muestra.", id, serie ); }
    if ( pr_ruta( p, serie, muestra, id, ext, r, sizeof r ) != 0 )
        { g_free( p ); return falla( "No pude componer la ruta." ); }
    hay = g_file_test( r, G_FILE_TEST_EXISTS );
    if ( json )
        {
        GString *t = g_string_new( "{\"ok\": true, " );

        js_par( t, "modelo", id ); g_string_append( t, ", " );
        js_par( t, "ruta", r );
        g_string_append_printf( t, ", \"existe\": %s}\n", hay ? "true" : "false" );
        fputs( t->str, stdout );
        g_string_free( t, TRUE );
        }
    else printf( "%s\n", r );
    g_free( p );
    return hay ? 0 : 1;
}

/* ------------------------------------------------------------------------ */
/* ecuacion: el modelo estimado, como lo escribe el motor                   */
/* ------------------------------------------------------------------------ */

/* EL BLOQUE DE ESTIMACIONES DEL .out, TAL CUAL, con la t de cada parametro y
   el nombre de cada determinista, que el .out no da («Omegas for
   deterministic variable 3») y se casa por posicion con el .inp (id_nombres).
   No se transcribe a otra forma: el bloque del motor es la presentacion
   autoritativa, y una tabla rehecha puede equivocar signos o convenios (el
   de la FLT resta los retardos). Lo que se anade se dice aparte.          */
static int ecuacion( const char *path, const char *serie, const char *id )
{
    char        out[PR_RUTA], inp[PR_RUTA], why[256], linea[512], est[96];
    char        det[ID_MAX_DET][ID_LINEA], sigma[128] = "", errt[128] = "";
    Proyecto   *p = NULL;
    FueOut     *o;
    FILE       *f;
    GString    *bloque, *jpar, *t;
    int         ndet, dentro = 0, npar = 0, k;

    if ( ruta_de( path, serie, id, ".out", out, sizeof out, &p ) != 0 ) { g_free( p ); return 1; }
    if ( pr_ruta( p, serie, muestra, id, ".inp", inp, sizeof inp ) != 0 ) inp[0] = '\0';
    g_free( p );
    o = g_new0( FueOut, 1 );
    if ( !fueout_read( out, o ) || !( f = g_fopen( out, "r" ) ) )
        { g_free( o ); return falla( "«%s» no está estimado: no hay .out.", id ); }
    ndet = inp[0] ? id_nombres( inp, det, ID_MAX_DET, why, sizeof why ) : -1;

    bloque = g_string_new( "" );
    jpar   = g_string_new( "" );
    while ( fgets( linea, sizeof linea, f ) )
        {
        double v, se;
        int    i, n;

        linea[strcspn( linea, "\r\n" )] = '\0';
        if ( !strncmp( linea, "sigma :", 7 ) ) g_snprintf( sigma, sizeof sigma, "%s", linea + 7 );
        if ( !strncmp( linea, "Standard errors:", 16 ) ) g_snprintf( errt, sizeof errt, "%s", linea + 16 );
        if ( !dentro && ( !strncmp( linea, "Omegas for", 10 ) || !strncmp( linea, "Deltas for", 10 ) ||
                          !strncmp( linea, "Coefficients for", 16 ) || !strncmp( linea, "Mean parameter", 14 ) ) )
            dentro = 1;
        if ( !dentro ) continue;
        if ( !strncmp( linea, "Box-Cox lambda", 14 ) ) { dentro = -1; continue; }
        if ( dentro < 0 ) continue;
        if ( !linea[0] ) continue;
        g_string_append( bloque, "  " );
        g_string_append( bloque, linea );
        if ( sscanf( linea, " Omegas for deterministic variable %d", &k ) == 1 ||
             sscanf( linea, " Deltas for deterministic variable %d", &k ) == 1 )
            {
            if ( k >= 1 && k <= ndet )
                g_string_append_printf( bloque, "   ← %s", det[k - 1] );
            g_snprintf( est, sizeof est, "%s%s%s", linea, ( k >= 1 && k <= ndet ) ? " " : "",
                        ( k >= 1 && k <= ndet ) ? det[k - 1] : "" );
            }
        else if ( linea[strlen( linea ) - 1] == ':' )
            g_snprintf( est, sizeof est, "%s", linea );
        else if ( sscanf( linea, " %lf (%lf) [%d]%n", &v, &se, &i, &n ) == 3 )
            {
            /* ESTIMADO: con su error tipico y su numero en la tabla */
            g_string_append_printf( bloque, "   t = %6.2f", se > 0 ? v / se : 0.0 );
            g_string_append_printf( jpar, "%s{\"n\": %d, ", npar++ ? ", " : "", i );
            js_par( jpar, "bloque", est ); g_string_append( jpar, ", " );
            js_num( jpar, "estimacion", v ); g_string_append( jpar, ", " );
            js_num( jpar, "et", se ); g_string_append( jpar, ", " );
            js_num( jpar, "t", se > 0 ? v / se : 0.0 ); g_string_append( jpar, "}" );
            }
        else if ( sscanf( linea, " %lf", &v ) == 1 )
            {
            g_string_append( bloque, "   (fijo)" );
            g_string_append_printf( jpar, "%s{\"n\": null, ", npar++ ? ", " : "" );
            js_par( jpar, "bloque", est ); g_string_append( jpar, ", " );
            js_num( jpar, "estimacion", v ); g_string_append( jpar, ", \"fijo\": true}" );
            }
        g_string_append_c( bloque, '\n' );
        }
    fclose( f );
    g_strstrip( sigma ); g_strstrip( errt );

    t = g_string_new( "" );
    fueout_estructura( o, linea, sizeof linea );
    if ( json )
        {
        g_string_append( t, "{\"ok\": true, " );
        js_par( t, "serie", serie ); g_string_append( t, ", " );
        js_par( t, "modelo", id ); g_string_append( t, ", " );
        js_par( t, "estructura", linea ); g_string_append( t, ", " );
        js_num( t, "lambda", o->lambda ); g_string_append( t, ", " );
        g_string_append_printf( t, "\"nobs\": %d, \"npar\": %d, ", o->nobs, o->npar );
        js_par( t, "sigma", sigma ); g_string_append( t, ", " );
        js_par( t, "errores_tipicos", errt ); g_string_append( t, ", " );
        if ( o->tiene_logelf ) { js_num( t, "logelf", o->logelf ); g_string_append( t, ", " ); }
        g_string_append_printf( t, "\"parametros\": [%s], ", jpar->str );
        js_par( t, "bloque", bloque->str );
        g_string_append( t, "}\n" );
        }
    else
        {
        g_string_append_printf( t, "MODELO ESTIMADO: %s/%s   %s\n", serie, id, linea );
        g_string_append_printf( t, "  %d observaciones, %d parámetros\n\n", o->nobs, o->npar );
        g_string_append( t, "  El bloque de estimaciones del .out, tal cual: estimación (error típico) [número].\n"
                            "  Se añade la t de cada uno, y el nombre de cada determinista (del .inp).\n\n" );
        g_string_append( t, bloque->str );
        g_string_append( t, "\n" );
        if ( sigma[0] ) g_string_append_printf( t, "  σ de las innovaciones: %s%s\n", sigma,
                                                 o->lambda == 0.0 ? " (en logaritmos: ×100 es un %)" : "" );
        if ( o->tiene_logelf ) g_string_append_printf( t, "  log-verosimilitud: %.6f\n", o->logelf );
        if ( errt[0] ) g_string_append_printf( t, "  errores típicos: %s\n", errt );
        g_string_append( t, "  Convenio: los operadores de retardo RESTAN (w(B) = w0 - w1 B - ...).\n" );
        }
    fputs( t->str, stdout );
    g_string_free( t, TRUE ); g_string_free( bloque, TRUE ); g_string_free( jpar, TRUE );
    g_free( o );
    return 0;
}

/* ------------------------------------------------------------------------ */
/* raices: la factorizacion del AR, y el modelo factorizado                 */
/* ------------------------------------------------------------------------ */

/* La s y si la covarianza vale, como la ventana (an_factores_ar). */
static int lee_factores( const char *out, RzFactores *f, char *why, size_t n )
{
    FueOut     *o = g_new0( FueOut, 1 );
    Convergence c;
    int         s, sin_et;

    if ( !fueout_read( out, o ) ) { g_free( o ); snprintf( why, n, "no hay .out" ); return 1; }
    s = o->s > 0 ? o->s : 1;
    g_free( o );
    /* CERO ITERACIONES: la covarianza es la semilla (art BUG-0027) */
    memset( &c, 0, sizeof c );
    sin_et = convergence_of( out, &c ) && c.iterations == 0;
    convergence_clear( &c );
    return rz_factores_ar( out, s, sin_et, f, why, n );
}

static int raices_factores( const char *path, const char *serie, const char *id )
{
    char        out[PR_RUTA], why[256];
    Proyecto   *p = NULL;
    RzFactores *f;
    GString    *t;
    int         k, i;

    if ( ruta_de( path, serie, id, ".out", out, sizeof out, &p ) != 0 ) { g_free( p ); return 1; }
    f = g_new0( RzFactores, 1 );
    if ( lee_factores( out, f, why, sizeof why ) != 0 )
        { g_free( f ); g_free( p ); return falla( "%s/%s: %s", serie, id, why ); }
    t = g_string_new( "" );
    if ( json ) g_string_append_printf( t, "{\"ok\": true, \"s\": %d, \"sin_et\": %s, \"factores\": [",
                                        f->s, f->sin_et ? "true" : "false" );
    else g_string_append_printf( t, "Factorización del AR de %s/%s (s = %d)%s\n", serie, id, f->s,
                                 f->nar ? "" : ": no tiene AR regular." );
    for ( k = 0; k < f->nar; k++ )
        {
        const RzAr *a = &f->ar[k];

        if ( json )
            {
            g_string_append_printf( t, "%s{\"orden\": %d, \"raices\": [", k ? ", " : "", a->orden );
            for ( i = 0; i < a->orden && ( a->zre[i] != 0.0 || a->zim[i] != 0.0 ); i++ )
                g_string_append_printf( t, "%s[%.10g, %.10g]", i ? ", " : "", a->zre[i], a->zim[i] );
            g_string_append( t, "], \"partes\": [" );
            }
        else
            g_string_append_printf( t, "Factor AR %d, orden %d\n", k + 1, a->orden );
        for ( i = 0; i < a->nf; i++ )
            {
            const RzFactor *x = &a->f[i];

            if ( json )
                {
                g_string_append( t, i ? ", {" : "{" );
                g_string_append_printf( t, "\"complejo\": %s, ", x->complejo ? "true" : "false" );
                js_num( t, "a1", x->a1 ); g_string_append( t, ", " );
                js_num( t, "a2", x->complejo ? x->a2 : 0.0 ); g_string_append( t, ", " );
                js_num( t, "modulo", x->modulo );
                if ( x->complejo )
                    {
                    g_string_append( t, ", " ); js_num( t, "d", x->r );
                    g_string_append( t, ", " ); js_num( t, "frecuencia", x->omega / ( 2.0 * M_PI ) );
                    g_string_append( t, ", " ); js_num( t, "armonico", x->armonico );
                    g_string_append( t, ", " ); js_num( t, "periodo", x->periodo );
                    g_string_append( t, ", " ); js_num( t, "vida_media", x->vida_media );
                    if ( x->con_et )
                        {
                        g_string_append( t, ", " ); js_num( t, "d_et", x->r_et );
                        g_string_append( t, ", " ); js_num( t, "periodo_et", x->periodo_et );
                        }
                    g_string_append_printf( t, ", \"estacional\": %s, \"casi_unitaria\": %s",
                                            x->estacional ? "true" : "false",
                                            x->casi_unitaria ? "true" : "false" );
                    }
                g_string_append( t, "}" );
                }
            else if ( !x->complejo )
                g_string_append_printf( t, "  real      (1 − %.5f B)  raíz %.4f\n", x->a1, 1.0 / x->a1 );
            else
                g_string_append_printf( t, "  complejo  a1 %.5f  a2 %.5f  d %.3f  frecuencia %.4f  "
                                        "periodo %.2f%s%s\n", x->a1, x->a2, x->r,
                                        x->omega / ( 2.0 * M_PI ), x->periodo,
                                        x->estacional ? "  estacional" : "",
                                        x->casi_unitaria ? "  casi unitario" : "" );
            }
        if ( json ) g_string_append( t, "]}" );
        }
    if ( json )
        g_string_append_printf( t, "], \"modulos_parecidos\": %s}\n",
                                f->modulos_parecidos ? "true" : "false" );
    fputs( t->str, stdout );
    g_string_free( t, TRUE );
    g_free( f );
    g_free( p );
    return 0;
}

/* raices factorizado P SERIE ID: el boton «Derivar el modelo factorizado…» */
static int raices_factorizado( const char *path, const char *serie, const char *id )
{
    char        origen[PR_RUTA], out[PR_RUTA], destino[PR_RUTA], nuevo[PR_ID], why[512], msg[512];
    Proyecto   *p = NULL;
    FueOut     *o;
    PrError     e;
    int         s = 1, partidos = 0, fijos = 0, desde_pre;
    gchar      *dir, *frase;

    if ( ruta_de( path, serie, id, ".pre", origen, sizeof origen, &p ) != 0 ) { g_free( p ); return 1; }
    desde_pre = g_file_test( origen, G_FILE_TEST_EXISTS );
    if ( !desde_pre && pr_ruta( p, serie, muestra, id, ".inp", origen, sizeof origen ) != 0 )
        { g_free( p ); return falla( "No pude componer la ruta." ); }
    o = g_new0( FueOut, 1 );
    if ( pr_ruta( p, serie, muestra, id, ".out", out, sizeof out ) == 0 && fueout_read( out, o ) &&
         o->s > 0 )
        s = o->s;
    g_free( o );
    if ( pr_deriva( p, serie, muestra, id, nuevo, sizeof nuevo, destino, sizeof destino, &e ) != 0 )
        { pr_error_es( &e, why, sizeof why ); g_free( p ); return falla( "%s", why ); }
    dir = g_path_get_dirname( destino );
    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );
    if ( rz_deriva_factorizado( origen, destino, s, &partidos, &fijos, why, sizeof why ) != 0 )
        { g_free( p ); return falla( "%s", why ); }
    /* EL JUEZ ES EL MOTOR, como en la madre */
    if ( inp_check_fue( destino, msg, sizeof msg ) != 0 )
        { g_unlink( destino ); g_free( p ); return falla( "Lo que salió no lo acepta fue: %s", msg ); }
    frase = g_strdup_printf( "Modelo factorizado: %s del AR regular partido%s en sus factores "
        "reales (1 − a B) y sus pares complejos (1 − a1 B − a2 B²), libres y sembrados con la "
        "factorización de %s.%s", partidos == 1 ? "el factor" : "los factores",
        partidos == 1 ? "" : "s", desde_pre ? "su óptimo" : "su especificación",
        fijos ? " Tenía coeficientes fijos: partido, esa restricción ya no está." : "" );
    pr_razon( p, serie, muestra, nuevo, razon && *razon ? razon : frase, &e );
    pon_por( p, serie, nuevo );
    if ( guarda( p, why, sizeof why ) != 0 )
        { g_free( frase ); g_free( p ); return falla( "%s", why ); }
    dice_modelo( serie, nuevo, destino, frase );
    g_free( frase );
    g_free( p );
    return 0;
}

/* ------------------------------------------------------------------------ */
/* coste: apuntar una corrida de fuera y sumar el registro                  */
/* ------------------------------------------------------------------------ */

/* La raiz del proyecto, que es donde vive costes.csv. */
static int raiz_de( const char *path, char *raiz, size_t n )
{
    char      why[512];
    Proyecto *p = abre( path, why, sizeof why );
    int       rc;

    if ( !p ) return falla( "%s", why );
    rc = pr_raiz( p, raiz, n );
    g_free( p );
    if ( rc != 0 ) return falla( "No pude saber la raíz del proyecto." );
    return 0;
}

/* Un numero de la orden, con punto; nada detras. */
static int numero_de( const char *opcion, const char *s, double *v )
{
    char *fin = NULL;

    if ( s == NULL ) return falla( "Falta %s.", opcion );
    *v = g_ascii_strtod( s, &fin );
    if ( fin == s || *fin != '\0' || !isfinite( *v ) )
        return falla( "%s %s: tiene que ser un número (con punto).", opcion, s );
    return 0;
}

/* coste apunta P: una corrida que no paso por lib/engine (el MTM, que corre
   en Python), al mismo registro y con el mismo escritor. */
static int coste_apunta_orden( const char *path )
{
    char       raiz[PR_RUTA], why[512];
    CosteLinea l;

    if ( raiz_de( path, raiz, sizeof raiz ) != 0 ) return 1;
    memset( &l, 0, sizeof l );
    if ( !c_motor || !*c_motor ) return falla( "Falta --motor: con qué programa se corrió." );
    if ( !c_paso || !*c_paso ) return falla( "Falta --paso: para qué se corrió." );
    if ( numero_de( "--cpu", c_cpu, &l.cpu_s ) != 0 ||
         numero_de( "--pared", c_pared, &l.pared_s ) != 0 ) return 1;
    l.energia_j = -1.0;
    if ( c_energia && numero_de( "--energia", c_energia, &l.energia_j ) != 0 ) return 1;
    l.fecha   = (char *) c_fecha;
    l.quien   = (char *) ( c_quien ? c_quien : "atsw-cli" );
    l.por     = (char *) ( c_por ? c_por : "analista" );
    l.objeto  = (char *) c_objeto;
    l.paso    = (char *) c_paso;
    l.motor   = (char *) c_motor;
    l.via     = (char *) c_via;
    l.metodo  = (char *) c_metodo;
    l.sha_pre = (char *) c_sha;
    if ( coste_apunta( raiz, &l, why, sizeof why ) != 0 ) return falla( "%s", why );
    if ( json )
        {
        GString *t = g_string_new( "{\"ok\": true, " );
        gchar   *f = g_build_filename( raiz, COSTE_FICHERO, NULL );
        gchar   *abs = g_canonicalize_filename( f, NULL );

        js_par( t, "fichero", abs ); g_string_append( t, "}\n" );
        fputs( t->str, stdout );
        g_string_free( t, TRUE );
        g_free( abs ); g_free( f );
        }
    else printf( "Apuntada en %s.\n", COSTE_FICHERO );
    return 0;
}

/* coste resumen P [--por-que G]: los totales, por grupo. */
static int coste_resumen_orden( const char *path )
{
    char        raiz[PR_RUTA], why[512];
    CosteTotal *t = NULL;
    GString    *s;
    int         n = 0, i;

    if ( raiz_de( path, raiz, sizeof raiz ) != 0 ) return 1;
    if ( coste_resumen( raiz, c_por_que, &t, &n, why, sizeof why ) != 0 )
        return falla( "%s", why );
    s = g_string_new( "" );
    if ( json )
        {
        g_string_append( s, "{\"ok\": true, " );
        js_par( s, "por_que", c_por_que && *c_por_que ? c_por_que : "total" );
        g_string_append( s, ", \"grupos\": [" );
        for ( i = 0; i < n; i++ )
            {
            g_string_append( s, i ? ", {" : "{" );
            js_par( s, "clave", t[i].clave );
            g_string_append_printf( s, ", \"corridas\": %d, ", t[i].corridas );
            js_num( s, "cpu_s", t[i].cpu_s ); g_string_append( s, ", " );
            js_num( s, "pared_s", t[i].pared_s ); g_string_append( s, ", " );
            js_num( s, "energia_j", t[i].energia_j ); g_string_append( s, "}" );
            }
        g_string_append( s, "]}\n" );
        }
    else if ( n == 0 )
        g_string_append( s, "No hay corridas apuntadas.\n" );
    else
        {
        g_string_append( s, "  " );
        col( s, c_por_que && *c_por_que ? c_por_que : "", 20 );
        g_string_append( s, "corridas      CPU (s)    reloj (s)   energía (J)\n" );
        for ( i = 0; i < n; i++ )
            {
            g_string_append( s, "  " );
            col( s, t[i].clave, 20 );
            g_string_append_printf( s, "%8d %12.3f %12.3f %13.3f\n", t[i].corridas,
                                    t[i].cpu_s, t[i].pared_s, t[i].energia_j );
            }
        }
    fputs( s->str, stdout );
    g_string_free( s, TRUE );
    coste_totales_libera( t, n );
    return 0;
}

/* ------------------------------------------------------------------------ */
/* ficha: la ficha de confianza (§7.8), la misma que la madre               */
/* ------------------------------------------------------------------------ */

static void js_corrida( GString *t, const char *k, const FiCorrida *c )
{
    js_cadena( t, k );
    if ( !c->consta ) { g_string_append( t, ": null" ); return; }
    g_string_append( t, ": {" );
    js_par( t, "fecha", c->fecha ); g_string_append( t, ", " );
    js_par( t, "motor", c->motor ); g_string_append( t, ", " );
    js_par( t, "via", c->via ); g_string_append( t, ", " );
    js_num( t, "cpu_s", c->cpu_s ); g_string_append( t, ", " );
    js_num( t, "pared_s", c->pared_s ); g_string_append( t, ", " );
    if ( c->energia_j >= 0.0 ) js_num( t, "energia_j", c->energia_j );
    else g_string_append( t, "\"energia_j\": null" );
    g_string_append( t, ", " );
    js_par( t, "metodo", c->metodo ); g_string_append( t, "}" );
}

/* LOS MOTORES DE SU ARBOL, como la madre (gui/atsw/src/gestos.c): primero
   al lado, en el arbol de compilacion o en el paquete; despues el PATH. */
static gchar *hermano( const char *programa )
{
    static const char *const sitio[] = {     /* relativos a gui/atsw-cli/ */
        "../../engines/fue/bin/%s",          /* el arbol de compilacion   */
        "../../engines/drtran/bin/%s",
        "../../engines/art/bin/%s",
        "../../engines/fuf/bin/%s",
        "../../engines/drvarma/bin/%s",      /* SIM: drvarma y drvec. Sin  */
        "../../engines/drvec/bin/%s",        /* ellos, el PATH da los de   */
                                             /* Python, que se llaman igual */
        "./%s", NULL                         /* el paquete: todo junto    */
    };
    return sitio_busca( programa, sitio );
}

/* ficha P SERIE MODELO [--comprobar]: con --comprobar, antes de la ficha se
   corre fue sobre el .pre y se apunta en costes.csv lo que salio. */
static int ficha( const char *path, const char *serie, const char *id )
{
    char      why[512], dif[512] = "";
    Proyecto *p = abre( path, why, sizeof why );
    Ficha    *f;
    gchar    *md;
    GString  *t;
    int       i, rep = -1;

    if ( !p ) return falla( "%s", why );
    if ( comprobar )
        {
        engine_set_finder( hermano );
        if ( fi_comprueba( p, serie, muestra, id, "atsw-cli",
                           c_por ? c_por : "analista", &rep, dif, sizeof dif ) != 0 )
            { g_free( p ); return falla( "%s", dif ); }
        if ( !rep && !json ) fprintf( stderr, "atsw-cli: no se reproduce: %s\n", dif );
        }
    f = g_new0( Ficha, 1 );
    if ( fi_ficha( p, serie, muestra, id, f, why, sizeof why ) != 0 )
        { g_free( f ); g_free( p ); return falla( "%s", why ); }
    md = fi_markdown( f );
    if ( !json ) fputs( md, stdout );
    else
        {
        t = g_string_new( "{\"ok\": true, " );
        js_par( t, "serie", f->serie ); g_string_append( t, ", " );
        js_par( t, "muestra", f->muestra ); g_string_append( t, ", " );
        js_par( t, "modelo", f->id ); g_string_append( t, ", " );
        js_par( t, "markdown", md );
        g_string_append( t, ", \"recorrido\": [" );
        for ( i = 0; i < f->npaso; i++ )
            {
            g_string_append( t, i ? ", {" : "{" );
            js_par( t, "id", f->paso[i].id ); g_string_append( t, ", " );
            js_par( t, "por", f->paso[i].datos ? "" : f->paso[i].por ); g_string_append( t, ", " );
            js_par( t, "razon", f->paso[i].razon );
            g_string_append_printf( t, ", \"datos\": %s}", f->paso[i].datos ? "true" : "false" );
            }
        g_string_append_printf( t, "], \"pasos\": %d, \"del_asistente\": %d, \"elegido\": %s, ",
                                f->pasos, f->del_asistente, f->elegido ? "true" : "false" );
        js_par( t, "por_elegido", f->por_elegido );
        g_string_append_printf( t, ", \"estimado\": %s, \"adecuacion\": [",
                                f->estimado ? "true" : "false" );
        for ( i = 0; f->estimado && i < f->dx.n; i++ )
            {
            g_string_append( t, i ? ", {" : "{" );
            js_par( t, "titulo", f->dx.l[i].titulo ); g_string_append( t, ", " );
            js_par( t, "estado", dx_estado_es( f->dx.l[i].estado ) ); g_string_append( t, ", " );
            js_par( t, "dato", f->dx.l[i].dato ); g_string_append( t, "}" );
            }
        g_string_append( t, "], \"reproducibilidad\": {" );
        js_par( t, "sha_pre", f->sha_pre );
        g_string_append_printf( t, ", \"se_reproduce\": %s, ",
                                !f->rep.consta ? "null" : f->reproduce ? "true" : "false" );
        js_corrida( t, "comprobacion", &f->rep ); g_string_append( t, ", " );
        js_corrida( t, "origen", &f->origen ); g_string_append( t, "}, \"coste\": {" );
        js_corrida( t, "estimacion", &f->estimacion );
        g_string_append_printf( t, ", \"recorrido\": {\"corridas\": %d, ", f->corridas );
        js_num( t, "cpu_s", f->cpu_s ); g_string_append( t, ", " );
        js_num( t, "pared_s", f->pared_s ); g_string_append( t, ", " );
        if ( f->con_energia ) js_num( t, "energia_j", f->energia_j );
        else g_string_append( t, "\"energia_j\": null" );
        g_string_append_printf( t, ", \"en_python\": %d}}", f->en_python );
        if ( comprobar )
            {
            g_string_append_printf( t, ", \"comprobado\": {\"se_reproduce\": %s, ",
                                    rep ? "true" : "false" );
            js_par( t, "diferencia", dif ); g_string_append( t, "}" );
            }
        g_string_append( t, "}\n" );
        fputs( t->str, stdout );
        g_string_free( t, TRUE );
        }
    g_free( md );
    g_free( f );
    g_free( p );
    return 0;
}

/* ------------------------------------------------------------------------ */
/* anomalos: cuanto de lo que se ve en el correlograma es el anomalo        */
/* ------------------------------------------------------------------------ */

static const char *veredicto_corto( AnVeredicto v )
{
    return v == AN_ENMASCARADA ? "enmascarada" : v == AN_FABRICADA ? "fabricada" : "igual";
}

/* anomalos P SERIE MODELO [--episodios 1,3|todos]: LO MISMO QUE LA VENTANA
   de la madre (an_anomalos), con las mismas funciones (lib/anomalos,
   an_residuos): los episodios de los residuos del .out y la ACF y la PACF
   con y sin los que se pidan, retardo a retardo, con su Ljung-Box y su
   normalidad. Sin --episodios, sin todos: «¿y si no hubiera anomalos?». La
   pregunta de verdad suele ser «¿y si no estuviera ESTE?», y es la que se
   acaba interviniendo.                                                 */
static int anomalos( const char *path, const char *serie, const char *id )
{
    char        out[PR_RUTA];
    Proyecto   *p = NULL;
    FueOut     *o;
    AnResiduos *r;
    AnCalibra   c;
    AnNormal    con, sinellos;
    int         marca[AN_MAX_EP], *omit, nomit, i, k;
    double      qc = 0.0, qs = 0.0;
    GString    *t;

    if ( ruta_de( path, serie, id, ".out", out, sizeof out, &p ) != 0 ) { g_free( p ); return 1; }
    o = g_new0( FueOut, 1 );
    r = g_new0( AnResiduos, 1 );
    if ( !fueout_read( out, o ) || an_residuos( o, r ) != 0 )
        { g_free( r ); g_free( o ); g_free( p );
          return falla( "«%s» no está estimado, o su .out no trae los residuos.", id ); }

    /* que episodios: todos, o los numeros que se digan (1..nep) */
    for ( i = 0; i < AN_MAX_EP; i++ ) marca[i] = episodios == NULL || !strcmp( episodios, "todos" );
    if ( episodios && strcmp( episodios, "todos" ) )
        {
        gchar **v = g_strsplit( episodios, ",", -1 );

        for ( i = 0; v[i]; i++ )
            {
            char *fin = NULL;
            long  e = strtol( v[i], &fin, 10 );

            if ( !*v[i] || ( fin && *fin ) || e < 1 || e > r->nep )
                {
                int nep = r->nep;
                g_strfreev( v ); g_free( r ); g_free( o ); g_free( p );
                return falla( "--episodios %s: son números de 1 a %d, separados por comas, "
                              "o «todos».", episodios, nep );
                }
            marca[e - 1] = 1;
            }
        g_strfreev( v );
        }
    omit  = g_new0( int, FO_MAX_RES );
    nomit = an_omite( r, marca, omit, FO_MAX_RES );
    an_calibra( o->res, o->nres, nomit ? omit : NULL, nomit, r->lags, &c );
    an_q( &c, c.nlags, &qc, &qs );
    if ( an_normalidad( r->z, o->nres, nomit ? omit : NULL, nomit, &con, &sinellos ) != 0 )
        { memset( &con, 0, sizeof con ); memset( &sinellos, 0, sizeof sinellos ); }

    t = g_string_new( "" );
    if ( json )
        {
        g_string_append( t, "{\"ok\": true, " );
        js_par( t, "serie", serie ); g_string_append( t, ", " );
        js_par( t, "muestra", muestra ); g_string_append( t, ", " );
        js_par( t, "modelo", id ); g_string_append( t, ", " );
        g_string_append_printf( t, "\"residuos\": %d, ", o->nres );
        js_num( t, "umbral", r->umbral );
        g_string_append_printf( t, ", \"freq\": %d, \"retardos\": %d, \"episodios\": [",
                                r->freq, c.nlags );
        for ( i = 0; i < r->nep; i++ )
            {
            const AnEpisodio *e = &r->ep[i];

            g_string_append_printf( t, "%s{\"n\": %d, ", i ? ", " : "", i + 1 );
            js_par( t, "desde", o->res_fecha[e->desde] ); g_string_append( t, ", " );
            js_par( t, "hasta", o->res_fecha[e->hasta] ); g_string_append( t, ", " );
            g_string_append_printf( t, "\"periodos\": %d, ", e->n );
            js_num( t, "z_max", e->z_max ); g_string_append( t, ", " );
            js_par( t, "fecha_z_max", o->res_fecha[e->i_max] ); g_string_append( t, ", " );
            js_num( t, "p", e->p );
            g_string_append_printf( t, ", \"omitido\": %s}", marca[i] ? "true" : "false" );
            }
        g_string_append_printf( t, "], \"omitidas\": %d, \"retardos_que_cambian\": %d, \"calibracion\": [",
                                nomit, c.cambia );
        for ( k = 0; k < c.nlags; k++ )
            {
            const AnLag *l = &c.l[k];

            g_string_append_printf( t, "%s{\"lag\": %d, ", k ? ", " : "", l->lag );
            js_num( t, "acf_con", l->acf_con ); g_string_append( t, ", " );
            js_num( t, "acf_sin", l->acf_sin ); g_string_append( t, ", " );
            js_num( t, "pacf_con", l->pacf_con ); g_string_append( t, ", " );
            js_num( t, "pacf_sin", l->pacf_sin ); g_string_append( t, ", " );
            js_par( t, "acf", veredicto_corto( l->acf ) ); g_string_append( t, ", " );
            js_par( t, "pacf", veredicto_corto( l->pacf ) ); g_string_append( t, "}" );
            }
        g_string_append( t, "], " );
        js_num( t, "banda_con", c.banda_con ); g_string_append( t, ", " );
        js_num( t, "banda_sin", c.banda_sin ); g_string_append( t, ", \"q\": {" );
        g_string_append_printf( t, "\"m\": %d, ", c.nlags );
        js_num( t, "con", qc ); g_string_append( t, ", " );
        js_num( t, "p_con", chisq_cola( qc, c.nlags ) ); g_string_append( t, ", " );
        js_num( t, "sin", qs ); g_string_append( t, ", " );
        js_num( t, "p_sin", chisq_cola( qs, c.nlags ) ); g_string_append( t, "}, \"jb\": {" );
        js_num( t, "con", con.jb ); g_string_append( t, ", " );
        js_num( t, "p_con", chisq_cola( con.jb, 2 ) ); g_string_append( t, ", " );
        js_num( t, "sin", sinellos.jb ); g_string_append( t, ", " );
        js_num( t, "p_sin", chisq_cola( sinellos.jb, 2 ) ); g_string_append( t, ", " );
        js_num( t, "asimetria_con", con.skew ); g_string_append( t, ", " );
        js_num( t, "curtosis_con", con.kurt ); g_string_append( t, ", " );
        js_num( t, "curtosis_sin", sinellos.kurt );
        g_string_append_printf( t, ", \"n_sin\": %d}}\n", sinellos.n );
        }
    else
        {
        g_string_append_printf( t, "Anómalos de %s/%s%s%s: %d episodio%s sobre %d residuos "
                                "(un tramo cuenta si es tan improbable como un extremo aislado "
                                "de |z| >= %.2f)\n", serie, id, *muestra ? ", muestra " : "",
                                muestra, r->nep, r->nep == 1 ? "" : "s", o->nres, r->umbral );
        for ( i = 0; i < r->nep; i++ )
            {
            const AnEpisodio *e = &r->ep[i];

            if ( e->n > 1 )
                g_string_append_printf( t, "  %d. %s-%s  (%d periodos, z máximo %+.2f en %s, p = %.2g)%s\n",
                                        i + 1, o->res_fecha[e->desde], o->res_fecha[e->hasta], e->n,
                                        e->z_max, o->res_fecha[e->i_max], e->p,
                                        marca[i] ? "  [omitido]" : "" );
            else
                g_string_append_printf( t, "  %d. %s  (z %+.2f, p = %.2g)%s\n", i + 1,
                                        o->res_fecha[e->desde], e->z_max, e->p,
                                        marca[i] ? "  [omitido]" : "" );
            }
        if ( r->nep == 0 )
            g_string_append( t, "  Ningún residuo pasa del umbral.\n" );
        else
            {
            int hay = 0;

            g_string_append_printf( t, "\nSin %s (%d observaciones de %d):\n",
                                    nomit && episodios && strcmp( episodios, "todos" )
                                    ? "los omitidos" : "los episodios", c.n_sin, c.n_con );
            for ( k = 0; k < c.nlags; k++ )
                {
                const AnLag *l = &c.l[k];

                if ( l->acf != AN_IGUAL )
                    { hay++; g_string_append_printf( t, "  ACF %2d  %+.3f -> %+.3f  %s\n", l->lag,
                                                     l->acf_con, l->acf_sin, an_veredicto_es( l->acf ) ); }
                if ( l->pacf != AN_IGUAL )
                    { hay++; g_string_append_printf( t, "  PACF %2d %+.3f -> %+.3f  %s\n", l->lag,
                                                     l->pacf_con, l->pacf_sin, an_veredicto_es( l->pacf ) ); }
                }
            g_string_append( t, hay ? "  -> quitar esto cambiaría la identificación.\n"
                                    : "  Ningún retardo cambia de lado de la banda: intervenir esto "
                                      "no compra nada para la identificación.\n" );
            }
        g_string_append_printf( t, "\n  Q(%d)  con %.1f (p = %.3f)   sin %.1f (p = %.3f)   banda ±%.3f / ±%.3f\n",
                                c.nlags, qc, chisq_cola( qc, c.nlags ), qs, chisq_cola( qs, c.nlags ),
                                c.banda_con, c.banda_sin );
        g_string_append_printf( t, "  JB    con %.1f (p = %.3f)   sin %.1f (p = %.3f)   curtosis %+.2f -> %+.2f\n",
                                con.jb, chisq_cola( con.jb, 2 ), sinellos.jb,
                                chisq_cola( sinellos.jb, 2 ), con.kurt, sinellos.kurt );
        }
    fputs( t->str, stdout );
    g_string_free( t, TRUE );
    g_free( omit ); g_free( r ); g_free( o ); g_free( p );
    return 0;
}

/* ------------------------------------------------------------------------ */
/* prewhiten: la CCF preblanqueada que identifica (b, s), sin ventana       */
/* ------------------------------------------------------------------------ */

/* lib/prewhiten y diagnose.c son los del motor drtran, y piden estos dos
   globales, como en drtran_gui (identifica.c) y su prueba.              */
#define PW_MAX_LAGS 64          /* como IDENT_MAX_LAGS de drtran_gui */
real  macheps = 2.220446049250313e-16;
FILE *outputv = NULL;

typedef struct {
    struct Tusmodel tm;
    struct Tseries  ts;
    real          **dm;
    int             leido;
} PreLeido;

/* SERIE/MODELO del proyecto -> su .pre, leido con el lector de la suite. */
static int lee_pre( Proyecto *p, const char *clave, PreLeido *x, char *ruta, size_t nr,
                    char *why, size_t n )
{
    char serie[PR_ID], id[PR_ID];
    const char *barra = strchr( clave, '/' );

    memset( x, 0, sizeof *x );
    if ( !barra || barra == clave || !barra[1] || (size_t) ( barra - clave ) >= sizeof serie )
        { g_snprintf( why, n, "«%s»: es SERIE/MODELO.", clave ); return 1; }
    g_snprintf( serie, sizeof serie, "%.*s", (int) ( barra - clave ), clave );
    g_snprintf( id, sizeof id, "%s", barra + 1 );
    if ( pr_modelo_idx( p, serie, muestra, id ) < 0 )
        { g_snprintf( why, n, "«%s» no es un modelo del proyecto en esa muestra.", clave ); return 1; }
    if ( pr_ruta( p, serie, muestra, id, ".pre", ruta, nr ) != 0 ||
         !g_file_test( ruta, G_FILE_TEST_EXISTS ) )
        { g_snprintf( why, n, "«%s» no tiene .pre: hay que estimarlo antes.", clave ); return 1; }
    if ( read_fue_pre( ruta, &x->tm, &x->ts, &x->dm ) != 0 )
        { g_snprintf( why, n, "No pude leer %s.", ruta ); return 1; }
    x->leido = 1;
    return 0;
}

static void suelta_pre( PreLeido *x )
{
    if ( x->leido ) free_fue_pre( &x->tm, &x->ts, x->dm );
    x->leido = 0;
}

/* prewhiten P SALIDA/MODELO ENTRADA/MODELO... [--retardos N]: LO MISMO QUE
   LA PANTALLA DE IDENTIFICACION de drtran_gui, con las mismas funciones: la
   entrada se preblanquea con su ARMA y la salida con el mismo filtro
   (prewhiten_ccf), el portmanteau del motor sobre los preblanqueados
   (hosking_test) y lo que se lee de la CCF (prewhiten_lee). Una entrada por
   fila: identificar es comparar cuales merecen estar en la red.        */
static int prewhiten( const char *path, char **cl, int ncl )
{
    char      why[512], ruta[PR_RUTA];
    Proyecto *p = abre( path, why, sizeof why );
    PreLeido *sal, *ent;
    GString  *t;
    int       j;

    if ( !p ) return falla( "%s", why );
    if ( ncl < 2 ) { g_free( p ); return falla( "prewhiten: la salida y al menos una entrada." ); }
    sal = g_new0( PreLeido, 1 );
    ent = g_new0( PreLeido, 1 );
    if ( lee_pre( p, cl[0], sal, ruta, sizeof ruta, why, sizeof why ) != 0 )
        { g_free( ent ); g_free( sal ); g_free( p ); return falla( "%s", why ); }

    t = g_string_new( "" );
    if ( json )
        {
        g_string_append( t, "{\"ok\": true, " );
        js_par( t, "salida", cl[0] ); g_string_append( t, ", \"entradas\": [" );
        }
    else
        g_string_append_printf( t, "Identificación de %s: la CCF preblanqueada de cada entrada\n", cl[0] );

    for ( j = 1; j < ncl; j++ )
        {
        double  ccf[2 * PW_MAX_LAGS + 1], nu[2 * PW_MAX_LAGS + 1], banda = 0.0;
        int     n = 0, nl, b = -1, s = -1, kmax = 0, neg = 0, k, vale = 0;
        real  **res = NULL, Q = 0, pv = 0;
        char    mot[512] = "";

        if ( lee_pre( p, cl[j], ent, ruta, sizeof ruta, mot, sizeof mot ) == 0 )
            {
            nl = retardos > 0 ? retardos : prewhiten_nlags( sal->ts.nobs );
            if ( nl > PW_MAX_LAGS ) nl = PW_MAX_LAGS;
            if ( prewhiten_ccf( &ent->tm, &ent->ts, ent->dm, &sal->tm, &sal->ts, sal->dm,
                                nl, ccf, nu, &n, &res, mot, sizeof mot ) == 0 )
                {
                hosking_test( res, n, 2, nl, &Q, &pv );
                free_matrix( res, 1, n, 1, 2 );
                banda = 2.0 / sqrt( (double) n );
                prewhiten_lee( ccf, nl, banda, &b, &s, &kmax, &neg );
                vale = 1;
                }
            suelta_pre( ent );
            }
        else nl = 0;

        if ( json )
            {
            g_string_append_printf( t, "%s{", j > 1 ? ", " : "" );
            js_par( t, "entrada", cl[j] );
            g_string_append_printf( t, ", \"vale\": %s", vale ? "true" : "false" );
            if ( !vale ) { g_string_append( t, ", " ); js_par( t, "motivo", mot ); g_string_append( t, "}" ); continue; }
            if ( mot[0] ) { g_string_append( t, ", " ); js_par( t, "aviso", mot ); }
            g_string_append_printf( t, ", \"n\": %d, \"retardos\": %d, ", n, nl );
            js_num( t, "banda", banda );
            g_string_append_printf( t, ", \"b\": %d, \"s\": %d, \"kmax\": %d, \"negativos_fuera\": %d, ",
                                    b, s, kmax, neg );
            js_num( t, "hosking", (double) Q );
            g_string_append_printf( t, ", \"gl\": %d, ", 4 * nl );
            js_num( t, "p", (double) pv );
            g_string_append( t, ", \"ccf\": [" );
            for ( k = -nl; k <= nl; k++ )
                {
                g_string_append_printf( t, "%s{\"k\": %d, ", k > -nl ? ", " : "", k );
                js_num( t, "r", ccf[nl + k] ); g_string_append( t, ", " );
                js_num( t, "nu", nu[nl + k] ); g_string_append( t, "}" );
                }
            g_string_append( t, "]}" );
            }
        else
            {
            g_string_append_printf( t, "\n  %s", cl[j] );
            if ( !vale ) { g_string_append_printf( t, ": no se pudo preblanquear: %s\n", mot ); continue; }
            if ( b < 0 )
                g_string_append_printf( t, ": ningún retardo k >= 0 sale de la banda (±%.3f sobre %d obs)\n",
                                        banda, n );
            else
                g_string_append_printf( t, ": b = %d, s = %d, pico en k = %d (banda ±%.3f sobre %d obs)\n",
                                        b, s, kmax, banda, n );
            if ( neg == 0 )
                g_string_append( t, "    exogeneidad: ningún retardo negativo fuera de la banda\n" );
            else
                g_string_append_printf( t, "    exogeneidad: %d retardo%s negativo%s fuera: la salida "
                                        "antecede a la entrada, y eso no lo arregla (b, s)\n",
                                        neg, neg == 1 ? "" : "s", neg == 1 ? "" : "s" );
            g_string_append_printf( t, "    Hosking sobre los preblanqueados: P(%d) = %.1f (p = %.4f)\n",
                                    4 * nl, (double) Q, (double) pv );
            g_string_append( t, "    k fuera de banda:" );
            for ( k = -nl; k <= nl; k++ )
                if ( fabs( ccf[nl + k] ) > banda ) g_string_append_printf( t, " %d (%+.3f)", k, ccf[nl + k] );
            g_string_append( t, "\n" );
            if ( mot[0] ) g_string_append_printf( t, "    aviso: %s\n", mot );
            }
        }
    if ( json ) g_string_append( t, "]}\n" );
    fputs( t->str, stdout );
    g_string_free( t, TRUE );
    suelta_pre( sal );
    g_free( ent ); g_free( sal ); g_free( p );
    return 0;
}

/* ------------------------------------------------------------------------ */
/* formales: los contrastes formales del modelo estimado (lib/formales)     */
/* ------------------------------------------------------------------------ */

/* El directorio de trabajo de los restringidos: en la cache, como la madre
   (an_fichero), con su propio prefijo para no pisar una ventana abierta. */
static gchar *dir_formales( const char *serie, const char *id )
{
    gchar *base = g_strdup_printf( "formales_%s_%s_%s", serie,
                                   *muestra ? muestra : "completa", id ), *s, *d;

    for ( s = base; *s; s++ )
        if ( !g_ascii_isalnum( *s ) && *s != '_' && *s != '-' ) *s = '_';
    d = g_build_filename( g_get_user_cache_dir(), "atsw", base, NULL );
    g_free( base );
    return d;
}

/* Lo que dice, en una frase: lo mismo que la ventana, sin decidir nada. */
static const char *lo_que_dice( const FmPrueba *x )
{
    if ( x->omitida || !x->calculable ) return "no calculable";
    if ( x->tipo == FM_MEG ) return x->rechaza ? "estocástica" : "determinista";
    return x->rechaza ? "rechaza H0 al 5 %" : "no rechaza H0 al 5 %";
}

/* La clave del contraste, en ASCII: para un programa, que no tiene por que
   comparar cadenas con tildes (en Windows llegan cambiadas por la linea de
   ordenes).                                                            */
static const char *clave_fm( FmTipo tipo )
{
    switch ( tipo )
        {
        case FM_ESTACIONAL: return "estacional";
        case FM_SF:         return "sf";
        case FM_DCD:        return "dcd";
        case FM_DCD_S:      return "dcd_s";
        case FM_SOBRE:      return "sobre";
        case FM_SUB:        return "sub";
        case FM_MEG:        return "meg";
        case FM_DCD_F:      return "dcd_f";
        }
    return "?";
}

static void js_prueba( GString *t, const FmPrueba *x )
{
    g_string_append( t, "{" );
    js_par( t, "clave", clave_fm( x->tipo ) ); g_string_append( t, ", " );
    js_par( t, "familia", fm_familia( x ) == FM_FAM_INTEGRACION ? "integracion"
                                                                : "estacionalidad" );
    g_string_append( t, ", " ); js_par( t, "contraste", fm_nombre( x->tipo ) );
    g_string_append( t, ", " ); js_par( t, "hipotesis", x->hipotesis );
    g_string_append_printf( t, ", \"factor\": %d, \"freq\": %d, \"gl\": %d", x->factor,
                            x->freq, x->gl );
    g_string_append( t, ", " ); js_num( t, "coef", x->coef );
    g_string_append( t, ", " ); js_num( t, "nulo", x->nulo );
    g_string_append_printf( t, ", \"calculable\": %s, \"omitida\": %s",
                            x->calculable && !x->omitida ? "true" : "false",
                            x->omitida ? "true" : "false" );
    g_string_append( t, ", " ); js_par( t, "motivo", x->motivo );
    g_string_append( t, ", " ); js_num( t, "l_libre", x->l_libre );
    g_string_append( t, ", " ); js_num( t, "l_restringido", x->l_restr );
    g_string_append( t, ", " ); js_num( t, "estadistico", x->est );
    g_string_append( t, ", " ); js_num( t, "critico_10", x->c10 );
    g_string_append( t, ", " ); js_num( t, "critico_5", x->c5 );
    g_string_append( t, ", " ); js_num( t, "critico_1", x->c1 );
    g_string_append( t, ", " ); js_num( t, "p", x->p );
    g_string_append_printf( t, ", \"rechaza\": %s, \"al_otro_lado\": %s, \"dudoso\": %s",
                            x->rechaza ? "true" : "false", x->al_otro_lado ? "true" : "false",
                            x->dudoso ? "true" : "false" );
    g_string_append( t, ", " ); js_par( t, "dice", lo_que_dice( x ) );
    if ( x->tipo == FM_SOBRE || x->tipo == FM_SUB || x->tipo == FM_MEG )
        {
        g_string_append( t, ", " ); js_num( t, "l_testigo", x->l_testigo );
        g_string_append( t, ", " ); js_par( t, "testigo", x->testigo );
        }
    if ( x->tipo == FM_SUB )
        {
        g_string_append( t, ", " ); js_num( t, "phi_restringido", x->phi_r );
        g_string_append( t, ", " ); js_num( t, "rho_m", x->rho_m );
        g_string_append_printf( t, ", \"acotado\": %s", x->acotado ? "true" : "false" );
        }
    g_string_append( t, "}" );
}

static void tx_prueba( GString *t, const FmPrueba *x )
{
    g_string_append_printf( t, "\n  %s", fm_nombre( x->tipo ) );
    if ( x->freq > 0 ) g_string_append_printf( t, ", f = %d", x->freq );
    g_string_append_printf( t, ": %s\n    H0: %s\n", lo_que_dice( x ), x->hipotesis );
    if ( x->omitida || !x->calculable )
        { g_string_append_printf( t, "    %s\n", x->motivo ); return; }
    g_string_append_printf( t, "    L = %.4f, L0 = %.4f, ", x->l_libre, x->l_restr );
    if ( x->tipo == FM_ESTACIONAL )
        g_string_append_printf( t, "LR = %.4f con %d gl, p = %.4f\n", x->est, x->gl, x->p );
    else
        g_string_append_printf( t, "%s = %.4f; críticos 10 %% %.3f, 5 %% %.3f, 1 %% %.3f\n",
                                x->tipo == FM_SF ? "Phi" : "LR", x->est, x->c10, x->c5, x->c1 );
    if ( x->tipo != FM_ESTACIONAL && isfinite( x->coef ) )
        g_string_append_printf( t, "    estimado %.6f, valor nulo %.6f\n", x->coef, x->nulo );
    if ( x->testigo[0] ) g_string_append_printf( t, "    testigo: %s\n", x->testigo );
    if ( x->al_otro_lado ) g_string_append( t, "    la raíz dominante ya está al otro lado de rho_m: Phi = 0\n" );
    if ( x->dudoso )
        g_string_append( t, "    alguna estimación acabó sin anular el gradiente, aun reiniciando: "
                            "el estadístico hereda esa duda\n" );
}

/* formales P SERIE MODELO [--familia integracion|estacionalidad] [--sub]:
   LAS MISMAS FUNCIONES que las dos ventanas de la madre (an_formales):
   fm_plan escribe los restringidos, fue los estima de uno en uno --aqui
   esperando, alli sin bloquear-- y fm_avanza los lee. Cada reestimacion
   queda en costes.csv. AQUI NO SE DECIDE NI SE ENCADENA NADA.          */
static int formales( const char *path, const char *serie, const char *id )
{
    char      out[PR_RUTA], pre[PR_RUTA], raiz[PR_RUTA], aviso[256] = "", why[256];
    Proyecto *p = NULL;
    FueOut   *o;
    FmPlan   *pl;
    Dictamen  d;
    gchar    *dir, *obj;
    GString  *t;
    int       i, fam = -1;

    if ( familia )
        {
        if ( !strcmp( familia, "integracion" ) ) fam = FM_FAM_INTEGRACION;
        else if ( !strcmp( familia, "estacionalidad" ) ) fam = FM_FAM_ESTACIONAL;
        else return falla( "--familia %s: es integracion o estacionalidad.", familia );
        }
    if ( ruta_de( path, serie, id, ".out", out, sizeof out, &p ) != 0 ) { g_free( p ); return 1; }
    if ( pr_ruta( p, serie, muestra, id, ".pre", pre, sizeof pre ) != 0 ||
         !g_file_test( pre, G_FILE_TEST_EXISTS ) )
        { g_free( p ); return falla( "«%s» no tiene .pre: hay que estimarlo antes de contrastar nada.", id ); }
    o = g_new0( FueOut, 1 );
    if ( !fueout_read( out, o ) || !o->tiene_logelf )
        { g_free( o ); g_free( p );
          return falla( "El .out de «%s» no trae su log-verosimilitud (logelf).", id ); }
    if ( fam == FM_FAM_ESTACIONAL && o->s <= 1 )
        { int s = o->s; g_free( o ); g_free( p );
          return falla( "«%s» no es estacional (s = %d): no hay estacionalidad que contrastar.", id, s ); }

    /* LA DIAGNOSIS PRIMERO (art BUG-0025): no se impide, se dice. */
    if ( fi_dictamen( p, serie, muestra, id, &d ) == 0 && d.peor == DX_NO )
        g_snprintf( aviso, sizeof aviso, "La diagnosis no está limpia: estos contrastes suponen "
                    "un modelo adecuado; léelos con esa reserva, o arregla antes el modelo." );

    dir = dir_formales( serie, id );
    g_mkdir_with_parents( dir, 0700 );
    pl = g_new0( FmPlan, 1 );
    if ( fm_plan( pre, out, dir, o->nobs, o->s > 0 ? o->s : 1, o->d, pl, why, sizeof why ) != 0 )
        { g_free( pl ); g_free( dir ); g_free( o ); g_free( p ); return falla( "%s", why ); }
    if ( fam >= 0 ) fm_quedate_con( pl, (FmFamilia) fam );
    /* EL LADO d - 1 SE PIDE, NO SE OFRECE (art BUG-0167) */
    if ( con_sub && fam != FM_FAM_ESTACIONAL && fm_plan_sub( pl, why, sizeof why ) != 0 )
        fprintf( stderr, "atsw-cli: subdiferenciación: %s\n", why );

    engine_set_finder( hermano );
    raiz_de( path, raiz, sizeof raiz );
    obj = *muestra ? g_strdup_printf( "%s/%s/%s", serie, muestra, id )
                   : g_strdup_printf( "%s/%s", serie, id );
    for ( i = 0; i < pl->n; i++ )
        {
        FmPrueba *x = &pl->t[i];

        while ( !x->hecho )
            {
            EngineResult r;
            gchar       *viejo = g_strdup( x->inp );
            int          corrio;

            /* el .out de una vuelta anterior, fuera: el que se lea sera este */
            strcpy( viejo + strlen( viejo ) - 4, ".out" );
            g_unlink( viejo );
            g_free( viejo );
            coste_contexto( raiz, "atsw-cli", c_por ? c_por : "analista", obj,
                            x->tipo == FM_MEG ? "meg" : "contraste" );
            r = engine_run( dir, "fue", x->base, NULL );
            coste_contexto_limpia();
            corrio = r.status == 0;
            if ( r.status == ENGINE_NORUN )
                {
                while ( fm_avanza( pl, x, o->logelf, FALSE ) ) ;
                g_snprintf( x->motivo, sizeof x->motivo, "no pude lanzar fue: %s", r.message );
                engine_result_clear( &r );
                break;
                }
            engine_result_clear( &r );
            if ( !fm_avanza( pl, x, o->logelf, corrio ) ) break;
            }
        }
    g_free( obj );

    t = g_string_new( "" );
    if ( json )
        {
        g_string_append( t, "{\"ok\": true, " );
        js_par( t, "serie", serie ); g_string_append( t, ", " );
        js_par( t, "muestra", muestra ); g_string_append( t, ", " );
        js_par( t, "modelo", id ); g_string_append( t, ", " );
        g_string_append_printf( t, "\"nobs\": %d, \"s\": %d, \"d\": %d, ", pl->nobs, pl->s, pl->d );
        js_num( t, "l_libre", o->logelf ); g_string_append( t, ", " );
        js_par( t, "aviso", aviso ); g_string_append( t, ", " );
        js_par( t, "directorio", dir ); g_string_append( t, ", \"contrastes\": [" );
        for ( i = 0; i < pl->n; i++ )
            { if ( i ) g_string_append( t, ", " ); js_prueba( t, &pl->t[i] ); }
        g_string_append( t, "], \"no_aplica\": [" );
        {
        int k, n = 0;

        for ( k = 0; k < FM_NA; k++ )
            if ( pl->no_aplica[k][0] )
                { if ( n++ ) g_string_append( t, ", " ); js_cadena( t, pl->no_aplica[k] ); }
        }
        g_string_append( t, "]}\n" );
        }
    else
        {
        int f2;

        g_string_append_printf( t, "Contrastes formales de %s/%s%s%s (%d observaciones, s = %d, d = %d)\n",
                                serie, id, *muestra ? ", muestra " : "", muestra,
                                pl->nobs, pl->s, pl->d );
        if ( aviso[0] ) g_string_append_printf( t, "\n  ! %s\n", aviso );
        for ( f2 = FM_FAM_INTEGRACION; f2 <= FM_FAM_ESTACIONAL; f2++ )
            {
            int hay = 0, k;

            for ( i = 0; i < pl->n; i++ ) hay |= (int) fm_familia( &pl->t[i] ) == f2;
            if ( fam >= 0 && fam != f2 ) continue;
            g_string_append_printf( t, "\n%s\n", f2 == FM_FAM_INTEGRACION
                                    ? "ORDEN DE INTEGRACIÓN" : "ESTACIONALIDAD" );
            for ( i = 0; i < pl->n; i++ )
                if ( (int) fm_familia( &pl->t[i] ) == f2 ) tx_prueba( t, &pl->t[i] );
            if ( !hay ) g_string_append( t, "\n  (ninguno)\n" );
            for ( k = 0; k < FM_NA; k++ )
                if ( pl->no_aplica[k][0] &&
                     ( ( k == 1 || k == 2 || k == 4 || k == 5 ) == ( f2 == FM_FAM_INTEGRACION ) ) )
                    g_string_append_printf( t, "  (%s)\n", pl->no_aplica[k] );
            }
        g_string_append( t, "\nCada contraste dice lo suyo; qué hacer con ello lo decide el analista.\n" );
        }
    fputs( t->str, stdout );
    g_string_free( t, TRUE );
    g_free( pl ); g_free( dir ); g_free( o ); g_free( p );
    return 0;
}

/* ------------------------------------------------------------------------ */
/* especifica y estima: lo que el asistente hace con un modelo nuevo        */
/* ------------------------------------------------------------------------ */

/* UN MODELO QUE SE PUEDE TOCAR: el que no es de datos y no esta estimado.
   Lo estimado no se reescribe (su .out y su .pre lo describen; la fase 5 de
   AUDITORIA-procesos): de el se deriva un hijo, con «proyecto itera».   */
static int tocable( Proyecto *p, const char *serie, const char *id, char *inp, size_t n )
{
    char out[PR_RUTA];

    if ( pr_es_datos( p, serie, muestra, id ) )
        return falla( "%s/%s son los datos: no se tocan. Deriva un modelo con «proyecto nuevo».",
                      serie, id );
    if ( pr_ruta( p, serie, muestra, id, ".out", out, sizeof out ) == 0 &&
         g_file_test( out, G_FILE_TEST_EXISTS ) )
        return falla( "%s/%s ya está estimado: no se reescribe. Deriva un hijo con «proyecto itera».",
                      serie, id );
    if ( pr_ruta( p, serie, muestra, id, ".inp", inp, n ) != 0 || !g_file_test( inp, G_FILE_TEST_EXISTS ) )
        return falla( "%s/%s no tiene .inp.", serie, id );
    return 0;
}

/* «a,b,c,d» -> cuatro enteros. 0 si cupo. */
static int cuatro( const char *s, int *a, int *b, int *c, int *d )
{
    return s && sscanf( s, "%d,%d,%d,%d", a, b, c, d ) == 4 && *a >= 0 && *b >= 0 && *c >= 0 && *d >= 0
           ? 0 : 1;
}

/* LOS ARMONICOS Y SEMANA SANTA, como deterministas que fue lee por su nombre
   («cos k», «sin k», «alter», «easter»): N pares k = 1..N, o «todos» (los
   pares hasta s/2 - 1 y, con s par, «alter», la de Nyquist), como el paquete
   estacional de art. Solo con D = 0: con la diferencia anual la
   estacionalidad ya va en ella. Los que el .inp ya trae no se repiten.
   Deja en nuevo[] (memoria de quien llama) los que hay que anadir.       */
static int armonicos_nuevos( const char *inp, const char *val, int pascua,
                             char nuevo[][ID_LINEA], int *nn, char *why, size_t n )
{
    InpFile f;
    char    hay[ID_MAX_DET][ID_LINEA], cual[ID_LINEA];
    int     nh, s, D, maxp, np, alter = 0, k, j, ya;

    *nn = 0;
    memset( &f, 0, sizeof f );
    if ( inp_read( inp, &f, why, n ) != 0 ) return 1;
    s = f.freq; D = f.nadiff;
    inp_free( &f );
    if ( ( nh = id_nombres( inp, hay, ID_MAX_DET, why, n ) ) < 0 ) return 1;
    maxp = s % 2 ? ( s - 1 ) / 2 : s / 2 - 1;
    if ( val )
        {
        if ( s < 2 ) { g_snprintf( why, n, "--armonicos: la serie es anual (s = %d), no hay estacionalidad que poner.", s ); return 1; }
        if ( D > 0 ) { g_snprintf( why, n, "--armonicos con D = %d: la estacionalidad ya va en la diferencia anual. "
                                   "Para la ruta determinista, D = 0 (--transformacion λ,d,0).", D ); return 1; }
        if ( !strcmp( val, "todos" ) ) { np = maxp; alter = ( s % 2 == 0 ); }
        else
            {
            char *fin = NULL;
            np = (int) strtol( val, &fin, 10 );
            if ( !fin || *fin || np < 1 || np > maxp )
                { g_snprintf( why, n, "--armonicos %s: es un número de pares, de 1 a %d con s = %d, o «todos».", val, maxp, s ); return 1; }
            }
        for ( k = 1; k <= np + alter; k++ )
            for ( j = 0; j < 2; j++ )
                {
                if ( k > np ) { if ( j ) break; g_snprintf( cual, sizeof cual, "alter" ); }
                else g_snprintf( cual, sizeof cual, "%s %d", j ? "sin" : "cos", k );
                for ( ya = 0; ya < nh && strcmp( hay[ya], cual ); ya++ ) ;
                if ( ya == nh && *nn < ID_MAX_DET ) g_snprintf( nuevo[(*nn)++], ID_LINEA, "%s", cual );
                }
        }
    if ( pascua )
        {
        if ( s != 12 ) { g_snprintf( why, n, "--pascua: Semana Santa es para series mensuales (s = %d).", s ); return 1; }
        for ( ya = 0; ya < nh && strcmp( hay[ya], "easter" ); ya++ ) ;
        if ( ya == nh && *nn < ID_MAX_DET ) g_snprintf( nuevo[(*nn)++], ID_LINEA, "easter" );
        }
    if ( *nn == 0 ) { g_snprintf( why, n, "El .inp ya trae %s.", val && pascua ? "esos armónicos y Semana Santa"
                                  : pascua ? "Semana Santa" : "esos armónicos" ); return 1; }
    return 0;
}

/* especifica P SERIE MODELO [--transformacion L,d,D] [--arma p,q,P,Q]
   [--anade-arma p,q,P,Q] [--intervencion "step 10 2008"]... [--media libre|V]:
   EL MISMO EDITOR DEL .inp QUE LAS VENTANAS DE LA MADRE (lib/inpdet), que
   reescribe solo lo que toca y deja lo demas byte a byte. Cada paso va a un
   temporal; el resultado pasa por la puerta del motor (inp_check_fue) antes
   de sustituir el .inp. Si algo falla, el .inp queda como estaba.      */
static int especifica( const char *path, const char *serie, const char *id )
{
    char      why[512], inp[PR_RUTA], msg[512];
    Proyecto *p = NULL;
    gchar    *a, *b, *cur;
    int       pa, qa, Pa, Qa, cambios = 0;
    GString  *hecho = g_string_new( "" ), *t;

    p = abre( path, why, sizeof why );
    if ( !p ) { g_string_free( hecho, TRUE ); return falla( "%s", why ); }
    if ( pr_modelo_idx( p, serie, muestra, id ) < 0 )
        { g_free( p ); g_string_free( hecho, TRUE ); return falla( "«%s» no es un modelo de %s.", id, serie ); }
    if ( tocable( p, serie, id, inp, sizeof inp ) != 0 ) { g_free( p ); g_string_free( hecho, TRUE ); return 1; }
    a = g_strdup_printf( "%s.esp1.tmp", inp );
    b = g_strdup_printf( "%s.esp2.tmp", inp );
    cur = g_strdup( inp );

#define PASO( llamada, que ) do { \
        const char *dest = ( cambios % 2 ) ? b : a; \
        if ( llamada != 0 ) goto mal; \
        g_free( cur ); cur = g_strdup( dest ); cambios++; \
        g_string_append_printf( hecho, "%s%s", hecho->len ? "; " : "", que ); } while ( 0 )

    if ( e_transf )
        {
        double lam; int d, D;
        if ( sscanf( e_transf, "%lf,%d,%d", &lam, &d, &D ) != 3 || d < 0 || D < 0 )
            { g_snprintf( why, sizeof why, "--transformacion %s: es λ,d,D (0,1,0 es log y una diferencia).", e_transf ); goto mal; }
        PASO( id_pon_transformacion( cur, ( cambios % 2 ) ? b : a, lam, d, D, why, sizeof why ),
              ( g_snprintf( msg, sizeof msg, "transformación λ = %g, d = %d, D = %d", lam, d, D ), msg ) );
        }
    if ( e_arma )
        {
        if ( cuatro( e_arma, &pa, &qa, &Pa, &Qa ) )
            { g_snprintf( why, sizeof why, "--arma %s: es p,q,P,Q.", e_arma ); goto mal; }
        PASO( id_pon_arma( cur, ( cambios % 2 ) ? b : a, pa, NULL, qa, NULL, Pa, NULL, Qa, NULL, why, sizeof why ),
              ( g_snprintf( msg, sizeof msg, "ARMA (%d,%d)(%d,%d), desde cero", pa, qa, Pa, Qa ), msg ) );
        }
    if ( e_anade )
        {
        if ( cuatro( e_anade, &pa, &qa, &Pa, &Qa ) )
            { g_snprintf( why, sizeof why, "--anade-arma %s: es p,q,P,Q.", e_anade ); goto mal; }
        PASO( id_anade_arma( cur, ( cambios % 2 ) ? b : a, pa, NULL, qa, NULL, Pa, NULL, Qa, NULL, why, sizeof why ),
              ( g_snprintf( msg, sizeof msg, "un factor más (%d,%d)(%d,%d)", pa, qa, Pa, Qa ), msg ) );
        }
    if ( e_armon || e_pascua )
        {
        char        nv[ID_MAX_DET][ID_LINEA];
        const char *pn[ID_MAX_DET];
        int         nn = 0, k;

        if ( armonicos_nuevos( cur, e_armon, e_pascua, nv, &nn, why, sizeof why ) != 0 ) goto mal;
        for ( k = 0; k < nn; k++ ) pn[k] = nv[k];
        g_snprintf( msg, sizeof msg, "%d determinista%s estacional%s (", nn, nn == 1 ? "" : "s", nn == 1 ? "" : "es" );
        for ( k = 0; k < nn; k++ ) g_strlcat( msg, k ? ", " : "", sizeof msg ), g_strlcat( msg, nv[k], sizeof msg );
        g_strlcat( msg, ")", sizeof msg );
        PASO( id_anade( cur, ( cambios % 2 ) ? b : a, pn, NULL, nn, why, sizeof why ), msg );
        }
    if ( e_ninterv > 0 )
        {
        g_snprintf( msg, sizeof msg, "%d intervención%s", e_ninterv, e_ninterv == 1 ? "" : "es" );
        PASO( id_anade( cur, ( cambios % 2 ) ? b : a, e_interv, NULL, e_ninterv, why, sizeof why ), msg );
        }
    if ( e_media )
        {
        int    libre = !strcmp( e_media, "libre" );
        double v = 0.0;
        char  *fin = NULL;

        if ( !libre ) { v = g_ascii_strtod( e_media, &fin ); if ( !fin || *fin )
            { g_snprintf( why, sizeof why, "--media %s: es «libre» o un número (fija).", e_media ); goto mal; } }
        PASO( id_fija_media( cur, ( cambios % 2 ) ? b : a, v, libre ? 1 : 0, why, sizeof why ),
              libre ? "media libre" : ( g_snprintf( msg, sizeof msg, "media fija en %g", v ), msg ) );
        }
#undef PASO
    if ( cambios == 0 )
        { g_snprintf( why, sizeof why, "No se ha pedido ningún cambio (--transformacion, --arma, "
                      "--anade-arma, --armonicos, --pascua, --intervencion, --media)." ); goto mal; }
    /* EL JUEZ ES EL MOTOR */
    if ( inp_check_fue( cur, msg, sizeof msg ) != 0 )
        { g_snprintf( why, sizeof why, "Lo que salió no lo acepta fue: %s", msg ); goto mal; }
    if ( g_rename( cur, inp ) != 0 )
        {
        /* en Windows rename no reemplaza: se quita el viejo y se pone */
        g_unlink( inp );
        if ( g_rename( cur, inp ) != 0 ) { g_snprintf( why, sizeof why, "No pude sustituir %s.", inp ); goto mal; }
        }
    g_unlink( a ); g_unlink( b );

    {
    int p2 = 0, q2 = 0, P2 = 0, Q2 = 0;

    id_arma_ordenes( inp, &p2, &q2, &P2, &Q2, msg, sizeof msg );
    t = g_string_new( "" );
    if ( json )
        {
        g_string_append( t, "{\"ok\": true, " );
        js_par( t, "serie", serie ); g_string_append( t, ", " );
        js_par( t, "modelo", id ); g_string_append( t, ", " );
        js_par( t, "inp", inp ); g_string_append( t, ", " );
        js_par( t, "cambios", hecho->str );
        g_string_append_printf( t, ", \"arma\": [%d, %d, %d, %d]}\n", p2, q2, P2, Q2 );
        }
    else
        g_string_append_printf( t, "%s/%s: %s. El .inp queda con ARMA (%d,%d)(%d,%d); fue lo acepta.\n"
                                "  %s\n", serie, id, hecho->str, p2, q2, P2, Q2, inp );
    fputs( t->str, stdout );
    g_string_free( t, TRUE );
    }
    g_free( cur ); g_free( a ); g_free( b ); g_free( p ); g_string_free( hecho, TRUE );
    return 0;
mal:
    g_unlink( a ); g_unlink( b );
    g_free( cur ); g_free( a ); g_free( b ); g_free( p ); g_string_free( hecho, TRUE );
    {
    size_t l = strlen( why );
    return falla( "%s%s El .inp queda como estaba.", why, l && why[l - 1] == '.' ? "" : "." );
    }
}

/* estima P SERIE MODELO: fue sobre el .inp del modelo, por lib/engine --sin
   consola, con plazo y con su linea en costes.csv--, y lo que dice el .out:
   la log-verosimilitud y el dictamen. Lo estimado no se reestima.       */
static int estima( const char *path, const char *serie, const char *id )
{
    char         why[512], inp[PR_RUTA], out[PR_RUTA], raiz[PR_RUTA];
    Proyecto    *p = abre( path, why, sizeof why );
    EngineResult r;
    gchar       *dir, *base, *obj;
    Dictamen     d;
    FueOut      *o;
    GString     *t;
    int          hay_dx;

    if ( !p ) return falla( "%s", why );
    if ( pr_modelo_idx( p, serie, muestra, id ) < 0 )
        { g_free( p ); return falla( "«%s» no es un modelo de %s.", id, serie ); }
    if ( tocable( p, serie, id, inp, sizeof inp ) != 0 ) { g_free( p ); return 1; }
    pr_ruta( p, serie, muestra, id, ".out", out, sizeof out );
    dir  = g_path_get_dirname( inp );
    base = g_path_get_basename( inp );
    base[strlen( base ) - 4] = '\0';
    obj  = *muestra ? g_strdup_printf( "%s/%s/%s", serie, muestra, id )
                    : g_strdup_printf( "%s/%s", serie, id );
    engine_set_finder( hermano );
    raiz_de( path, raiz, sizeof raiz );
    coste_contexto( raiz, "atsw-cli", c_por ? c_por : "analista", obj, "estimar" );
    r = engine_run( dir, "fue", base, NULL );
    coste_contexto_limpia();
    g_free( obj ); g_free( base ); g_free( dir );

    if ( !engine_wrote_results( &r ) )
        {
        gchar *m = g_strdup( r.message );
        engine_result_clear( &r ); g_free( p );
        falla( "%s", m ); g_free( m );
        return 1;
        }
    o = g_new0( FueOut, 1 );
    fueout_read( out, o );
    hay_dx = fi_dictamen( p, serie, muestra, id, &d ) == 0;
    t = g_string_new( "" );
    if ( json )
        {
        g_string_append( t, "{\"ok\": true, " );
        js_par( t, "serie", serie ); g_string_append( t, ", " );
        js_par( t, "modelo", id ); g_string_append( t, ", " );
        g_string_append_printf( t, "\"estado\": %d, ", r.status );
        js_par( t, "mensaje", r.message ); g_string_append( t, ", " );
        if ( o->tiene_logelf ) js_num( t, "logelf", o->logelf );
        else g_string_append( t, "\"logelf\": null" );
        g_string_append( t, ", " );
        js_par( t, "dictamen", hay_dx ? dx_estado_es( d.peor ) : "" ); g_string_append( t, ", " );
        js_par( t, "out", out ); g_string_append( t, "}\n" );
        }
    else
        {
        g_string_append_printf( t, "%s/%s: %s\n", serie, id, r.message );
        if ( o->tiene_logelf ) g_string_append_printf( t, "  log-verosimilitud %.6f\n", o->logelf );
        if ( hay_dx ) g_string_append_printf( t, "  diagnosis: %s (atsw-cli dictamen para el detalle)\n",
                                              dx_estado_es( d.peor ) );
        g_string_append_printf( t, "  %s\n", out );
        }
    fputs( t->str, stdout );
    g_string_free( t, TRUE );
    engine_result_clear( &r );
    g_free( o ); g_free( p );
    return 0;
}

/* ------------------------------------------------------------------------ */
/* identifica: lo que art propone, sin ventana                              */
/* ------------------------------------------------------------------------ */

static gchar *art_hermano( void )
{
    static const char *const sitio[] = { "../../engines/art/bin/%s", "./%s", NULL };
    return sitio_busca( "art", sitio );
}

/* identifica P SERIE [--transformacion L,d,D] [--residuos MODELO]: LO MISMO
   QUE LA VENTANA DE IDENTIFICACION de la madre, con las mismas funciones
   (lib/artcand: ac_escribe_datos, ac_corre, ac_leer). Sobre los datos de la
   serie --el nodo de datos de la muestra-- con la transformacion pedida (sin
   ella, la del .inp de los datos, hecha una que art acepte: log o niveles,
   d hasta 2, D hasta 1), o sobre los residuos de un modelo estimado (lo que
   les falta). Propone; no deriva ni escribe nada en el proyecto.       */
static int identifica( const char *path, const char *serie )
{
    char      why[512], ruta[PR_RUTA];
    Proyecto *p = abre( path, why, sizeof why );
    ArtCand  *c;
    gchar    *exe, *dir, *datos, *base;
    double    lam = 1.0;
    int       d = 0, D = 0, freq = 1, per = 0, anio = 0, n = 0, sin = 0, i, k;
    double   *x = NULL;
    GString  *t;

    if ( !p ) return falla( "%s", why );
    if ( pr_serie_idx( p, serie ) < 0 ) { g_free( p ); return falla( "«%s» no es una serie del proyecto.", serie ); }
    if ( i_resid )
        {
        FueOut *o = g_new0( FueOut, 1 );

        if ( pr_modelo_idx( p, serie, muestra, i_resid ) < 0 ||
             pr_ruta( p, serie, muestra, i_resid, ".out", ruta, sizeof ruta ) != 0 ||
             !fueout_read( ruta, o ) || o->nres < 20 )
            { g_free( o ); g_free( p );
              return falla( "%s/%s no está estimado, o su .out no trae los residuos.", serie, i_resid ); }
        freq = o->s;
        for ( i = 0; i < o->nres; i++ )
            {
            int pp = 0, a = 0;
            if ( fo_fecha_parte( o->res_fecha[i], &pp, &a ) == 0 && pp > freq && pp <= 52 ) freq = pp;
            }
        if ( freq < 1 ) freq = 1;
        fo_fecha_parte( o->res_fecha[0], &per, &anio );
        n = o->nres;
        x = g_memdup2( o->res, (gsize) n * sizeof *x );
        g_free( o );
        }
    else
        {
        const char *datos_id = pr_datos_de( p, serie, muestra );
        InpFile     f;
        int         positiva = 1;

        if ( !*datos_id || pr_ruta( p, serie, muestra, datos_id, ".inp", ruta, sizeof ruta ) != 0 ||
             inp_read( ruta, &f, why, sizeof why ) != 0 )
            { g_free( p ); return falla( "%s no tiene datos en esa muestra.", serie ); }
        n = f.nobs; freq = f.freq > 0 ? f.freq : 1; per = f.begtime; anio = f.begyear;
        x = g_memdup2( f.data, (gsize) n * sizeof *x );
        for ( i = 0; i < n; i++ ) if ( !( x[i] > 0.0 ) ) positiva = 0;
        lam = ( f.boxlam == 0.0 && positiva ) ? 0.0 : 1.0;
        d = f.nrdiff < 0 ? 0 : ( f.nrdiff > 2 ? 2 : f.nrdiff );
        D = ( freq > 1 && f.nadiff > 0 ) ? 1 : 0;
        inp_free( &f );
        if ( e_transf )
            {
            if ( sscanf( e_transf, "%lf,%d,%d", &lam, &d, &D ) != 3 || ( lam != 0.0 && lam != 1.0 ) ||
                 d < 0 || D < 0 )
                { g_free( x ); g_free( p );
                  return falla( "--transformacion %s: art identifica en logaritmos (λ = 0) o en niveles "
                                "(λ = 1): es 0,d,D o 1,d,D.", e_transf ); }
            if ( lam == 0.0 && !positiva )
                { g_free( x ); g_free( p ); return falla( "La serie tiene valores no positivos: no hay logaritmo." ); }
            }
        }
    if ( n < 10 ) { g_free( x ); g_free( p ); return falla( "Muy pocas observaciones (%d).", n ); }

    dir = g_build_filename( g_get_user_cache_dir(), "atsw", NULL );
    g_mkdir_with_parents( dir, 0700 );
    base = g_strdup_printf( "identifica_%s_%s_%s", serie, *muestra ? muestra : "completa",
                            i_resid ? i_resid : "datos" );
    for ( i = 0; base[i]; i++ ) if ( !g_ascii_isalnum( base[i] ) && base[i] != '_' && base[i] != '-' ) base[i] = '_';
    datos = g_strdup_printf( "%s%c%s.txt", dir, G_DIR_SEPARATOR, base );
    g_free( base ); g_free( dir );
    exe = art_hermano();
    c = g_new0( ArtCand, 1 );
    if ( ac_escribe_datos( datos, x, n, freq, per, anio ) != 0 )
        { g_snprintf( why, sizeof why, "No pude escribir %s.", datos ); goto mal; }
    if ( ac_corre( exe, datos, freq, lam, d, D, i_resid != NULL, c, &sin, why, sizeof why ) != 0 )
        goto mal;

    t = g_string_new( "" );
    if ( json )
        {
        g_string_append( t, "{\"ok\": true, " );
        js_par( t, "serie", serie ); g_string_append( t, ", " );
        js_par( t, "sobre", i_resid ? "residuos" : "datos" ); g_string_append( t, ", " );
        if ( i_resid ) { js_par( t, "modelo", i_resid ); g_string_append( t, ", " ); }
        g_string_append_printf( t, "\"lambda\": %g, \"d\": %d, \"D\": %d, \"s\": %d, \"n\": %d, "
                                "\"sin_armonicos\": %s, ", lam, d, D, c->s, c->n_used, sin ? "true" : "false" );
        js_num( t, "banda", c->band );
        if ( c->has_seasonal )
            { g_string_append_printf( t, ", \"estacional\": {\"detectada\": %s, ", c->seasonal_detected ? "true" : "false" );
              js_num( t, "F", c->seasonal_F ); g_string_append( t, ", " ); js_num( t, "p", c->seasonal_p );
              g_string_append( t, "}" ); }
        if ( c->has_unit_root )
            { g_string_append( t, ", \"adf\": {" ); js_num( t, "estadistico", c->adf_stat );
              g_string_append( t, ", " ); js_num( t, "p", c->adf_p ); g_string_append( t, "}, \"kpss\": {" );
              js_num( t, "estadistico", c->kpss_stat ); g_string_append( t, ", " ); js_num( t, "p", c->kpss_p );
              g_string_append( t, "}" ); }
        g_string_append( t, ", \"candidatos\": [" );
        for ( k = 0; k < c->ncand; k++ )
            {
            const AcCand *a = &c->cand[k];
            char kind[64];

            g_string_append_printf( t, "%s{\"arma\": [%d, %d, %d, %d], ", k ? ", " : "", a->p, a->q, a->P, a->Q );
            js_par( t, "modelo", ac_kind( a, kind, sizeof kind ) ); g_string_append( t, ", " );
            js_num( t, "similitud", a->sim ); g_string_append( t, ", " );
            js_num( t, "peso", a->weight ); g_string_append( t, ", " );
            if ( a->scored ) js_num( t, "aicc", a->aicc ); else g_string_append( t, "\"aicc\": null" );
            g_string_append_printf( t, ", \"propuesto\": %s}", a->proposed ? "true" : "false" );
            }
        g_string_append( t, "], \"avisos\": [" );
        for ( k = 0; k < c->nmsg; k++ ) { if ( k ) g_string_append( t, ", " ); js_cadena( t, c->msg[k] ); }
        g_string_append( t, "]}\n" );
        }
    else
        {
        g_string_append_printf( t, "Identificación de %s%s%s, %s (λ = %g, d = %d, D = %d; s = %d, %d obs, banda ±%.3f)%s\n",
                                serie, i_resid ? ": residuos de " : "", i_resid ? i_resid : "",
                                i_resid ? "lo que les falta" : "los datos", lam, d, D, c->s, c->n_used, c->band,
                                sin ? "\n  sin armónicos: la F no detecta estacionalidad" : "" );
        if ( c->has_seasonal )
            g_string_append_printf( t, "  estacionalidad: F = %.3f (p = %.4f), %s\n", c->seasonal_F, c->seasonal_p,
                                    c->seasonal_detected ? "detectada" : "no detectada" );
        if ( c->has_unit_root )
            g_string_append_printf( t, "  raíz unitaria: ADF %.3f (p = %.4f), KPSS %.3f (p = %.4f)\n",
                                    c->adf_stat, c->adf_p, c->kpss_stat, c->kpss_p );
        g_string_append( t, "  candidatos, en el orden de art (el primero es su propuesta):\n" );
        for ( k = 0; k < c->ncand; k++ )
            {
            const AcCand *a = &c->cand[k];
            char kind[64];

            char aicc[32];

            if ( a->scored ) g_snprintf( aicc, sizeof aicc, "AICc %.2f", a->aicc );
            else             g_snprintf( aicc, sizeof aicc, "sin puntuar" );
            g_string_append_printf( t, "    %d. (%d,%d)(%d,%d)  %-22s similitud %.3f  peso %.3f  %s%s\n",
                                    k + 1, a->p, a->q, a->P, a->Q, ac_kind( a, kind, sizeof kind ), a->sim,
                                    a->weight, aicc, a->proposed ? "  <- propuesto" : "" );
            }
        for ( k = 0; k < c->nmsg; k++ ) g_string_append_printf( t, "  aviso: %s\n", c->msg[k] );
        g_string_append( t, "Propone; no deriva nada. El modelo se hace con «proyecto nuevo» y «especifica».\n" );
        }
    fputs( t->str, stdout );
    g_string_free( t, TRUE );
    g_free( c ); g_free( exe ); g_free( datos ); g_free( x ); g_free( p );
    return 0;
mal:
    g_free( c ); g_free( exe ); g_free( datos ); g_free( x ); g_free( p );
    return falla( "%s", why );
}

/* ------------------------------------------------------------------------ */
/* caso-estima: una corrida de un caso de transferencia, con drtran         */
/* ------------------------------------------------------------------------ */

static int copia_fichero( const char *de, const char *a )
{
    gchar *c = NULL;
    gsize  n = 0;
    int    r = g_file_get_contents( de, &c, &n, NULL ) && g_file_set_contents( a, c, (gssize) n, NULL );

    g_free( c );
    return r ? 0 : 1;
}

/* caso-estima P CASO [--enlace ENTRADA->SALIDA:b,r,s]... [--desde cNN]
   [--diagonal] [--razon R]: LO QUE HACE «Estimar» EN drtran_gui, sin ventana.
   Una corrida nueva del caso (hija de --desde), con su red: la de los
   enlaces dados --nombres de serie del proyecto--, o la de la corrida
   --desde, o ninguna (con una entrada, drtran identifica b, r y s solo).
   drtran por lib/engine, con su coste. Si no deja resultados, la corrida se
   retira (AUDITORIA-procesos, fase 4): no queda en el linaje una
   estimacion que no existe. Un caso desfasado --un .pre cambio desde el
   alta-- no se estima: se crea otro con esos .pre.                     */
/* LA TABLA DE PARAMETROS DEL .out DEL MOTOR (drtran o drvarma), TAL CUAL:
   la log-verosimilitud de drtran, cada slot con su nombre --omega1[0],
   phi_2[B^1], AR1[MINK<-MUSKRAT], Q[MINK,MUSKRAT]--, su estimacion, su error
   tipico, su t y su p, y los FIJOS como tales; y la linea del hessiano. Es lo
   que el asistente presenta y de donde salen los nombres de las
   restricciones (--restriccion) y de los ceros (--cero). Se copia, no se
   rehace. drtran la cierra con rayas; drvarma, con «Standard errors».   */
static int tabla_motor( const char *out, GString *t )
{
    FILE *f = g_fopen( out, "r" );
    char  l[512];
    int   dentro = 0, n = 0, tras = 0;

    if ( !f ) return 1;
    while ( fgets( l, sizeof l, f ) )
        {
        l[strcspn( l, "\r\n" )] = '\0';
        if ( !dentro && !strncmp( l, "Log-likelihood =", 16 ) ) { g_string_append_printf( t, "  %s\n", l ); continue; }
        if ( !dentro && strstr( l, "Parameter" ) && strstr( l, "Std.Error" ) )
            { dentro = 1; g_string_append_printf( t, "  %s\n", l ); continue; }
        if ( !dentro ) continue;
        if ( strstr( l, "Standard errors:" ) ) { g_string_append_printf( t, "  %s\n", l ); break; }
        if ( dentro == 1 )
            {
            if ( !strncmp( l, "-----", 5 ) ) { if ( n ) dentro = 2; continue; }
            if ( !l[0] ) { if ( n ) break; continue; }
            g_string_append_printf( t, "  %s\n", l ); n++;
            continue;
            }
        if ( ++tras > 3 ) break;          /* dentro == 2: tras las rayas */
        }
    fclose( f );
    return n ? 0 : 1;
}

/* ------------------------------------------------------------------------ */
/* Los casos de SIM (motor drvarma): la puerta y la estimacion VARMA, sin    */
/* ventana, con lib/sistema --la misma orden que arma sim_gui-- y            */
/* lib/outvarma (docs/DISENO-sim.md; sima run_gate y estimate)              */
/* ------------------------------------------------------------------------ */

/* El sistema del caso: sus .pre en el orden del manifiesto (el de Cholesky),
   comprobados contra el sha del alta. Un caso desfasado no se estima por
   debajo, como en caso-estima.                                          */
static int sis_del_caso( Proyecto *p, const PrCaso *c, Sistema *sis, char *why, size_t n )
{
    int i;

    memset( sis, 0, sizeof *sis );
    for ( i = 0; i < c->nen; i++ )
        {
        char   pre[PR_RUTA];
        gchar *h;

        if ( pr_ruta( p, c->en[i].serie, c->muestra, c->en[i].modelo, ".pre", pre, sizeof pre ) != 0 ||
             !g_file_test( pre, G_FILE_TEST_EXISTS ) )
            { g_snprintf( why, n, "%s/%s ya no tiene .pre.", c->en[i].serie, c->en[i].modelo ); return 1; }
        h = ge_sha( pre );
        if ( c->en[i].sha[0] && ( !h || strcmp( h, c->en[i].sha ) ) )
            { g_free( h );
              g_snprintf( why, n, "%s está desfasado: cambió el .pre de %s/%s desde el alta. Crea un caso nuevo "
                          "con esos modelos.", c->id, c->en[i].serie, c->en[i].modelo ); return 1; }
        g_free( h );
        if ( sis_anade( sis, pre, why, n ) != 0 ) return 1;
        }
    if ( sis->n < 2 ) { g_snprintf( why, n, "Un sistema cruza dos series o más." ); return 1; }
    return 0;
}

static const PrCaso *caso_sis( Proyecto *p, const char *caso, char *why, size_t n )
{
    const PrCaso *c = pr_caso_ver( p, caso );

    if ( !c ) { g_snprintf( why, n, "«%s» no es un caso del proyecto.", caso ); return NULL; }
    if ( strcmp( c->motor, "drvarma" ) )
        { g_snprintf( why, n, "%s es de %s (MT): se estima con caso-estima, sin la puerta de SIM.", caso, c->motor );
          return NULL; }
    return c;
}

/* caso-puerta P CASO: N1 de sima (run_gate). El sistema diagonal --p = q = 0,
   Sigma diagonal-- tiene que reproducir la suma de las univariantes: si no,
   nada de lo que se estime encima se puede creer (drvarma sale con 5). No es
   una corrida: es un artefacto del caso, <C>_puerta.out, como en sim_gui.
   Devuelve el bloque de la puerta tal cual y, de las CCF de los residuos del
   diagonal --las innovaciones de cada serie--, los retardos fuera de banda
   de cada par: la evidencia para los enlaces (N2, identify_cross).        */
static int caso_puerta( const char *path, const char *caso )
{
    char          why[1024], base[PR_RUTA], out[PR_RUTA], raiz[PR_RUTA], l[512];
    Proyecto     *p = abre( path, why, sizeof why );
    const PrCaso *c;
    Sistema      *sis;
    SisVentana    v;
    VarmaOut     *o;
    EngineResult  r;
    const char   *av[SIS_MAX_SER + 8];
    GString      *t, *bloque = g_string_new( "" );
    gchar        *dir, *obj;
    FILE         *f;
    int           k, i, j, dentro = 0, hay;

    if ( !p ) { g_string_free( bloque, TRUE ); return falla( "%s", why ); }
    if ( !( c = caso_sis( p, caso, why, sizeof why ) ) )
        { g_free( p ); g_string_free( bloque, TRUE ); return falla( "%s", why ); }
    sis = g_new0( Sistema, 1 );
    if ( sis_del_caso( p, c, sis, why, sizeof why ) != 0 || sis_ventana( sis, &v, why, sizeof why ) != 0 )
        { g_free( sis ); g_free( p ); g_string_free( bloque, TRUE ); return falla( "%s", why ); }
    pr_corrida_ruta( p, caso, "puerta", NULL, base, sizeof base );
    g_snprintf( out, sizeof out, "%s.out", base );
    dir = g_path_get_dirname( base );
    g_mkdir_with_parents( dir, 0700 );
    g_remove( out );
    k = sis_orden_puerta( sis, base, av, G_N_ELEMENTS( av ) - 1 );
    av[k] = NULL;
    engine_set_finder( hermano );
    raiz_de( path, raiz, sizeof raiz );
    obj = g_strdup_printf( "%s/puerta", caso );
    coste_contexto( raiz, "atsw-cli", c_por ? c_por : "analista", obj, "puerta" );
    r = engine_run_v( dir, "drvarma", av );
    coste_contexto_limpia();
    g_free( dir ); g_free( obj );

    o = g_new0( VarmaOut, 1 );
    hay = g_file_test( out, G_FILE_TEST_EXISTS ) && ov_parse_file( out, o ) == 0 && o->puerta.hay;
    if ( !hay )
        {
        gchar *m = g_strdup( r.message );
        engine_result_clear( &r ); g_free( o ); g_free( sis ); g_free( p ); g_string_free( bloque, TRUE );
        falla( "%s No hay puerta.", m ); g_free( m );
        return 1;
        }
    /* el bloque de la puerta, tal cual: desde «THE DIAGONAL GATE» hasta la
       primera linea en blanco despues del veredicto                       */
    if ( ( f = g_fopen( out, "r" ) ) )
        {
        while ( fgets( l, sizeof l, f ) )
            {
            l[strcspn( l, "\r\n" )] = '\0';
            if ( !dentro && strstr( l, "THE DIAGONAL GATE" ) ) dentro = 1;
            if ( !dentro ) continue;
            if ( dentro == 2 && !l[0] ) break;
            if ( strstr( l, "GATE:" ) ) dentro = 2;
            if ( l[0] && strncmp( l, "=====", 5 ) ) g_string_append_printf( bloque, "%s\n", l );
            }
        fclose( f );
        }

    t = g_string_new( "" );
    if ( json )
        {
        g_string_append( t, "{\"ok\": true, " );
        js_par( t, "caso", caso ); g_string_append( t, ", " );
        g_string_append_printf( t, "\"pasa\": %s, \"nobs\": %d, ", o->puerta.pasa ? "true" : "false", o->nobs );
        js_num( t, "suma", o->puerta.suma ); g_string_append( t, ", " );
        js_num( t, "conjunta", o->puerta.conjunta ); g_string_append( t, ", \"series\": [" );
        for ( i = 0; i < o->puerta.ns; i++ )
            {
            const OvPuertaSerie *x = &o->puerta.s[i];
            g_string_append( t, i ? ", {" : "{" );
            js_par( t, "serie", x->nombre ); g_string_append( t, ", " );
            js_num( t, "logl", x->logl ); g_string_append( t, ", " );
            js_num( t, "mov", x->mov );
            g_string_append_printf( t, ", \"recortada\": %s, \"no_optimo\": %s}",
                                    x->recortada ? "true" : "false", x->no_optimo ? "true" : "false" );
            }
        g_string_append( t, "], \"ccf\": [" );
        }
    else
        {
        g_string_append_printf( t, "%s: la puerta del sistema diagonal (p = q = 0, Σ diagonal), %d observaciones "
                                "estacionarias comunes\n\n%s\n", caso, o->nobs, bloque->str );
        g_string_append( t, o->puerta.pasa
            ? "La puerta pasa: el sistema diagonal reproduce las univariantes. Se puede estimar encima.\n"
            : "La puerta NO pasa: nada de lo que se estime encima se puede creer. Revisa los univariantes.\n" );
        if ( o->nccf > 0 )
            g_string_append( t, "\nLas CCF de los residuos del diagonal (las innovaciones de cada serie), retardos "
                                "fuera de banda:\n  k > 0: B entra en la ecuación de A («A<-B»); k < 0: «B<-A»; "
                                "k = 0: contemporánea (Σ completa)\n" );
        }
    for ( i = 0; i < o->nccf; i++ )
        {
        const OvCcf *x = &o->ccf[i];
        GString *pos = g_string_new( "" ), *neg = g_string_new( "" );
        int cero = fabs( x->r[x->nlags] ) > x->banda;

        for ( j = 1; j <= x->nlags; j++ )
            {
            if ( fabs( x->r[x->nlags + j] ) > x->banda ) g_string_append_printf( pos, "%s%d", pos->len ? "," : "", j );
            if ( fabs( x->r[x->nlags - j] ) > x->banda ) g_string_append_printf( neg, "%s%d", neg->len ? "," : "", -j );
            }
        if ( json )
            {
            g_string_append( t, i ? ", {" : "{" );
            js_par( t, "a", x->a ); g_string_append( t, ", " );
            js_par( t, "b", x->b ); g_string_append( t, ", " );
            js_num( t, "banda", x->banda ); g_string_append( t, ", " );
            js_par( t, "k_pos", pos->str ); g_string_append( t, ", " );
            js_par( t, "k_neg", neg->str );
            g_string_append_printf( t, ", \"k0\": %s, ", cero ? "true" : "false" );
            js_num( t, "q_gt0", x->q_gt0 ); g_string_append( t, ", " );
            js_num( t, "q_lt0", x->q_lt0 ); g_string_append_printf( t, ", \"df\": %d}", x->df_gt0 );
            }
        else
            g_string_append_printf( t, "  %s, %s (banda %.3f): k > 0 [%s] (%s<-%s); k < 0 [%s] (%s<-%s); k = 0 %s;"
                                    " Q(k>0) = %.2f, Q(k<0) = %.2f con %d g.l.\n",
                                    x->a, x->b, x->banda, pos->len ? pos->str : "ninguno", x->a, x->b,
                                    neg->len ? neg->str : "ninguno", x->b, x->a, cero ? "fuera" : "dentro",
                                    x->q_gt0, x->q_lt0, x->df_gt0 );
        g_string_free( pos, TRUE ); g_string_free( neg, TRUE );
        }
    if ( json ) { g_string_append( t, "], " ); js_par( t, "bloque", bloque->str ); g_string_append( t, "}\n" ); }
    fputs( t->str, stdout );
    k = o->puerta.pasa ? 0 : 1;
    g_string_free( t, TRUE ); g_string_free( bloque, TRUE );
    engine_result_clear( &r ); g_free( o ); g_free( sis ); g_free( p );
    return k;
}

/* caso-estima de un caso de SIM: N3 de sima (estimate), un VARMA como corrida
   del caso. --p N --q N (la dinamica cruzada), --diagcov, --enlace "A<-B"
   (sin ninguno, todos los pares), --cero "AR1[A<-B]" (se repite). Antes, la
   puerta superada. La corrida se guarda solo si drvarma deja resultados,
   como en caso-estima.                                                     */
static int caso_estima_sis( const char *path, const char *caso )
{
    char          why[1024], out[PR_RUTA], base[PR_RUTA], pu[PR_RUTA], raiz[PR_RUTA], id[PR_ID];
    char          buf[4096];
    Proyecto     *p = abre( path, why, sizeof why );
    const PrCaso *c;
    Sistema      *sis;
    SisVentana    v;
    SisModelo    *mo;
    VarmaOut     *o;
    EngineResult  r;
    PrError       e;
    const char   *av[SIS_MAX_SER + 32];
    GString      *t, *enl;
    gchar        *dir, *obj;
    int           k, ok;

    if ( !p ) return falla( "%s", why );
    if ( !( c = caso_sis( p, caso, why, sizeof why ) ) ) { g_free( p ); return falla( "%s", why ); }
    if ( k_p < 0 || k_q < 0 )
        { int r0 = falla( "%s es un caso de SIM (drvarma): se estima desde atsw-sis, y un VARMA lleva --p N --q N, "
                          "la dinámica cruzada (p = 1, q = 0 para empezar).", caso );
          g_free( p ); return r0; }
    if ( k_nrestr > 0 || k_prever > 0 || k_evaluar > 0 )
        { g_free( p ); return falla( "En un caso de SIM, --restriccion, --prever y --evaluar llegan con el siguiente "
                                     "paso; aquí, --p --q --diagcov --enlace --cero." ); }
    sis = g_new0( Sistema, 1 );
    if ( sis_del_caso( p, c, sis, why, sizeof why ) != 0 || sis_ventana( sis, &v, why, sizeof why ) != 0 )
        { g_free( sis ); g_free( p ); return falla( "%s", why ); }
    /* LA PUERTA, ANTES (sim_gui, estima_lanzar) */
    o = g_new0( VarmaOut, 1 );
    pr_corrida_ruta( p, caso, "puerta", ".out", pu, sizeof pu );
    if ( !g_file_test( pu, G_FILE_TEST_EXISTS ) || ov_parse_file( pu, o ) != 0 || !o->puerta.hay || !o->puerta.pasa )
        { g_free( o ); g_free( sis ); g_free( p );
          return falla( "Antes, la puerta superada (caso-puerta): si el sistema diagonal no reproduce las "
                        "univariantes, nada de lo que se estime encima se puede creer." ); }
    mo = g_new0( SisModelo, 1 );
    sis_modelo_defecto( mo, sis->n );
    mo->p = k_p; mo->q = k_q; mo->diagcov = k_diagcov;
    enl = g_string_new( "" );
    for ( k = 0; k < k_nenlace; k++ )
        {
        if ( !strstr( k_enlace[k], "<-" ) || strchr( k_enlace[k], ',' ) )
            { g_string_free( enl, TRUE ); g_free( mo ); g_free( o ); g_free( sis ); g_free( p );
              return falla( "--enlace %s: en un caso de SIM es «A<-B» (B entra en la ecuación de A), uno por "
                            "--enlace.", k_enlace[k] ); }
        g_string_append_printf( enl, "%s%s", enl->len ? "," : "", k_enlace[k] );
        }
    if ( enl->len && sis_enlaces_de_texto( sis, mo, enl->str ) != 0 )
        { int r0 = falla( "--enlace %s: las series tienen que ser del caso.", enl->str );
          g_string_free( enl, TRUE ); g_free( mo ); g_free( o ); g_free( sis ); g_free( p ); return r0; }
    g_string_free( enl, TRUE );
    for ( k = 0; k < k_ncero; k++ )
        {
        if ( strlen( mo->zero ) + strlen( k_cero[k] ) + 2 >= sizeof mo->zero ) break;
        if ( mo->zero[0] ) g_strlcat( mo->zero, ",", sizeof mo->zero );
        g_strlcat( mo->zero, k_cero[k], sizeof mo->zero );
        }
    if ( k_desde && pr_corrida_idx( p, caso, k_desde ) < 0 )
        { g_free( mo ); g_free( o ); g_free( sis ); g_free( p ); return falla( "«%s» no es un modelo de %s.", k_desde, caso ); }
    if ( pr_corrida_nueva( p, caso, k_desde ? k_desde : "", id, sizeof id, out, sizeof out, &e ) != 0 )
        { pr_error_es( &e, why, sizeof why ); g_free( mo ); g_free( o ); g_free( sis ); g_free( p ); return falla( "%s", why ); }
    if ( razon && *razon ) pr_corrida_razon( p, caso, id, razon, &e );
    g_snprintf( base, sizeof base, "%s", out );
    if ( g_str_has_suffix( base, ".out" ) ) base[strlen( base ) - 4] = '\0';
    k = sis_orden_modelo( sis, mo, base, buf, sizeof buf, av, G_N_ELEMENTS( av ) - 1 );
    if ( k < 0 ) { g_free( mo ); g_free( o ); g_free( sis ); g_free( p ); return falla( "La orden no cabe." ); }
    av[k] = NULL;
    dir = g_path_get_dirname( out );
    g_mkdir_with_parents( dir, 0700 );
    g_remove( out );
    engine_set_finder( hermano );
    raiz_de( path, raiz, sizeof raiz );
    obj = g_strdup_printf( "%s/%s", caso, id );
    coste_contexto( raiz, "atsw-cli", c_por ? c_por : "analista", obj, "estimar" );
    r = engine_run_v( dir, "drvarma", av );
    coste_contexto_limpia();
    g_free( dir ); g_free( obj );

    memset( o, 0, sizeof *o );
    ok = engine_wrote_results( &r ) && g_file_test( out, G_FILE_TEST_EXISTS ) && ov_parse_file( out, o ) == 0;
    if ( !ok || o->fallo )
        {
        gchar *m = g_strdup( o->fallo && o->fallo_msg[0] ? o->fallo_msg : r.message );
        const char *err = r.output ? strstr( r.output, "Error" ) : NULL;

        if ( err && !o->fallo )
            { gchar *l = g_strndup( err, strcspn( err, "\r\n" ) ), *m2 = g_strdup_printf( "%s (%s)", m, l );
              g_free( m ); g_free( l ); m = m2; }
        g_remove( out );
        engine_result_clear( &r ); g_free( mo ); g_free( o ); g_free( sis ); g_free( p );
        falla( "%s No queda la corrida.", m ); g_free( m );
        return 1;
        }
    if ( guarda( p, why, sizeof why ) != 0 )
        { engine_result_clear( &r ); g_free( mo ); g_free( o ); g_free( sis ); g_free( p ); return falla( "%s", why ); }

    {
    const char *opt = o->optim == OV_OPT_CONVERGIO ? "convergió" : o->optim == OV_OPT_PARED_MA
                      ? "parado en la pared de invertibilidad MA" : o->optim == OV_OPT_PARADO ? "parado sin converger"
                      : "sin optimizador (el diagonal)";
    GString *tb = g_string_new( "" );

    tabla_motor( out, tb );
    t = g_string_new( "" );
    if ( json )
        {
        g_string_append( t, "{\"ok\": true, " );
        js_par( t, "caso", caso ); g_string_append( t, ", " );
        js_par( t, "corrida", id ); g_string_append( t, ", " );
        js_par( t, "padre", k_desde ? k_desde : "" ); g_string_append( t, ", " );
        js_par( t, "clase", "VARMA" ); g_string_append( t, ", " );
        g_string_append_printf( t, "\"p\": %d, \"q\": %d, \"diagcov\": %s, ", mo->p, mo->q, mo->diagcov ? "true" : "false" );
        js_par( t, "enlaces", o->enlaces ); g_string_append( t, ", " );
        js_par( t, "optimizador", opt ); g_string_append( t, ", " );
        if ( o->tiene_logl ) { js_num( t, "logl", o->logl ); g_string_append( t, ", " ); }
        if ( o->tiene_lr )
            { js_num( t, "lr", o->lr ); g_string_append_printf( t, ", \"lr_df\": %d, ", o->lr_df );
              js_num( t, "lr_p", o->lr_p ); g_string_append( t, ", " ); }
        if ( o->tiene_diag && o->diag.hosking.hay )
            { js_num( t, "hosking_p", o->diag.hosking.p ); g_string_append( t, ", " ); }
        if ( o->tiene_diag && o->diag.jb.hay )
            { js_num( t, "jb_p", o->diag.jb.p ); g_string_append( t, ", " ); }
        js_num( t, "raiz_ar_max", o->raiz_ar_max ); g_string_append( t, ", " );
        js_num( t, "raiz_ma_max", o->raiz_ma_max ); g_string_append( t, ", " );
        js_par( t, "tabla", tb->str ); g_string_append( t, ", " );
        js_par( t, "out", out ); g_string_append( t, "}\n" );
        }
    else
        {
        g_string_append_printf( t, "%s/%s: VARMA p = %d, q = %d, Σ %s, enlaces %s\n", caso, id, mo->p, mo->q,
                                mo->diagcov ? "diagonal" : "completa", o->enlaces[0] ? o->enlaces : "todos" );
        g_string_append_printf( t, "  optimizador: %s%s%s\n", opt, o->criterio[0] ? " · " : "", o->criterio );
        if ( o->tiene_logl ) g_string_append_printf( t, "  log-verosimilitud exacta: %.6f\n", o->logl );
        if ( o->tiene_lr )
            g_string_append_printf( t, "  frente a las univariantes (el diagonal): LR = %.4f, %d g.l., p = %.4f\n",
                                    o->lr, o->lr_df, o->lr_p );
        if ( o->tiene_diag && o->diag.hosking.hay )
            g_string_append_printf( t, "  Hosking (retardo %d): Q = %.2f, %d g.l., p = %.4f\n", o->diag.hosking_lag,
                                    o->diag.hosking.q, o->diag.hosking.df, o->diag.hosking.p );
        if ( o->tiene_diag && o->diag.jb.hay )
            g_string_append_printf( t, "  Jarque-Bera multivariante: %.2f, %d g.l., p = %.4f\n",
                                    o->diag.jb.q, o->diag.jb.df, o->diag.jb.p );
        if ( o->n_raices_ar || o->n_raices_ma )
            g_string_append_printf( t, "  la raíz inversa mayor: AR %.4f, MA %.4f\n", o->raiz_ar_max, o->raiz_ma_max );
        g_string_append_printf( t, "\n%s  %s\n", tb->str, out );
        }
    fputs( t->str, stdout );
    g_string_free( t, TRUE ); g_string_free( tb, TRUE );
    }
    engine_result_clear( &r ); g_free( mo ); g_free( o ); g_free( sis ); g_free( p );
    return 0;
}

static int caso_estima( const char *path, const char *caso )
{
    char            why[512], raiz[PR_RUTA], out[PR_RUTA], dag[PR_RUTA], cns[PR_RUTA], res[PR_RUTA];
    char            eval_csv[PR_RUTA], uni_csv[PR_RUTA], uni_out[PR_RUTA];
    FcComp          comp;
    int             hay_comp = 0;
    char            sh[16], sw[16];
    char            id[PR_ID], pre[PR_MAX_ENTRADA][PR_RUTA];
    const char     *nom[NET_MAX_SER + 1];
    char            nombre[PR_MAX_ENTRADA][128];
    Proyecto       *p = abre( path, why, sizeof why );
    const PrCaso   *c;
    NetLink         lnk[NET_MAX_LINK];
    PrError         e;
    EngineResult    r;
    GPtrArray      *av;
    GString        *desfase = g_string_new( "" ), *t;
    gchar          *dir, *obj;
    int             i, k, nl = 0, hay_dag = 0, hay_cns = 0;

    if ( !p ) { g_string_free( desfase, TRUE ); return falla( "%s", why ); }
    if ( k_evaluar > 0 && k_prever <= 0 )
        { g_free( p ); g_string_free( desfase, TRUE );
          return falla( "--evaluar W necesita el horizonte: --prever H." ); }
    if ( !( c = pr_caso_ver( p, caso ) ) )
        { g_free( p ); g_string_free( desfase, TRUE ); return falla( "«%s» no es un caso del proyecto.", caso ); }
    if ( !strcmp( c->motor, "drvarma" ) )
        { g_free( p ); g_string_free( desfase, TRUE ); return caso_estima_sis( path, caso ); }
    if ( strcmp( c->motor, "drtran" ) != 0 )
        { int r0 = falla( "%s es de %s: no lo estima atsw-cli.", caso, c->motor );
          g_free( p ); g_string_free( desfase, TRUE ); return r0; }

    /* las entradas: sus .pre, su hash y el nombre que lleva cada uno */
    for ( i = 0; i < c->nen; i++ )
        {
        struct Tusmodel tm; struct Tseries ts; real **dm = NULL;
        gchar *h;

        if ( pr_ruta( p, c->en[i].serie, c->muestra, c->en[i].modelo, ".pre", pre[i], PR_RUTA ) != 0 ||
             !g_file_test( pre[i], G_FILE_TEST_EXISTS ) )
            { int r0 = falla( "%s/%s ya no tiene .pre.", c->en[i].serie, c->en[i].modelo );
              g_free( p ); g_string_free( desfase, TRUE ); return r0; }
        h = ge_sha( pre[i] );
        if ( c->en[i].sha[0] && ( !h || strcmp( h, c->en[i].sha ) ) )
            g_string_append_printf( desfase, "%s%s/%s", desfase->len ? ", " : "", c->en[i].serie, c->en[i].modelo );
        g_free( h );
        if ( read_fue_pre( pre[i], &tm, &ts, &dm ) != 0 )
            { g_free( p ); g_string_free( desfase, TRUE ); return falla( "No pude leer %s.", pre[i] ); }   /* pre[] es nuestro */
        g_snprintf( nombre[i], sizeof nombre[i], "%s", ts.name ? ts.name : c->en[i].serie );
        free_fue_pre( &tm, &ts, dm );
        nom[i + 1] = nombre[i];
        }
    nom[0] = NULL;
    if ( desfase->len )
        {
        gchar *m = g_strdup( desfase->str );
        g_free( p ); g_string_free( desfase, TRUE );
        falla( "%s está desfasado: cambió el .pre de %s desde el alta. No se estima por debajo: "
               "crea un caso nuevo con esos modelos.", caso, m );
        g_free( m );
        return 1;
        }
    g_string_free( desfase, TRUE );

    /* la red: los enlaces, por el nombre de serie del proyecto */
    for ( k = 0; k < k_nenlace; k++ )
        {
        char a[PR_ID], b[PR_ID];
        int  ia = 0, ib = 0, ob, orr, os, j;

        if ( sscanf( k_enlace[k], "%47[^-]->%47[^:]:%d,%d,%d", a, b, &ob, &orr, &os ) != 5 ||
             ob < 0 || orr < 0 || os < 0 )
            { g_free( p ); return falla( "--enlace %s: es ENTRADA->SALIDA:b,r,s («WTI->IPC:0,1,0»).", k_enlace[k] ); }
        for ( j = 0; j < c->nen; j++ )
            {
            if ( !strcmp( c->en[j].serie, a ) ) ia = j + 1;
            if ( !strcmp( c->en[j].serie, b ) ) ib = j + 1;
            }
        if ( !ia || !ib || ia == ib )
            { g_free( p ); return falla( "--enlace %s: las dos series tienen que ser del caso, y distintas.", k_enlace[k] ); }
        lnk[nl].out = ib; lnk[nl].inp = ia; lnk[nl].b = ob; lnk[nl].r = orr; lnk[nl].s = os;
        nl++;
        }
    if ( k_desde && pr_corrida_idx( p, caso, k_desde ) < 0 )
        { g_free( p ); return falla( "«%s» no es un modelo de %s.", k_desde, caso ); }

    /* la corrida, y sus ficheros */
    if ( pr_corrida_nueva( p, caso, k_desde ? k_desde : "", id, sizeof id, out, sizeof out, &e ) != 0 )
        { pr_error_es( &e, why, sizeof why ); g_free( p ); return falla( "%s", why ); }
    if ( razon && *razon ) pr_corrida_razon( p, caso, id, razon, &e );
    pr_corrida_ruta( p, caso, id, ".dag", dag, sizeof dag );
    pr_corrida_ruta( p, caso, id, ".cns", cns, sizeof cns );
    pr_corrida_ruta( p, caso, id, "_res.txt", res, sizeof res );
    pr_corrida_ruta( p, caso, id, "_eval.csv", eval_csv, sizeof eval_csv );
    pr_corrida_ruta( p, caso, id, "_uni_eval.csv", uni_csv, sizeof uni_csv );
    pr_corrida_ruta( p, caso, id, "_uni_eval.out", uni_out, sizeof uni_out );
    dir = g_path_get_dirname( out );
    g_mkdir_with_parents( dir, 0700 );
    if ( !k_diagonal )
        {
        if ( nl > 0 )
            {
            if ( net_write( dag, nom, lnk, nl, "escrito por atsw-cli caso-estima" ) != 0 )
                { g_free( dir ); g_free( p ); return falla( "No pude escribir %s.", dag ); }
            hay_dag = 1;
            }
        else if ( k_desde )
            {
            char d0[PR_RUTA];

            if ( pr_corrida_ruta( p, caso, k_desde, ".dag", d0, sizeof d0 ) == 0 &&
                 g_file_test( d0, G_FILE_TEST_EXISTS ) && copia_fichero( d0, dag ) == 0 ) hay_dag = 1;
            }
        if ( k_desde )
            {
            char c0[PR_RUTA];

            if ( pr_corrida_ruta( p, caso, k_desde, ".cns", c0, sizeof c0 ) == 0 &&
                 g_file_test( c0, G_FILE_TEST_EXISTS ) && copia_fichero( c0, cns ) == 0 ) hay_cns = 1;
            }
        }
    /* LAS RESTRICCIONES DE ESTA CORRIDA, DETRAS DE LAS HEREDADAS: el .cns se
       aplica linea a linea, asi que la ultima sobre un slot es la que manda.
       Quien juzga si el slot existe y si la linea se entiende es el motor
       (cns_read, lib/slots): un nombre que no tiene no deja corrida.     */
    if ( k_nrestr > 0 )
        {
        FILE *fc = g_fopen( cns, hay_cns ? "a" : "w" );

        if ( !fc ) { g_free( dir ); g_free( p ); return falla( "No pude escribir %s.", cns ); }
        fprintf( fc, "%s# atsw-cli caso-estima %s/%s --restriccion\n", hay_cns ? "\n" : "", caso, id );
        for ( k = 0; k < k_nrestr; k++ )
            {
            if ( !strchr( k_restr[k], '=' ) || strchr( k_restr[k], '\n' ) )
                { fclose( fc ); g_remove( cns ); g_free( dir ); g_free( p );
                  return falla( "--restriccion %s: es NOMBRE = VALOR («q[2,1] = free», «omega1[1] = 0», "
                                "«delta1[1] = delta2[1]»).", k_restr[k] ); }
            fprintf( fc, "%s\n", k_restr[k] );
            }
        fclose( fc );
        hay_cns = 1;
        }

    g_snprintf( sh, sizeof sh, "%d", k_prever );
    g_snprintf( sw, sizeof sw, "%d", k_evaluar );
    av = g_ptr_array_new();
    for ( i = 0; i < c->nen; i++ ) g_ptr_array_add( av, pre[i] );
    g_ptr_array_add( av, "-o" ); g_ptr_array_add( av, out );
    if ( hay_dag ) { g_ptr_array_add( av, "-n" ); g_ptr_array_add( av, dag ); }
    if ( hay_cns ) { g_ptr_array_add( av, "-c" ); g_ptr_array_add( av, cns ); }
    g_ptr_array_add( av, "-e" ); g_ptr_array_add( av, res );
    if ( k_diagonal ) g_ptr_array_add( av, "-0" );
    /* LA PREVISION Y LA EVALUACION, EN LA MISMA CORRIDA: el motor las hace
       en la misma pasada (drtran_gui, arma_argv).                       */
    if ( k_prever > 0 )
        { g_ptr_array_add( av, "-f" ); g_ptr_array_add( av, sh ); }
    if ( k_evaluar > 0 )
        {
        g_ptr_array_add( av, "-estwin" ); g_ptr_array_add( av, sw );
        g_ptr_array_add( av, "-C" );      g_ptr_array_add( av, eval_csv );
        }
    g_ptr_array_add( av, NULL );

    engine_set_finder( hermano );
    raiz_de( path, raiz, sizeof raiz );
    obj = g_strdup_printf( "%s/%s", caso, id );
    coste_contexto( raiz, "atsw-cli", c_por ? c_por : "analista", obj, "estimar" );
    r = engine_run_v( dir, "drtran", (const char *const *) av->pdata );
    coste_contexto_limpia();
    g_ptr_array_free( av, TRUE );
    g_free( dir );

    if ( !engine_wrote_results( &r ) || !g_file_test( out, G_FILE_TEST_EXISTS ) )
        {
        /* sin resultados no hay corrida: no se guarda el manifiesto, y los
           ficheros que se escribieron para ella, fuera                   */
        gchar *m = g_strdup( r.message );
        const char *err = r.output ? strstr( r.output, "Error:" ) : NULL;

        /* LO QUE DICE EL MOTOR, si lo dice («Error: unknown parameter
           'omega9[1]' in the constraints file»): el estado solo no sirve */
        if ( err )
            {
            gchar *l = g_strndup( err, strcspn( err, "\r\n" ) ), *m2;
            size_t lm = strlen( m );

            if ( lm && m[lm - 1] == '.' ) m[lm - 1] = '\0';
            m2 = g_strdup_printf( "%s (%s).", m, l );
            g_free( m ); g_free( l ); m = m2;
            }
        g_remove( out ); g_remove( dag ); g_remove( cns ); g_remove( res ); g_remove( eval_csv );
        engine_result_clear( &r ); g_free( obj ); g_free( p );
        falla( "%s No queda la corrida.", m );
        g_free( m );
        return 1;
        }
    if ( guarda( p, why, sizeof why ) != 0 )
        { engine_result_clear( &r ); g_free( obj ); g_free( p ); return falla( "%s", why ); }

    /* FRENTE AL UNIVARIANTE (drtran_gui, fase P3): el diagonal (-0) con la
       MISMA ventana y el mismo horizonte, para que la unica diferencia
       entre los dos sea la transferencia; y lib/fcomp los compara: el
       cociente de RMSE y Diebold-Mariano por horizonte.                */
    if ( k_evaluar > 0 && !k_diagonal && g_file_test( eval_csv, G_FILE_TEST_EXISTS ) )
        {
        GPtrArray   *au = g_ptr_array_new();
        EngineResult ru;
        gchar       *du = g_path_get_dirname( out );

        for ( i = 0; i < c->nen; i++ ) g_ptr_array_add( au, pre[i] );
        g_ptr_array_add( au, "-o" ); g_ptr_array_add( au, uni_out );
        g_ptr_array_add( au, "-0" );
        g_ptr_array_add( au, "-f" ); g_ptr_array_add( au, sh );
        g_ptr_array_add( au, "-estwin" ); g_ptr_array_add( au, sw );
        g_ptr_array_add( au, "-C" ); g_ptr_array_add( au, uni_csv );
        g_ptr_array_add( au, NULL );
        coste_contexto( raiz, "atsw-cli", c_por ? c_por : "analista", obj, "evaluar" );
        ru = engine_run_v( du, "drtran", (const char *const *) au->pdata );
        coste_contexto_limpia();
        hay_comp = ru.status == 0 && fc_compara_csv( uni_csv, eval_csv, &comp ) == 0;
        engine_result_clear( &ru );
        g_ptr_array_free( au, TRUE );
        g_free( du );
        }

    {
    Diagnosis *d = g_new0( Diagnosis, 1 );
    Forecast  *fc = g_new0( Forecast, 1 );
    int        hay_fc = ( k_prever > 0 || k_evaluar > 0 ) && of_parse_file( out, fc ) == 0;
    int        hay = od_parse_file( out, d ) == 0;
    int        noad = hay ? od_no_adecuados( d ) : -1, noex = hay ? od_no_exogenos( d ) : -1;

    t = g_string_new( "" );
    if ( json )
        {
        g_string_append( t, "{\"ok\": true, " );
        js_par( t, "caso", caso ); g_string_append( t, ", " );
        js_par( t, "corrida", id ); g_string_append( t, ", " );
        js_par( t, "padre", k_desde ? k_desde : "" ); g_string_append( t, ", " );
        g_string_append_printf( t, "\"estado\": %d, \"enlaces\": %d, \"red\": %s, \"diagonal\": %s, ",
                                r.status, nl, hay_dag ? "true" : "false", k_diagonal ? "true" : "false" );
        js_par( t, "mensaje", r.message ); g_string_append( t, ", " );
        if ( hay ) g_string_append_printf( t, "\"no_adecuados\": %d, \"no_exogenos\": %d, ", noad, noex );
        if ( hay_fc && fc->ns > 0 )
            {
            int a, b;

            g_string_append( t, "\"prevision\": [" );
            for ( a = 0; a < fc->ns; a++ )
                {
                const OfSerie *x = &fc->s[a];
                int primera = -1;

                for ( b = 0; b < x->nf; b++ ) if ( x->f[b].tiene_sd ) { primera = b; break; }
                g_string_append_printf( t, "%s{", a ? ", " : "" );
                js_par( t, "serie", x->nombre ); g_string_append( t, ", " );
                js_par( t, "origen", x->origen );
                g_string_append_printf( t, ", \"horizonte\": %d", x->nprev );
                if ( primera >= 0 )
                    { g_string_append( t, ", \"primera\": {" ); js_par( t, "fecha", x->f[primera].fecha );
                      g_string_append( t, ", " ); js_num( t, "nivel", x->f[primera].nivel );
                      g_string_append( t, ", " ); js_num( t, "sd", x->f[primera].sd_nivel ); g_string_append( t, "}" ); }
                g_string_append( t, "}" );
                }
            g_string_append( t, "], " );
            }
        if ( hay_fc && fc->tiene_ev )
            {
            int a;

            g_string_append( t, "\"evaluacion\": {" );
            js_par( t, "salida", fc->ev.salida );
            g_string_append_printf( t, ", \"origenes\": %d, \"horizontes\": [", fc->ev.origenes );
            for ( a = 0; a < fc->ev.nh; a++ )
                {
                g_string_append_printf( t, "%s{\"h\": %d, \"n\": %d, ", a ? ", " : "", fc->ev.h[a].h, fc->ev.h[a].n );
                js_num( t, "rmse", fc->ev.h[a].rmse ); g_string_append( t, ", " );
                js_num( t, "mae", fc->ev.h[a].mae ); g_string_append( t, "}" );
                }
            g_string_append( t, "]}, " );
            }
        if ( hay_comp )
            {
            int a;

            g_string_append_printf( t, "\"frente_al_diagonal\": {\"origenes\": %d, \"horizontes\": [", comp.origenes );
            for ( a = 0; a < comp.nh; a++ )
                {
                const FcHoriz *z = &comp.h[a];

                g_string_append_printf( t, "%s{\"h\": %d, \"n\": %d, ", a ? ", " : "", z->h, z->n );
                js_num( t, "rmse_diagonal", z->rmse_a ); g_string_append( t, ", " );
                js_num( t, "rmse_modelo", z->rmse_b ); g_string_append( t, ", " );
                js_num( t, "cociente", z->ratio ); g_string_append( t, ", " );
                if ( z->dm_ok ) { js_num( t, "dm", z->dm ); g_string_append( t, ", " ); js_num( t, "p", z->p ); }
                else g_string_append( t, "\"dm\": null, \"p\": null" );
                g_string_append( t, "}" );
                }
            g_string_append( t, "]}, " );
            }
        {
        GString *tb = g_string_new( "" );

        tabla_motor( out, tb );
        g_string_append_printf( t, "\"restricciones\": %s, ", hay_cns ? "true" : "false" );
        js_par( t, "tabla", tb->str ); g_string_append( t, ", " );
        g_string_free( tb, TRUE );
        }
        js_par( t, "out", out ); g_string_append( t, "}\n" );
        }
    else
        {
        g_string_append_printf( t, "%s/%s: %s\n", caso, id, r.message );
        g_string_append_printf( t, "  red: %s\n", k_diagonal ? "ninguna (el diagonal, -0)" :
                                hay_dag ? ( nl ? "la de los enlaces dados" : "la de la corrida de partida" )
                                        : "la que identifique drtran" );
        if ( hay )
            g_string_append_printf( t, "  diagnosis: %d contraste%s de adecuación sin pasar, %d de exogeneidad\n",
                                    noad, noad == 1 ? "" : "s", noex );
        if ( hay_cns ) g_string_append( t, "  restricciones: las del .cns de la corrida\n" );
        g_string_append( t, "\n" );
        tabla_motor( out, t );
        if ( hay_fc && fc->ns > 0 )
            {
            char tb[16384];

            /* la tabla de la SALIDA, tal cual: la primera serie del caso, con
               el nombre que lleva su .pre (el .out puede listar otra antes) */
            if ( of_tabla_file( out, nombre[0], tb, sizeof tb ) == 0 ||
                 of_tabla_file( out, fc->s[0].nombre, tb, sizeof tb ) == 0 )
                g_string_append_printf( t, "\n%s\n", tb );
            }
        if ( hay_fc && fc->tiene_ev )
            {
            int a;

            g_string_append_printf( t, "  evaluación de %s fuera de muestra, %d orígenes:\n", fc->ev.salida, fc->ev.origenes );
            for ( a = 0; a < fc->ev.nh; a++ )
                g_string_append_printf( t, "    h = %2d  RMSE %.4f  MAE %.4f  (%d)\n", fc->ev.h[a].h,
                                        fc->ev.h[a].rmse, fc->ev.h[a].mae, fc->ev.h[a].n );
            }
        if ( hay_comp )
            {
            int a;

            g_string_append_printf( t, "  frente al diagonal (sin transferencias, la misma ventana), %d orígenes:\n",
                                    comp.origenes );
            for ( a = 0; a < comp.nh; a++ )
                {
                const FcHoriz *z = &comp.h[a];

                if ( z->dm_ok )
                    g_string_append_printf( t, "    h = %2d  RMSE %.4f frente a %.4f, cociente %.3f; DM %.2f (p = %.3f)\n",
                                            z->h, z->rmse_b, z->rmse_a, z->ratio, z->dm, z->p );
                else
                    g_string_append_printf( t, "    h = %2d  RMSE %.4f frente a %.4f, cociente %.3f; DM no calculable\n",
                                            z->h, z->rmse_b, z->rmse_a, z->ratio );
                }
            g_string_append( t, "    (un cociente < 1 es que la transferencia prevé mejor; DM > 0, con su p)\n" );
            }
        g_string_append_printf( t, "  %s\n", out );
        }
    fputs( t->str, stdout );
    g_string_free( t, TRUE );
    g_free( d ); g_free( fc );
    }
    engine_result_clear( &r );
    g_free( obj ); g_free( p );
    return 0;
}

static int copia_pre( const char *de, const char *a )
{
    gchar *c = NULL;
    gsize  n = 0;
    int    r = g_file_get_contents( de, &c, &n, NULL ) && g_file_set_contents( a, c, (gssize) n, NULL );

    g_free( c );
    return r ? 0 : 1;
}

/* ------------------------------------------------------------------------ */
/* prever: la prevision univariante, fue -f y fuf                           */
/* ------------------------------------------------------------------------ */

/* prever P SERIE MODELO: LO QUE HACE «Forecast» EN fue_gui --fue -f escribe
   forecast_<base>.inp y fuf lo corre--, sin tocar el modelo. fue -f
   reestima, y sobre el .inp reescribia el .out del modelo (la C3 de
   AUDITORIA-procesos): aqui se hace en una carpeta temporal y desde el .pre,
   que es el optimo y no se mueve. Junto al modelo quedan solo
   forecast_<base>.inp y forecast_<base>.out, donde los deja fue_gui. Las dos
   corridas, en costes.csv como «prever».                               */
static int prever( const char *path, const char *serie, const char *id )
{
    char         why[512], pre[PR_RUTA], raiz[PR_RUTA], tabla[16384];
    Proyecto    *p = NULL;
    gchar       *tmp, *base, *dmod, *fin, *fout, *inp_t, *obj;
    EngineResult r;
    Forecast    *f;
    GString     *t;
    int          i, k, rc = 1;

    if ( ruta_de( path, serie, id, ".pre", pre, sizeof pre, &p ) != 0 ) { g_free( p ); return 1; }
    if ( !g_file_test( pre, G_FILE_TEST_EXISTS ) )
        { g_free( p ); return falla( "%s/%s no está estimado: no hay .pre del que prever.", serie, id ); }
    tmp = g_dir_make_tmp( "atsw-prever-XXXXXX", NULL );
    if ( !tmp ) { g_free( p ); return falla( "No pude crear la carpeta de trabajo." ); }
    base  = g_path_get_basename( pre );
    base[strlen( base ) - 4] = '\0';
    dmod  = g_path_get_dirname( pre );
    inp_t = g_strdup_printf( "%s%c%s.inp", tmp, G_DIR_SEPARATOR, base );
    if ( copia_pre( pre, inp_t ) != 0 ) { g_snprintf( why, sizeof why, "No pude copiar %s.", pre ); goto fin_mal; }

    engine_set_finder( hermano );
    raiz_de( path, raiz, sizeof raiz );
    obj = *muestra ? g_strdup_printf( "%s/%s/%s", serie, muestra, id ) : g_strdup_printf( "%s/%s", serie, id );
    coste_contexto( raiz, "atsw-cli", c_por ? c_por : "analista", obj, "prever" );
    r = engine_run( tmp, "fue", base, "-f", NULL );
    coste_contexto_limpia();
    if ( !engine_wrote_results( &r ) )
        { g_snprintf( why, sizeof why, "fue -f: %s", r.message ); engine_result_clear( &r ); g_free( obj ); goto fin_mal; }
    engine_result_clear( &r );
    {
    gchar *fb = g_strdup_printf( "forecast_%s", base );

    coste_contexto( raiz, "atsw-cli", c_por ? c_por : "analista", obj, "prever" );
    r = engine_run( tmp, "fuf", fb, NULL );
    coste_contexto_limpia();
    g_free( fb );
    }
    g_free( obj );
    if ( !engine_wrote_results( &r ) )
        { g_snprintf( why, sizeof why, "fuf: %s", r.message ); engine_result_clear( &r ); goto fin_mal; }
    engine_result_clear( &r );

    /* lo de la prevision, junto al modelo */
    fin  = g_strdup_printf( "%s%cforecast_%s.inp", dmod, G_DIR_SEPARATOR, base );
    fout = g_strdup_printf( "%s%cforecast_%s.out", dmod, G_DIR_SEPARATOR, base );
    {
    gchar *a = g_strdup_printf( "%s%cforecast_%s.inp", tmp, G_DIR_SEPARATOR, base );
    gchar *b = g_strdup_printf( "%s%cforecast_%s.out", tmp, G_DIR_SEPARATOR, base );
    int    ok = copia_pre( a, fin ) == 0 && copia_pre( b, fout ) == 0;

    g_free( a ); g_free( b );
    if ( !ok ) { g_snprintf( why, sizeof why, "No pude dejar la previsión junto al modelo." );
                 g_free( fin ); g_free( fout ); goto fin_mal; }
    }

    f = g_new0( Forecast, 1 );
    t = g_string_new( "" );
    if ( of_parse_file( fout, f ) != 0 || f->ns < 1 )
        { g_free( f ); g_string_free( t, TRUE ); g_free( fin ); g_free( fout );
          g_snprintf( why, sizeof why, "fuf no dejó una tabla de previsión que se lea." ); goto fin_mal; }
    if ( json )
        {
        const OfSerie *s0 = &f->s[0];

        g_string_append( t, "{\"ok\": true, " );
        js_par( t, "serie", serie ); g_string_append( t, ", " );
        js_par( t, "modelo", id ); g_string_append( t, ", " );
        js_par( t, "origen", s0->origen );
        g_string_append_printf( t, ", \"horizonte\": %d, ", s0->lead );
        js_par( t, "out", fout ); g_string_append( t, ", \"prevision\": [" );
        for ( i = 0, k = 0; i < s0->nf; i++ )
            {
            const OfFila *x = &s0->f[i];

            if ( !x->tiene_sd ) continue;
            g_string_append_printf( t, "%s{", k++ ? ", " : "" );
            js_par( t, "fecha", x->fecha ); g_string_append( t, ", " );
            js_num( t, "nivel", x->nivel ); g_string_append( t, ", " );
            js_num( t, "sd", x->sd_nivel ); g_string_append( t, ", " );
            js_num( t, "var_periodo", x->var_per ); g_string_append( t, ", " );
            js_num( t, "sd_periodo", x->sd_per ); g_string_append( t, ", " );
            /* una serie anual no tiene columnas anuales: «-» en la tabla */
            if ( strchr( x->fecha, '/' ) )
                { js_num( t, "var_anual", x->var_anu ); g_string_append( t, ", " );
                  js_num( t, "sd_anual", x->sd_anu ); }
            else g_string_append( t, "\"var_anual\": null, \"sd_anual\": null" );
            g_string_append( t, "}" );
            }
        g_string_append( t, "]}\n" );
        }
    else
        {
        if ( of_tabla_file( fout, f->s[0].nombre, tabla, sizeof tabla ) != 0 ) tabla[0] = '\0';
        g_string_append_printf( t, "Previsión de %s/%s (fue -f y fuf, desde el .pre: el modelo no se toca)\n%s\n  %s\n",
                                serie, id, tabla, fout );
        }
    fputs( t->str, stdout );
    g_string_free( t, TRUE );
    g_free( f ); g_free( fin ); g_free( fout );
    rc = 0;
    goto fin;
fin_mal:
    falla( "%s", why );
fin:
    {
    GDir        *d = g_dir_open( tmp, 0, NULL );
    const gchar *n;

    while ( d && ( n = g_dir_read_name( d ) ) != NULL )
        { gchar *x = g_build_filename( tmp, n, NULL ); g_unlink( x ); g_free( x ); }
    if ( d ) g_dir_close( d );
    g_rmdir( tmp );
    }
    g_free( inp_t ); g_free( dmod ); g_free( base ); g_free( tmp ); g_free( p );
    return rc;
}

/* ------------------------------------------------------------------------ */
/* La orden                                                                 */
/* ------------------------------------------------------------------------ */

static void uso( FILE *f )
{
    fputs( "uso: atsw-cli ORDEN [opciones]\n\n"
           "  version\n"
           "  proyecto ver P\n"
           "  proyecto nuevo P SERIE                el modelo nuevo, de los datos\n"
           "  proyecto itera P SERIE PADRE          el .pre de PADRE a un hijo .inp\n"
           "  proyecto razon P SERIE MODELO TEXTO\n"
           "  proyecto elige P SERIE MODELO RAZON\n"
           "  proyecto caso-nuevo P SERIE/MODELO SERIE/MODELO...   (la salida primero)\n"
           "  proyecto ruta P SERIE MODELO|datos .inp|.pre|.out   la ruta del fichero (estado 1 si no existe)\n"
           "  caso-estima P CASO [--enlace E->S:b,r,s]... [--desde cNN] [--diagonal]\n"
           "              [--prever H [--evaluar W]] [--restriccion \"NOMBRE = VALOR\"]...   drtran sobre el caso\n"
           "  caso-puerta P CASO                    SIM: la puerta del sistema diagonal y las CCF\n"
           "  caso-estima P CASO --p N --q N [--diagcov] [--enlace A<-B]... [--cero \"AR1[A<-B]\"]... [--desde cNN]\n"
           "                                        SIM: un VARMA con drvarma, tras la puerta\n"
           "  dictamen P SERIE MODELO               la diagnosis del .out\n"
           "  ecuacion P SERIE MODELO               el modelo estimado: el bloque del .out, con sus t\n"
           "  raices factores P SERIE MODELO        la factorización del AR\n"
           "  raices factorizado P SERIE MODELO     deriva el modelo factorizado\n"
           "  ficha P SERIE MODELO [--comprobar]    la ficha de confianza, en Markdown\n"
           "  identifica P SERIE [--transformacion L,d,D] [--residuos MODELO]   lo que art propone\n"
           "  especifica P SERIE MODELO [cambios]   la especificación de un modelo sin estimar\n"
           "  estima P SERIE MODELO                fue sobre el modelo, y su dictamen\n"
           "  prever P SERIE MODELO                la previsión: fue -f y fuf, sin tocar el modelo\n"
           "  formales P SERIE MODELO [--familia F] [--sub]   los contrastes formales\n"
           "  anomalos P SERIE MODELO [--episodios L]   los episodios y su calibración\n"
           "  prewhiten P SALIDA/MODELO ENTRADA/MODELO...   la CCF preblanqueada\n"
           "  coste apunta P --motor M --paso X --cpu S --pared S   una corrida de fuera\n"
           "  coste resumen P [--por-que G]         los totales de costes.csv\n\n"
           "  P es el proyecto.yaml.\n"
           "  --muestra M   la muestra (sin ella, la completa)\n"
           "  --razon R     la razón del modelo nuevo o del caso\n"
           "  --titulo T    el título del caso\n"
           "  --motor M     caso-nuevo: drtran (MT, si no se dice) o drvarma (SIM)\n"
           "  --json        la salida en JSON\n"
           "  --por Q       quién da el paso: analista (si no se dice) o asistente;\n"
           "                en nuevo, itera, elige y raices factorizado queda escrito\n"
           "  --comprobar   ficha: corre fue sobre el .pre y lo apunta en costes.csv\n"
           "  especifica: --transformacion L,d,D  --arma p,q,P,Q  --anade-arma p,q,P,Q\n"
           "    --intervencion \"step 10 2008\" (se repite)  --media libre|V (fija en V)\n"
           "    --armonicos N|todos (N pares cos/sin; todos: y alter, con D = 0)  --pascua (mensuales)\n"
           "  --familia F   formales: integracion o estacionalidad (sin ella, las dos)\n"
           "  --sub         formales: también d - 1 (se pide, no se ofrece)\n"
           "  --episodios L anomalos: los que se omiten al calibrar, 1,3 o todos\n"
           "                (sin ella, todos)\n"
           "  --retardos N  prewhiten: a cada lado (sin ella, la regla del motor)\n\n"
           "  coste apunta: --objeto SERIE/MODELO, --via C|python, --quien (atsw-cli),\n"
           "    --fecha, --sha-pre, y --energia J con su --metodo (sin ellos, se estima)\n"
           "  coste resumen: --por-que serie|objeto|motor|via|por|paso|quien\n", f );
}

int main( int argc, char **argv )
{
    char *pos[64];
    int   npos = 0, i;

    setlocale( LC_ALL, "" );
    setlocale( LC_NUMERIC, "C" );          /* los ficheros, con punto */
    for ( i = 1; i < argc; i++ )
        {
        if ( !strcmp( argv[i], "--json" ) ) json = TRUE;
        else if ( !strcmp( argv[i], "--comprobar" ) ) comprobar = TRUE;
        else if ( !strcmp( argv[i], "--p" ) && i + 1 < argc ) k_p = atoi( argv[++i] );
        else if ( !strcmp( argv[i], "--q" ) && i + 1 < argc ) k_q = atoi( argv[++i] );
        else if ( !strcmp( argv[i], "--diagcov" ) ) k_diagcov = 1;
        else if ( !strcmp( argv[i], "--cero" ) && i + 1 < argc )
            { if ( k_ncero < K_MAX_RESTR ) k_cero[k_ncero++] = argv[++i]; else i++; }
        else if ( !strcmp( argv[i], "--restriccion" ) && i + 1 < argc )
            { if ( k_nrestr < K_MAX_RESTR ) k_restr[k_nrestr++] = argv[++i]; else i++; }
        else if ( !strcmp( argv[i], "--enlace" ) && i + 1 < argc )
            { if ( k_nenlace < NET_MAX_LINK ) k_enlace[k_nenlace++] = argv[++i]; else i++; }
        else if ( !strcmp( argv[i], "--desde" ) && i + 1 < argc ) k_desde = argv[++i];
        else if ( !strcmp( argv[i], "--diagonal" ) ) k_diagonal = TRUE;
        else if ( !strcmp( argv[i], "--prever" ) && i + 1 < argc ) k_prever = atoi( argv[++i] );
        else if ( !strcmp( argv[i], "--evaluar" ) && i + 1 < argc ) k_evaluar = atoi( argv[++i] );
        else if ( !strcmp( argv[i], "--residuos" ) && i + 1 < argc ) i_resid = argv[++i];
        else if ( !strcmp( argv[i], "--arma" ) && i + 1 < argc ) e_arma = argv[++i];
        else if ( !strcmp( argv[i], "--anade-arma" ) && i + 1 < argc ) e_anade = argv[++i];
        else if ( !strcmp( argv[i], "--transformacion" ) && i + 1 < argc ) e_transf = argv[++i];
        else if ( !strcmp( argv[i], "--armonicos" ) && i + 1 < argc ) e_armon = argv[++i];
        else if ( !strcmp( argv[i], "--pascua" ) ) e_pascua = 1;
        else if ( !strcmp( argv[i], "--media" ) && i + 1 < argc ) e_media = argv[++i];
        else if ( !strcmp( argv[i], "--intervencion" ) && i + 1 < argc )
            { if ( e_ninterv < (int) G_N_ELEMENTS(e_interv) ) e_interv[e_ninterv++] = argv[++i]; else i++; }
        else if ( !strcmp( argv[i], "--sub" ) ) con_sub = TRUE;
        else if ( !strcmp( argv[i], "--familia" ) && i + 1 < argc ) familia = argv[++i];
        else if ( !strcmp( argv[i], "--episodios" ) && i + 1 < argc ) episodios = argv[++i];
        else if ( !strcmp( argv[i], "--retardos" ) && i + 1 < argc ) retardos = atoi( argv[++i] );
        else if ( !strcmp( argv[i], "--muestra" ) && i + 1 < argc ) muestra = argv[++i];
        else if ( !strcmp( argv[i], "--razon" ) && i + 1 < argc ) razon = argv[++i];
        else if ( !strcmp( argv[i], "--titulo" ) && i + 1 < argc ) titulo = argv[++i];
        else if ( !strcmp( argv[i], "--quien" ) && i + 1 < argc ) c_quien = argv[++i];
        else if ( !strcmp( argv[i], "--por" ) && i + 1 < argc ) c_por = argv[++i];
        else if ( !strcmp( argv[i], "--objeto" ) && i + 1 < argc ) c_objeto = argv[++i];
        else if ( !strcmp( argv[i], "--paso" ) && i + 1 < argc ) c_paso = argv[++i];
        else if ( !strcmp( argv[i], "--motor" ) && i + 1 < argc ) c_motor = argv[++i];
        else if ( !strcmp( argv[i], "--via" ) && i + 1 < argc ) c_via = argv[++i];
        else if ( !strcmp( argv[i], "--cpu" ) && i + 1 < argc ) c_cpu = argv[++i];
        else if ( !strcmp( argv[i], "--pared" ) && i + 1 < argc ) c_pared = argv[++i];
        else if ( !strcmp( argv[i], "--energia" ) && i + 1 < argc ) c_energia = argv[++i];
        else if ( !strcmp( argv[i], "--metodo" ) && i + 1 < argc ) c_metodo = argv[++i];
        else if ( !strcmp( argv[i], "--sha-pre" ) && i + 1 < argc ) c_sha = argv[++i];
        else if ( !strcmp( argv[i], "--fecha" ) && i + 1 < argc ) c_fecha = argv[++i];
        else if ( !strcmp( argv[i], "--por-que" ) && i + 1 < argc ) c_por_que = argv[++i];
        else if ( !strcmp( argv[i], "-h" ) || !strcmp( argv[i], "--help" ) )
            { uso( stdout ); return 0; }
        else if ( argv[i][0] == '-' && argv[i][1] == '-' )
            { fprintf( stderr, "atsw-cli: no entiendo «%s»\n", argv[i] ); uso( stderr ); return 2; }
        else if ( npos < (int) G_N_ELEMENTS(pos) ) pos[npos++] = argv[i];
        }
    if ( npos == 0 ) { uso( stderr ); return 2; }
    /* QUIEN: solo los dos que hay (§7.2). Un valor que no se entiende no se
       escribe en el manifiesto ni en el registro.                       */
    if ( c_por && strcmp( c_por, "analista" ) && strcmp( c_por, "asistente" ) )
        return falla( "--por %s: es analista o asistente.", c_por );

#define ES(a, b) ( npos > (a) && !strcmp( pos[a], b ) )
    if ( ES( 0, "version" ) )
        {
        if ( json ) printf( "{\"ok\": true, \"version\": \"%s\"}\n", ATSW_VERSION );
        else printf( "atsw-cli %s\n", ATSW_VERSION );
        return 0;
        }
    if ( ES( 0, "proyecto" ) && ES( 1, "ver" ) && npos == 3 ) return proyecto_ver( pos[2] );
    if ( ES( 0, "proyecto" ) && ES( 1, "ruta" ) && npos == 6 )
        return proyecto_ruta( pos[2], pos[3], pos[4], pos[5] );
    if ( ES( 0, "proyecto" ) && ES( 1, "nuevo" ) && npos == 4 )
        return proyecto_deriva( pos[2], pos[3], NULL );
    if ( ES( 0, "proyecto" ) && ES( 1, "itera" ) && npos == 5 )
        return proyecto_deriva( pos[2], pos[3], pos[4] );
    if ( ES( 0, "proyecto" ) && ES( 1, "razon" ) && npos == 6 )
        return proyecto_decide( pos[2], pos[3], pos[4], pos[5], FALSE );
    if ( ES( 0, "proyecto" ) && ES( 1, "elige" ) && npos == 6 )
        return proyecto_decide( pos[2], pos[3], pos[4], pos[5], TRUE );
    if ( ES( 0, "proyecto" ) && ES( 1, "caso-nuevo" ) && npos >= 4 )
        return proyecto_caso( pos[2], pos + 3, npos - 3 );
    if ( ES( 0, "dictamen" ) && npos == 4 ) return dictamen( pos[1], pos[2], pos[3] );
    if ( ES( 0, "ecuacion" ) && npos == 4 ) return ecuacion( pos[1], pos[2], pos[3] );
    if ( ES( 0, "raices" ) && ES( 1, "factores" ) && npos == 5 )
        return raices_factores( pos[2], pos[3], pos[4] );
    if ( ES( 0, "raices" ) && ES( 1, "factorizado" ) && npos == 5 )
        return raices_factorizado( pos[2], pos[3], pos[4] );
    if ( ES( 0, "ficha" ) && npos == 4 ) return ficha( pos[1], pos[2], pos[3] );
    if ( ES( 0, "prever" ) && npos == 4 ) return prever( pos[1], pos[2], pos[3] );
    if ( ES( 0, "caso-estima" ) && npos == 3 ) return caso_estima( pos[1], pos[2] );
    if ( ES( 0, "caso-puerta" ) && npos == 3 ) return caso_puerta( pos[1], pos[2] );
    if ( ES( 0, "identifica" ) && npos == 3 ) return identifica( pos[1], pos[2] );
    if ( ES( 0, "especifica" ) && npos == 4 ) return especifica( pos[1], pos[2], pos[3] );
    if ( ES( 0, "estima" ) && npos == 4 ) return estima( pos[1], pos[2], pos[3] );
    if ( ES( 0, "formales" ) && npos == 4 ) return formales( pos[1], pos[2], pos[3] );
    if ( ES( 0, "anomalos" ) && npos == 4 ) return anomalos( pos[1], pos[2], pos[3] );
    if ( ES( 0, "prewhiten" ) && npos >= 3 ) return prewhiten( pos[1], pos + 2, npos - 2 );
    if ( ES( 0, "coste" ) && ES( 1, "apunta" ) && npos == 3 ) return coste_apunta_orden( pos[2] );
    if ( ES( 0, "coste" ) && ES( 1, "resumen" ) && npos == 3 ) return coste_resumen_orden( pos[2] );
#undef ES
    fprintf( stderr, "atsw-cli: orden desconocida o mal escrita\n" );
    uso( stderr );
    return 2;
}
