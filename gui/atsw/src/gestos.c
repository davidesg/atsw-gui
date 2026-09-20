/*
 * gestos.c -- lo que la madre HACE. Ver atsw.h.
 */

#include <string.h>

#include <glib/gstdio.h>

#include "atsw.h"

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

void atsw_lanza( Atsw *a, const char *programa )
{
    gchar  *argv[4];
    GError *e = NULL;

    if ( !a->hay ) return;

    argv[0] = (gchar *) programa;
    argv[1] = (gchar *) "--proyecto";
    argv[2] = a->p->path;
    argv[3] = NULL;

    if ( !g_spawn_async( NULL, argv, NULL,
                         G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                         G_SPAWN_STDERR_TO_DEV_NULL,
                         NULL, NULL, NULL, &e ) )
        {
        gchar *s = g_strdup_printf( "No pude lanzar %s: %s. ¿Está en el PATH?",
                                    programa, e ? e->message : "" );

        barra_pub( a, s );
        g_free( s );
        if ( e ) g_error_free( e );
        }
    else
        {
        gchar *s = g_strdup_printf( "%s lanzado con este proyecto.", programa );

        barra_pub( a, s );
        g_free( s );
        }
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
