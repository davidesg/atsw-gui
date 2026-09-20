/*
 * gestos.c -- lo que la madre HACE. Ver atsw.h.
 */

#include <string.h>

#include <glib/gstdio.h>

#include "atsw.h"
#include "previewhost.h"

void       barra_pub( Atsw *a, const char *s );
GtkWidget *atsw_dialogo_texto( GtkWidget *padre, const char *titulo,
                               const char *previo, char *out, size_t n );

/* ------------------------------------------------------------------------ */
/* LANZAR LOS TRES, CON EL PROYECTO                                           */
/*                                                                           */
/* Procesos aparte, como hasta ahora. La madre no los absorbe: les da el      */
/* contexto que no tenian -- ninguno de los tres guardaba NADA entre          */
/* ejecuciones, asi que cada arranque empezaba preguntando donde esta todo.   */
/* ------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------ */
/* Lo que la madre le debe a lib/preview                                     */
/* ------------------------------------------------------------------------ */

void preview_open_external( PreviewApp *app, const gchar *path )
{
    gchar *uri = g_filename_to_uri( path, NULL, NULL );

    if ( uri ) {
        gtk_show_uri_on_window( GTK_WINDOW(app->ventana), uri,
                                GDK_CURRENT_TIME, NULL );
        g_free( uri );
    }
}

void preview_show_status( PreviewApp *app, const gchar *format, ... )
{
    va_list ap;
    gchar  *s;

    va_start( ap, format );
    s = g_strdup_vprintf( format, ap );
    va_end( ap );
    barra_pub( app, s );
    g_free( s );
}

/* La extension de un nombre, con su punto. La de lib/utils arrastra GTK y el
 * contexto del GUI de fue; esta son cuatro lineas.                      */
const char *getExt( const char *fspec )
{
    const char *e = strrchr( fspec, '.' );

    return ( e == NULL ) ? "" : e;
}

/* DONDE ESTA EL PROGRAMA, y el orden importa.
 *
 * Primero AL LADO DE LA MADRE, en el arbol de compilacion; despues el PATH.
 * No al reves, y por una razon medida: en esta maquina habia un fue_gui de
 * mayo en /usr/local/bin que no entiende --proyecto. Encontrar el equivocado
 * es PEOR que no encontrar ninguno -- el programa abre, no hace lo que se le
 * pidio, y nada lo explica.
 *
 * Un programa lanzado desde un arbol de compilacion tiene que lanzar a SUS
 * hermanos. Instalado, no hay hermanos al lado y manda el PATH, que es lo
 * correcto alli.
 *
 * Devuelve una ruta nueva (g_free) o NULL.                              */
static gchar *donde_esta( const char *programa )
{
    static const char *sitio[] = {         /* relativos a gui/atsw/        */
        "../fue/bin/%s", "../fug/%s", "../drtran/%s", "./%s", NULL
    };
    gchar *mio = g_file_read_link( "/proc/self/exe", NULL );
    gchar *dir = mio ? g_path_get_dirname( mio ) : NULL;
    int    i;

    g_free( mio );

    for ( i = 0; dir && sitio[i]; i++ )
        {
        gchar *rel = g_strdup_printf( sitio[i], programa );
        gchar *p   = g_build_filename( dir, rel, NULL );

        g_free( rel );
        if ( g_file_test( p, G_FILE_TEST_IS_EXECUTABLE ) )
            { g_free( dir ); return p; }
        g_free( p );
        }
    g_free( dir );

    return g_find_program_in_path( programa );
}

/* fichero puede ser NULL: entonces solo se abre el programa.
 *
 * MANDARLE LA SERIE ES LA MITAD DEL GESTO. Sin el fichero, "abrir en fue"
 * solo arrancaba fue y el analista tenia que ir a buscar a mano la serie que
 * acababa de marcar en la ventana de al lado. Una madre que lanza programas
 * sin decirles a que vienen no gestiona nada.                          */
void atsw_lanza( Atsw *a, const char *programa, const char *fichero )
{
    gchar  *argv[5];
    GError *e = NULL;
    gchar  *exe;
    int     n = 0;

    if ( !a->hay ) return;

    exe = donde_esta( programa );
    if ( exe == NULL )
        {
        gchar *s = g_strdup_printf(
            "No encuentro «%s»: ni al lado de esta ventana ni en el PATH. "
            "¿Está compilado?", programa );

        barra_pub( a, s );
        g_free( s );
        return;
        }

    argv[n++] = exe;
    argv[n++] = (gchar *) "--proyecto";
    argv[n++] = a->p->path;
    if ( fichero && *fichero ) argv[n++] = (gchar *) fichero;
    argv[n] = NULL;

    if ( !g_spawn_async( NULL, argv, NULL,
                         G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                         G_SPAWN_STDERR_TO_DEV_NULL,
                         NULL, NULL, NULL, &e ) )
        {
        gchar *s = g_strdup_printf( "No pude lanzar %s: %s",
                                    exe, e ? e->message : "" );

        barra_pub( a, s );
        g_free( s );
        if ( e ) g_error_free( e );
        }
    else
        {
        /* SE DICE QUE BINARIO, con su ruta: asi una instalacion vieja que se
           cuele por el PATH se ve a la primera.                        */
        gchar *s = ( fichero && *fichero )
                 ? g_strdup_printf( "%s, con %s.", exe,
                                    strrchr( fichero, '/' )
                                    ? strrchr( fichero, '/' ) + 1 : fichero )
                 : g_strdup_printf( "%s, con este proyecto.", exe );

        barra_pub( a, s );
        g_free( s );
        }
    g_free( exe );
}

/* ------------------------------------------------------------------------ */
/* EL GESTO QUE CIERRA EL CICLO                                              */
/*                                                                           */
/*     .pre(-1) --copiar--> .inp(0)                                          */
/*                                                                           */
/* .pre e .inp SON EL MISMO FORMATO --se copia un .pre a Z.inp, se corre fue  */
/* y sale Z.pre-- pero el motor EXIGE la extension .inp, asi que esto es una  */
/* COPIA FISICA REAL y no una manera de hablar. Hasta hoy no la hacia nadie:  */
/* habia que renombrar a mano y nada registraba de donde salia el fichero.    */
/*                                                                           */
/* VA EN LA MADRE Y NO EN fue_gui (decision del analista) porque es un gesto  */
/* de PROYECTO: crea una iteracion y registra su linaje. Estimar es otra cosa.*/
/*                                                                           */
/* Y UN .pre QUE SE TOCA VUELVE A SER UN .inp: eso es el contrato. El .pre    */
/* afirma "estos valores son su optimo"; en cuanto se edita la especificacion,*/
/* esa afirmacion deja de valer y sus valores vuelven a ser SEMILLAS.         */
/* ------------------------------------------------------------------------ */

gboolean atsw_itera( Atsw *a, const char *serie, const char *padre,
                     char *why, size_t n )
{
    PrError e;
    char    id[PR_ID], origen[PR_RUTA], destino[PR_RUTA];
    gchar  *contenido = NULL, *dir;
    gsize   largo = 0;

    if ( why && n ) why[0] = '\0';
    if ( !a->hay ) return FALSE;

    /* De donde se copia: el .pre del modelo del que se itera. Si no hay
       padre, no hay de donde: la primera iteracion la trae fue.         */
    if ( padre == NULL || *padre == '\0' )
        {
        if ( why ) snprintf( why, n, "Marca de qué modelo quieres iterar. La "
                             "primera iteración la trae fue." );
        return FALSE;
        }
    /* De los DATOS no se itera: no tienen .pre porque no se estiman. Decir
       «ese modelo no se ha estimado» seria cierto y no ayudaria nada.   */
    if ( pr_es_datos( a->p, serie, padre ) )
        {
        if ( why ) snprintf( why, n, "Los datos no se iteran. Mándalos a fue "
                             "y de ellos sale el primer modelo." );
        return FALSE;
        }
    if ( pr_ruta( a->p, serie, padre, ".pre", origen, sizeof origen ) != 0 )
        { if ( why ) snprintf( why, n, "No pude componer la ruta." );
          return FALSE; }

    if ( !g_file_get_contents( origen, &contenido, &largo, NULL ) )
        {
        /* SE DICE QUE FALTA, no se crea uno vacio. Un .pre que no esta es un
           modelo que no se ha estimado, y eso es un hecho del proyecto.  */
        if ( why ) snprintf( why, n, "No hay %s: ese modelo no se ha estimado "
                             "todavía.", origen );
        return FALSE;
        }

    /* La iteracion nueva, con su LINAJE, sin preguntar. */
    if ( pr_deriva( a->p, serie, padre, id, sizeof id,
                    destino, sizeof destino, &e ) != 0 )
        { if ( why ) pr_error_es( &e, why, n ); g_free( contenido ); return FALSE; }

    dir = g_path_get_dirname( destino );
    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );

    if ( !g_file_set_contents( destino, contenido, (gssize) largo, NULL ) )
        {
        if ( why ) snprintf( why, n, "No pude escribir %s", destino );
        g_free( contenido );
        return FALSE;
        }
    g_free( contenido );

    if ( pr_escribir( a->p, a->p->path, &e ) != 0 )
        {
        if ( why ) snprintf( why, n, "El .inp está, pero no pude guardar el "
                             "proyecto." );
        return FALSE;
        }

    if ( why ) snprintf( why, n, "%s: %s.pre → %s.inp. Ahora estímalo en fue.",
                         serie, padre, id );
    return TRUE;
}
