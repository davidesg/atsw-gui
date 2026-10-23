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

#ifndef ATSW_VERSION
#define ATSW_VERSION "en desarrollo"
#endif

/* ------------------------------------------------------------------------ */
/* Las opciones comunes                                                     */
/* ------------------------------------------------------------------------ */

static gboolean  json;
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

            g_string_append_printf( t, "  %s/%s%s%s%s%s%s\n", m->serie, m->id,
                                    m->muestra[0] ? "  [" : "", m->muestra,
                                    m->muestra[0] ? "]" : "",
                                    m->elegido ? "  * elegido" : "",
                                    m->rol == PR_DATOS ? "  (datos)" : "" );
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
    if ( pr_caso_add( p, e, nen, muestra, NULL, titulo, razon, id, sizeof id, &pe ) != 0 )
        { pr_error_es( &pe, why, sizeof why ); g_free( p ); return falla( "%s", why ); }
    if ( guarda( p, why, sizeof why ) != 0 ) { g_free( p ); return falla( "%s", why ); }
    if ( json )
        {
        GString *t = g_string_new( "{\"ok\": true, " );

        js_par( t, "caso", id ); g_string_append( t, "}\n" );
        fputs( t->str, stdout );
        g_string_free( t, TRUE );
        }
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
    static char det[64][ID_LINEA];
    const char *ptr[64];
    char        out[PR_RUTA], pre[PR_RUTA], why[256];
    Proyecto   *p = NULL;
    FueOut     *o;
    Convergence c;
    Dictamen    d;
    GString    *t;
    int         nd = 0, i;

    if ( ruta_de( path, serie, id, ".out", out, sizeof out, &p ) != 0 ) { g_free( p ); return 1; }
    o = g_new0( FueOut, 1 );
    if ( !fueout_read( out, o ) )
        { g_free( o ); g_free( p ); return falla( "«%s» no está estimado: no hay .out.", id ); }
    /* LOS NOMBRES DE LOS DETERMINISTAS, del .pre o del .inp: el .out dice
       «deterministic variable 12» y nada mas (como an_deterministas). */
    if ( ( pr_ruta( p, serie, muestra, id, ".pre", pre, sizeof pre ) == 0 &&
           g_file_test( pre, G_FILE_TEST_EXISTS ) ) ||
         pr_ruta( p, serie, muestra, id, ".inp", pre, sizeof pre ) == 0 )
        nd = id_nombres( pre, det, 64, why, sizeof why );
    if ( nd < 0 ) nd = 0;
    for ( i = 0; i < nd; i++ ) ptr[i] = det[i];
    memset( &c, 0, sizeof c );
    if ( !convergence_of( out, &c ) ) memset( &c, 0, sizeof c );
    dx_dictamen( o, &c, nd ? ptr : NULL, nd, &d );
    convergence_clear( &c );

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
    g_free( o );
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
    l.por     = (char *) ( c_por ? c_por : "asistente" );
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
           "  coste apunta P --motor M --paso X --cpu S --pared S   una corrida de fuera\n"
           "  coste resumen P [--por-que G]         los totales de costes.csv\n\n"
           "  P es el proyecto.yaml.\n"
           "  --muestra M   la muestra (sin ella, la completa)\n"
           "  --razon R     la razón del modelo nuevo o del caso\n"
           "  --titulo T    el título del caso\n"
           "  --json        la salida en JSON\n\n"
           "  coste apunta: --objeto SERIE/MODELO, --via C|python, --quien (atsw-cli),\n"
           "    --por (asistente), --fecha, --sha-pre; --energia J con su --metodo, o\n"
           "    sin los dos, que se estima (cpu×TDP/núcleos).\n"
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
    if ( ES( 0, "coste" ) && ES( 1, "apunta" ) && npos == 3 ) return coste_apunta_orden( pos[2] );
    if ( ES( 0, "coste" ) && ES( 1, "resumen" ) && npos == 3 ) return coste_resumen_orden( pos[2] );
#undef ES
    fprintf( stderr, "atsw-cli: orden desconocida o mal escrita\n" );
    uso( stderr );
    return 2;
}
