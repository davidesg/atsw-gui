/*
 * analisis.c -- la diagnosis y los anómalos, desde fue_gui.
 *
 * POR QUE AQUI TAMBIEN. Estando con el modelo recién estimado delante,
 * volver a la madre y buscarlo en la lista para mirarle los residuos es el
 * camino largo, y el camino largo es el que no se recorre. Las ventanas son
 * LAS MISMAS --lib/analisis--, no una copia: dos copias divergen.
 *
 * Y QUE MODELO ES EL QUE HAY DELANTE. fue_gui trabaja sobre un FICHERO; el
 * proyecto trabaja sobre una CLAVE. La ruta se compone igual que al guardar
 * --el espacio de trabajo más el nombre-- y la clave se le PREGUNTA al
 * manifiesto, porque el nombre del fichero es cortesía y de él no se deduce
 * nada. Si el fichero no está en el proyecto no hay clave, y los botones se
 * quedan apagados diciendo por qué: fuera del proyecto no hay linaje que
 * cuidar.
 */

#include <string.h>

#include "fue_context.h"
#include "analisis.h"

Proyecto   *fue_proyecto( void );
int         fue_proyecto_relee( void );
const char *fue_raiz_proyecto( void );
void        fue_abre_al_arrancar( FueContext *ctx, const char *path );

/* La ruta del .inp que la ventana tiene delante. Se compone igual que en
 * file_io.c al guardar, porque es EL MISMO fichero.                     */
static gchar *ruta_actual( FueContext *ctx )
{
    gchar      *ws, *ruta, *fichero;
    const char *nombre;

    if ( !ctx || !ctx->workspace_file_chooser || !ctx->input_name_entry )
        return NULL;
    ws = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(ctx->workspace_file_chooser) );
    if ( !ws ) return NULL;
    nombre = gtk_entry_get_text( GTK_ENTRY(ctx->input_name_entry) );
    if ( !nombre || !*nombre ) { g_free( ws ); return NULL; }

    fichero = g_str_has_suffix( nombre, ".inp" )
            ? g_strdup( nombre ) : g_strdup_printf( "%s.inp", nombre );
    ruta = g_build_filename( ws, fichero, NULL );
    g_free( fichero );
    g_free( ws );
    return ruta;
}

int fue_modelo_actual( FueContext *ctx, char *serie, size_t ns,
                       char *muestra, size_t nm, char *id, size_t nid )
{
    Proyecto *p = fue_proyecto();
    gchar    *ruta;
    int       r;

    if ( serie && ns ) serie[0] = '\0';
    if ( id && nid )   id[0] = '\0';
    if ( !p ) return 1;

    ruta = ruta_actual( ctx );
    if ( !ruta ) return 1;
    r = pr_de_ruta( p, ruta, serie, ns, muestra, nm, id, nid );
    g_free( ruta );
    return r;
}


/* ------------------------------------------------------------------------ */
/* El anfitrión que fue_gui le da a lib/analisis                             */
/* ------------------------------------------------------------------------ */

static void fg_di( void *d, const char *s )
{
    FueContext *ctx = (FueContext *) d;

    if ( ctx && ctx->status_label ) gtk_label_set_text( GTK_LABEL(ctx->status_label), s );
}

/* GUARDAR EL MANIFIESTO DESDE AQUI.
 *
 * La primera version dejaba esto a NULL --"derivar lo lleva la madre"-- y
 * el resultado fue un fichero .inp en disco que el proyecto no conocia: un
 * HUERFANO. El boton parecia no hacer nada y en realidad hacia daño.
 *
 * Y el bucle que se rompia era el bueno: estimar aqui, mirar los residuos
 * aqui, y tener que volver a la madre para poner la intervencion es el
 * camino largo, que es el que no se recorre.
 *
 * La copia en memoria se relee de disco ANTES de derivar --lo hace
 * fue_host()-- porque la madre sigue viva al lado.                    */
static int fg_guarda( void *d )
{
    Proyecto *p = fue_proyecto();
    PrError   e;

    (void) d;
    if ( !p || !p->path[0] ) return 1;
    return pr_escribir( p, p->path, &e );
}

/* ABRIR EL DERIVADO ES CARGARLO AQUI MISMO, en esta ventana. No se lanza
 * otro proceso: el analista estaba trabajando en esta, y el hijo es la
 * iteracion siguiente de lo mismo. Se carga por el mismo camino que el
 * selector de ficheros, que es el unico que hay.                      */
static void fg_abre( void *d, const char *serie, const char *muestra,
                     const char *id, AnHerramienta con )
{
    FueContext *ctx = (FueContext *) d;
    char        ruta[PR_RUTA];

    (void) con;                     /* aqui solo hay una puerta: esta */
    if ( pr_ruta( fue_proyecto(), serie, muestra, id, ".inp",
                  ruta, sizeof ruta ) != 0 )
        { fg_di( ctx, "No pude componer la ruta del modelo nuevo." ); return; }
    fue_abre_al_arrancar( ctx, ruta );
}

static AnHost fue_host( FueContext *ctx )
{
    AnHost h;

    /* LA MADRE SIGUE VIVA AL LADO. Ver fue_proyecto_relee(). */
    fue_proyecto_relee();

    memset( &h, 0, sizeof h );
    h.p       = fue_proyecto();
    h.padre   = ctx->main_window ? GTK_WINDOW(ctx->main_window) : NULL;
    h.preview = (PreviewApp *) ctx;
    h.dueno   = ctx;
    h.di      = fg_di;
    h.guarda  = fg_guarda;
    h.abre    = fg_abre;
    h.puede   = AN_PUEDE_FUE;      /* el editor es de la madre */
    return h;
}


/* ------------------------------------------------------------------------ */

void fue_on_diagnosis( GtkWidget *w, FueContext *ctx )
{
    char serie[PR_ID], muestra[PR_ID], id[PR_ID];

    (void) w;
    if ( fue_modelo_actual( ctx, serie, sizeof serie, muestra, sizeof muestra,
                            id, sizeof id ) != 0 )
        { fg_di( ctx, "Este fichero no es un modelo de este proyecto." ); return; }
    {
    AnHost h = fue_host( ctx );

    an_diagnosis( &h, serie, muestra, id );
    }
}

void fue_on_anomalos( GtkWidget *w, FueContext *ctx )
{
    char serie[PR_ID], muestra[PR_ID], id[PR_ID];

    (void) w;
    if ( fue_modelo_actual( ctx, serie, sizeof serie, muestra, sizeof muestra,
                            id, sizeof id ) != 0 )
        { fg_di( ctx, "Este fichero no es un modelo de este proyecto." ); return; }
    {
    AnHost h = fue_host( ctx );

    an_anomalos( &h, serie, muestra, id );
    }
}

/* LOS DOS BOTONES, CON SU INSUMO. La regla es la de lib/analisis, la misma
 * que usa la madre: dos copias de una regla son dos reglas. Y un botón
 * apagado SIEMPRE dice por qué en su globo.                            */
void fue_analisis_refresca( FueContext *ctx )
{
    char     serie[PR_ID], muestra[PR_ID], id[PR_ID], porque[512];
    AnEstado e = AN_SIN_CLAVE;

    if ( !ctx || !ctx->btn_diagnosis || !ctx->btn_anomalos ) return;

    if ( fue_modelo_actual( ctx, serie, sizeof serie, muestra, sizeof muestra,
                            id, sizeof id ) == 0 )
        e = an_estado( fue_proyecto(), serie, muestra, id, porque, sizeof porque );
    else
        g_snprintf( porque, sizeof porque,
            "Este fichero no es un modelo de este proyecto: sin clave no hay "
            "linaje que mirar." );

    {
    gboolean listo = ( e == AN_LISTO );
    const char *g1 = listo
        ? "El resumen de la diagnosis de los residuos: media, autocorrelación, "
          "normalidad, estimación y sobreparametrización."
        : porque;
    const char *g2 = listo
        ? "Los residuos con sus anómalos, sobre el gráfico de fue, y qué "
          "cambia en la ACF/PACF al calibrarlos."
        : porque;

    gtk_widget_set_sensitive( ctx->btn_diagnosis, listo );
    gtk_widget_set_sensitive( ctx->btn_anomalos, listo );
    gtk_widget_set_tooltip_text( ctx->btn_diagnosis, g1 );
    gtk_widget_set_tooltip_text( ctx->btn_anomalos, g2 );
    }
}
