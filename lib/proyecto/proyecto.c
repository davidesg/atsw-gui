/*
 * proyecto.c -- ver proyecto.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static int hay_que_entrecomillar( const char *s )
{
   if ( *s == '\0' ) return 1;
   if ( strpbrk( s, ":#\"'\n" ) != NULL ) return 1;
   if ( s[0] == ' ' || s[strlen( s ) - 1] == ' ' ) return 1;
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
}

int pr_serie_idx( const Proyecto *p, const char *serie )
{
   int i;

   if ( p == NULL || serie == NULL ) return -1;
   for ( i = 0; i < p->ns; i++ )
       if ( strcmp( p->s[i].id, serie ) == 0 ) return i;
   return -1;
}

int pr_modelo_idx( const Proyecto *p, const char *serie, const char *id )
{
   int i;

   if ( p == NULL || serie == NULL || id == NULL ) return -1;
   for ( i = 0; i < p->nm; i++ )
       if ( strcmp( p->m[i].serie, serie ) == 0 &&
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

const char *pr_elegido( const Proyecto *p, const char *serie )
{
   int i = pr_serie_idx( p, serie );

   return ( i < 0 ) ? "" : p->s[i].elegido;
}

int pr_elige( Proyecto *p, const char *serie, const char *id,
              const char *razon, PrError *e )
{
   int i = pr_serie_idx( p, serie );

   if ( i < 0 ) { falla( e, PR_ENOSERIE, 0, serie ); return 1; }
   if ( id && *id && pr_modelo_idx( p, serie, id ) < 0 )
       { falla( e, PR_ENOMODELO, 0, id ); return 1; }

   snprintf( p->s[i].elegido, PR_ID, "%s", id ? id : "" );
   if ( razon ) snprintf( p->s[i].razon, PR_RAZON, "%s", razon );
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

int pr_ruta( const Proyecto *p, const char *serie, const char *id,
             const char *ext, char *out, size_t n )
{
   char raiz[PR_RUTA];
   int  esc;

   if ( p == NULL || out == NULL || n == 0 ) return 1;
   out[0] = '\0';
   if ( serie == NULL || *serie == '\0' ) return 1;

   if ( raiz_real( p, raiz, sizeof raiz ) != 0 ) return 1;

   if ( id == NULL || *id == '\0' )
       esc = snprintf( out, n, "%s/%s", raiz, serie );
   else
       /* EL NOMBRE ES CORTESIA: se compone para que el directorio se entienda
          a ojo, y NADIE lo vuelve a leer. La identidad esta en el
          manifiesto.                                                      */
       esc = snprintf( out, n, "%s/%s/work/%s_%s%s", raiz, serie, serie, id,
                       ext ? ext : "" );

   return ( esc < 0 || (size_t) esc >= n ) ? 1 : 0;
}

/* ------------------------------------------------------------------------ */
/* La cadena                                                                 */
/* ------------------------------------------------------------------------ */

int pr_camino( const Proyecto *p, const char *serie, const char *id,
               char camino[][PR_ID], int max )
{
   int n = 0, i = pr_modelo_idx( p, serie, id );
   int vueltas = 0;

   while ( i >= 0 )
       {
       if ( n < max ) snprintf( camino[n], PR_ID, "%s", p->m[i].id );
       n++;
       if ( p->m[i].padre[0] == '\0' ) break;
       /* Un linaje que se muerde la cola no puede dar mas pasos que modelos
          hay: si los da, es un ciclo.                                     */
       if ( ++vueltas > p->nm ) return -1;
       i = pr_modelo_idx( p, serie, p->m[i].padre );
       }
   return n;
}

int pr_deriva( Proyecto *p, const char *serie, const char *padre,
               char *id_out, size_t nid, char *ruta_out, size_t nruta,
               PrError *e )
{
   char id[PR_ID];
   int  i, v = 0;

   if ( p == NULL ) return 1;
   if ( pr_serie_idx( p, serie ) < 0 )
       { falla( e, PR_ENOSERIE, 0, serie ); return 1; }
   if ( padre && *padre && pr_modelo_idx( p, serie, padre ) < 0 )
       { falla( e, PR_EPADRE, 0, padre ); return 1; }
   if ( p->nm >= PR_MAX_MODELO )
       { falla( e, PR_EMUCHAS, 0, serie ); return 1; }

   /* La VERSION es un campo: el siguiente numero libre de esta serie. El id
      se compone de el, pero el id no se parsea nunca para recuperarlo.    */
   for ( i = 0; i < p->nm; i++ )
       if ( strcmp( p->m[i].serie, serie ) == 0 && p->m[i].version >= v )
           v = p->m[i].version + 1;

   snprintf( id, sizeof id, "m%02d", v );
   while ( pr_modelo_idx( p, serie, id ) >= 0 )
       snprintf( id, sizeof id, "m%02d", ++v );

   memset( &p->m[p->nm], 0, sizeof p->m[0] );
   snprintf( p->m[p->nm].id, PR_ID, "%s", id );
   snprintf( p->m[p->nm].serie, PR_ID, "%s", serie );
   p->m[p->nm].version = v;
   /* EL LINAJE, SIN PREGUNTAR. Es lo minimo que no se puede perder. */
   snprintf( p->m[p->nm].padre, PR_ID, "%s", ( padre && *padre ) ? padre : "" );
   /* La razon NO se pone: "sin razon" tiene que verse como sin razon. */
   p->nm++;

   if ( id_out && nid ) snprintf( id_out, nid, "%s", id );
   if ( ruta_out && nruta ) pr_ruta( p, serie, id, ".inp", ruta_out, nruta );
   return 0;
}

int pr_razon( Proyecto *p, const char *serie, const char *id,
              const char *razon, PrError *e )
{
   int i = pr_modelo_idx( p, serie, id );

   if ( i < 0 ) { falla( e, PR_ENOMODELO, 0, id ); return 1; }
   snprintf( p->m[i].razon, PR_RAZON, "%s", razon ? razon : "" );
   return 0;
}

int pr_sin_razon( const Proyecto *p, char ids[][PR_ID], int max )
{
   int i, n = 0;

   if ( p == NULL ) return 0;
   for ( i = 0; i < p->nm; i++ )
       if ( p->m[i].razon[0] == '\0' )
           {
           if ( n < max )
               snprintf( ids[n], PR_ID, "%s/%s", p->m[i].serie, p->m[i].id );
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
   fprintf( f, "raiz: " );      escribe_valor( f, p->raiz );     fputc( '\n', f );

   fprintf( f, "\nseries:\n" );
   for ( i = 0; i < p->ns; i++ )
       {
       fprintf( f, "  %s:\n", p->s[i].id );
       fprintf( f, "    elegido: " ); escribe_valor( f, p->s[i].elegido );
       fputc( '\n', f );
       if ( p->s[i].razon[0] )
           { fprintf( f, "    razon: " ); escribe_valor( f, p->s[i].razon );
             fputc( '\n', f ); }
       }

   fprintf( f, "\n# La cadena de iteracion. padre vacio = raiz.\n"
               "# razon vacia = NO CONSTA, y no consta nunca significa cuadra.\n"
               "modelos:\n" );
   for ( i = 0; i < p->nm; i++ )
       {
       fprintf( f, "  %s/%s:\n", p->m[i].serie, p->m[i].id );
       fprintf( f, "    version: %d\n", p->m[i].version );
       fprintf( f, "    padre: " );  escribe_valor( f, p->m[i].padre );
       fputc( '\n', f );
       fprintf( f, "    razon: " );  escribe_valor( f, p->m[i].razon );
       fputc( '\n', f );
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
   int   nl = 0, seccion = 0;     /* 0 raiz, 1 series, 2 modelos           */
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
           if ( strcmp( clave, "series" ) == 0 )  { seccion = 1; continue; }
           if ( strcmp( clave, "modelos" ) == 0 ) { seccion = 2; continue; }
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
               char *barra = strchr( clave, '/' );

               if ( barra == NULL )
                   { falla( e, PR_ESINTAXIS, nl, clave ); fclose( f ); return 1; }
               *barra = '\0';
               if ( p->nm >= PR_MAX_MODELO )
                   { falla( e, PR_EMUCHAS, nl, clave ); fclose( f ); return 1; }
               if ( pr_modelo_idx( p, clave, barra + 1 ) >= 0 )
                   { falla( e, PR_EDUP, nl, clave ); fclose( f ); return 1; }

               memset( &p->m[p->nm], 0, sizeof p->m[0] );
               if ( pon_id( p->m[p->nm].serie, clave, e, nl ) ||
                    pon_id( p->m[p->nm].id, barra + 1, e, nl ) )
                   { fclose( f ); return 1; }
               cur = p->nm++;
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
               if ( strcmp( clave, "elegido" ) == 0 )
                   { if ( pon_id( p->s[cur].elegido, valor, e, nl ) )
                       { fclose( f ); return 1; } }
               else if ( strcmp( clave, "razon" ) == 0 )
                   snprintf( p->s[cur].razon, PR_RAZON, "%s", valor );
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
               else if ( strcmp( clave, "creado" ) == 0 )
                   snprintf( p->m[cur].creado, 16, "%s", valor );
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
            pr_modelo_idx( p, p->m[i].serie, p->m[i].padre ) < 0 )
           { falla( e, PR_EPADRE, 0, p->m[i].padre ); return 1; }
       if ( pr_camino( p, p->m[i].serie, p->m[i].id, camino,
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
       }
   return out;
}
