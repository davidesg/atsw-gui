/*
 * proyecto.c -- ver proyecto.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "proyecto.h"

/* ------------------------------------------------------------------------ */
/* Utiles                                                                    */
/* ------------------------------------------------------------------------ */

static void recorta( char *s )
{
   char *a = s, *b;

   while ( *a == ' ' || *a == '\t' || *a == '\r' || *a == '\n' ) a++;
   if ( a != s ) memmove( s, a, strlen( a ) + 1 );
   b = s + strlen( s );
   while ( b > s && ( b[-1] == ' ' || b[-1] == '\t' ||
                      b[-1] == '\r' || b[-1] == '\n' ) ) b--;
   *b = '\0';
}

/* Las comillas del valor, si las lleva. */
static void desentrecomilla( char *s )
{
   size_t n = strlen( s );

   if ( n >= 2 && s[0] == '"' && s[n - 1] == '"' )
       { memmove( s, s + 1, n - 2 ); s[n - 2] = '\0'; }
}

/* Un valor que hay que entrecomillar: vacio, o con algo que confunda. */
/* PARA QUE LOS DOS LECTORES VEAN LO MISMO.
 *
 * Nuestro lector devuelve SIEMPRE texto, pero el de Python no: sin comillas,
 * yaml.safe_load convierte 2026-09-20 en un datetime.date, 12 en un int y
 * "yes" en True. Entonces las dos encarnaciones del taller leen el mismo
 * fichero y obtienen cosas distintas -- que es la version silenciosa del
 * problema de los dos dueños, y se encontro asi: la prueba de lectura desde
 * Python fallo comparando la fecha de bajada con una cadena.
 *
 * Asi que se entrecomilla todo lo que Python interpretaria: una fecha, un
 * numero, un booleano o un nulo. Los identificadores y el texto normal siguen
 * saliendo a pelo, que es lo que hace el manifiesto legible a ojo.        */
static int parece_fecha( const char *s )
{
   int i;

   if ( strlen( s ) != 10 ) return 0;
   for ( i = 0; i < 10; i++ )
       {
       if ( i == 4 || i == 7 ) { if ( s[i] != '-' ) return 0; }
       else if ( s[i] < '0' || s[i] > '9' ) return 0;
       }
   return 1;
}

static int parece_numero( const char *s )
{
   char *fin;

   if ( *s == '\0' ) return 0;
   strtod( s, &fin );
   return ( *fin == '\0' );
}

static int parece_palabra_de_yaml( const char *s )
{
   static const char *p[] = { "true", "false", "yes", "no", "on", "off",
                              "null", "~", NULL };
   int i, j;

   for ( i = 0; p[i]; i++ )
       {
       for ( j = 0; p[i][j] && s[j]; j++ )
           if ( tolower( (unsigned char) s[j] ) != p[i][j] ) break;
       if ( p[i][j] == '\0' && s[j] == '\0' ) return 1;
       }
   return 0;
}

static int hay_que_entrecomillar( const char *s )
{
   if ( *s == '\0' ) return 1;
   if ( strpbrk( s, ":#\"'\n" ) != NULL ) return 1;
   if ( s[0] == ' ' || s[strlen( s ) - 1] == ' ' ) return 1;
   if ( parece_fecha( s ) || parece_numero( s ) ||
        parece_palabra_de_yaml( s ) ) return 1;
   return 0;
}

static void escribe_valor( FILE *f, const char *s )
{
   if ( !hay_que_entrecomillar( s ) ) { fputs( s, f ); return; }
   fputc( '"', f );
   for ( ; *s; s++ )
       {
       if ( *s == '"' || *s == '\\' ) fputc( '\\', f );
       fputc( *s, f );
       }
   fputc( '"', f );
}

static void falla( PrError *e, PrCodigo c, int linea, const char *texto )
{
   if ( e == NULL ) return;
   e->cod   = c;
   e->linea = linea;
   /* Se RECORTA a proposito: el texto es para el mensaje, y una linea de
      2000 caracteres no ayuda a nadie. La precision explicita ademas le
      dice al compilador que el truncamiento es intencionado.          */
   snprintf( e->texto, sizeof e->texto, "%.*s",
             (int) sizeof e->texto - 1, texto ? texto : "" );
}

/* ------------------------------------------------------------------------ */

void pr_nuevo( Proyecto *p, const char *id, const char *titulo,
               const char *raiz )
{
   if ( p == NULL ) return;
   memset( p, 0, sizeof *p );
   p->schema_version = 1;
   snprintf( p->id, sizeof p->id, "%s", id ? id : "proyecto" );
   snprintf( p->titulo, sizeof p->titulo, "%s", titulo ? titulo : "" );
   snprintf( p->raiz, sizeof p->raiz, "%s", ( raiz && *raiz ) ? raiz : "." );

   /* CUANDO SE EMPEZO. Nadie lo ponia, asi que el campo existia y salia
      siempre vacio -- y esa es una fecha que no se puede reconstruir
      despues mirando nada.                                            */
   {
   time_t     t = time( NULL );
   struct tm *g = localtime( &t );

   if ( g ) strftime( p->creado, sizeof p->creado, "%Y-%m-%d", g );
   }
}

int pr_serie_idx( const Proyecto *p, const char *serie )
{
   int i;

   if ( p == NULL || serie == NULL ) return -1;
   for ( i = 0; i < p->ns; i++ )
       if ( strcmp( p->s[i].id, serie ) == 0 ) return i;
   return -1;
}

int pr_modelo_idx( const Proyecto *p, const char *serie, const char *muestra,
                   const char *id )
{
   int i;

   if ( p == NULL || serie == NULL || id == NULL ) return -1;
   if ( muestra == NULL ) muestra = "";
   for ( i = 0; i < p->nm; i++ )
       if ( strcmp( p->m[i].serie, serie ) == 0 &&
            strcmp( p->m[i].muestra, muestra ) == 0 &&
            strcmp( p->m[i].id, id ) == 0 ) return i;
   return -1;
}

int pr_serie_add( Proyecto *p, const char *serie, PrError *e )
{
   if ( p == NULL || serie == NULL || *serie == '\0' )
       { falla( e, PR_ENOSERIE, 0, serie ); return 1; }
   if ( pr_serie_idx( p, serie ) >= 0 )
       { falla( e, PR_EDUP, 0, serie ); return 1; }
   if ( p->ns >= PR_MAX_SERIE )
       { falla( e, PR_EMUCHAS, 0, serie ); return 1; }

   memset( &p->s[p->ns], 0, sizeof p->s[0] );
   snprintf( p->s[p->ns].id, PR_ID, "%s", serie );
   p->ns++;
   return 0;
}

PrSerie *pr_serie( Proyecto *p, const char *id )
{
   int i = pr_serie_idx( p, id );

   return ( i < 0 ) ? NULL : &p->s[i];
}

const PrSerie *pr_serie_ver( const Proyecto *p, const char *id )
{
   int i = pr_serie_idx( p, id );

   return ( i < 0 ) ? NULL : &p->s[i];
}

/* LA DESCRIPCION SI LA HAY, Y SI NO EL ID. Nunca una descripcion inventada
   a partir del id: "IPC_DE" no es "IPC DE" ni nada parecido, y fingir que
   se sabe lo que significa un mnemotecnico es justo lo contrario de para
   lo que existe este campo.                                            */
const char *pr_serie_titulo( const Proyecto *p, const char *id )
{
   const PrSerie *s = pr_serie_ver( p, id );

   if ( s == NULL ) return id ? id : "";
   return s->descripcion[0] ? s->descripcion : s->id;
}

/* --- las muestras ------------------------------------------------------- */

int pr_muestra_idx( const Proyecto *p, const char *id )
{
   int i;

   if ( p == NULL || id == NULL || !*id ) return -1;   /* "" es la completa */
   for ( i = 0; i < p->nmu; i++ )
       if ( strcmp( p->mu[i].id, id ) == 0 ) return i;
   return -1;
}

const PrMuestra *pr_muestra_ver( const Proyecto *p, const char *id )
{
   int i = pr_muestra_idx( p, id );

   return ( i < 0 ) ? NULL : &p->mu[i];
}

int pr_muestra_add( Proyecto *p, const char *id, const char *desde,
                    const char *hasta, const char *razon, PrError *e )
{
   if ( p == NULL ) return 1;

   /* LA COMPLETA NO SE DECLARA: es lo que entro. Dejar crearla daria dos
      formas de decir lo mismo, y una de ellas mentiria en cuanto alguien
      le pusiera un "hasta".                                           */
   if ( id == NULL || !*id )
       { falla( e, PR_EDUP, 0, "(la muestra completa)" ); return 1; }
   if ( pr_muestra_idx( p, id ) >= 0 )
       { falla( e, PR_EDUP, 0, id ); return 1; }
   if ( p->nmu >= PR_MAX_MUESTRA )
       { falla( e, PR_EMUCHAS, 0, id ); return 1; }
   if ( strlen( id ) >= PR_ID )
       { falla( e, PR_EMUCHAS, 0, id ); return 1; }

   memset( &p->mu[p->nmu], 0, sizeof p->mu[0] );
   snprintf( p->mu[p->nmu].id, PR_ID, "%s", id );
   if ( desde ) snprintf( p->mu[p->nmu].desde, 16, "%s", desde );
   if ( hasta ) snprintf( p->mu[p->nmu].hasta, 16, "%s", hasta );
   if ( razon ) snprintf( p->mu[p->nmu].razon, PR_RAZON, "%s", razon );
   p->nmu++;
   return 0;
}

int pr_muestra_borra( Proyecto *p, const char *id, PrError *e )
{
   int i = pr_muestra_idx( p, id ), j;

   if ( i < 0 ) { falla( e, PR_EMUESTRA, 0, id ? id : "" ); return 1; }

   /* LOS MODELOS DE UNA MUESTRA NO SE VAN DE REBOTE. Son estimaciones con
      su .out; que desaparezcan porque se borro una etiqueta seria perder
      trabajo sin decirlo. Se dice CUAL vive ahi.                      */
   for ( j = 0; j < p->nm; j++ )
       if ( strcmp( p->m[j].muestra, id ) == 0 )
           {
           char b[PR_TEXTO];

           snprintf( b, sizeof b, "%s/%s", p->m[j].serie, p->m[j].id );
           falla( e, PR_EENMUESTRA, 0, b );
           return 1;
           }

   for ( j = i; j + 1 < p->nmu; j++ ) p->mu[j] = p->mu[j + 1];
   p->nmu--;
   memset( &p->mu[p->nmu], 0, sizeof p->mu[0] );
   return 0;
}

/* EL ELEGIDO ES POR (serie, muestra), Y VIVE EN EL MODELO.
 *
 * Estaba en la serie, y con las hojas no se sostiene: "el modelo de esta
 * serie" solo significa algo DENTRO de una ventana. Puesto en el modelo, la
 * unicidad la impone pr_elige --se le quita la marca al que la tuviera-- y
 * no hay dos sitios que puedan discrepar.                              */

const char *pr_elegido( const Proyecto *p, const char *serie,
                        const char *muestra )
{
   int i;

   if ( p == NULL || serie == NULL ) return "";
   if ( muestra == NULL ) muestra = "";
   for ( i = 0; i < p->nm; i++ )
       if ( p->m[i].elegido && strcmp( p->m[i].serie, serie ) == 0 &&
            strcmp( p->m[i].muestra, muestra ) == 0 )
           return p->m[i].id;
   return "";
}

const char *pr_razon_elegido( const Proyecto *p, const char *serie,
                              const char *muestra )
{
   int i;

   if ( p == NULL || serie == NULL ) return "";
   if ( muestra == NULL ) muestra = "";
   for ( i = 0; i < p->nm; i++ )
       if ( p->m[i].elegido && strcmp( p->m[i].serie, serie ) == 0 &&
            strcmp( p->m[i].muestra, muestra ) == 0 )
           return p->m[i].razon_elegido;
   return "";
}

int pr_elige( Proyecto *p, const char *serie, const char *muestra,
              const char *id, const char *razon, PrError *e )
{
   int i, k;

   if ( pr_serie_idx( p, serie ) < 0 )
       { falla( e, PR_ENOSERIE, 0, serie ? serie : "" ); return 1; }
   if ( muestra == NULL ) muestra = "";

   k = ( id && *id ) ? pr_modelo_idx( p, serie, muestra, id ) : -1;
   if ( id && *id && k < 0 )
       { falla( e, PR_ENOMODELO, 0, id ); return 1; }

   /* DENTRO DE UNA VENTANA SOLO HAY UN ELEGIDO. */
   for ( i = 0; i < p->nm; i++ )
       if ( strcmp( p->m[i].serie, serie ) == 0 &&
            strcmp( p->m[i].muestra, muestra ) == 0 )
           { p->m[i].elegido = 0; p->m[i].razon_elegido[0] = '\0'; }

   if ( k >= 0 )
       {
       p->m[k].elegido = 1;
       if ( razon ) snprintf( p->m[k].razon_elegido, PR_RAZON, "%s", razon );
       }
   return 0;
}

/* ------------------------------------------------------------------------ */
/* Nombres -> rutas                                                          */
/* ------------------------------------------------------------------------ */

/* La raiz, resuelta contra el directorio del manifiesto. 0 si cupo.
 *
 * Se devuelve el truncamiento en vez de tragarselo: una ruta a medias no
 * apunta a un sitio equivocado, apunta a NINGUNO, y el que llama tiene que
 * enterarse antes de abrir nada con ella.                              */
static int raiz_real( const Proyecto *p, char *out, size_t n )
{
   char dir[PR_RUTA], *s;
   int  esc;

   if ( p->raiz[0] == '/' )
       {
       esc = snprintf( out, n, "%s", p->raiz );
       return ( esc < 0 || (size_t) esc >= n );
       }

   snprintf( dir, sizeof dir, "%s", p->path );
   s = strrchr( dir, '/' );
   if ( s ) *s = '\0'; else snprintf( dir, sizeof dir, "." );

   if ( strcmp( p->raiz, "." ) == 0 ) esc = snprintf( out, n, "%s", dir );
   else                               esc = snprintf( out, n, "%s/%s", dir,
                                                      p->raiz );
   return ( esc < 0 || (size_t) esc >= n );
}

int pr_ruta( const Proyecto *p, const char *serie, const char *muestra,
             const char *id, const char *ext, char *out, size_t n )
{
   char raiz[PR_RUTA];
   int  esc;

   if ( p == NULL || out == NULL || n == 0 ) return 1;
   out[0] = '\0';
   if ( serie == NULL || *serie == '\0' ) return 1;
   if ( muestra == NULL ) muestra = "";

   if ( raiz_real( p, raiz, sizeof raiz ) != 0 ) return 1;

   if ( id == NULL || *id == '\0' )
       esc = snprintf( out, n, "%s/%s", raiz, serie );
   else if ( *muestra )
       /* LA SUBMUESTRA TIENE CARPETA, y por eso m01 de la completa y m01 de
          pre-covid pueden llamarse igual sin pisarse. La COMPLETA no la
          tiene: es "" -- no se declara -- asi que no tiene nombre que
          poner aqui.                                                     */
       esc = snprintf( out, n, "%s/%s/%s/work/%s_%s%s", raiz, serie, muestra,
                       serie, id, ext ? ext : "" );
   else
       /* EL NOMBRE ES CORTESIA: se compone para que el directorio se entienda
          a ojo, y NADIE lo vuelve a leer. La identidad esta en el
          manifiesto.                                                      */
       esc = snprintf( out, n, "%s/%s/work/%s_%s%s", raiz, serie, serie, id,
                       ext ? ext : "" );

   return ( esc < 0 || (size_t) esc >= n ) ? 1 : 0;
}

int pr_hijos( const Proyecto *p, const char *serie, const char *muestra,
              const char *id, char hijos[][PR_ID], int max )
{
   int i, n = 0;

   if ( !p || !serie || !id || !*id ) return 0;
   if ( !muestra ) muestra = "";
   for ( i = 0; i < p->nm; i++ )
       {
       if ( strcmp( p->m[i].serie, serie ) ) continue;
       if ( strcmp( p->m[i].muestra, muestra ) ) continue;
       if ( strcmp( p->m[i].padre, id ) ) continue;
       if ( hijos && n < max ) snprintf( hijos[n], PR_ID, "%s", p->m[i].id );
       n++;
       }
   return n;
}

int pr_herramienta( const Proyecto *p )
{
   return ( p && p->herramienta == 1 ) ? 1 : 0;
}

void pr_pon_herramienta( Proyecto *p, int editor )
{
   if ( p ) p->herramienta = editor ? 1 : 0;
}

/* Los ultimos k componentes de ruta. Devuelve NULL si no llega a tener k. */
static const char *cola( const char *ruta, int k )
{
   const char *p = ruta + strlen( ruta );
   int         n = 0;

   while ( p > ruta )
       {
       p--;
       if ( *p == '/' || *p == '\\' )
          {
          if ( ++n == k ) return p + 1;
          }
       }
   return ( n + 1 >= k ) ? ruta : NULL;
}

int pr_de_ruta( const Proyecto *p, const char *ruta,
                char *serie, size_t ns, char *muestra, size_t nm,
                char *id, size_t nid )
{
   static const char *EXT[] = { ".inp", ".pre", ".out" };
   const char        *q;
   char               cand[PR_RUTA];
   int                i, e, k;

   if ( serie  && ns  ) serie[0]   = '\0';
   if ( muestra && nm ) muestra[0] = '\0';
   if ( id     && nid ) id[0]      = '\0';
   if ( p == NULL || ruta == NULL || *ruta == '\0' ) return 1;

   for ( i = 0; i < p->nm; i++ )
       {
       /* CUANTOS COMPONENTES COMPONE pr_ruta: con submuestra son cuatro
          --serie/muestra/work/base-- y sin ella tres. La completa no se
          declara, asi que no tiene carpeta que contar.               */
       k = ( p->m[i].muestra[0] ) ? 4 : 3;
       q = cola( ruta, k );
       if ( q == NULL ) continue;

       for ( e = 0; e < 3; e++ )
           {
           const char *c;

           if ( pr_ruta( p, p->m[i].serie, p->m[i].muestra, p->m[i].id,
                         EXT[e], cand, sizeof cand ) != 0 ) continue;
           c = cola( cand, k );
           if ( c == NULL || strcmp( c, q ) != 0 ) continue;

           if ( serie   && ns  ) snprintf( serie, ns, "%s", p->m[i].serie );
           if ( muestra && nm  ) snprintf( muestra, nm, "%s", p->m[i].muestra );
           if ( id      && nid ) snprintf( id, nid, "%s", p->m[i].id );
           return 0;
           }
       }
   return 1;
}

/* ------------------------------------------------------------------------ */
/* La cadena                                                                 */
/* ------------------------------------------------------------------------ */

/* EL LINAJE NO SALE DE SU VENTANA, y por eso cada hoja tiene su propio nodo
 * de datos: la misma serie vista por esa ventana. Es una derivacion mas del
 * .csv --que sigue siendo el unico dueño del dato-- exactamente igual que lo
 * es el m00 de la completa.
 *
 * Se intento al reves --un solo nodo de datos, en la total, y los modelos de
 * una submuestra colgando de el-- y dejaba la hoja recien declarada VACIA:
 * sin nada que marcar, no habia de donde empezar un modelo ni que mandar a
 * fug. Y mirar la ACF de la serie recortada es lo PRIMERO que se hace al
 * truncar.                                                             */

int pr_camino( const Proyecto *p, const char *serie, const char *muestra,
               const char *id, char camino[][PR_ID], int max )
{
   int n = 0, i = pr_modelo_idx( p, serie, muestra, id );
   int vueltas = 0;

   while ( i >= 0 )
       {
       if ( n < max ) snprintf( camino[n], PR_ID, "%s", p->m[i].id );
       n++;
       if ( p->m[i].padre[0] == '\0' ) break;
       /* Un linaje que se muerde la cola no puede dar mas pasos que modelos
          hay: si los da, es un ciclo.                                     */
       if ( ++vueltas > p->nm ) return -1;
       i = pr_modelo_idx( p, serie, p->m[i].muestra, p->m[i].padre );
       }
   return n;
}

int pr_deriva( Proyecto *p, const char *serie, const char *muestra,
               const char *padre,
               char *id_out, size_t nid, char *ruta_out, size_t nruta,
               PrError *e )
{
   return pr_deriva_rol( p, serie, muestra, padre, PR_MODELO, id_out, nid,
                         ruta_out, nruta, e );
}

const char *pr_datos_de( const Proyecto *p, const char *serie,
                         const char *muestra )
{
   int i;

   if ( p == NULL || serie == NULL ) return "";
   if ( muestra == NULL ) muestra = "";
   for ( i = 0; i < p->nm; i++ )
      if ( p->m[i].rol == PR_DATOS && strcmp( p->m[i].serie, serie ) == 0 &&
           strcmp( p->m[i].muestra, muestra ) == 0 )
         return p->m[i].id;
   return "";
}

int pr_es_datos( const Proyecto *p, const char *serie, const char *muestra,
                 const char *id )
{
   int i = pr_modelo_idx( p, serie, muestra, id );

   return ( i >= 0 && p->m[i].rol == PR_DATOS );
}

int pr_deriva_rol( Proyecto *p, const char *serie, const char *muestra,
                   const char *padre,
                   PrRol rol, char *id_out, size_t nid,
                   char *ruta_out, size_t nruta, PrError *e )
{
   char id[PR_ID];
   int  i, v = 0;

   if ( p == NULL ) return 1;
   if ( muestra == NULL ) muestra = "";
   if ( pr_serie_idx( p, serie ) < 0 )
       { falla( e, PR_ENOSERIE, 0, serie ); return 1; }

   if ( *muestra && pr_muestra_idx( p, muestra ) < 0 )
       { falla( e, PR_EMUESTRA, 0, muestra ); return 1; }

   /* El padre, en esta ventana o --si es el nodo de datos-- en la total. */
   if ( padre && *padre && pr_modelo_idx( p, serie, muestra, padre ) < 0 )
       { falla( e, PR_EPADRE, 0, padre ); return 1; }
   if ( p->nm >= PR_MAX_MODELO )
       { falla( e, PR_EMUCHAS, 0, serie ); return 1; }

   /* LA VERSION ES DE (serie, muestra). Cada ventana lleva su propio linaje,
      asi que m01 existe en la completa y en pre-covid a la vez y son dos
      modelos distintos. Meter la muestra en el NOMBRE --"m01_A"-- seria
      volver a parsear el nombre para saber algo, que es justo lo que este
      proyecto no hace.                                                 */
   for ( i = 0; i < p->nm; i++ )
       if ( strcmp( p->m[i].serie, serie ) == 0 &&
            strcmp( p->m[i].muestra, muestra ) == 0 && p->m[i].version >= v )
           v = p->m[i].version + 1;

   /* m00 ES DE LOS DATOS, SIEMPRE -- y en una submuestra no hay datos, asi
      que alli los modelos tambien empiezan en m01: el hueco de m00 se deja
      para que "m00 son los datos" valga mirando cualquier hoja.      */
   if ( rol != PR_DATOS && v == 0 ) v = 1;

   snprintf( id, sizeof id, "m%02d", v );
   while ( pr_modelo_idx( p, serie, muestra, id ) >= 0 )
       snprintf( id, sizeof id, "m%02d", ++v );

   memset( &p->m[p->nm], 0, sizeof p->m[0] );
   snprintf( p->m[p->nm].id, PR_ID, "%s", id );
   snprintf( p->m[p->nm].serie, PR_ID, "%s", serie );
   snprintf( p->m[p->nm].muestra, PR_ID, "%s", muestra );
   p->m[p->nm].version = v;
   /* EL LINAJE, SIN PREGUNTAR. Es lo minimo que no se puede perder. */
   snprintf( p->m[p->nm].padre, PR_ID, "%s", ( padre && *padre ) ? padre : "" );
   p->m[p->nm].rol = rol;
   /* La razon NO se pone: "sin razon" tiene que verse como sin razon. */
   p->nm++;

   if ( id_out && nid ) snprintf( id_out, nid, "%s", id );
   if ( ruta_out && nruta )
       pr_ruta( p, serie, muestra, id, ".inp", ruta_out, nruta );
   return 0;
}

int pr_borra( Proyecto *p, const char *serie, const char *muestra,
              const char *id, PrError *e )
{
   int i = pr_modelo_idx( p, serie, muestra, id ), j;

   if ( i < 0 ) { falla( e, PR_ENOMODELO, 0, id ? id : "" ); return 1; }

   /* LOS DATOS NO SE BORRAN. Son la raiz: sin ellos los modelos que quedan
      apuntan a un padre que no esta, que es justo lo que el lector rechaza
      al abrir el manifiesto.                                           */
   if ( p->m[i].rol == PR_DATOS )
       { falla( e, PR_EDATOS, 0, id ); return 1; }

   /* NI UNO CON HIJOS. Se dice CUAL cuelga, no "tiene hijos": el analista
      tiene que saber por donde empezar. Un hijo de OTRA ventana tambien
      cuenta -- un modelo de datos los tiene en todas.                 */
   for ( j = 0; j < p->nm; j++ )
       if ( j != i && strcmp( p->m[j].serie, serie ) == 0 &&
            strcmp( p->m[j].padre, id ) == 0 &&
            pr_modelo_idx( p, serie, muestra, id ) == i )
           { falla( e, PR_EHIJOS, 0, p->m[j].id ); return 1; }

   for ( j = i; j + 1 < p->nm; j++ ) p->m[j] = p->m[j + 1];
   p->nm--;
   memset( &p->m[p->nm], 0, sizeof p->m[0] );
   return 0;
}

int pr_razon( Proyecto *p, const char *serie, const char *muestra,
              const char *id, const char *razon, PrError *e )
{
   int i = pr_modelo_idx( p, serie, muestra, id );

   if ( i < 0 ) { falla( e, PR_ENOMODELO, 0, id ); return 1; }
   snprintf( p->m[i].razon, PR_RAZON, "%s", razon ? razon : "" );
   return 0;
}

int pr_sin_razon( const Proyecto *p, char ids[][PR_ID], int max )
{
   int i, n = 0;

   if ( p == NULL ) return 0;
   for ( i = 0; i < p->nm; i++ )
       /* Los DATOS no llevan razon porque no son una decision. Contarlos
          entre los que la deben daria un aviso imposible de apagar.   */
       if ( p->m[i].razon[0] == '\0' && p->m[i].rol != PR_DATOS )
           {
           /* Con nombre Y APELLIDO, y el apellido incluye la ventana:
              "IPC_DE/m01" es ambiguo en cuanto hay dos hojas.     */
           if ( n < max )
               {
               if ( p->m[i].muestra[0] )
                   snprintf( ids[n], PR_ID, "%s/%s/%s", p->m[i].serie,
                             p->m[i].muestra, p->m[i].id );
               else
                   snprintf( ids[n], PR_ID, "%s/%s", p->m[i].serie,
                             p->m[i].id );
               }
           n++;
           }
   return n;
}

/* ------------------------------------------------------------------------ */
/* El manifiesto                                                             */
/*                                                                           */
/* EL SUBCONJUNTO DE YAML ESTA DECLARADO Y ES PEQUEÑO: "clave: valor" con     */
/* sangria de 0, 2 o 4 espacios, valores a secas o entre comillas. Es lo que  */
/* esta biblioteca escribe y lo que sabe leer. Lo que no entiende LO DICE en  */
/* vez de adivinarlo -- un parser de YAML que adivina es peor que no tenerlo. */
/* Un yaml.safe_load de Python lo lee entero, que es lo que importa para que  */
/* la otra encarnacion pueda mirarlo.                                        */
/* ------------------------------------------------------------------------ */

/* Un campo opcional: si esta vacio, la linea no se escribe. */
static void escribe_campo( FILE *f, const char *clave, const char *valor )
{
   if ( valor == NULL || valor[0] == '\0' ) return;
   fprintf( f, "    %s: ", clave );
   escribe_valor( f, valor );
   fputc( '\n', f );
}

int pr_escribir( const Proyecto *p, const char *path, PrError *e )
{
   FILE *f;
   int   i;

   if ( p == NULL || path == NULL ) return 1;
   f = fopen( path, "w" );
   if ( f == NULL ) { falla( e, PR_EESCRIBIR, 0, path ); return 1; }

   fprintf( f, "# Proyecto de ATSW GUI. El nombre de cada fichero es CORTESIA:\n"
               "# la identidad esta aqui, y ningun programa parsea un nombre.\n"
               "schema_version: %d\n", p->schema_version );
   fprintf( f, "id: " );        escribe_valor( f, p->id );       fputc( '\n', f );
   fprintf( f, "titulo: " );    escribe_valor( f, p->titulo );   fputc( '\n', f );
   fprintf( f, "creado: " );    escribe_valor( f, p->creado );   fputc( '\n', f );
   fprintf( f, "analista: " );  escribe_valor( f, p->analista ); fputc( '\n', f );
   /* Con que se abre un modelo derivado. Una preferencia de trabajo, no un
      hecho del analisis -- pero la madre no tiene donde guardarla si no.  */
   fprintf( f, "herramienta: " );
   escribe_valor( f, p->herramienta ? "editor" : "fue" );
   fputc( '\n', f );
   fprintf( f, "raiz: " );      escribe_valor( f, p->raiz );     fputc( '\n', f );

   fprintf( f, "\nseries:\n" );
   for ( i = 0; i < p->ns; i++ )
       {
       fprintf( f, "  %s:\n", p->s[i].id );
       /* LOS METADATOS, SOLO SI LOS HAY. Una serie sin ellos no escribe
          seis lineas vacias: el manifiesto se lee a ojo, y seis "" por
          serie lo entierran.                                          */
       escribe_campo( f, "descripcion", p->s[i].descripcion );
       escribe_campo( f, "fuente",      p->s[i].fuente );
       escribe_campo( f, "url",         p->s[i].url );
       escribe_campo( f, "bajada",      p->s[i].bajada );
       escribe_campo( f, "unidades",    p->s[i].unidades );
       escribe_campo( f, "notas",       p->s[i].notas );
       }

   /* LAS MUESTRAS, SOLO SI LAS HAY. La completa es "" y no se declara: un
      proyecto que nunca trunque nada no escribe nada de esto.        */
   if ( p->nmu > 0 )
       {
       fprintf( f, "\n# Ventanas declaradas sobre los datos. La COMPLETA es la\n"
                   "# muestra total y no se declara: es lo que entro.\n"
                   "muestras:\n" );
       for ( i = 0; i < p->nmu; i++ )
           {
           fprintf( f, "  %s:\n", p->mu[i].id );
           escribe_campo( f, "desde", p->mu[i].desde );
           escribe_campo( f, "hasta", p->mu[i].hasta );
           fprintf( f, "    razon: " ); escribe_valor( f, p->mu[i].razon );
           fputc( '\n', f );
           }
       }

   fprintf( f, "\n# La cadena de iteracion. padre vacio = raiz.\n"
               "# razon vacia = NO CONSTA, y no consta nunca significa cuadra.\n"
               "modelos:\n" );
   for ( i = 0; i < p->nm; i++ )
       {
       /* LA CLAVE ES (serie, muestra, id). La completa no pone muestra --
          no tiene nombre-- asi que son dos barras o tres, y se distingue
          contandolas.                                                   */
       if ( p->m[i].muestra[0] )
           fprintf( f, "  %s/%s/%s:\n", p->m[i].serie, p->m[i].muestra,
                    p->m[i].id );
       else
           fprintf( f, "  %s/%s:\n", p->m[i].serie, p->m[i].id );
       fprintf( f, "    version: %d\n", p->m[i].version );
       fprintf( f, "    padre: " );  escribe_valor( f, p->m[i].padre );
       fputc( '\n', f );
       fprintf( f, "    razon: " );  escribe_valor( f, p->m[i].razon );
       fputc( '\n', f );
       /* El rol SOLO cuando no es lo normal: un manifiesto lleno de
          "rol: modelo" no dice nada y se lee peor.                    */
       if ( p->m[i].rol == PR_DATOS ) fprintf( f, "    rol: datos\n" );
       /* La muestra SOLO si no es la completa, por la misma razon que el
          rol: un manifiesto lleno de "muestra: completa" no dice nada. */
       if ( p->m[i].elegido )
           {
           fprintf( f, "    elegido: si\n" );
           escribe_campo( f, "razon_elegido", p->m[i].razon_elegido );
           }
       if ( p->m[i].creado[0] )
           { fprintf( f, "    creado: " ); escribe_valor( f, p->m[i].creado );
             fputc( '\n', f ); }
       }

   if ( fclose( f ) != 0 ) { falla( e, PR_EESCRIBIR, 0, path ); return 1; }
   return 0;
}

/* Parte "clave: valor". Devuelve 0 si la linea no tiene esa forma. */
static int clave_valor( char *s, char **clave, char **valor )
{
   char *c = strchr( s, ':' );

   if ( c == NULL ) return 0;
   *c = '\0';
   *clave = s;
   *valor = c + 1;
   recorta( *clave );
   recorta( *valor );
   desentrecomilla( *valor );
   return 1;
}

/* UN IDENTIFICADOR NO SE RECORTA: SE RECHAZA.
 *
 * Recortar un nombre de 60 caracteres a 47 puede hacerlo casar con OTRO --o
 * dejar de casar con su padre-- y las dos cosas rompen el linaje en silencio.
 * Para el texto libre (titulo, razon) recortar es inocuo; para una clave, no.
 * 0 si cupo.                                                            */
static int pon_id( char *dst, const char *valor, PrError *e, int nl )
{
   if ( strlen( valor ) >= PR_ID )
       { falla( e, PR_ESINTAXIS, nl, valor ); return 1; }
   snprintf( dst, PR_ID, "%s", valor );
   return 0;
}

static int sangria( const char *s )
{
   int n = 0;

   while ( *s == ' ' ) { n++; s++; }
   return n;
}

int pr_leer( const char *path, Proyecto *p, PrError *e )
{
   FILE *f;
   char  linea[2048], copia[2048];
   int   nl = 0, seccion = 0;     /* 0 raiz, 1 series, 2 modelos, 3 muestras */
   int   cur = -1;                /* la serie o el modelo en curso         */

   if ( p == NULL ) return 1;
   pr_nuevo( p, "", "", "." );
   falla( e, PR_OK, 0, "" );
   snprintf( p->path, sizeof p->path, "%s", path ? path : "" );

   f = fopen( path, "r" );
   if ( f == NULL ) { falla( e, PR_ENOFILE, 0, path ); return 1; }

   while ( fgets( linea, sizeof linea, f ) != NULL )
       {
       char *clave, *valor;
       int   san;

       nl++;
       snprintf( copia, sizeof copia, "%s", linea );
       { char *h = strchr( copia, '\n' ); if ( h ) *h = '\0'; }
       { char *h = strchr( copia, '\r' ); if ( h ) *h = '\0'; }

       if ( copia[0] == '#' ) continue;
       san = sangria( copia );
       { char t[2048]; snprintf( t, sizeof t, "%s", copia ); recorta( t );
         if ( t[0] == '\0' ) continue;
         if ( t[0] == '#' ) continue; }

       if ( !clave_valor( copia, &clave, &valor ) )
           { falla( e, PR_ESINTAXIS, nl, copia ); fclose( f ); return 1; }

       /* --- nivel 0: las cabeceras y las dos secciones ------------------ */
       if ( san == 0 )
           {
           cur = -1;
           if ( strcmp( clave, "series" ) == 0 )   { seccion = 1; continue; }
           if ( strcmp( clave, "modelos" ) == 0 )  { seccion = 2; continue; }
           if ( strcmp( clave, "muestras" ) == 0 ) { seccion = 3; continue; }
           seccion = 0;

           if ( strcmp( clave, "schema_version" ) == 0 )
               p->schema_version = atoi( valor );
           else if ( strcmp( clave, "id" ) == 0 )
               { if ( pon_id( p->id, valor, e, nl ) ) { fclose( f ); return 1; } }
           else if ( strcmp( clave, "titulo" ) == 0 )
               snprintf( p->titulo, PR_TEXTO, "%s", valor );
           else if ( strcmp( clave, "creado" ) == 0 )
               snprintf( p->creado, sizeof p->creado, "%s", valor );
           else if ( strcmp( clave, "analista" ) == 0 )
               snprintf( p->analista, PR_TEXTO, "%s", valor );
           else if ( strcmp( clave, "raiz" ) == 0 )
               snprintf( p->raiz, PR_RUTA, "%s", valor );
           else if ( strcmp( clave, "herramienta" ) == 0 )
               p->herramienta = ( strcmp( valor, "editor" ) == 0 );
           else
               { falla( e, PR_ECLAVE, nl, clave ); fclose( f ); return 1; }
           continue;
           }

       /* --- nivel 1: el nombre de la serie o del modelo ----------------- */
       if ( san == 2 )
           {
           if ( seccion == 1 )
               {
               if ( strlen( clave ) >= PR_ID )
                   { falla( e, PR_ESINTAXIS, nl, clave ); fclose( f ); return 1; }
               if ( pr_serie_add( p, clave, e ) != 0 )
                   { if ( e ) e->linea = nl; fclose( f ); return 1; }
               cur = p->ns - 1;
               }
           else if ( seccion == 2 )
               {
               /* SERIE/id  -> la muestra completa
                  SERIE/muestra/id -> una submuestra                     */
               char *b1 = strchr( clave, '/' );
               char *b2, *mu = "", *id;

               if ( b1 == NULL )
                   { falla( e, PR_ESINTAXIS, nl, clave ); fclose( f ); return 1; }
               *b1 = '\0';
               b2 = strchr( b1 + 1, '/' );
               if ( b2 ) { *b2 = '\0'; mu = b1 + 1; id = b2 + 1; }
               else      { id = b1 + 1; }

               if ( p->nm >= PR_MAX_MODELO )
                   { falla( e, PR_EMUCHAS, nl, clave ); fclose( f ); return 1; }
               if ( pr_modelo_idx( p, clave, mu, id ) >= 0 )
                   { falla( e, PR_EDUP, nl, clave ); fclose( f ); return 1; }

               memset( &p->m[p->nm], 0, sizeof p->m[0] );
               if ( pon_id( p->m[p->nm].muestra, mu, e, nl ) ||
                    pon_id( p->m[p->nm].serie, clave, e, nl ) ||
                    pon_id( p->m[p->nm].id, id, e, nl ) )
                   { fclose( f ); return 1; }
               cur = p->nm++;
               }
           else if ( seccion == 3 )
               {
               if ( pr_muestra_add( p, clave, NULL, NULL, NULL, e ) != 0 )
                   { if ( e ) e->linea = nl; fclose( f ); return 1; }
               cur = p->nmu - 1;
               }
           else
               { falla( e, PR_ECLAVE, nl, clave ); fclose( f ); return 1; }
           continue;
           }

       /* --- nivel 2: los campos ---------------------------------------- */
       if ( san == 4 && cur >= 0 )
           {
           if ( seccion == 1 )
               {
               if ( strcmp( clave, "descripcion" ) == 0 )
                   snprintf( p->s[cur].descripcion, PR_TEXTO, "%s", valor );
               else if ( strcmp( clave, "fuente" ) == 0 )
                   snprintf( p->s[cur].fuente, PR_TEXTO, "%s", valor );
               else if ( strcmp( clave, "url" ) == 0 )
                   snprintf( p->s[cur].url, PR_RUTA, "%s", valor );
               else if ( strcmp( clave, "bajada" ) == 0 )
                   snprintf( p->s[cur].bajada, 16, "%s", valor );
               else if ( strcmp( clave, "unidades" ) == 0 )
                   snprintf( p->s[cur].unidades, PR_TEXTO, "%s", valor );
               else if ( strcmp( clave, "notas" ) == 0 )
                   snprintf( p->s[cur].notas, PR_TEXTO, "%s", valor );
               else
                   { falla( e, PR_ECLAVE, nl, clave ); fclose( f ); return 1; }
               }
           else if ( seccion == 3 )
               {
               if ( strcmp( clave, "desde" ) == 0 )
                   snprintf( p->mu[cur].desde, 16, "%s", valor );
               else if ( strcmp( clave, "hasta" ) == 0 )
                   snprintf( p->mu[cur].hasta, 16, "%s", valor );
               else if ( strcmp( clave, "razon" ) == 0 )
                   snprintf( p->mu[cur].razon, PR_RAZON, "%s", valor );
               else
                   { falla( e, PR_ECLAVE, nl, clave ); fclose( f ); return 1; }
               }
           else if ( seccion == 2 )
               {
               if ( strcmp( clave, "version" ) == 0 )
                   p->m[cur].version = atoi( valor );
               else if ( strcmp( clave, "padre" ) == 0 )
                   { if ( pon_id( p->m[cur].padre, valor, e, nl ) )
                       { fclose( f ); return 1; } }
               else if ( strcmp( clave, "razon" ) == 0 )
                   snprintf( p->m[cur].razon, PR_RAZON, "%s", valor );
               else if ( strcmp( clave, "elegido" ) == 0 )
                   p->m[cur].elegido = ( strcmp( valor, "si" ) == 0 ||
                                         strcmp( valor, "sí" ) == 0 );
               else if ( strcmp( clave, "razon_elegido" ) == 0 )
                   snprintf( p->m[cur].razon_elegido, PR_RAZON, "%s", valor );
               else if ( strcmp( clave, "creado" ) == 0 )
                   snprintf( p->m[cur].creado, 16, "%s", valor );
               else if ( strcmp( clave, "rol" ) == 0 )
                   p->m[cur].rol = !strcmp( valor, "datos" ) ? PR_DATOS
                                                             : PR_MODELO;
               else
                   { falla( e, PR_ECLAVE, nl, clave ); fclose( f ); return 1; }
               }
           continue;
           }

       falla( e, PR_ESINTAXIS, nl, clave );
       fclose( f );
       return 1;
       }

   fclose( f );

   /* EL LINAJE SE COMPRUEBA AL LEER, no cuando alguien lo recorra: un padre
      que no esta, o un ciclo, es un fichero roto y hay que decirlo ya.   */
   {
   int i;

   for ( i = 0; i < p->nm; i++ )
       {
       char camino[PR_MAX_MODELO][PR_ID];

       if ( p->m[i].padre[0] &&
            pr_modelo_idx( p, p->m[i].serie, p->m[i].muestra,
                           p->m[i].padre ) < 0 )
           { falla( e, PR_EPADRE, 0, p->m[i].padre ); return 1; }
       if ( pr_camino( p, p->m[i].serie, p->m[i].muestra, p->m[i].id, camino,
                       PR_MAX_MODELO ) < 0 )
           { falla( e, PR_ECICLO, 0, p->m[i].id ); return 1; }
       }
   }
   return 0;
}

/* ------------------------------------------------------------------------ */

const char *pr_error_es( const PrError *e, char *out, size_t n )
{
   if ( out == NULL || n == 0 ) return out;
   out[0] = '\0';
   if ( e == NULL ) return out;

   switch ( e->cod )
       {
       case PR_OK: break;
       case PR_ENOFILE:
           snprintf( out, n, "No pude abrir «%s».", e->texto ); break;
       case PR_ESINTAXIS:
           snprintf( out, n, "Línea %d: no entiendo «%s». El manifiesto "
                     "admite «clave: valor» con sangría de 0, 2 o 4 espacios.",
                     e->linea, e->texto ); break;
       case PR_ECLAVE:
           snprintf( out, n, "Línea %d: «%s» no es una clave de este nivel.",
                     e->linea, e->texto ); break;
       case PR_EMUCHAS:
           snprintf( out, n, "No caben más (en «%s»).", e->texto ); break;
       case PR_EDUP:
           snprintf( out, n, "«%s» está dos veces.", e->texto ); break;
       case PR_EPADRE:
           snprintf( out, n, "El padre «%s» no existe: el linaje se rompe ahí.",
                     e->texto ); break;
       case PR_ECICLO:
           snprintf( out, n, "El linaje de «%s» se muerde la cola.",
                     e->texto ); break;
       case PR_ENOSERIE:
           snprintf( out, n, "«%s» no es una serie del proyecto.", e->texto );
           break;
       case PR_ENOMODELO:
           snprintf( out, n, "«%s» no es un modelo del proyecto.", e->texto );
           break;
       case PR_EESCRIBIR:
           snprintf( out, n, "No pude escribir «%s».", e->texto ); break;
       case PR_EDATOS:
           snprintf( out, n, "«%s» son los datos de la serie: no se borran, "
                     "que de ahí cuelga todo.", e->texto ); break;
       case PR_EHIJOS:
           snprintf( out, n, "De ese modelo cuelga «%s». Borra antes lo que "
                     "viene de él, o el linaje se rompe.", e->texto ); break;
       case PR_EMUESTRA:
           snprintf( out, n, "«%s» no es una muestra declarada de este "
                     "proyecto.", e->texto ); break;
       case PR_EENMUESTRA:
           snprintf( out, n, "En esa muestra está «%s», que es una estimación "
                     "con su registro. Bórralo antes, o quédate la muestra.",
                     e->texto ); break;
       }
   return out;
}

const char *pr_error_en( const PrError *e, char *out, size_t n )
{
   if ( out == NULL || n == 0 ) return out;
   out[0] = '\0';
   if ( e == NULL ) return out;

   switch ( e->cod )
       {
       case PR_OK: break;
       case PR_ENOFILE:
           snprintf( out, n, "can not open %s", e->texto ); break;
       case PR_ESINTAXIS:
           snprintf( out, n, "line %d: can not parse %s", e->linea, e->texto );
           break;
       case PR_ECLAVE:
           snprintf( out, n, "line %d: unknown key %s", e->linea, e->texto );
           break;
       case PR_EMUCHAS:
           snprintf( out, n, "too many (at %s)", e->texto ); break;
       case PR_EDUP:
           snprintf( out, n, "duplicate %s", e->texto ); break;
       case PR_EPADRE:
           snprintf( out, n, "unknown parent %s", e->texto ); break;
       case PR_ECICLO:
           snprintf( out, n, "cycle in the lineage of %s", e->texto ); break;
       case PR_ENOSERIE:
           snprintf( out, n, "unknown series %s", e->texto ); break;
       case PR_ENOMODELO:
           snprintf( out, n, "unknown model %s", e->texto ); break;
       case PR_EESCRIBIR:
           snprintf( out, n, "can not write %s", e->texto ); break;
       case PR_EDATOS:
           snprintf( out, n, "%s holds the data: it is the root", e->texto );
           break;
       case PR_EHIJOS:
           snprintf( out, n, "%s hangs from it", e->texto ); break;
       case PR_EMUESTRA:
           snprintf( out, n, "unknown sample %s", e->texto ); break;
       case PR_EENMUESTRA:
           snprintf( out, n, "%s lives in it", e->texto ); break;
       }
   return out;
}
