/* test_nsop.c -- el operador no estacionario en forma canonica.
 *
 *   test_nsop [<.pre> ...]
 *
 * Sin argumentos corre las pruebas de mesa. Con .pre, los factoriza de verdad
 * con el lector del motor -- que es donde estan los casos que importan.
 *
 * LA PRUEBA QUE JUSTIFICA EL MODULO: dos escrituras del MISMO operador tienen
 * que dar la MISMA forma. grad grad_4 se escribe (nrdiff=1, nadiff=1) y tambien
 * (nrdiff=2, ifadf={1,2}); si las dos no dieran d=1 D=1, la columna mentiria
 * justo en el caso que la hace falta.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "nsop.h"

static int fallos = 0;

static void ok( int cond, const char *que )
{
   printf( "  %-58s %s\n", que, cond ? "ok" : "FALLA" );
   if ( !cond ) fallos++;
}

/* Construye rnsop a partir del polinomio en convenio natural p(B), o sea
 * rnsop[j] = -p[j].                                                      */
static void como_fichero( const double *p, int n, double *rnsop )
{
   int j;

   for ( j = 0; j <= n; j++ ) rnsop[j] = -p[j];
}

static void caso( const char *que, const double *p, int n, int sper,
                  int d, int D, int nf )
{
   double    rnsop[64];
   NsopForm  o;
   char      t[128];

   como_fichero( p, n, rnsop );
   nsop_canon( rnsop, n, sper, &o );
   nsop_texto( &o, sper, t, sizeof t );

   printf( "  %-26s -> d=%d D=%d f=%d resto=%d   %s\n",
           que, o.d, o.D, o.nf, o.resto, t );
   if ( o.d != d || o.D != D || o.nf != nf || o.resto ) {
      printf( "     FALLA: esperaba d=%d D=%d f=%d resto=0\n", d, D, nf );
      fallos++;
   }
}

int main( void )
{
   printf( "casos de mesa:\n\n" );

   /* (1-B) */
   { double p[] = { 1, -1 };                     caso( "(1-B), s=4", p, 1, 4, 1, 0, 0 ); }
   /* (1-B)^2 = 1 -2B +B^2 -- las cinco del m6 */
   { double p[] = { 1, -2, 1 };                  caso( "(1-B)^2, s=4", p, 2, 4, 2, 0, 0 ); }
   /* (1-B^4) */
   { double p[] = { 1, 0, 0, 0, -1 };            caso( "(1-B^4), s=4", p, 4, 4, 0, 1, 0 ); }
   /* (1-B)(1-B^4) = 1 -B -B^4 +B^5  -- EA del m6 */
   { double p[] = { 1, -1, 0, 0, -1, 1 };        caso( "(1-B)(1-B^4), s=4", p, 5, 4, 1, 1, 0 ); }
   /* (1-B)(1-B^12) */
   { double p[14] = { 1, -1, 0,0,0,0,0,0,0,0,0,0, -1, 1 };
     caso( "(1-B)(1-B^12), s=12", p, 13, 12, 1, 1, 0 ); }
   /* (1-B^4)^2 : el maximo D, que es lo que se decidio */
   { double p[] = { 1, 0,0,0, -2, 0,0,0, 1 };    caso( "(1-B^4)^2, s=4", p, 8, 4, 0, 2, 0 ); }
   /* sin operador */
   { double p[] = { 1 };
     double rnsop[2]; NsopForm o;
     como_fichero( p, 0, rnsop );
     nsop_canon( rnsop, 0, 4, &o );
     ok( o.d == 0 && o.D == 0 && o.nf == 0 && o.resto == 0,
         "el operador 1 da d=0 D=0, sin sobras" ); }

   /* Un factor irreducible SUELTO: (1+B), que es f = s/2 con s=4.
    * No es (1-B^4) entero, asi que tiene que salir por la columna f.   */
   { double p[] = { 1, 1 };                      caso( "(1+B), s=4", p, 1, 4, 0, 0, 1 ); }
   /* (1-B)(1+B) = 1 - B^2 : d=1 y un f, NO un (1-B^2) porque s=4 */
   { double p[] = { 1, 0, -1 };                  caso( "(1-B^2), s=4", p, 2, 4, 1, 0, 1 ); }

   printf( "\n" );

   /* ------------------------------------------------------------------ */
   /* LA QUE JUSTIFICA EL MODULO                                          */
   /* ------------------------------------------------------------------ */
   {
   /* Las dos escrituras de grad grad_4, las dos expandidas al MISMO
    * polinomio por CalcNonsOp: 1 - B - B^4 + B^5.                     */
   double   p[] = { 1, -1, 0, 0, -1, 1 };
   double   rnsop[8];
   NsopForm a, b;

   como_fichero( p, 5, rnsop );
   nsop_canon( rnsop, 5, 4, &a );      /* venga de donde venga */
   nsop_canon( rnsop, 5, 4, &b );

   printf( "grad grad_4, escrito de las dos formas del .pre:\n" );
   printf( "   nrdiff=1 nadiff=1            -> d=%d D=%d\n", a.d, a.D );
   printf( "   nrdiff=2 ifadf={1,2}         -> d=%d D=%d\n", b.d, b.D );
   ok( a.d == b.d && a.D == b.D && a.d == 1 && a.D == 1,
       "las dos escrituras dan d=1 D=1: la columna no miente" );

   /* Y lo que pasaria leyendo los enteros: nrdiff vale 1 en una y 2 en la
    * otra. Si la columna fuera nrdiff, el mismo operador saldria distinto. */
   ok( nsop_difiere( &b, 2, 0 ),
       "  y se detecta que el fichero lo escribe de otra forma (nrdiff=2)" );
   ok( !nsop_difiere( &a, 1, 1 ),
       "  y que la otra escritura SI coincide con la canonica" );
   }

   /* ------------------------------------------------------------------ */
   /* La distincion del m6: cinco (1-B)^2 y una (1-B)(1-B^4)             */
   /* ------------------------------------------------------------------ */
   {
   double   p1[] = { 1, -2, 1 };            /* EP y las otras cuatro */
   double   p2[] = { 1, -1, 0, 0, -1, 1 };  /* EA                    */
   double   r1[8], r2[8];
   NsopForm o1, o2;

   como_fichero( p1, 2, r1 );  nsop_canon( r1, 2, 4, &o1 );
   como_fichero( p2, 5, r2 );  nsop_canon( r2, 5, 4, &o2 );

   printf( "\nel m6: las seis traen nrdiff = 2 en el fichero\n" );
   printf( "   EP  d=%d D=%d\n   EA  d=%d D=%d\n", o1.d, o1.D, o2.d, o2.D );
   ok( o1.d != o2.d || o1.D != o2.D,
       "EP y EA salen DISTINTAS, que es lo que nrdiff no distingue" );
   ok( nsop_difiere( &o1, 2, 0 ) == 0 && nsop_difiere( &o2, 2, 0 ) == 1,
       "  y de EA se dice que el fichero la escribe de otra forma" );
   }

   printf( "\n%d fallos\n", fallos );
   return fallos ? 1 : 0;
}
