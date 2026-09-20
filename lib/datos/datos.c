/*
 * datos.c -- ver datos.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "datos.h"

#define LINEA  8192

/* ------------------------------------------------------------------------ */
/* Partir una linea                                                          */
/* ------------------------------------------------------------------------ */

/* Los campos de la linea, en punteros dentro de ella (la linea se modifica).
   Devuelve cuantos. Con separador de blancos, los repetidos no cuentan.    */
static int parte( char *s, char sep, char **campo, int max )
{
   int n = 0;

   while ( *s && n < max )
       {
       if ( sep == ' ' )
           {
           while ( *s == ' ' || *s == '\t' ) s++;
           if ( !*s ) break;
           campo[n++] = s;
           while ( *s && *s != ' ' && *s != '\t' ) s++;
           if ( *s ) *s++ = '\0';
           }
       else
           {
           campo[n++] = s;
           while ( *s && *s != sep ) s++;
           if ( *s ) *s++ = '\0';
           else break;
           /* un separador al final abre un campo vacio */
           if ( !*s && n < max && *( s - 1 ) == '\0' ) { campo[n++] = s; break; }
           }
       }
   return n;
}

static void quita_blancos( char *s )
{
   char *a = s, *b;

   while ( *a == ' ' || *a == '\t' || *a == '\r' ) a++;
   if ( a != s ) memmove( s, a, strlen( a ) + 1 );
   b = s + strlen( s );
   while ( b > s && ( b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r' ) ) b--;
   *b = '\0';
}

/* ------------------------------------------------------------------------ */
/* Numeros                                                                   */
/* ------------------------------------------------------------------------ */

/* Un numero, aceptando la coma decimal si dec es ','. 0 si no lo es.       */
static int numero( const char *s, char dec, double *v )
{
   char  b[64], *fin;
   int   i = 0;

   if ( s == NULL || *s == '\0' ) return 0;
   for ( ; *s && i < (int) sizeof b - 1; s++ )
       b[i++] = ( *s == dec && dec == ',' ) ? '.' : *s;
   b[i] = '\0';

   *v = strtod( b, &fin );
   while ( *fin == ' ' || *fin == '\t' || *fin == '\r' ) fin++;
   return ( fin != b && *fin == '\0' );
}

/* ------------------------------------------------------------------------ */
/* Fechas                                                                    */
/* ------------------------------------------------------------------------ */

/* "4/1977", "1977-04" o "1977". Devuelve 1 y pone periodo y año.          */
static int fecha( const char *s, int *per, int *anio )
{
   int a, p;

   if ( sscanf( s, "%d/%d", &p, &a ) == 2 && a > 1000 )
       { *per = p; *anio = a; return 1; }
   if ( sscanf( s, "%d-%d", &a, &p ) == 2 && a > 1000 )
       { *per = p; *anio = a; return 1; }
   if ( sscanf( s, "%d", &a ) == 1 && a > 1000 && strchr( s, '.' ) == NULL
        && strchr( s, ',' ) == NULL )
       { *per = 1; *anio = a; return 1; }
   return 0;
}

/* La frecuencia, de dos fechas seguidas. 0 si no se puede decir.           */
static int freq_de( int p1, int a1, int p2, int a2 )
{
   if ( a2 == a1 && p2 == p1 + 1 ) return 0;      /* hace falta un salto    */
   if ( a2 == a1 + 1 && p2 == 1 ) return p1;      /* el ultimo periodo era p1 */
   return 0;
}

/* ------------------------------------------------------------------------ */
/* Deducir el separador y el decimal                                         */
/* ------------------------------------------------------------------------ */

/* El separador de una linea de datos: el que da mas de un campo. Se prueban
   en orden de menos ambiguo a mas.                                         */
static char deduce_sep( const char *linea )
{
   static const char cand[] = { ';', '\t', ',', ' ' };
   size_t i;

   for ( i = 0; i < sizeof cand; i++ )
       {
       const char *p;
       int         n = 0;

       for ( p = linea; *p; p++ ) if ( *p == cand[i] ) n++;
       if ( n > 0 ) return ( cand[i] == '\t' ) ? ' ' : cand[i];
       }
   return ' ';
}

/* ------------------------------------------------------------------------ */

static void falla( DtError *e, DtCodigo c, int linea, int campo,
                   const char *texto, int esp, int enc )
{
   if ( e == NULL ) return;
   e->cod   = c;
   e->linea = linea;
   e->campo = campo;
   e->esperaba = esp;
   e->encontro = enc;
   snprintf( e->texto, sizeof e->texto, "%s", texto ? texto : "" );
}

int dt_leer( const char *path, DtDatos *d, DtError *e )
{
   FILE *f;
   char  linea[LINEA], copia[LINEA];
   char *campo[DT_MAX_COL + 1];
   int   nl = 0, ncampo = 0, primera = 1;
   int   p1 = 0, a1 = 0, vistas = 0;

   if ( d == NULL ) return 1;
   memset( d, 0, sizeof *d );
   d->sep = ' ';
   d->dec = '.';
   falla( e, DT_OK, 0, 0, "", 0, 0 );

   f = fopen( path, "r" );
   if ( f == NULL ) { falla( e, DT_ENOFILE, 0, 0, path, 0, 0 ); return 1; }

   while ( fgets( linea, sizeof linea, f ) != NULL )
       {
       int   n, j, col0 = 0;
       char *cr;

       nl++;
       cr = strchr( linea, '\n' ); if ( cr ) *cr = '\0';
       cr = strchr( linea, '\r' ); if ( cr ) *cr = '\0';

       /* --- la cabecera de '#': el convenio de "drtran -e" ------------- */
       if ( linea[0] == '#' )
           {
           int x, y;

           if ( sscanf( linea, "# freq %d", &x ) == 1 ||
                sscanf( linea, "#freq %d", &x ) == 1 ) d->freq = x;
           else if ( sscanf( linea, "# start %d/%d", &x, &y ) == 2 ||
                     sscanf( linea, "#start %d/%d", &x, &y ) == 2 )
               { d->per = x; d->anio = y; }
           continue;
           }

       quita_blancos( linea );
       if ( linea[0] == '\0' ) continue;

       if ( primera ) d->sep = deduce_sep( linea );

       snprintf( copia, sizeof copia, "%s", linea );
       n = parte( copia, d->sep, campo, DT_MAX_COL + 1 );
       if ( n == 0 ) continue;
       for ( j = 0; j < n; j++ ) quita_blancos( campo[j] );

       /* --- la primera linea util: cabecera, fechas, decimal ----------- */
       if ( primera )
           {
           double t;
           int    numerica = 0, pp, aa;

           /* El decimal: si el separador NO es la coma y hay comas en los
              campos, la coma es decimal. Si el separador ES la coma, el
              decimal tiene que ser el punto -- no hay otra opcion.       */
           if ( d->sep != ',' )
               {
               for ( j = 0; j < n; j++ )
                   if ( strchr( campo[j], ',' ) != NULL ) { d->dec = ','; break; }
               }

           for ( j = 0; j < n; j++ )
               if ( numero( campo[j], d->dec, &t ) ) numerica++;

           if ( numerica == 0 && !fecha( campo[0], &pp, &aa ) )
               {
               /* cabecera: se guarda y se pasa a la siguiente linea */
               d->tiene_cabecera = 1;
               for ( j = 0; j < n && j < DT_MAX_COL; j++ )
                   snprintf( d->nombre[j], DT_NOMBRE, "%s", campo[j] );
               ncampo = n;
               continue;
               }
           primera = 0;
           ncampo  = ( ncampo > 0 ) ? ncampo : n;
           }

       if ( n > DT_MAX_COL )
           { falla( e, DT_EMUCHAS, nl, 0, "", DT_MAX_COL, n );
             fclose( f ); return 1; }
       if ( n != ncampo )
           { falla( e, DT_ECOLS, nl, 0, "", ncampo, n );
             fclose( f ); return 1; }
       if ( d->nobs >= DT_MAX_OBS )
           { falla( e, DT_ELARGA, nl, 0, "", DT_MAX_OBS, d->nobs + 1 );
             fclose( f ); return 1; }

       /* --- la columna de fechas ---------------------------------------- */
       {
       int pp, aa;
       double t;

       if ( !numero( campo[0], d->dec, &t ) && fecha( campo[0], &pp, &aa ) )
           {
           d->tiene_fechas = 1;
           col0 = 1;
           snprintf( d->fecha[d->nobs], DT_FECHA, "%s", campo[0] );

           if ( vistas == 0 ) { p1 = pp; a1 = aa; d->per = pp; d->anio = aa; }
           else if ( vistas == 1 && d->freq == 0 )
               {
               int fr = freq_de( p1, a1, pp, aa );

               if ( fr ) d->freq = fr;
               else if ( aa == a1 && pp == p1 + 1 ) d->freq = 0;   /* aun no */
               }
           /* Con dos fechas del mismo año no se sabe la frecuencia; con el
              salto de año si. Se sigue mirando hasta encontrarlo.        */
           else if ( d->freq == 0 && aa == a1 + 1 && pp == 1 )
               d->freq = vistas;
           vistas++;
           }
       else if ( d->tiene_fechas )
           { falla( e, DT_EFECHA, nl, 1, campo[0], 0, 0 );
             fclose( f ); return 1; }
       }

       /* --- los numeros -------------------------------------------------- */
       for ( j = col0; j < n; j++ )
           {
           double v;

           if ( !numero( campo[j], d->dec, &v ) )
               { falla( e, DT_EVALOR, nl, j + 1, campo[j], 0, 0 );
                 fclose( f ); return 1; }
           d->v[j - col0][d->nobs] = v;
           }
       d->nobs++;
       if ( d->ncol < n - col0 ) d->ncol = n - col0;
       }

   fclose( f );

   if ( d->nobs == 0 ) { falla( e, DT_EVACIO, 0, 0, path, 0, 0 ); return 1; }

   /* Si habia columna de fechas, los nombres se corren: el de la columna de
      fechas no nombra ninguna serie.                                      */
   if ( d->tiene_fechas && d->tiene_cabecera )
       {
       int j;

       for ( j = 0; j < d->ncol; j++ )
           memmove( d->nombre[j], d->nombre[j + 1], DT_NOMBRE );
       d->nombre[d->ncol][0] = '\0';
       }

   return 0;
}

int dt_nobs( const DtDatos *d, int c )
{
   if ( d == NULL || c < 0 || c >= d->ncol ) return 0;
   return d->nobs;
}

/* ------------------------------------------------------------------------ */
/* Los dos idiomas. La biblioteca da el HECHO; esto lo redacta.              */
/* ------------------------------------------------------------------------ */

const char *dt_error_es( const DtError *e, char *out, size_t n )
{
   if ( out == NULL || n == 0 ) return out;
   if ( e == NULL ) { out[0] = '\0'; return out; }

   switch ( e->cod )
       {
       case DT_OK:
           out[0] = '\0'; break;
       case DT_ENOFILE:
           snprintf( out, n, "No pude abrir «%s».", e->texto ); break;
       case DT_EVACIO:
           snprintf( out, n, "«%s» no tiene ni una fila de números.",
                     e->texto ); break;
       case DT_EVALOR:
           snprintf( out, n, "Línea %d, campo %d: «%s» no es un número.",
                     e->linea, e->campo, e->texto ); break;
       case DT_ECOLS:
           snprintf( out, n, "Línea %d: %d campos donde las anteriores traían "
                     "%d. Una columna que falta cambia la serie entera.",
                     e->linea, e->encontro, e->esperaba ); break;
       case DT_EMUCHAS:
           snprintf( out, n, "Línea %d: %d columnas, y caben %d.",
                     e->linea, e->encontro, e->esperaba ); break;
       case DT_ELARGA:
           snprintf( out, n, "Más de %d observaciones (línea %d).",
                     e->esperaba, e->linea ); break;
       case DT_EFECHA:
           snprintf( out, n, "Línea %d: «%s» no es una fecha, y las anteriores "
                     "sí lo eran.", e->linea, e->texto ); break;
       default:
           snprintf( out, n, "Error %d.", (int) e->cod ); break;
       }
   return out;
}

const char *dt_error_en( const DtError *e, char *out, size_t n )
{
   if ( out == NULL || n == 0 ) return out;
   if ( e == NULL ) { out[0] = '\0'; return out; }

   switch ( e->cod )
       {
       case DT_OK:
           out[0] = '\0'; break;
       case DT_ENOFILE:
           snprintf( out, n, "can not open %s", e->texto ); break;
       case DT_EVACIO:
           snprintf( out, n, "%s has no numeric rows", e->texto ); break;
       case DT_EVALOR:
           snprintf( out, n, "line %d, field %d: %s is not a number",
                     e->linea, e->campo, e->texto ); break;
       case DT_ECOLS:
           snprintf( out, n, "line %d: %d fields, expected %d",
                     e->linea, e->encontro, e->esperaba ); break;
       case DT_EMUCHAS:
           snprintf( out, n, "line %d: %d columns, at most %d",
                     e->linea, e->encontro, e->esperaba ); break;
       case DT_ELARGA:
           snprintf( out, n, "more than %d observations (line %d)",
                     e->esperaba, e->linea ); break;
       case DT_EFECHA:
           snprintf( out, n, "line %d: %s is not a date", e->linea, e->texto );
           break;
       default:
           snprintf( out, n, "error %d", (int) e->cod ); break;
       }
   return out;
}
