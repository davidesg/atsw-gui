/*
 * datos_gui.c -- «Datos…»: de un fichero a n series del proyecto.
 *
 * ES EL ESLABON QUE EL INVENTARIO ENCONTRO ROTO. Habia cuatro lectores de
 * datos que discrepaban, y ninguno hacia lo unico que importa: convertir un
 * fichero de n columnas en n SERIES de un proyecto, con su primer .inp. El
 * camino "datos -> .inp(-1)" estaba sin recorrer.
 *
 *     .xlsx / .csv / .txt  ──→  n × .inp(-1)  ──fue──→  .pre  ──→ ...
 *
 * NO SE ABRE UN QUINTO ESCRITOR DE .inp. El estudio del contrato conto CUATRO
 * --gtkfue, pyart, pypre y el motor-- y midio que cada divergencia salia de
 * que el mismo fichero viviera en dos sitios. Aqui se enlaza el de fug,
 * engines/fug/src/inpfile.c, que no depende de nada: lo que ese escritor haga
 * es lo que este gesto hace, por construccion y no por parecido.
 *
 * LO QUE SE ESCRIBE ES UN .inp SIN MODELO: la serie, su Box-Cox y nada mas.
 * Poner un modelo aqui seria inventarselo -- identificar es de fue.
 */

#include <string.h>

#include <glib/gstdio.h>

#include "atsw.h"
#include "datos.h"
#include "inpfile.h"

void barra_pub( Atsw *a, const char *s );

enum { C_USAR, C_NOMBRE, C_OBS, C_PRIMERO, C_ULTIMO, C_COL, C_N };

typedef struct {
   Atsw      *a;
   DtDatos   *d;
   GtkWidget *lista;
   GtkWidget *l_aviso;
   gchar     *path;
} Dialogo;

/* Un nombre que valga de ID de serie.
 *
 * LO QUE HAY QUE QUITAR ES EL ESPACIO, NO EL ACENTO. Los motores leen el
 * nombre con "%s", asi que lo unico que parte la linea del .inp es un blanco.
 * La primera version filtraba a-zA-Z0-9 y convertia «España» en «Espaa»:
 * destrozaba el nombre para protegerse de un problema que no existe.
 *
 * Se conservan los bytes >= 0x80 --las letras acentuadas son UTF-8 de varios
 * bytes-- y se cambian por '_' los blancos y los separadores de ruta, que si
 * darian guerra en un nombre de fichero.                                */
static void a_id( const char *s, char *out, size_t n )
{
    const unsigned char *p = (const unsigned char *) s;
    size_t i = 0;

    for ( ; *p && i + 1 < n; p++ )
        {
        if ( *p >= 0x80 ||                                /* UTF-8: se queda */
             ( *p >= 'A' && *p <= 'Z' ) || ( *p >= 'a' && *p <= 'z' ) ||
             ( *p >= '0' && *p <= '9' ) || *p == '_' || *p == '-' )
            out[i++] = (char) *p;
        else if ( *p == ' ' || *p == '\t' || *p == '.' ||
                  *p == '/' || *p == '\\' )
            { if ( i && out[i-1] != '_' ) out[i++] = '_'; }
        }
    while ( i && out[i-1] == '_' ) i--;
    out[i] = '\0';
    if ( out[0] == '\0' ) snprintf( out, n, "serie" );
}

static void on_usar( GtkCellRendererToggle *r, gchar *ruta, Dialogo *D )
{
    GtkTreeModel *mo = gtk_tree_view_get_model( GTK_TREE_VIEW(D->lista) );
    GtkTreeIter   it;
    gboolean      v;

    (void) r;
    if ( !gtk_tree_model_get_iter_from_string( mo, &it, ruta ) ) return;
    gtk_tree_model_get( mo, &it, C_USAR, &v, -1 );
    gtk_list_store_set( GTK_LIST_STORE(mo), &it, C_USAR, !v, -1 );
}

/* Da de alta la serie: ESCRIBE EL .csv Y GENERA EL .inp DE EL.
 *
 * El orden importa y es el del contrato nuevo: primero el dato --la muestra
 * total, tal como entro-- y despues lo que se deriva de el. Si el .inp se
 * escribiera aparte habria dos copias de los mismos numeros y, tarde o
 * temprano, dos que no coinciden.
 *
 * 0 si pudo.                                                             */
static int da_de_alta( Atsw *a, const DtDatos *d, int col, const char *serie,
                       int freq, int anio, int per, char *why, size_t n )
{
    DtDatos *uno;
    DtError  de;
    PrError  e;
    char     id[PR_ID], ruta[PR_RUTA], csv[PR_RUTA], *dir;
    int      i, rc;

    if ( pr_serie_idx( a->p, serie ) < 0 &&
         pr_serie_add( a->p, serie, &e ) != 0 )
        { pr_error_es( &e, why, n ); return 1; }

    /* UNA SERIE TIENE UNOS DATOS, Y SON ESTOS. Volver a cargarla encima
       cambiaria el suelo bajo modelos ya estimados sin que nada lo dijera:
       el .out seguiria ahi, calculado sobre otros numeros. Asi que se para
       y lo decide el analista -- otro nombre, u otro proyecto.         */
    if ( pr_datos_de( a->p, serie )[0] )
        { snprintf( why, n, "«%s» ya tiene datos en este proyecto. No los "
                            "piso: cárgala con otro nombre, o en otro "
                            "proyecto.", serie );
          return 1; }

    /* --- 1. EL DATO ----------------------------------------------------- */
    if ( atsw_csv_de( a->p, serie, csv, sizeof csv ) != 0 )
        { snprintf( why, n, "No pude componer la ruta de «%s».", serie );
          return 1; }

    dir = g_path_get_dirname( csv );
    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );

    /* Una columna del fichero es UNA serie: se saca a su propio DtDatos y
       se escribe con lo que sabemos de frecuencia y fecha.            */
    uno = g_new0( DtDatos, 1 );
    uno->ncol = 1;
    uno->nobs = d->nobs;
    uno->freq = freq;
    uno->anio = anio;
    uno->per  = per;
    snprintf( uno->nombre[0], DT_NOMBRE, "%s", serie );
    for ( i = 0; i < d->nobs; i++ ) uno->v[0][i] = d->v[col][i];
    if ( d->tiene_fechas )
        {
        uno->tiene_fechas = 1;
        for ( i = 0; i < d->nobs; i++ )
            snprintf( uno->fecha[i], DT_FECHA, "%s", d->fecha[i] );
        }

    rc = dt_escribir( csv, uno, &de );
    g_free( uno );
    if ( rc != 0 ) { dt_error_es( &de, why, n ); return 1; }

    /* --- 2. LO QUE SE DERIVA DE EL -------------------------------------- */
    /* LA RAIZ DE LA CADENA SON LOS DATOS, y se declara como tal. No es un
       modelo: nadie la eligio. Y NADIE LA EDITA -- quien vaya a especificar
       deriva uno nuevo, asi que este .inp sigue estando para los graficos y
       el linaje conserva su raiz.                                      */
    if ( pr_deriva_rol( a->p, serie, NULL, PR_DATOS, id, sizeof id,
                        ruta, sizeof ruta, &e ) != 0 )
        { pr_error_es( &e, why, n ); return 1; }

    dir = g_path_get_dirname( ruta );
    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );

    /* Sin ventana: el nodo de datos es la muestra TOTAL. */
    return atsw_genera_inp( csv, ruta, serie, "", why, n );
}

static void on_importar( GtkButton *b, Dialogo *D )
{
    GtkTreeModel *mo = gtk_tree_view_get_model( GTK_TREE_VIEW(D->lista) );
    GtkTreeIter   it;
    PrError       e;
    GString      *hechas = g_string_new( NULL );
    char          why[512] = "";
    int           n = 0, fallos = 0;

    (void) b;
    if ( !gtk_tree_model_get_iter_first( mo, &it ) ) return;

    do {
        gboolean usar;
        gchar   *nombre = NULL;
        gint     col;

        gtk_tree_model_get( mo, &it, C_USAR, &usar, C_NOMBRE, &nombre,
                            C_COL, &col, -1 );
        if ( usar )
            {
            char id[PR_ID];

            a_id( nombre, id, sizeof id );
            if ( da_de_alta( D->a, D->d, col, id, D->d->freq, D->d->anio,
                             D->d->per, why, sizeof why ) == 0 )
                { g_string_append_printf( hechas, "%s%s", n ? ", " : "", id );
                  n++; }
            else
                fallos++;
            }
        g_free( nombre );
    } while ( gtk_tree_model_iter_next( mo, &it ) );

    if ( n && pr_escribir( D->a->p, D->a->p->path, &e ) != 0 )
        { pr_error_es( &e, why, sizeof why ); fallos++; }

    if ( fallos )
        barra_pub( D->a, why );
    else if ( n )
        {
        gchar *s = g_strdup_printf( "%d serie%s del proyecto: %s. "
                                    "Ábrelas en fue para identificarlas.",
                                    n, n == 1 ? "" : "s", hechas->str );

        barra_pub( D->a, s );
        g_free( s );
        }
    else
        barra_pub( D->a, "No marcaste ninguna columna." );

    g_string_free( hechas, TRUE );
    atsw_refresca( D->a );
}

/* ------------------------------------------------------------------------ */

void atsw_datos( Atsw *a )
{
    GtkWidget    *d, *caja, *sc, *bt;
    GtkFileFilter *f;
    DtDatos      *dd;
    DtError       e;
    Dialogo       D;
    gchar        *path;
    char          b[512];
    int           i;

    if ( !a->hay )
        { barra_pub( a, "Abre un proyecto antes: las series van dentro de uno." );
          return; }

    d = gtk_file_chooser_dialog_new( "Cargar datos",
            GTK_WINDOW(a->ventana), GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Cancelar", GTK_RESPONSE_CANCEL, "_Abrir", GTK_RESPONSE_ACCEPT,
            NULL );
    f = gtk_file_filter_new();
    gtk_file_filter_set_name( f, "Datos (*.xlsx *.csv *.txt *.dat)" );
    gtk_file_filter_add_pattern( f, "*.xlsx" );
    gtk_file_filter_add_pattern( f, "*.csv" );
    gtk_file_filter_add_pattern( f, "*.txt" );
    gtk_file_filter_add_pattern( f, "*.dat" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(d), f );
    f = gtk_file_filter_new();
    gtk_file_filter_set_name( f, "Todos" );
    gtk_file_filter_add_pattern( f, "*" );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(d), f );

    if ( gtk_dialog_run( GTK_DIALOG(d) ) != GTK_RESPONSE_ACCEPT )
        { gtk_widget_destroy( d ); return; }
    path = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );
    gtk_widget_destroy( d );

    dd = g_new0( DtDatos, 1 );
    if ( dt_leer( path, dd, &e ) != 0 )
        {
        dt_error_es( &e, b, sizeof b );
        barra_pub( a, b );
        g_free( dd ); g_free( path );
        return;
        }

    /* --- el dialogo: QUE columnas, y que se ve antes de importar -------- */
    d = gtk_dialog_new_with_buttons( "Qué series cargar", GTK_WINDOW(a->ventana),
            GTK_DIALOG_MODAL, "_Cerrar", GTK_RESPONSE_CLOSE, NULL );
    gtk_window_set_default_size( GTK_WINDOW(d), 620, 420 );
    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );

    D.a = a; D.d = dd; D.path = path;

    /* Lo que el FICHERO dijo, y lo que no. Que se vea antes de importar. */
    D.l_aviso = gtk_label_new( NULL );
    gtk_label_set_line_wrap( GTK_LABEL(D.l_aviso), TRUE );
    gtk_widget_set_halign( D.l_aviso, GTK_ALIGN_START );
    if ( dd->freq > 0 )
        snprintf( b, sizeof b,
            "%d columnas, %d observaciones. El fichero dice: frecuencia %d, "
            "desde %d/%d.", dd->ncol, dd->nobs, dd->freq, dd->per, dd->anio );
    else
        snprintf( b, sizeof b,
            "%d columnas, %d observaciones. EL FICHERO NO DICE LA FRECUENCIA "
            "NI LA FECHA: se cargarán como anuales desde el año 1, y habrá que "
            "corregirlas en fue. Una serie mal fechada envenena todo lo que "
            "venga después.", dd->ncol, dd->nobs );
    gtk_label_set_text( GTK_LABEL(D.l_aviso), b );
    gtk_box_pack_start( GTK_BOX(caja), D.l_aviso, FALSE, FALSE, 4 );

    {
    GtkListStore    *st = gtk_list_store_new( C_N, G_TYPE_BOOLEAN,
                              G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING,
                              G_TYPE_STRING, G_TYPE_INT );
    GtkCellRenderer *r;
    GtkTreeIter      it;

    D.lista = gtk_tree_view_new_with_model( GTK_TREE_MODEL(st) );

    r = gtk_cell_renderer_toggle_new();
    g_signal_connect( r, "toggled", G_CALLBACK(on_usar), &D );
    gtk_tree_view_append_column( GTK_TREE_VIEW(D.lista),
        gtk_tree_view_column_new_with_attributes( "Cargar", r, "active",
                                                  C_USAR, NULL ) );
    r = gtk_cell_renderer_text_new();
    gtk_tree_view_append_column( GTK_TREE_VIEW(D.lista),
        gtk_tree_view_column_new_with_attributes( "Serie", r, "text",
                                                  C_NOMBRE, NULL ) );
    gtk_tree_view_append_column( GTK_TREE_VIEW(D.lista),
        gtk_tree_view_column_new_with_attributes( "Obs", r, "text",
                                                  C_OBS, NULL ) );
    gtk_tree_view_append_column( GTK_TREE_VIEW(D.lista),
        gtk_tree_view_column_new_with_attributes( "Primero", r, "text",
                                                  C_PRIMERO, NULL ) );
    gtk_tree_view_append_column( GTK_TREE_VIEW(D.lista),
        gtk_tree_view_column_new_with_attributes( "Último", r, "text",
                                                  C_ULTIMO, NULL ) );

    for ( i = 0; i < dd->ncol; i++ )
        {
        char nom[DT_NOMBRE], p1[32], p2[32];

        if ( dd->nombre[i][0] ) snprintf( nom, sizeof nom, "%s", dd->nombre[i] );
        else                    snprintf( nom, sizeof nom, "columna %d", i + 1 );
        snprintf( p1, sizeof p1, "%.6g", dd->v[i][0] );
        snprintf( p2, sizeof p2, "%.6g", dd->v[i][dd->nobs - 1] );

        gtk_list_store_append( st, &it );
        gtk_list_store_set( st, &it, C_USAR, TRUE, C_NOMBRE, nom,
                            C_OBS, dd->nobs, C_PRIMERO, p1, C_ULTIMO, p2,
                            C_COL, i, -1 );
        }

    sc = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sc),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER(sc), D.lista );
    gtk_box_pack_start( GTK_BOX(caja), sc, TRUE, TRUE, 4 );
    }

    bt = gtk_button_new_with_label( "Cargar las marcadas" );
    gtk_widget_set_tooltip_text( bt,
        "Cada columna marcada será una SERIE del proyecto, con su primer .inp "
        "—sin modelo: identificar es de fue— y su linaje empezado." );
    g_signal_connect( bt, "clicked", G_CALLBACK(on_importar), &D );
    gtk_box_pack_start( GTK_BOX(caja), bt, FALSE, FALSE, 4 );

    gtk_widget_show_all( d );
    gtk_dialog_run( GTK_DIALOG(d) );
    gtk_widget_destroy( d );

    g_free( dd );
    g_free( path );
}
