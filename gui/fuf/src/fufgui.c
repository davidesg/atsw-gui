/*
 * fufgui.c -- la ventana: previsión, tabla y gráfico.
 */

#include <string.h>
#include <stdlib.h>

#include <glib/gstdio.h>

#include "fufgui.h"

#include "rutas.h"

/* ------------------------------------------------------------------------ */
/* Decir cosas                                                               */
/* ------------------------------------------------------------------------ */

void fuf_di( Fuf *f, const char *fmt, ... )
{
    va_list ap;
    gchar  *s;

    va_start( ap, fmt );
    s = g_strdup_vprintf( fmt, ap );
    va_end( ap );
    gtk_label_set_text( GTK_LABEL(f->estado), s );
    gtk_widget_set_tooltip_text( f->estado, s );
    g_free( s );
}

/* LA CONSOLA ES UN REGISTRO: lo de antes no se borra. Ver la orden anterior
 * al lado de la de ahora es lo que deja entender que cambio entre las dos,
 * y es lo que hace reproducible fuera lo que acaba de pasar aqui.      */
void fuf_consola( Fuf *f, const char *fmt, ... )
{
    GtkTextBuffer *b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(f->consola) );
    GtkTextIter    fin;
    GtkTextMark   *m;
    va_list        ap;
    gchar         *s;

    va_start( ap, fmt );
    s = g_strdup_vprintf( fmt, ap );
    va_end( ap );

    gtk_text_buffer_get_end_iter( b, &fin );
    gtk_text_buffer_insert( b, &fin, s, -1 );
    g_free( s );

    gtk_text_buffer_get_end_iter( b, &fin );
    m = gtk_text_buffer_create_mark( b, NULL, &fin, FALSE );
    gtk_text_view_scroll_mark_onscreen( GTK_TEXT_VIEW(f->consola), m );
    gtk_text_buffer_delete_mark( b, m );
}


/* ------------------------------------------------------------------------ */
/* EL HORIZONTE VIVE EN EL .inp, y por eso se cambia ahi                     */
/*                                                                           */
/* "fue -f" genera el forecast_<base>.inp con un horizonte, y fuf lo lee de   */
/* ese fichero. Cambiarlo por la linea de ordenes seria pisar por comando la  */
/* especificacion que el fichero declara -- la regla que ya costo la opcion   */
/* -B de fug. Asi que se toca EL FICHERO, que es donde la especificacion      */
/* vive, y despues se corre el motor sin decirle nada mas.                    */
/*                                                                           */
/* La linea es la que sigue a "** Forecast horizon": el .inp de esta escuela  */
/* es POSICIONAL bajo sus rotulos, no un diccionario.                         */
/* ------------------------------------------------------------------------ */

int fuf_pon_horizonte( const char *inp, int h, char *why, size_t n )
{
    gchar  *txt = NULL, **lin;
    GString *s;
    gsize   len = 0;
    int     i, puesto = 0;

    if ( why && n ) why[0] = '\0';
    if ( h < 1 ) { if ( why ) g_snprintf( why, n, "El horizonte es al menos 1." );
                   return 1; }
    if ( !g_file_get_contents( inp, &txt, &len, NULL ) )
        { if ( why ) g_snprintf( why, n, "No pude leer %s.", inp ); return 1; }

    lin = g_strsplit( txt, "\n", -1 );
    g_free( txt );
    s = g_string_new( NULL );

    for ( i = 0; lin[i]; i++ )
        {
        if ( puesto == 1 )
            {
            /* La varianza de la innovacion va en esta misma linea y ES DEL
               MOTOR: se conserva tal cual y solo se cambia el horizonte. */
            double var = 0.0;
            int    viejo = 0;

            if ( sscanf( lin[i], " %d %lf", &viejo, &var ) == 2 )
                { g_string_append_printf( s, "%d  %.10f\n", h, var );
                  puesto = 2; continue; }
            }
        if ( strstr( lin[i], "Forecast horizon" ) ) puesto = 1;
        g_string_append( s, lin[i] );
        if ( lin[i + 1] ) g_string_append_c( s, '\n' );
        }
    g_strfreev( lin );

    if ( puesto != 2 )
        {
        /* NO SE ADIVINA DONDE IBA. Un .inp sin ese rotulo no es un .inp de
           fuf, y escribir el numero a ojo lo romperia mas.             */
        g_string_free( s, TRUE );
        if ( why ) g_snprintf( why, n, "%s no declara un horizonte: ¿es un "
                               ".inp de fuf?", inp );
        return 1;
        }

    if ( !g_file_set_contents( inp, s->str, -1, NULL ) )
        { g_string_free( s, TRUE );
          if ( why ) g_snprintf( why, n, "No pude escribir %s.", inp );
          return 1; }
    g_string_free( s, TRUE );
    return 0;
}


/* ------------------------------------------------------------------------ */
/* EL INFORME, TAL CUAL                                                      */
/*                                                                           */
/* Se carga el .out y ya. Hubo aqui una tabla que lo destilaba en cinco       */
/* columnas, y la quite: el motor ya escribe el informe entero y bien, y      */
/* resumirlo era ofrecer una segunda version de lo mismo -- con la pregunta   */
/* de cual de las dos manda.                                                  */
/*                                                                           */
/* De lib/outfcst se sigue usando el origen, que va a la barra de estado: eso */
/* no es una version del informe, es saber DESDE DONDE se previo sin tener    */
/* que buscarlo con los ojos.                                                 */
/* ------------------------------------------------------------------------ */

void fuf_trae_out( Fuf *f )
{
    GtkTextBuffer *tb = gtk_text_view_get_buffer( GTK_TEXT_VIEW(f->salida) );
    Forecast      *fc;
    gchar         *txt = NULL, *out;
    gsize          n = 0;
    int            i;

    out = g_strdup_printf( "%s/%s.out", f->dir, f->prev );
    if ( !g_file_get_contents( out, &txt, &n, NULL ) )
        {
        gtk_text_buffer_set_text( tb,
            "Todavía no hay informe: sale al prever.", -1 );
        g_free( out );
        return;
        }
    g_free( out );

    gtk_text_buffer_set_text( tb, txt, (gint) n );

    fc = g_new0( Forecast, 1 );
    of_parse( txt, fc );
    g_free( txt );

    for ( i = 0; i < fc->ns; i++ )
        if ( fc->s[i].origen[0] )
            fuf_di( f, "%s: %d previsiones desde %s, horizonte %d.",
                    fc->s[i].nombre, fc->s[i].nprev, fc->s[i].origen,
                    fc->s[i].lead );
    g_free( fc );
}
