/*
 * gesto.h -- los gestos de PROYECTO, sin ventana: iterar y modelo nuevo.
 *
 * POR QUE SALEN DE LA MADRE. Vivian en gui/atsw/src/gestos.c, atados a su
 * ventana (Atsw *), y ahora los hace tambien atsw-cli, que es la puerta del
 * asistente (docs/ESTUDIO-terminal-mcp.md §2.2 y §2.5). Dos copias de un
 * gesto son dos gestos: el dia que uno cambie de donde copia, el otro
 * seguira copiando de donde copiaba. Asi que el gesto es este, y la madre y
 * atsw-cli son dos anfitriones.
 *
 * NO GUARDAN EL MANIFIESTO. Tocan el Proyecto en memoria y escriben el .inp
 * nuevo; guardar es del que llama, que es quien sabe como (la madre apunta
 * ademas la huella del fichero para no releerse a si misma).
 *
 * Devuelven 0, o 1 con el porque en why. Con 0, why trae la frase que la
 * madre pone en la barra.
 */

#ifndef ATSW_GESTO_H
#define ATSW_GESTO_H

#include <stddef.h>

#include "proyecto.h"

/* ITERAR: el .pre de padre se copia al .inp de un hijo nuevo, con su linaje.
 *
 *     .pre(-1) --copiar--> .inp(0)
 *
 * Es una copia fisica: el motor exige la extension .inp. Y un .pre que se
 * toca vuelve a ser un .inp: sus valores pasan a ser semillas. Se queda en
 * la misma muestra: seguir desde un optimo es seguir sobre las mismas
 * observaciones.                                                         */
int ge_itera( Proyecto *p, const char *serie, const char *muestra,
              const char *padre, char *id_out, size_t nid,
              char *ruta_out, size_t nruta, char *why, size_t n );

/* UN MODELO NUEVO: nace de los DATOS de esa muestra, no de un optimo. El
 * .inp nuevo es copia del de los datos y cuelga de el.                   */
int ge_modelo_nuevo( Proyecto *p, const char *serie, const char *muestra,
                     char *id_out, size_t nid, char *ruta_out, size_t nruta,
                     char *why, size_t n );

/* El sha256 de un fichero (g_free), o NULL si no se puede leer. Es el que
 * guardan los casos de sus .pre.                                        */
char *ge_sha( const char *ruta );

#endif /* ATSW_GESTO_H */
