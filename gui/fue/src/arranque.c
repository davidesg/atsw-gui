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
#include <stdlib.h>
#include "proyecto.h"

static char g_raiz_proyecto[1024];

/* EL MANIFIESTO, NO SOLO SU RAIZ.
 *
 * Antes se leia para sacar la raiz y se tiraba. Con eso fue_gui sabia DONDE
 * estaba y no DE QUE MODELO era el fichero que tiene abierto --y el nombre
 * del fichero es cortesia, asi que no se saca de el--. Sin la clave no hay
 * linaje, y sin linaje no se puede ofrecer ni la diagnosis ni los anomalos
 * de ESTE modelo. Se guarda, y se libera al salir el programa.        */
static Proyecto *g_proyecto;
static char g_abrir[1024];
static int  g_prever;

/* --proyecto FICHERO: el espacio de trabajo sale de la RAIZ del proyecto.
 *
 * Es lo minimo que fue_gui necesita de la interfaz madre y lo que de verdad
 * le falta: NO GUARDA NADA entre ejecuciones --ni sesion, ni preferencias,
 * ni recientes-- asi que cada arranque empezaba preguntando donde esta todo.
 * Con esto arranca sabiendo en que proyecto esta.                        */
Proyecto *fue_proyecto(void)
{
    return g_proyecto;
}

void fue_pon_proyecto(Proyecto *p)
{
    g_proyecto = p;
}

/* RELEER EL MANIFIESTO ANTES DE TOCARLO.
 *
 * Esta copia se leyo al arrancar y la madre sigue viva al lado: entre una
 * cosa y otra puede haber dado de alta una serie o derivado un modelo.
 * Escribir encima la copia de hace media hora seria perder eso, y no da
 * error -- el fichero queda bien formado, sin lo que falta.
 *
 * Si la relectura falla NO SE TOCA la que hay: un manifiesto roto por fuera
 * no puede tirarse por delante del que funciona. Devuelve 0 si pudo.  */
int fue_proyecto_relee(void)
{
    Proyecto *nuevo;
    PrError   e;
    int       rc = 1;

    if (!g_proyecto || !g_proyecto->path[0]) return 1;
    /* En el monton: Proyecto ocupa ~800 KB, y la pila del hilo principal
     * de Windows es de 1 MB (ver DtDatos en gui/fug/src/callbacks.c). */
    nuevo = calloc(1, sizeof *nuevo);
    if (nuevo == NULL) return 1;
    if (pr_leer(g_proyecto->path, nuevo, &e) == 0) {
        *g_proyecto = *nuevo;
        rc = 0;
    }
    free(nuevo);
    return rc;
}

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


/* --prever: AL ABRIR, EL CICLO DE PREVISION.
 *
 * Es lo mismo que pulsar «Forecast» en la barra --corre "fue -f", corre fuf y
 * carga las dos cosas en la pestaña-- pero pedido desde fuera. La madre lo usa
 * para que «Prever con fuf» de su lista de modelos acabe donde acabaria el
 * boton: en la pestaña de prevision, con el informe delante.
 *
 * Es una opcion y no un fichero: dice QUE HACER con lo que se abre.     */
int fue_prever_al_arrancar(void)
{
    return g_prever;
}

void fue_pon_prever(int si)
{
    g_prever = si;
}
