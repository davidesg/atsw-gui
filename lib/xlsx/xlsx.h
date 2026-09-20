/*
 * xlsx.h -- leer una hoja de calculo .xlsx. Lo justo, y declarado.
 *
 * EN TODA LA SUITE NO HABIA NINGUNO. El inventario lo midio: el unico sitio
 * donde se lee un .xlsx es en los guiones Python de cases/, con pandas. Del
 * lado C, nada -- y los datos del analista vienen en .xlsx.
 *
 * QUE ES UN .xlsx: un ZIP con XML dentro. Hacen falta tres partes:
 *
 *     xl/sharedStrings.xml   el texto, en una tabla aparte y por indice
 *     xl/styles.xml          CUAL ES LA TRAMPA, ver abajo
 *     xl/worksheets/sheet1.xml   las celdas
 *
 * LA TRAMPA SON LAS FECHAS, Y NO SE PUEDE ESQUIVAR. En Excel una fecha ES UN
 * NUMERO: 35095 es el 31/1/1996. Lo unico que distingue una fecha de un numero
 * es SU ESTILO. Comprobado en un fichero real del analista:
 *
 *     <c r="A2" s="1"><v>35095</v></c>      s=1 -> numFmtId=17 -> "mmm-yy"
 *     <c r="B2" s="2"><v>55.12</v></c>      s=2 -> numFmtId=2  -> "0.00"
 *
 * Un lector que ignore los estilos importa las dos como numeros y se lleva la
 * columna de fechas dentro de los datos SIN ENTERARSE. Por eso styles.xml no
 * es opcional aqui: es la diferencia entre leer bien y leer mal.
 *
 * EL ALCANCE ESTA DECLARADO, y lo que no cabe se dice en vez de adivinarse:
 *
 *     - LA PRIMERA HOJA. Un libro con varias hojas se lee la primera y se
 *       dice cuantas habia.
 *     - numeros, texto compartido, texto en linea y fechas.
 *     - las celdas vacias se respetan: la referencia "r" dice que columna es
 *       cada una, asi que un hueco NO corre las de al lado.
 *     - NO se evaluan formulas: se lee el valor cacheado, que es lo que Excel
 *       dejo escrito. Si no lo hay, la celda sale vacia.
 *
 * La descompresion es zlib en crudo (deflate sin cabecera), que es lo que usa
 * el formato ZIP. No hace falta nada mas.
 */

#ifndef ATSW_XLSX_H
#define ATSW_XLSX_H

#include <stddef.h>

/* Los limites son GUARDAS, no reservas: la hoja se reserva por su tamaño
 * REAL. El del total de celdas existe porque la rejilla es densa y una hoja
 * patologica de 512 x 65536 pediria gigas.                              */
#define XL_MAX_COL    512
#define XL_MAX_FILA   65536
#define XL_MAX_CELDA  2000000
#define XL_TEXTO      96

typedef enum {
   XL_OK = 0,
   XL_ENOFILE,       /* no se pudo abrir                                  */
   XL_ENOZIP,        /* no es un ZIP: no es un .xlsx                      */
   XL_EPARTE,        /* falta una parte que hace falta                    */
   XL_EINFLATE,      /* la descompresion fallo                            */
   XL_EMEM,
   XL_EGRANDE,       /* mas filas o columnas de las que caben             */
   XL_EVACIA         /* la hoja no tiene nada                             */
} XlCodigo;

typedef struct {
   XlCodigo cod;
   char     texto[160];
   int      esperaba, encontro;
} XlError;

typedef enum { XL_NADA = 0, XL_NUM, XL_TXT, XL_FECHA } XlTipo;

typedef struct {
   XlTipo tipo;
   double v;                       /* XL_NUM, y el serial si XL_FECHA     */
   char   s[XL_TEXTO];             /* XL_TXT, y la fecha ya escrita       */
} XlCelda;

typedef struct {
   XlCelda *c;                     /* nfila x ncol                        */
   int      nfila, ncol;
   int      nhojas;                /* cuantas habia; se leyo la primera   */
   char     hoja[XL_TEXTO];        /* el nombre de la que se leyo         */
} XlHoja;

/* Lee la PRIMERA hoja. 0 si pudo. Liberar con xl_libre.                  */
int  xl_leer( const char *path, XlHoja *h, XlError *e );
void xl_libre( XlHoja *h );

/* La celda (fila, col), 0-indexadas. NULL fuera de rango.                */
const XlCelda *xl_celda( const XlHoja *h, int fila, int col );

/* Un serial de Excel a "p/aaaa" o "aaaa-mm-dd". El origen es 1899-12-30:
 * el 1900 de Excel tiene un 29 de febrero que no existio, y ese origen lo
 * compensa para todo lo que sea posterior a marzo de 1900 -- que es todo lo
 * que un economista va a cargar.                                        */
void xl_fecha( double serial, int *anio, int *mes, int *dia );

const char *xl_error_es( const XlError *e, char *out, size_t n );
const char *xl_error_en( const XlError *e, char *out, size_t n );

#endif /* ATSW_XLSX_H */
