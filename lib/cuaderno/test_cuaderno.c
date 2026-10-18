/*
 * test_cuaderno.c -- que una entrada se escriba entera, con su procedencia, y
 * que el cuaderno crezca y nunca se trunque.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "cuaderno.h"

static int fallos = 0;

static void ok( int c, const char *que )
{
    printf( c ? "  ok    %s\n" : "  FALLO %s\n", que );
    if ( !c ) fallos++;
}

static gchar *lee( const char *f )
{
    gchar *s = NULL;

    g_file_get_contents( f, &s, NULL, NULL );
    return s;
}

static int veces( const char *s, const char *que )
{
    int n = 0;

    for ( ; s && ( s = strstr( s, que ) ) != NULL; s += strlen( que ) ) n++;
    return n;
}

int main( int argc, char **argv )
{
    gchar    *dir = g_build_filename( argc > 1 ? argv[1] : ".", "proy", NULL );
    gchar    *man = g_build_filename( dir, "proyecto.yaml", NULL );
    char      md[1024], why[512], png[1024];
    Cuaderno *c;
    Tabla    *t;
    gchar    *s;

    g_mkdir_with_parents( dir, 0755 );
    ok( cu_ruta( man, md, sizeof md ) == 0, "cu_ruta da una ruta" );
    g_unlink( md );
    ok( g_str_has_suffix( md, "cuaderno.md" ), "al lado del manifiesto, en cuaderno/" );

    printf( "\nuna entrada con tabla\n" );
    c = cu_nueva( man, "Diagnosis — IPC / m03" );
    ok( c != NULL, "se abre una entrada" );
    cu_dato( c, "Serie", "IPC" );
    cu_dato( c, "Modelo", "m03" );
    cu_dato( c, "Vacio", "" );                /* no se escribe             */
    t = tb_new( "La diagnosis" );
    tb_col( t, "Contraste", NULL, TB_TXT, 0 );
    tb_col( t, "p", NULL, TB_NUM, 3 );
    tb_fila( t ); tb_pon_txt( t, 0, "Q(24)" ); tb_pon_num( t, 1, 0.4321 );
    cu_tabla( c, t );
    tb_free( t );
    cu_texto( c, "Lectura: **sin problemas**." );
    ok( cu_cierra( c, why, sizeof why ) == 0, "se escribe" );

    s = lee( md );
    ok( s != NULL, "el fichero existe" );
    ok( s && strstr( s, "# Cuaderno del proyecto" ) == s, "nace con su cabecera" );
    ok( s && strstr( s, " · Diagnosis — IPC / m03\n" ) != NULL, "la entrada lleva fecha y título" );
    ok( s && strstr( s, "- **Serie:** IPC\n- **Modelo:** m03\n" ) != NULL, "la procedencia" );
    ok( s && strstr( s, "Vacio" ) == NULL, "un dato vacío no se escribe" );
    ok( s && strstr( s, "| Q(24) | 0.432 |" ) != NULL, "la tabla, en Markdown" );
    ok( s && strstr( s, "Lectura: **sin problemas**." ) != NULL, "el texto" );
    g_free( s );

    printf( "\nun gráfico, y uno que no llega\n" );
    c = cu_nueva( man, "Anómalos" );
    ok( cu_imagen( c, "Residuos IPC/m03", "Los residuos", png, sizeof png ) == 0,
        "cu_imagen da dónde escribir" );
    ok( strstr( png, "residuos-ipc-m03.png" ) != NULL, "con un nombre limpio" );
    g_file_set_contents( png, "png", 3, NULL );
    {
    char otro[1024];

    cu_imagen( c, "Residuos IPC/m03", "", otro, sizeof otro );
    ok( strcmp( otro, png ) != 0, "dos en el mismo segundo no se pisan" );
    /* este no se escribe */
    }
    ok( cu_cierra( c, why, sizeof why ) == 0, "se escribe" );
    s = lee( md );
    ok( s && strstr( s, "![Los residuos](img/" ) != NULL, "el enlace, relativo" );
    ok( s && strstr( s, "*(El gráfico no se pudo guardar.)*" ) != NULL,
        "el que no existe no deja un enlace roto" );
    g_free( s );

    printf( "\nnotas, y que nunca se trunca\n" );
    ok( cu_nota( man, "   ", why, sizeof why ) != 0, "una nota vacía se rechaza" );
    ok( strstr( why, "vacía" ) != NULL, "y dice por qué" );
    ok( cu_nota( man, "  m03 me convence más que m02.  ", why, sizeof why ) == 0,
        "una nota se escribe" );
    s = lee( md );
    ok( veces( s, "\n## " ) == 3, "tres entradas, en orden" );
    ok( veces( s, "# Cuaderno del proyecto" ) == 1, "la cabecera, una sola vez" );
    ok( s && strstr( s, "Diagnosis" ) < strstr( s, "Nota" ), "la primera, primero" );
    ok( s && strstr( s, "\nm03 me convence más que m02.\n" ) != NULL, "la nota, sin espacios de sobra" );
    g_free( s );

    printf( "\nsin proyecto\n" );
    ok( cu_nueva( NULL, "x" ) == NULL, "sin manifiesto no hay entrada" );
    ok( cu_cierra( NULL, why, sizeof why ) != 0 && why[0], "y cerrar NULL dice por qué" );

    g_free( dir ); g_free( man );
    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
