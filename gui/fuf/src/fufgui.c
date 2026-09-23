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
/* La tabla, leida del .out                                                  */
/* ------------------------------------------------------------------------ */

/* EL .out ENTERO Y LA TABLA, QUE NO SON LO MISMO.
 *
 * La tabla es el informe DESTILADO: las filas previstas y cuatro numeros. Va
 * bien para leer la prevision de un vistazo, pero se deja por el camino todo
 * lo demas que el motor dice -- la cabecera, los parametros con que previo,
 * la columna de error, las observaciones con que empalma. Y eso tambien hay
 * que poder leerlo, que es el informe.
 *
 * Asi que van los dos, en pestañas: son dos vistas de LO MISMO. Es la forma
 * que ya tiene el editor del .inp, y por la misma razon.               */
void fuf_trae_out( Fuf *f )
{
    GtkListStore *st = GTK_LIST_STORE( gtk_tree_view_get_model(
                                           GTK_TREE_VIEW(f->tabla) ) );
    GtkTextBuffer *tb = gtk_text_view_get_buffer( GTK_TEXT_VIEW(f->salida) );
    GtkTreeIter   it;
    Forecast     *fc;
    gchar        *txt = NULL, *out;
    gsize         n = 0;
    int           i, j;

    gtk_list_store_clear( st );

    out = g_strdup_printf( "%s/%s.out", f->dir, f->prev );
    if ( !g_file_get_contents( out, &txt, &n, NULL ) )
        {
        gtk_text_buffer_set_text( tb,
            "Todavía no hay informe: sale al prever.", -1 );
        g_free( out );
        return;
        }
    g_free( out );

    /* EL INFORME, TAL CUAL LO ESCRIBIO EL MOTOR. Sin resumir: lo que se lee
       aqui es lo que hay en el fichero.                                */
    gtk_text_buffer_set_text( tb, txt, (gint) n );

    fc = g_new0( Forecast, 1 );
    of_parse( txt, fc );
    g_free( txt );

    for ( i = 0; i < fc->ns; i++ )
        {
        const OfSerie *s = &fc->s[i];

        for ( j = 0; j < s->nf; j++ )
            {
            const OfFila *r = &s->f[j];
            char nivel[32], sd[32], var[32], anual[32];

            /* SOLO LO PREVISTO. Las filas de antes del origen son el pasado
               --el motor las imprime para que el grafico empalme-- y
               ponerlas en la tabla de previsiones seria decir que se
               previeron.                                               */
            if ( !r->tiene_sd ) continue;

            g_snprintf( nivel, sizeof nivel, "%.4f", r->nivel );
            g_snprintf( sd,    sizeof sd,    "%.4f", r->sd_nivel );
            g_snprintf( var,   sizeof var,   "%.2f", r->var_per );
            g_snprintf( anual, sizeof anual, "%.2f", r->var_anu );

            gtk_list_store_append( st, &it );
            gtk_list_store_set( st, &it,
                FC_FECHA, r->fecha, FC_NIVEL, nivel, FC_SD, sd,
                FC_VAR, var, FC_ANUAL, anual, -1 );
            }

        if ( s->origen[0] )
            fuf_di( f, "%s: %d previsiones desde %s.",
                    s->nombre, s->nprev, s->origen );
        }
    g_free( fc );
}
