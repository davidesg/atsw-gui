/*
 * atsw.h -- la interfaz madre.
 *
 * ATSW es un TALLER, y tiene dos encarnaciones (DISENO-madre.md §10): ATSW
 * Python, conducida por un LLM, y ATSW GUI, esta, con los motores en C. Los
 * mismos motores; GESTION DE PROYECTOS DISTINTA, y distribuciones separadas.
 *
 * LA MADRE NO ABSORBE LOS TRES GUIs: LES DA UN CONTEXTO QUE NO TENIAN. Los
 * tres funcionan y ninguno guardaba nada entre ejecuciones --ni sesion, ni
 * preferencias, ni recientes--: son context-free por construccion. Lo que les
 * faltaba no era una ventana que los contuviera, era saber en que proyecto
 * estan. Por eso se lanzan con --proyecto y siguen siendo programas aparte.
 *
 * GESTIONA TRES COSAS, no una: DATOS, MODELOS y PROYECTOS.
 *
 * LOS RESULTADOS SE LEEN DEL .out, CON SU HUELLA. Decision del analista.
 * Cachear sigma y Q en el manifiesto seria mas rapido y PODRIA MENTIR: si
 * alguien reestima por fuera --y puede, porque los motores son programas-- el
 * numero guardado seguiria diciendo lo de antes. El manifiesto guarda LINAJE y
 * RAZON, que son decisiones; los resultados son del .out, que es el registro.
 *
 * La huella es tamaño + fecha, no un SHA. No es una garantia criptografica y
 * no pretende serlo: lo que tiene que cazar es que el fichero haya cambiado
 * desde que se leyo, y para eso basta. Se dice aqui para que nadie le pida lo
 * que no da.
 *
 * Y EL GESTO QUE CIERRA EL CICLO VIVE AQUI:
 *
 *     .pre(-1) ──copiar──→ .inp(0)
 *
 * .pre e .inp son el mismo formato, pero el motor EXIGE la extension .inp, asi
 * que es una copia fisica real. Va en la madre y no en fue_gui porque es un
 * gesto de PROYECTO --crea una iteracion y registra su linaje-- y no de
 * estimacion.
 */

#ifndef ATSW_GUI_H
#define ATSW_GUI_H

#include <gtk/gtk.h>

#include "proyecto.h"
#include "outdiag.h"
#include "tabla.h"

#define AT_MAX_RES  PR_MAX_MODELO

/* Las columnas de las dos listas. */
/* La DESCRIPCION no es columna: crece con lo que el analista escriba y
   arrollaria una lista que tiene que caber al lado de la rejilla. Va en el
   globo de la fila, con el resto de la procedencia.                     */
enum { S_ID, S_ELEGIDO, S_NMOD, S_RAZON, S_GLOBO, S_N };
/* LAS COLUMNAS DE LA REJILLA.
 *
 * La rejilla contesta UNA pregunta: de estos modelos de la misma serie,
 * ¿cual me quedo? Y eso son cuatro cosas -- que ES, como ajusta, si los
 * residuos son blancos, y por que se hizo.
 *
 * Lo que se fue y por que:
 *   Version   el id ya lo dice (m01 -> 1). Dos columnas para un dato.
 *   Hosking   es el portmanteau MULTIVARIANTE, de drtran. fue no lo emite,
 *             asi que en univariante salia siempre vacia.
 *   Blancos   se deducia de Hosking. Con el p del Ljung-Box delante, el
 *             veredicto sobra: .000 y .44 se leen solos.
 *
 * Y el Jarque-Bera NO es columna: la normalidad no decide entre modelos,
 * se le pregunta al elegido. Va en el globo de la fila, con la asimetria
 * y la curtosis, que es donde se mira cuando se mira.                   */
enum { M_ID, M_PADRE, M_ESTRUCT, M_SD, M_Q, M_P, M_RAZON, M_ESTRELLA,
       M_GLOBO, M_N };

/* Lo que se leyo de un .out, con la huella del fichero del que salio. */
typedef struct {
   char     serie[PR_ID], id[PR_ID];
   gboolean hay;                  /* se pudo leer                          */
   double   sd;                   /* la d.t. residual de la ecuacion 1      */
   double   logl;
   gboolean tiene_logl;
   /* EL LJUNG-BOX de la ACF de los residuos, que es el contraste del
      modelo UNIVARIANTE. Antes aqui estaba el Hosking de drtran, que fue
      no emite: la columna salia siempre vacia.                         */
   double   q, qp;
   int      qdf;
   double   jb, jbp;              /* Jarque-Bera, para el globo            */
   double   skew, kurt;
   int      npar;
   char     estruct[64];          /* "(0,1,1)(0,1,1)12  log"               */

   /* LA HUELLA: tamaño y fecha. Si cambia, se relee. */
   long     tam;
   long     mtime;
} AtRes;

typedef struct {
   GtkWidget *ventana;
   GtkWidget *l_proy;             /* que proyecto esta abierto              */
   GtkWidget *l_series;           /* la lista de series                     */
   GtkWidget *l_modelos;          /* la rejilla de modelos de la marcada    */
   GtkWidget *ver_cuenta;         /* veredicto: cuantas cosas hay           */
   GtkWidget *ver_ojo;            /* veredicto: LO QUE HAY QUE MIRAR        */
   GtkWidget *estado;             /* la barra de abajo                      */
   GtkWidget *b_fue, *b_fug, *b_drtran, *b_nuevo, *b_iterar, *b_elegir, *b_razon;

   Proyecto  *p;
   gboolean   hay;

   AtRes      r[AT_MAX_RES];
   int        nr;

   char       serie[PR_ID];       /* la serie marcada                       */

   /* REPINTANDO: no leer la seleccion.
    *
    * gtk_list_store_clear dispara "changed" con nada marcado, asi que marcar
    * una serie borraba la marca que acababa de ponerse: click -> changed ->
    * refresco -> clear -> changed -> serie vacia. Es el mismo guardia que
    * main_window.c de drtran_gui llama «recolocando».                 */
   gboolean   recolocando;
} Atsw;

GtkWidget *atsw_ventana_new( GtkApplication *app, Atsw *a );
void       atsw_refresca( Atsw *a );

/* EL EDITOR DEL .inp, sobre un NODO del proyecto. La otra puerta a la misma
   iteracion: fue_gui especifica por formulario, y el formulario solo puede
   expresar lo que tiene widgets. Ver docs/DISENO-editor.md.             */
void       atsw_editor( Atsw *a, const char *serie, const char *id );

/* «Editar…» una serie: descripcion, unidades, fuente, url, bajada, notas.
   Nada de esto toca un numero -- son los campos que el .inp no puede
   llevar y que hacen falta para volver al analisis meses despues.      */
void       atsw_serie_edita( Atsw *a, const char *serie );
gchar     *atsw_programa( const char *programa );

/* Guardar un .inp VALIDANDO ANTES, con el comprobador del motor. Si no vale,
   el fichero que habia NO se toca y why dice por que, con su linea. Fuera
   del widget porque es LA regla del editor y ahi se puede probar.       */
int        atsw_guarda_inp( const char *destino, const char *txt,
                            char *why, size_t n );
gboolean   atsw_abre( Atsw *a, const char *path, char *why, size_t n );

/* El resultado de un modelo, releyendo el .out si la huella cambio. */
const AtRes *atsw_resultado( Atsw *a, const char *serie, const char *id );

/* EL MODELO AL QUE APUNTAN LOS BOTONES si el analista no marca otro: el
 * ELEGIDO si lo hay, y si no el ULTIMO -- la iteracion mas reciente, que es
 * lo que casi siempre se quiere. "" si la serie no tiene ninguno.
 *
 * Es una REGLA, no un asunto de interfaz, asi que vive aqui y se prueba
 * aparte: la rejilla la usa para marcar y los envios para apuntar.    */
const char *atsw_modelo_por_defecto( const Proyecto *p, const char *serie );

#endif /* ATSW_GUI_H */
