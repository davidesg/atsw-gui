/*
 * netfile.h -- el .dag: la red de transferencias.
 *
 * Una linea por ENLACE:
 *
 *     SALIDA <- ENTRADA   b r s        # lo que siga a # es comentario
 *
 * donde SALIDA y ENTRADA son el nombre de la serie --el del .pre-- o su
 * posicion en la linea de ordenes (1 = el primer fichero).
 *
 * Esto NO es codigo nuevo: read_network y topo_sort estaban dentro de
 * drtran.c, atados a los globales n_ser/n_link/lnk/Ts/topo. Aqui reciben sus
 * argumentos y nada mas. El motor sigue llamando a estas mismas funciones, asi
 * que el GUI y el motor no pueden discrepar sobre lo que es un .dag valido.
 *
 * POR QUE IMPORTA EL CICLO. La red no es adorno: el motor resuelve el sistema
 * por RECURSION en orden topologico -- una serie solo se puede construir
 * despues de todas las que la alimentan. Si hay un ciclo, el sistema es
 * SIMULTANEO y no se puede triangularizar restando transferencias. Eso no es
 * un error de sintaxis: es un modelo que no es de este peldaño. El que toca
 * entonces es el VARMA simultaneo, drvarma.
 *
 * Es el mismo veredicto que da la CCF bidireccional cuando los retardos
 * negativos salen de la banda, dicho sobre la red entera en vez de sobre un
 * enlace.
 */

#ifndef ATSW_NETFILE_H
#define ATSW_NETFILE_H

#include <stddef.h>

#define NET_MAX_SER   64
#define NET_MAX_LINK 256

/* Un enlace. out e inp son indices de serie, 1..nser. */
typedef struct {
   int out, inp;
   int b, r, s;
} NetLink;

/* Lo que puede ir mal, como HECHO y no como frase.
 *
 * La libreria no elige idioma. El motor tiene declarado que su salida va en
 * ingles --es una propiedad del puerto, y la bateria la comprueba al pie de la
 * letra-- y el GUI habla en espanol. Los dos leen el mismo .dag y tienen que
 * dar el mismo veredicto; lo unico que cambia es quien lo cuenta y como.  */
typedef enum {
   NET_OK = 0,
   NET_ENOFILE,      /* no se puede abrir                                  */
   NET_ESYNTAX,      /* la linea no tiene la forma SALIDA <- ENTRADA b r s */
   NET_EARROW,       /* donde iba "<-" hay otra cosa (en token)            */
   NET_EUNKNOWN,     /* no hay ninguna serie que se llame asi (en token)   */
   NET_ESELF,        /* una serie alimentandose a si misma (en token)      */
   NET_ENEG,         /* b, r o s negativos                                 */
   NET_EMANY         /* mas enlaces de los que caben                       */
} NetErr;

typedef struct {
   NetErr err;
   int    line;            /* numero de linea, 1..n; 0 si no aplica       */
   char   token[64];       /* la palabra que causo el fallo, si la hay    */
   int    b, r, s;         /* los ordenes, para NET_ENEG                  */
} NetError;

/* La frase del MOTOR, en ingles y con su redaccion de siempre. Escribe en
 * out[size] y devuelve out.                                             */
const char *net_error_en( const NetError *e, char *out, size_t size );

/* El indice de la serie que nombra tok: su nombre (sin distinguir mayusculas)
 * o su posicion en decimal. 0 si no la hay. nombre[1..nser].            */
int net_series_index( const char * const *nombre, int nser, const char *tok );

/* Lee un .dag. Devuelve cuantos enlaces leyo, o -1 y llena *e con el hecho.
 * Las lineas en blanco y los comentarios se saltan. e puede ser NULL.   */
int net_read( const char *path, const char * const *nombre, int nser,
              NetLink *lnk, int max, NetError *e );

/* Escribe un .dag. Si cabecera no es NULL se escribe como comentario
 * arriba. Devuelve 0 si pudo.                                           */
int net_write( const char *path, const char * const *nombre,
               const NetLink *lnk, int nlinks, const char *cabecera );

/* El orden topologico, topo[1..nser]. Devuelve 1 si la red es acíclica, 0 si
 * hay ciclo. Es la misma cuenta que hace el motor.                       */
int net_topo( const NetLink *lnk, int nlinks, int nser, int *topo );

/* Si hay ciclo, UNO concreto, en ciclo[0..*n-1] como indices de serie y
 * cerrado (el primero se repite al final). Devuelve 1 si lo encontro.
 * El motor solo dice que hay ciclo; el GUI puede decir CUAL, y por eso esto
 * esta aqui y no alli: no cambia ningun veredicto, añade el detalle.     */
int net_cycle( const NetLink *lnk, int nlinks, int nser, int *ciclo, int *n );

/* Cuantos enlaces entran en la serie i, y cuantos salen de ella. */
int net_indegree( const NetLink *lnk, int nlinks, int i );
int net_outdegree( const NetLink *lnk, int nlinks, int i );

#endif /* ATSW_NETFILE_H */
