/*
 * tabla.c -- ver tabla.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tabla.h"

#define FILAS_PASO  64            /* cuantas filas se reservan de golpe */

typedef struct {
   char   s[TB_TEXTO_MAX];        /* si es texto                          */
   double v;                      /* si es numero                         */
   int    vacio;
} Celda;

typedef struct {
   char   nombre[64];
   char   unidad[32];
   TbTipo tipo;
   int    dec;
} Col;

struct Tabla {
   char  titulo[160];
   Col   col[TB_MAX_COL];
   int   ncol;

   char  pclave[TB_MAX_PROC][48];
   char  pvalor[TB_MAX_PROC][192];
   int   nproc;

   Celda *c;                      /* nfila x TB_MAX_COL                    */
   int    nfila, cap;
};

/* ------------------------------------------------------------------------ */

Tabla *tb_new( const char *titulo )
{
   Tabla *t = (Tabla *) calloc( 1, sizeof( Tabla ) );

   if ( t == NULL ) return NULL;
   if ( titulo ) snprintf( t->titulo, sizeof t->titulo, "%s", titulo );
   return t;
}

void tb_free( Tabla *t )
{
   if ( t == NULL ) return;
   free( t->c );
   free( t );
}

void tb_procedencia( Tabla *t, const char *clave, const char *valor )
{
   if ( t == NULL || t->nproc >= TB_MAX_PROC || clave == NULL ) return;
   snprintf( t->pclave[t->nproc], sizeof t->pclave[0], "%s", clave );
   snprintf( t->pvalor[t->nproc], sizeof t->pvalor[0], "%s",
             valor ? valor : "" );
   t->nproc++;
}

int tb_col( Tabla *t, const char *nombre, const char *unidad,
            TbTipo tipo, int dec )
{
   Col *c;

   if ( t == NULL || t->ncol >= TB_MAX_COL ) return -1;
   c = &t->col[t->ncol];
   snprintf( c->nombre, sizeof c->nombre, "%s", nombre ? nombre : "" );
   snprintf( c->unidad, sizeof c->unidad, "%s", unidad ? unidad : "" );
   c->tipo = tipo;
   c->dec  = ( dec < 0 ) ? 0 : ( dec > 12 ? 12 : dec );
   return t->ncol++;
}

int tb_fila( Tabla *t )
{
   int j;

   if ( t == NULL || t->ncol == 0 ) return -1;

   if ( t->nfila >= t->cap )
       {
       int    cap = t->cap + FILAS_PASO;
       Celda *n   = (Celda *) realloc( t->c,
                        (size_t) cap * TB_MAX_COL * sizeof( Celda ) );

       if ( n == NULL ) return -1;
       t->c   = n;
       t->cap = cap;
       }

   /* Una fila nueva nace VACIA: una celda que no se pone se escribe "-" y no
      basura del realloc.                                                  */
   for ( j = 0; j < TB_MAX_COL; j++ )
       {
       Celda *k = &t->c[(size_t) t->nfila * TB_MAX_COL + j];

       k->s[0] = '\0';
       k->v     = 0.0;
       k->vacio = 1;
       }
   return t->nfila++;
}

static Celda *celda( Tabla *t, int col )
{
   if ( t == NULL || t->nfila == 0 || col < 0 || col >= t->ncol ) return NULL;
   return &t->c[(size_t) ( t->nfila - 1 ) * TB_MAX_COL + col];
}

void tb_pon_txt( Tabla *t, int col, const char *v )
{
   Celda *k = celda( t, col );

   if ( k == NULL ) return;
   snprintf( k->s, sizeof k->s, "%s", v ? v : "" );
   k->vacio = 0;
}

void tb_pon_num( Tabla *t, int col, double v )
{
   Celda *k = celda( t, col );

   if ( k == NULL ) return;
   k->v     = v;
   k->s[0]  = '\0';
   k->vacio = 0;
}

void tb_pon_vacio( Tabla *t, int col )
{
   Celda *k = celda( t, col );

   if ( k != NULL ) k->vacio = 1;
}

int tb_nfilas( const Tabla *t ) { return t ? t->nfila : 0; }
int tb_ncols ( const Tabla *t ) { return t ? t->ncol  : 0; }

/* ------------------------------------------------------------------------ */
/* El texto de una celda, ya formateado. Es el MISMO en los tres formatos.   */
/* ------------------------------------------------------------------------ */

static void texto_celda( const Tabla *t, int fila, int col,
                         char *out, size_t n )
{
   const Celda *k = &t->c[(size_t) fila * TB_MAX_COL + col];
   const Col   *c = &t->col[col];

   if ( k->vacio ) { snprintf( out, n, "-" ); return; }

   switch ( c->tipo )
       {
       case TB_NUM: snprintf( out, n, "%.*f", c->dec, k->v ); break;
       case TB_ENT: snprintf( out, n, "%ld", (long) ( k->v < 0 ? k->v - 0.5
                                                               : k->v + 0.5 ) );
                    break;
       default:     snprintf( out, n, "%s", k->s ); break;
       }
}

/* EL ANCHO EN CARACTERES, NO EN BYTES.
 *
 * strlen cuenta bytes, y "Parámetro" ocupa 10 bytes para 9 caracteres: con
 * "%-*s" la cabecera quedaba una columna corrida respecto a los datos. Es el
 * mismo fallo que descuadro la matriz de covarianzas de la pagina Modelo, y
 * la regla R5 del diseño esta para eso: una tabla que no mantiene los
 * espacios deja de leerse.
 *
 * Se cuentan los bytes que NO son continuacion de UTF-8 (10xxxxxx).     */
static int ancho( const char *s )
{
   int n = 0;

   for ( ; *s; s++ )
       if ( ( (unsigned char) *s & 0xC0 ) != 0x80 ) n++;
   return n;
}

/* s, ajustado a un ancho de w CARACTERES. izq != 0 lo alinea a la izquierda. */
static void pon( FILE *f, const char *s, int w, int izq )
{
   int r = w - ancho( s );

   if ( r < 0 ) r = 0;
   if ( izq ) { fputs( s, f ); while ( r-- ) fputc( ' ', f ); }
   else       { while ( r-- ) fputc( ' ', f ); fputs( s, f ); }
}

/* El encabezado de una columna: "nombre (unidad)" si la lleva. */
static void texto_cabecera( const Col *c, char *out, size_t n )
{
   if ( c->unidad[0] ) snprintf( out, n, "%s (%s)", c->nombre, c->unidad );
   else                snprintf( out, n, "%s", c->nombre );
}

/* ------------------------------------------------------------------------ */
/* CSV                                                                       */
/* ------------------------------------------------------------------------ */

/* RFC 4180: se entrecomilla si lleva coma, comilla o salto; la comilla se
   duplica.                                                                 */
static void csv_campo( FILE *f, const char *s )
{
   const char *p;
   int         hay = 0;

   for ( p = s; *p; p++ )
       if ( *p == ',' || *p == '"' || *p == '\n' || *p == '\r' ) { hay = 1; break; }

   if ( !hay ) { fputs( s, f ); return; }

   fputc( '"', f );
   for ( p = s; *p; p++ )
       {
       if ( *p == '"' ) fputc( '"', f );
       fputc( *p, f );
       }
   fputc( '"', f );
}

int tb_write_csv( const Tabla *t, const char *path )
{
   FILE *f;
   char  b[TB_TEXTO_MAX + 64];
   int   i, j;

   if ( t == NULL || path == NULL ) return 1;
   f = fopen( path, "w" );
   if ( f == NULL ) return 1;

   /* La procedencia, en lineas de '#'. Es el convenio que la suite ya usa en
      el fichero de residuos de "drtran -e".                               */
   if ( t->titulo[0] ) fprintf( f, "# %s\n", t->titulo );
   for ( i = 0; i < t->nproc; i++ )
       fprintf( f, "# %s: %s\n", t->pclave[i], t->pvalor[i] );

   for ( j = 0; j < t->ncol; j++ )
       {
       if ( j ) fputc( ',', f );
       texto_cabecera( &t->col[j], b, sizeof b );
       csv_campo( f, b );
       }
   fputc( '\n', f );

   for ( i = 0; i < t->nfila; i++ )
       {
       for ( j = 0; j < t->ncol; j++ )
           {
           if ( j ) fputc( ',', f );
           texto_celda( t, i, j, b, sizeof b );
           csv_campo( f, b );
           }
       fputc( '\n', f );
       }

   return fclose( f ) ? 1 : 0;
}

/* ------------------------------------------------------------------------ */
/* Texto de ancho fijo, como los cuadros del .out                            */
/* ------------------------------------------------------------------------ */

int tb_write_txt( const Tabla *t, const char *path )
{
   FILE *f;
   char  b[TB_TEXTO_MAX + 64];
   int   an[TB_MAX_COL];
   int   i, j, total = 0;

   if ( t == NULL || path == NULL ) return 1;
   f = fopen( path, "w" );
   if ( f == NULL ) return 1;

   /* El ancho de cada columna: el del contenido mas largo. */
   for ( j = 0; j < t->ncol; j++ )
       {
       texto_cabecera( &t->col[j], b, sizeof b );
       an[j] = ancho( b );
       for ( i = 0; i < t->nfila; i++ )
           {
           int l;

           texto_celda( t, i, j, b, sizeof b );
           l = ancho( b );
           if ( l > an[j] ) an[j] = l;
           }
       total += an[j] + 2;
       }
   if ( total > 2 ) total -= 2;

   if ( t->titulo[0] ) fprintf( f, "%s\n\n", t->titulo );

   for ( j = 0; j < t->ncol; j++ )
       {
       texto_cabecera( &t->col[j], b, sizeof b );
       /* El texto a la izquierda, los numeros a la derecha: es como se lee
          una columna de cifras.                                          */
       /* La ultima columna no se rellena: dejaria espacios al final de cada
          linea, que es basura invisible y molesta al pegar.             */
       if ( j < t->ncol - 1 )
           { pon( f, b, an[j], t->col[j].tipo == TB_TXT ); fputs( "  ", f ); }
       else
           pon( f, b, t->col[j].tipo == TB_TXT ? 0 : an[j], 1 );
       }
   fputc( '\n', f );

   for ( j = 0; j < total; j++ ) fputc( '-', f );
   fputc( '\n', f );

   for ( i = 0; i < t->nfila; i++ )
       {
       for ( j = 0; j < t->ncol; j++ )
           {
           texto_celda( t, i, j, b, sizeof b );
           if ( j < t->ncol - 1 )
               { pon( f, b, an[j], t->col[j].tipo == TB_TXT ); fputs( "  ", f ); }
           else
               pon( f, b, t->col[j].tipo == TB_TXT ? 0 : an[j], 1 );
           }
       fputc( '\n', f );
       }

   if ( t->nproc )
       {
       fputc( '\n', f );
       for ( i = 0; i < t->nproc; i++ )
           fprintf( f, "%s: %s\n", t->pclave[i], t->pvalor[i] );
       }

   return fclose( f ) ? 1 : 0;
}

/* ------------------------------------------------------------------------ */
/* LaTeX                                                                     */
/* ------------------------------------------------------------------------ */

static void tex_escapa( FILE *f, const char *s )
{
   const char *p;

   for ( p = s; *p; p++ )
       switch ( *p )
           {
           case '&': case '%': case '$': case '#':
           case '_': case '{': case '}':
               fputc( '\\', f ); fputc( *p, f ); break;
           case '~': fputs( "\\textasciitilde{}", f ); break;
           case '^': fputs( "\\textasciicircum{}", f ); break;
           case '\\': fputs( "\\textbackslash{}", f ); break;
           default:  fputc( *p, f ); break;
           }
}

int tb_write_tex( const Tabla *t, const char *path )
{
   FILE *f;
   char  b[TB_TEXTO_MAX + 64];
   int   i, j;

   if ( t == NULL || path == NULL ) return 1;
   f = fopen( path, "w" );
   if ( f == NULL ) return 1;

   fputs( "\\begin{table}[htbp]\n  \\centering\n", f );
   if ( t->titulo[0] )
       {
       fputs( "  \\caption{", f );
       tex_escapa( f, t->titulo );
       fputs( "}\n", f );
       }

   fputs( "  \\begin{tabular}{", f );
   for ( j = 0; j < t->ncol; j++ )
       fputc( t->col[j].tipo == TB_TXT ? 'l' : 'r', f );
   fputs( "}\n    \\hline\n    ", f );

   for ( j = 0; j < t->ncol; j++ )
       {
       if ( j ) fputs( " & ", f );
       texto_cabecera( &t->col[j], b, sizeof b );
       tex_escapa( f, b );
       }
   fputs( " \\\\\n    \\hline\n", f );

   for ( i = 0; i < t->nfila; i++ )
       {
       fputs( "    ", f );
       for ( j = 0; j < t->ncol; j++ )
           {
           if ( j ) fputs( " & ", f );
           texto_celda( t, i, j, b, sizeof b );
           tex_escapa( f, b );
           }
       fputs( " \\\\\n", f );
       }

   fputs( "    \\hline\n  \\end{tabular}\n", f );

   /* La procedencia, al pie. No es decoracion: sin ella la tabla no se puede
      reproducir.                                                          */
   if ( t->nproc )
       {
       fputs( "  \\\\[2pt]\n  {\\footnotesize\n", f );
       for ( i = 0; i < t->nproc; i++ )
           {
           fputs( "  ", f );
           tex_escapa( f, t->pclave[i] );
           fputs( ": ", f );
           tex_escapa( f, t->pvalor[i] );
           fputs( " \\\\\n", f );
           }
       fputs( "  }\n", f );
       }

   fputs( "\\end{table}\n", f );
   return fclose( f ) ? 1 : 0;
}

/* ------------------------------------------------------------------------ */

int tb_write( const Tabla *t, const char *path )
{
   const char *p;

   if ( path == NULL ) return 1;
   p = strrchr( path, '.' );

   if ( p != NULL )
       {
       if ( strcmp( p, ".tex" ) == 0 ) return tb_write_tex( t, path );
       if ( strcmp( p, ".txt" ) == 0 ||
            strcmp( p, ".dat" ) == 0 ) return tb_write_txt( t, path );
       }
   return tb_write_csv( t, path );
}
