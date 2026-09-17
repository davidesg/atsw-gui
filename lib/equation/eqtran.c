/*
 * eqtran.c -- ver eqtran.h.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "eqtran.h"

/* ------------------------------------------------------------------------ */
/* Los items de la transferencia                                             */
/* ------------------------------------------------------------------------ */

static EqItem *nuevo( EqItem *item, int max, int *n, EqItemKind k )
{
   EqItem *it;

   if ( *n >= max ) return NULL;
   it = &item[(*n)++];
   memset( it, 0, sizeof *it );
   it->kind = k;
   it->sign = ' ';
   return it;
}

static void coef( EqItem *item, int max, int *n, double v, double se,
                  char signo )
{
   EqItem *it = nuevo( item, max, n, EI_COEF );

   if ( !it ) return;
   it->value  = fabs( v );
   it->se     = se;
   it->has_se = se > 0.0;
   /* El signo se lleva al operador: la ecuacion se lee mejor con el numero
    * siempre positivo y el signo delante, que es lo que hace fue.        */
   it->sign   = ( v < 0.0 ) ? ( signo == '+' ? '-' : '+' ) : signo;
}

static void op( EqItem *item, int max, int *n, char c )
{
   EqItem *it = nuevo( item, max, n, EI_OP );

   if ( it ) { it->text[0] = c; it->text[1] = 0; }
}

static void texto( EqItem *item, int max, int *n, const char *s )
{
   EqItem *it = nuevo( item, max, n, EI_TEXT );

   if ( it ) snprintf( it->text, sizeof it->text, "%s", s );
}

static void potencia_B( EqItem *item, int max, int *n, int k )
{
   EqItem *it;

   if ( k <= 0 ) return;
   it = nuevo( item, max, n, EI_B );
   if ( it ) it->a = k;
}

int eqtran_items( EqItem *item, int max, const EqLink *lnk, int nlinks )
{
   int n = 0, j, k;

   for ( j = 0; j < nlinks; j++ ) {
      const EqLink *L = &lnk[j];

      if ( j > 0 ) op( item, max, &n, '+' );

      /* El numerador. Con s = 0 es un solo coeficiente y no hace falta el
       * corchete; con s > 0 va entre corchetes porque divide.           */
      if ( L->s > 0 || L->r > 0 ) {
         EqItem *it = nuevo( item, max, &n, EI_OPEN );
         if ( it ) it->bracket = '[';
      }

      coef( item, max, &n, L->omega ? L->omega[0] : 0.0,
            L->omega_se ? L->omega_se[0] : 0.0, ' ' );

      /* w1..ws RESTAN: es el convenio de Box-Jenkins. */
      for ( k = 1; k <= L->s; k++ ) {
         coef( item, max, &n, L->omega ? L->omega[k] : 0.0,
               L->omega_se ? L->omega_se[k] : 0.0, '-' );
         potencia_B( item, max, &n, k );
      }

      if ( L->r > 0 ) {
         EqItem *it;

         it = nuevo( item, max, &n, EI_CLOSE ); if ( it ) it->bracket = ']';
         op( item, max, &n, '/' );
         it = nuevo( item, max, &n, EI_OPEN );  if ( it ) it->bracket = '[';

         texto( item, max, &n, "1" );
         for ( k = 1; k <= L->r; k++ ) {
            coef( item, max, &n, L->delta ? L->delta[k] : 0.0,
                  L->delta_se ? L->delta_se[k] : 0.0, '-' );
            potencia_B( item, max, &n, k );
         }
      }

      if ( L->s > 0 || L->r > 0 ) {
         EqItem *it = nuevo( item, max, &n, EI_CLOSE );
         if ( it ) it->bracket = ']';
      }

      potencia_B( item, max, &n, L->b );
      texto( item, max, &n, L->entrada ? L->entrada : "X" );
   }
   return n;
}

/* ------------------------------------------------------------------------ */
/* A texto plano                                                             */
/* ------------------------------------------------------------------------ */

/* Los decimales con que se escribe un coeficiente: los mismos que usan los
 * dos escritores de fue.                                                  */
static int decimales( const EqItem *it )
{
   double v = fabs( it->value );

   if ( v >= 100.0 ) return 1;
   if ( v >= 10.0  ) return 2;
   if ( v >= 1.0   ) return 3;
   return 4;
}

static int pon( char *out, size_t size, int *pos, const char *s )
{
   int n = (int) strlen( s );

   if ( *pos + n < (int) size ) memcpy( out + *pos, s, n );
   *pos += n;
   return n;
}

static void items_a_texto( char *out, size_t size, int *pos,
                           const EqItem *item, int n )
{
   char b[128];
   int  i;

   for ( i = 0; i < n; i++ ) {
      const EqItem *it = &item[i];

      switch ( it->kind ) {

      case EI_TEXT:
         if ( *pos && out[*pos - 1] != ' ' && out[*pos - 1] != '['
              && out[*pos - 1] != '(' ) pon( out, size, pos, " " );
         pon( out, size, pos, it->text );
         break;

      case EI_COEF:
         if ( it->sign == '-' )      pon( out, size, pos, " - " );
         else if ( it->sign == '+' ) pon( out, size, pos, " + " );
         snprintf( b, sizeof b, "%.*f", decimales( it ), it->value );
         pon( out, size, pos, b );
         break;

      case EI_B:
         if ( it->a == 1 ) pon( out, size, pos, "B" );
         else { snprintf( b, sizeof b, "B^%d", it->a ); pon( out, size, pos, b ); }
         break;

      case EI_NABLA:
         if ( it->b > 1 ) snprintf( b, sizeof b, "\xe2\x88\x87%d", it->b );
         else             snprintf( b, sizeof b, "\xe2\x88\x87" );
         pon( out, size, pos, b );
         if ( it->a > 1 ) { snprintf( b, sizeof b, "^%d", it->a );
                            pon( out, size, pos, b ); }
         break;

      case EI_ALTER:
         pon( out, size, pos, " (-1)^t" );
         break;

      case EI_HARMONIC:
         snprintf( b, sizeof b, " %s(%d\xcf\x80t/%d)", it->text, it->a, it->b );
         pon( out, size, pos, b );
         break;

      case EI_DUMMY:
         snprintf( b, sizeof b, " \xce\xbe_t(%s)", it->text );
         pon( out, size, pos, b );
         break;

      case EI_OPEN:
         /* sin espacio si el anterior ya lo dejo: " /  [" queda feo */
         if ( *pos && out[*pos - 1] == ' ' )
              pon( out, size, pos, it->bracket == '[' ? "[" : "(" );
         else pon( out, size, pos, it->bracket == '[' ? " [" : " (" );
         break;

      case EI_CLOSE:
         pon( out, size, pos, it->bracket == ']' ? "]" : ")" );
         break;

      case EI_OP:
         snprintf( b, sizeof b, " %s ", it->text );
         pon( out, size, pos, b );
         break;
      }
   }
}

int eq_part_texto( char *out, size_t size, const Equation *eq, EqPart p )
{
   int pos = 0;

   if ( !eq || !eq->used[p] ) { if ( size ) out[0] = 0; return 0; }
   items_a_texto( out, size, &pos, eq->item[p], eq->n[p] );
   out[pos < (int) size ? pos : (int) size - 1] = 0;
   return pos;
}

int eqtran_texto( char *out, size_t size,
                  const char *salida, const EqLink *lnk, int nlinks,
                  const Equation *eq )
{
   EqItem item[512];
   int    n, pos = 0;
   char   b[256];

   snprintf( b, sizeof b, "%s_t  =", salida ? salida : "Y" );
   pon( out, size, &pos, b );

   n = eqtran_items( item, 512, lnk, nlinks );
   items_a_texto( out, size, &pos, item, n );

   pon( out, size, &pos, "  +  N_t" );

   if ( eq ) {
      /* La ecuacion del ruido, la que fue ya sabe escribir. */
      static const EqPart orden[] = { EQ_ARR, EQ_ARA, EQ_ARF };
      static const EqPart ma[]    = { EQ_MAR, EQ_MAA, EQ_MAF };
      int i;

      pon( out, size, &pos, "\n\ncon   " );
      for ( i = 0; i < 3; i++ )
         if ( eq->used[orden[i]] )
            items_a_texto( out, size, &pos, eq->item[orden[i]], eq->n[orden[i]] );

      pon( out, size, &pos, " [" );
      for ( i = EQ_NRDIFF; i <= EQ_IFADF; i++ )
         if ( eq->used[i] )
            items_a_texto( out, size, &pos, eq->item[i], eq->n[i] );
      pon( out, size, &pos, " N_t" );
      if ( eq->used[EQ_MU] )
         items_a_texto( out, size, &pos, eq->item[EQ_MU], eq->n[EQ_MU] );
      pon( out, size, &pos, " ]  =" );

      for ( i = 0; i < 3; i++ )
         if ( eq->used[ma[i]] )
            items_a_texto( out, size, &pos, eq->item[ma[i]], eq->n[ma[i]] );
      pon( out, size, &pos, " A_t" );
   }

   out[pos < (int) size ? pos : (int) size - 1] = 0;
   return pos;
}
