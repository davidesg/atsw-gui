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

/* EL ICONO DE ATSW: el de la web (packaging/linux/atsw.svg), el cuadrado
 * azul #1f5fa8 con la quebrada blanca. Se dibuja con cairo, sin cargador de
 * SVG: es el icono del programa y tiene que salir siempre.
 *
 *   at_logo              el dibujo en un pixbuf de px x px (g_object_unref)
 *   at_logo_por_defecto  el icono de todas las ventanas del proceso, en
 *                        varios tamaños; cada GUI lo llama al arrancar.  */
GdkPixbuf *at_logo( int px );
void       at_logo_por_defecto( void );

#endif
