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

#ifndef ATSW_VERSION
#define ATSW_VERSION "en desarrollo"
#endif

/* ------------------------------------------------------------------------ */
/* Las opciones comunes                                                     */
/* ------------------------------------------------------------------------ */

static gboolean  json;
static gboolean  comprobar;
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
           "  dictamen P SERIE MODELO               la diagnosis del .out\n"
           "  raices factores P SERIE MODELO        la factorización del AR\n"
           "  raices factorizado P SERIE MODELO     deriva el modelo factorizado\n"
           "  ficha P SERIE MODELO [--comprobar]    la ficha de confianza, en Markdown\n"
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
    if ( ES( 0, "raices" ) && ES( 1, "factores" ) && npos == 5 )
        return raices_factores( pos[2], pos[3], pos[4] );
    if ( ES( 0, "raices" ) && ES( 1, "factorizado" ) && npos == 5 )
        return raices_factorizado( pos[2], pos[3], pos[4] );
    if ( ES( 0, "ficha" ) && npos == 4 ) return ficha( pos[1], pos[2], pos[3] );
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
