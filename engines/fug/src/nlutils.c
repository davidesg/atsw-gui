
/*****************************************************************************/
/*  NLATOOLS.C                                                               */
/*  Copyright (C) Jose Alberto Mauricio, 1995 (except where indicated).      */
/*****************************************************************************/

#include "fug.h"

#define RADIX 2.0
#define SWAP(g,h) {y=(g);(g)=(h);(h)=y;}
#define SIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))


/*****************************************************************************/
/*****************************************************************************/


/****************************************************************************/

void nrerror( char error_text[] )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.  Public Domain Software                      */
/* see: http://www.numerical-recipes.com/public-domain.html                                            */

{
   printf( "Unrecoverable run-time error:\n" );
   printf( "%s\n", error_text );
   printf( "... Exiting to system ...\n" );
   exit( 1 );
}

/****************************************************************************/

real *vector( long nl, long nh )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.    Public Domain Software                     */
/* see: http://www.numerical-recipes.com/public-domain.html                                        */

{
   real *v;

   v = (real *)malloc((size_t)((nh-nl+1) * sizeof(real)));
   if ( !v ) nrerror( "ALLOCATION FAILURE IN vector()" );
   return( v-nl );
}

/****************************************************************************/

int *ivector( long nl, long nh )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.   Public Domain Software                      */
/* see: http://www.numerical-recipes.com/public-domain.html                                        */

{
   int *v;

   v = (int *)malloc((size_t)((nh-nl+1) * sizeof(int)));
   if ( !v ) nrerror( "ALLOCATION FAILURE IN ivector()" );
   return( v-nl );
}

/****************************************************************************/

real **matrix( long nrl, long nrh, long ncl, long nch )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.   Public Domain Software              */
/* see: http://www.numerical-recipes.com/public-domain.html                                */


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

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.    Public Domain Software             */
/* see: http://www.numerical-recipes.com/public-domain.html                                */

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

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.   Public Domain Software            */
/* see: http://www.numerical-recipes.com/public-domain.html                              */

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

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.    Public Domain Software      */
/* see: http://www.numerical-recipes.com/public-domain.html                         */
{
   free((FREE_ARG) (v + nl));
}

/****************************************************************************/

void free_ivector( int *v, long nl, long nh )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.     Public Domain Software   */
/* see: http://www.numerical-recipes.com/public-domain.html                       */

{
   free((FREE_ARG) (v + nl));
}

/****************************************************************************/

void free_matrix( real **m, long nrl, long nrh, long ncl, long nch )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.     Public Domain Software   */
/* see: http://www.numerical-recipes.com/public-domain.html                       */

{
   free((FREE_ARG) (m[nrl] + ncl));
   free((FREE_ARG) (m + nrl));
}

/****************************************************************************/

void free_imatrix( int **m, long nrl, long nrh, long ncl, long nch )

/* (C) Copr. 1986-92 Numerical Recipes Software *!-.     Public Domain Software   */
/* see: http://www.numerical-recipes.com/public-domain.html                       */

{
   free((FREE_ARG) (m[nrl] + ncl));
   free((FREE_ARG) (m + nrl));
}

/****************************************************************************/


void free_tensor( real ***t, long nrl, long nrh, long ncl, long nch,
                  long ndl, long ndh )
/* (C) Copr. 1986-92 Numerical Recipes Software *!-.     Public Domain Software   */
/* see: http://www.numerical-recipes.com/public-domain.html                       */


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

int iround( real num )

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
    if ( (i >= 0) && (n > 0) && ((size_t)(i + n) <= strlen( s )) )
       {
       memmove( s + i, s + i + n, strlen( s + i + n ) + 1 );
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

    if ( (i >= 0) && (n > 0) && ((size_t)(i + n) <= strlen( source )) )
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
    if ( (i >= 0) && ((size_t)i <= strlen( s )) && (j > 0)  )
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

void choleskyg( double **a, int n, double p[] )
{
	void nrerror( char error_text[] );
	int i,j,k;
	double sum;

	for ( i=1; i<=n; i++ ) {
		for ( j=i; j<=n; j++) {
			for ( sum=a[i][j],k=i-1; k>=1; k--) sum -= a[i][k]*a[j][k];
			if (i == j) {
				if (sum <= 0.0)
					nrerror("cholesky failed");
				p[i]=sqrt(sum);
			} else a[j][i]=sum/p[i];
		}
	}
}

/****************************************************************************/
/****************************************************************************/

void choleskyinverseg( double **a, double *p, double **h, int n )
{
  int i,j,k;
  double sum;
  for( i=1; i<=n; i++)
	{
	a[i][i] = 1.0/p[i];
	h[i][i] = a[i][i];
	for( j=i+1; j<=n; j++)
		{
		h[i][j] = 0.0;
		sum = 0.0;
		for( k=i; k<j; k++)
			{
			sum -= a[j][k]*a[k][i];
			}
		a[j][i] = sum/p[j];
		h[j][i] = a[j][i];
		}
	}
  
}

void productmatrixg ( double **a, double **b, double **t, int n, int m, int p, int q )
/* Calculate the product: a(n,m)*b(p,q)* with m = p */
{
	
	int i, j, k;
	double sum;

	if (m == p)

		{
		for (i=1; i<=n; i++)
			{
			for (k=1; k<=q; k++)
				{
				sum = 0;
				for ( j=1; j<=m; j++)
					{
					sum += a[i][j]*b[j][k];
					}

				t[i][k] = sum;
				}
			}

                 }

	else printf("Matrix Multiplication Error Occur: In Pacf Estimation Function");

}

void transposeg ( double **a, double **at, int N, int M )
{
int k, l;
for ( l=1; l <= M; l++ )
        {
        for( k=1; k<=N; k++)
                {
  		at[l][k] = a[k][l];
                }
        }

}

/****************************************************************************/
/****************************************************************************/

#undef FREE_ARG
#undef SIGN
#undef SWAP
#undef RADIX
#undef IMIN

/****************************************************************************/
/****************************************************************************/
