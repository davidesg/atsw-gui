/*
 * gui.h -- el contexto de mtram.
 */

#ifndef DRTRAN_GUI_H
#define DRTRAN_GUI_H

#include <gtk/gtk.h>
#include "series.h"
#include "netfile.h"
#include "slots.h"
#include "verdict.h"
#include "engine.h"
#include "outdiag.h"
#include "outfcst.h"
#include "proyecto.h"
#include "fugdraw.h"

/* El directorio de trabajo, bajo la cache del usuario: los .dag, .cns, .out,
 * los residuos y los EPS que se ensenan. Se llama COMO EL PROGRAMA, y el
 * programa se llama como su motor -- "mtram" es el servidor MCP de
 * drtran-python, y en esta suite un nombre senala una cosa sola.       */
#define GUI_CACHE "drtran_gui"

#define IDENT_MAX_LAGS 64

/* La pantalla de identificacion.
 *
 * UNA FILA POR ENTRADA CANDIDATA, no por enlace. Identificar es precisamente
 * decidir CUALES merecen estar en la red, asi que se calcula la CCF
 * preblanqueada de la salida contra TODAS, y se comparan de un vistazo. Con un
 * combo habia que ir una por una recordando de memoria lo que decia la
 * anterior -- y eso es justo la decision que se toma aqui.               */
typedef struct {
    int      serie;           /* 1..n-1: la entrada, en indice de Conjunto  */
    gboolean vale;
    int      nlags, n;
    double   ccf[2 * IDENT_MAX_LAGS + 1];   /* k = i - nlags               */
    double   nu [2 * IDENT_MAX_LAGS + 1];
    double   banda;           /* 2/sqrt(n)                                  */
    double   Q;               /* el portmanteau de Hosking                  */
    int      df;

    /* lo que se lee del grafico */
    int      b, s, kmax, ultimo, neg;
} IdentUno;

typedef struct {
    GtkWidget *lista;
    GtkWidget *ver_tran;      /* veredicto: la transferencia                */
    GtkWidget *ver_exo;       /* veredicto: la exogeneidad                  */
    GtkWidget *s_lags;

    IdentUno   u[GUI_MAX_SER];
    int        nent;          /* cuantas entradas candidatas                */
    int        marcada;       /* indice en u[], -1 si ninguna               */
    int        nlags;         /* el que se pide; 0 = el que elige el motor   */
} Ident;

/* La pantalla de la red: el .dag. El motor resuelve el sistema por recursion
 * en orden topologico, asi que la red decide si el modelo se puede estimar.
 * Un ciclo no es un error de sintaxis: es un sistema simultaneo, y ese es de
 * drvarma, no de aqui.                                                    */
typedef struct {
    GtkWidget *lista;         /* los enlaces                                */
    GtkWidget *ver_topo;      /* veredicto: aciclica y su orden, o el ciclo */
    GtkWidget *ver_forma;     /* veredicto: red o estrella, y las sueltas   */

    NetLink    lnk[NET_MAX_LINK];
    int        n;
    gchar     *path;          /* de donde se leyo, o donde se guardo        */
} Red;

/* La pantalla del modelo: el .cns. La tabla de slots ES la forma del modelo
 * --tantos omega como diga s, tantos phi como el .pre deje libres-- asi que la
 * pantalla no es un editor de texto: es una vista de esa tabla.          */
typedef struct {
    GtkWidget *lista;         /* los parametros, en un ARBOL por grupos     */
    GtkWidget *ver_cuenta;    /* veredicto: cuantos hay y cuantos libres    */
    GtkWidget *ver_ojo;       /* veredicto: LO QUE HAY QUE MIRAR            */
    GtkWidget *c_solo;        /* solo lo restringido                        */

    gboolean   solo;          /* la casilla                                 */

    SlotTable  st;
    gboolean   vale;
    int        perdidas;      /* restricciones que no cupieron al rehacer   */
    gboolean   orden_cambio;  /* las series se movieron: los nombres ya no
                                 significan lo mismo                        */
    gchar     *path;
} Modelo;

/* La pantalla de estimacion: la unica que LANZA EL MOTOR como programa. */
typedef struct {
    GtkWidget *boton;         /* Estimar                                    */
    GtkWidget *b_parar;       /* Detener                                    */
    GtkWidget *orden;         /* la orden, en una linea COPIABLE            */
    GtkWidget *titulo;        /* el estado, encima de la salida             */
    GtkWidget *barra;         /* en PULSO: no se puede saber el avance      */
    GtkWidget *salida;        /* la CONSOLA: lo que dice el motor, en vivo  */
    GtkWidget *informe;       /* el .out del modelo, aqui dentro            */
    GtkWidget *libreta;       /* las dos pestañas                           */
    GtkWidget *ver_fin;       /* veredicto: como acabo                      */
    GtkWidget *ver_que;       /* veredicto: que se estima                   */
    GtkWidget *motor;         /* que drtran, y de cuando                    */
    GtkWidget *c_diag;        /* Diagonal: es un MODO, no una opcion        */

    EngineJob *trabajo;       /* la corrida en marcha, para poder pararla   */
    guint      pulso;         /* el temporizador de la barra                */
    gboolean   corriendo;
    gboolean   diagonal;      /* -0 : sin transferencia (homologacion)      */
    gchar     *nota;          /* lo que decir encima de la salida del motor:
                                 a donde va la corrida (el caso, la cache) */
    gboolean   cast_resta;    /* -S : el cast antiguo                       */
    gboolean   traza;         /* -v                                         */

    /* Lo que el estimador MANTIENE del .pre en vez de reestimarlo al juntar
     * las ecuaciones. Vive aqui y no en Modelo porque dice COMO SE ESTIMA,
     * no QUE ES EL MODELO -- igual que el cast. Pero cambia la cuenta de
     * parametros, asi que la pagina Modelo la lee de aqui.             */
    gboolean   fix_N;         /* -N  el ARMA del ruido de la SALIDA         */
    gboolean   fix_X;         /* -X  el ARMA de las ENTRADAS                */
    gboolean   fix_D;         /* -D  los deterministas de la salida         */
    gboolean   fix_E;         /* -E  los deterministas de las entradas      */
    gboolean   fix_M;         /* -M  las medias                             */
    gchar     *out_path;
    VerdictInfo v;
} Estima;

/* La pantalla de diagnosis. Los residuos son los de ESTA corrida: TASTE tenia
 * una sola ranura 'RESIDUOS' (TASTECTV.PAS:475) y por eso no se podian comparar
 * dos modelos. Aqui cada .out trae los suyos.                            */
/* Las paginas, en el orden del metodo. La diagnosis las nombra porque su
 * veredicto dice A DONDE VOLVER.                                        */
enum { PG_SERIES, PG_IDENT, PG_RED, PG_MODELO, PG_ESTIMA, PG_DIAG, PG_PREV };

/* La pagina Diagnosis.
 *
 * Seis pestañas EN EL ORDEN DE LAS PREGUNTAS del analista, que va de lo que
 * INVALIDA el modelo a lo que lo matiza:
 *
 *   1 exogeneidad   ¿transferencia, o esto es un VARMA?
 *   2 adecuacion    ¿la forma (b,r,s) agota la relacion?
 *   3 ajuste        ¿aportan algo las transferencias? (LR contra el diagonal)
 *   4 residuos      ¿son ruido blanco?
 *   5 modelo        los parametros, con su d.t. -- y de donde viene cada uno
 *   6 salida        el .out entero
 *
 * Si (1) falla, las otras cinco no importan.                            */
typedef struct {
    GtkWidget *libreta;
    GtkWidget *l_exo, *l_ade, *l_res, *l_par, *l_aju;  /* las cinco listas  */
    GtkWidget *l_lr;          /* el LR, encima de la tabla de ajuste        */
    GtkWidget *t_out;
    GtkWidget *ver_global, *ver_ojo;
    GtkWidget *b_volver, *l_ecu;

    int        ir_a;          /* la pagina a la que hay que volver, o -1    */
    int        actual;        /* la ecuacion en la que se esta, 1..n        */

    Diagnosis  d;
    OdResiduos res;           /* los residuos, como NUMEROS                 */
    OdParams   par;           /* la tabla con las desviaciones tipicas      */
    gboolean   vale, hay_res, hay_par;
    gchar     *path;          /* el .out del que salio                      */

    /* EL BASELINE: el modelo DIAGONAL. Contesta "¿aportan algo las
     * transferencias?" de tres formas, y la escuela cierra cada caso con las
     * tres (Brajin 6.4, Muñoz 6.4.1):
     *
     *   LR = 2(logL - logL_base) ~ chi2(k)   -- el diagonal esta ANIDADO
     *   la desviacion tipica residual, que pasa de una a otra
     *   el R² de Brajin (A.28), que pasa de una a otra
     *
     * EL R² SI TIENE SENTIDO, Y ES ESTE. Va sobre la serie ESTACIONARIA
     *
     *     R² = 1 - SUM (a_t - abar)² / SUM (w_t - wbar)²,  w = nabla^d z
     *
     * y no sobre el nivel: sobre el nivel de una I(1) sale cerca de 1 por
     * construccion y no dice nada. El denominador es PROPIEDAD DE LOS DATOS
     * --no lleva parametros-- asi que es el mismo en las dos estimaciones y
     * por eso los dos R² se pueden comparar. Deja de ser comparable entre d
     * distintas, donde w_t es otra variable: es una TRANSICION entre dos
     * ajustes de una especificacion, nunca una nota para ordenar modelos.  */
    double     logl_base;
    int        npar_base;
    gboolean   hay_base;
    char       base_que[80];
    OdResiduos res_base;      /* los residuos del diagonal: de ahi el R² y  */
    gboolean   hay_res_base;  /* la d.t. "univariante" de cada ecuacion     */

    /* La corrida en marcha es la del baseline, pedida desde aqui: al acabar
     * se fija sola y se vuelve a dejar el modo como estaba. Sin esto habia
     * que ir a Estimacion, marcar Diagonal, estimar, volver, fijar, ir otra
     * vez, desmarcar y estimar -- ocho pasos para un boton.            */
    gboolean   pidiendo_base;
} Diag;

/* La pantalla de prevision y evaluacion.
 *
 * Las bandas de la prevision son TEORICAS; la evaluacion fuera de muestra es
 * EMPIRICA. La pantalla existe para no dejar confundirlas -- y para poder
 * comparar dos modelos, que es lo que TASTE no podia hacer.            */
typedef struct {
    GtkWidget *lista;         /* el error por horizonte                     */
    GtkWidget *texto;         /* la prevision y lo que significa            */
    GtkWidget *b_calc;        /* Calcular: LANZA EL MOTOR desde aqui        */
    GtkWidget *c_prever, *c_eval, *s_hor, *s_win;

    Forecast   f;
    gboolean   vale;

    gboolean   prever, evaluar;
    int        horizonte;     /* -f L                                       */
    int        ventana;       /* -estwin E, 0 = sin ventana                 */

    OfEval     ref;           /* la evaluacion guardada, para comparar      */
    gboolean   tiene_ref;
    char       ref_que[80];
} Prev;

typedef struct {
    GtkWidget *ventana_p;     /* la ventana principal                       */
    GtkWidget *lista;         /* las series, en orden                       */
    GtkWidget *ver_ventana;   /* veredicto: la ventana comun, UNA linea     */
    GtkWidget *ver_oper;      /* veredicto: los operadores, UNA linea       */
    GtkWidget *estado;        /* la barra de abajo                          */
    GtkWidget *libro;         /* el cuaderno: la diagnosis lo usa para volver */
    Conjunto   c;             /* las series cargadas                        */
    gboolean   recolocando;   /* repintando la lista: no leer su orden      */
    Red        red;           /* la pantalla de la red                      */
    Ident      id;            /* la pantalla de identificacion              */
    Modelo     mod;           /* la pantalla del modelo                     */
    Estima     est;           /* la pantalla de estimacion                  */
    Diag       dia;           /* la pantalla de diagnosis                   */
    Prev       prev;          /* la pantalla de prevision                   */

    /* EL PROYECTO, si se abrio con --proyecto. Sin el, el programa funciona
     * como siempre --en la cache, con nombres fijos-- y entonces NO CABEN DOS
     * MODELOS: es la ranura unica RESIDUOS de TASTE. Con el, cada estimacion
     * es una CORRIDA con su nombre y su linaje.                        */
    Proyecto  *proy;
    gboolean   hay_proy;
    char       corrida[PR_ID];   /* el id de la corrida en curso           */
    char       previa[PR_ID];    /* la anterior: el PADRE de la siguiente   */

    /* EL CASO (docs/DISENO-casos.md): lo que se cruza, con el sha256 del .pre
     * de cada entrada. Las corridas cuelgan de el, no de una serie. Vacio
     * hasta que se abre con --caso o el alta automatica lo crea.        */
    char       caso[PR_ID];
    char       corrida_ini[PR_ID];  /* la de --corrida: de donde se parte  */
    gboolean   caso_desfasado;   /* algun .pre ya no es el del alta         */
} Mtram;

/* --- el proyecto (proyecto_gui.c) --------------------------------------- */

/* Abre el manifiesto. Devuelve FALSE y pone el motivo en why.            */
gboolean mtram_proyecto_abre(Mtram *m, const char *path, char *why, size_t n);

/* La ruta de un artefacto de la corrida en curso. sufijo lleva su punto o su
 * guion: ".out", ".dag", "_res.txt".
 *
 * SIN CASO devuelve el nombre FIJO de siempre en la cache, que es lo que
 * hace que no quepan dos modelos. CON caso, el nombre de cortesia de la
 * corrida: <raiz>/_casos/<caso>/work/<caso>_<id><sufijo>. Nueva; g_free. */
gchar *mtram_artefacto(Mtram *m, const char *sufijo);

/* Carga el caso de --caso: sus entradas EN SU ORDEN, cada una del .pre de su
 * modelo, comprobando los sha256; y la red y las restricciones de la corrida
 * de --corrida, o de la elegida. FALSE con el motivo en why. Si un .pre
 * cambio, carga igual y lo dice en why (y caso_desfasado queda puesto). */
gboolean mtram_caso_carga(Mtram *m, char *why, size_t n);

/* Abre una corrida nueva DEL CASO y registra el LINAJE sin preguntar.
 *
 * Sin caso, con proyecto (drtran_gui lanzado a mano): si TODAS las series
 * cargadas son modelos del proyecto, en la misma muestra y con la ventana
 * comun, se busca el caso con esas entradas o se da de alta (decision del
 * analista: alta automatica). Si el caso esta desfasado, la corrida va a un
 * caso NUEVO derivado con los .pre de hoy: nunca se reescribe nada.
 *
 * Sin proyecto, o si alguna serie no es del proyecto, no hace nada --la
 * cache de siempre-- y devuelve TRUE; en el segundo caso why dice por que.
 * FALSE con el motivo en why si no se pudo registrar.                   */
gboolean mtram_corrida_nueva(Mtram *m, char *why, size_t n);

/* Guarda el manifiesto. Sin proyecto, no hace nada.                     */
gboolean mtram_proyecto_guarda(Mtram *m, char *why, size_t n);

GtkWidget *mtram_window_new(GtkApplication *app, Mtram *m);

/* Refresca TODAS las paginas. La llama cualquiera que cambie el estado
 * compartido --las series, la red, el modelo-- porque lo de aguas abajo
 * depende de ello: añadir un enlace cambia lo que Estimacion va a lanzar y
 * lo que Modelo enseña.                                                 */
void mtram_refresca(Mtram *m);

/* ------------------------------------------------------------------------ */
/* Presentacion, compartida por las paginas                                  */
/*                                                                           */
/* Un VEREDICTO es una linea de altura FIJA con un punto de color. Existe    */
/* porque los marcos que crecian con los datos dejaban la lista en dos filas */
/* --y en Red, ademas, daban saltos mientras se editaba--. El detalle se     */
/* pide con un panel; el veredicto esta siempre. Ver DISENO-interfaz.md.     */
/* ------------------------------------------------------------------------ */

#define MT_VERDE  "#1a7f37"
#define MT_AMBAR  "#9a6700"
#define MT_ROJO   "#b3261e"

void mtram_verdicto(GtkWidget *w, const char *color, const char *fmt, ...)
     G_GNUC_PRINTF(3, 4);

GtkWidget *mtram_popover(GtkWidget *ancla, const char *txt);
void       mtram_popover_mostrar(GtkWidget *ancla, const char *txt);

/* Para un panel que no es solo texto. *caja recibe la caja que hay que llenar;
 * al acabar, mtram_popover_popup(pop) -- y NO gtk_popover_popup a secas: ver
 * el porque en main_window.c.                                            */
GtkWidget *mtram_popover_caja(GtkWidget *ancla, GtkWidget **caja);
void       mtram_popover_popup(GtkWidget *pop);

/* Texto de ANCHO FIJO. Casi todo lo que va en estos paneles son tablas hechas
 * con espacios: con fuente proporcional se descuadran y dejan de leerse.  */
GtkWidget *mtram_mono(const char *txt);

/* red.c */
GtkWidget *red_pagina_new(Mtram *m);
void       red_refresca(Mtram *m);
/* El dialogo de un enlace, para poder tocar (b,r,s) desde la pagina Modelo:
 * la estructura de la transferencia se decide alli tanto como aqui.     */
gboolean   red_edita_enlace(Mtram *m, int k);
/* Lee un .dag sin dialogo (el cuerpo de «Abrir»). TRUE si se leyo. */
gboolean   red_lee(Mtram *m, const char *path);

/* modelo.c */
GtkWidget *modelo_pagina_new(Mtram *m);
void       modelo_refresca(Mtram *m);
/* Lee un .cns sin dialogo. Si no vale, la tabla queda como estaba. */
gboolean   modelo_lee(Mtram *m, const char *path);

/* estima.c */
GtkWidget *estima_pagina_new(Mtram *m);
void       estima_refresca(Mtram *m);

/* LANZA EL MOTOR, desde donde sea. La orden la arman entre varias paginas
 * --Prevision pone -f y -C, Diagnosis quiere el -e-- asi que obligar a ir a
 * la pagina 5 a pulsar un boton es contrario a la regla: cada pantalla
 * fabrica lo que la siguiente pide, y si una necesita una corrida, la pide.
 * Devuelve TRUE si arranco.                                            */
gboolean   estima_lanzar(Mtram *m);

/* prevision.c */
GtkWidget *prevision_pagina_new(Mtram *m);
void       prevision_refresca(Mtram *m);
void       prevision_desde(Mtram *m, const char *path);

/* diagnosis.c */
GtkWidget *diagnosis_pagina_new(Mtram *m);
void       diagnosis_refresca(Mtram *m);
void       diagnosis_desde(Mtram *m, const char *path);

/* pagina.c -- la pagina del modelo: grafico (puede ser NULL) arriba y las
 * ecuaciones estimadas de la serie i debajo, en el PDF pdf. 0 si pudo; si
 * no, el motivo en why.                                                  */
int        pagina_modelo(Mtram *m, int i, FDFig *grafico, const char *pdf,
                         char *why, size_t n);

/* identifica.c */
GtkWidget *identifica_pagina_new(Mtram *m);
void       identifica_refresca(Mtram *m);

#endif
