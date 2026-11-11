/*
 * outvec.c -- ver outvec.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "outvec.h"

/* ------------------------------------------------------------------------ */
/* Recorrer el texto por lineas (como lib/outvarma)                          */
/* ------------------------------------------------------------------------ */

static const char *linea( const char *p, char *b, size_t size )
{
   const char *fin;
   size_t      n;

   if ( !p || !*p ) { if ( size ) b[0] = 0; return NULL; }
   fin = strchr( p, '\n' );
   n   = fin ? (size_t)( fin - p ) : strlen( p );
   if ( n >= size ) n = size - 1;
   memcpy( b, p, n );
   b[n] = 0;
   if ( n && b[n - 1] == '\r' ) b[n - 1] = 0;
   return fin ? fin + 1 : p + strlen( p );
}

static int empieza( const char *l, const char *pre )
{
   return strncmp( l, pre, strlen( pre ) ) == 0;
}

static const char *sb( const char *l )
{
   while ( *l == ' ' || *l == '\t' ) l++;
   return l;
}

static void copia( char *d, size_t n, const char *s )
{
   size_t k;

   if ( !n ) return;
   s = sb( s );
   k = strlen( s );
   while ( k && ( s[k - 1] == ' ' || s[k - 1] == '\t' ) ) k--;
   if ( k >= n ) k = n - 1;
   memcpy( d, s, k );
   d[k] = 0;
}

/* El valor de una linea «Clave   : valor». */
static const char *valor( const char *l )
{
   const char *d = strchr( l, ':' );
   return d ? sb( d + 1 ) : "";
}

/* Una linea de solo numeros: los lee en x (hasta max). Cuantos, o 0 si hay
 * algo que no es un numero.                                              */
static int numeros( const char *l, double *x, int max )
{
   int   n = 0;
   char *fin;

   l = sb( l );
   if ( !*l ) return 0;
   while ( *l )
      {
      double v = strtod( l, &fin );
      if ( fin == l ) return 0;
      if ( n < max ) x[n] = v;
      n++;
      l = sb( fin );
      }
   return n > max ? max : n;
}

/* ------------------------------------------------------------------------ */
/* La cabecera                                                               */
/* ------------------------------------------------------------------------ */

static void cabecera( const char *l, VecOut *v )
{
   if ( empieza( l, "Program " ) ) copia( v->programa, sizeof v->programa, valor( l ) );
   else if ( empieza( l, "Model " ) )
      {
      const char *m = valor( l ), *k;
      sscanf( m, "VARMA-VECM(%d,%d)", &v->p, &v->q );
      if ( strstr( m, "sequential test" ) ) { v->es_rango = 1; v->r = -1; }
      else if ( ( k = strstr( m, "rank r = " ) ) != NULL ) v->r = atoi( k + 9 );
      }
   else if ( empieza( l, "Deterministic " ) )
      {
      const char *d = valor( l );
      copia( v->determinista, sizeof v->determinista, d );
      if ( empieza( d, "case " ) )
         {
         size_t i = 0;
         d += 5;
         while ( d[i] && d[i] != ' ' && i + 1 < sizeof v->caso ) { v->caso[i] = d[i]; i++; }
         v->caso[i] = 0;
         }
      }
   else if ( empieza( l, "Series " ) )
      {
      const char *s = valor( l ), *par = strchr( s, '(' );
      char        tok[OVEC_NOMBRE];
      int         n;

      v->m = 0;
      while ( *s && s != par && v->m < OVEC_MAX_SER &&
              sscanf( s, "%63s%n", tok, &n ) == 1 && tok[0] != '(' )
         {
         copia( v->serie[v->m++], OVEC_NOMBRE, tok );
         s = sb( s + n );
         }
      if ( par ) v->ny2 = atoi( par + 1 );
      }
   else if ( empieza( l, "Sample " ) )
      v->nobs = atoi( valor( l ) );
}

/* ------------------------------------------------------------------------ */
/* Las tablas de la prueba del rango                                         */
/* ------------------------------------------------------------------------ */

/* «  0       4    25.9118    25.82    29.78    33.69   0.1000    99   ...»
 * «  2       2     4.0722    13.75    15.67    20.20   H0 not rejected»   */
static int prueba( const char *l, OvecPrueba *x, int con_p )
{
   char  *fin;
   double d[8];
   int    i;
   const char *s = sb( l );

   for ( i = 0; i < ( con_p ? 8 : 6 ); i++ )
      {
      d[i] = strtod( s, &fin );
      if ( fin == s ) break;
      s = sb( fin );
      }
   if ( i < 3 ) return 0;
   memset( x, 0, sizeof *x );
   x->r = (int) d[0]; x->mr = (int) d[1]; x->lr = d[2];
   x->p = -1.0;
   if ( i >= 6 ) { x->hay_cv = 1; x->cv10 = d[3]; x->cv5 = d[4]; x->cv1 = d[5]; }
   if ( con_p && i >= 8 ) { x->p = d[6]; x->reps = (int) d[7]; }
   return 1;
}

/* ------------------------------------------------------------------------ */
/* La tabla de parametros                                                    */
/* ------------------------------------------------------------------------ */

/* «  D.LRM <- ec1                   -0.165570     0.072981   -2.269  0.0233 *»
 * Los cuatro numeros se leen desde el final (el nombre lleva blancos y
 * puede llevar numeros: «D.LRY <- D.LRM(-1)»).                          */
static int fila_param( const char *l, OvecPar *x )
{
   char        b[256];
   char       *tok[64];
   int         n = 0, i, k;
   const char *s = l;

   if ( l[0] != ' ' ) return 0;
   copia( b, sizeof b, s );
   for ( s = strtok( b, " \t" ); s && n < 64; s = strtok( NULL, " \t" ) ) tok[n++] = (char *) s;
   /* las estrellas de la significacion, fuera */
   while ( n && ( !strcmp( tok[n - 1], "*" ) || !strcmp( tok[n - 1], "**" ) ||
                  !strcmp( tok[n - 1], "***" ) || !strcmp( tok[n - 1], "." ) ) ) n--;
   if ( n < 5 ) return 0;
   for ( i = n - 4; i < n; i++ )
      {
      char *fin;
      strtod( tok[i], &fin );
      if ( *fin ) return 0;
      }
   memset( x, 0, sizeof *x );
   x->est = atof( tok[n - 4] ); x->se = atof( tok[n - 3] );
   x->t   = atof( tok[n - 2] ); x->p  = atof( tok[n - 1] );
   x->nombre[0] = 0;
   for ( k = 0; k < n - 4; k++ )
      {
      if ( k ) strncat( x->nombre, " ", sizeof x->nombre - strlen( x->nombre ) - 1 );
      strncat( x->nombre, tok[k], sizeof x->nombre - strlen( x->nombre ) - 1 );
      }
   return 1;
}

/* ------------------------------------------------------------------------ */
/* El recorrido                                                              */
/* ------------------------------------------------------------------------ */

enum { NADA, T_RANGO, T_ASINT, T_BOOT, T_PARAM, M_ALPHA, M_BETA, M_EW, M_SIGMA };

int ovec_parse( const char *texto, VecOut *v )
{
   char        l[1024], seccion[96] = "";
   const char *p = texto;
   int         estado = NADA, fila = 0, cuenta_cab = 0;
   OvecLR     *lr = NULL;

   memset( v, 0, sizeof *v );
   v->puerta = -1;
   v->ultimo_conv = -1;
   v->r = -1;
   if ( !texto ) return 1;

   while ( ( p = linea( p, l, sizeof l ) ) != NULL )
      {
      const char *s = sb( l );
      double      x[OVEC_MAX_SER];
      int         k;

      if ( cuenta_cab < 14 ) { cabecera( l, v ); cuenta_cab++; }

      /* --- lo que se lee en cualquier sitio ---------------------------- */
      if ( empieza( s, "OPTIMIZER CONVERGED after " ) || empieza( s, "OPTIMIZER STOPPED after " ) )
         {
         int c = empieza( s, "OPTIMIZER CONVERGED" );
         if ( c ) v->n_conv++; else v->n_parado++;
         v->ultimo_conv = c;
         v->ultimo_iter = atoi( strstr( s, "after " ) + 6 );
         v->ultimo_criterio[0] = 0;
         estado = NADA;
         continue;
         }
      if ( empieza( s, "Convergence criterion: " ) )
         { copia( v->ultimo_criterio, sizeof v->ultimo_criterio, s + 23 ); continue; }
      if ( empieza( s, "Search: " ) )
         {
         const char *f = strstr( s, "logL from " );
         sscanf( s, "Search: %d starting points, %d converged", &v->arranques, &v->arranques_conv );
         if ( f ) sscanf( f, "logL from %lf to %lf", &v->busca_min, &v->busca_max );
         continue;
         }
      if ( empieza( s, "sum  =" ) ) { v->puerta_suma = atof( s + 6 ); continue; }
      if ( empieza( s, "joint=" ) ) { v->puerta_conjunta = atof( s + 6 ); continue; }
      if ( empieza( s, "crossing identity (joint - sum) = " ) )
         {
         v->puerta_dif = atof( s + 34 );
         v->puerta = strstr( s, "NOT VERIFIED" ) ? 0 : strstr( s, "VERIFIED" ) ? 1 : -1;
         continue;
         }
      if ( empieza( s, "=== Parametric bootstrap under H0 (" ) )
         { v->B = atoi( s + 35 ); continue; }

      /* --- las cabeceras de las tablas ---------------------------------- */
      if ( empieza( l, "  r    npar        logL" ) ) { estado = T_RANGO; continue; }
      if ( empieza( l, "  r    M-r        LR" ) )
         { estado = strstr( l, "p-value" ) ? T_BOOT : T_ASINT; continue; }
      if ( empieza( l, "Parameter " ) && strstr( l, "Estimate" ) )
         { estado = T_PARAM; seccion[0] = 0; continue; }
      if ( empieza( l, "alpha matrix (M x r)" ) ) { estado = M_ALPHA; fila = 0; continue; }
      if ( empieza( l, "beta matrix (M x r)" ) )  { estado = M_BETA;  fila = 0; continue; }
      if ( empieza( l, "E[W] vector:" ) )         { estado = M_EW;    v->new_ = 0; continue; }
      if ( empieza( l, "Sigma = sigma2 * Q" ) )   { estado = M_SIGMA; fila = 0; continue; }

      /* --- las lineas «clave : valor» del ajuste ------------------------ */
      if ( empieza( l, "sigma2           :" ) ) { v->sigma2 = atof( valor( l ) ); continue; }
      if ( empieza( l, "logelf           :" ) ) { v->logL   = atof( valor( l ) ); continue; }
      if ( empieza( l, "npar             :" ) ) { v->npar   = atoi( valor( l ) ); continue; }
      if ( empieza( l, "AIC              :" ) ) { v->aic    = atof( valor( l ) ); continue; }
      if ( empieza( l, "BIC              :" ) ) { v->bic    = atof( valor( l ) ); continue; }

      /* --- las restricciones -------------------------------------------- */
      if ( empieza( l, "--- " ) && !empieza( l, "--- rank r = " ) )
         {
         if ( v->nlr < OVEC_MAX_LR )
            {
            char t[256];
            size_t n;
            copia( t, sizeof t, l + 4 );
            n = strlen( t );
            while ( n && ( t[n - 1] == '-' || t[n - 1] == ' ' ) ) t[--n] = 0;
            lr = &v->lr[v->nlr];
            memset( lr, 0, sizeof *lr );
            copia( lr->que, sizeof lr->que, t );
            lr->p = -1.0;
            }
         else lr = NULL;
         continue;
         }
      if ( lr && empieza( l, "logL H(r)  free        :" ) ) { lr->libre = atof( valor( l ) ); continue; }
      if ( lr && empieza( l, "logL H1(r) restricted  :" ) ) { lr->restringido = atof( valor( l ) ); continue; }
      if ( lr && empieza( l, "LR = 2(free - restr.)  :" ) ) { lr->lr = atof( valor( l ) ); continue; }
      if ( lr && empieza( l, "degrees of freedom     :" ) ) { lr->gl = atoi( valor( l ) ); continue; }
      if ( lr && empieza( l, "p-value (chi2)         :" ) )
         { lr->p = atof( valor( l ) ); v->nlr++; lr = NULL; continue; }

      /* --- el cuerpo de lo que este abierto ----------------------------- */
      switch ( estado )
         {
         case T_RANGO:
            if ( empieza( s, "---" ) ) break;
            if ( ( k = numeros( l, x, 5 ) ) == 5 && v->nrango < OVEC_MAX_SER )
               {
               OvecRango *g = &v->rango[v->nrango++];
               g->r = (int) x[0]; g->npar = (int) x[1];
               g->logL = x[2]; g->aic = x[3]; g->bic = x[4];
               }
            else estado = NADA;
            break;
         case T_ASINT:
         case T_BOOT:
            if ( empieza( s, "---" ) ) break;
            if ( isdigit( (unsigned char) *s ) )
               {
               OvecPrueba t;
               if ( prueba( l, &t, estado == T_BOOT ) )
                  {
                  if ( estado == T_BOOT && v->nboot < OVEC_MAX_SER ) v->boot[v->nboot++] = t;
                  if ( estado == T_ASINT && v->nasint < OVEC_MAX_SER ) v->asint[v->nasint++] = t;
                  }
               }
            else estado = NADA;
            break;
         case T_PARAM:
            if ( empieza( l, "-----" ) ) { if ( v->npar_t ) estado = NADA; break; }
            if ( !*s ) break;
            if ( l[0] != ' ' ) { copia( seccion, sizeof seccion, l ); break; }
            if ( v->npar_t < OVEC_MAX_PAR )
               {
               OvecPar *q = &v->par[v->npar_t];
               if ( fila_param( l, q ) )
                  { copia( q->seccion, sizeof q->seccion, seccion ); v->npar_t++; }
               }
            break;
         case M_ALPHA:
         case M_BETA:
            if ( ( k = numeros( l, x, OVEC_MAX_SER ) ) > 0 && fila < OVEC_MAX_SER )
               {
               int j;
               for ( j = 0; j < k; j++ )
                  {
                  if ( estado == M_ALPHA ) v->alpha[fila][j] = x[j];
                  else                     v->beta[fila][j]  = x[j];
                  }
               v->nr = k;
               fila++;
               if ( estado == M_BETA ) v->hay_vec = 1;
               }
            else estado = NADA;
            break;
         case M_EW:
            if ( ( k = numeros( l, x, OVEC_MAX_SER ) ) > 0 )
               {
               int j;
               for ( j = 0; j < k && v->new_ < OVEC_MAX_SER; j++ ) v->ew[v->new_++] = x[j];
               }
            else estado = NADA;
            break;
         case M_SIGMA:
            if ( ( k = numeros( l, x, OVEC_MAX_SER ) ) > 0 && fila < OVEC_MAX_SER )
               {
               int j;
               for ( j = 0; j < k; j++ ) v->sigma[fila][j] = v->sigma[j][fila] = x[j];
               fila++;
               }
            else estado = NADA;
            break;
         default:
            break;
         }
      }
   return 0;
}

int ovec_parse_file( const char *path, VecOut *v )
{
   FILE  *f = fopen( path, "rb" );
   char  *b;
   long   n;
   int    rc;

   memset( v, 0, sizeof *v );
   v->puerta = -1; v->ultimo_conv = -1; v->r = -1;
   if ( !f ) return 1;
   fseek( f, 0, SEEK_END );
   n = ftell( f );
   fseek( f, 0, SEEK_SET );
   if ( n < 0 ) { fclose( f ); return 1; }
   b = malloc( (size_t) n + 1 );
   if ( !b ) { fclose( f ); return 1; }
   n = (long) fread( b, 1, (size_t) n, f );
   b[n] = 0;
   fclose( f );
   rc = ovec_parse( b, v );
   free( b );
   return rc;
}

const OvecPar *ovec_param( const VecOut *v, const char *nombre )
{
   int i;

   for ( i = 0; i < v->npar_t; i++ )
      if ( !strcmp( v->par[i].nombre, nombre ) ) return &v->par[i];
   return NULL;
}

int ovec_rango_propuesto( const VecOut *v, int *al_menos )
{
   const OvecPrueba *t = v->nboot ? v->boot : v->asint;
   int               n = v->nboot ? v->nboot : v->nasint, h, i;

   if ( al_menos ) *al_menos = 0;
   if ( v->m < 2 ) return -1;
   for ( h = 0; h < v->m - 1; h++ )
      {
      const OvecPrueba *c = NULL;
      for ( i = 0; i < n; i++ ) if ( t[i].r == h ) c = &t[i];
      if ( !c || !c->hay_cv ) return -1;
      if ( c->lr <= c->cv5 ) return h;
      }
   if ( al_menos ) *al_menos = 1;
   return v->m - 1;
}

int ovec_boot_hechas( const char *path_boot )
{
   FILE *f = fopen( path_boot, "r" );
   char  l[256];
   int   n = 0;

   if ( !f ) return -1;
   while ( fgets( l, sizeof l, f ) )
      if ( isdigit( (unsigned char) l[0] ) ) n++;
   fclose( f );
   return n;
}
