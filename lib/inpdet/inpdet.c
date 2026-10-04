/*
 * inpdet.c -- el bloque de deterministas del .inp. Ver inpdet.h.
 *
 * COMO SE CONSERVA EL FICHERO. Todo lo que se puede dejar como estaba se
 * deja como estaba: las líneas de antes y de después del bloque se copian
 * tal cual, y dentro del bloque las líneas que ya había tampoco se tocan --
 * lo nuevo se AÑADE. De ahí sale la propiedad que la batería comprueba:
 * añadir cero variables devuelve el fichero idéntico, byte a byte, y no por
 * casualidad sino por construcción. Lo que esa prueba mide entonces es lo
 * único que puede fallar: que los límites del bloque estén bien encontrados.
 *
 * La gramática es la de inpcheck.c, que repite los fscanf del motor en el
 * mismo orden. Aquí se lee POR LINEAS, que es más estricto que el motor: el
 * motor admitiría dos deterministas en una línea y esto lo rechaza en vez de
 * reescribirlo mal. Rechazar un fichero raro es barato; reescribirlo mal, no.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "inpdet.h"

#define MAX_LINEAS  200000

typedef struct {
   char **l;            /* las lineas, con su '\n'          */
   int    n;
} Fich;

static void fich_free( Fich *f )
{
   int i;

   if ( !f->l ) return;
   for ( i = 0; i < f->n; i++ ) free( f->l[i] );
   free( f->l );
   f->l = NULL;
   f->n = 0;
}

static int fich_lee( const char *ruta, Fich *out, char *porque, size_t np )
{
   FILE  *f = fopen( ruta, "rb" );
   char   buf[4096];
   char **l = NULL;
   int    n = 0, cap = 0;

   out->l = NULL;
   out->n = 0;
   if ( !f )
      { snprintf( porque, np, "no pude abrir «%s»", ruta ); return 1; }

   while ( fgets( buf, sizeof buf, f ) )
       {
       if ( n == cap )
          {
          char **t;

          cap = cap ? cap * 2 : 256;
          if ( cap > MAX_LINEAS ) { fclose( f ); free( l );
              snprintf( porque, np, "«%s» tiene demasiadas líneas", ruta ); return 1; }
          t = (char **) realloc( l, (size_t) cap * sizeof *l );
          if ( !t ) { fclose( f ); free( l );
              snprintf( porque, np, "sin memoria" ); return 1; }
          l = t;
          }
       l[n] = strdup( buf );
       if ( !l[n] ) { fclose( f ); out->l = l; out->n = n; fich_free( out );
           snprintf( porque, np, "sin memoria" ); return 1; }
       n++;
       }
   fclose( f );
   out->l = l;
   out->n = n;
   return 0;
}

static int es_etiqueta( const char *s )
{
   while ( *s == ' ' || *s == '\t' ) s++;
   return ( *s == '*' );
}

/* Los tokens de una linea, sin tocarla. Devuelve cuantos hay. */
static int tokens( const char *linea, char t[][64], int max )
{
   const char *p = linea;
   int         n = 0;

   while ( *p && n < max )
       {
       int i = 0;

       while ( *p && isspace( (unsigned char) *p ) ) p++;
       if ( !*p ) break;
       while ( *p && !isspace( (unsigned char) *p ) && i < 63 ) t[n][i++] = *p++;
       while ( *p && !isspace( (unsigned char) *p ) ) p++;     /* lo que no cupo */
       t[n][i] = '\0';
       n++;
       }
   return n;
}

static int es_numero( const char *s )
{
   char *fin;

   if ( !*s ) return 0;
   strtod( s, &fin );
   return ( *fin == '\0' );
}

/* Cuantos tokens MAS pide este determinista, o -1 si la palabra no vale.
 * Un determinista no estandar (una columna de datos propia) no pide
 * ninguno: su nombre es la linea entera.                                  */
static int pide( const char *palabra, int freq, int *no_estandar )
{
   *no_estandar = 0;
   if ( !strcmp( palabra, "impulse" ) || !strcmp( palabra, "compimp" ) ||
        !strcmp( palabra, "step" )    || !strcmp( palabra, "ramp" ) )
      return ( freq == 1 ) ? 1 : 2;
   if ( !strcmp( palabra, "easter" ) || !strcmp( palabra, "trend" ) ||
        !strcmp( palabra, "alter" ) )
      return 0;
   if ( !strcmp( palabra, "cos" ) || !strcmp( palabra, "sin" ) ) return 1;
   if ( isdigit( (unsigned char) palabra[0] ) || palabra[0] == '-' ||
        palabra[0] == '.' || palabra[0] == '*' )
      return -1;
   *no_estandar = 1;
   return 0;
}

/* Donde empieza y acaba el bloque de deterministas.
 *
 * i_ndet   la linea con el NUMERO de deterministas
 * i_nom    la primera linea de nombres (o -1 si ndet == 0)
 * i_om     la fila con las cuentas de omegas
 * i_del    la fila con las cuentas de deltas
 * i_fin    la primera linea de DESPUES del bloque                          */
typedef struct {
   int ndet, freq;
   int i_ndet, i_nom, i_om, i_del, i_fin;
   int nomega[ID_MAX_DET], ndelta[ID_MAX_DET];
} Bloque;

static int mira( const Fich *f, Bloque *b, char *porque, size_t np )
{
   char t[8][64];
   int  i, k, nt;

   memset( b, 0, sizeof *b );
   if ( f->n < 12 )
      { snprintf( porque, np, "el fichero se acaba antes del modelo: no es un .inp de fue" ); return 1; }

   /* [3.0] cinco lineas libres, [3.1] frecuencia y fecha */
   if ( !es_etiqueta( f->l[5] ) || !es_etiqueta( f->l[7] ) || !es_etiqueta( f->l[9] ) )
      {
      snprintf( porque, np, "la cabecera no tiene las etiquetas donde el motor las cuenta "
                            "(cinco líneas libres y una etiqueta por sección). No lo "
                            "reescribo a ciegas" );
      return 1;
      }
   nt = tokens( f->l[6], t, 8 );
   if ( nt < 1 ) { snprintf( porque, np, "falta la frecuencia" ); return 1; }
   b->freq = strcmp( t[0], "number" ) ? atoi( t[0] ) : 1;
   if ( b->freq < 1 ) { snprintf( porque, np, "frecuencia «%s» que no entiendo", t[0] ); return 1; }

   b->i_ndet = 10;
   nt = tokens( f->l[10], t, 8 );
   if ( nt != 1 || !es_numero( t[0] ) )
      {
      snprintf( porque, np, "donde va el número de deterministas hay «%s». ¿Es un .inp "
                            "de fug o de fuf en vez de uno de fue?", f->l[10] );
      return 1;
      }
   b->ndet = atoi( t[0] );
   if ( b->ndet < 0 || b->ndet > ID_MAX_DET )
      {
      snprintf( porque, np, "el fichero trae %d deterministas y aquí caben %d",
                b->ndet, ID_MAX_DET );
      return 1;
      }

   if ( b->ndet == 0 ) { b->i_nom = -1; b->i_om = -1; b->i_del = -1; b->i_fin = 11; return 0; }

   i = 11;
   if ( i >= f->n || !es_etiqueta( f->l[i] ) )
      { snprintf( porque, np, "falta la etiqueta antes de los nombres" ); return 1; }
   i++;
   b->i_nom = i;

   for ( k = 0; k < b->ndet; k++, i++ )
       {
       int falta, nostd;

       if ( i >= f->n ) { snprintf( porque, np, "el fichero se acaba dentro de los nombres" ); return 1; }
       nt = tokens( f->l[i], t, 8 );
       if ( nt < 1 ) { snprintf( porque, np, "línea vacía dentro de los nombres" ); return 1; }
       falta = pide( t[0], b->freq, &nostd );
       if ( falta < 0 )
          { snprintf( porque, np, "«%s» no es un determinista que yo sepa leer", t[0] ); return 1; }
       if ( !nostd && nt != 1 + falta )
          {
          snprintf( porque, np, "«%s» pone %d cosas en una línea y el motor espera %d. "
                                "Este .inp no reparte un determinista por línea, y no lo "
                                "reescribo sin saber qué es cada cosa", t[0], nt, 1 + falta );
          return 1;
          }
       }

   if ( i >= f->n || !es_etiqueta( f->l[i] ) )
      { snprintf( porque, np, "falta la etiqueta antes de las cuentas de omegas" ); return 1; }
   i++;
   b->i_om = i;
   nt = tokens( f->l[i], t, 8 );
   if ( nt != b->ndet && b->ndet <= 8 )
      { snprintf( porque, np, "hay %d deterministas y %d cuentas de omegas", b->ndet, nt ); return 1; }
   {
   /* la fila entera, que puede tener mas de 8 numeros */
   const char *p = f->l[i];

   for ( k = 0; k < b->ndet; k++ )
       {
       char *fin;
       long  v;

       while ( *p && isspace( (unsigned char) *p ) ) p++;
       v = strtol( p, &fin, 10 );
       if ( fin == p ) { snprintf( porque, np, "faltan cuentas de omegas" ); return 1; }
       if ( v < 0 || v > 64 ) { snprintf( porque, np, "una cuenta de omegas de %ld", v ); return 1; }
       b->nomega[k] = (int) v;
       p = fin;
       }
   }
   i++;

   for ( k = 0; k < b->ndet; k++ )
       {
       int j;

       if ( i >= f->n || !es_etiqueta( f->l[i] ) )
          { snprintf( porque, np, "falta la etiqueta del bloque de omegas %d", k + 1 ); return 1; }
       i++;
       for ( j = 0; j <= b->nomega[k]; j++, i++ )
           {
           if ( i >= f->n ) { snprintf( porque, np, "el fichero se acaba dentro de los omegas" ); return 1; }
           nt = tokens( f->l[i], t, 8 );
           if ( nt < 1 || nt > 2 || !es_numero( t[0] ) )
              {
              snprintf( porque, np, "el omega %d del determinista %d no es «valor bandera» "
                                    "en su propia línea", j + 1, k + 1 );
              return 1;
              }
           }
       }

   if ( i >= f->n || !es_etiqueta( f->l[i] ) )
      { snprintf( porque, np, "falta la etiqueta antes de las cuentas de deltas" ); return 1; }
   i++;
   b->i_del = i;
   {
   const char *p = f->l[i];

   for ( k = 0; k < b->ndet; k++ )
       {
       char *fin;
       long  v;

       while ( *p && isspace( (unsigned char) *p ) ) p++;
       v = strtol( p, &fin, 10 );
       if ( fin == p ) { snprintf( porque, np, "faltan cuentas de deltas" ); return 1; }
       if ( v < 0 || v > 64 ) { snprintf( porque, np, "una cuenta de deltas de %ld", v ); return 1; }
       b->ndelta[k] = (int) v;
       p = fin;
       }
   }
   i++;

   for ( k = 0; k < b->ndet; k++ ) if ( b->ndelta[k] > 0 )
       {
       int j;

       if ( i >= f->n || !es_etiqueta( f->l[i] ) )
          { snprintf( porque, np, "falta la etiqueta del bloque de deltas %d", k + 1 ); return 1; }
       i++;
       for ( j = 0; j < b->ndelta[k]; j++, i++ )
           if ( i >= f->n )
              { snprintf( porque, np, "el fichero se acaba dentro de los deltas" ); return 1; }
       }

   b->i_fin = i;
   return 0;
}


int id_intervenciones( const char *origen, char det[][ID_LINEA], int max,
                       char *porque, size_t n )
{
   Fich   f;
   Bloque b;
   char   t[8][64];
   int    k, hallados = 0;

   if ( fich_lee( origen, &f, porque, n ) ) return -1;
   if ( mira( &f, &b, porque, n ) ) { fich_free( &f ); return -1; }

   for ( k = 0; k < b.ndet; k++ )
       {
       const char *l = f.l[b.i_nom + k];
       int         nt = tokens( l, t, 8 );

       if ( nt < 1 ) continue;
       if ( strcmp( t[0], "impulse" ) && strcmp( t[0], "compimp" ) &&
            strcmp( t[0], "step" )    && strcmp( t[0], "ramp" ) ) continue;
       if ( hallados < max )
          {
          int i, p = 0;

          det[hallados][0] = '\0';
          for ( i = 0; i < nt && p < ID_LINEA - 2; i++ )
              p += snprintf( det[hallados] + p, (size_t)( ID_LINEA - p ),
                             "%s%s", i ? " " : "", t[i] );
          }
       hallados++;
       }
   fich_free( &f );
   return hallados;
}


static int omegas_de( const int *nomega, int k )
{
   int v = nomega ? nomega[k] : 0;

   return ( v < 0 ) ? 0 : ( ( v > 32 ) ? 32 : v );
}

int id_nombres( const char *origen, char det[][ID_LINEA], int max,
                char *porque, size_t n )
{
   Fich   f;
   Bloque b;
   int    k;

   if ( fich_lee( origen, &f, porque, n ) ) return -1;
   if ( mira( &f, &b, porque, n ) ) { fich_free( &f ); return -1; }

   for ( k = 0; k < b.ndet && k < max; k++ )
       {
       char t[8][64];
       int  nt = tokens( f.l[b.i_nom + k], t, 8 ), i, p = 0;

       det[k][0] = '\0';
       for ( i = 0; i < nt && p < ID_LINEA - 2; i++ )
           p += snprintf( det[k] + p, (size_t)( ID_LINEA - p ),
                          "%s%s", i ? " " : "", t[i] );
       }
   k = b.ndet;
   fich_free( &f );
   return k;
}

int id_anade( const char *origen, const char *destino,
              const char *const *nuevo, const int *nomega, int nn,
              char *porque, size_t n )
{
   Fich   f;
   Bloque b;
   FILE  *o;
   char   t[8][64];
   int    i, k, nostd, falta;

   if ( nn < 0 ) nn = 0;
   if ( fich_lee( origen, &f, porque, n ) ) return 1;
   if ( mira( &f, &b, porque, n ) ) { fich_free( &f ); return 1; }

   if ( b.ndet + nn > ID_MAX_DET )
      {
      snprintf( porque, n, "quedarían %d deterministas y aquí caben %d",
                b.ndet + nn, ID_MAX_DET );
      fich_free( &f );
      return 1;
      }

   /* LO QUE SE VA A ESCRIBIR, ANTES DE ABRIR NADA. Si una línea nueva no es
      un determinista que el motor sepa leer, no se toca el destino.     */
   for ( k = 0; k < nn; k++ )
       {
       int nt = tokens( nuevo[k], t, 8 );

       if ( nt < 1 ) { snprintf( porque, n, "el determinista %d está vacío", k + 1 );
                       fich_free( &f ); return 1; }
       falta = pide( t[0], b.freq, &nostd );
       if ( falta < 0 || nostd )
          {
          snprintf( porque, n, "«%s» no es una intervención que yo escriba: un "
                               "determinista con columna de datos propia hay que "
                               "ponerlo a mano, porque trae sus datos", t[0] );
          fich_free( &f );
          return 1;
          }
       if ( nt != 1 + falta )
          {
          snprintf( porque, n, "«%s» necesita %s y trae %d: en una serie de "
                               "frecuencia %d el motor lee «%s»", t[0],
                    ( b.freq == 1 ) ? "el año" : "el período y el año", nt - 1,
                    b.freq, ( b.freq == 1 ) ? "step 2008" : "step 10 2008" );
          fich_free( &f );
          return 1;
          }
       }

   o = fopen( destino, "wb" );
   if ( !o ) { snprintf( porque, n, "no pude escribir «%s»", destino );
               fich_free( &f ); return 1; }

   /* la cabecera, tal cual */
   for ( i = 0; i < b.i_ndet; i++ ) fputs( f.l[i], o );

   /* el numero de deterministas: la linea de siempre si no cambia */
   if ( nn == 0 ) fputs( f.l[b.i_ndet], o );
   else           fprintf( o, "%d\n", b.ndet + nn );

   if ( b.ndet + nn > 0 )
      {
      /* nombres */
      fputs( ( b.i_nom > 0 ) ? f.l[b.i_nom - 1] : "**\n", o );
      for ( k = 0; k < b.ndet; k++ ) fputs( f.l[b.i_nom + k], o );
      for ( k = 0; k < nn; k++ ) fprintf( o, "%s\n", nuevo[k] );

      /* cuentas de omegas: la fila de antes, y un 0 por cada nuevo */
      fputs( ( b.i_om > 0 ) ? f.l[b.i_om - 1] : "**\n", o );
      if ( b.i_om > 0 && nn == 0 ) fputs( f.l[b.i_om], o );
      else
         {
         if ( b.i_om > 0 )
            {
            char *s = strdup( f.l[b.i_om] ), *p;

            if ( s ) { p = s + strlen( s );
                       while ( p > s && ( p[-1] == '\n' || p[-1] == '\r' ) ) *--p = '\0';
                       fputs( s, o ); free( s ); }
            }
         for ( k = 0; k < nn; k++ ) fprintf( o, " %d", omegas_de( nomega, k ) );
         fputs( "\n", o );
         }

      /* los bloques de omegas que habia, y uno nuevo por cada intervencion:
         UN omega, semilla 0.0 y estimado. La semilla en cero y no cerca de
         nada: una semilla mal puesta lleva el ajuste a un optimo espurio. */
      {
      int li = ( b.i_om > 0 ) ? b.i_om + 1 : -1;

      for ( k = 0; k < b.ndet; k++ )
          {
          int j;

          fputs( f.l[li++], o );                       /* su etiqueta */
          for ( j = 0; j <= b.nomega[k]; j++ ) fputs( f.l[li++], o );
          }
      for ( k = 0; k < nn; k++ )
          {
          int j, cuantos = omegas_de( nomega, k ) + 1;

          fputs( "**\n", o );
          for ( j = 0; j < cuantos; j++ ) fputs( "0.000000  1\n", o );
          }

      /* cuentas de deltas */
      if ( b.i_del > 0 )
         {
         fputs( f.l[b.i_del - 1], o );
         if ( nn == 0 ) fputs( f.l[b.i_del], o );
         else
            {
            char *s = strdup( f.l[b.i_del] ), *p;

            if ( s ) { p = s + strlen( s );
                       while ( p > s && ( p[-1] == '\n' || p[-1] == '\r' ) ) *--p = '\0';
                       fputs( s, o ); free( s ); }
            for ( k = 0; k < nn; k++ ) fputs( " 0", o );
            fputs( "\n", o );
            }
         for ( i = b.i_del + 1; i < b.i_fin; i++ ) fputs( f.l[i], o );
         }
      else
         {
         fputs( "**\n", o );
         for ( k = 0; k < nn; k++ ) fprintf( o, "%s0", k ? " " : "" );
         fputs( "\n", o );
         }
      }
      }

   for ( i = b.i_fin; i < f.n; i++ ) fputs( f.l[i], o );

   fich_free( &f );
   if ( fclose( o ) != 0 )
      { snprintf( porque, n, "no pude cerrar «%s»", destino ); return 1; }
   return 0;
}


/* ---------------------------------------------------------------------- */
/* The ARMA part: see inpdet.h.                                            */
/* ---------------------------------------------------------------------- */

/* The four ARMA section labels, in the order fue reads them. "anual" is
 * how the corpus spells the last one, "annual" the second. */
static const char *const ARMA_ETIQ[4] = {
   "regular AR operators", "nual AR operators", "regular MA operators", "nual MA operators"
};

/* Where the four sections are: lab[k] is the label line of section k,
 * fin the first line after the fourth; ord[k] the summed order. */
static int arma_mira( const Fich *f, int lab[4], int *fin, int ord[4],
                      char *porque, size_t np )
{
   int i = 0, k;

   for ( k = 0; k < 4; k++ )
       {
       char t[64][64];
       int  nt, nf, j, o = 0;

       while ( i < f->n && !( es_etiqueta( f->l[i] ) && strstr( f->l[i], ARMA_ETIQ[k] ) ) ) i++;
       if ( i >= f->n )
          { snprintf( porque, np, "no está la sección «%s»", ARMA_ETIQ[k] ); return 1; }
       lab[k] = i++;
       if ( i >= f->n ) { snprintf( porque, np, "falta la cuenta de «%s»", ARMA_ETIQ[k] ); return 1; }
       nt = tokens( f->l[i], t, 64 );
       nf = nt > 0 ? atoi( t[0] ) : -1;
       if ( nf < 0 || nt < nf + 1 )
          { snprintf( porque, np, "cuenta ilegible en «%s»", ARMA_ETIQ[k] ); return 1; }
       i++;
       for ( j = 0; j < nf; j++ )
           {
           int oj = atoi( t[j + 1] );
           o += oj;
           i += 1 + oj;                 /* the "**" line and the coefficients */
           }
       if ( i > f->n ) { snprintf( porque, np, "el fichero se acaba en «%s»", ARMA_ETIQ[k] ); return 1; }
       ord[k] = o;
       }
   *fin = i;
   return 0;
}

int id_arma_ordenes( const char *origen, int *p, int *q, int *P, int *Q,
                     char *porque, size_t n )
{
   Fich f;
   int  lab[4], fin, ord[4];

   if ( fich_lee( origen, &f, porque, n ) ) return 1;
   if ( arma_mira( &f, lab, &fin, ord, porque, n ) ) { fich_free( &f ); return 1; }
   *p = ord[0]; *P = ord[1]; *q = ord[2]; *Q = ord[3];
   fich_free( &f );
   return 0;
}

/* One section: its label as the file has it; then the count line -- the
 * file's own, byte for byte, when an empty section stays empty -- and the
 * coefficients. */
static void arma_seccion( FILE *o, const char *etiqueta, const char *cuenta_vieja,
                          int vacia, int ord, const double *v )
{
   int j;

   fputs( etiqueta, o );
   if ( ord <= 0 ) { fputs( vacia ? cuenta_vieja : "0\n", o ); return; }
   fprintf( o, "1 %d\n**\n", ord );
   for ( j = 0; j < ord; j++ ) fprintf( o, "%.6f  1\n", v ? v[j] : 0.0 );
}

int id_pon_arma( const char *origen, const char *destino,
                 int p, const double *phi, int q, const double *theta,
                 int P, const double *Phi, int Q, const double *Theta,
                 char *porque, size_t n )
{
   Fich  f;
   int   lab[4], fin, ord[4], i;
   FILE *o;

   if ( p < 0 || q < 0 || P < 0 || Q < 0 )
      { snprintf( porque, n, "órdenes negativos" ); return 1; }
   if ( fich_lee( origen, &f, porque, n ) ) return 1;
   if ( arma_mira( &f, lab, &fin, ord, porque, n ) ) { fich_free( &f ); return 1; }

   o = fopen( destino, "wb" );
   if ( !o ) { snprintf( porque, n, "no pude escribir «%s»", destino ); fich_free( &f ); return 1; }
   for ( i = 0; i < lab[0]; i++ ) fputs( f.l[i], o );
   arma_seccion( o, f.l[lab[0]], f.l[lab[0] + 1], ord[0] == 0, p, phi );
   arma_seccion( o, f.l[lab[1]], f.l[lab[1] + 1], ord[1] == 0, P, Phi );
   arma_seccion( o, f.l[lab[2]], f.l[lab[2] + 1], ord[2] == 0, q, theta );
   arma_seccion( o, f.l[lab[3]], f.l[lab[3] + 1], ord[3] == 0, Q, Theta );
   for ( i = fin; i < f.n; i++ ) fputs( f.l[i], o );
   fclose( o );
   fich_free( &f );
   return 0;
}

int id_pon_transformacion( const char *origen, const char *destino,
                           double lam, int d, int D, char *porque, size_t n )
{
   Fich  f;
   FILE *o;
   int   i, lab = -1;

   if ( d < 0 || D < 0 ) { snprintf( porque, n, "diferencias negativas" ); return 1; }
   if ( fich_lee( origen, &f, porque, n ) ) return 1;
   for ( i = 0; i < f.n; i++ )
       if ( es_etiqueta( f.l[i] ) && strstr( f.l[i], "Box-Cox lambda" ) ) { lab = i; break; }
   if ( lab < 0 || lab + 1 >= f.n )
      { snprintf( porque, n, "no está la línea de Box-Cox" ); fich_free( &f ); return 1; }
   o = fopen( destino, "wb" );
   if ( !o ) { snprintf( porque, n, "no pude escribir «%s»", destino ); fich_free( &f ); return 1; }
   for ( i = 0; i < f.n; i++ )
       if ( i == lab + 1 ) fprintf( o, "%.2f %d %d\n", lam, d, D );
       else fputs( f.l[i], o );
   fclose( o );
   fich_free( &f );
   return 0;
}
