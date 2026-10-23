/* coste.h -- lo que cuesta cada corrida de un motor, en costes.csv.
 *
 * CADA CORRIDA DEJA UNA LINEA en <raiz del proyecto>/costes.csv: cuando,
 * quien la lanzo, para que, con que motor, cuanto tiempo de CPU y de reloj, y
 * la energia ESTIMADA con el metodo escrito al lado
 * (docs/ESTUDIO-terminal-mcp.md §7.7). Es un registro de solo añadir: una
 * linea por corrida, la cabecera al crearlo, y se lee igual con una hoja de
 * calculo, con R o con Python.
 *
 * LA ESCRIBEN TRES PROGRAMAS CON EL MISMO CODIGO: la madre (y las ventanas
 * que lanza), por lib/engine; atsw-cli, por las mismas funciones; y el MTM,
 * que lo que corre en Python lo apunta con «atsw-cli coste apunta», que
 * acaba en coste_apunta. Un solo escritor de un solo formato.
 *
 * SIN CONTEXTO NO SE ESCRIBE NADA. lib/engine solo apunta una corrida si el
 * que la lanza dijo antes de que proyecto es y para que (coste_contexto).
 * Las baterias y un fue_gui suelto no crean ficheros que nadie pidio.
 *
 *   campo      ejemplo                      que es
 *   fecha      2026-10-21T10:14:03          cuando (hora local)
 *   quien      madre | atsw-cli | mtm       que programa la lanzo
 *   por        analista | asistente         quien lo pidio
 *   objeto     IPC/m04, C1/c02              el modelo o la corrida (o vacio)
 *   paso       estimar, meg, contraste,     para que
 *              prever, reproducir...
 *   motor      fue 1.14                     el programa y su version
 *   via        C | python                   el binario o el extra Python
 *   cpu_s      0.210                        CPU del proceso (usuario+sistema)
 *   pared_s    0.340                        tiempo de reloj
 *   energia_j  0.394                        la energia ESTIMADA, en julios
 *   metodo     cpu×TDP/núcleos: 15 W, 8     como se estimo
 *   sha_pre    3f2a9c...                    sha256 del .pre resultante
 *
 * Los numeros van SIEMPRE con punto, sea cual sea el idioma del sistema.
 */

#ifndef COSTE_H
#define COSTE_H

#include <stddef.h>
#include <glib.h>

#define COSTE_FICHERO "costes.csv"

/* Una linea del registro. Para escribir (coste_apunta) los punteros pueden
 * ser NULL, que se escribe vacio; al leer (coste_lee) todos son cadenas
 * propias, nunca NULL, y se liberan con coste_lineas_libera.            */
typedef struct {
    char   *fecha, *quien, *por, *objeto, *paso, *motor, *via;
    double  cpu_s, pared_s;
    double  energia_j;           /* < 0: sin energia (vacio en el fichero) */
    char   *metodo, *sha_pre;
} CosteLinea;

/* --- el contexto: lo que lib/engine apunta con cada corrida --------------- */

/* DE QUE PROYECTO Y PARA QUE es lo que se va a lanzar. raiz es el directorio
 * del proyecto (donde va costes.csv); sin raiz no se apunta nada. quien y
 * por, si son NULL, valen «madre» y «analista». objeto y paso pueden ir
 * vacios. Se copia: el que llama puede liberar lo suyo.
 *
 * lib/engine se queda con una COPIA al lanzar cada motor, asi que el contexto
 * se puede limpiar justo despues de lanzar, aunque la corrida sea asincrona,
 * y eso es lo que conviene: un contexto que se queda puesto se pega a la
 * siguiente corrida, que puede ser de otro modelo.                       */
void coste_contexto( const char *raiz, const char *quien, const char *por,
                     const char *objeto, const char *paso );
void coste_contexto_limpia( void );

/* Lo mismo para un modelo del proyecto: objeto es «serie/id», o
 * «serie/muestra/id» en una submuestra (m01 de la completa y m01 de
 * pre-covid son modelos distintos). quien «madre», por «analista».      */
void coste_contexto_modelo( const char *raiz, const char *serie,
                            const char *muestra, const char *id,
                            const char *paso );

/* --- lo que usa lib/engine --------------------------------------------- */

/* Una corrida en marcha, medida desde fuera. NULL si no hay contexto: todas
 * las funciones de abajo aceptan NULL y no hacen nada.                  */
typedef struct CosteCorrida CosteCorrida;

/* Al lanzar: copia el contexto y pone el reloj en marcha. argv es el de la
 * orden SIN el programa (para buscar el .pre resultante).               */
CosteCorrida *coste_empieza( const char *workdir, const char *programa,
                             const char *const *argv );

/* Cuando el hijo ha terminado. En Windows, pid es el manejador del proceso
 * y se llama ANTES de cerrarlo (GetProcessTimes); en Unix, despues de que
 * GLib lo haya recogido (la diferencia de getrusage(RUSAGE_CHILDREN)).   */
void coste_hijo_acabo( CosteCorrida *c, GPid pid );

/* Al final, con lo que el motor dijo (para su version) y si dejo
 * resultados (para el .pre): escribe la linea y libera c.               */
void coste_termina( CosteCorrida *c, gboolean escribio, const char *salida );

/* No se pudo lanzar: no hubo corrida, no hay linea. */
void coste_descarta( CosteCorrida *c );

/* --- para los escritores de fuera (atsw-cli coste apunta) ---------------- */

/* Añade una linea a <raiz>/costes.csv, con la cabecera si el fichero es
 * nuevo, bajo el cerrojo del fichero y de una sola escritura. fecha NULL es
 * ahora. Con energia_j < 0 y metodo NULL o vacio, la energia se ESTIMA con
 * el metodo de siempre (coste_metodo); una energia >= 0 sin metodo se
 * rechaza: una energia sin su metodo no se escribe. 0 si pudo; si no, 1 y
 * el porque en why.                                                      */
int coste_apunta( const char *raiz, const CosteLinea *l, char *why, size_t n );

/* La estimacion por defecto: E = cpu_s × TDP / nucleos logicos. El TDP es
 * 15 W (un portatil) o lo que diga ATSW_TDP_W. Devuelve la energia y, si
 * metodo no es NULL, escribe en el el metodo («cpu×TDP/núcleos: 15 W, 8»).*/
double coste_energia( double cpu_s, char *metodo, size_t n );

/* --- leer y sumar (atsw-cli coste resumen) ------------------------------ */

/* Todas las lineas de <raiz>/costes.csv, en orden. Sin fichero, 0 lineas y
 * 0 (no es un error: no ha corrido nada). Una linea que no se entiende se
 * salta y se cuenta en *malas (si no es NULL). 0 si pudo leer.           */
int  coste_lee( const char *raiz, CosteLinea **lineas, int *n, int *malas,
                char *why, size_t nwhy );
void coste_lineas_libera( CosteLinea *lineas, int n );

/* Los totales de un grupo. energia_j suma solo las lineas que la traen. */
typedef struct {
    char   *clave;
    int     corridas;
    double  cpu_s, pared_s, energia_j;
} CosteTotal;

/* Suma el registro agrupando por_que: «serie» (lo de antes de la «/» del
 * objeto), «objeto», «motor» (el programa, sin la version), «via», «por»,
 * «paso» o «quien»; NULL o "" es un solo total, con clave «total». Los grupos
 * salen en el orden en que aparecen. Las claves vacias se llaman «(sin
 * objeto)», «(sin paso)»... 0 si pudo; 1 con el porque (un por_que
 * desconocido, un fichero que no se lee).                               */
int  coste_resumen( const char *raiz, const char *por_que, CosteTotal **t,
                    int *n, char *why, size_t nwhy );
void coste_totales_libera( CosteTotal *t, int n );

#endif /* COSTE_H */
