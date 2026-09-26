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

   /* Abrir ese modelo para trabajarlo. NULL: no se ofrece derivar.       */
   void (*abre)( void *dueno, const char *serie, const char *muestra,
                 const char *id );
} AnHost;

/* Los residuos con sus anómalos, sobre el gráfico de fue, y la calibración.
 * Ver DISENO-anomalos.md.                                                */
void an_anomalos( const AnHost *h, const char *serie, const char *muestra,
                  const char *id );

/* El resumen de la diagnosis del .out. Ver DISENO-diagnosis-univariante.md. */
void an_diagnosis( const AnHost *h, const char *serie, const char *muestra,
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
