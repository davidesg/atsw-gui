/* main_wrap.c -- el main.c DE VERDAD, para que la prueba lo conduzca.
 *
 * No es una copia: se incluye src/main.c entero. Lo unico que se cambia son
 * tres nombres, y por que:
 *
 *   main          -> gui_main       la prueba tiene su propio main, y llama
 *                                   a este con los argumentos que quiera
 *                                   (--proyecto, un fichero, --help...).
 *   gtk_main      -> test_gtk_main  en vez de esperar al usuario, la prueba
 *                                   toma el mando con la ventana ya armada.
 *   g_slice_new0  -> se guarda      AppWidgets es una variable LOCAL de main;
 *                                   sin esto la prueba no tendria con que
 *                                   llamar a los botones.
 *
 * gtk.h se incluye ANTES, y por eso funciona: sus guardas impiden que al
 * volver a incluirse desde main.c pise nuestras definiciones.          */

#include <stdlib.h>
#include <gtk/gtk.h>
#include "gui.h"

AppWidgets *test_app = NULL;
void test_gtk_main(void);

#undef  g_slice_new0
#define g_slice_new0(T)    (test_app = g_new0(T, 1))
/* La prueba sigue mirando la ventana despues de que gui_main vuelva.   */
#undef  g_slice_free
#define g_slice_free(T, p) ((void) (p))
#define main               gui_main
#define gtk_main           test_gtk_main

#include "../src/main.c"
