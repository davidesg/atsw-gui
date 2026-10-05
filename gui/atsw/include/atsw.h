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
#include "dictamen.h"
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

/* LOS CASOS (docs/DISENO-casos.md §4): la lista de la izquierda, las
   entradas y el arbol de corridas de la derecha.                       */
enum { CA_ID, CA_TITULO, CA_ELEGIDA, CA_MARCA, CA_GLOBO, CA_N };
enum { EN_POS, EN_SERIE, EN_MODELO, EN_PRE, EN_HOY, EN_NOTA, EN_N };
enum { CO_ID, CO_ESTRELLA, CO_RAZON, CO_LOGL, CO_PUERTA, CO_ESTADO, CO_GLOBO, CO_N };

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

   /* EL DICTAMEN, resumido: el peor de los cinco bloques y una linea por
      bloque para el globo. La rejilla ya enseña Q y p; esto dice QUE
      SIGNIFICAN JUNTOS.                                                */
   int      peor;                 /* DxEstado                              */
   char     dx[256];

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
   /* LA HOJA DELANTE, de un campo: el widget no lo sabe todavia cuando
      "switch-page" nos llama. Ver atsw_muestra_actual.                */
   int        hoja_actual;
   GtkWidget *ver_cuenta;         /* veredicto: cuantas cosas hay           */
   GtkWidget *ver_ojo;            /* veredicto: LO QUE HAY QUE MIRAR        */
   GtkWidget *estado;             /* la barra de abajo                      */
   GtkWidget *b_fue, *b_fug, *b_drtran, *b_nuevo, *b_iterar, *b_elegir, *b_razon;

   /* LOS CASOS. La derecha es una PILA: la rejilla de modelos de la serie
      marcada, o la vista del caso marcado. Las dos a la vez no caben, y
      marcar una cosa u otra en la izquierda es ya decir cual se mira.  */
   GtkWidget *pila;
   GtkWidget *l_casos;
   GtkWidget *ver_desfase, *ver_nota;   /* las dos lineas del veredicto   */
   /* LAS CORRIDAS DE ANTES (DISENO-casos §5): su linea y su boton, en una
      caja que solo se ve si las hay.                                   */
   GtkWidget *caja_legado, *ver_legado, *b_convertir;
   GtkWidget *c_cabeza, *c_entradas, *c_corridas;
   GtkWidget *b_nuevo_caso, *b_c_drtran, *b_c_elegir, *b_c_razon,
             *b_c_razon_caso, *b_c_derivar, *b_c_borrar, *b_c_borrar_caso;
   char       caso[PR_ID];        /* el caso marcado                        */
   gboolean   viendo_caso;        /* la derecha enseña el caso, no la serie */

   Proyecto  *p;
   gboolean   hay;

   /* LA HUELLA DEL MANIFIESTO: tamaño y fecha, como la de los .out. Si el
      fichero cambia por fuera --otra madre, un agente, un editor-- hay que
      releerlo; si no, no se toca nada.                                 */
   long       p_tam, p_mtime;

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

/* UN SUCESO MARCADO en la ventana de anomalos, listo para que se le lea la
   forma. Lleva sus EXTREMOS --no su rango-- porque la forma la decide la
   firma que dejan, no cuantos periodos dura; y lleva la fecha ya leida del
   .out, que es la que el motor usa para fechar la intervencion.        */
/* Las ventanas de analisis --anomalos, diagnosis, sugerir-- viven en
   lib/analisis porque hacen falta tambien en fue_gui. El anfitrion que la
   madre les da esta en include/anfitrion.h, aparte: analisis.h arrastra
   preview.h, que arrastra previewhost.h, que arrastra ESTE fichero.  */

/* EL LINAJE: la cadena entera y lo que cada nodo DEBE. Enseña lo que falta
   --sin estimar, sin razon, sin elegido-- porque eso es lo que dentro de
   seis meses hace irreproducible un analisis.                          */
void       atsw_linaje( Atsw *a );

/* «Editar…» una serie: descripcion, unidades, fuente, url, bajada, notas.
   Nada de esto toca un numero -- son los campos que el .inp no puede
   llevar y que hacen falta para volver al analisis meses despues.      */
void       atsw_serie_edita( Atsw *a, const char *serie );

/* «Información del proyecto…»: id, título, analista. La raíz y la ruta se
   enseñan y no se editan -- moverlas es mover el proyecto entero.      */
void       atsw_proyecto_edita( Atsw *a );

/* LA DIAGNOSIS de un modelo, pintada. Todo lo que se juzga está en
   lib/dictamen; esta ventana sólo le pone color y un botón de exportar. */

/* LANZAR UNO DE LOS GUIs con este proyecto abierto. La madre no los
   absorbe: les da el contexto que no tenian.                           */
void       atsw_lanza( Atsw *a, const char *programa, const char *fichero );

/* Lo mismo, con una opcion antes del fichero: fue_gui con «--prever»
   arranca el ciclo de prevision al abrir.                              */
void       atsw_lanza_con( Atsw *a, const char *programa, const char *opcion,
                           const char *fichero );

/* drtran_gui con un CASO: «--proyecto P --caso C [--corrida c]». corrida
   puede ser NULL o "": entonces drtran parte de la elegida, o de nada. */
void       atsw_lanza_caso( Atsw *a, const char *programa, const char *caso,
                            const char *corrida );

/* --- los casos (docs/DISENO-casos.md) ----------------------------------- */

/* EL SHA256 DE UN FICHERO, en hexadecimal (g_free), o NULL si no se lee. El
   mismo calculo que drtran_gui: el fichero entero, tal cual.           */
gchar     *atsw_sha_de( const char *path );

/* COMO ESTA EL .pre DE UNA ENTRADA respecto del alta.                  */
typedef enum {
   AT_PRE_IGUAL = 0,              /* el mismo sha256                        */
   AT_PRE_CAMBIO,                 /* otro: lo estimado ya no corresponde    */
   AT_PRE_FALTA,                  /* el fichero no esta                     */
   AT_PRE_SIN_HASH                /* el alta no lo guardo: no se sabe       */
} AtPre;

AtPre      atsw_entrada_pre( const Proyecto *p, const PrCaso *c, int i );

/* LOS DOS DESFASES (DISENO-casos §2.4), con las entradas que lo causan en
   cuales ("ipc/m01 (cambió), wti/m03 (no está)"). Devuelven cuantas.
     desfases  el .pre cambio o no esta: lo estimado YA NO CORRESPONDE;
     notas     la serie tiene hoy otro elegido: INFORMATIVO, a menudo
               deliberado --art aconseja otro modelo para lo multivariante.*/
int        atsw_caso_desfases( const Proyecto *p, const PrCaso *c,
                               char *cuales, size_t n );
/* LAS CORRIDAS DE drtran REGISTRADAS COMO MODELOS (DISENO-casos §5).
   Antes de los casos, drtran_gui daba de alta cada estimacion con
   pr_deriva sobre la serie de salida: un «modelo» que tiene .out y .dag y
   no tiene .inp. Devuelve cuantos y deja sus indices en p->m en idx.  */
int        atsw_legados( const Proyecto *p, int idx[], int max );

/* CONVERTIRLAS EN CASOS: cada una pasa a ser una corrida del caso de sus
   entradas, con su linaje, su razon y su elegida; sus ficheros se mueven a
   _casos/ y el modelo se quita. La que no se puede se deja como estaba y
   se dice por que. Guarda el manifiesto. Devuelve cuantas convirtio; lo
   que paso, en informe.                                                */
int        atsw_convierte_legados( Atsw *a, char *informe, size_t n );

/* La linea del veredicto con su boton «Convertir en casos…». */
GtkWidget *atsw_legado_caja( Atsw *a );

int        atsw_caso_notas( const Proyecto *p, const PrCaso *c,
                            char *cuales, size_t n );

/* LA VENTANA COMUN (BUG-2): la regla de fuepre_check_alignment --misma
   frecuencia, mismo calendario, misma fecha final-- y la de drtran, que
   pide ademas el mismo numero de observaciones. pre[0..n-1] son los .pre;
   0 si se pueden cruzar, si no != 0 con el motivo en why.             */
int        atsw_ventana_comun( const char *const pre[], int n,
                               char *why, size_t nw );

/* La seccion CASOS de la izquierda, la vista del caso para la derecha, y
   su repintado. Ver casos_gui.c.                                       */
void       atsw_casos_panel( Atsw *a, GtkWidget *izq );
GtkWidget *atsw_caso_vista( Atsw *a );
void       atsw_pinta_casos( Atsw *a );

/* «Nuevo caso…», o «Derivar caso…» si deriva_de no es NULL.            */
void       atsw_caso_nuevo( Atsw *a, const char *deriva_de );

/* «Abrir en drtran» con el programa dado: el caso marcado y, si hay una
   corrida marcada, esa.                                                */
void       atsw_caso_lanza( Atsw *a, const char *programa );

/* Un texto en una linea, en un dialogo modal. TRUE si se acepto.       */
gboolean   atsw_pide_texto( Atsw *a, const char *titulo, const char *aviso,
                            const char *previo, char *out, size_t n );

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

/* RELEE EL MANIFIESTO SI HA CAMBIADO POR FUERA. TRUE si lo releyó.
   Un manifiesto roto NO se traga: se dice y se deja el que hay en memoria,
   que es el bueno.                                                     */
gboolean   atsw_relee( Atsw *a, char *why, size_t n );

/* Escribe el manifiesto Y APUNTA SU HUELLA. Lo segundo no es un detalle: sin
   ello, el proximo foco releeria nuestra propia escritura creyendo que la
   hizo otro -- y lo diria.                                             */
int        atsw_guarda( Atsw *a, PrError *e );

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
