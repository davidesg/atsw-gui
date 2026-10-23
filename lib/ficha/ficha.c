/* ficha.c -- la ficha de confianza de un modelo. Ver ficha.h. */

#include "ficha.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>

#include "outfile.h"
#include "inpdet.h"
#include "coste.h"
#include "engine.h"

/* LA TOLERANCIA DE «SE REPRODUCE»: |a - b| <= FI_TOL * max(1, |a|, |b|).
 *
 * NO PUEDE SER LA DEL FLOTANTE (1e-6 relativo, 1e-10 absoluto). El .pre
 * CUANTIZA: los parametros van con seis decimales, y los ARMA y la delta con
 * cuatro en los dialectos viejos (REGLAS-NO-ESCRITAS §III.39). Reestimar desde
 * el valor redondeado vuelve al mismo optimo dentro de la tolerancia del
 * optimizador, y eso mueve la ultima cifra: medido con fue 1.14 sobre los
 * golden de engines/fue (RIPC.2, el de la bateria, pasa de 0.025253 a
 * 0.025254), un .pre genuino se mueve hasta ~3e-5 en un parametro de orden
 * uno y ~1e-4 en uno de orden cien. Un guardian con la tolerancia igual al
 * cuanto del fichero no puede pasar de forma robusta (DISENO-escalera §1.2).
 *
 * 1e-3, RELATIVO POR ENCIMA DE UNO, es el umbral que ya uso el certificado
 * del .pre de art, y esta MEDIDO, no elegido: un .pre genuino se mueve
 * ~3e-5, una especificacion entre 14 y 47 (DISENO-escalera §1.2). Deja diez
 * cuantos del formato mas grueso de margen y ni se acerca a lo que mueve un
 * .pre tocado a mano. Lo que no es un numero tiene que ser IGUAL.       */
#define FI_TOL  1e-3

static const char *const mes[] = { "ene", "feb", "mar", "abr", "may", "jun",
                                   "jul", "ago", "sep", "oct", "nov", "dic" };

/* ------------------------------------------------------------------------ */
/* Lo pequeño                                                                */
/* ------------------------------------------------------------------------ */

/* «serie/id» o «serie/muestra/id»: el objeto de costes.csv (coste_contexto_
   modelo lo compone igual). */
static gchar *objeto_de( const char *serie, const char *muestra, const char *id )
{
   if ( muestra && *muestra )
       return g_strdup_printf( "%s/%s/%s", serie, muestra, id );
   return g_strdup_printf( "%s/%s", serie, id );
}

static gchar *sha_fichero( const char *ruta )
{
   gchar *datos = NULL, *sha;
   gsize  n = 0;

   if ( !g_file_get_contents( ruta, &datos, &n, NULL ) ) return NULL;
   sha = g_compute_checksum_for_data( G_CHECKSUM_SHA256, (const guchar *) datos, n );
   g_free( datos );
   return sha;
}

static void corrida_de( FiCorrida *c, const CosteLinea *l )
{
   c->consta = 1;
   g_snprintf( c->fecha, sizeof c->fecha, "%s", l->fecha );
   g_snprintf( c->motor, sizeof c->motor, "%s", l->motor );
   g_snprintf( c->via, sizeof c->via, "%s", l->via );
   g_snprintf( c->metodo, sizeof c->metodo, "%s", l->metodo );
   c->cpu_s     = l->cpu_s;
   c->pared_s   = l->pared_s;
   c->energia_j = l->energia_j;
}

static int es_reproducir( const char *paso )
{
   return paso && g_str_has_prefix( paso, "reproducir" );
}

/* ------------------------------------------------------------------------ */
/* El dictamen                                                               */
/* ------------------------------------------------------------------------ */

int fi_dictamen( const Proyecto *p, const char *serie, const char *muestra,
                 const char *id, Dictamen *d )
{
   static char det[64][ID_LINEA];
   const char *ptr[64];
   char        out[PR_RUTA], pre[PR_RUTA], why[256];
   FueOut     *o;
   Convergence c;
   int         nd = 0, i;

   memset( d, 0, sizeof *d );
   if ( pr_ruta( p, serie, muestra, id, ".out", out, sizeof out ) != 0 ) return 1;
   o = g_new0( FueOut, 1 );
   if ( !fueout_read( out, o ) ) { g_free( o ); return 1; }
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
   dx_dictamen( o, &c, nd ? ptr : NULL, nd, d );
   convergence_clear( &c );
   g_free( o );
   return 0;
}

/* ------------------------------------------------------------------------ */
/* La ficha                                                                  */
/* ------------------------------------------------------------------------ */

int fi_ficha( const Proyecto *p, const char *serie, const char *muestra,
              const char *id, Ficha *f, char *why, size_t n )
{
   static char camino[FI_MAX_PASO][PR_ID];
   char        pre[PR_RUTA], raiz[PR_RUTA];
   CosteLinea *l = NULL;
   gchar     **objs;
   int         k, i, j, nl = 0;

   if ( muestra == NULL ) muestra = "";
   memset( f, 0, sizeof *f );
   g_snprintf( f->serie, sizeof f->serie, "%s", serie ? serie : "" );
   g_snprintf( f->muestra, sizeof f->muestra, "%s", muestra );
   g_snprintf( f->id, sizeof f->id, "%s", id ? id : "" );
   f->rep.energia_j = f->origen.energia_j = f->estimacion.energia_j = -1.0;

   i = pr_modelo_idx( p, serie, muestra, id );
   if ( i < 0 )
       {
       g_snprintf( why, n, "«%s» no es un modelo de %s en esa muestra.",
                   id ? id : "", serie ? serie : "" );
       return 1;
       }

   /* 1. EL RECORRIDO: pr_camino da del modelo a la raiz; se le da la
         vuelta, que se lee de los datos al modelo.                    */
   k = pr_camino( p, serie, muestra, id, camino, FI_MAX_PASO );
   if ( k < 0 )
       { g_snprintf( why, n, "El linaje de %s/%s se muerde la cola.", serie, id ); return 1; }
   for ( j = k - 1; j >= 0; j-- )
       {
       int     m = pr_modelo_idx( p, serie, muestra, camino[j] );
       FiPaso *s = &f->paso[f->npaso++];

       g_snprintf( s->id, sizeof s->id, "%s", camino[j] );
       if ( m < 0 ) continue;
       s->datos = p->m[m].rol == PR_DATOS;
       g_snprintf( s->por, sizeof s->por, "%s", p->m[m].por[0] ? p->m[m].por : "analista" );
       g_snprintf( s->razon, sizeof s->razon, "%s", p->m[m].razon );
       if ( !s->datos )
           {
           f->pasos++;
           if ( !strcmp( s->por, "asistente" ) ) f->del_asistente++;
           }
       }
   f->elegido = p->m[i].elegido;
   if ( f->elegido )
       g_snprintf( f->por_elegido, sizeof f->por_elegido, "%s",
                   p->m[i].por_elegido[0] ? p->m[i].por_elegido : "analista" );

   /* 2. LA ADECUACION */
   f->estimado = fi_dictamen( p, serie, muestra, id, &f->dx ) == 0;

   /* 3 y 4. EL .pre Y EL REGISTRO */
   if ( pr_ruta( p, serie, muestra, id, ".pre", pre, sizeof pre ) == 0 &&
        g_file_test( pre, G_FILE_TEST_EXISTS ) )
       {
       gchar *h = sha_fichero( pre );

       if ( h ) g_snprintf( f->sha_pre, sizeof f->sha_pre, "%s", h );
       g_free( h );
       }

   if ( pr_raiz( p, raiz, sizeof raiz ) != 0 ||
        coste_lee( raiz, &l, &nl, NULL, why, n ) != 0 )
       nl = 0;                          /* sin registro: todo «no consta» */
   if ( why && n ) why[0] = '\0';

   if ( f->npaso == 0 ) { coste_lineas_libera( l, nl ); return 0; }
   objs = g_new0( gchar *, f->npaso + 1 );
   for ( j = 0; j < f->npaso; j++ ) objs[j] = objeto_de( serie, muestra, f->paso[j].id );
   for ( j = 0; j < nl; j++ )
       {
       const CosteLinea *x = &l[j];
       gboolean          mio = !strcmp( x->objeto, objs[f->npaso - 1] );
       int               t;

       /* LA ULTIMA de cada cosa: el registro va en orden. */
       if ( mio && !strcmp( x->paso, "estimar" ) ) corrida_de( &f->estimacion, x );
       if ( f->sha_pre[0] && !strcmp( x->sha_pre, f->sha_pre ) )
           {
           if ( es_reproducir( x->paso ) )
               {
               corrida_de( &f->rep, x );
               f->reproduce = !strcmp( x->paso, "reproducir:si" );
               }
           else corrida_de( &f->origen, x );
           }
       for ( t = 0; t < f->npaso; t++ )
           if ( !strcmp( x->objeto, objs[t] ) )
               {
               f->corridas++;
               f->cpu_s   += x->cpu_s;
               f->pared_s += x->pared_s;
               if ( x->energia_j >= 0.0 ) { f->energia_j += x->energia_j; f->con_energia++; }
               if ( !strcmp( x->via, "python" ) ) f->en_python++;
               break;
               }
       }
   g_strfreev( objs );
   coste_lineas_libera( l, nl );
   return 0;
}

/* ------------------------------------------------------------------------ */
/* El Markdown                                                               */
/* ------------------------------------------------------------------------ */

/* Un numero con COMA DECIMAL, como se escribe en español: «0,21». */
static void num( GString *t, double v, int dec )
{
   char  b[G_ASCII_DTOSTR_BUF_SIZE], fmt[8], *c;

   g_snprintf( fmt, sizeof fmt, "%%.%df", dec );
   g_ascii_formatd( b, sizeof b, fmt, v );
   if ( ( c = strchr( b, '.' ) ) != NULL ) *c = ',';
   g_string_append( t, b );
}

/* «2026-10-21T10:14:03» -> «21-oct-2026». Lo que no se entiende, tal cual. */
static void fecha( GString *t, const char *f )
{
   int a, m, d;

   if ( f && sscanf( f, "%4d-%2d-%2d", &a, &m, &d ) == 3 && m >= 1 && m <= 12 )
       g_string_append_printf( t, "%d-%s-%d", d, mes[m - 1], a );
   else
       g_string_append( t, f ? f : "" );
}

/* «fue 1.14 (C)» */
static void motor_via( GString *t, const FiCorrida *c )
{
   g_string_append( t, c->motor[0] ? c->motor : "el motor" );
   if ( c->via[0] ) g_string_append_printf( t, " (%s)", c->via );
}

/* LA ENERGIA, CON SU METODO O NADA. La estimada lleva «~» delante. */
static void energia( GString *t, double e, const char *metodo )
{
   if ( e < 0.0 || !metodo || !*metodo ) return;
   g_string_append( t, ", " );
   if ( strstr( metodo, "TDP" ) ) g_string_append( t, "~" );
   num( t, e, 2 );
   g_string_append_printf( t, " J (%s)", metodo );
}

static const char *signo( DxEstado e )
{
   switch ( e )
       {
       case DX_CUADRA:    return "✓";
       case DX_NO:        return "✗";
       case DX_MIRAR:     return "⚠";
       case DX_NO_APLICA: return "·";
       default:           return "—";
       }
}

gchar *fi_markdown( const Ficha *f )
{
   GString *t = g_string_new( "" );
   int      i;

   g_string_append_printf( t, "## Ficha de confianza · %s/", f->serie );
   if ( f->muestra[0] ) g_string_append_printf( t, "%s/", f->muestra );
   g_string_append_printf( t, "%s\n\n", f->id );

   /* 1. EL RECORRIDO */
   g_string_append_printf( t, "**Recorrido** (%d %s; %d del asistente, %d %%; ",
                           f->pasos, f->pasos == 1 ? "paso" : "pasos", f->del_asistente,
                           f->pasos ? ( 200 * f->del_asistente + f->pasos ) / ( 2 * f->pasos )
                                    : 0 );
   if ( f->elegido ) g_string_append_printf( t, "elegido por el %s)\n", f->por_elegido );
   else              g_string_append( t, "no es el elegido)\n" );
   for ( i = 0; i < f->npaso; i++ )
       {
       const FiPaso *s = &f->paso[i];

       if ( i ) g_string_append( t, " → " );
       g_string_append( t, s->id );
       if ( s->datos ) { g_string_append( t, " datos" ); continue; }
       g_string_append_printf( t, " %s ", s->por );
       if ( s->razon[0] ) g_string_append_printf( t, "«%s»", s->razon );
       else               g_string_append( t, "(sin razón)" );
       }
   g_string_append( t, "\n\n" );

   /* 2. LA ADECUACION */
   g_string_append( t, "**Adecuación**\n" );
   if ( !f->estimado )
       g_string_append( t, "— no consta: el modelo no está estimado (no hay .out)\n" );
   else
       {
       for ( i = 0; i < f->dx.n; i++ )
           {
           const DxLinea *l = &f->dx.l[i];

           g_string_append_printf( t, "- %s %s: ", signo( l->estado ), l->titulo );
           if ( l->estado == DX_NO_CONSTA )      g_string_append( t, "no consta" );
           else if ( l->estado == DX_NO_APLICA ) g_string_append( t, "no aplica" );
           else
               {
               g_string_append( t, l->dato[0] ? l->dato : dx_estado_es( l->estado ) );
               if ( l->estado == DX_MIRAR ) g_string_append( t, " (mirar)" );
               }
           g_string_append_c( t, '\n' );
           }
       /* LO QUE EL DICTAMEN NO MIRA, Y NADIE HA APUNTADO QUE SE HICIERA. */
       g_string_append( t, "- — sobreparametrización: no consta\n"
                           "- — estabilidad: no consta\n" );
       }
   g_string_append_c( t, '\n' );

   /* 3. LA REPRODUCIBILIDAD */
   g_string_append( t, "**Reproducibilidad**  " );
   if ( !f->sha_pre[0] )
       g_string_append( t, "no consta: el modelo no tiene .pre" );
   else if ( !f->rep.consta )
       g_string_append( t, "no consta: pídela con atsw-cli ficha --comprobar" );
   else
       {
       g_string_append( t, f->reproduce ? "✓ se reproduce con " : "✗ no se reproduce con " );
       motor_via( t, &f->rep );
       g_string_append( t, ", comprobado el " );
       fecha( t, f->rep.fecha );
       }
   if ( f->sha_pre[0] && f->origen.consta )
       {
       if ( !strcmp( f->origen.via, "python" ) )
           {
           g_string_append( t, "; el .pre salió del extra Python (" );
           g_string_append( t, f->origen.motor );
           g_string_append( t, ")" );
           }
       else if ( f->rep.consta && strcmp( f->origen.motor, f->rep.motor ) )
           g_string_append_printf( t, "; el .pre lo escribió %s, otra versión del motor",
                                   f->origen.motor );
       }
   g_string_append( t, "\n\n" );

   /* 4. EL COSTE */
   g_string_append( t, "**Coste**  " );
   if ( !f->estimacion.consta && f->corridas == 0 )
       g_string_append( t, "no consta" );
   else
       {
       if ( f->estimacion.consta )
           {
           g_string_append( t, "la estimación, " );
           num( t, f->estimacion.cpu_s, 2 );
           g_string_append_printf( t, " s de CPU (%s%s%s)",
                                   f->estimacion.motor[0] ? f->estimacion.motor : "el motor",
                                   f->estimacion.via[0] ? ", " : "", f->estimacion.via );
           energia( t, f->estimacion.energia_j, f->estimacion.metodo );
           }
       else
           g_string_append( t, "la estimación: no consta" );
       g_string_append( t, "; el recorrido, " );
       num( t, f->cpu_s, 2 );
       g_string_append_printf( t, " s de CPU en %d %s", f->corridas,
                               f->corridas == 1 ? "corrida" : "corridas" );
       if ( f->con_energia > 0 )
           {
           g_string_append( t, ", ~" );
           num( t, f->energia_j, 2 );
           g_string_append( t, " J" );
           }
       if ( f->en_python == 0 )                 g_string_append( t, "; todo en C" );
       else if ( f->en_python == f->corridas )  g_string_append( t, "; todo en Python" );
       else g_string_append_printf( t, "; %d en Python", f->en_python );
       }
   g_string_append_c( t, '\n' );

   return g_string_free( t, FALSE );
}

/* ------------------------------------------------------------------------ */
/* La comprobacion                                                           */
/* ------------------------------------------------------------------------ */

static gboolean es_numero( const char *s, double *v )
{
   char *fin;

   *v = g_ascii_strtod( s, &fin );
   return fin != s && *fin == '\0' && isfinite( *v );
}

/* LOS TOKENS DE UN TEXTO, separados por blancos, cada uno con su linea. */
typedef struct { const char *t; int linea; } Token;

static GArray *tokens( gchar *txt )
{
   GArray *a = g_array_new( FALSE, FALSE, sizeof( Token ) );
   gchar  *c = txt;
   int     linea = 1;

   while ( *c )
       {
       Token k;

       while ( *c && g_ascii_isspace( *c ) ) { if ( *c == '\n' ) linea++; c++; }
       if ( !*c ) break;
       k.t = c;
       k.linea = linea;
       while ( *c && !g_ascii_isspace( *c ) ) c++;
       if ( *c ) { if ( *c == '\n' ) linea++; *c++ = '\0'; }
       g_array_append_val( a, k );
       }
   return a;
}

int fi_compara_pre( const char *a, const char *b, char *dif, size_t n )
{
   gchar  *ta = NULL, *tb = NULL;
   GArray *xa, *xb;
   guint   i;
   int     rc = 1;

   if ( dif && n ) dif[0] = '\0';
   if ( !g_file_get_contents( a, &ta, NULL, NULL ) ||
        !g_file_get_contents( b, &tb, NULL, NULL ) )
       {
       if ( dif ) g_snprintf( dif, n, "no pude leer los dos .pre" );
       g_free( ta ); g_free( tb );
       return -1;
       }
   xa = tokens( ta );
   xb = tokens( tb );
   for ( i = 0; rc == 1 && i < xa->len && i < xb->len; i++ )
       {
       const Token *ka = &g_array_index( xa, Token, i ), *kb = &g_array_index( xb, Token, i );
       double       va, vb;

       if ( !strcmp( ka->t, kb->t ) ) continue;
       if ( es_numero( ka->t, &va ) && es_numero( kb->t, &vb ) &&
            fabs( va - vb ) <= FI_TOL * MAX( 1.0, MAX( fabs( va ), fabs( vb ) ) ) )
           continue;
       rc = 0;
       if ( dif ) g_snprintf( dif, n, "línea %d: %s → %s", ka->linea, ka->t, kb->t );
       }
   if ( rc == 1 && xa->len != xb->len )
       {
       rc = 0;
       if ( dif ) g_snprintf( dif, n, "el .pre que sale tiene %s tokens",
                              xa->len > xb->len ? "menos" : "más" );
       }
   g_array_free( xa, TRUE );
   g_array_free( xb, TRUE );
   g_free( ta );
   g_free( tb );
   return rc;
}

/* El directorio temporal y lo que el motor deja en el: todo fuera. */
static void borra_dir( const char *dir )
{
   GDir       *d = g_dir_open( dir, 0, NULL );
   const char *e;

   while ( d && ( e = g_dir_read_name( d ) ) != NULL )
       {
       gchar *f = g_build_filename( dir, e, NULL );

       if ( g_file_test( f, G_FILE_TEST_IS_DIR ) ) borra_dir( f );
       else g_unlink( f );
       g_free( f );
       }
   if ( d ) g_dir_close( d );
   g_rmdir( dir );
}

int fi_comprueba( const Proyecto *p, const char *serie, const char *muestra,
                  const char *id, const char *quien, const char *por,
                  int *reproduce, char *dif, size_t n )
{
   char         pre[PR_RUTA], raiz[PR_RUTA], why[512];
   gchar       *tmp, *base, *inp, *nuevo, *datos = NULL, *sha, *obj;
   gsize        len = 0;
   EngineResult r;
   CosteLinea  *l;
   int          rc;

   if ( muestra == NULL ) muestra = "";
   *reproduce = 0;
   if ( dif && n ) dif[0] = '\0';
   if ( pr_modelo_idx( p, serie, muestra, id ) < 0 )
       { g_snprintf( dif, n, "«%s» no es un modelo de %s en esa muestra.", id, serie ); return 1; }
   if ( pr_ruta( p, serie, muestra, id, ".pre", pre, sizeof pre ) != 0 ||
        !g_file_get_contents( pre, &datos, &len, NULL ) )
       { g_snprintf( dif, n, "%s/%s no tiene .pre: no hay nada que reproducir.", serie, id );
         return 1; }
   if ( pr_raiz( p, raiz, sizeof raiz ) != 0 )
       { g_free( datos ); g_snprintf( dif, n, "No pude saber la raíz del proyecto." ); return 1; }

   /* EL .pre ES UN .inp: el motor exige la extension, asi que es una copia
      fisica (proyecto.h). En un directorio aparte, para no pisar el .out ni
      los graficos del modelo.                                           */
   tmp = g_dir_make_tmp( "atsw-ficha-XXXXXX", NULL );
   if ( !tmp ) { g_free( datos ); g_snprintf( dif, n, "No pude crear un directorio temporal." );
                 return 1; }
   base = g_path_get_basename( pre );
   base[strlen( base ) - 4] = '\0';
   inp = g_strconcat( tmp, G_DIR_SEPARATOR_S, base, ".inp", NULL );
   nuevo = g_strconcat( tmp, G_DIR_SEPARATOR_S, base, ".pre", NULL );
   sha = g_compute_checksum_for_data( G_CHECKSUM_SHA256, (const guchar *) datos, len );
   if ( !g_file_set_contents( inp, datos, len, NULL ) )
       {
       g_snprintf( dif, n, "No pude copiar el .pre a %s.", inp );
       rc = 1;
       goto fin;
       }

   /* LA MEDIDA ES LA DE SIEMPRE (lib/engine), pero la linea se decide
      despues: se retiene, y se apunta con su resultado y el sha del .pre
      COMPROBADO, no el del que deja la corrida.                         */
   obj = objeto_de( serie, muestra, id );
   coste_contexto( raiz, quien, por, obj, "reproducir" );
   coste_contexto_retiene();
   g_free( obj );
   r = engine_run( tmp, "fue", base, NULL );
   coste_contexto_limpia();
   l = coste_retenida();

   if ( r.status == ENGINE_NORUN )
       {
       g_snprintf( dif, n, "%s", r.message );
       engine_result_clear( &r );
       coste_lineas_libera( l, l ? 1 : 0 );
       rc = 1;
       goto fin;
       }
   if ( g_file_test( nuevo, G_FILE_TEST_EXISTS ) )
       *reproduce = fi_compara_pre( pre, nuevo, dif, n ) == 1;
   else
       g_snprintf( dif, n, "fue no dejó .pre: %s", r.message );
   engine_result_clear( &r );

   rc = 0;
   if ( l )
       {
       g_free( l->paso );
       l->paso = g_strdup( *reproduce ? "reproducir:si" : "reproducir:no" );
       g_free( l->sha_pre );
       l->sha_pre = g_strdup( sha );
       if ( coste_apunta( raiz, l, why, sizeof why ) != 0 )
           {
           g_snprintf( dif, n, "costes.csv: %s", why );
           rc = 1;
           }
       coste_lineas_libera( l, 1 );
       }

fin:
   borra_dir( tmp );
   g_free( tmp ); g_free( base ); g_free( inp ); g_free( nuevo );
   g_free( datos ); g_free( sha );
   return rc;
}
