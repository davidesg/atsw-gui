/*
 * proyecto.h -- el proyecto: que series, que modelos, y de donde viene cada uno.
 *
 * Es la entidad que el estudio declaro AUSENTE (DISENO-proyecto.md): no existe
 * ni en el lado C ni en el Python. El agrupador de facto es el DIRECTORIO.
 *
 * TRES COSAS, Y NINGUNA ES DE INTERFAZ:
 *
 *   1. NOMBRES -> RUTAS contra una raiz declarada. El lado C ya opera con
 *      nombres cortos y por eso mover un caso no lo rompe (INVENTARIO-madre
 *      §3): eso se conserva, no se tira.
 *
 *   2. NOMBRAR LAS CORRIDAS. Hoy drtran_gui escribe siempre modelo.out, red.dag
 *      y modelo.cns, asi que NO CABEN DOS MODELOS. Es la ranura unica RESIDUOS
 *      de TASTE (TASTECTV.PAS:475) otra vez.
 *
 *   3. LA CADENA DE ITERACION, que es lo que hoy no registra nadie:
 *
 *          .inp(-1) --estimar--> .pre(-1) --copiar--> .inp(0) --estimar--> ...
 *
 *      .pre e .inp son el MISMO formato --verificado: se copia un .pre a
 *      Z.inp, se corre fue y sale Z.pre-- pero el motor EXIGE la extension
 *      .inp, asi que el paso es una COPIA FISICA REAL.
 *
 * EL NOMBRE DEL FICHERO ES CORTESIA. Decision del analista, 2026-09-20.
 *
 *     IPC_ES_m03.inp      se llama asi para que el directorio se entienda a
 *                         ojo, Y NINGUN PROGRAMA LO LEE.
 *
 * La identidad esta en el manifiesto: el id es una CLAVE, y la version y el
 * padre son CAMPOS. Es la conclusion de DISENO-proyecto.md §2, donde se
 * demostro que con todos los ficheros delante la convencion _mNN se deduce MAL
 * -- en el mismo arbol _m01 significa dos cosas distintas para la misma serie.
 *
 *     Si hay que parsear el nombre para saber algo, ese algo no esta
 *     registrado.
 *
 * EL LINAJE ES OBLIGATORIO; LA RAZON, NO. Decision del analista: "deberia
 * registrar el porque de cada iteracion, pero el linaje es lo minimo que se
 * deberia mantener." Son dos exigencias de rango distinto:
 *
 *   linaje   AUTOMATICO. pr_deriva() lo escribe al copiar, sin preguntar:
 *            el programa ya sabe de que .pre sale este .inp.
 *   razon    SE PIDE, no se exige, y se puede poner DESPUES -- mirando el
 *            .out, que es cuando de verdad se sabe.
 *
 * Y "SIN RAZON" SE VE COMO SIN RAZON. Nunca se infiere una ni se pone relleno.
 * En art, guion_node RECHAZA la llamada sin razon y funciona (881 de 924 nodos
 * la llevan), pero eso es un LLM escribiendo: un modal "¿por que?" en cada
 * estimacion se contesta "asdf" a la tercera, y UNA RAZON FALSA ES PEOR QUE
 * NINGUNA porque no se distingue de una de verdad. Es la regla de la huella
 * vacia del guion: "no consta" nunca significa "cuadra".
 *
 * NO ES EL guion.json, Y NO TIENE QUE SERLO. Son las dos encarnaciones del
 * taller (DISENO-madre.md §10): ATSW Python gestiona proyectos con guion.json,
 * registro y policy; ATSW GUI, con esto. No convergen, y forzarlo haria peor a
 * las dos.
 *
 * Sin dependencias, como lib/rutas, lib/tabla y lib/datos.
 */

#ifndef ATSW_PROYECTO_H
#define ATSW_PROYECTO_H

#include <stddef.h>

#define PR_MAX_SERIE   64
#define PR_MAX_MODELO  512
#define PR_ID          48
#define PR_TEXTO       256
#define PR_RAZON       512
#define PR_RUTA        1024

/* Que paso. La biblioteca da el HECHO; cada front end lo redacta -- la
 * leccion que costo una prueba de la bateria en lib/netfile.            */
typedef enum {
   PR_OK = 0,
   PR_ENOFILE,       /* no se pudo abrir                                  */
   PR_ESINTAXIS,     /* una linea que no se entiende                      */
   PR_ECLAVE,        /* una clave desconocida en ese nivel                */
   PR_EMUCHAS,       /* no caben mas series o modelos                     */
   PR_EDUP,          /* una serie o un modelo repetido                    */
   PR_EPADRE,        /* el padre no existe                                */
   PR_ECICLO,        /* el linaje se muerde la cola                       */
   PR_ENOSERIE,      /* esa serie no esta en el proyecto                  */
   PR_ENOMODELO,     /* ese modelo no esta                                */
   PR_EESCRIBIR      /* no se pudo escribir                               */
} PrCodigo;

typedef struct {
   PrCodigo cod;
   int      linea;                /* 1..n del fichero; 0 si no aplica     */
   char     texto[PR_TEXTO];      /* la clave, el nombre, lo que sea      */
} PrError;

/* Un modelo: UNA iteracion de la cadena. */
typedef struct {
   char id[PR_ID];                /* la CLAVE. "m03". No se parsea.       */
   char serie[PR_ID];
   int  version;                  /* el numero, COMO CAMPO                */
   char padre[PR_ID];             /* "" si es raiz                        */
   char razon[PR_RAZON];          /* "" si no consta. NO se rellena.      */
   char creado[16];               /* AAAA-MM-DD                           */
} PrModelo;

typedef struct {
   char id[PR_ID];
   char elegido[PR_ID];           /* "" si no se ha declarado             */
   char razon[PR_RAZON];          /* por que ese y no otro                */
} PrSerie;

typedef struct {
   int  schema_version;
   char id[PR_ID];
   char titulo[PR_TEXTO];
   char creado[16];
   char analista[PR_TEXTO];
   char raiz[PR_RUTA];            /* todo lo demas es relativo a esto     */

   PrSerie  s[PR_MAX_SERIE];
   int      ns;
   PrModelo m[PR_MAX_MODELO];
   int      nm;

   char path[PR_RUTA];            /* de donde se leyo                     */
} Proyecto;

/* --- el manifiesto ------------------------------------------------------ */

/* Un proyecto nuevo, en memoria. raiz puede ser NULL (entonces ".").      */
void pr_nuevo( Proyecto *p, const char *id, const char *titulo,
               const char *raiz );

/* Lee y escribe el proyecto.yaml. 0 si pudo.
 *
 * EL SUBCONJUNTO DE YAML ESTA DECLARADO Y ES PEQUEÑO: claves "a: b" con
 * sangria de dos espacios, hasta tres niveles, valores entre comillas o a
 * secas. Es lo que esta biblioteca escribe y lo que sabe leer, y lo que no
 * entiende LO DICE en vez de adivinarlo. Un yaml.safe_load de Python lo lee
 * entero -- eso es lo que importa para que la otra encarnacion pueda mirarlo.
 */
int pr_leer( const char *path, Proyecto *p, PrError *e );
int pr_escribir( const Proyecto *p, const char *path, PrError *e );

/* --- nombres -> rutas --------------------------------------------------- */

/* <raiz>/<serie>/work/<serie>_<id><ext>, con la raiz resuelta contra el
 * directorio del propio manifiesto. ext lleva su punto ("­.inp").
 * Con id NULL o "", devuelve el directorio de la serie.                  */
int pr_ruta( const Proyecto *p, const char *serie, const char *id,
             const char *ext, char *out, size_t n );

/* --- las series --------------------------------------------------------- */

int  pr_serie_add( Proyecto *p, const char *serie, PrError *e );
int  pr_serie_idx( const Proyecto *p, const char *serie );

/* El modelo ELEGIDO de una serie: hoy esa decision vive en un diccionario a
 * pelo repetido en tres guiones de cases/. Devuelve "" si no se declaro.  */
const char *pr_elegido( const Proyecto *p, const char *serie );
int  pr_elige( Proyecto *p, const char *serie, const char *id,
               const char *razon, PrError *e );

/* --- la cadena ---------------------------------------------------------- */

/* UNA ITERACION NUEVA. padre puede ser NULL/"" para la primera.
 *
 * El id sale solo --m00, m01...-- y la version es su numero COMO CAMPO. En
 * id_out queda la clave; en ruta_out, el .inp que hay que escribir. El linaje
 * se registra aqui, SIN PREGUNTAR: es lo minimo que no se puede perder.   */
int pr_deriva( Proyecto *p, const char *serie, const char *padre,
               char *id_out, size_t nid, char *ruta_out, size_t nruta,
               PrError *e );

/* La razon, DESPUES. Se puede no llamar nunca.                           */
int pr_razon( Proyecto *p, const char *serie, const char *id,
              const char *razon, PrError *e );

/* Los que no la tienen. La ventana los ENSEÑA, no los esconde.
 * Devuelve cuantos; llena hasta max.                                     */
int pr_sin_razon( const Proyecto *p, char ids[][PR_ID], int max );

/* DE DONDE VIENE ESTE MODELO, hasta la raiz. camino[0] es el propio modelo.
 * Devuelve cuantos pasos, o -1 si el linaje se muerde la cola.           */
int pr_camino( const Proyecto *p, const char *serie, const char *id,
               char camino[][PR_ID], int max );

int pr_modelo_idx( const Proyecto *p, const char *serie, const char *id );

/* --- los errores, en los dos idiomas ------------------------------------ */

const char *pr_error_es( const PrError *e, char *out, size_t n );
const char *pr_error_en( const PrError *e, char *out, size_t n );

#endif /* ATSW_PROYECTO_H */
