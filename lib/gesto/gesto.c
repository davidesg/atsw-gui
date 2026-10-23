/*
 * gesto.c -- iterar y modelo nuevo, sin ventana (ver gesto.h).
 *
 * Los textos son los que la madre ponia en la barra: salen de aqui para
 * que la madre y atsw-cli digan lo mismo.
 */

#include <stdio.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "gesto.h"

/* Copia el contenido al .inp del hijo, creando su directorio. */
static int escribe( const char *destino, const gchar *contenido, gsize largo,
                    char *why, size_t n )
{
    gchar *dir = g_path_get_dirname( destino );

    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );
    if ( !g_file_set_contents( destino, contenido, (gssize) largo, NULL ) )
        {
        if ( why ) snprintf( why, n, "No pude escribir %s", destino );
        return 1;
        }
    return 0;
}

int ge_itera( Proyecto *p, const char *serie, const char *muestra,
              const char *padre, char *id_out, size_t nid,
              char *ruta_out, size_t nruta, char *why, size_t n )
{
    PrError e;
    char    id[PR_ID], origen[PR_RUTA], destino[PR_RUTA];
    gchar  *contenido = NULL;
    gsize   largo = 0;
    int     rc;

    if ( why && n ) why[0] = '\0';
    if ( id_out && nid ) id_out[0] = '\0';
    if ( ruta_out && nruta ) ruta_out[0] = '\0';

    /* De donde se copia: el .pre del modelo del que se itera. Si no hay
       padre, no hay de donde: la primera iteracion la trae fue.         */
    if ( padre == NULL || *padre == '\0' )
        {
        if ( why ) snprintf( why, n, "Marca de qué modelo quieres iterar. La "
                             "primera iteración la trae fue." );
        return 1;
        }
    /* De los DATOS no se itera: no tienen .pre porque no se estiman. Decir
       «ese modelo no se ha estimado» seria cierto y no ayudaria nada.   */
    if ( pr_es_datos( p, serie, muestra, padre ) )
        {
        if ( why ) snprintf( why, n, "Los datos no se iteran. Mándalos a fue "
                             "y de ellos sale el primer modelo." );
        return 1;
        }
    if ( pr_modelo_idx( p, serie, muestra, padre ) < 0 )
        {
        if ( why ) snprintf( why, n, "«%s» no es un modelo de %s en esa muestra.",
                             padre, serie );
        return 1;
        }
    if ( pr_ruta( p, serie, muestra, padre, ".pre", origen, sizeof origen ) != 0 )
        { if ( why ) snprintf( why, n, "No pude componer la ruta." ); return 1; }

    if ( !g_file_get_contents( origen, &contenido, &largo, NULL ) )
        {
        /* SE DICE QUE FALTA, no se crea uno vacio. Un .pre que no esta es un
           modelo que no se ha estimado, y eso es un hecho del proyecto.  */
        if ( why ) snprintf( why, n, "No hay %s: ese modelo no se ha estimado "
                             "todavía.", origen );
        return 1;
        }

    /* La iteracion nueva, con su LINAJE, sin preguntar. */
    if ( pr_deriva( p, serie, muestra, padre, id, sizeof id,
                    destino, sizeof destino, &e ) != 0 )
        { if ( why ) pr_error_es( &e, why, n ); g_free( contenido ); return 1; }

    rc = escribe( destino, contenido, largo, why, n );
    g_free( contenido );
    if ( rc ) { pr_borra( p, serie, muestra, id, &e ); return 1; }

    if ( id_out && nid ) snprintf( id_out, nid, "%s", id );
    if ( ruta_out && nruta ) snprintf( ruta_out, nruta, "%s", destino );
    if ( why ) snprintf( why, n, "%s: %s.pre → %s.inp. Ahora estímalo en fue.",
                         serie, padre, id );
    return 0;
}

int ge_modelo_nuevo( Proyecto *p, const char *serie, const char *muestra,
                     char *id_out, size_t nid, char *ruta_out, size_t nruta,
                     char *why, size_t n )
{
    PrError     e;
    const char *datos;
    char        id[PR_ID], origen[PR_RUTA], destino[PR_RUTA];
    gchar      *contenido = NULL;
    gsize       largo = 0;
    int         rc;

    if ( why && n ) why[0] = '\0';
    if ( id_out && nid ) id_out[0] = '\0';
    if ( ruta_out && nruta ) ruta_out[0] = '\0';

    if ( serie == NULL || *serie == '\0' )
        { if ( why ) snprintf( why, n, "Marca una serie." ); return 1; }

    datos = pr_datos_de( p, serie, muestra );
    if ( !*datos )
        {
        /* SE DICE QUE FALTAN LOS DATOS. Derivar de la nada daria un .inp
           vacio que fue rechazaria con un error del motor, mas lejos del
           sitio donde se puede arreglar.                                */
        if ( why ) snprintf( why, n, "«%s» no tiene datos cargados en este "
                             "proyecto: cárgalos con «Datos…».", serie );
        return 1;
        }

    /* EL .inp DE LOS DATOS DE ESTA VENTANA, no el de la completa. Con "" a
       pelo aqui, un modelo nuevo en una submuestra nacia con la serie
       ENTERA dentro: la ventana se declaraba y no se aplicaba.        */
    if ( pr_ruta( p, serie, muestra, datos, ".inp", origen, sizeof origen ) != 0 ||
         !g_file_get_contents( origen, &contenido, &largo, NULL ) )
        {
        if ( why ) snprintf( why, n, "No pude leer los datos de «%s».", serie );
        return 1;
        }

    if ( pr_deriva( p, serie, muestra, datos, id, sizeof id,
                    destino, sizeof destino, &e ) != 0 )
        { if ( why ) pr_error_es( &e, why, n ); g_free( contenido ); return 1; }

    /* El .inp nuevo arranca siendo COPIA de los datos: los mismos numeros,
       sin modelo. Es lo que el analista va a especificar encima.        */
    rc = escribe( destino, contenido, largo, why, n );
    g_free( contenido );
    if ( rc ) { pr_borra( p, serie, muestra, id, &e ); return 1; }

    if ( id_out && nid ) snprintf( id_out, nid, "%s", id );
    if ( ruta_out && nruta ) snprintf( ruta_out, nruta, "%s", destino );
    if ( why ) snprintf( why, n, "%s: %s nace de los datos (%s), que siguen "
                         "intactos. Especifícalo en fue.", serie, id, datos );
    return 0;
}

char *ge_sha( const char *ruta )
{
    gchar *c = NULL, *h;
    gsize  n = 0;

    if ( !g_file_get_contents( ruta, &c, &n, NULL ) ) return NULL;
    h = g_compute_checksum_for_data( G_CHECKSUM_SHA256, (const guchar *) c, n );
    g_free( c );
    return h;
}
