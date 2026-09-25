/*
 * dictamen.c -- los cinco bloques.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "dictamen.h"

static DxLinea *nueva( Dictamen *d, const char *titulo )
{
   DxLinea *l;

   if ( d->n >= DX_MAX_LINEA ) return NULL;
   l = &d->l[d->n++];
   memset( l, 0, sizeof *l );
   snprintf( l->titulo, DX_TITULO, "%s", titulo );
   l->estado = DX_NO_CONSTA;
   snprintf( l->dice, DX_DICE, "El informe no lo trae." );
   return l;
}

/* EL RESUMEN NO ES UN MAXIMO, y por eso no se escribe como tal.
 *
 * El orden del enum es de GRAVEDAD --no consta, cuadra, mirar, no-- y tomar
 * el maximo hacia que «cuadra» se tragara a «no consta»: dos bloques que el
 * informe no traia y tres que pasaban daban un resumen de «cuadra». Eso es
 * exactamente la mentira que este modulo existe para no contar.
 *
 * Son dos ejes distintos --lo grave y lo que se sabe-- asi que la precedencia
 * se escribe entera: CUADRA sale solo si TODOS cuadran.                */
static void resume( Dictamen *d )
{
   int i, hay_no = 0, hay_mirar = 0, hay_sin = 0;

   for ( i = 0; i < d->n; i++ )
       switch ( d->l[i].estado )
           {
           case DX_NO:        hay_no = 1;    break;
           case DX_MIRAR:     hay_mirar = 1; break;
           case DX_NO_CONSTA: hay_sin = 1;   break;
           default: break;
           }

   d->peor = hay_no    ? DX_NO
           : hay_mirar ? DX_MIRAR
           : hay_sin   ? DX_NO_CONSTA
           : d->n      ? DX_CUADRA
                       : DX_NO_CONSTA;
}

/* Un p contra los dos umbrales. */
static DxEstado por_p( double p )
{
   if ( p < DX_ALFA )     return DX_NO;
   if ( p < DX_ALFA_OJO ) return DX_MIRAR;
   return DX_CUADRA;
}


/* ------------------------------------------------------------------------ */
/* 1. La estimación                                                          */
/* ------------------------------------------------------------------------ */

static void bloque_estimacion( const Convergence *c, Dictamen *d )
{
   DxLinea *l = nueva( d, "Estimación" );

   if ( l == NULL || c == NULL || c->kind == CONV_NONE ) return;

   snprintf( l->dato, DX_DATO, "%s%s", c->brief ? c->brief : "",
             c->iterations >= 0 ? "" : "" );
   if ( c->iterations >= 0 )
       snprintf( l->dato, DX_DATO, "%s, %d iteraciones",
                 c->brief ? c->brief : "", c->iterations );

   switch ( c->kind )
       {
       case CONV_GRADTOL:
       case CONV_STEPTOL:
           l->estado = DX_CUADRA;
           snprintf( l->dice, DX_DICE, "Convergió." );
           break;
       case CONV_NO_LOWER:
           /* «Parado en un punto sin mejora» SUENA a fracaso y es lo que
              sale cuando el .pre YA ERA el óptimo -- que es justamente el
              invariante del contrato. Confundirlo con un fallo sería
              confundir el éxito con el fracaso.                        */
           l->estado = DX_CUADRA;
           snprintf( l->dice, DX_DICE, "Se paró sin encontrar nada mejor. Es "
                     "lo normal si se arrancó EN el óptimo." );
           break;
       case CONV_MAXITS:
           l->estado = DX_NO;
           snprintf( l->dice, DX_DICE, "NO convergió: se acabaron las "
                     "iteraciones. Lo estimado no es un óptimo." );
           break;
       case CONV_MAXSTEPS:
           l->estado = DX_NO;
           snprintf( l->dice, DX_DICE, "NO convergió: cinco pasos seguidos de "
                     "longitud máxima. Lo estimado no es un óptimo." );
           break;
       default:
           break;
       }
}


/* ------------------------------------------------------------------------ */
/* 2. La media de los residuos                                               */
/* ------------------------------------------------------------------------ */

static void bloque_media( const FueOut *o, Dictamen *d )
{
   DxLinea *l = nueva( d, "Media" );
   double   t;

   if ( l == NULL || !o->tiene_res ) return;

   /* LA TRAMPA: con mu estimada, la media de los residuos es CERO POR
      CONSTRUCCIÓN y el contraste no dice nada. Presentar t = -0.006 como
      un aprobado brillante sería presentar una tautología como
      evidencia.                                                        */
   if ( o->tiene_mu )
       {
       snprintf( l->dato, DX_DATO, "%.6f", o->media );
       snprintf( l->dice, DX_DICE, "El modelo estima la media, así que ésta "
                 "es cero por construcción: aquí no hay contraste." );
       l->estado = DX_NO_CONSTA;
       return;
       }

   if ( o->media_et <= 0.0 )
       { snprintf( l->dato, DX_DATO, "%.6f", o->media ); return; }

   t = o->media / o->media_et;
   snprintf( l->dato, DX_DATO, "%.6f (e.t. %.6f), t = %.2f",
             o->media, o->media_et, t );

   if ( fabs( t ) >= DX_T_FLOJO )
       { l->estado = DX_NO;
         snprintf( l->dice, DX_DICE, "La media NO es cero: al modelo le falta "
                   "un término constante, o algo sistemático." ); }
   else
       { l->estado = DX_CUADRA;
         snprintf( l->dice, DX_DICE, "La media no se distingue de cero." ); }
}


/* ------------------------------------------------------------------------ */
/* 3. La autocorrelación -- la escalera entera                               */
/* ------------------------------------------------------------------------ */

static void bloque_autocorrelacion( const FueOut *o, Dictamen *d )
{
   DxLinea *l = nueva( d, "Autocorrelación" );
   int      i, peor_i = -1;
   char    *p;
   size_t   n;

   if ( l == NULL || o->nlb < 1 ) return;

   /* LA ESCALERA ENTERA, porque el sitio donde falla ES el diagnóstico:
      Q(12) mal y Q(36) bien es un problema CERCA --casi siempre estacional
      o de forma-- y al revés es arrastre lejano. Dos cosas distintas.  */
   p = l->dato;
   n = DX_DATO;
   for ( i = 0; i < o->nlb; i++ )
       {
       int esc = snprintf( p, n, "%sQ(%d)=%.1f p=%.3f", i ? "  " : "",
                           o->lb_df_[i], o->lb_q_[i], o->lb_p_[i] );

       if ( esc < 0 || (size_t) esc >= n ) break;
       p += esc; n -= (size_t) esc;
       if ( peor_i < 0 || o->lb_p_[i] < o->lb_p_[peor_i] ) peor_i = i;
       }

   l->estado = por_p( o->lb_p_[peor_i] );
   if ( l->estado == DX_CUADRA )
       snprintf( l->dice, DX_DICE, "Los residuos son blancos en toda la "
                 "ventana." );
   else
       snprintf( l->dice, DX_DICE, "Los residuos NO son blancos, y donde peor "
                 "es en el retardo %d (p = %.3f).",
                 o->lb_df_[peor_i], o->lb_p_[peor_i] );
}


/* ------------------------------------------------------------------------ */
/* 4. La normalidad                                                          */
/* ------------------------------------------------------------------------ */

static void bloque_normalidad( const FueOut *o, Dictamen *d )
{
   DxLinea *l = nueva( d, "Normalidad" );

   if ( l == NULL || !o->tiene_jb ) return;

   snprintf( l->dato, DX_DATO,
             "JB = %.2f (2 g.l.) p = %.3f · asimetría %.2f, curtosis %.2f%s",
             o->jb, o->jb_p, o->skew, o->kurt,
             o->tiene_hist ? "" : "" );

   if ( o->tiene_hist )
       {
       char b[64];

       snprintf( b, sizeof b, " · fuera de ±2: %.1f%% (esperado %.1f%%)",
                 o->fuera2, o->esp2 );
       strncat( l->dato, b, DX_DATO - strlen( l->dato ) - 1 );
       }

   l->estado = por_p( o->jb_p );
   if ( l->estado == DX_CUADRA )
       snprintf( l->dice, DX_DICE, "No se rechaza la normalidad." );
   else
       snprintf( l->dice, DX_DICE, "Los residuos NO son normales. Con "
                 "curtosis %.1f suele ser cosa de unos pocos atípicos, no de "
                 "toda la muestra.", o->kurt );
}


/* ------------------------------------------------------------------------ */
/* 5. Los parámetros                                                         */
/* ------------------------------------------------------------------------ */

static void bloque_parametros( const FueOut *o, Dictamen *d )
{
   DxLinea *l = nueva( d, "Parámetros" );
   int      i, flojos = 0, prim = -1;

   if ( l == NULL || o->npar_leidos < 1 ) return;

   for ( i = 0; i < o->npar_leidos; i++ )
       if ( o->par_estimado[i] && o->par_et[i] > 0.0 &&
            fabs( o->par[i] / o->par_et[i] ) < DX_T_FLOJO )
           { flojos++; if ( prim < 0 ) prim = i + 1; }

   snprintf( l->dato, DX_DATO, "%d estimados · %d con |t| < %.0f · %d "
             "pares con |r| ≥ %.1f", o->npar_leidos, flojos, DX_T_FLOJO,
             o->npares, DX_R_ALTO );

   /* LA SEGUNDA TRAMPA: una correlación alta entre un AR y un MA puede ser
      ESTRUCTURAL --una transferencia racional, un AR(2) con phi2 < 0-- y no
      un exceso de parámetros. Así que se enuncia el hecho y no se dicta la
      sentencia: decidir si sobra es del analista.                      */
   if ( o->npares > 0 )
       {
       l->estado = DX_MIRAR;
       snprintf( l->dice, DX_DICE, "Los parámetros [%d] y [%d] van juntos "
                 "(r = %.2f): puede sobrar uno, o puede ser la forma del "
                 "modelo.", o->par_a[0], o->par_b[0], o->par_r[0] );
       }
   else if ( flojos > 0 )
       {
       l->estado = DX_MIRAR;
       snprintf( l->dice, DX_DICE, "%d parámetro%s no se gana%s su sitio, "
                 "empezando por el [%d].", flojos, flojos == 1 ? "" : "s",
                 flojos == 1 ? "" : "n", prim );
       }
   else
       {
       l->estado = DX_CUADRA;
       snprintf( l->dice, DX_DICE, "Todos se ganan su sitio y ninguno se pisa "
                 "con otro." );
       }
}


/* ------------------------------------------------------------------------ */

void dx_dictamen( const FueOut *o, const Convergence *conv, Dictamen *d )
{
   if ( d == NULL ) return;
   memset( d, 0, sizeof *d );
   if ( o == NULL || !o->hay ) return;

   bloque_estimacion( conv, d );
   bloque_media( o, d );
   bloque_autocorrelacion( o, d );
   bloque_normalidad( o, d );
   bloque_parametros( o, d );
   resume( d );
}

const char *dx_estado_es( DxEstado e )
{
   switch ( e )
       {
       case DX_CUADRA: return "cuadra";
       case DX_MIRAR:  return "mirar";
       case DX_NO:     return "NO";
       default:        return "no consta";
       }
}
