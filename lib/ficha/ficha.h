/*
 * ficha.h -- la ficha de confianza de un modelo (docs/ESTUDIO-terminal-mcp.md
 * §7.8): lo que hace falta para fiarse de el, o para no fiarse.
 *
 * NO SE ESCRIBE A MANO Y NO SE GUARDA. Sale de lo que ya existe --el linaje
 * del manifiesto, el .out, el registro de costes-- y se calcula cada vez, asi
 * que nunca se queda vieja. Cuatro bloques:
 *
 *   1. EL RECORRIDO, desde los datos hasta el modelo, con quien dio cada paso
 *      (§7.2) y su razon, y la parte del asistente. Un paso sin razon se ve
 *      como tal.
 *   2. LA ADECUACION, de lib/dictamen sobre el .out: la misma llamada que la
 *      ventana de la madre y que «atsw-cli dictamen».
 *   3. LA REPRODUCIBILIDAD: la ultima comprobacion de ESE .pre (su sha256)
 *      en costes.csv. Comprobar cuesta una corrida, asi que se hace al
 *      pedirlo (fi_comprueba) y la ficha solo la lee.
 *   4. EL COSTE: el de la estimacion y el de todo el recorrido.
 *
 * LO QUE NO SE HIZO DICE «NO CONSTA», NUNCA «BIEN». Es la regla de la huella
 * vacia del guion, la de DX_NO_CONSTA y la de «sin razon».
 *
 * UNA SOLA FUNCION ESCRIBE EL MARKDOWN (fi_markdown), para que la madre,
 * atsw-cli y el MTM den los mismos bytes. Lo que no es el Markdown --el JSON
 * de atsw-cli-- se hace con la estructura, que lleva los mismos datos.
 *
 * Solo GLib: la usa atsw-cli, que no tiene GTK.
 */

#ifndef ATSW_FICHA_H
#define ATSW_FICHA_H

#include <stddef.h>
#include <glib.h>

#include "proyecto.h"
#include "dictamen.h"

#define FI_MAX_PASO   64
#define FI_MOTOR      64
#define FI_METODO    128

/* Un nodo del recorrido. */
typedef struct {
   char id[PR_ID];
   char por[PR_ID];             /* "analista" o "asistente"; nunca ""     */
   char razon[PR_RAZON];        /* "" = sin razon, y se dice              */
   int  datos;                  /* el nodo de datos: no es una decision   */
} FiPaso;

/* Una corrida de costes.csv, la que haga al caso. */
typedef struct {
   int    consta;
   char   fecha[32];            /* como en el registro: AAAA-MM-DDTHH:MM:SS */
   char   motor[FI_MOTOR];
   char   via[16];
   double cpu_s, pared_s;
   double energia_j;            /* < 0: sin energia                       */
   char   metodo[FI_METODO];
} FiCorrida;

typedef struct {
   char serie[PR_ID], muestra[PR_ID], id[PR_ID];

   /* 1. EL RECORRIDO, de los datos (paso[0]) al modelo (paso[npaso-1]).
    * pasos son las DECISIONES: los nodos que no son de datos. La eleccion
    * no cuenta como paso: se dice aparte.                               */
   FiPaso paso[FI_MAX_PASO];
   int    npaso;
   int    pasos, del_asistente;
   int    elegido;
   char   por_elegido[PR_ID];   /* "analista" o "asistente"; "" si no es  */

   /* 2. LA ADECUACION. estimado = hay un .out que se pudo leer. */
   int      estimado;
   Dictamen dx;

   /* 3. LA REPRODUCIBILIDAD. sha_pre "" si no hay .pre. */
   char      sha_pre[PR_SHA];
   FiCorrida rep;               /* la ultima comprobacion de ESE sha      */
   int       reproduce;         /* lo que dijo, si consta                 */
   FiCorrida origen;            /* la corrida que dejo ese .pre, si consta */

   /* 4. EL COSTE: la ultima estimacion del modelo y todo el recorrido. */
   FiCorrida estimacion;
   int       corridas;          /* las de todos los modelos del recorrido */
   double    cpu_s, pared_s, energia_j;
   int       con_energia;       /* cuantas traian energia                 */
   int       en_python;         /* cuantas fueron por el extra Python     */
} Ficha;

/* LA FICHA DE UN MODELO. Lee el .out, el .pre y costes.csv; no escribe nada.
 * f es grande (el Dictamen y los pasos): con g_new0 o static. 0 si pudo; si
 * no, 1 con el porque (un modelo que no esta, un linaje roto).          */
int fi_ficha( const Proyecto *p, const char *serie, const char *muestra,
              const char *id, Ficha *f, char *why, size_t n );

/* El Markdown, siempre el mismo para la misma ficha (g_free). Los numeros
 * con coma decimal y las fechas como «21-oct-2026».                     */
gchar *fi_markdown( const Ficha *f );

/* EL DICTAMEN DE UN MODELO, con los nombres de sus deterministas sacados del
 * .pre (o del .inp): lo que hacen la ventana y «atsw-cli dictamen». 0 si
 * habia .out; 1 si no lo hay o no se lee.                              */
int fi_dictamen( const Proyecto *p, const char *serie, const char *muestra,
                 const char *id, Dictamen *d );

/* --- la comprobacion de reproducibilidad --------------------------------- */

/* DOS .pre, TOKEN A TOKEN: los que no son numeros, iguales; los numeros,
 * dentro de la tolerancia (ver ficha.c, FI_TOL). 1 si son el mismo optimo;
 * 0 si no, y en dif (si no es NULL) el primer token que se movio; -1 si no
 * se pudo leer alguno.                                                  */
int fi_compara_pre( const char *a, const char *b, char *dif, size_t n );

/* CORRE EL MOTOR SOBRE EL .pre DEL MODELO Y LO APUNTA. Copia el .pre a un
 * directorio temporal como <base>.inp, corre fue con lib/engine (el que
 * llama ha dicho antes donde esta, con engine_set_finder) y compara el .pre
 * que sale con el original. En costes.csv queda una linea con paso
 * «reproducir:si» o «reproducir:no», el sha del .pre ORIGINAL y lo que costo,
 * medido como cualquier otra corrida. quien y por, como en coste_contexto.
 *
 * 0 si se pudo comprobar, con *reproduce 1 o 0 y, si no se reproduce, el
 * primer token que se movio en dif; 1 si no se pudo (no hay .pre, el motor
 * no corre...), con el porque en dif, y entonces no se apunta nada.     */
int fi_comprueba( const Proyecto *p, const char *serie, const char *muestra,
                  const char *id, const char *quien, const char *por,
                  int *reproduce, char *dif, size_t n );

#endif /* ATSW_FICHA_H */
