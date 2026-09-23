/*
 * test_editor.c -- LA regla del editor del .inp, sin widgets.
 *
 * «Un guardado fallido no puede costar trabajo.» El editor valida por la
 * puerta del motor --inp_check_fue, el fichero que compila el propio fue--
 * y si no pasa, el fichero que habia sigue donde estaba.
 *
 * Es la fase 2 (docs/DISENO-editor.md §2.2) probada otra vez donde ahora
 * vive: el editor de la madre trabaja sobre un NODO del proyecto, no sobre
 * un fichero suelto, pero la regla es la misma y no se puede relajar.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

int atsw_guarda_inp( const char *destino, const char *txt,
                     char *why, size_t n );

/* Lo que el editor usa de la ventana. Aqui no hay ventana: la regla que se
   prueba esta FUERA del widget a proposito, que es de lo que iba la fase 2
   -- un solo camino, y probable.                                        */
typedef struct _Atsw Atsw;
typedef struct _PreviewApp PreviewApp;
void  barra_pub( Atsw *a, const char *s )       { (void) a; (void) s; }
void  atsw_refresca( Atsw *a )                  { (void) a; }
char *atsw_programa( const char *p )            { (void) p; return 0; }
int   atsw_guarda( Atsw *a, void *e )           { (void) a; (void) e; return 0; }

/* Los dos ganchos que lib/preview pide al anfitrion. Aqui no hay anfitrion:
   el visor no entra en lo que se prueba.                               */
void  preview_open_external( PreviewApp *a, const char *p )
                                                { (void) a; (void) p; }
void  preview_show_status( PreviewApp *a, const char *f, ... )
                                                { (void) a; (void) f; }

static int fallos = 0;

static void ok( int c, const char *que )
{
    printf( c ? "  ok    %s\n" : "  FALLO %s\n", que );
    if ( !c ) fallos++;
}

static gchar *lee( const char *p )
{
    gchar *c = NULL;

    if ( !g_file_get_contents( p, &c, NULL, NULL ) ) return NULL;
    return c;
}

int main( int argc, char **argv )
{
    const char *dir = argc > 1 ? argv[1] : ".";
    const char *bueno = argc > 2 ? argv[2] : NULL;
    char        destino[1024], why[512];
    gchar      *original = NULL, *ahora;

    if ( bueno == NULL ) { printf( "  (sin .inp de referencia: me salto)\n" );
                           return 0; }
    original = lee( bueno );
    if ( original == NULL )
        { printf( "  (no pude leer %s: me salto)\n", bueno ); return 0; }

    g_snprintf( destino, sizeof destino, "%s/editor.inp", dir );

    printf( "UN .inp VALIDO SE GUARDA\n" );
    ok( atsw_guarda_inp( destino, original, why, sizeof why ) == 0,
        "se escribe" );
    ahora = lee( destino );
    ok( ahora && strcmp( ahora, original ) == 0, "y es lo que se le dio" );
    g_free( ahora );

    printf( "\nUNO QUE EL MOTOR NO PODRIA LEER, NO\n" );
    ok( atsw_guarda_inp( destino, "esto no es un .inp\n", why,
                         sizeof why ) != 0, "se rechaza" );
    ok( strstr( why, "No lo guardo" ) != NULL,
        "y se dice que NO se guardo, no que fallo algo" );
    ok( strlen( why ) > 16, "con el motivo del propio motor" );
    printf( "        [%s]\n", why );

    /* LO QUE IMPORTA: el fichero que habia sigue entero. */
    ahora = lee( destino );
    ok( ahora && strcmp( ahora, original ) == 0,
        "Y EL FICHERO QUE HABIA NO SE TOCO: un guardado fallido no "
        "puede costar trabajo" );
    g_free( ahora );

    printf( "\nNI SE DEJA BASURA AL LADO\n" );
    {
    char tmp[1100];

    g_snprintf( tmp, sizeof tmp, "%s.editando", destino );
    ok( !g_file_test( tmp, G_FILE_TEST_EXISTS ),
        "el temporal de la validacion se recoge" );
    }

    printf( "\nUN .inp VACIO TAMPOCO PASA\n" );
    ok( atsw_guarda_inp( destino, "", why, sizeof why ) != 0,
        "se rechaza" );
    ahora = lee( destino );
    ok( ahora && strcmp( ahora, original ) == 0, "y sigue sin tocarse" );
    g_free( ahora );

    printf( "\nSIN DESTINO NO HAY GUARDADO, Y NO REVIENTA\n" );
    ok( atsw_guarda_inp( NULL, original, why, sizeof why ) != 0, "sin ruta" );
    ok( atsw_guarda_inp( destino, NULL, why, sizeof why ) != 0, "sin texto" );

    g_free( original );
    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
