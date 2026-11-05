/*
 * outvarma.c -- ver outvarma.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "outvarma.h"

/* ------------------------------------------------------------------------ */
/* Recorrer el texto por lineas                                              */
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

/* Sin blancos delante. */
static const char *sb( const char *l )
{
   while ( *l == ' ' || *l == '\t' ) l++;
   return l;
}

static int vacia( const char *l )
{
   return *sb( l ) == '\0';
}

static int tras( const char *l, const char *etiqueta, double *v )
{
   const char *p = strstr( l, etiqueta );
   if ( !p ) return 0;
   return sscanf( p + strlen( etiqueta ), "%lf", v ) == 1;
}

static void copia( char *d, size_t n, const char *s )
{
   size_t k = strlen( s );
   if ( !n ) return;
   if ( k >= n ) k = n - 1;          /* se corta: los nombres del motor caben */
   memcpy( d, s, k );
   d[k] = 0;
}

/* El valor de una linea de la cabecera «Etiqueta        : valor». */
static const char *valor( const char *l, const char *etiqueta )
{
   const char *p;
   if ( !empieza( l, etiqueta ) ) return NULL;
   p = strchr( l, ':' );
   return p ? sb( p + 1 ) : NULL;
}

/* ------------------------------------------------------------------------ */
/* Las secciones                                                             */
/* ------------------------------------------------------------------------ */

/* La tabla de modelos univariantes: «  1   MUSKRAT   62  1/1850  1/1911  0
 * D  0 (6)  (1)  fixed», y luego «  [1] MUSKRAT_m03.pre».                 */
static const char *lee_unis( const char *p, VarmaOut *v )
{
   char l[1024];
   const char *q;

   while ( ( q = linea( p, l, sizeof l ) ) ) {
      int k, nobs, det;
      char nom[OV_NOMBRE], d1[16], d2[16], op[32], ar[64], ma[64], me[16];
      double lam;

      if ( sscanf( l, " [%d]", &k ) == 1 ) {
         const char *r = strchr( l, ']' );
         if ( k >= 1 && k <= v->m && r ) copia( v->uni[k - 1].pre, OV_RUTA, sb( r + 1 ) );
         p = q;
         continue;
      }
      if ( sscanf( l, " %d %63s %d %15s %15s %lf %31s %d %63s %63s %15s",
                   &k, nom, &nobs, d1, d2, &lam, op, &det, ar, ma, me ) == 11 ) {
         if ( k == v->m + 1 && v->m < OV_MAX_SER ) {
            OvUni *u = &v->uni[v->m++];
            copia( u->nombre, sizeof u->nombre, nom );
            u->nobs = nobs;
            copia( u->desde, sizeof u->desde, d1 );
            copia( u->hasta, sizeof u->hasta, d2 );
            u->lambda = lam;
            copia( u->operador, sizeof u->operador, op );
            copia( u->ar, sizeof u->ar, ar );
            copia( u->ma, sizeof u->ma, ma );
            copia( u->media, sizeof u->media, me );
         }
         p = q;
         continue;
      }
      break;
   }
   return p;
}

static const char *lee_puerta( const char *p, VarmaOut *v )
{
   char l[1024];
   const char *q;
   OvPuerta *g = &v->puerta;

   g->hay = 1;
   while ( ( q = linea( p, l, sizeof l ) ) ) {
      const char *s = sb( l );
      char nom[OV_NOMBRE];
      double a, b, c;
      p = q;
      if ( empieza( s, "GATE:" ) ) { g->pasa = strstr( s, "PASSED" ) != NULL; break; }
      if ( empieza( s, "SUM " ) )            { sscanf( s + 3, "%lf", &g->suma ); continue; }
      if ( empieza( s, "joint diagonal" ) )  { sscanf( s + 14, "%lf", &g->conjunta ); continue; }
      if ( empieza( s, "difference" ) )      { sscanf( s + 10, "%lf", &g->diferencia ); continue; }
      if ( empieza( s, "series " ) ) continue;
      if ( sscanf( s, "%63s %lf %lf %lf", nom, &a, &b, &c ) == 4 && g->ns < OV_MAX_SER ) {
         OvPuertaSerie *x = &g->s[g->ns++];
         copia( x->nombre, sizeof x->nombre, nom );
         x->logl = a; x->sigma2 = b; x->mov = c;
         x->recortada = strstr( s, "trimmed sample" ) != NULL;
         x->no_optimo = strstr( s, "not an optimum" ) != NULL;
      }
   }
   return p;
}

/* «  OPTIMIZER CONVERGED after 42 iterations» y sus dos o tres lineas. */
static const char *lee_optim( const char *l0, const char *p, VarmaOut *v )
{
   char l[1024];
   const char *q, *s = sb( l0 );

   if ( strstr( s, "MA invertibility boundary" ) ) v->optim = OV_OPT_PARED_MA;
   else if ( strstr( s, "CONVERGED" ) )           v->optim = OV_OPT_CONVERGIO;
   else                                            v->optim = OV_OPT_PARADO;
   {
      const char *a = strstr( s, "after " );
      if ( a ) sscanf( a + 6, "%d", &v->iteraciones );
   }
   while ( ( q = linea( p, l, sizeof l ) ) ) {
      const char *t = sb( l );
      if ( empieza( t, "=====" ) ) { p = q; break; }
      if ( empieza( t, "Convergence criterion:" ) )
         copia( v->criterio, sizeof v->criterio, sb( t + 22 ) );
      else if ( empieza( t, "MA boundary:" ) )
         sscanf( t + 12, "%d of %d", &v->pared_n, &v->pared_raices );
      p = q;
   }
   return p;
}

/* La tabla de parametros, hasta «Standard errors: ...». */
static const char *lee_params( const char *p, VarmaOut *v )
{
   char l[1024];
   const char *q;

   while ( ( q = linea( p, l, sizeof l ) ) ) {
      const char *s = sb( l );
      char nom[OV_NOMBRE];
      double e, se, t, pv;
      int n;
      p = q;
      if ( empieza( s, "Standard errors:" ) ) {
         copia( v->se_como, sizeof v->se_como, sb( s + 16 ) );
         break;
      }
      if ( empieza( s, "Parameter" ) || empieza( s, "Number of parameters" ) ) continue;
      if ( vacia( s ) ) break;
      n = sscanf( s, "%63s %lf %lf %lf %lf", nom, &e, &se, &t, &pv );
      if ( n >= 2 && v->npar < OV_MAX_PAR ) {
         OvParam *x = &v->par[v->npar++];
         copia( x->nombre, sizeof x->nombre, nom );
         x->est = e;
         x->tiene_se = n == 5;
         if ( n == 5 ) { x->se = se; x->t = t; x->p = pv; }
      }
   }
   return p;
}

/* Una matriz m x m de filas «  NOMBRE  a  b ...». */
static const char *lee_matriz( const char *p, int m, double out[OV_MAX_SER][OV_MAX_SER] )
{
   char l[1024];
   const char *q;
   int i;

   for ( i = 0; i < m && ( q = linea( p, l, sizeof l ) ); i++ ) {
      const char *s = sb( l );
      int j;
      p = q;
      while ( *s && *s != ' ' ) s++;          /* el nombre */
      for ( j = 0; j < m; j++ ) {
         char *fin;
         out[i][j] = strtod( s, &fin );
         if ( fin == s ) break;
         s = fin;
      }
   }
   return p;
}

/* Las raices inversas: «     -0.461419      0.354628 i  (modulus     0.581953)». */
static const char *lee_raices( const char *p, double *max, int *n )
{
   char l[1024];
   const char *q;

   *max = 0; *n = 0;
   while ( ( q = linea( p, l, sizeof l ) ) ) {
      double mod;
      if ( !tras( l, "(modulus", &mod ) ) break;
      if ( mod > *max ) *max = mod;
      ( *n )++;
      p = q;
   }
   return p;
}

/* Una tabla «Horizon  A  B» y sus filas: t[h][j]. Devuelve las filas. */
static const char *lee_tabla_h( const char *p, int m, double t[OV_MAX_HOR][OV_MAX_SER],
                                int *nh, int h0 )
{
   char l[1024];
   const char *q;

   *nh = 0;
   while ( ( q = linea( p, l, sizeof l ) ) ) {
      const char *s = sb( l );
      char *fin;
      long h;
      int j;
      if ( empieza( s, "Horizon" ) ) { p = q; continue; }
      h = strtol( s, &fin, 10 );
      if ( fin == s ) break;
      if ( h - h0 >= 0 && h - h0 < OV_MAX_HOR ) {
         s = fin;
         for ( j = 0; j < m; j++ ) {
            t[h - h0][j] = strtod( s, &fin );
            if ( fin == s ) break;
            s = fin;
         }
         if ( h - h0 + 1 > *nh ) *nh = (int)( h - h0 + 1 );
      }
      p = q;
   }
   return p;
}

/* La IRF ortogonalizada: «Shock to A:» y su tabla, por cada choque. */
static const char *lee_irf( const char *p, VarmaOut *v )
{
   char l[1024];
   const char *q;
   int k = 0;

   while ( ( q = linea( p, l, sizeof l ) ) ) {
      const char *s = sb( l );
      if ( empieza( s, "=====" ) && k > 0 ) break;
      if ( empieza( s, "Shock to " ) && k < v->m ) {
         static double t[OV_MAX_HOR][OV_MAX_SER];
         int nh, h, j;
         p = lee_tabla_h( q, v->m, t, &nh, 0 );
         for ( h = 0; h < nh; h++ )
            for ( j = 0; j < v->m; j++ ) v->irf[k][h][j] = t[h][j];
         if ( nh > v->nh_irf ) v->nh_irf = nh;
         k++;
         continue;
      }
      p = q;
   }
   return p;
}

/* La FEVD: «A:» y su tabla (horizontes desde 1), por cada serie. */
static const char *lee_fevd( const char *p, VarmaOut *v )
{
   char l[1024];
   const char *q;
   int k = 0;

   while ( ( q = linea( p, l, sizeof l ) ) ) {
      const char *s = sb( l );
      size_t n = strlen( s );
      if ( empieza( s, "=====" ) && k > 0 ) break;
      if ( k < v->m && n > 1 && s[n - 1] == ':' &&
           strncmp( s, v->uni[k].nombre, n - 1 ) == 0 && strlen( v->uni[k].nombre ) == n - 1 ) {
         static double t[OV_MAX_HOR][OV_MAX_SER];
         int nh, h, j;
         p = lee_tabla_h( q, v->m, t, &nh, 1 );
         for ( h = 0; h < nh; h++ )
            for ( j = 0; j < v->m; j++ ) v->fevd[k][h][j] = t[h][j];
         if ( nh > v->nh_fevd ) v->nh_fevd = nh;
         k++;
         continue;
      }
      p = q;
   }
   return p;
}

/* Una tabla de prevision: «date Level Low95 High95 mon% std ann% std» y sus
 * filas, hasta la primera linea vacia.                                    */
static const char *lee_prev_filas( const char *p, OvPrev *x )
{
   char l[1024];
   const char *q;

   while ( ( q = linea( p, l, sizeof l ) ) ) {
      const char *s = sb( l );
      char f[16];
      double a[7];
      if ( empieza( s, "date" ) ) { p = q; continue; }
      if ( sscanf( s, "%15s %lf %lf %lf %lf %lf %lf %lf", f,
                   &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6] ) != 8 ) break;
      if ( x->nf < OV_MAX_FILA ) {
         int i = x->nf++;
         copia( x->fecha[i], sizeof x->fecha[i], f );
         x->nivel[i] = a[0]; x->lo95[i] = a[1]; x->hi95[i] = a[2];
         x->var_per[i] = a[3]; x->sd_per[i] = a[4];
         x->var_anu[i] = a[5]; x->sd_anu[i] = a[6];
      }
      p = q;
   }
   return p;
}

/* La evaluacion recursiva: «  A» y «    h  n  MAE  RMSE  MAPE(%)», por serie. */
static const char *lee_eval( const char *p, VarmaOut *v )
{
   char l[1024];
   const char *q;
   OvEvalSerie *e = NULL;

   while ( ( q = linea( p, l, sizeof l ) ) ) {
      const char *s = sb( l );
      int h, n;
      double a, b, c;
      if ( empieza( s, "=====" ) ) break;
      if ( strstr( s, " origins." ) && sscanf( s, "%d origins", &h ) == 1 ) v->ev_origenes = h;
      if ( sscanf( s, "%d %d %lf %lf %lf", &h, &n, &a, &b, &c ) == 5 ) {
         if ( e && e->nh < OV_MAX_HOR ) {
            OvHoriz *x = &e->h[e->nh++];
            x->h = h; x->n = n; x->mae = a; x->rmse = b; x->mape = c;
         }
      } else if ( !vacia( s ) && ov_serie( v, s ) >= 0 && v->nev < OV_MAX_SER ) {
         e = &v->ev[v->nev++];
         copia( e->nombre, sizeof e->nombre, s );
      }
      p = q;
   }
   return p;
}

/* «CROSS CORRELATION a[j] (B) - a[i] (A)», la banda, las filas «  k  r |…»
 * y los dos Q.                                                           */
static const char *lee_ccf( const char *l0, const char *p, VarmaOut *v )
{
   char l[1024];
   const char *q;
   OvCcf *c;
   int j, i, filas = 0;
   char b[OV_NOMBRE] = "", a[OV_NOMBRE] = "";

   if ( v->nccf >= OV_MAX_PARES ) return p;
   if ( sscanf( sb( l0 ), "CROSS CORRELATION a[%d] (%63[^)]) - a[%d] (%63[^)])", &j, b, &i, a ) != 4 &&
        sscanf( sb( l0 ), "CROSS CORRELATION a[%d] - a[%d]", &j, &i ) != 2 ) return p;
   c = &v->ccf[v->nccf];
   memset( c, 0, sizeof *c );
   c->i = i - 1; c->j = j - 1;
   copia( c->a, sizeof c->a, a );
   copia( c->b, sizeof c->b, b );

   while ( ( q = linea( p, l, sizeof l ) ) ) {
      const char *s = sb( l );
      int k;
      double r;
      p = q;
      if ( empieza( s, "CCF BANDS" ) ) { const char *e = strchr( s, '=' ); if ( e ) c->banda = atof( e + 1 ); continue; }
      if ( strchr( s, '|' ) && sscanf( s, "%d %lf |", &k, &r ) == 2 ) {
         if ( filas < 2 * OV_MAX_LAG + 1 ) c->r[filas++] = r;
         continue;
      }
      if ( empieza( s, "Q(" ) ) {
         double q1, q2;
         int d1, d2;
         if ( sscanf( s, "Q(%d) = %lf k >= 0 Q(%d) = %lf", &d1, &q1, &d2, &q2 ) == 4 ) {
            c->q_ge0 = q1; c->q_gt0 = q2; c->df_ge0 = d1; c->df_gt0 = d2;
            continue;
         }
         if ( sscanf( s, "Q(%d) = %lf k <= 0 Q(%d) = %lf", &d1, &q1, &d2, &q2 ) == 4 ) {
            c->q_le0 = q1; c->q_lt0 = q2;
            break;
         }
      }
   }
   if ( filas >= 3 && filas % 2 == 1 ) {
      c->nlags = ( filas - 1 ) / 2;
      v->nccf++;
   }
   return p;
}

const OvCcf *ov_ccf( const VarmaOut *v, int i, int j )
{
   int k;
   for ( k = 0; k < v->nccf; k++ )
      if ( ( v->ccf[k].i == i && v->ccf[k].j == j ) || ( v->ccf[k].i == j && v->ccf[k].j == i ) )
         return &v->ccf[k];
   return NULL;
}

double ov_ccf_r( const VarmaOut *v, int a, int b, int k, double *banda )
{
   const OvCcf *c = ov_ccf( v, a, b );

   if ( banda ) *banda = c ? c->banda : 0;
   if ( !c || k < 0 || k > c->nlags ) return 0;
   /* En la CCF del par (i < j), k > 0 es «j --> i»: la serie j entra en la
    * ecuacion de la serie i. Para a = i, b = j es +k; al reves, -k.      */
   return c->r[c->nlags + ( a == c->i ? k : -k )];
}

/* ------------------------------------------------------------------------ */

int ov_parse( const char *texto, VarmaOut *v )
{
   char l[1024];
   const char *p = texto, *q;
   int escalera = 0;

   memset( v, 0, sizeof *v );
   if ( !texto ) return 1;

   while ( ( q = linea( p, l, sizeof l ) ) ) {
      const char *s = sb( l ), *x;
      p = q;

      if ( strstr( l, "(ladder mode: .pre input)" ) ) { escalera = 1; continue; }
      if ( ( x = valor( l, "Model            " ) ) ) {
         const char *c = strstr( x, "p=" );
         if ( c ) sscanf( c, "p=%d q=%d", &v->p, &v->q );
         continue;
      }
      if ( ( x = valor( l, "Cross links      " ) ) ) { copia( v->enlaces, sizeof v->enlaces, x ); continue; }
      if ( ( x = valor( l, "Innovation cov.  " ) ) ) { v->sigma_diagonal = strstr( x, "diagonal" ) != NULL; continue; }
      if ( ( x = valor( l, "Deterministics   " ) ) ) { v->det_reestimados = strstr( x, "re-estimated" ) != NULL; continue; }
      if ( ( x = valor( l, "Univariate ARMA  " ) ) ) { v->arma_fijos = strstr( x, "fixed" ) != NULL; continue; }
      if ( ( x = valor( l, "Forecast horizon " ) ) ) { v->horizonte = atoi( x ); continue; }
      if ( ( x = valor( l, "Common window    " ) ) ) {
         sscanf( x, "%15s - %15s (%d", v->desde, v->hasta, &v->nobs );
         continue;
      }
      if ( empieza( l, "Univariate models (one .pre per series):" ) ) {
         const char *r = linea( p, l, sizeof l );       /* la cabecera de la tabla */
         if ( r ) p = lee_unis( r, v );
         continue;
      }
      if ( empieza( s, "THE DIAGONAL GATE" ) ) { p = lee_puerta( p, v ); continue; }
      if ( empieza( s, "OPTIMIZER " ) )        { p = lee_optim( l, p, v ); continue; }
      if ( empieza( s, "**** ESTIMATION FAILED:" ) || empieza( s, "**** univariate fit of" ) ) {
         v->fallo = 1;
         copia( v->fallo_msg, sizeof v->fallo_msg, s + 5 );
         continue;
      }
      if ( empieza( s, "ESTIMATED MODEL" ) ) {
         v->solo_diagonal = strstr( s, "diagonal system" ) != NULL;
         continue;
      }
      if ( empieza( s, "Number of parameters:" ) ) { p = lee_params( p, v ); continue; }
      if ( tras( s, "Exact log-likelihood:", &v->logl ) ) { v->tiene_logl = 1; continue; }
      if ( empieza( s, "logL diagonal" ) ) {
         sscanf( s, "logL diagonal %lf, logL full %lf", &v->lr_logl_diag, &v->lr_logl_full );
         continue;
      }
      if ( empieza( s, "LR = " ) ) {
         v->tiene_lr = sscanf( s, "LR = %lf df = %d p-value = %lf",
                               &v->lr, &v->lr_df, &v->lr_p ) == 3;
         continue;
      }
      if ( empieza( s, "Innovation covariance Sigma" ) ) {
         p = lee_matriz( p, v->m, v->sigma );
         v->tiene_sigma = 1;
         continue;
      }
      if ( empieza( s, "Correlations:" ) ) { p = lee_matriz( p, v->m, v->correl ); continue; }
      if ( empieza( s, "Inverse roots of |phi(B)|=0:" ) )   { p = lee_raices( p, &v->raiz_ar_max, &v->n_raices_ar ); continue; }
      if ( empieza( s, "Inverse roots of |theta(B)|=0:" ) ) { p = lee_raices( p, &v->raiz_ma_max, &v->n_raices_ma ); continue; }
      if ( empieza( s, "ORTHOGONALIZED IMPULSE RESPONSE" ) ) { p = lee_irf( p, v ); continue; }
      if ( empieza( s, "FORECAST ERROR VARIANCE DECOMPOSITION" ) ) { p = lee_fevd( p, v ); continue; }
      if ( empieza( s, "Forecast of " ) && v->nprev < OV_MAX_SER ) {
         OvPrev *x = &v->prev[v->nprev];
         char nom[OV_NOMBRE], org[16];
         if ( sscanf( s, "Forecast of %63s from %15[^ (:]", nom, org ) == 2 ) {
            memset( x, 0, sizeof *x );
            copia( x->nombre, sizeof x->nombre, nom );
            copia( x->origen, sizeof x->origen, org );
            p = lee_prev_filas( p, x );
            v->nprev++;
         }
         continue;
      }
      if ( empieza( s, "CROSS CORRELATION a[" ) ) { p = lee_ccf( l, p, v ); continue; }
      if ( empieza( s, "RECURSIVE FORECAST EVALUATION" ) ) {
         const char *r = linea( p, l, sizeof l );        /* la raya de debajo */
         if ( r ) p = lee_eval( r, v );
         continue;
      }
   }

   if ( !escalera ) return 1;
   v->tiene_diag = od_parse( texto, &v->diag ) == 0;
   return 0;
}

static char *lee_todo( const char *path )
{
   FILE *f = fopen( path, "rb" );
   char *b;
   long n;

   if ( !f ) return NULL;
   fseek( f, 0, SEEK_END );
   n = ftell( f );
   fseek( f, 0, SEEK_SET );
   if ( n < 0 ) { fclose( f ); return NULL; }
   b = malloc( (size_t) n + 1 );
   if ( !b ) { fclose( f ); return NULL; }
   n = (long) fread( b, 1, (size_t) n, f );
   b[n] = 0;
   fclose( f );
   return b;
}

int ov_parse_file( const char *path, VarmaOut *v )
{
   char *t = lee_todo( path );
   int r;

   if ( !t ) { memset( v, 0, sizeof *v ); return -1; }
   r = ov_parse( t, v );
   free( t );
   return r;
}

const OvParam *ov_param( const VarmaOut *v, const char *nombre )
{
   int i;
   for ( i = 0; i < v->npar; i++ )
      if ( strcmp( v->par[i].nombre, nombre ) == 0 ) return &v->par[i];
   return NULL;
}

int ov_serie( const VarmaOut *v, const char *nombre )
{
   int i;
   for ( i = 0; i < v->m; i++ )
      if ( strcmp( v->uni[i].nombre, nombre ) == 0 ) return i;
   return -1;
}

/* «Series 1 (MUSKRAT):» y su tabla. */
int ov_forecast_file( const char *path, OvPrev *prev, int max )
{
   char *t = lee_todo( path );
   char l[1024];
   const char *p, *q;
   int n = 0;

   if ( !t ) return -1;
   p = t;
   while ( ( q = linea( p, l, sizeof l ) ) ) {
      int k;
      char nom[OV_NOMBRE];
      p = q;
      if ( n < max && sscanf( l, "Series %d (%63[^)])", &k, nom ) == 2 ) {
         memset( &prev[n], 0, sizeof prev[n] );
         copia( prev[n].nombre, sizeof prev[n].nombre, nom );
         p = lee_prev_filas( p, &prev[n] );
         n++;
      }
   }
   free( t );
   return n;
}

int ov_recursive_file( const char *path, OvRecursive *r )
{
   char *t = lee_todo( path );
   char l[1024];
   const char *p, *q;
   int cap = 0;

   memset( r, 0, sizeof *r );
   if ( !t ) return -1;
   p = t;
   while ( ( q = linea( p, l, sizeof l ) ) ) {
      char org[16], ser[OV_NOMBRE];
      int h;
      double nv;
      p = q;
      if ( empieza( l, "estwin=" ) ) {
         const char *c = strstr( l, "horizon=" );
         r->estwin = atoi( l + 7 );
         if ( c ) r->horizonte = atoi( c + 8 );
         continue;
      }
      if ( sscanf( l, "%15s %63s %d %lf", org, ser, &h, &nv ) != 4 ) continue;
      if ( r->nf == cap ) {
         int nc = cap ? 2 * cap : 256;
         OvRecFila *f = realloc( r->f, (size_t) nc * sizeof *f );
         if ( !f ) break;
         r->f = f; cap = nc;
      }
      copia( r->f[r->nf].origen, sizeof r->f[r->nf].origen, org );
      copia( r->f[r->nf].serie, sizeof r->f[r->nf].serie, ser );
      r->f[r->nf].h = h;
      r->f[r->nf].nivel = nv;
      r->nf++;
   }
   free( t );
   return r->nf ? 0 : 1;
}

void ov_recursive_free( OvRecursive *r )
{
   free( r->f );
   r->f = NULL;
   r->nf = 0;
}

int ov_residuos_file( const char *path, OvResiduos *r )
{
   char *t = lee_todo( path );
   char l[1024];
   const char *p, *q;
   int serie = -1, en_grafico = 0, rayas = 0, k = 0;

   memset( r, 0, sizeof *r );
   if ( !t ) return -1;
   p = t;
   while ( ( q = linea( p, l, sizeof l ) ) ) {
      const char *s = sb( l );
      int a, obs;
      char nom[OV_NOMBRE], f[16];
      p = q;
      if ( sscanf( s, "--- Residual series a[%d] (%63[^)])", &a, nom ) == 2 ) {
         if ( r->m >= OV_MAX_SER ) break;
         serie = r->m++;
         copia( r->nombre[serie], sizeof r->nombre[serie], nom );
         en_grafico = 0;
         continue;
      }
      if ( serie < 0 ) continue;
      if ( empieza( s, "Standardized time series plot" ) ) { en_grafico = 1; rayas = 0; k = 0; continue; }
      if ( !en_grafico ) continue;
      if ( empieza( s, "-----" ) ) {
         if ( ++rayas == 2 ) {           /* la raya de debajo: se acabo */
            en_grafico = 0;
            if ( serie == 0 || k < r->n ) r->n = k;
         }
         continue;
      }
      if ( rayas == 1 && sscanf( s, "%d %15s", &obs, f ) == 2 ) {
         const char *u = strrchr( s, '|' );
         if ( !u || k >= OV_MAX_OBS ) continue;
         u++;
         while ( *u == '>' || *u == ' ' ) u++;
         r->v[serie][k] = atof( u );
         if ( serie == 0 ) copia( r->fecha[k], sizeof r->fecha[k], f );
         k++;
      }
   }
   free( t );
   return r->m > 0 && r->n > 0 ? 0 : 1;
}
