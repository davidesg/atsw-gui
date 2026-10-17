/*
 * cuaderno.h -- el cuaderno del proyecto: LA EVIDENCIA QUE SE MIRO.
 *
 * El guion guarda las DECISIONES (que modelo, por que). Nada guardaba lo que
 * el analista tenia delante al tomarlas: la diagnosis, la ganancia, los
 * contrastes. Se cerraba la ventana y se perdia. El cuaderno es eso, en un
 * solo fichero que se lee en orden:
 *
 *     <directorio del manifiesto>/cuaderno/cuaderno.md
 *     <directorio del manifiesto>/cuaderno/img/...png
 *
 * VIENE DE TASTE, CORREGIDO. TASTE mandaba todo resultado a un visor y la
 * tecla [A] lo añadia a TASTE.OUT; lo que no se añadia se perdia, y el
 * fichero se truncaba en cada arranque (ESTUDIO-taste-interfaz.md §3.1).
 * Aqui es al reves: añadir es un boton, el fichero NUNCA se trunca y cada
 * entrada lleva su procedencia.
 *
 * MARKDOWN porque se lee sin la suite: en un editor es texto, en GitHub o en
 * cualquier visor es un documento con sus tablas y sus graficos.
 *
 * UNA ENTRADA SE ESCRIBE ENTERA O NO SE ESCRIBE. Se arma en memoria y se
 * añade de una vez al cerrar: un fallo a medias no deja media entrada.
 *
 *     Cuaderno *c = cu_nueva( p->path, "Diagnosis — IPC / m03" );
 *     cu_dato( c, "Serie", "IPC" );
 *     cu_tabla( c, t );
 *     if ( cu_cierra( c, why, sizeof why ) != 0 ) ...
 */

#ifndef ATSW_CUADERNO_H
#define ATSW_CUADERNO_H

#include <stddef.h>

#include "tabla.h"

typedef struct Cuaderno Cuaderno;

/* Una entrada nueva, todavia sin escribir. manifiesto es la ruta del
 * proyecto.yaml: el cuaderno vive a su lado. NULL si no hay manifiesto.   */
Cuaderno *cu_nueva( const char *manifiesto, const char *titulo );

/* Una linea de procedencia: «- **Serie:** IPC». */
void cu_dato( Cuaderno *c, const char *clave, const char *valor );

/* Un parrafo de Markdown, tal cual. */
void cu_texto( Cuaderno *c, const char *md );

/* Una tabla de lib/tabla, en Markdown, con su propia procedencia. */
void cu_tabla( Cuaderno *c, const Tabla *t );

/* UN GRAFICO. Devuelve en ruta donde hay que escribir el PNG
 * (cuaderno/img/<fecha>-<nombre>.png) y deja el enlace puesto. Si al cerrar
 * el fichero no existe, el enlace se quita y la entrada lo dice: un enlace
 * roto en un cuaderno es peor que no tenerlo. 0 si fue bien.             */
int cu_imagen( Cuaderno *c, const char *nombre, const char *pie,
               char *ruta, size_t n );

/* Añade la entrada a cuaderno.md, creando el directorio si hace falta, y la
 * libera. 0 si fue bien; si no, why dice por que.                        */
int cu_cierra( Cuaderno *c, char *why, size_t n );

/* La libera sin escribir nada. */
void cu_descarta( Cuaderno *c );

/* Una nota del analista, sin mas: una entrada con su fecha y el texto. */
int cu_nota( const char *manifiesto, const char *texto, char *why, size_t n );

/* Donde esta (o estara) el cuaderno de ese manifiesto. 0 si se pudo. */
int cu_ruta( const char *manifiesto, char *out, size_t n );

#endif /* ATSW_CUADERNO_H */
