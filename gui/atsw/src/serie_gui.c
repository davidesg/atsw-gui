/*
 * serie_gui.c -- «Editar…» una serie: lo que es y de donde vino.
 *
 * NADA DE ESTO TOCA UN NUMERO. Son los campos que el .inp no puede llevar --
 * el motor lee un nombre y ya-- y que sin embargo son la mitad de lo que hace
 * falta para volver a un analisis seis meses despues: que serie es
 * exactamente, en que unidades, de donde se bajo y cuando.
 *
 * Y son OPCIONALES, todos. Un campo vacio se ve vacio y el manifiesto no
 * escribe la linea: "no consta" tiene que verse como no consta, igual que la
 * razon de una iteracion. Rellenarlos con el id --"descripcion: IPC_DE"--
 * seria fingir que se sabe lo que significa un mnemotecnico, que es justo lo
 * contrario de para lo que existen.
 */

#include <string.h>

#include "atsw.h"

void barra_pub( Atsw *a, const char *s );

/* Una fila del formulario: etiqueta a la izquierda, entrada a la derecha. */
static GtkWidget *fila( GtkWidget *rejilla, int y, const char *et,
                        const char *valor, const char *tip )
{
    GtkWidget *l = gtk_label_new( et );
    GtkWidget *e = gtk_entry_new();

    gtk_label_set_xalign( GTK_LABEL(l), 1.0 );
    gtk_entry_set_text( GTK_ENTRY(e), valor ? valor : "" );
    gtk_entry_set_width_chars( GTK_ENTRY(e), 44 );
    if ( tip ) gtk_widget_set_tooltip_text( e, tip );
    gtk_grid_attach( GTK_GRID(rejilla), l, 0, y, 1, 1 );
    gtk_grid_attach( GTK_GRID(rejilla), e, 1, y, 1, 1 );
    return e;
}

static void pon( char *destino, size_t n, GtkWidget *e )
{
    snprintf( destino, n, "%s", gtk_entry_get_text( GTK_ENTRY(e) ) );
}

void atsw_serie_edita( Atsw *a, const char *serie )
{
    PrSerie   *s;
    PrError    e;
    GtkWidget *d, *caja, *rej, *cab;
    GtkWidget *desc, *fue, *url, *baj, *uni, *not;
    int        r;

    if ( !a->hay || serie == NULL || !*serie ) return;
    s = pr_serie( a->p, serie );
    if ( s == NULL ) { barra_pub( a, "Esa serie no está en el proyecto." );
                       return; }

    d = gtk_dialog_new_with_buttons( "Editar la serie", GTK_WINDOW(a->ventana),
            GTK_DIALOG_MODAL, "Cancelar", GTK_RESPONSE_CANCEL,
            "Guardar", GTK_RESPONSE_OK, NULL );
    gtk_dialog_set_default_response( GTK_DIALOG(d), GTK_RESPONSE_OK );

    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );
    gtk_box_set_spacing( GTK_BOX(caja), 8 );

    /* LA CLAVE, ENSEÑADA Y NO EDITABLE AQUI. Cambiarla no es editar un
       campo: mueve el directorio y reescribe el manifiesto. Es otra cosa
       y tiene que pedirse aparte, no caer de un formulario.          */
    {
    gchar *t = g_strdup_printf(
        "<b>%s</b>\n<small>El mnemotécnico es la clave: nombra el directorio "
        "y es el nombre que el motor imprime.\nPara cambiarlo hace falta "
        "renombrar la serie, que mueve ficheros.</small>", s->id );

    cab = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL(cab), t );
    gtk_label_set_xalign( GTK_LABEL(cab), 0.0 );
    gtk_box_pack_start( GTK_BOX(caja), cab, FALSE, FALSE, 0 );
    g_free( t );
    }

    rej = gtk_grid_new();
    gtk_grid_set_row_spacing( GTK_GRID(rej), 6 );
    gtk_grid_set_column_spacing( GTK_GRID(rej), 8 );
    gtk_box_pack_start( GTK_BOX(caja), rej, TRUE, TRUE, 0 );

    desc = fila( rej, 0, "Descripción ", s->descripcion,
        "De qué va la serie, para quien no reconozca el mnemotécnico.\n"
        "«Índice de precios de consumo armonizado, Alemania»." );
    uni  = fila( rej, 1, "Unidades ", s->unidades,
        "«índice 2015 = 100», «millones de euros», «tasa anual %»." );
    fue  = fila( rej, 2, "Fuente ", s->fuente,
        "Quién la publica y en qué tabla: «Eurostat, prc_hicp_midx»." );
    url  = fila( rej, 3, "URL ", s->url,
        "De dónde se bajó. Es lo que no se puede reconstruir después." );
    baj  = fila( rej, 4, "Bajada ", s->bajada,
        "AAAA-MM-DD. La fecha de la descarga, no la del último dato: dice "
        "de qué revisión son estos números." );
    not  = fila( rej, 5, "Notas ", s->notas,
        "Lo que haya que saber al mirarla: cambios de base, rupturas, "
        "cómo se enlazó." );

    gtk_widget_show_all( d );
    r = gtk_dialog_run( GTK_DIALOG(d) );

    if ( r == GTK_RESPONSE_OK )
        {
        pon( s->descripcion, PR_TEXTO, desc );
        pon( s->unidades,    PR_TEXTO, uni );
        pon( s->fuente,      PR_TEXTO, fue );
        pon( s->url,         PR_RUTA,  url );
        pon( s->bajada,      16,       baj );
        pon( s->notas,       PR_TEXTO, not );

        if ( pr_escribir( a->p, a->p->path, &e ) != 0 )
            {
            char why[512];

            pr_error_es( &e, why, sizeof why );
            barra_pub( a, why );
            }
        else
            {
            gchar *t = g_strdup_printf( "%s: guardado.", s->id );

            barra_pub( a, t );
            g_free( t );
            }
        atsw_refresca( a );
        }
    gtk_widget_destroy( d );
}
