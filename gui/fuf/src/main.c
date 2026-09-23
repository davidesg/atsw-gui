/*
 * fuf_gui -- la ventana de fuf, el motor de previsión.
 *
 * SE LLAMA COMO LOS HERMANOS: gui/fue construye fue_gui, gui/drtran
 * drtran_gui, y este fuf_gui. El taller tiene un GUI por motor y la madre
 * orquesta.
 *
 * NON_UNIQUE, por la misma razón que fue_gui: prever dos modelos a la vez es
 * normal, y una instancia única convertiría el segundo lanzamiento en un
 * "activate" que abre otra ventana con el fichero del primero. Ya pasó.
 */

#include <string.h>

#include "fufgui.h"

#include "rutas.h"

static char g_abrir[FG_RUTA];
static char g_raiz[FG_RUTA];

static void uso( const char *me )
{
    printf( "uso: %s [--proyecto FICHERO] [MODELO.pre|MODELO.inp]\n\n"
            "  --proyecto F  el proyecto del que sale el modelo. Los ficheros\n"
            "                de la previsión se escriben AL LADO del modelo,\n"
            "                que es donde se buscan después.\n\n"
            "Sin fichero, se abre vacío y se elige desde la ventana.\n", me );
}

static void activate( GtkApplication *app, gpointer d )
{
    static GtkWidget *abierta = NULL;
    Fuf *f = d;

    /* UNA VENTANA POR PROCESO. La bandera NON_UNIQUE lo garantiza, pero el
       guardia deja dicho por que no puede haber dos.                   */
    if ( abierta ) { gtk_window_present( GTK_WINDOW(abierta) ); return; }

    abierta = fuf_ventana_nueva( app, f );
    gtk_widget_show_all( abierta );
}

int main( int argc, char **argv )
{
    GtkApplication *app;
    Fuf            *f;
    const char     *proy = NULL;
    int             rc, i;

    for ( i = 1; i < argc; i++ )
        {
        if ( !strcmp( argv[i], "--proyecto" ) && i + 1 < argc )
            proy = argv[++i];
        else if ( !strncmp( argv[i], "--proyecto=", 11 ) )
            proy = argv[i] + 11;
        else if ( !strcmp( argv[i], "-h" ) || !strcmp( argv[i], "--help" ) )
            { uso( argv[0] ); return 0; }
        else if ( argv[i][0] != '-' )
            snprintf( g_abrir, sizeof g_abrir, "%s", argv[i] );
        else
            { fprintf( stderr, "%s: no entiendo «%s»\n", argv[0], argv[i] );
              uso( argv[0] ); return 2; }
        }

    if ( proy )
        {
        Proyecto *p = calloc( 1, sizeof *p );
        PrError   e;

        if ( pr_leer( proy, p, &e ) != 0 && e.cod != PR_ENOFILE )
            {
            /* Un manifiesto ROTO se dice y se para: seguir seria trabajar
               sobre un proyecto que no se entendio.                    */
            char why[512];

            pr_error_es( &e, why, sizeof why );
            fprintf( stderr, "%s: %s\n", proy, why );
            free( p );
            return 3;
            }
        pr_ruta( p, "", "", NULL, NULL, g_raiz, sizeof g_raiz );
        free( p );
        }

    f = g_new0( Fuf, 1 );
    if ( g_abrir[0] )
        {
        char *dir = g_path_get_dirname( g_abrir );

        snprintf( f->inp, sizeof f->inp, "%s", g_abrir );
        snprintf( f->dir, sizeof f->dir, "%s", dir );
        g_free( dir );

        /* EL DIRECTORIO DEL MODELO ES DONDE CORREN LOS MOTORES, y por eso
           los ficheros de la previsión caen al lado del modelo sin que
           nadie los mueva: es la respuesta a "dónde se guardan".     */
        {
        char *base = g_path_get_basename( f->inp );

        ruta_nombre( base, f->base, sizeof f->base );
        g_free( base );
        }
        snprintf( f->prev, sizeof f->prev, "forecast_%s", f->base );
        }

    /* Las opciones ya están leídas: al GTK sólo le llega el nombre. */
    argc = 1;
    app = gtk_application_new( "org.fuf.gui", G_APPLICATION_NON_UNIQUE );
    g_signal_connect( app, "activate", G_CALLBACK(activate), f );
    rc = g_application_run( G_APPLICATION(app), argc, argv );
    g_object_unref( app );
    return rc;
}
