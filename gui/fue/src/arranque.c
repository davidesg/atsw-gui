/*
 * arranque.c -- lo que la linea de ordenes deja dicho, para que lo pregunte
 * la ventana.
 *
 * NO ESTA EN main.c A PROPOSITO. La ventana consulta estas dos cosas, y las
 * pruebas levantan la ventana sin main() --no puede haber dos main()-- asi
 * que teniendolas alli el enlace se caia por fue_raiz_proyecto. Un dato que
 * la interfaz consulta no puede vivir en el unico fichero que las pruebas no
 * pueden enlazar.
 *
 * Sin --proyecto y sin fichero, las dos contestan NULL y el programa funciona
 * como siempre: son opcionales, y decir "no hay" es una respuesta.
 */

#include <stdio.h>

static char g_raiz_proyecto[1024];
static char g_abrir[1024];

/* --proyecto FICHERO: el espacio de trabajo sale de la RAIZ del proyecto.
 *
 * Es lo minimo que fue_gui necesita de la interfaz madre y lo que de verdad
 * le falta: NO GUARDA NADA entre ejecuciones --ni sesion, ni preferencias,
 * ni recientes-- asi que cada arranque empezaba preguntando donde esta todo.
 * Con esto arranca sabiendo en que proyecto esta.                        */
const char *fue_raiz_proyecto(void)
{
    return g_raiz_proyecto[0] ? g_raiz_proyecto : NULL;
}

void fue_pon_raiz_proyecto(const char *s)
{
    snprintf(g_raiz_proyecto, sizeof g_raiz_proyecto, "%s", s ? s : "");
}

/* EL FICHERO QUE LA MADRE MANDA.
 *
 * Sin esto, "abrir en fue" solo arrancaba fue: el analista tenia que ir a
 * buscar a mano la serie que acababa de marcar en la ventana de al lado. Una
 * madre que lanza programas sin decirles a que vienen no gestiona nada.  */
const char *fue_abrir(void)
{
    return g_abrir[0] ? g_abrir : NULL;
}

void fue_pon_abrir(const char *s)
{
    snprintf(g_abrir, sizeof g_abrir, "%s", s ? s : "");
}
