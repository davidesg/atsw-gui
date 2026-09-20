/*
 * datos.h -- LA PUERTA DE LOS DATOS. Una, no cuatro.
 *
 * El inventario (INVENTARIO-madre.md §1) encontro CUATRO lectores del mismo
 * formato --columnas de numeros separadas por blancos-- y que DISCREPAN:
 *
 *     gui/fug/src/data_load.c:117      APLANA todas las columnas en un vector
 *     gui/fue/src/data_handling.c:11   se queda con la COLUMNA 1
 *     engines/drvarma/gui/...:198      multivariante, con setlocale
 *     engines/drvarma/old/...:148      la copia anterior, sin setlocale
 *
 * El mismo fichero cargado en fue y en fug da DOS SERIES DISTINTAS. No
 * parecidas: una entrelaza las columnas y la otra se queda con una.
 *
 * NO ES EL CASO DE fugdraw. Aquellas cuatro copias eran IDENTICAS y se
 * factorizaron sin riesgo. Estas discrepan, asi que unificarlas OBLIGA A
 * ELEGIR un comportamiento, y elegirlo es una decision del metodo. Las
 * decisiones estan tomadas en DISENO-madre.md §4 y son estas:
 *
 *   columnas    LA 1 ES LA SERIE; las demas, regresores. Aplanar dos columnas
 *               fabrica una serie que no existe.
 *   cabecera    se acepta y se USA para nombrar. Quita un tecleo y no cuesta.
 *   separador   espacio, TAB, ';' y COMA. Hoy el filtro de fug anuncia *.csv
 *               y un CSV de verdad falla.
 *   decimal     coma si el separador no es la coma. Es la regla europea y es
 *               decidible: si el separador ES la coma, el decimal es el punto.
 *   frecuencia  DEL FICHERO si esta; del dialogo si no.
 *
 * ESE ULTIMO ES EL QUE IMPORTA. Hoy la frecuencia y la fecha de inicio NO
 * VIAJAN CON LA SERIE: salen de un combo, con defaults distintos por programa
 * (fug freq=1; fue y drvarma freq=12, año 2000). Una serie mal fechada al
 * cargarla envenena todo lo que venga despues y no hay nada que lo detecte.
 *
 * Se leen de dos sitios, los dos convenio de la casa:
 *
 *   una cabecera de '#', como la que escribe "drtran -e":
 *       # freq 4
 *       # start 1/1977
 *   o una COLUMNA DE FECHAS, si la primera columna no son numeros:
 *       1/1977   20.649   -18.097
 *
 * gui/drtran NO usa esto: no lee datos crudos, y es deliberado (main.c:9-11).
 * Un CSV alli se saltaria el escalon univariante. Esta puerta es para fue, fug
 * y para la madre al dar de alta una serie.
 *
 * EL ERROR ES UN HECHO, NO UNA FRASE. Es la leccion que costo una prueba de la
 * bateria en lib/netfile: la biblioteca devuelve QUE paso y cada front end lo
 * dice en su idioma. El motor habla ingles porque es una propiedad declarada
 * del puerto; el GUI, castellano.
 *
 * Sin dependencias, como lib/rutas y lib/tabla.
 */

#ifndef ATSW_DATOS_H
#define ATSW_DATOS_H

#include <stddef.h>

#define DT_MAX_COL    32
#define DT_MAX_OBS    8192
#define DT_NOMBRE     64
#define DT_FECHA      16

/* Que paso. El front end lo redacta. */
typedef enum {
   DT_OK = 0,
   DT_ENOFILE,          /* no se pudo abrir                                 */
   DT_EVACIO,           /* ni una fila de numeros                           */
   DT_EVALOR,           /* un campo que no es un numero (linea, columna)    */
   DT_ECOLS,            /* una fila con distinto numero de campos           */
   DT_EMUCHAS,          /* mas columnas de las que caben                    */
   DT_ELARGA,           /* mas observaciones de las que caben               */
   DT_EFECHA,           /* una columna de fechas que no se entiende         */
   DT_EXLSX             /* el libro no se pudo leer; el motivo va en texto  */
} DtCodigo;

typedef struct {
   DtCodigo cod;
   int      linea;      /* 1..n del fichero; 0 si no aplica                 */
   int      campo;      /* 1..n de la fila;  0 si no aplica                 */
   char     texto[64];  /* el campo que fallo, o el limite                  */
   int      esperaba;   /* para DT_ECOLS y los limites                      */
   int      encontro;
} DtError;

typedef struct {
   char   nombre[DT_MAX_COL][DT_NOMBRE];  /* de la cabecera, o ""           */
   double v[DT_MAX_COL][DT_MAX_OBS];      /* [columna][0..nobs-1]           */
   int    ncol, nobs;

   int    tiene_cabecera;
   int    tiene_fechas;                   /* habia columna de fechas        */
   char   fecha[DT_MAX_OBS][DT_FECHA];    /* si tiene_fechas                */

   /* Lo que el FICHERO dijo. 0 = no lo dijo, y entonces lo pone el que
      llama -- pero sabiendo que lo esta poniendo el.                       */
   int    freq;
   int    anio, per;

   char   sep;                            /* el separador que se dedujo     */
   char   dec;                            /* '.' o ','                      */
} DtDatos;

/* Lee el fichero. 0 si pudo; si no, e dice que paso.
 * d->freq / anio / per salen a 0 si el fichero no los declara.
 *
 * .xlsx TAMBIEN, y se reconoce POR SU CONTENIDO --los cuatro bytes PK\3\4--
 * y no por la extension: un libro con otro nombre se lee igual, y un texto
 * llamado .xlsx no se toma por un libro.
 *
 * Con un .xlsx la frecuencia y la fecha salen MEJOR que de un texto: una fecha
 * de Excel es un numero con un estilo de fecha, asi que se sabe el dia exacto
 * y la frecuencia se deduce del salto de MESES. Ver lib/xlsx, donde esta
 * explicado por que los estilos no son opcionales.                       */
int dt_leer( const char *path, DtDatos *d, DtError *e );

/* El error en castellano, para los GUIs. Devuelve out.                     */
const char *dt_error_es( const DtError *e, char *out, size_t n );

/* El error en ingles, para los motores: su salida en ingles es una propiedad
 * declarada del puerto y hay pruebas que la comprueban VERBATIM.           */
const char *dt_error_en( const DtError *e, char *out, size_t n );

/* Cuantas observaciones utiles tiene la columna c (sin los huecos del final).
 * Hoy no hay huecos: esta para cuando los haya.                            */
int dt_nobs( const DtDatos *d, int c );

#endif /* ATSW_DATOS_H */
