/*
 * iconos.h -- los iconos propios de las barras de los GUIs (fue_gui, drtran_gui).
 *
 * Dibujan lo que ensena cada ventana (la diagnosis, sus renglones con
 * semaforo; los anomalos, el residuo que se sale de la banda; la ganancia,
 * la escalera hasta su nivel de largo plazo...), y no dependen del tema del
 * escritorio, que en cada plataforma pone otra cosa. Van como SVG dentro
 * del binario, y el trazo toma el color del texto del tema, asi que valen
 * en claro y en oscuro.
 *
 * Si no hay cargador de SVG (gdk-pixbuf sin librsvg), se cae al icono del
 * tema que se pase de respaldo: el programa sigue, con otro dibujo.
 */

#ifndef AT_ICONOS_H
#define AT_ICONOS_H

#include <gtk/gtk.h>

/* Un GtkImage con el icono nombre ("estimar", "ver-modelo", "diagnosis",
 * "anomalos", "ganancia", "identificar", "prever", "integracion",
 * "estacionalidad", "factores", "exportar", "nuevo", "abrir", "guardar"),
 * de px pixeles logicos, con el color del texto de ref (puede ser NULL).
 * respaldo es el nombre del icono del tema si no se puede dibujar.       */
GtkWidget *at_icono( const char *nombre, int px, GtkWidget *ref,
                      const char *respaldo );

/* Una entrada de menu con su icono a la izquierda: GTK 3 ya no los pone. */
GtkWidget *at_menu_item( const char *texto, const char *icono, GtkWidget *ref,
                          const char *respaldo );

#endif
