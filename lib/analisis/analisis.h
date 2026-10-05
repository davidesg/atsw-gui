/*
 * analisis.h -- las ventanas de análisis de un modelo, sin dueño.
 *
 * POR QUE SALEN DE LA MADRE
 *
 * «Anómalos…», «Diagnosis…» y «Sugerir intervención…» nacieron en la madre y
 * recibían un `Atsw *`, o sea que eran de la madre POR ACCIDENTE: de ella
 * sólo usan el manifiesto, una barra de estado y «abre este modelo». Lo demás
 * --la rejilla, las hojas, los menús-- no lo tocan.
 *
 * Y hacen falta en los dos sitios. Estando en fue_gui con el modelo recién
 * estimado delante, volver a la madre a buscarlo en la lista para mirarle los
 * residuos es el camino largo, y el camino largo es el que no se recorre.
 *
 * Las dos salidas que NO valen:
 *
 *   pedírselo a la madre        los dos programas son G_APPLICATION_NON_UNIQUE,
 *                               así que lanzar «atsw_gui --anomalos» arrancaría
 *                               una SEGUNDA madre con el mismo proyecto abierto.
 *   duplicarlas en fue_gui      dos copias divergen. Ya pasó con getExt y con
 *                               inpcheck, y las dos veces costó más arreglarlo
 *                               que haberlo hecho bien.
 *
 * Así que salen aquí, con un anfitrión pequeño.
 *
 * COMO SE COMPILA -- el mismo convenio que lib/fugplot
 *
 * Estos ficheros se compilan UNA VEZ POR PROGRAMA, con el include path de ese
 * programa, porque `preview.h` pide el `previewhost.h` del anfitrión y cada
 * uno tiene el suyo. Es exactamente lo que ya hace fugplot.c con plothost.h.
 * Por eso PreviewApp aparece aquí sin definir: lo define quien compila.
 */

#ifndef ATSW_ANALISIS_H
#define ATSW_ANALISIS_H

#include <gtk/gtk.h>

#include "proyecto.h"
#include "preview.h"      /* trae el previewhost.h del programa que compila */

/* CON QUE SE ABRE UN MODELO DERIVADO.
 *
 * Las dos son puertas a la misma iteración: fue_gui especifica por
 * FORMULARIO --y un formulario sólo puede expresar lo que tiene widgets-- y
 * el editor especifica el FICHERO, que es todo lo que el motor lee.
 *
 * Por defecto fue_gui: lo que un nodo recién derivado necesita a
 * continuación es ESTIMARSE, y fue_gui estima y enseña la diagnosis. El
 * editor es para cuando la especificación pide algo que el formulario no
 * sabe decir.                                                          */
typedef enum {
   AN_CON_FUE = 0,
   AN_CON_EDITOR
} AnHerramienta;

/* QUE PUERTAS SABE ABRIR EL ANFITRION. La madre las dos; fue_gui sólo la
 * suya, porque el editor es de la madre. Lo que no se sepa abrir NO SE
 * OFRECE: un desplegable con una opción que no hace nada es peor que no
 * tener desplegable.                                                    */
#define AN_PUEDE_FUE     0x1u
#define AN_PUEDE_EDITOR  0x2u

/* LO QUE UNA VENTANA DE ANALISIS NECESITA DE QUIEN LA ABRE, y nada más.
 *
 * Los punteros a función pueden ser NULL: entonces ese gesto no se ofrece.
 * Un fue_gui que no sepa abrir otro modelo no enseña el botón de derivar en
 * vez de enseñarlo roto.                                                 */
typedef struct {
   Proyecto   *p;
   GtkWindow  *padre;            /* para transient_for; puede ser NULL    */
   PreviewApp *preview;          /* el anfitrión de lib/preview           */
   void       *dueno;            /* se le devuelve a las funciones        */

   /* La barra de estado del que llama. Obligatoria: una ventana que no
      pueda decir por qué se niega es un botón que no funciona --y eso ya
      costó una sesión entera de «no pasa nada».                        */
   void (*di)( void *dueno, const char *s );

   /* Persistir el manifiesto tras tocarlo. NULL: no se deriva.          */
   int  (*guarda)( void *dueno );

   /* Refrescar lo que el que llama enseñe del proyecto. Puede ser NULL.  */
   void (*refresca)( void *dueno );

   /* Abrir ese modelo para trabajarlo, con la herramienta que se pida.
      NULL: no se ofrece derivar.                                       */
   void (*abre)( void *dueno, const char *serie, const char *muestra,
                 const char *id, AnHerramienta con );

   /* Cerrar --guardando-- lo que el anfitrión tenga abierto de ese modelo.
      Puede ser NULL.

      POR QUE EXISTE. Derivar es una TRANSICION, no una bifurcación de la
      atención: el hijo salió del .inp del padre TAL COMO ESTABA. Dejar al
      padre abierto y editable invita a seguir tocándolo, y entonces la
      procedencia del hijo pasa a ser mentira -- dice que salió de un
      fichero que ya no es ése.                                         */
   void (*cierra)( void *dueno, const char *serie, const char *muestra,
                   const char *id );

   /* Con qué sabe abrir: AN_PUEDE_FUE | AN_PUEDE_EDITOR. En 0 con `abre`
      puesto se entiende que las dos, por compatibilidad.               */
   unsigned puede;
} AnHost;

/* Los residuos con sus anómalos, sobre el gráfico de fue, y la calibración.
 * Ver DISENO-anomalos.md.                                                */
void an_anomalos( const AnHost *h, const char *serie, const char *muestra,
                  const char *id );

/* El resumen de la diagnosis del .out. Ver DISENO-diagnosis-univariante.md. */
void an_diagnosis( const AnHost *h, const char *serie, const char *muestra,
                   const char *id );

/* LA GANANCIA DE LAS INTERVENCIONES: ¿el suceso dejó algo para siempre o
 * revirtió? No es diagnosis --la diagnosis juzga lo que hay-- sino una
 * hipotesis sobre la naturaleza del incidente, contrastada.           */
void an_ganancia( const AnHost *h, const char *serie, const char *muestra,
                  const char *id );

/* Qué forma pide cada suceso marcado. La abre an_anomalos, no el anfitrión.
 * Ver DISENO-intervencion.md.                                            */
#define AN_MAX_EXT  16
#define AN_MAX_SUC  64
#define AN_VENTANA_Z 64

typedef struct {
   int    desde, hasta;
   int    per, anno;
   char   fecha[16];
   int    obs[AN_MAX_EXT];
   double z[AN_MAX_EXT];
   int    next;

   int    base, nwin;
   double zwin[AN_VENTANA_Z];
   double umbral;
} AnSuceso;

void an_sugerir( const AnHost *h, const char *serie, const char *muestra,
                 const char *id, int d, int D, int freq,
                 const AnSuceso *suc, int ns );

/* EL IDENTIFICADOR: art propone, el analista elige (an_identifica.c,
 * docs/ESTUDIO-identificador.md). Dos puntos del proceso:
 *
 *   an_identifica_serie     E2, los gráficos de identificación: una serie con
 *                           su lambda (0 log, 1 niveles), d y D. `id` es el
 *                           nodo del que se deriva (el de datos).
 *   an_identifica_residuos  E3/E4, los residuos de un modelo, leídos de su
 *                           .out; derivar AÑADE el candidato como un factor
 *                           más, sobre su .pre.                            */
typedef struct {
   const double *x;     /* la serie en NIVEL, x[0..n-1]                  */
   int           n;
   int           freq, per, anio;   /* frecuencia y fecha de x[0]       */
   double        lam;
   int           d, D;
   const char   *que;   /* para el título; puede ser NULL               */
} AnSerie;

void an_identifica_serie( const AnHost *h, const char *serie, const char *muestra,
                          const char *id, const AnSerie *s );
void an_identifica_residuos( const AnHost *h, const char *serie, const char *muestra,
                             const char *id );

/* E1, los datos: la misma ventana con la transformación EDITABLE (log o
 * niveles, d, D) y los contrastes --la F estacional, ADF y KPSS-- dichos
 * con lo que dicen. `s` trae la serie y la transformación de partida. */
void an_identifica_datos( const AnHost *h, const char *serie, const char *muestra,
                          const char *id, const AnSerie *s );

/* --- CUANDO SE PUEDE ANALIZAR, Y SI NO, POR QUE ------------------------- */

/* UN BOTON SE ENCIENDE CUANDO EXISTE SU INSUMO, Y NO ANTES. No es una
 * restricción: un botón encendido que no puede hacer nada útil enseña el
 * camino equivocado.
 *
 * Y «AL DIA» NO ES «EXISTE». Un .out más antiguo que su .inp es la diagnosis
 * de OTRA especificación -- el analista tocó el modelo y no lo volvió a
 * estimar. Mirarlo es leer el informe de un modelo que ya no existe, y eso
 * no da error: da una conclusión sobre otra cosa.
 *
 * La regla vive aquí y no en cada ventana para que sea LA MISMA en la madre
 * y en fue_gui. Dos copias de una regla son dos reglas.                */
typedef enum {
   AN_LISTO = 0,      /* hay .out y es posterior al .inp                */
   AN_SIN_CLAVE,      /* ese fichero no es un modelo de este proyecto   */
   AN_SIN_OUT,        /* todavía no se ha estimado                      */
   AN_OUT_VIEJO       /* el informe es de antes que la especificación   */
} AnEstado;

/* porque[n] recibe la frase que va al globo del botón apagado. */
AnEstado an_estado( const Proyecto *p, const char *serie, const char *muestra,
                    const char *id, char *porque, size_t n );

/* LOS NOMBRES DE LOS DETERMINISTAS de ese modelo, leidos de su .inp.
 *
 * El .out no los trae --dice «Omegas for deterministic variable 12»-- y sin
 * ellos todo lo que se diga de una intervencion la nombra por su numero, que
 * obliga al analista a contar lineas en el fichero.
 *
 * Devuelve cuantos (0 si no se pudo), y el vector queda en det[][]. */
#define AN_MAX_DET  64
#define AN_LDET     128

int an_deterministas( const Proyecto *p, const char *serie,
                      const char *muestra, const char *id,
                      char det[][AN_LDET], int max );

/* Decirle algo a quien abrió la ventana. Sin anfitrión, al log: un aviso
 * que no se ve es un botón que no funciona.                             */
void an_di( const AnHost *h, const char *fmt, ... ) G_GNUC_PRINTF( 2, 3 );

/* El directorio donde estas ventanas dejan sus EPS, creado. g_free.     */
gchar *an_cache_dir( void );

/* La ruta base --sin extensión-- de un fichero de trabajo de este modelo.
 * Lleva la clave dentro porque lib/preview reutiliza la ventana POR RUTA:
 * con un nombre fijo, dos modelos compartirían ventana. g_free.        */
gchar *an_fichero( const char *que, const char *serie, const char *muestra,
                   const char *id );

#endif /* ATSW_ANALISIS_H */
