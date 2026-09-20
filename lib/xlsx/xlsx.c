/*
 * xlsx.c -- ver xlsx.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <zlib.h>

#include "xlsx.h"

/* ------------------------------------------------------------------------ */
/* El ZIP                                                                    */
/*                                                                           */
/* Lo justo para sacar una parte por su nombre: el directorio central al      */
/* final del fichero, y de ahi al encabezado local de cada entrada.          */
/* ------------------------------------------------------------------------ */

static unsigned le16( const unsigned char *p )
{
   return (unsigned) p[0] | ( (unsigned) p[1] << 8 );
}

static unsigned long le32( const unsigned char *p )
{
   return (unsigned long) p[0] | ( (unsigned long) p[1] << 8 ) |
          ( (unsigned long) p[2] << 16 ) | ( (unsigned long) p[3] << 24 );
}

static void falla( XlError *e, XlCodigo c, const char *t, int esp, int enc )
{
   if ( e == NULL ) return;
   e->cod = c;
   e->esperaba = esp;
   e->encontro = enc;
   snprintf( e->texto, sizeof e->texto, "%.*s",
             (int) sizeof e->texto - 1, t ? t : "" );
}

/* El fichero entero en memoria: un .xlsx de datos son unos megas. */
static unsigned char *todo( const char *path, long *n )
{
   FILE          *f = fopen( path, "rb" );
   unsigned char *b;

   *n = 0;
   if ( f == NULL ) return NULL;
   fseek( f, 0, SEEK_END ); *n = ftell( f ); fseek( f, 0, SEEK_SET );
   if ( *n <= 0 ) { fclose( f ); return NULL; }
   b = (unsigned char *) malloc( (size_t) *n );
   if ( b == NULL ) { fclose( f ); *n = 0; return NULL; }
   if ( fread( b, 1, (size_t) *n, f ) != (size_t) *n )
       { free( b ); fclose( f ); *n = 0; return NULL; }
   fclose( f );
   return b;
}

/* Saca la parte llamada "nombre". Devuelve un bloque nuevo terminado en '\0'
   (free por el que llama) o NULL.                                         */
static char *parte( const unsigned char *z, long nz, const char *nombre,
                    XlError *e )
{
   long i;
   const unsigned char *eocd = NULL;

   /* El final del directorio central: la firma PK\5\6, buscada desde atras. */
   for ( i = nz - 22; i >= 0 && i > nz - 66000; i-- )
       if ( z[i] == 'P' && z[i+1] == 'K' && z[i+2] == 5 && z[i+3] == 6 )
           { eocd = z + i; break; }
   if ( eocd == NULL ) { falla( e, XL_ENOZIP, nombre, 0, 0 ); return NULL; }

   {
   unsigned long n_ent = le16( eocd + 10 );
   unsigned long off   = le32( eocd + 16 );
   const unsigned char *p = z + off;
   unsigned long k;

   for ( k = 0; k < n_ent && p + 46 <= z + nz; k++ )
       {
       unsigned long nlen = le16( p + 28 ), elen = le16( p + 30 );
       unsigned long clen = le16( p + 32 );
       unsigned long comp = le16( p + 10 );
       unsigned long csz  = le32( p + 20 ), usz = le32( p + 24 );
       unsigned long lho  = le32( p + 42 );

       if ( p + 46 + nlen > z + nz ) break;

       if ( nlen == strlen( nombre ) &&
            memcmp( p + 46, nombre, nlen ) == 0 )
           {
           const unsigned char *lh = z + lho;
           unsigned long ln, le_;
           char *out;

           if ( lh + 30 > z + nz ) { falla( e, XL_ENOZIP, nombre, 0, 0 ); return NULL; }
           ln  = le16( lh + 26 );
           le_ = le16( lh + 28 );
           {
           const unsigned char *dat = lh + 30 + ln + le_;

           if ( dat + csz > z + nz )
               { falla( e, XL_ENOZIP, nombre, 0, 0 ); return NULL; }

           out = (char *) malloc( usz + 1 );
           if ( out == NULL ) { falla( e, XL_EMEM, nombre, 0, 0 ); return NULL; }

           if ( comp == 0 )                     /* guardado sin comprimir */
               memcpy( out, dat, usz );
           else
               {
               z_stream s;

               memset( &s, 0, sizeof s );
               s.next_in   = (Bytef *) dat;
               s.avail_in  = (uInt) csz;
               s.next_out  = (Bytef *) out;
               s.avail_out = (uInt) usz;
               /* -15: deflate EN CRUDO, sin cabecera zlib. Es lo que el
                  formato ZIP guarda.                                   */
               if ( inflateInit2( &s, -15 ) != Z_OK )
                   { free( out ); falla( e, XL_EINFLATE, nombre, 0, 0 ); return NULL; }
               if ( inflate( &s, Z_FINISH ) != Z_STREAM_END )
                   { inflateEnd( &s ); free( out );
                     falla( e, XL_EINFLATE, nombre, 0, 0 ); return NULL; }
               inflateEnd( &s );
               }
           out[usz] = '\0';
           return out;
           }
           }
       p += 46 + nlen + elen + clen;
       }
   }
   falla( e, XL_EPARTE, nombre, 0, 0 );
   return NULL;
}

/* ------------------------------------------------------------------------ */
/* XML: lo justo, y para formas conocidas                                    */
/* ------------------------------------------------------------------------ */

/* El valor de un atributo dentro de una etiqueta. 0 si no esta. */
static int atrib( const char *tag, const char *fin, const char *nombre,
                  char *out, size_t n )
{
   char  busca[32];
   const char *p;

   snprintf( busca, sizeof busca, "%s=\"", nombre );
   p = strstr( tag, busca );
   if ( p == NULL || p >= fin ) return 0;
   p += strlen( busca );
   {
   const char *q = strchr( p, '"' );
   size_t      l;

   if ( q == NULL || q > fin ) return 0;
   l = (size_t) ( q - p );
   if ( l >= n ) l = n - 1;
   memcpy( out, p, l );
   out[l] = '\0';
   }
   return 1;
}

/* Las cinco entidades de XML. In situ. */
static void desentidad( char *s )
{
   char *d = s, *p = s;

   while ( *p )
       {
       if ( *p == '&' )
           {
           if      ( !strncmp( p, "&amp;",  5 ) ) { *d++ = '&';  p += 5; continue; }
           else if ( !strncmp( p, "&lt;",   4 ) ) { *d++ = '<';  p += 4; continue; }
           else if ( !strncmp( p, "&gt;",   4 ) ) { *d++ = '>';  p += 4; continue; }
           else if ( !strncmp( p, "&quot;", 6 ) ) { *d++ = '"';  p += 6; continue; }
           else if ( !strncmp( p, "&apos;", 6 ) ) { *d++ = '\''; p += 6; continue; }
           else if ( p[1] == '#' )
               {
               /* Una referencia numerica: se deja el caracter si es ASCII y
                  si no, se pone '?' -- no se inventa un UTF-8 a medias.  */
               int  c = ( p[2] == 'x' || p[2] == 'X' )
                        ? (int) strtol( p + 3, NULL, 16 )
                        : (int) strtol( p + 2, NULL, 10 );
               char *q = strchr( p, ';' );

               if ( q == NULL ) break;
               *d++ = ( c > 0 && c < 128 ) ? (char) c : '?';
               p = q + 1;
               continue;
               }
           }
       *d++ = *p++;
       }
   *d = '\0';
}

/* ------------------------------------------------------------------------ */
/* Los estilos: QUE numFmt tiene cada s="", y cual de esos es una fecha       */
/* ------------------------------------------------------------------------ */

#define MAX_XF   4096
#define MAX_NFMT 512

typedef struct {
   int  numfmt[MAX_XF];      /* por indice de estilo                       */
   int  nxf;
   int  id[MAX_NFMT];        /* los numFmt propios del fichero             */
   int  esfecha[MAX_NFMT];
   int  nfmt;
} Estilos;

/* Los formatos de fecha que Excel trae de fabrica. */
static int builtin_fecha( int id )
{
   return ( id >= 14 && id <= 22 ) || ( id >= 27 && id <= 36 ) ||
          ( id >= 45 && id <= 47 ) || ( id >= 50 && id <= 58 );
}

/* Un formato propio es fecha si lleva y/m/d/h/s FUERA de las comillas y
   fuera de los corchetes de idioma, como "[$-C0A]mmm\-yy;@".            */
static int codigo_es_fecha( const char *f )
{
   int comillas = 0, corchete = 0;

   for ( ; *f; f++ )
       {
       if ( *f == '"' ) { comillas = !comillas; continue; }
       if ( comillas ) continue;
       if ( *f == '[' ) { corchete = 1; continue; }
       if ( *f == ']' ) { corchete = 0; continue; }
       if ( corchete ) continue;
       if ( *f == '\\' ) { if ( f[1] ) f++; continue; }
       if ( strchr( "ymdhs", *f ) || strchr( "YMDHS", *f ) ) return 1;
       }
   return 0;
}

static void lee_estilos( const char *xml, Estilos *E )
{
   const char *p;

   memset( E, 0, sizeof *E );
   if ( xml == NULL ) return;

   /* Los numFmt propios. */
   for ( p = xml; ( p = strstr( p, "<numFmt " ) ) != NULL; p++ )
       {
       const char *fin = strchr( p, '>' );
       char idt[16], code[256];

       if ( fin == NULL || E->nfmt >= MAX_NFMT ) break;
       if ( atrib( p, fin, "numFmtId", idt, sizeof idt ) &&
            atrib( p, fin, "formatCode", code, sizeof code ) )
           {
           desentidad( code );
           E->id[E->nfmt]      = atoi( idt );
           E->esfecha[E->nfmt] = codigo_es_fecha( code );
           E->nfmt++;
           }
       }

   /* cellXfs: el orden ES el indice de s="". */
   p = strstr( xml, "<cellXfs" );
   if ( p == NULL ) return;
   {
   const char *fin = strstr( p, "</cellXfs>" );

   for ( ; ( p = strstr( p, "<xf " ) ) != NULL; p++ )
       {
       const char *t = strchr( p, '>' );
       char        idt[16];

       if ( t == NULL || ( fin && p > fin ) ) break;
       if ( E->nxf >= MAX_XF ) break;
       E->numfmt[E->nxf++] = atrib( p, t, "numFmtId", idt, sizeof idt )
                             ? atoi( idt ) : 0;
       }
   }
}

static int estilo_es_fecha( const Estilos *E, int s )
{
   int id, i;

   if ( s < 0 || s >= E->nxf ) return 0;
   id = E->numfmt[s];
   if ( builtin_fecha( id ) ) return 1;
   for ( i = 0; i < E->nfmt; i++ )
       if ( E->id[i] == id ) return E->esfecha[i];
   return 0;
}

/* ------------------------------------------------------------------------ */
/* Las fechas                                                                */
/* ------------------------------------------------------------------------ */

void xl_fecha( double serial, int *anio, int *mes, int *dia )
{
   /* De serial de Excel a fecha civil.
    *
    * El origen es 1899-12-30, no 1900-01-01: Excel cree que 1900 fue bisiesto
    * --el 29 de febrero de 1900 no existio-- y ese origen compensa el dia de
    * mas para todo lo posterior a marzo de 1900, que es todo lo que un
    * economista va a cargar.
    *
    * De ahi a dias desde 1970-01-01 hay 25569 dias, y la conversion es el
    * algoritmo civil_from_days, que no tiene casos especiales y se comprueba
    * contra la de Python.                                              */
   long z = (long) ( serial + 0.5 ) - 25569L;   /* dias desde 1970-01-01  */
   long era, doe, yoe, y, doy, mp, d, m;

   z += 719468;                                  /* al origen del algoritmo */
   era = ( z >= 0 ? z : z - 146096 ) / 146097;
   doe = z - era * 146097;                                   /* 0..146096  */
   yoe = ( doe - doe / 1460 + doe / 36524 - doe / 146096 ) / 365;  /* 0..399 */
   y   = yoe + era * 400;
   doy = doe - ( 365 * yoe + yoe / 4 - yoe / 100 );           /* 0..365     */
   mp  = ( 5 * doy + 2 ) / 153;                               /* 0..11      */
   d   = doy - ( 153 * mp + 2 ) / 5 + 1;                      /* 1..31      */
   m   = mp + ( mp < 10 ? 3 : -9 );                           /* 1..12      */

   *anio = (int) ( y + ( m <= 2 ) );
   *mes  = (int) m;
   *dia  = (int) d;
}

/* ------------------------------------------------------------------------ */
/* La hoja                                                                   */
/* ------------------------------------------------------------------------ */

/* "BC12" -> columna 54 (0-indexada). -1 si no hay letras. */
static int columna_de( const char *ref )
{
   int c = 0, hay = 0;

   for ( ; *ref && isalpha( (unsigned char) *ref ); ref++ )
       { c = c * 26 + ( toupper( (unsigned char) *ref ) - 'A' + 1 ); hay = 1; }
   return hay ? c - 1 : -1;
}

/* Los <t> de un trozo, concatenados (el texto enriquecido va en varios). */
static void junta_t( const char *ini, const char *fin, char *out, size_t n )
{
   const char *p = ini;
   size_t      l = 0;

   out[0] = '\0';
   while ( ( p = strstr( p, "<t" ) ) != NULL && p < fin )
       {
       const char *g = strchr( p, '>' );
       const char *c;

       if ( g == NULL || g > fin ) break;
       if ( g[-1] == '/' ) { p = g + 1; continue; }   /* <t/> vacio        */
       c = strstr( g + 1, "</t>" );
       if ( c == NULL || c > fin ) break;
       {
       size_t k = (size_t) ( c - g - 1 );

       if ( l + k >= n ) k = ( n > l + 1 ) ? n - l - 1 : 0;
       memcpy( out + l, g + 1, k );
       l += k;
       out[l] = '\0';
       }
       p = c + 4;
       }
   desentidad( out );
}

int xl_leer( const char *path, XlHoja *h, XlError *e )
{
   unsigned char *z;
   long           nz;
   char          *ss = NULL, *sty = NULL, *sh = NULL, *wb = NULL;
   char         **cad = NULL;
   int            ncad = 0;
   Estilos        E;
   int            rc = 1;

   if ( h == NULL ) return 1;
   memset( h, 0, sizeof *h );
   falla( e, XL_OK, "", 0, 0 );

   z = todo( path, &nz );
   if ( z == NULL ) { falla( e, XL_ENOFILE, path, 0, 0 ); return 1; }

   sh = parte( z, nz, "xl/worksheets/sheet1.xml", e );
   if ( sh == NULL ) goto fin;

   /* Las otras dos pueden faltar en un fichero minimo, y no es un error. */
   { XlError t; ss  = parte( z, nz, "xl/sharedStrings.xml", &t ); }
   { XlError t; sty = parte( z, nz, "xl/styles.xml", &t ); }
   { XlError t; wb  = parte( z, nz, "xl/workbook.xml", &t ); }

   lee_estilos( sty, &E );

   /* Cuantas hojas habia, y como se llama la primera. */
   if ( wb )
       {
       const char *p = wb;

       while ( ( p = strstr( p, "<sheet " ) ) != NULL )
           {
           const char *g = strchr( p, '>' );

           if ( g == NULL ) break;
           if ( h->nhojas == 0 )
               atrib( p, g, "name", h->hoja, sizeof h->hoja );
           h->nhojas++;
           p = g;
           }
       if ( h->hoja[0] ) desentidad( h->hoja );
       }
   if ( h->nhojas == 0 ) h->nhojas = 1;

   /* La tabla de cadenas. */
   if ( ss )
       {
       const char *p;
       int         k = 0;

       for ( p = ss; ( p = strstr( p, "<si" ) ) != NULL; p++ ) k++;
       if ( k > 0 )
           {
           cad = (char **) calloc( (size_t) k, sizeof *cad );
           if ( cad == NULL ) { falla( e, XL_EMEM, "", 0, 0 ); goto fin; }
           for ( p = ss; ( p = strstr( p, "<si" ) ) != NULL && ncad < k; )
               {
               const char *c = strstr( p, "</si>" );
               char        b[XL_TEXTO];

               if ( c == NULL ) break;
               junta_t( p, c, b, sizeof b );
               cad[ncad] = strdup( b );
               ncad++;
               p = c + 5;
               }
           }
       }

   /* --- las celdas ------------------------------------------------------ */
   {
   const char *p;
   int         maxf = 0, maxc = 0;

   /* Primera pasada: cuanto ocupa. */
   for ( p = sh; ( p = strstr( p, "<c " ) ) != NULL; p++ )
       {
       const char *g = strchr( p, '>' );
       char        ref[32];

       if ( g == NULL ) break;
       if ( atrib( p, g, "r", ref, sizeof ref ) )
           {
           int c = columna_de( ref ), f = atoi( ref + strspn( ref,
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" ) );

           if ( c + 1 > maxc ) maxc = c + 1;
           if ( f > maxf ) maxf = f;
           }
       }

   if ( maxf == 0 || maxc == 0 ) { falla( e, XL_EVACIA, path, 0, 0 ); goto fin; }
   if ( maxc > XL_MAX_COL )
       { falla( e, XL_EGRANDE, "columnas", XL_MAX_COL, maxc ); goto fin; }
   if ( maxf > XL_MAX_FILA )
       { falla( e, XL_EGRANDE, "filas", XL_MAX_FILA, maxf ); goto fin; }
   if ( (double) maxf * maxc > XL_MAX_CELDA )
       { falla( e, XL_EGRANDE, "celdas", XL_MAX_CELDA, (int) ( (double) maxf * maxc ) );
         goto fin; }

   h->nfila = maxf;
   h->ncol  = maxc;
   h->c = (XlCelda *) calloc( (size_t) maxf * maxc, sizeof *h->c );
   if ( h->c == NULL ) { falla( e, XL_EMEM, "", 0, 0 ); goto fin; }

   /* Segunda pasada: los valores. */
   for ( p = sh; ( p = strstr( p, "<c " ) ) != NULL; )
       {
       const char *g = strchr( p, '>' );
       const char *cierre, *v;
       char        ref[32], t[16], st[16];
       int         c, f, es;
       XlCelda    *k;

       if ( g == NULL ) break;
       if ( !atrib( p, g, "r", ref, sizeof ref ) ) { p = g + 1; continue; }
       c = columna_de( ref );
       f = atoi( ref + strspn( ref,
               "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" ) );
       if ( c < 0 || f < 1 || c >= maxc || f > maxf ) { p = g + 1; continue; }

       k = &h->c[(size_t) ( f - 1 ) * maxc + c];
       t[0] = '\0'; atrib( p, g, "t", t, sizeof t );
       st[0] = '\0';
       es = atrib( p, g, "s", st, sizeof st ) ? atoi( st ) : -1;

       if ( g[-1] == '/' ) { p = g + 1; continue; }   /* <c .../> vacia     */
       cierre = strstr( g, "</c>" );
       if ( cierre == NULL ) break;

       if ( !strcmp( t, "s" ) )                       /* cadena compartida  */
           {
           v = strstr( g, "<v>" );
           if ( v && v < cierre )
               {
               int i = atoi( v + 3 );

               if ( i >= 0 && i < ncad && cad[i] )
                   { snprintf( k->s, XL_TEXTO, "%s", cad[i] ); k->tipo = XL_TXT; }
               }
           }
       else if ( !strcmp( t, "inlineStr" ) )
           { junta_t( g, cierre, k->s, XL_TEXTO ); k->tipo = XL_TXT; }
       else if ( !strcmp( t, "str" ) )                /* formula que da texto */
           {
           v = strstr( g, "<v>" );
           if ( v && v < cierre )
               {
               const char *c2 = strstr( v, "</v>" );
               size_t      l  = ( c2 && c2 < cierre ) ? (size_t)( c2 - v - 3 ) : 0;

               if ( l >= XL_TEXTO ) l = XL_TEXTO - 1;
               memcpy( k->s, v + 3, l ); k->s[l] = '\0';
               desentidad( k->s );
               k->tipo = XL_TXT;
               }
           }
       else                                           /* numero, o fecha    */
           {
           v = strstr( g, "<v>" );
           if ( v && v < cierre )
               {
               k->v = strtod( v + 3, NULL );
               /* AQUI ESTA TODO: sin los estilos, una fecha es un numero. */
               if ( es >= 0 && estilo_es_fecha( &E, es ) )
                   {
                   int a, m, d;

                   k->tipo = XL_FECHA;
                   xl_fecha( k->v, &a, &m, &d );
                   snprintf( k->s, XL_TEXTO, "%04d-%02d-%02d", a, m, d );
                   }
               else
                   k->tipo = XL_NUM;
               }
           }
       p = cierre + 4;
       }
   }
   rc = 0;

fin:
   if ( cad ) { int i; for ( i = 0; i < ncad; i++ ) free( cad[i] ); free( cad ); }
   free( ss ); free( sty ); free( sh ); free( wb ); free( z );
   if ( rc != 0 ) xl_libre( h );
   return rc;
}

void xl_libre( XlHoja *h )
{
   if ( h == NULL ) return;
   free( h->c );
   h->c = NULL;
   h->nfila = h->ncol = 0;
}

const XlCelda *xl_celda( const XlHoja *h, int fila, int col )
{
   if ( h == NULL || h->c == NULL ) return NULL;
   if ( fila < 0 || fila >= h->nfila || col < 0 || col >= h->ncol ) return NULL;
   return &h->c[(size_t) fila * h->ncol + col];
}

/* ------------------------------------------------------------------------ */

const char *xl_error_es( const XlError *e, char *out, size_t n )
{
   if ( out == NULL || n == 0 ) return out;
   out[0] = '\0';
   if ( e == NULL ) return out;

   switch ( e->cod )
       {
       case XL_OK: break;
       case XL_ENOFILE:
           snprintf( out, n, "No pude abrir «%s».", e->texto ); break;
       case XL_ENOZIP:
           snprintf( out, n, "«%s» no es un .xlsx: un .xlsx es un ZIP, y esto "
                     "no lo es. ¿Es un .xls antiguo?", e->texto ); break;
       case XL_EPARTE:
           snprintf( out, n, "Al libro le falta «%s».", e->texto ); break;
       case XL_EINFLATE:
           snprintf( out, n, "No pude descomprimir «%s».", e->texto ); break;
       case XL_EMEM:
           snprintf( out, n, "Sin memoria." ); break;
       case XL_EGRANDE:
           snprintf( out, n, "%d %s, y caben %d.",
                     e->encontro, e->texto, e->esperaba ); break;
       case XL_EVACIA:
           snprintf( out, n, "La primera hoja de «%s» está vacía.", e->texto );
           break;
       }
   return out;
}

const char *xl_error_en( const XlError *e, char *out, size_t n )
{
   if ( out == NULL || n == 0 ) return out;
   out[0] = '\0';
   if ( e == NULL ) return out;

   switch ( e->cod )
       {
       case XL_OK: break;
       case XL_ENOFILE: snprintf( out, n, "can not open %s", e->texto ); break;
       case XL_ENOZIP:  snprintf( out, n, "%s is not a zip: not an .xlsx",
                                  e->texto ); break;
       case XL_EPARTE:  snprintf( out, n, "missing part %s", e->texto ); break;
       case XL_EINFLATE:snprintf( out, n, "can not inflate %s", e->texto ); break;
       case XL_EMEM:    snprintf( out, n, "out of memory" ); break;
       case XL_EGRANDE: snprintf( out, n, "%d %s, at most %d",
                                  e->encontro, e->texto, e->esperaba ); break;
       case XL_EVACIA:  snprintf( out, n, "%s: first sheet is empty", e->texto );
                        break;
       }
   return out;
}
