/*****************************************************************************/
/*  nlatools.c -- part of drtran (Box-Jenkins transfer function models).
 *
 *  Exact VARMA likelihood engine, from drvarma/ART.
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/


/*****************************************************************************/
/*  NLATOOLS.C                                                               */
/*  Numerical linear algebra and dynamic memory allocation routines.         */
/*  Copyright (C) Jos‚ Alberto Mauricio, 1995 (except where indicated).      */
/*****************************************************************************/

#include "main.h"            /* Header file (prototype declarations)        */
extern real macheps;          /* Machine epsilon (global: declared in DRV.C) */
extern FILE *outputv;         /* Output file (global: declared in DRV.C)     */

/*****************************************************************************/

static  int iminarg1, iminarg2;
#define IMIN(a,b) (iminarg1=(a),iminarg2=(b),(iminarg1) < (iminarg2) ?\
        (iminarg1) : (iminarg2))
#define RADIX 2.0
#define SWAP(g,h) {y=(g);(g)=(h);(h)=y;}
#define SIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))

/*****************************************************************************/
/*****************************************************************************/

void ludcp( real **a, int n, int *ip )

{
   int  i, j, k, kp1, m;
   real tmp;

   ip[n] = 1;
   for ( k = 1; k <= n; k++ )
       {
       if ( k != n )
          {
          kp1 = k + 1;
          m = k;
          for ( i = kp1; i <= n; i++ )
              if ( fabs( a[i][k] ) > fabs( a[m][k] ) ) m = i;
          ip[k] = m;
          if ( m != k ) ip[n] = -ip[n];
          tmp = a[m][k];
          a[m][k] = a[k][k];
          a[k][k] = tmp;
          if ( tmp != 0.0 )
             {
             for ( i = kp1; i <= n; i++ ) a[i][k] /= -tmp;
             for ( j = kp1; j <= n; j++ )
                 {
                 tmp = a[m][j];
                 a[m][j] = a[k][j];
                 a[k][j] = tmp;
                 if ( tmp != 0.0 )
                    for ( i = kp1; i <= n; i++ ) a[i][j] += a[i][k] * tmp;
                 }
             }
          }
       if ( a[k][k] == 0.0 ) ip[n] = 0;
       }
}

/****************************************************************************/

void lusol( real **a, real *b, int n, int *ip )

{
   int  i, k, km1, kp1, k1, m, nm1;
   real tmp;

   if ( n != 1 )
      {
      nm1 = n - 1;
      for ( k = 1; k <= nm1; k++ )
          {
          kp1 = k + 1;
          m = ip[k];
          tmp = b[m];
          b[m] = b[k];
          b[k] = tmp;
          for ( i = kp1; i <= n; i++ ) b[i] += a[i][k] * tmp;
          }
      for ( k1 = 1; k1 <= nm1; k1++ )
          {
          km1 = n - k1;
          k = km1 + 1;
          b[k] /= a[k][k];
          tmp = -b[k];
          for ( i = 1; i <= km1; i++ ) b[i] += a[i][k] * tmp;
          }
      }
   b[1] /= a[1][1];
}

/****************************************************************************/
/****************************************************************************/

void choldcp( real **mat, int n, real *d1, real *d2, int *ifault )

{
   int   i, j, k;
   real  sum1, minl, maxoffl, minl2, maxadd, minljj, sqrteps;

   *ifault = 0;
   *d1     = 1.0;
   *d2     = 0.0;
   sqrteps = sqrt( macheps );

/* [1]: check wether mat is numerically different from zero (redundancy):   */

   for ( maxoffl = 0.0, j = 1; j <= n; j++ )
       if ( sqrt( fabs( mat[j][j] ) ) > maxoffl )
          maxoffl = sqrt( fabs( mat[j][j] ) );
   if ( maxoffl * maxoffl <= sqrteps )
      {
      for ( i = 1; i <= n; i++ ) for ( j = 1; j <= n; j++ ) mat[i][j] = 0.0;
      return;
      }

/* [2]: initialize finite-arithmetic constants:                             */

   minl   = 0.0;
   minl2  = sqrteps * maxoffl;
   maxadd = 0.0;

/* [3]: form the j-th column of the Cholesky factor of mat:                 */

   for ( j = 1; j <= n; j++ )
       {
       sum1 = mat[j][j];
       for ( i = 1; i <= j - 1; i++ ) sum1 -= mat[j][i] * mat[j][i];

       if ( (sum1 != fabs( sum1 )) && (fabs( sum1 ) > minl2) )
          {
          *ifault = 1;
          return;
          }
       else
          mat[j][j] = sum1;

       minljj = 0.0;
       for ( i = j + 1; i <= n; i++ )
           {
           sum1 = mat[j][i];
           for ( k = 1; k <= j - 1; k++ ) sum1 -= mat[i][k] * mat[j][k];
           mat[i][j] = sum1;
           if ( fabs( mat[i][j] ) > minljj ) minljj = fabs( mat[i][j] );
           }

       if ( (minljj / maxoffl) > minl )
          minljj /= maxoffl;
       else
          minljj = minl;

       if ( mat[j][j] > (minljj * minljj) )
          mat[j][j] = sqrt( mat[j][j] );
       else
          {
          if ( minljj < minl2 )
             minljj = minl2;
          if ( maxadd < (minljj * minljj - mat[j][j]) )
             maxadd = minljj * minljj - mat[j][j];
          mat[j][j] = minljj;
          }

       *d1 *= mat[j][j] * mat[j][j];
       while ( fabs( *d1 ) >= 1.0 )
             {
             *d1 *= 0.0625;
             *d2 += 4.0;
             }
       while ( fabs( *d1 ) < 0.0625 )
             {
             *d1 *= 16.0;
             *d2 -=  4.0;
             }

       for ( i = j + 1; i <= n; i++ ) mat[i][j] /= mat[j][j];
       }

   for ( j = 2; j <= n; j++ )
       for ( i = 1; i <= j - 1; i++ ) mat[i][j] = 0.0;

}

/****************************************************************************/

void cholfor( real **matl, int n, real *rhsol )

{
   int   i, j;
   real  tmp;

   rhsol[1] /= matl[1][1];

   for ( i = 2; i <= n; i++ )
       {
       tmp = 0.0;
       for ( j = 1; j <= i - 1; j++ )
           tmp += matl[i][j] * rhsol[j];
       rhsol[i] = (rhsol[i] - tmp) / matl[i][i];
       }
}

/****************************************************************************/

void cholbak( real **matl, int n, real *rhsol )

{
   int   i, j;
   real  tmp;

   rhsol[n] /= matl[n][n];

   for ( i = n - 1; i >= 1; i-- )
       {
       tmp = 0.0;
       for ( j = i + 1; j <= n; j++ )
           tmp += matl[j][i] * rhsol[j];
       rhsol[i] = (rhsol[i] - tmp) / matl[i][i];
       }
}

/****************************************************************************/

void cholsol( real **matl, int n, real *rhsol )

{
   cholfor( matl, n, rhsol );
   cholbak( matl, n, rhsol );
}

/****************************************************************************/
/****************************************************************************/

void eigenql( real **z, int n, real *d )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   real s, r, p, g, f, dd, c, b, *e, pythag( real a, real b );
   int  m, l, iter, i, k;
   void tred2( real **a, int n, real *d, real *e );

   e = vector( 1, n );
   tred2( z, n, d, e );

   for ( i = 2; i <= n; i++ ) e[i-1] = e[i];
   e[n] = 0.0;
   for ( l = 1; l <= n; l++ )
       {
       iter = 0;
       do {
          for ( m = l; m <= n-1; m++ )
              {
              dd = fabs( d[m] ) + fabs( d[m+1] );
              if ( (real)(fabs( e[m] ) + dd) == dd ) break;
              }
          if ( m != l )
             {
             if ( iter++ == 30 )
                nrerror( "TOO MANY ITERATIONS IN eigenql()" );
             g = (d[l+1] - d[l]) / (2.0 * e[l]);
             r = pythag( g, 1.0 );
             g = d[m] - d[l] + e[l] / (g + SIGN(r,g));
             s = c = 1.0;
             p = 0.0;
             for ( i = m-1; i >= l; i-- )
                 {
                 f = s * e[i];
                 b = c * e[i];
                 e[i+1] = (r = pythag( f, g ));
                 if ( r == 0.0 )
                    {
                    d[i+1] -= p;
                    e[m] = 0.0;
                    break;
                    }
                 s = f / r;
                 c = g / r;
                 g = d[i+1] - p;
                 r = (d[i] - g) * s + 2.0 * c * b;
                 d[i+1] = g + (p = s * r);
                 g = c * r - b;
                 for ( k = 1; k <= n; k++ )
                     {
                     f = z[k][i+1];
                     z[k][i+1] = s * z[k][i] + c * f;
                     z[k][i] = c * z[k][i] - s * f;
                     }
                 }
             if ( r == 0.0 && i >= l ) continue;
             d[l] -= p;
             e[l] = g;
             e[m] = 0.0;
             }
       } while ( m != l );
       }
   free_vector( e, 1, n );
}

/****************************************************************************/

void tred2( real **a, int n, real *d, real *e )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   int l, k, j, i;
   real scale, hh, h, g, f;

   for ( i = n; i >= 2; i-- )
       {
       l = i - 1;
       h = scale = 0.0;
       if ( l > 1 )
          {
          for ( k = 1; k <= l; k++ ) scale += fabs( a[i][k] );
          if ( scale == 0.0 )
             e[i] = a[i][l];
          else
             {
             for (k = 1; k <= l; k++ )
                 {
                 a[i][k] /= scale;
                 h += a[i][k] * a[i][k];
                 }
             f = a[i][l];
             g = (f >= 0.0 ? -sqrt( h ) : sqrt( h ));
             e[i] = scale * g;
             h -= f * g;
             a[i][l] = f - g;
             f = 0.0;
             for ( j = 1; j <= l; j++ )
                 {
                 a[j][i] = a[i][j] / h;
                 g = 0.0;
                 for ( k = 1; k <= j; k++ ) g += a[j][k] * a[i][k];
                 for ( k = j+1; k <= l; k++ ) g += a[k][j]*a[i][k];
                 e[j] = g / h;
                 f += e[j] * a[i][j];
                 }
             hh = f / (h + h);
             for ( j = 1; j <= l; j++ )
                 {
                 f = a[i][j];
                 e[j] = g = e[j] - hh * f;
                 for ( k = 1; k <= j; k++ )
                     a[j][k] -= (f * e[k] + g * a[i][k]);
                 }
             }
          }
       else
          e[i] = a[i][l];
       d[i] = h;
       }

   d[1] = 0.0;
   e[1] = 0.0;

   for ( i = 1; i <= n; i++ )
       {
       l = i - 1;
       if ( d[i] )
          for ( j = 1; j <= l; j++ )
              {
              g = 0.0;
              for ( k = 1; k <= l; k++ ) g += a[i][k] * a[k][j];
              for ( k = 1; k <= l; k++ ) a[k][j] -= g * a[k][i];
              }
       d[i] = a[i][i];
       a[i][i] = 1.0;
       for (j = 1; j <= l; j++ ) a[j][i] = a[i][j] = 0.0;
       }
}

/****************************************************************************/
/****************************************************************************/

void eigenqr( real **a, int n, real *wr, real *wi )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   int  nn, m, l, k, j, its, i, mmin;
   real z, y, x, w, v, u, t, s, r, q, p, anorm, pythag( real a, real b );
   void balanc( real **a, int n ), elmhes( real **a, int n );

   balanc( a, n );
   elmhes( a, n );

   anorm = fabs( a[1][1] );
   for ( i = 2; i <= n; i++ )
       for ( j = (i-1); j <= n; j++ ) anorm += fabs( a[i][j] );

   nn = n;
   t  = 0.0;

   while ( nn >= 1 )
     {
     its = 0;
     do {
        for ( l = nn; l >= 2; l-- )
            {
            s = fabs( a[l-1][l-1] ) + fabs( a[l][l] );
            if ( s == 0.0 ) s = anorm;
            if ( (real)(fabs( a[l][l-1]) + s ) == s ) break;
            }
        x = a[nn][nn];
        if ( l == nn )
           {
           wr[nn]   = x + t;
           wi[nn--] = 0.0;
           }
        else
           {
           y = a[nn-1][nn-1];
           w = a[nn][nn-1] * a[nn-1][nn];
           if ( l == (nn-1) )
              {
              p = 0.5 * (y - x);
              q = p * p + w;
              z = sqrt( fabs( q ) );
              x += t;
              if ( q >= 0.0 )
                 {
                 z = p + SIGN(z,p);
                 wr[nn-1] = wr[nn] = x + z;
                 if ( z ) wr[nn] = x - w / z;
                 wi[nn-1] = wi[nn] = 0.0;
                 }
              else
                 {
                 wr[nn-1] = wr[nn] = x + p;
                 wi[nn-1]= -(wi[nn] = z);
                 }
              nn -= 2;
              }
           else
              {
              if ( its == 80 )
                 nrerror( "TOO MANY ITERATIONS IN eigenqr()" );
              if ( its == 10 || its == 20 )
                 {
                 t += x;
                 for ( i = 1; i <= nn;i++ ) a[i][i] -= x;
                 s = fabs( a[nn][nn-1] ) + fabs( a[nn-1][nn-2] );
                 y = x = 0.75 * s;
                 w = -0.4375 * s * s;
                 }
              ++its;
              for ( m = (nn-2); m >= l; m--)
                  {
                  z = a[m][m];
                  r = x - z;
                  s = y - z;
                  p = (r * s - w) / a[m+1][m] + a[m][m+1];
                  q = a[m+1][m+1] - z - r - s;
                  r = a[m+2][m+1];
                  s = fabs( p ) + fabs( q ) + fabs( r );
                  p /= s;
                  q /= s;
                  r /= s;
                  if ( m == l ) break;
                  u = fabs( a[m][m-1] ) * (fabs( q ) + fabs( r ));
                  v = fabs( p ) * (fabs( a[m-1][m-1] ) + fabs( z ) +
                      fabs( a[m+1][m+1] ));
                  if ( (real)(u+v) == v ) break;
                  }
              for ( i = m+2; i <= nn; i++ )
                  {
                  a[i][i-2] = 0.0;
                  if ( i != (m+2) ) a[i][i-3] = 0.0;
                  }
              for ( k = m; k <= nn-1; k++ )
                  {
                  if ( k != m )
                     {
                     p = a[k][k-1];
                     q = a[k+1][k-1];
                     r = 0.0;
                     if ( k != (nn-1) ) r = a[k+2][k-1];
                     if ( (x = fabs( p ) + fabs( q ) + fabs( r )) != 0.0 )
                        {
                        p /= x;
                        q /= x;
                        r /= x;
                        }
                     }
                  if ( (s = SIGN(sqrt(p*p+q*q+r*r),p)) != 0.0 )
                     {
                     if (k == m)
                        {
                        if ( l != m )
                           a[k][k-1] = -a[k][k-1];
                        }
                     else
                        a[k][k-1] = -s*x;
                     p += s;
                     x = p / s;
                     y = q / s;
                     z = r / s;
                     q /= p;
                     r /= p;
                     for ( j = k; j <= nn; j++ )
                         {
                         p = a[k][j] + q * a[k+1][j];
                         if ( k != (nn-1) )
                            {
                            p += r * a[k+2][j];
                            a[k+2][j] -= p * z;
                            }
                         a[k+1][j] -= p * y;
                         a[k][j] -= p * x;
                         }
                     mmin = nn < k+3 ? nn : k+3;
                     for ( i = l; i <= mmin; i++)
                         {
                         p = x * a[i][k] + y * a[i][k+1];
                         if (k != (nn-1))
                            {
                            p += z * a[i][k+2];
                            a[i][k+2] -= p * r;
                            }
                         a[i][k+1] -= p*q;
                         a[i][k] -= p;
                         }
                     }
                  }
              }
           }
     } while ( l < nn-1 );
     }
     for ( i = 1; i <= n; i++ ) a[i][i] = pythag( wr[i], wi[i] );
}

/****************************************************************************/

void balanc( real **a, int n )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   int  last, j, i;
   real s, r, g, f, c, sqrdx;

   sqrdx = RADIX * RADIX;
   last  = 0;
   while ( last == 0 )
     {
     last = 1;
     for ( i = 1; i <= n; i++ )
         {
         r = c = 0.0;
         for ( j = 1; j <= n; j++ )
             if ( j != i )
                {
                c += fabs(a[j][i]);
                r += fabs(a[i][j]);
                }
         if ( c && r )
            {
            g = r / RADIX;
            f = 1.0;
            s = c + r;
            while ( c < g )
              {
              f *= RADIX;
              c *= sqrdx;
              }
            g = r * RADIX;
            while ( c > g )
              {
              f /= RADIX;
              c /= sqrdx;
              }
            if ( (c + r) / f < 0.95 * s )
               {
               last = 0;
               g = 1.0 / f;
               for ( j = 1; j <= n; j++ ) a[i][j] *= g;
               for ( j = 1; j <= n; j++ ) a[j][i] *= f;
               }
            }
         }
     }
}

/****************************************************************************/

void elmhes( real **a, int n )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   int  m, j, i;
   real y, x;

   for ( m = 2; m < n; m++ )
       {
       x = 0.0;
       i = m;
       for ( j = m; j <= n; j++ )
           {
           if ( fabs( a[j][m-1] ) > fabs( x ) )
              {
              x = a[j][m-1];
              i = j;
              }
           }
       if ( i != m )
          {
          for ( j = m-1; j <= n; j++ ) SWAP(a[i][j],a[m][j])
          for ( j = 1; j <= n; j++ ) SWAP(a[j][i],a[j][m])
          }
       if ( x )
          {
          for ( i = m+1; i <= n; i++ )
              {
              if ( (y = a[i][m-1] ) != 0.0 )
                 {
                 y /= x;
                 a[i][m-1] = y;
                 for ( j = m; j <= n; j++ ) a[i][j] -= y * a[m][j];
                 for ( j = 1; j <= n; j++ ) a[j][m] += y * a[j][i];
                 }
              }
          }
       }
}

/****************************************************************************/
/****************************************************************************/

void svdcp( real **a, int m, int n, real *w, real **v )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   real anorm, c, f, g, h, s, scale, x, y, z, *rv1;
   int  flag, i, its, j, jj, k, l, nm;
   real pythag( real a, real b );

   rv1 = vector( 1, n );
   g   = scale = anorm = 0.0;

   for ( i = 1; i <= n; i++ )
       {
       l = i + 1;
       rv1[i] = scale * g;
       g = s = scale = 0.0;
       if ( i <= m )
          {
          for ( k = i; k <= m; k++ ) scale += fabs( a[k][i] );
          if ( scale )
             {
             for ( k = i; k <= m; k++ )
                 {
                 a[k][i] /= scale;
                 s += a[k][i] * a[k][i];
                 }
             f = a[i][i];
             g = -SIGN(sqrt(s),f);
             h = f * g - s;
             a[i][i] = f - g;
             for ( j = l; j <= n; j++)
                 {
                 for ( s = 0.0, k = i; k <= m; k++ ) s += a[k][i] * a[k][j];
                 f = s / h;
                 for ( k = i; k <= m; k++ ) a[k][j] += f * a[k][i];
                 }
             for ( k = i; k <= m; k++ ) a[k][i] *= scale;
             }
          }
       w[i] = scale * g;
       g = s = scale = 0.0;
       if ( i <= m && i != n )
          {
          for ( k = l; k <= n; k++ ) scale += fabs( a[i][k] );
          if ( scale )
             {
             for ( k = l; k <= n; k++)
                 {
                 a[i][k] /= scale;
                 s += a[i][k] * a[i][k];
                 }
             f = a[i][l];
             g = -SIGN(sqrt(s),f);
             h = f * g - s;
             a[i][l] = f - g;
             for ( k = l; k <= n; k++ ) rv1[k] = a[i][k] / h;
             for ( j = l; j <=m ; j++)
                 {
                 for ( s = 0.0, k = l; k <= n; k++ ) s += a[j][k] * a[i][k];
                 for ( k = l; k <= n; k++ ) a[j][k] += s * rv1[k];
                 }
             for ( k = l; k <= n; k++) a[i][k] *= scale;
             }
          }
       anorm = rmax(anorm,(fabs(w[i])+fabs(rv1[i])));
       }

   for ( i = n; i >= 1; i-- )
       {
       if ( i < n )
          {
          if ( g )
             {
             for ( j = l; j <= n; j++ )
                 v[j][i] = (a[i][j] / a[i][l]) / g;
             for ( j = l; j <= n; j++ )
                 {
                 for ( s = 0.0, k = l; k <= n; k++ ) s += a[i][k] * v[k][j];
                 for ( k = l; k <= n; k++ ) v[k][j] += s * v[k][i];
                 }
             }
          for ( j = l; j <= n; j++ ) v[i][j] = v[j][i] = 0.0;
          }
       v[i][i] = 1.0;
       g = rv1[i];
       l = i;
       }

   for ( i = IMIN(m,n); i >= 1; i-- )
       {
       l = i + 1;
       g = w[i];
       for ( j = l; j <= n; j++ ) a[i][j] = 0.0;
       if ( g )
          {
          g = 1.0 / g;
          for ( j = l; j <= n; j++ )
              {
              for ( s = 0.0, k = l; k <= m; k++ ) s += a[k][i] * a[k][j];
              f = (s / a[i][i]) * g;
              for ( k = i; k <= m; k++ ) a[k][j] += f * a[k][i];
              }
          for ( j = i; j <= m; j++ ) a[j][i] *= g;
          }
       else
          for ( j = i; j <= m; j++ ) a[j][i] = 0.0;
       ++a[i][i];
       }

   for ( k = n; k >= 1; k-- )
       {
       for ( its = 1; its <= 30; its++ )
           {
           flag = 1;
           for ( l = k; l >= 1;l-- )
               {
               nm = l - 1;
               if ( (real)(fabs( rv1[l]) + anorm ) == anorm )
                  {
                  flag = 0;
                  break;
                  }
               if ( (real)(fabs(w[nm]) + anorm) == anorm ) break;
               }
           if ( flag )
              {
              c = 0.0;
              s = 1.0;
              for ( i = l; i <= k; i++)
                  {
                  f = s * rv1[i];
                  rv1[i] = c * rv1[i];
                  if ((real)(fabs( f ) + anorm) == anorm ) break;
                  g = w[i];
                  h = pythag( f, g );
                  w[i] = h;
                  h = 1.0 / h;
                  c = g * h;
                  s = -f * h;
                  for ( j = 1; j <= m; j++ )
                      {
                      y = a[j][nm];
                      z = a[j][i];
                      a[j][nm] = y * c + z * s;
                      a[j][i] = z * c - y * s;
                      }
                  }
              }
           z = w[k];
           if ( l == k )
              {
              if ( z < 0.0 )
                 {
                 w[k] = -z;
                 for ( j = 1; j <= n; j++ ) v[j][k] = -v[j][k];
                 }
              break;
              }
           if ( its == 30 )
              nrerror( "NO CONVERGENCE IN 30 svdcmp() ITERATIONS" );
           x = w[l];
           nm = k - 1;
           y = w[nm];
           g = rv1[nm];
           h = rv1[k];
           f = ((y - z) * (y + z) + (g - h) * (g + h)) / (2.0 * h * y);
           g = pythag( f, 1.0 );
           f = ((x - z) * (x + z) + h * ((y / (f + SIGN(g,f))) - h)) / x;
           c = s = 1.0;
           for ( j = l; j <= nm; j++ )
               {
               i = j + 1;
               g = rv1[i];
               y = w[i];
               h = s * g;
               g = c * g;
               z = pythag( f, h );
               rv1[j] = z;
               c = f / z;
               s = h / z;
               f = x * c + g * s;
               g = g * c - x * s;
               h = y * s;
               y *= c;
               for ( jj = 1; jj <= n; jj++ )
                   {
                   x = v[jj][j];
                   z = v[jj][i];
                   v[jj][j] = x * c + z * s;
                   v[jj][i] = z * c - x * s;
                   }
               z = pythag( f, h );
               w[j] = z;
               if ( z )
                  {
                  z = 1.0 / z;
                  c = f * z;
                  s = h * z;
                  }
               f = c * g + s * y;
               x = c * y - s * g;
               for ( jj = 1; jj <= m; jj++ )
                   {
                   y = a[jj][j];
                   z = a[jj][i];
                   a[jj][j] = y * c + z * s;
                   a[jj][i] = z * c - y * s;
                   }
               }
           rv1[l] = 0.0;
           rv1[k] = f;
           w[k]   = x;
           }
       }
   free_vector( rv1, 1, n );
}

/****************************************************************************/

void svsol( real **u, real *w, real **v, int n, real *x )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   int  jj, j, i;
   real s, *tmp, wmax, wmin;

   tmp  = vector( 1, n );
   wmax = 0.0;
   for ( j = 1; j <= n; j++ ) if ( w[j] > wmax ) wmax = w[j];
   wmin = wmax * sqrt( macheps );
   for ( j = 1; j <= n; j++ ) if ( w[j] < wmin ) w[j] = 0.0;

   for ( j = 1; j <= n; j++ )
       {
       s = 0.0;
       if ( w[j] )
          {
          for ( i = 1; i <= n; i++ ) s += u[i][j] * x[i];
          s /= w[j];
          }
       tmp[j] = s;
       }
   for ( j = 1; j <= n; j++ )
       {
       s = 0.0;
       for ( jj = 1; jj <= n; jj++ ) s += v[j][jj] * tmp[jj];
       x[j] = s;
       }
   free_vector( tmp, 1, n );
}

/****************************************************************************/
/****************************************************************************/

void nrerror( char error_text[] )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   printf( "Unrecoverable run-time error:\n" );
   printf( "%s\n", error_text );
   printf( "... Exiting to system ...\n" );
   exit( 1 );
}

/****************************************************************************/

real *vector( long nl, long nh )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   real *v;

   v = (real *)malloc((size_t)((nh-nl+1) * sizeof(real)));
   if ( !v ) nrerror( "ALLOCATION FAILURE IN vector()" );
   return( v-nl );
}

/****************************************************************************/

int *ivector( long nl, long nh )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   int *v;

   v = (int *)malloc((size_t)((nh-nl+1) * sizeof(int)));
   if ( !v ) nrerror( "ALLOCATION FAILURE IN ivector()" );
   return( v-nl );
}

/****************************************************************************/

real **matrix( long nrl, long nrh, long ncl, long nch )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   long i, nrow=nrh-nrl+1, ncol=nch-ncl+1;
   real **m;

/* Allocate pointers to rows:                                               */

   m = (real **)malloc((size_t)(nrow * sizeof(real*)));
   if ( !m ) nrerror( "ALLOCATION FAILURE 1 IN matrix()" );
   m -= nrl;

/* Allocate rows and set pointers to them:                                  */

   m[nrl] = (real *)malloc((size_t)(nrow * ncol * sizeof(real)));
   if ( !m[nrl] ) nrerror( "ALLOCATION FAILURE 2 IN matrix()" );
   m[nrl] -= ncl;

   for( i = nrl + 1; i <= nrh; i++ ) m[i] = m[i-1] + ncol;

/* Return pointer to array of pointers to rows:                             */

   return( m );
}

/****************************************************************************/

int **imatrix( long nrl, long nrh, long ncl, long nch )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   long i, nrow=nrh-nrl+1,ncol=nch-ncl+1;
   int  **m;

/* Allocate pointers to rows:                                               */

   m = (int **)malloc((size_t)(nrow * sizeof(int*)));
   if ( !m ) nrerror( "ALLOCATION FAILURE 1 IN imatrix()" );
   m -= nrl;

/* Allocate rows and set pointers to them:                                  */

   m[nrl] = (int *)malloc((size_t)(nrow * ncol * sizeof(int)));
   if ( !m[nrl] ) nrerror( "ALLOCATION FAILURE 2 IN imatrix()" );
   m[nrl] -= ncl;

   for( i = nrl + 1; i <= nrh; i++ ) m[i] = m[i-1] + ncol;

/* Return pointer to array of pointers to rows:                             */

   return( m );
}

/****************************************************************************/

real ***tensor( long nrl, long nrh, long ncl, long nch, long ndl, long ndh )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   long i, j, nrow=nrh-nrl+1, ncol=nch-ncl+1, ndep=ndh-ndl+1;
   real ***t;

/* Allocate pointers to pointers to rows:                                    */

   t = (real ***)malloc((size_t)(nrow * sizeof(real**)));
   if ( !t ) nrerror( "ALLOCATION FAILURE 1 IN tensor()" );
   t -= nrl;

/* Allocate pointers to rows and set pointers to them:                       */

   t[nrl] = (real **)malloc((size_t)(nrow * ncol * sizeof(real*)));
   if ( !t[nrl] ) nrerror( "ALLOCATION FAILURE 2 IN tensor()" );
   t[nrl] -= ncl;

/* Allocate rows and set pointers to them:                                   */

   t[nrl][ncl] = (real *)malloc((size_t)(nrow * ncol * ndep * sizeof(real)));
   if ( !t[nrl][ncl] ) nrerror( "ALLOCATION FAILURE 3 IN tensor()" );
   t[nrl][ncl] -= ndl;

   for (j = ncl + 1; j <= nch; j++ ) t[nrl][j] = t[nrl][j-1] + ndep;
   for (i = nrl + 1; i <= nrh; i++)
       {
       t[i] = t[i-1] + ncol;
       t[i][ncl] = t[i-1][ncl] + ncol * ndep;
       for( j = ncl + 1; j <= nch; j++ ) t[i][j] = t[i][j-1] + ndep;
       }

/* Return pointer to array of pointers to rows:                              */

   return( t );
}

/****************************************************************************/

void free_vector( real *v, long nl, long nh )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   free((FREE_ARG) (v + nl));
}

/****************************************************************************/

void free_ivector( int *v, long nl, long nh )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   free((FREE_ARG) (v + nl));
}

/****************************************************************************/

void free_matrix( real **m, long nrl, long nrh, long ncl, long nch )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   free((FREE_ARG) (m[nrl] + ncl));
   free((FREE_ARG) (m + nrl));
}

/****************************************************************************/

void free_imatrix( int **m, long nrl, long nrh, long ncl, long nch )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   free((FREE_ARG) (m[nrl] + ncl));
   free((FREE_ARG) (m + nrl));
}

/****************************************************************************/

void free_tensor( real ***t, long nrl, long nrh, long ncl, long nch,
                  long ndl, long ndh )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.                        */

{
   free((FREE_ARG) (t[nrl][ncl] + ndl));
   free((FREE_ARG) (t[nrl] + ncl));
   free((FREE_ARG) (t + nrl));
}

/****************************************************************************/
/****************************************************************************/

real rmax( real a, real b )

{
   return( ( a > b ) ? a : b );
}

/****************************************************************************/

real rmin( real a, real b )

{
   return( ( a < b ) ? a : b );
}

/****************************************************************************/

real cmacheps( void )

{
   real e;

   e = 1.0;
   do {
      e /= 2.0;
   } while ( (1.0 + e ) > 1.0 );
   return( 2.0 * e );
}

/****************************************************************************/

int round_local( real num )

{
   real tmp1, tmp2;
   int  itmp;

   tmp1 = fabs( num );
   tmp2 = floor( tmp1 );
   if ( tmp1 - tmp2 >= 0.5 ) tmp2 = ceil( tmp1 );
   itmp = (int)tmp2;

   if ( num >= 0.0 )
      return( itmp );
   else
      return( -itmp );
}

/****************************************************************************/

real pythag( real a, real b )

{
   real at, bt;

   at = fabs( a );
   bt = fabs( b );
   at *= at;
   bt *= bt;
   return( sqrt( at + bt ) );
}

/****************************************************************************/
/****************************************************************************/

STRING NEW_STR( int size )

// Returns the starting address of a (size + 1)-char string, allocates space
// and initializes the string to an empty string.

{
    STRING s;

    if ( (s = (STRING)malloc( (size_t)(size + 1) * sizeof( char ) )) == NULL )
       return( NULL );
    else
       {
       *s = '\0';
       return( s );
       }
}

/****************************************************************************/

void FREE_STR( STRING s )

// Deallocates previously allocated space for string s.

{
    free( (char *)s );
}

/****************************************************************************/

int DELETE_STR( STRING s, int i, int n )

// Removes n chars from string s starting from the i-th position of s.

{
    if ( (i >= 0) && (n > 0) && ((i + n) <= strlen( s )) )
       {
       s[i] = '\0';
       strcat( s, s + i + n );
       return( OK );
       }
    else
       return( WRONG );
}

/****************************************************************************/

int COPY_STR( STRING source, int i, int n, STRING dest )

// Fills string dest with a copy of a substring of source, starting from
// the i-th position of source and consisting of n chars.

{
    register int j;

    if ( (i >= 0) && (n > 0) && ((i + n) <= strlen( source )) )
       {
       for ( j = 0; j <= n - 1; j++ )
           dest[j] = source[i + j ];
       dest[n] = '\0';
       return( OK );
       }
    else
       return( WRONG );
}

/****************************************************************************/

int INSERT_STR( STRING s1, STRING s, int i )

// Inserts the string s1 into s at the i-th position of s;
// if i == strlen( s ), then s1 is added to s.

{
    register int j, k;

    j = strlen( s1 );
    if ( (i >= 0) && (i <= strlen( s )) && (j > 0)  )
       {
       for ( k = strlen( s ) - i; k >= 0; k-- )
           s[i + j + k] = s[i + k];
       s += i;
       while ( *s1 ) *s++ = *s1++;
       return( OK );
       }
    else
       return( WRONG );
}

/****************************************************************************/

int POS_STR( STRING s1, STRING s2 )

// Returns the position (in s2) of the first occurrence of s1 in s2,
// or -1 if s1 does not exist in s2.

{
    STRING aux1, aux2, prevs2 = s2;

    for ( aux1 = s1, aux2 = s2; *aux2; aux2++ )
        if ( *aux1 == *aux2 )
           {
           aux1++;
           if ( !*aux1 )
              return( aux2 - s2 - (aux1 - s1) + 1 );
           }
        else
          {
          aux1 = s1;
          aux2 = prevs2++;
          }
    return( -1 );
}

/****************************************************************************/

int CHANGE_STR( STRING s1, int i, STRING s2 )

// Replaces strlen( s2 ) chars in s1, starting from the ith position of s1,
// with the chars from the string s2.

{
    int j, s1l, s2l;

    s1l = strlen( s1 );
    s2l = strlen( s2 );
    if ( (s1l == 0) || (i < 0) || (i > s1l) || ((i + s2l) > s1l) )
       return( WRONG );
    if ( s2l == 0 )
       return( OK );
    if ( s2l > (s1l - i + 1) )
       {
       s1[i] = '\0';
       strcat( s1, s2 );
       }
    else
       {
       s1 += i;
       for ( j = 0; j < s2l; j++ )
           *s1++ = *s2++;
       }
    return( OK );
}

/****************************************************************************/

void BLANKS_STR( STRING s )

{
   int k, len;

   len = strlen( s );
   while ( (len > 0) && (s[--len] == ' ') )     // Remove trailing blanks
      s[len] = '\0';
   k = 0;                                       // Remove leading blanks
   while ( (k <= len) && (s[k] == ' ') )
      k++;
   COPY_STR( s, k, len+1-k, s );
}

/****************************************************************************/

void UPCASE_STR( STRING s )

{
   int i, len;

   len = strlen( s );
   for ( i = 0; i < len; i++ )
       s[i] = toupper( s[i] );
}

/****************************************************************************/
/****************************************************************************/

void Easter( int *day, int *month, int year )

{
   div_t a, b, c, d, e;

   a = div( year, 19 );
   b = div( year,  4 );
   c = div( year,  7 );
   d = div( 19 * a.rem + 24, 30 );
   e = div( 2 * b.rem + 4 * c.rem + 6 * d.rem + 5, 7 );

   *day = 22 + d.rem + e.rem;

   if ( *day <= 31 )
      *month = 3;
   else
      {
      *day  -= 31;
      *month =  4;
      }
}

/****************************************************************************/
/****************************************************************************/

void calcnu( double omega, int s, double delta, int r, double *nu, int lags )

{
   int  i, j;
   double sum1, sum2;

   nu[0] = omega;
   for ( j = 1; j <= lags; j++ )
       {
       sum1 = 0.0;
       if ( r > 0 )
          for ( i = 1; i <= j; i++ )
              if ( i <= r ) sum1 = sum1 + delta * nu[j-i];
       sum2 = 0.0;
       if ( s > 0 )
          if ( j <= s ) sum2 = omega;
       nu[j] = sum1 - sum2;
       }
}

/****************************************************************************/
/*****************************************************************************/

/*****************************************************************************/
/* DISTRIBUCIÓN CHI-CUADRADO - FUNCIÓN DE DISTRIBUCIÓN ACUMULADA (CDF)      */
/*****************************************************************************/

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Función gamma (necesaria para chi-cuadrado) */
static real gamma_func(real x) {
    /* Aproximación de Stirling para gamma(x) */
    real coef[6] = {
        76.18009172947146,
        -86.50532032941677,
        24.01409824083091,
        -1.231739572450155,
        0.1208650973866179e-2,
        -0.5395239384953e-5
    };

    real ser = 1.000000000190015;
    real tmp = x + 5.5;
    tmp -= (x + 0.5) * log(tmp);

    for (int j = 0; j < 6; j++) {
        ser += coef[j] / (x + j + 1);
    }

    return exp(-tmp + log(2.5066282746310005 * ser / x));
}

/* Función gamma incompleta regularizada P(a,x) */
static real gammap(real a, real x) {
    /* Serie para gamma incompleta regularizada */
    if (x < a + 1.0) {
        /* Usar desarrollo en serie */
        real ap = a;
        real del = 1.0 / a;
        real sum = del;

        for (int n = 1; n <= 100; n++) {
            ap += 1.0;
            del *= x / ap;
            sum += del;
            if (fabs(del) < fabs(sum) * 1e-10) break;
        }

        return sum * exp(-x + a * log(x) - log(gamma_func(a)));
    } else {
        /* Usar fracción continua */
        real b = x + 1.0 - a;
        real c = 1.0 / 1e-30;
        real d = 1.0 / b;
        real h = d;

        for (int i = 1; i <= 100; i++) {
            real an = -i * (i - a);
            b += 2.0;
            d = an * d + b;
            if (fabs(d) < 1e-30) d = 1e-30;
            c = b + an / c;
            if (fabs(c) < 1e-30) c = 1e-30;
            d = 1.0 / d;
            real del = d * c;
            h *= del;
            if (fabs(del - 1.0) < 1e-10) break;
        }

        return 1.0 - exp(-x + a * log(x) - log(gamma_func(a))) * h;
    }
}

/* Distribución chi-cuadrado acumulada - P(?² < x | df) */
real chisq(real x, int df) {
    if (df <= 0) return 0.0;
    if (x <= 0.0) return 0.0;
    if (x > 1000.0) return 1.0;  /* Límite superior */

    /* Para df pequeños, cálculo exacto */
    if (df < 30) {
        return gammap(df / 2.0, x / 2.0);
    } else {
        /* Para df grandes, usar aproximación normal (Wilson-Hilferty) */
        real z = (pow(x / df, 1.0/3.0) - (1.0 - 2.0/(9.0 * df))) / sqrt(2.0/(9.0 * df));

        /* Distribución normal acumulada */
        real t = 1.0 / (1.0 + 0.2316419 * fabs(z));
        real d = 0.3989423 * exp(-z * z / 2.0);
        real prob = d * t * (0.3193815 + t * (-0.3565638 + t * (1.7814779 + t * (-1.821256 + t * 1.330274))));

        if (z > 0) prob = 1.0 - prob;

        /* Ajustar para cola izquierda */
        if (z < 0) prob = 1.0 - prob;

        return prob;
    }
}

/* Versión alternativa más simple (menos precisa pero más rápida) */
real chisq_simple(real x, int df) {
    if (df <= 0 || x <= 0.0) return 0.0;
    if (x > 1000.0) return 1.0;

    /* Aproximación de Wilson-Hilferty (buena para df > 30) */
    if (df > 30) {
        real z = (pow(x / df, 1.0/3.0) - (1.0 - 2.0/(9.0 * df))) / sqrt(2.0/(9.0 * df));

        /* Distribución normal estándar acumulada usando erf */
        return 0.5 * (1.0 + erf(z / sqrt(2.0)));
    }

    /* Para df pequeños, usar serie más simple */
    real sum = 0.0;
    real term = exp(-x/2.0);

    if (df % 2 == 0) {
        /* df par */
        int k = df / 2;
        real running_sum = term;
        for (int i = 1; i < k; i++) {
            term *= x / (2.0 * i);
            running_sum += term;
        }
        sum = 1.0 - running_sum;
    } else {
        /* df impar - usar fórmula más compleja */
        real sqrt_x = sqrt(x);
        real t = sqrt_x / sqrt(df);
        sum = 2.0 * (1.0 - 0.5 * (1.0 + erf(t/sqrt(2.0))));

        /* Corrección */
        if (df > 1) {
            term = exp(-x/2.0) * sqrt(x/2.0) / sqrt(M_PI);
            sum += term;
            for (int i = 1; i < (df-1)/2; i++) {
                term *= x / (2.0 * i + 1.0);
                sum += term;
            }
        }
    }

    return (sum < 0.0) ? 0.0 : (sum > 1.0) ? 1.0 : sum;
}

/*****************************************************************************/
/* DISTRIBUCIÓN t-STUDENT - FUNCIÓN DE DISTRIBUCIÓN ACUMULADA (CDF)         */
/*****************************************************************************/

real tdist(real t, int df) {
    if (df <= 0) return 0.5;  /* Distribución indefinida */

    real x = df / (df + t * t);

    if (df % 2 == 0) {
        /* Grados de libertad par */
        real prob = 0.5 * (1.0 + t / sqrt(df + t * t));
        real term = 1.0;

        for (int i = 2; i <= df - 2; i += 2) {
            term *= (i - 1.0) * x / i;
            prob += term;
        }

        return prob;
    } else {
        /* Grados de libertad impar */
        real prob = 0.5 + atan(t / sqrt(df)) / M_PI;
        if (df == 1) return prob;

        real term = t * sqrt(x) / sqrt(df * M_PI);
        prob += term;

        for (int i = 3; i <= df - 2; i += 2) {
            term *= (i - 2.0) * x / (i - 1.0);
            prob += term;
        }

        return prob;
    }
}

/* Versión alternativa usando beta incompleta */
real tdist_beta(real t, int df) {
    real x = df / (df + t * t);

    /* Beta incompleta regularizada I_x(a,b) */
    /* Para la t-student: P(T < t) = 1 - 0.5 * I_{df/(df+t^2)}(df/2, 1/2) si t > 0 */

    if (t == 0.0) return 0.5;

    real a = df / 2.0;
    real b = 0.5;

    /* Aproximación de la beta incompleta */
    real bt = exp(lgamma(a + b) - lgamma(a) - lgamma(b) + a * log(x) + b * log(1.0 - x));

    if (x < (a + 1.0) / (a + b + 2.0)) {
        /* Usar desarrollo en serie */
        real apb = a + b;
        real ap1 = a + 1.0;
        real ns = floor(b + x * apb);
        real tx, sum;
        real w = 1.0;

        for (int i = 0; i < ns; i++) {
            w *= (apb + i) * x / (ap1 + i);
        }

        tx = w;
        sum = w;

        for (int n = ns + 1; n <= 100; n++) {
            w *= (apb + n - 1.0) * x / (ap1 + n - 1.0);
            sum += w;
            if (fabs(w) < fabs(sum) * 1e-10) break;
        }

        real result = bt * sum / (a * (1.0 + (b - ns) / (ap1 + ns - 1.0)));

        if (t > 0) {
            return 1.0 - 0.5 * result;
        } else {
            return 0.5 * result;
        }
    } else {
        /* Usar fracción continua */
        real c = 1.0;
        real d = 1.0 - (a + b) * x / (a + 1.0);
        if (fabs(d) < 1e-30) d = 1e-30;
        d = 1.0 / d;
        real h = d;

        for (int i = 1; i <= 100; i++) {
            int m2 = 2 * i;
            real aa = i * (b - i) * x / ((a + m2 - 1.0) * (a + m2));
            d = 1.0 + aa * d;
            if (fabs(d) < 1e-30) d = 1e-30;
            c = 1.0 + aa / c;
            if (fabs(c) < 1e-30) c = 1e-30;
            d = 1.0 / d;
            h *= d * c;

            aa = -(a + i) * (a + b + i) * x / ((a + m2) * (a + m2 + 1.0));
            d = 1.0 + aa * d;
            if (fabs(d) < 1e-30) d = 1e-30;
            c = 1.0 + aa / c;
            if (fabs(c) < 1e-30) c = 1e-30;
            d = 1.0 / d;
            real del = d * c;
            h *= del;

            if (fabs(del - 1.0) < 1e-10) break;
        }

        real result = 1.0 - bt * h / a;

        if (t > 0) {
            return 1.0 - 0.5 * result;
        } else {
            return 0.5 * result;
        }
    }
}

/* Función de distribución normal estándar acumulada (para referencia) */
real normal_cdf(real z) {
    /* Aproximación de Abramowitz y Stegun (precisión 7.5e-8) */
    real t = 1.0 / (1.0 + 0.2316419 * fabs(z));
    real d = 0.3989423 * exp(-z * z / 2.0);
    real prob = d * t * (0.3193815 + t * (-0.3565638 + t * (1.7814779 + t * (-1.821256 + t * 1.330274))));

    if (z > 0) prob = 1.0 - prob;
    return prob;
}

/*---------------------------------------------------------------------------*/
/*  Funciones auxiliares matriciales                                         */
/*---------------------------------------------------------------------------*/
 void matrix_multiply(real **A, real **B, real **C, int n, int m, int p) {
    int i, j, k;
    for (i = 1; i <= n; i++) {
        for (j = 1; j <= p; j++) {
            real sum = 0.0;
            for (k = 1; k <= m; k++)
                sum += A[i][k] * B[k][j];
            C[i][j] = sum;
        }
    }
}

void matrix_transpose(real **A, real **B, int n, int m) {
    int i, j;
    for (i = 1; i <= n; i++)
        for (j = 1; j <= m; j++)
            B[j][i] = A[i][j];
}

 void matrix_inverse(real **A, real **invA, int n) {
    /* Calcula la inversa de A usando LU (ludcp y lusol) */
    real **Acopy = matrix(1, n, 1, n);
    int *ip = ivector(1, n);
    real *col = vector(1, n);
    int i, j;

    for (i = 1; i <= n; i++)
        for (j = 1; j <= n; j++)
            Acopy[i][j] = A[i][j];

    ludcp(Acopy, n, ip);
    for (j = 1; j <= n; j++) {
        for (i = 1; i <= n; i++) col[i] = 0.0;
        col[j] = 1.0;
        lusol(Acopy, col, n, ip);
        for (i = 1; i <= n; i++) invA[i][j] = col[i];
    }

    free_vector(col, 1, n);
    free_ivector(ip, 1, n);
    free_matrix(Acopy, 1, n, 1, n);
}


/*****************************************************************************/


#undef FREE_ARG
#undef SIGN
#undef SWAP
#undef RADIX
#undef IMIN

/****************************************************************************/
