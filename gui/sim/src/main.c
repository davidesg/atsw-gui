/*
 * sim_gui -- el GUI de los sistemas simultaneos (SIM): VARMA con drvarma y,
 * mas adelante, VEC con drvec. docs/DISENO-sim.md.
 *
 * SE LLAMA POR EL MENU DE LA MADRE, no por un motor: lleva dos. Y no «sima»,
 * que es el servidor MCP de sima-python y pip lo instala con ese nombre.
 *
 * Parte de ficheros de fue YA HECHOS: el modelo de cada serie se hace en US;
 * aqui se cruzan. Las opciones son las de drtran_gui.
 */

#include <locale.h>
#include <string.h>

#include "sim.h"
#include "sitio.h"
#include "iconos.h"

/* La madre que lo lanzo («--madre PID»): cuando se va, sim_gui tambien. */
static const char *g_madre = NULL;

static gboolean se_fue_la_madre( gpointer data )
{
    (void) data;
    engine_stop_all();
    g_application_quit( g_application_get_default() );
    return G_SOURCE_REMOVE;
}

/* Los ficheros dados en la linea de ordenes, sin proyecto. */
static gchar **g_ficheros = NULL;

static void activate( GtkApplication *app, gpointer data )
{
    Sim *m = data;
    int  i;

    at_logo_por_defecto();
    gtk_widget_show_all( sim_ventana_new( app, m ) );
    sitio_vigila_madre( g_madre, se_fue_la_madre, NULL );

    if (m->caso[0]) {
        char why[1024];

        if (!sim_caso_carga( m, why, sizeof why ) || why[0])
            sim_estado( m, "%s", why );
        else
            sim_estado( m, "Sistema %s: %d series. Lo siguiente es la puerta "
                        "(«2 · Puerta»).", m->caso, m->sis.n );
    }
    for (i = 0; g_ficheros && g_ficheros[i]; i++) series_anade( m, g_ficheros[i] );
}

static void uso( const char *me )
{
    printf( "uso: %s [--proyecto FICHERO [--caso C [--modelo c]]] [FICHERO.pre ...]\n\n"
            "  --proyecto F  abre el proyecto F de la madre.\n"
            "  --caso C      carga el sistema C: sus series, en su orden (el de\n"
            "                Cholesky de la respuesta al impulso).\n"
            "  --modelo c    parte del modelo c del sistema, no del elegido.\n"
            "  --madre PID   la madre que lo lanza: si se cierra, sim_gui también.\n"
            "  FICHERO.pre   sin proyecto, los ficheros de fue del sistema, en orden.\n",
            me );
}

/* LOS NUMEROS, CON PUNTO: GTK pone el idioma del sistema al arrancar, y con
 * el espanol strtod deja de entender los ficheros. Va despues del de GTK. */
static void numeros_con_punto( GApplication *app, gpointer data )
{
    (void) app; (void) data;
    setlocale( LC_NUMERIC, "C" );
}

int main( int argc, char **argv )
{
    GtkApplication *app;
    static Sim m;
    const char *proy = NULL, *caso = NULL, *modelo = NULL;
    GPtrArray *fich = g_ptr_array_new();
    int rc, i;

    memset( &m, 0, sizeof m );
    for (i = 1; i < argc; i++) {
        if (!strcmp( argv[i], "--proyecto" ) && i + 1 < argc) proy = argv[++i];
        else if (!strncmp( argv[i], "--proyecto=", 11 )) proy = argv[i] + 11;
        else if (!strcmp( argv[i], "--caso" ) && i + 1 < argc) caso = argv[++i];
        else if (!strcmp( argv[i], "--modelo" ) && i + 1 < argc) modelo = argv[++i];
        else if (!strcmp( argv[i], "--madre" ) && i + 1 < argc) g_madre = argv[++i];
        else if (!strcmp( argv[i], "-h" ) || !strcmp( argv[i], "--help" ))
            { uso( argv[0] ); g_ptr_array_free( fich, TRUE ); return 0; }
        else if (argv[i][0] != '-') g_ptr_array_add( fich, g_strdup( argv[i] ) );
        else { fprintf( stderr, "%s: no entiendo «%s»\n", argv[0], argv[i] );
               uso( argv[0] ); g_ptr_array_free( fich, TRUE ); return 2; }
    }

    if ((caso || modelo) && !proy) {
        fprintf( stderr, "%s: --caso y --modelo son de un proyecto: hace falta "
                         "--proyecto\n", argv[0] );
        return 2;
    }
    if (modelo && !caso) {
        fprintf( stderr, "%s: --modelo es de un sistema: hace falta --caso\n", argv[0] );
        return 2;
    }
    if (caso && fich->len) {
        fprintf( stderr, "%s: con --caso, las series son las del sistema: no se "
                         "dan ficheros\n", argv[0] );
        return 2;
    }

    if (proy) {
        char why[512];
        const PrCaso *c;

        if (!sim_proyecto_abre( &m, proy, why, sizeof why )) {
            fprintf( stderr, "%s: %s\n", proy, why );
            return 3;
        }
        if (caso) {
            if ((c = pr_caso_ver( m.proy, caso )) == NULL) {
                fprintf( stderr, "%s: «%s» no es un caso del proyecto\n", proy, caso );
                return 3;
            }
            if (strcmp( c->motor, "drvarma" ) && strcmp( c->motor, "drvec" )) {
                fprintf( stderr, "%s: «%s» es un caso MT (%s): se abre con drtran_gui\n",
                         proy, caso, c->motor[0] ? c->motor : "drtran" );
                return 3;
            }
            if (modelo && pr_corrida_idx( m.proy, caso, modelo ) < 0) {
                fprintf( stderr, "%s: «%s» no es un modelo del sistema %s\n", proy,
                         modelo, caso );
                return 3;
            }
            snprintf( m.caso, sizeof m.caso, "%s", caso );
            if (modelo) snprintf( m.modelo_ini, sizeof m.modelo_ini, "%s", modelo );
        }
    }

    g_ptr_array_add( fich, NULL );
    g_ficheros = (gchar **) g_ptr_array_free( fich, FALSE );

    /* drvarma, el de su arbol antes que el del PATH. */
    engine_set_finder( sim_hermano );

    argc = 1;
    app = gtk_application_new( "org.atsw.sim", G_APPLICATION_NON_UNIQUE );
    g_signal_connect( app, "startup", G_CALLBACK(numeros_con_punto), NULL );
    g_signal_connect( app, "activate", G_CALLBACK(activate), &m );
    g_signal_connect_swapped( app, "shutdown", G_CALLBACK(engine_stop_all), NULL );
    rc = g_application_run( G_APPLICATION(app), argc, argv );
    g_object_unref( app );
    g_strfreev( g_ficheros );
    return rc;
}
