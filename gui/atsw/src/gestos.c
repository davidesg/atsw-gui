/*
 * gestos.c -- lo que la madre HACE. Ver atsw.h.
 */

#include <string.h>

#include <glib/gstdio.h>

#include "atsw.h"
#include "previewhost.h"
#include "sitio.h"

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
gchar *atsw_programa( const char *programa )
{
    static const char *const sitio[] = {   /* relativos a gui/atsw/  */
        "../fue/bin/%s", "../fug/%s", "../drtran/%s", "./%s",
        /* Y LOS MOTORES, que el editor corre directamente. Van DESPUES de
           los GUIs porque ninguno se llama igual, y ANTES del PATH por la
           misma razon que ellos: una instalacion vieja en /usr/local se
           cuela sin avisar -- ya paso con fue_gui de mayo.            */
        "../../engines/fue/bin/%s", "../../engines/fug/%s",
        "../../engines/fuf/bin/%s", "../../engines/art/bin/%s", NULL
    };
    return sitio_busca( programa, sitio );
}

/* fichero puede ser NULL: entonces solo se abre el programa.
 *
 * MANDARLE LA SERIE ES LA MITAD DEL GESTO. Sin el fichero, "abrir en fue"
 * solo arrancaba fue y el analista tenia que ir a buscar a mano la serie que
 * acababa de marcar en la ventana de al lado. Una madre que lanza programas
 * sin decirles a que vienen no gestiona nada.                          */
void atsw_lanza( Atsw *a, const char *programa, const char *fichero )
{
    atsw_lanza_con( a, programa, NULL, fichero );
}

/* LANZAR CON UNA OPCION DE MAS.
 *
 * Los tres GUIs reciben "--proyecto P [fichero]". Alguno necesita ademas que
 * se le diga QUE HACER al abrir --fue_gui con "--prever" arranca el ciclo de
 * prevision en su pestaña-- y eso es una opcion, no un fichero. Va antes del
 * fichero porque el fichero es el argumento posicional.               */
void atsw_lanza_con( Atsw *a, const char *programa, const char *opcion,
                     const char *fichero )
{
    gchar  *argv[6];
    GError *e = NULL;
    gchar  *exe;
    int     n = 0;

    if ( !a->hay ) return;

    exe = atsw_programa( programa );
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
    if ( opcion && *opcion )   argv[n++] = (gchar *) opcion;
    if ( fichero && *fichero ) argv[n++] = (gchar *) fichero;
    argv[n] = NULL;

    /* EL stderr DE LOS HIJOS NO SE TIRA.
     *
     * Iba a /dev/null, y eso convierte cualquier queja suya en un sintoma
     * sin causa: una ventana que se cuelga, unos simbolos raros en una
     * barra, y nada que leer. Los avisos de GTK --UTF-8 invalido, un widget
     * mal parentado-- salen justo por ahi.
     *
     * Heredando el de la madre van a donde va el suyo, que es el log con el
     * que se la lanza. El stdout si se tira: son los motores hablando, y eso
     * ya se enseña donde toca.                                         */
    if ( !g_spawn_async( NULL, argv, NULL,
                         G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL,
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

gboolean atsw_itera( Atsw *a, const char *serie, const char *muestra,
                     const char *padre, char *why, size_t n )
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
    if ( pr_es_datos( a->p, serie, muestra, padre ) )
        {
        if ( why ) snprintf( why, n, "Los datos no se iteran. Mándalos a fue "
                             "y de ellos sale el primer modelo." );
        return FALSE;
        }
    if ( pr_ruta( a->p, serie, muestra, padre, ".pre", origen,
                  sizeof origen ) != 0 )
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
    /* ITERAR SE QUEDA EN SU VENTANA: seguir desde un optimo es seguir
       sobre las mismas observaciones. Cambiar de ventana es otro gesto. */
    if ( pr_deriva( a->p, serie, muestra, padre, id, sizeof id,
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

    if ( atsw_guarda( a, &e ) != 0 )
        {
        if ( why ) snprintf( why, n, "El .inp está, pero no pude guardar el "
                             "proyecto." );
        return FALSE;
        }

    if ( why ) snprintf( why, n, "%s: %s.pre → %s.inp. Ahora estímalo en fue.",
                         serie, padre, id );
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* UN MODELO NUEVO: nace de los DATOS, no de un optimo.                      */
/*                                                                           */
/* Es el gesto hermano de Iterar, y la diferencia es de donde copia:         */
/*                                                                           */
/*   Iterar        del .pre del modelo marcado -- SEGUIR desde su optimo.    */
/*   Modelo nuevo  del .inp de los datos       -- EMPEZAR otra vez.          */
/*                                                                           */
/* Por eso cuelga siempre de m00 y no del modelo que hubiera marcado: un     */
/* modelo que empieza de cero no viene del anterior, viene de la serie.      */
/*                                                                           */
/* Y por eso existe: especificar el PRIMER modelo no tenia gesto propio. Se  */
/* mandaba la serie a fue y fue escribia sobre el .inp de los datos, que es  */
/* el que fug dibuja y la raiz del linaje.                                   */
/* ------------------------------------------------------------------------ */

gboolean atsw_modelo_nuevo( Atsw *a, const char *serie, const char *muestra,
                            char *id_out, size_t nid,
                            char *ruta_out, size_t nruta, char *why, size_t n )
{
    PrError     e;
    const char *datos;
    char        id[PR_ID], origen[PR_RUTA], destino[PR_RUTA];
    gchar      *contenido = NULL, *dir;
    gsize       largo = 0;

    if ( why && n ) why[0] = '\0';
    if ( id_out && nid ) id_out[0] = '\0';
    if ( ruta_out && nruta ) ruta_out[0] = '\0';
    if ( !a->hay ) return FALSE;

    if ( serie == NULL || *serie == '\0' )
        { if ( why ) snprintf( why, n, "Marca una serie." ); return FALSE; }

    datos = pr_datos_de( a->p, serie, muestra );
    if ( !*datos )
        {
        /* SE DICE QUE FALTAN LOS DATOS. Derivar de la nada daria un .inp
           vacio que fue rechazaria con un error del motor, mas lejos del
           sitio donde se puede arreglar.                                */
        if ( why ) snprintf( why, n, "«%s» no tiene datos cargados en este "
                             "proyecto: cárgalos con «Datos…».", serie );
        return FALSE;
        }

    /* EL .inp DE LOS DATOS DE ESTA VENTANA, no el de la completa. Con "" a
       pelo aqui, un modelo nuevo en una submuestra nacia con la serie
       ENTERA dentro: la ventana se declaraba y no se aplicaba.        */
    if ( pr_ruta( a->p, serie, muestra, datos, ".inp", origen,
                  sizeof origen ) != 0 ||
         !g_file_get_contents( origen, &contenido, &largo, NULL ) )
        {
        if ( why ) snprintf( why, n, "No pude leer los datos de «%s».", serie );
        return FALSE;
        }

    if ( pr_deriva( a->p, serie, muestra, datos, id, sizeof id,
                    destino, sizeof destino, &e ) != 0 )
        { if ( why ) pr_error_es( &e, why, n ); g_free( contenido ); return FALSE; }

    dir = g_path_get_dirname( destino );
    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );

    /* El .inp nuevo arranca siendo COPIA de los datos: los mismos numeros,
       sin modelo. Es lo que el analista va a especificar encima.        */
    if ( !g_file_set_contents( destino, contenido, (gssize) largo, NULL ) )
        {
        if ( why ) snprintf( why, n, "No pude escribir %s", destino );
        g_free( contenido );
        return FALSE;
        }
    g_free( contenido );

    if ( atsw_guarda( a, &e ) != 0 )
        {
        if ( why ) snprintf( why, n, "El .inp está, pero no pude guardar el "
                             "proyecto." );
        return FALSE;
        }

    if ( id_out && nid ) snprintf( id_out, nid, "%s", id );
    if ( ruta_out && nruta ) snprintf( ruta_out, nruta, "%s", destino );
    if ( why ) snprintf( why, n, "%s: %s nace de los datos (%s), que siguen "
                         "intactos. Especifícalo en fue.", serie, id, datos );
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/* LLEVAR UN MODELO A OTRA MUESTRA                                           */
/*                                                                           */
/* LA MISMA ESPECIFICACION, OTRA VENTANA. Y es DERIVAR, no editar: el .out   */
/* del modelo de partida describe una estimacion sobre otras observaciones,  */
/* asi que cambiarle la muestra dejaria un informe que ya no corresponde.    */
/*                                                                           */
/* El .inp nuevo NO es una copia del viejo: se GENERA del .csv con la ventana */
/* nueva --otros numeros y otro nobs-- y encima se le pone la especificacion  */
/* del padre. Copiarlo y tocarle el nobs seria dejar dentro los datos de la   */
/* ventana anterior.                                                         */
/*                                                                           */
/* De momento lo que viaja es la SERIE, no los operadores: sale un .inp con  */
/* los datos de la ventana y sin modelo, y el analista especifica encima --  */
/* que es lo mismo que hace "Modelo nuevo". Llevar tambien los operadores    */
/* pide un editor del .inp que sepa de estructura, y eso es otra cosa.       */
/* ------------------------------------------------------------------------ */

gboolean atsw_en_muestra( Atsw *a, const char *serie, const char *padre,
                          const char *muestra, char *why, size_t n )
{
    PrError          e;
    const PrMuestra *mu;
    const char      *datos;
    char             id[PR_ID], destino[PR_RUTA], csv[PR_RUTA], *dir;

    if ( why && n ) why[0] = '\0';
    if ( !a->hay ) return FALSE;

    if ( serie == NULL || !*serie )
        { if ( why ) snprintf( why, n, "Marca una serie." ); return FALSE; }

    if ( muestra && *muestra )
        {
        mu = pr_muestra_ver( a->p, muestra );
        if ( mu == NULL )
            { if ( why ) snprintf( why, n, "«%s» no es una muestra de este "
                                   "proyecto.", muestra ); return FALSE; }
        }
    else
        mu = NULL;                                  /* la completa */

    /* EL DATO ES EL .csv. Sin el no se puede recortar nada: los numeros de
       la ventana tienen que salir de algun sitio.                     */
    if ( atsw_csv_de( a->p, serie, csv, sizeof csv ) != 0 ||
         !g_file_test( csv, G_FILE_TEST_EXISTS ) )
        {
        if ( why ) snprintf( why, n, "«%s» no tiene datos.csv: se cargó antes "
                             "de que los datos vivieran ahí. Vuelve a "
                             "importarla.", serie );
        return FALSE;
        }

    datos = pr_datos_de( a->p, serie, muestra );
    if ( !*datos )
        { if ( why ) snprintf( why, n, "«%s» no tiene datos en el proyecto.",
                               serie ); return FALSE; }

    /* CUELGA DE LOS DATOS DE ESTA VENTANA, y no del modelo de donde vino la
       idea. El linaje no sale de su ventana: el padre de un modelo es de
       que .pre salio, y el de la hoja de al lado se estimo sobre otras
       observaciones. De donde vino la idea se dice en la barra y se ve en
       el globo --"la misma estructura esta en..."-- que es informacion,
       no linaje.                                                      */
    (void) padre;
    if ( pr_deriva( a->p, serie, muestra, datos,
                    id, sizeof id, destino, sizeof destino, &e ) != 0 )
        { if ( why ) pr_error_es( &e, why, n ); return FALSE; }

    dir = g_path_get_dirname( destino );
    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );

    if ( atsw_genera_inp( csv, destino, serie,
                          mu ? mu->hasta : "", why, n ) != 0 )
        { pr_borra( a->p, serie, muestra, id, &e ); return FALSE; }

    if ( atsw_guarda( a, &e ) != 0 )
        { if ( why ) snprintf( why, n, "El .inp está, pero no pude guardar el "
                               "proyecto." ); return FALSE; }

    if ( why )
        snprintf( why, n, "%s: %s, en la muestra «%s»%s%s. Especifícalo y "
                  "estímalo; el de la otra hoja no se ha tocado.",
                  serie, id, muestra && *muestra ? muestra : "completa",
                  ( padre && *padre ) ? ", con la idea de " : "",
                  ( padre && *padre ) ? padre : "" );
    return TRUE;
}
