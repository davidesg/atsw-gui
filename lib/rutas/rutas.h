/*
 * rutas.h -- componer nombres de fichero SIN perder el directorio.
 *
 * EL FALLO QUE ESTO CIERRA, y estaba en cuatro sitios de tres programas:
 *
 *     snprintf( out, n, "A%s", x11out );          <- fue.c:1545, :1567
 *     snprintf( out, n, "forecast_%s.inp", base ) <- fue.c:205
 *     sprintf ( file, "%s%s.eps", prefix, x11out ) <- fugplot.c (arreglado ya)
 *
 * Con un nombre a secas funcionan. Con una RUTA, el prefijo se antepone al
 * directorio y sale un disparate:
 *
 *     fue caso/X        ->  "Acaso/X.eps"          el directorio "Acaso" no existe
 *     fue caso/X -f     ->  "forecast_caso/X.inp"  idem
 *
 * Y LOS DOS SE PORTAN DISTINTO, que es lo peor: el de -f aborta y se ve; el del
 * EPS solo AVISA y sigue, asi que el informe sale sin el grafico de residuos y
 * nadie se entera. Un fallo que solo avisa es mas caro que uno que para.
 *
 * El tercer caso es el mismo problema por el otro lado: drtran componia el
 * nombre de salida por defecto con base_name(), que DESCARTA el directorio, asi
 * que el .out no caia junto a los datos sino donde se hubiera lanzado el
 * programa. El resultado dependia del cwd.
 *
 * LA REGLA, UNA SOLA: el prefijo y el sufijo van en el NOMBRE; el directorio
 * se conserva. Es requisito de la interfaz madre -- mientras un motor escriba
 * donde se lanzo, el proyecto no puede prometer donde estan las cosas.
 *
 * Sin dependencias: lo enlazan los motores, que no tienen glib.
 */

#ifndef ATSW_RUTAS_H
#define ATSW_RUTAS_H

#include <stddef.h>

/* out = <dir de path>/<prefijo><nombre de path><sufijo>
 *
 * El directorio de path se conserva; prefijo y sufijo se pegan al NOMBRE.
 * prefijo o sufijo pueden ser NULL. Devuelve 0 si cupo, 1 si no (y out queda
 * truncado pero terminado).
 *
 *     ruta_componer( "caso/X", "A", ".eps", b, n )  ->  "caso/A X.eps" sin el
 *                                                        espacio: "caso/AX.eps"
 *     ruta_componer( "X",      "A", ".eps", b, n )  ->  "AX.eps"
 */
int ruta_componer( const char *path, const char *prefijo, const char *sufijo,
                   char *out, size_t n );

/* out = <dir de path>/<nombre de path sin extension>. Conserva el directorio.
 * Es lo que hay que usar en vez de un base_name() que lo tira.          */
int ruta_sin_ext( const char *path, char *out, size_t n );

/* out = <dir de path>, o "." si no lleva. Sin la barra final.           */
int ruta_dir( const char *path, char *out, size_t n );

/* El nombre SIN DIRECTORIO, con su extension intacta.
 *
 * NO ES LO MISMO QUE ruta_nombre, Y LA DIFERENCIA COSTO 72 PRUEBAS DE ORO.
 * En fue el nombre base de un caso puede llevar puntos que NO son extension:
 * "DE.2" es el nombre entero, y quitarle el ".2" renombraba la serie de
 * residuos de ADE.2 a ADE -- cambiando el EPS y la ecuacion del .tex.
 *
 * Regla: si lo que se quiere es quitar el DIRECTORIO, ruta_base. Si ademas se
 * quiere quitar la extension --porque se le va a poner otra-- ruta_nombre, y
 * sabiendo lo que se hace.                                              */
int ruta_base( const char *path, char *out, size_t n );

/* El nombre a secas, sin directorio Y SIN EXTENSION. Ver el aviso de
 * ruta_base: un punto en el nombre se pierde.                           */
int ruta_nombre( const char *path, char *out, size_t n );

#endif /* ATSW_RUTAS_H */
