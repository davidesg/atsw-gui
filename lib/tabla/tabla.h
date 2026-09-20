/*
 * tabla.h -- la tabla publicable: UNA tabla, TRES renderizados.
 *
 * ES EL MODULO QUE TASTE DECLARO Y NUNCA ESCRIBIO. En su fuente estan
 * declarados TABLA, Graba_Ser_PRN y Graba_Ser_TSF, y los tres estan vacios. El
 * estudio lo señalo asi:
 *
 *     "Se construyo el instrumento y se dejo sin construir el informe. Ese es
 *      exactamente el hueco que la interfaz madre tiene que llenar, y la fase
 *      5 deberia tomarlo como su PRIMER requisito y no como el ultimo."
 *                                                  (ESTUDIO-taste.md §5)
 *
 * Treinta años despues el inventario encontro lo mismo en la suite de hoy:
 * ningun boton "Exportar" en ninguno de los tres GUIs, ningun export de
 * tablas, y un solo CSV en todo el programa. Siete pantallas, y la unica forma
 * de sacar un numero era copiarlo de una etiqueta.
 *
 * LA PROCEDENCIA VA DENTRO, Y NO ES DECORACION. Una tabla que sale del
 * programa sin decir de que modelo, de que muestra y de que version del motor
 * viene es la forma mas facil de que un numero acabe en un paper sin poder
 * reproducirlo. Aqui no se puede escribir una tabla sin ella: los tres
 * escritores la emiten.
 *
 * UNA TABLA, TRES RENDERIZADOS, LOS MISMOS NUMEROS. Los decimales los declara
 * la COLUMNA y valen para los tres formatos. Una tabla que dice 0.42 en el PDF
 * y 0.4237 en el CSV son dos tablas distintas, y la de arriba es la que se
 * publica: si hace falta toda la precision, esta el .out, que es el registro.
 *
 * Sin dependencias, como lib/rutas: lo enlazan los motores tambien.
 */

#ifndef ATSW_TABLA_H
#define ATSW_TABLA_H

#include <stddef.h>

#define TB_MAX_COL    24
#define TB_MAX_PROC   12          /* entradas de procedencia                 */
#define TB_TEXTO_MAX  96          /* lo que cabe en una celda de texto       */

typedef enum {
   TB_TXT,                        /* texto: se alinea a la izquierda         */
   TB_NUM,                        /* numero con decimales declarados         */
   TB_ENT                         /* entero                                  */
} TbTipo;

typedef struct Tabla Tabla;

/* --- construir ---------------------------------------------------------- */

/* titulo: el de la tabla. NULL para ninguno. */
Tabla *tb_new( const char *titulo );
void   tb_free( Tabla *t );

/* LA PROCEDENCIA. Clave y valor, en el orden en que se declaran. Lo tipico:
 *
 *     tb_procedencia( t, "Modelo",  "m6, 4 enlaces" );
 *     tb_procedencia( t, "Muestra", "1/1977 - 4/1992, 64 obs" );
 *     tb_procedencia( t, "Motor",   "drtran 1.0" );
 *     tb_procedencia( t, "Fichero", "modelo.out" );
 */
void tb_procedencia( Tabla *t, const char *clave, const char *valor );

/* Una columna. unidad puede ser NULL; dec se ignora si no es TB_NUM.
 * Devuelve el indice de la columna, o -1 si no cabe.                       */
int tb_col( Tabla *t, const char *nombre, const char *unidad,
            TbTipo tipo, int dec );

/* Abre una fila nueva. Devuelve su indice, o -1 si no se pudo.            */
int tb_fila( Tabla *t );

/* Poner una celda en la ULTIMA fila abierta. Fuera de rango no hace nada. */
void tb_pon_txt  ( Tabla *t, int col, const char *v );
void tb_pon_num  ( Tabla *t, int col, double v );
void tb_pon_vacio( Tabla *t, int col );        /* se escribe como "-"       */

/* --- escribir ----------------------------------------------------------- */

/* Los tres devuelven 0 si fue bien.
 *
 * CSV     el portable. La procedencia va en lineas de cabecera que empiezan
 *         por '#', que es el convenio que la suite ya usa en el fichero de
 *         residuos de "drtran -e". El separador es la coma y los campos con
 *         coma, comillas o salto de linea se entrecomillan segun RFC 4180. El
 *         separador decimal es el PUNTO: es lo que lee cualquier herramienta,
 *         y una tabla con coma decimal y coma de campo no se puede leer.
 *
 * TXT     ancho fijo, como los cuadros del .out. Es lo que se pega en un
 *         correo y sigue cuadrando.
 *
 * TEX     un tabular con su caption. La procedencia va como nota al pie.
 */
int tb_write_csv( const Tabla *t, const char *path );
int tb_write_txt( const Tabla *t, const char *path );
int tb_write_tex( const Tabla *t, const char *path );

/* Elige por la extension de path: .csv, .txt/.dat, .tex. Si no la reconoce,
 * CSV, que es el unico que se lee en todas partes.                        */
int tb_write( const Tabla *t, const char *path );

/* Cuantas filas y columnas lleva. Para poder decir "no hay nada que exportar"
 * antes de abrir un dialogo.                                              */
int tb_nfilas( const Tabla *t );
int tb_ncols ( const Tabla *t );

#endif /* ATSW_TABLA_H */
