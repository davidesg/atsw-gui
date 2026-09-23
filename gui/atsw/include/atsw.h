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
   char     serie[PR_ID], muestra[PR_ID], id[PR_ID];
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
   /* LA REJILLA QUE ESTA DELANTE.
    *
    * Hay una por MUESTRA -- un widget no puede tener dos padres, asi que
    * las hojas del cuaderno no pueden compartir vista-- y este campo apunta
    * a la de la hoja visible. Todo lo que actua sobre "el modelo marcado"
    * sigue leyendo de aqui sin enterarse de que hay varias.            */
   GtkWidget *libro;              /* el cuaderno, con las hojas abajo       */
   GtkWidget *l_modelos;          /* la rejilla de la hoja VISIBLE          */
   GtkWidget *hoja[PR_MAX_MUESTRA + 1];
   char       hoja_mu[PR_MAX_MUESTRA + 1][PR_ID];
   int        nhojas;
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
void       atsw_editor( Atsw *a, const char *serie, const char *muestra,
                        const char *id );

/* «Editar…» una serie: descripcion, unidades, fuente, url, bajada, notas.
   Nada de esto toca un numero -- son los campos que el .inp no puede
   llevar y que hacen falta para volver al analisis meses despues.      */
void       atsw_serie_edita( Atsw *a, const char *serie );

/* --- el dato y lo que se deriva de el ----------------------------------- */

/* EL .csv ES EL DATO; LOS .inp SE GENERAN DE EL. Un dueño y una derivacion,
   como .pre -> .inp. <hasta> recorta la ventana ("" = la muestra entera) y
   recorta el .inp QUE SALE, nunca el .csv: la muestra total es lo que
   entro y no se toca.                                                   */
int        atsw_genera_inp( const char *csv, const char *destino,
                            const char *serie, const char *hasta,
                            char *why, size_t n );

/* Cuantas observaciones caben hasta esa fecha. Fuera para poder probarlo:
   traducir "hasta 12/2019" a un numero de observaciones es lo unico de
   esto que hay que hacer bien.                                          */
int        atsw_hasta_n( int freq, int anio, int per, int nobs,
                         const char *hasta );

/* Donde vive el .csv de una serie: <raiz>/<serie>/datos.csv, FUERA de
   work/, que es donde se corre y lo que se limpia.                      */
int        atsw_csv_de( const Proyecto *p, const char *serie,
                        char *out, size_t n );

/* EL TRAMO DEL PROYECTO: la union de las series, porque las muestras son
   del proyecto y los datos de cada serie. mezcla avisa de frecuencias
   distintas, que es un proyecto que no se puede alimentar a drtran ni a
   drvarma sin sembrar bugs.                                            */
typedef struct {
   int      freq;
   int      anio, per;            /* el comienzo mas temprano             */
   int      fin_anio, fin_per;    /* el final mas tardio                  */
   int      nobs;                 /* la serie mas larga                   */
   int      nseries;              /* cuantas tenian datos.csv             */
   gboolean mezcla;               /* no todas con la misma frecuencia     */
} AtTramo;

int        atsw_tramo( const Proyecto *p, AtTramo *t );

/* El nodo de datos de esa ventana en cada serie que tenga .csv. Devuelve
   cuantos creo. Una hoja sin m00 esta viva pero vacia: no hay que mandar
   a fug ni de donde empezar un modelo.                                 */
int        atsw_puebla_muestra( Atsw *a, const char *muestra,
                                char *why, size_t n );
gchar     *atsw_programa( const char *programa );

/* Guardar un .inp VALIDANDO ANTES, con el comprobador del motor. Si no vale,
   el fichero que habia NO se toca y why dice por que, con su linea. Fuera
   del widget porque es LA regla del editor y ahi se puede probar.       */
int        atsw_guarda_inp( const char *destino, const char *txt,
                            char *why, size_t n );
gboolean   atsw_abre( Atsw *a, const char *path, char *why, size_t n );

/* El resultado de un modelo, releyendo el .out si la huella cambio. */
const AtRes *atsw_resultado( Atsw *a, const char *serie, const char *muestra,
                             const char *id );

/* EL MODELO AL QUE APUNTAN LOS BOTONES si el analista no marca otro: el
 * ELEGIDO si lo hay, y si no el ULTIMO -- la iteracion mas reciente, que es
 * lo que casi siempre se quiere. "" si la serie no tiene ninguno.
 *
 * Es una REGLA, no un asunto de interfaz, asi que vive aqui y se prueba
 * aparte: la rejilla la usa para marcar y los envios para apuntar.    */
const char *atsw_modelo_por_defecto( const Proyecto *p, const char *serie );

/* Lo mismo, DENTRO de una muestra. Lo que la hoja de al lado tenga no es
   candidato: no se compara con esto, asi que tampoco se manda por esto. */
const char *atsw_modelo_por_defecto_en( const Proyecto *p, const char *serie,
                                        const char *muestra );

/* La muestra de la hoja que esta delante. "" es la completa.           */
const char *atsw_muestra_actual( Atsw *a );

/* Rehace las hojas del cuaderno segun las muestras del proyecto.        */
void       atsw_hojas( Atsw *a );

/* LLEVAR UNA SERIE A OTRA MUESTRA: deriva un modelo colgado de <padre> --o
   de los datos-- y le GENERA el .inp con la ventana de <muestra>. No toca
   nada de lo que habia: el .out del padre describe otra estimacion.    */
gboolean   atsw_en_muestra( Atsw *a, const char *serie, const char *padre,
                            const char *muestra, char *why, size_t n );

/* --- piezas compartidas entre los ficheros de la ventana ---------------- */
void       atsw_columna( GtkWidget *tv, const char *titulo, int col );
GtkWidget *atsw_en_scroll( GtkWidget *w );
GtkWidget *atsw_fila( GtkWidget *rejilla, int y, const char *et,
                      const char *valor, const char *tip );

/* UN SELECTOR DE FECHA: periodo y año, acotados a los datos. Una fecha de
   esta escuela son DOS numeros, no uno, y escribirla a mano era la trampa
   que hacia que una ventana no se aplicara sin que nada lo dijera.     */
typedef struct {
   GtkWidget *per, *anio;
   int        freq;
   gboolean   girando;
} AtFecha;

GtkWidget  *atsw_fecha_nueva( AtFecha *F, int freq, int anio, int per,
                              int a1, int a2 );
const char *atsw_fecha_texto( const AtFecha *F, char *out, size_t n );
void       atsw_on_activado( GtkTreeView *tv, GtkTreePath *ruta,
                             GtkTreeViewColumn *col, Atsw *a );
gboolean   atsw_on_click( GtkWidget *tv, GdkEventButton *ev, Atsw *a );
void       a_id( const char *s, char *out, size_t n );
gchar     *atsw_marcada( GtkWidget *tv, int columna );

#endif /* ATSW_GUI_H */
