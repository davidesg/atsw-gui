/*
 * serie_gui.c -- los formularios de texto: la serie y el proyecto.
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
GtkWidget *atsw_fila( GtkWidget *rejilla, int y, const char *et,
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

    desc = atsw_fila( rej, 0, "Descripción ", s->descripcion,
        "De qué va la serie, para quien no reconozca el mnemotécnico.\n"
        "«Índice de precios de consumo armonizado, Alemania»." );
    uni  = atsw_fila( rej, 1, "Unidades ", s->unidades,
        "«índice 2015 = 100», «millones de euros», «tasa anual %»." );
    fue  = atsw_fila( rej, 2, "Fuente ", s->fuente,
        "Quién la publica y en qué tabla: «Eurostat, prc_hicp_midx»." );
    url  = atsw_fila( rej, 3, "URL ", s->url,
        "De dónde se bajó. Es lo que no se puede reconstruir después." );
    baj  = atsw_fila( rej, 4, "Bajada ", s->bajada,
        "AAAA-MM-DD. La fecha de la descarga, no la del último dato: dice "
        "de qué revisión son estos números." );
    not  = atsw_fila( rej, 5, "Notas ", s->notas,
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

/* ------------------------------------------------------------------------ */
/* UN SELECTOR DE FECHA: PERIODO Y AÑO, ACOTADOS A LOS DATOS                 */
/*                                                                           */
/* Una fecha de esta escuela no es un numero, son DOS -- periodo y año -- y   */
/* por eso son dos controles y no uno. Escribirla a mano era una trampa: con  */
/* "2019-12" en vez de "12/2019" la ventana no se aplicaba y se estimaba      */
/* sobre la muestra entera sin que nada lo dijera.                            */
/*                                                                           */
/* El de periodo va 1..freq, asi que se ajusta solo a la frecuencia, y        */
/* DESAPARECE en anual, que alli la fecha es solo el año. Y DA LA VUELTA:     */
/* subir de 12 pone 1 y suma un año, que es lo que uno espera de un boton     */
/* que camina por el tiempo.                                                  */
/* ------------------------------------------------------------------------ */

static void fecha_gira( GtkSpinButton *sp, AtFecha *F )
{
    int p = gtk_spin_button_get_value_as_int( sp );

    if ( F->freq <= 1 || F->girando ) return;

    /* El rango del periodo es 0..freq+1 para poder VER el desbordamiento;
       lo que se enseña nunca se queda fuera de 1..freq.               */
    F->girando = TRUE;
    if ( p > F->freq )
        {
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(F->anio),
            gtk_spin_button_get_value( GTK_SPIN_BUTTON(F->anio) ) + 1 );
        gtk_spin_button_set_value( sp, 1 );
        }
    else if ( p < 1 )
        {
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(F->anio),
            gtk_spin_button_get_value( GTK_SPIN_BUTTON(F->anio) ) - 1 );
        gtk_spin_button_set_value( sp, F->freq );
        }
    F->girando = FALSE;
}

GtkWidget *atsw_fecha_nueva( AtFecha *F, int freq, int anio, int per,
                             int a1, int a2 )
{
    GtkWidget *caja = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 4 );

    memset( F, 0, sizeof *F );
    F->freq = freq > 0 ? freq : 1;

    if ( F->freq > 1 )
        {
        F->per = gtk_spin_button_new_with_range( 0, F->freq + 1, 1 );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON(F->per),
                                   per > 0 ? per : 1 );
        gtk_widget_set_tooltip_text( F->per, F->freq == 12
            ? "El mes. Al pasar de 12 salta al año siguiente."
            : "El período dentro del año. Al pasar del último salta al "
              "siguiente." );
        g_signal_connect( F->per, "value-changed",
                          G_CALLBACK(fecha_gira), F );
        gtk_box_pack_start( GTK_BOX(caja), F->per, FALSE, FALSE, 0 );
        gtk_box_pack_start( GTK_BOX(caja), gtk_label_new( "/" ),
                            FALSE, FALSE, 0 );
        }

    /* ACOTADO AL TRAMO REAL: no se puede pedir una ventana que no existe. */
    F->anio = gtk_spin_button_new_with_range( a1 > 0 ? a1 : 1,
                                              a2 > 0 ? a2 : 9999, 1 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(F->anio), anio > 0 ? anio : a1 );
    gtk_widget_set_tooltip_text( F->anio, "El año, entre los que hay datos." );
    gtk_box_pack_start( GTK_BOX(caja), F->anio, FALSE, FALSE, 0 );
    return caja;
}

const char *atsw_fecha_texto( const AtFecha *F, char *out, size_t n )
{
    int y = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(F->anio) );

    if ( out == NULL || n == 0 ) return out;
    if ( F->freq <= 1 ) g_snprintf( out, n, "%d", y );
    else
        g_snprintf( out, n, "%d/%d",
            gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(F->per) ), y );
    return out;
}


/* ------------------------------------------------------------------------ */
/* «Información del proyecto…»                                              */
/*                                                                           */
/* El manifiesto lleva id, titulo, analista y creado, y hasta ahora NO HABIA */
/* FORMA DE PONERLOS salvo editando el fichero a mano: «Nuevo…» ponia el id  */
/* del nombre del fichero y el titulo vacio. Es el hermano de «Editar la     */
/* serie…», y por la misma razon -- son los campos que ningun .inp puede     */
/* llevar y que hacen falta para volver a esto meses despues.                */
/*                                                                           */
/* LA RAIZ Y LA RUTA SE ENSEÑAN Y NO SE EDITAN. La raiz se resuelve CONTRA   */
/* EL DIRECTORIO DEL MANIFIESTO, asi que cambiar cualquiera de las dos aqui  */
/* dejaria el proyecto apuntando a un sitio sin series. Mover un proyecto es */
/* mover el manifiesto Y las carpetas de las series: es una operacion, no un */
/* campo de un formulario.                                                   */
/* ------------------------------------------------------------------------ */

void atsw_proyecto_edita( Atsw *a )
{
    PrError    e;
    GtkWidget *d, *caja, *rej;
    GtkWidget *e_id, *e_tit, *e_ana;
    int        r;

    if ( !a->hay ) { barra_pub( a, "Abre un proyecto antes." ); return; }

    d = gtk_dialog_new_with_buttons( "Información del proyecto",
            GTK_WINDOW(a->ventana), GTK_DIALOG_MODAL,
            "Cancelar", GTK_RESPONSE_CANCEL, "Guardar", GTK_RESPONSE_OK, NULL );
    gtk_dialog_set_default_response( GTK_DIALOG(d), GTK_RESPONSE_OK );

    caja = gtk_dialog_get_content_area( GTK_DIALOG(d) );
    gtk_container_set_border_width( GTK_CONTAINER(caja), 10 );
    gtk_box_set_spacing( GTK_BOX(caja), 8 );

    rej = gtk_grid_new();
    gtk_grid_set_row_spacing( GTK_GRID(rej), 6 );
    gtk_grid_set_column_spacing( GTK_GRID(rej), 8 );
    gtk_box_pack_start( GTK_BOX(caja), rej, TRUE, TRUE, 0 );

    e_id  = atsw_fila( rej, 0, "Identificador ", a->p->id,
        "Corto, para nombrarlo: «SF_MEG», «emu». No nombra ningún "
        "directorio, así que cambiarlo no mueve nada." );
    e_tit = atsw_fila( rej, 1, "Título ", a->p->titulo,
        "De qué va este proyecto, en una línea." );
    e_ana = atsw_fila( rej, 2, "Analista ", a->p->analista,
        "Quién lo lleva. Dentro de un año, saberlo no es trivial." );

    /* LO QUE NO SE EDITA, pero sí se enseña: es la mitad de lo que hay que
     * saber de un proyecto, y esconderlo no lo hace menos cierto.      */
    {
    GtkWidget *l = gtk_label_new( NULL );
    gchar     *t = g_markup_printf_escaped(
        "<small>Creado el %s.\n"
        "Manifiesto: %s\n"
        "Raíz de los datos: %s (relativa al manifiesto)\n\n"
        "<b>El proyecto se guarda solo</b> en cada cambio: dar de alta una "
        "serie,\nderivar, iterar, elegir, poner una razón. No hay que "
        "guardarlo a mano.\n\n"
        "Para llevarlo a otro sitio hay que mover el manifiesto <b>y las "
        "carpetas\nde las series</b>: mover sólo este fichero lo dejaría "
        "apuntando a un\ndirectorio sin datos.</small>",
        a->p->creado[0] ? a->p->creado : "(no consta)",
        a->p->path, a->p->raiz );

    gtk_label_set_markup( GTK_LABEL(l), t );
    gtk_label_set_xalign( GTK_LABEL(l), 0.0 );
    gtk_box_pack_start( GTK_BOX(caja), l, FALSE, FALSE, 0 );
    g_free( t );
    }

    gtk_widget_show_all( d );
    r = gtk_dialog_run( GTK_DIALOG(d) );

    if ( r == GTK_RESPONSE_OK )
        {
        char nuevo[PR_ID];

        /* El identificador se limpia como los demás: lo que no cabe o no
           vale no se guarda a medias.                                  */
        a_id( gtk_entry_get_text( GTK_ENTRY(e_id) ), nuevo, sizeof nuevo );
        if ( nuevo[0] ) snprintf( a->p->id, PR_ID, "%s", nuevo );

        snprintf( a->p->titulo, PR_TEXTO, "%s",
                  gtk_entry_get_text( GTK_ENTRY(e_tit) ) );
        snprintf( a->p->analista, PR_TEXTO, "%s",
                  gtk_entry_get_text( GTK_ENTRY(e_ana) ) );

        if ( pr_escribir( a->p, a->p->path, &e ) != 0 )
            { char why[512]; pr_error_es( &e, why, sizeof why );
              barra_pub( a, why ); }
        else
            barra_pub( a, "Proyecto guardado." );
        atsw_refresca( a );
        }
    gtk_widget_destroy( d );
}
