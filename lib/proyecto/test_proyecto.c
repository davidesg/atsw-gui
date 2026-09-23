/*
 * test_proyecto.c -- que la cadena se pueda reconstruir, que el nombre no
 * haga falta para nada, y que "sin razon" se vea.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "proyecto.h"

static int fallos = 0;
static char DIR[256];

static void ok( int c, const char *que )
{
    printf( c ? "  ok    %s\n" : "  FALLO %s\n", que );
    if ( !c ) fallos++;
}

static void es( const char *dio, const char *debe, const char *que )
{
    int c = strcmp( dio, debe ) == 0;

    printf( c ? "  ok    %s\n" : "  FALLO %s\n        dio  \"%s\"\n"
                                 "        debe \"%s\"\n", que, dio, debe );
    if ( !c ) fallos++;
}

static const char *pon( const char *nombre, const char *txt )
{
    static char path[512];
    FILE       *f;

    snprintf( path, sizeof path, "%s/%s", DIR, nombre );
    f = fopen( path, "w" );
    if ( f ) { fputs( txt, f ); fclose( f ); }
    return path;
}

int main( int argc, char **argv )
{
    Proyecto *p, *q;
    PrError   e;
    char      id[PR_ID], ruta[PR_RUTA], b[512];
    char      camino[16][PR_ID], sin[16][PR_ID];
    int       n;

    snprintf( DIR, sizeof DIR, "%s", argc > 1 ? argv[1] : "." );
    p = malloc( sizeof *p );
    q = malloc( sizeof *q );

    printf( "LA CADENA DE ITERACION\n" );
    pr_nuevo( p, "SF_MEG", "Inflación del área euro", "." );
    snprintf( p->path, sizeof p->path, "%s/proyecto.yaml", DIR );
    ok( pr_serie_add( p, "IPC_ES", &e ) == 0, "se da de alta una serie" );
    ok( pr_serie_add( p, "IPC_ES", &e ) != 0 && e.cod == PR_EDUP,
        "y dos veces no" );

    /* m00 SON LOS DATOS. Los pone la carga, y de ahi cuelga todo.  */
    ok( pr_deriva_rol( p, "IPC_ES", "", NULL, PR_DATOS, id, sizeof id,
                       ruta, sizeof ruta, &e ) == 0, "los datos entran" );
    es( id, "m00", "y son m00" );
    ok( pr_es_datos( p, "IPC_ES", "", "m00" ), "y se sabe que lo son" );
    es( pr_datos_de( p, "IPC_ES", "" ), "m00", "y se encuentran por la serie" );

    /* .inp(-1) -> .pre(-1) -> .inp(0) ... */
    ok( pr_deriva( p, "IPC_ES", "", "m00", id, sizeof id, ruta, sizeof ruta, &e ) == 0,
        "el primer modelo cuelga de los datos" );
    es( id, "m01", "y se llama m01" );
    ok( !pr_es_datos( p, "IPC_ES", "", "m01" ), "y NO es datos" );
    pr_deriva( p, "IPC_ES", "", "m01", id, sizeof id, ruta, sizeof ruta, &e );
    es( id, "m02", "y el segundo, m02" );

    ok( pr_deriva( p, "IPC_ES", "", "m99", id, sizeof id, NULL, 0, &e ) != 0 &&
        e.cod == PR_EPADRE, "un padre que no existe se rechaza" );
    {
    /* La reserva del m00 NO depende de que la carga haya pasado antes: una
       serie que llega sin datos tampoco lo ocupa.                       */
    pr_serie_add( p, "SUELTA", &e );
    pr_deriva( p, "SUELTA", "", NULL, id, sizeof id, NULL, 0, &e );
    es( id, "m01", "una serie sin datos: su primer modelo TAMBIEN es m01" );
    es( pr_datos_de( p, "SUELTA", "" ), "", "y no tiene datos que dar" );
    }
    ok( pr_deriva( p, "NO_ESTA", "", NULL, id, sizeof id, NULL, 0, &e ) != 0 &&
        e.cod == PR_ENOSERIE, "y una serie que no esta, tambien" );

    printf( "\nEL LINAJE, QUE ES LO MINIMO QUE NO SE PUEDE PERDER\n" );
    n = pr_camino( p, "IPC_ES", "", "m02", camino, 16 );
    ok( n == 3, "de m02 a la raiz hay tres pasos" );
    es( camino[0], "m02", "  el primero es el propio modelo" );
    es( camino[1], "m01", "  luego su padre" );
    es( camino[2], "m00", "  y la raiz" );

    printf( "\nEL NOMBRE ES CORTESIA: NADIE LO PARSEA\n" );
    pr_ruta( p, "IPC_ES", "", "m02", ".inp", b, sizeof b );
    ok( strstr( b, "IPC_ES/work/IPC_ES_m02.inp" ) != NULL,
        "el fichero se llama para que se entienda a ojo" );
    {
    int i = pr_modelo_idx( p, "IPC_ES", "", "m02" );

    ok( p->m[i].version == 2,
        "pero la VERSION es un campo, no un trozo del nombre" );
    es( p->m[i].padre, "m01", "y el padre tambien" );
    }

    printf( "\nLA RAZON: SE PIDE, NO SE EXIGE, Y SE VE CUANDO FALTA\n" );
    n = pr_sin_razon( p, sin, 16 );
    ok( n == 3, "los tres MODELOS nacen sin razon: no se inventa ninguna" );
    es( sin[0], "IPC_ES/m01", "y se dicen con nombre y apellido" );
    ok( pr_es_datos( p, "IPC_ES", "", "m00" ),
        "los datos no estan entre ellos: no son una decision" );

    ok( pr_razon( p, "IPC_ES", "", "m01",
                  "el residuo de 2020-03 pedía intervención", &e ) == 0,
        "se puede poner DESPUES" );
    ok( pr_sin_razon( p, sin, 16 ) == 2, "y entonces quedan dos sin ella" );
    ok( pr_razon( p, "IPC_ES", "", "m77", "x", &e ) != 0 && e.cod == PR_ENOMODELO,
        "en un modelo que no existe, no" );

    printf( "\nEL ELEGIDO, QUE HOY VIVE EN UN DICCIONARIO A PELO\n" );
    ok( pr_elige( p, "IPC_ES", "", "m02", "el SAR no se gana su sitio", &e ) == 0,
        "se declara cual es" );
    es( pr_elegido( p, "IPC_ES", "" ), "m02", "y se recupera" );
    ok( pr_elige( p, "IPC_ES", "", "m99", NULL, &e ) != 0 && e.cod == PR_ENOMODELO,
        "uno que no existe se rechaza" );

    printf( "\nLA SERIE: UNA CLAVE CORTA Y UNOS CAMPOS DE TEXTO\n" );
    {
    PrSerie *x = pr_serie( p, "IPC_ES" );

    ok( x != NULL, "se llega a la serie por su clave" );
    ok( pr_serie( p, "NO_ESTA" ) == NULL, "y a una que no esta, no" );
    es( pr_serie_titulo( p, "IPC_ES" ), "IPC_ES",
        "sin descripcion, el titulo es la CLAVE" );

    snprintf( x->descripcion, PR_TEXTO, "%s",
              "Índice de precios de consumo armonizado, España" );
    snprintf( x->unidades, PR_TEXTO, "índice 2015 = 100" );
    snprintf( x->fuente, PR_TEXTO, "Eurostat, tabla prc_hicp_midx" );
    snprintf( x->bajada, 16, "2026-09-20" );
    es( pr_serie_titulo( p, "IPC_ES" ),
        "Índice de precios de consumo armonizado, España",
        "y con ella, la descripcion" );
    es( pr_serie_titulo( p, "SUELTA" ), "SUELTA",
        "cada serie la suya: la de al lado sigue sin describir" );
    }

    printf( "\nLAS MUESTRAS: VENTANAS DECLARADAS, Y LA COMPLETA NO SE DECLARA\n" );
    ok( pr_muestra_add( p, "pre-covid", "", "12/2019",
                        "2020-2022 es otro proceso", &e ) == 0,
        "se declara una submuestra" );
    ok( pr_muestra_add( p, "pre-covid", "", "12/2019", "", &e ) != 0 &&
        e.cod == PR_EDUP, "y dos veces no" );
    ok( pr_muestra_add( p, "", "", "12/2019", "", &e ) != 0,
        "la COMPLETA no se declara: es lo que entro" );
    es( pr_muestra_ver( p, "pre-covid" )->hasta, "12/2019", "y se recupera" );
    ok( pr_muestra_ver( p, "no-existe" ) == NULL, "una que no esta, no" );
    ok( pr_muestra_idx( p, "" ) < 0, "y «» no es una muestra declarada" );

    printf( "\nLA MUESTRA ES PARTE DE LA CLAVE\n" );
    {
    char nid[PR_ID], r1[PR_RUTA], r2[PR_RUTA];

    /* CADA VENTANA TIENE SU NODO DE DATOS: la misma serie vista por esa
       ventana. Sin el, la hoja recien declarada esta viva pero vacia y no
       hay de donde empezar nada -- ni que mandar a fug, que mirar la ACF
       de la serie recortada es lo PRIMERO que se hace al truncar.     */
    ok( pr_deriva_rol( p, "IPC_ES", "pre-covid", NULL, PR_DATOS,
                       nid, sizeof nid, NULL, 0, &e ) == 0,
        "cada ventana tiene SU nodo de datos" );
    es( nid, "m00", "y tambien se llama m00" );
    es( pr_datos_de( p, "IPC_ES", "pre-covid" ), "m00",
        "se encuentra por su ventana" );
    ok( pr_es_datos( p, "IPC_ES", "pre-covid", "m00" ), "y se sabe que lo es" );

    /* m01 EN LAS DOS HOJAS, Y SON DOS MODELOS. Cada ventana lleva su
       linaje, asi que no hace falta ensuciar el nombre con «m01_A».  */
    ok( pr_deriva( p, "IPC_ES", "pre-covid", "m00", nid, sizeof nid,
                   r2, sizeof r2, &e ) == 0,
        "se deriva en una submuestra, colgando de SUS datos" );
    es( nid, "m01", "y se llama m01, como el de la completa" );
    ok( !pr_es_datos( p, "IPC_ES", "pre-covid", "m01" ), "que no es datos" );
    ok( pr_modelo_idx( p, "IPC_ES", "", "m01" ) !=
        pr_modelo_idx( p, "IPC_ES", "pre-covid", "m01" ),
        "y son DOS modelos distintos: la clave es (serie, muestra, id)" );

    pr_ruta( p, "IPC_ES", "", "m01", ".inp", r1, sizeof r1 );
    ok( strcmp( r1, r2 ) != 0, "sus ficheros TAMPOCO son el mismo" );
    ok( strstr( r2, "/pre-covid/work/" ) != NULL,
        "la submuestra tiene carpeta" );
    ok( strstr( r1, "/pre-covid/" ) == NULL,
        "y la completa NO: es «», no tiene nombre que poner en una ruta" );

    ok( pr_deriva( p, "IPC_ES", "no-existe", "m00", nid, sizeof nid,
                   NULL, 0, &e ) != 0 && e.cod == PR_EMUESTRA,
        "una muestra sin declarar se rechaza al derivar" );
    ok( pr_deriva( p, "IPC_ES", "", "m01", nid, sizeof nid, NULL, 0, &e ) == 0 &&
        pr_borra( p, "IPC_ES", "", nid, &e ) == 0,
        "(y derivar en la completa sigue funcionando igual)" );
    }

    printf( "\nEL LINAJE NO SALE DE SU VENTANA\n" );
    {
    char cam[16][PR_ID];
    int  k = pr_camino( p, "IPC_ES", "pre-covid", "m01", cam, 16 );

    ok( k == 2, "de m01 de pre-covid a la raiz hay dos pasos" );
    es( cam[1], "m00", "y la raiz es el nodo de datos DE ESTA hoja" );
    }

    printf( "\nEL ELEGIDO ES DE (serie, muestra)\n" );
    ok( pr_elige( p, "IPC_ES", "pre-covid", "m01",
                  "en esta ventana gana este", &e ) == 0,
        "se elige en una hoja" );
    es( pr_elegido( p, "IPC_ES", "pre-covid" ), "m01", "y se recupera" );
    es( pr_elegido( p, "IPC_ES", "" ), "m02",
        "y la completa conserva EL SUYO: son dos decisiones distintas" );
    ok( pr_elige( p, "IPC_ES", "pre-covid", "m99", NULL, &e ) != 0 &&
        e.cod == PR_ENOMODELO, "uno que no esta en esa hoja se rechaza" );

    printf( "\nUNA MUESTRA CON MODELOS DENTRO NO SE BORRA DE REBOTE\n" );
    ok( pr_muestra_borra( p, "pre-covid", &e ) != 0 && e.cod == PR_EENMUESTRA,
        "se niega" );
    es( e.texto, "IPC_ES/m00", "y DICE CUAL vive ahi" );
    ok( pr_muestra_borra( p, "no-existe", &e ) != 0 && e.cod == PR_EMUESTRA,
        "una que no esta, tampoco" );

    printf( "\nLO QUE PYTHON REINTERPRETARIA VA ENTRECOMILLADO\n" );
    {
    /* Nuestro lector devuelve siempre texto; yaml.safe_load no. Sin comillas
       las dos encarnaciones del taller leen cosas distintas del MISMO
       fichero, que es la version silenciosa de los dos dueños.         */
    const char *path;
    PrSerie    *x = pr_serie( p, "SUELTA" );
    Proyecto   *z = malloc( sizeof *z );

    snprintf( x->unidades, PR_TEXTO, "12" );
    snprintf( x->notas, PR_TEXTO, "no" );
    snprintf( x->bajada, 16, "2026-09-20" );

    path = pon( "comillas.yaml", "" );
    pr_escribir( p, path, &e );
    ok( pr_leer( path, z, &e ) == 0, "se escribe y se relee" );
    es( pr_serie_ver( z, "SUELTA" )->unidades, "12",
        "un numero vuelve como el TEXTO que era" );
    es( pr_serie_ver( z, "SUELTA" )->notas, "no",
        "«no» tambien, que en YAML seria false" );
    es( pr_serie_ver( z, "SUELTA" )->bajada, "2026-09-20",
        "y una fecha, que seria un datetime.date" );

    x->unidades[0] = x->notas[0] = '\0';
    free( z );
    }

    printf( "\nIDA Y VUELTA POR EL MANIFIESTO\n" );
    {
    const char *path = pon( "proyecto.yaml", "" );

    ok( pr_escribir( p, path, &e ) == 0, "se escribe" );
    ok( pr_leer( path, q, &e ) == 0, "se lee" );
    es( q->id, "SF_MEG", "  el id" );
    es( q->titulo, "Inflación del área euro", "  el titulo, con sus acentos" );
    ok( q->ns == 2 && q->nm == 6,
        "  dos series, dos nodos de datos y cuatro modelos" );
    ok( pr_es_datos( q, "IPC_ES", "", "m00" ), "  y m00 sigue siendo los datos" );
    es( pr_datos_de( q, "IPC_ES", "" ), "m00", "  que se encuentran por la serie" );
    ok( !pr_es_datos( q, "IPC_ES", "", "m01" ), "  mientras que m01 no lo es" );
    es( pr_elegido( q, "IPC_ES", "" ), "m02", "  el elegido" );
    {
    int i = pr_modelo_idx( q, "IPC_ES", "", "m02" );

    ok( q->m[i].version == 2, "  la version" );
    es( q->m[i].padre, "m01", "  el padre" );
    }
    {
    int i = pr_modelo_idx( q, "IPC_ES", "", "m01" );

    es( q->m[i].razon, "el residuo de 2020-03 pedía intervención",
        "  y la razon, entera" );
    }
    ok( pr_sin_razon( q, sin, 16 ) == 3,
        "  y los que no la tienen SIGUEN sin tenerla" );
    ok( q->nmu == 1, "  la muestra declarada" );
    ok( pr_modelo_idx( q, "IPC_ES", "pre-covid", "m01" ) >= 0,
        "  y m01 de pre-covid, que convive con el m01 de la completa" );
    es( pr_elegido( q, "IPC_ES", "pre-covid" ), "m01",
        "  con su elegido, que es de la hoja" );
    es( pr_elegido( q, "IPC_ES", "" ), "m02", "  y el de la completa aparte" );
    es( pr_muestra_ver( q, "pre-covid" )->hasta, "12/2019", "  con su hasta" );
    es( pr_muestra_ver( q, "pre-covid" )->razon, "2020-2022 es otro proceso",
        "  y con su razon" );

    {
    const PrSerie *x = pr_serie_ver( q, "IPC_ES" );

    es( x->descripcion, "Índice de precios de consumo armonizado, España",
        "  la descripcion, con sus acentos" );
    es( x->unidades, "índice 2015 = 100",  "  las unidades" );
    es( x->fuente,   "Eurostat, tabla prc_hicp_midx", "  la fuente" );
    es( x->bajada,   "2026-09-20", "  y la fecha de la bajada" );
    es( pr_serie_ver( q, "SUELTA" )->descripcion, "",
        "  y lo que no se puso sigue VACIO: no se rellena con la clave" );
    }
    }

    printf( "\nBORRAR: LO QUE ROMPERIA EL LINAJE NO SE BORRA\n" );
    ok( pr_borra( p, "IPC_ES", "", "m00", &e ) != 0 && e.cod == PR_EDATOS,
        "los datos no se borran: son la raiz" );
    ok( pr_borra( p, "IPC_ES", "", "m01", &e ) != 0 && e.cod == PR_EHIJOS,
        "ni un modelo del que cuelga otro" );
    es( e.texto, "m02", "y se dice CUAL cuelga, para saber por donde empezar" );
    ok( pr_borra( p, "IPC_ES", "", "m77", &e ) != 0 && e.cod == PR_ENOMODELO,
        "uno que no esta, tampoco" );

    es( pr_elegido( p, "IPC_ES", "" ), "m02", "m02 era el elegido" );
    ok( pr_borra( p, "IPC_ES", "", "m02", &e ) == 0, "una hoja SI se borra" );
    ok( p->nm == 5, "y el modelo se va del manifiesto" );
    es( pr_elegido( p, "IPC_ES", "" ), "",
        "la serie se queda SIN elegido: la decision se va con el modelo" );
    ok( pr_modelo_idx( p, "IPC_ES", "", "m02" ) < 0, "y ya no se encuentra" );
    ok( pr_borra( p, "IPC_ES", "", "m01", &e ) == 0,
        "ahora m01 es hoja y se puede borrar" );
    ok( pr_es_datos( p, "IPC_ES", "", "m00" ), "y los datos siguen ahi" );

    printf( "\nUN MANIFIESTO ROTO LO DICE, NO LO ADIVINA\n" );
    {
    const char *path = pon( "ciclo.yaml",
        "schema_version: 1\nid: X\nraiz: .\n"
        "series:\n  S:\n"
        "modelos:\n"
        "  S/a:\n    version: 0\n    padre: b\n    razon: \"\"\n"
        "  S/b:\n    version: 1\n    padre: a\n    razon: \"\"\n" );

    ok( pr_leer( path, q, &e ) != 0 && e.cod == PR_ECICLO,
        "un linaje que se muerde la cola" );
    pr_error_es( &e, b, sizeof b );
    ok( strstr( b, "muerde la cola" ) != NULL, "y se dice en castellano" );
    }
    {
    const char *path = pon( "huerfano.yaml",
        "schema_version: 1\nid: X\nraiz: .\n"
        "series:\n  S:\n"
        "modelos:\n  S/a:\n    version: 0\n    padre: nadie\n    razon: \"\"\n" );

    ok( pr_leer( path, q, &e ) != 0 && e.cod == PR_EPADRE,
        "un padre que no esta" );
    }
    {
    const char *path = pon( "rara.yaml", "schema_version: 1\ncolor: azul\n" );

    ok( pr_leer( path, q, &e ) != 0 && e.cod == PR_ECLAVE,
        "una clave desconocida NO se ignora" );
    ok( e.linea == 2, "y se dice en que linea" );
    }
    {
    /* Un id de 60 caracteres: recortarlo a 47 podria hacerlo casar con OTRO. */
    const char *path = pon( "largo.yaml",
        "schema_version: 1\nid: X\nraiz: .\n"
        "series:\n  S:\n"
        "modelos:\n  S/"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:\n"
        "    version: 0\n" );

    ok( pr_leer( path, q, &e ) != 0,
        "un identificador que no cabe se RECHAZA, no se recorta" );
    }

    printf( "\n%d fallos\n", fallos );
    free( p ); free( q );
    return fallos ? 1 : 0;
}
