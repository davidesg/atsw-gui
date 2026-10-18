/*
 * cuaderno_gui.c -- «Cuaderno…»: el cuaderno del proyecto, para leerlo y
 * anotarlo.
 *
 * QUIEN ESCRIBE NO ES ESTA VENTANA. Las entradas las mandan las ventanas de
 * analisis con su boton «Al cuaderno» (lib/analisis, lib/cuaderno), desde la
 * madre o desde fue_gui. Aqui se LEE, en orden, y se añade lo unico que no
 * sale de ninguna ventana: una nota del analista.
 *
 * SE ENSEÑA EL MARKDOWN TAL CUAL, sin pintarlo. Es texto pensado para leerse
 * como texto, y un visor de Markdown dentro de la madre seria una dependencia
 * para algo que cualquier editor ya hace. «Abrir fuera» lo da al programa
 * que el sistema tenga para .md, que si sabe pintar tablas e imagenes.
 */

#include <string.h>

#include "cuaderno.h"

#include "atsw.h"

void barra_pub( Atsw *a, const char *s );

typedef struct {
   Atsw      *a;
   GtkWidget *win, *vista, *nota, *l_ruta;
   char       md[PR_RUTA];
} Cu;

static Cu *abierto = NULL;          /* una ventana, reutilizada           */

static void carga( Cu *c )
{
   GtkTextBuffer *b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(c->vista) );
   gchar         *s = NULL;
   GtkTextIter    fin;

   if ( g_file_get_contents( c->md, &s, NULL, NULL ) && s && *s )
      gtk_text_buffer_set_text( b, s, -1 );
   else
      gtk_text_buffer_set_text( b,
         "Todavía no hay cuaderno.\n\n"
         "Lo empieza el primer «Al cuaderno» de una ventana de análisis "
         "(Diagnosis, Ganancia, Anómalos, Identificar, los contrastes "
         "formales) o una nota escrita aquí abajo.", -1 );
   g_free( s );

   /* AL FINAL: lo ultimo que se mando es lo que se viene a ver. */
   gtk_text_buffer_get_end_iter( b, &fin );
   gtk_text_buffer_place_cursor( b, &fin );
   gtk_text_view_scroll_to_mark( GTK_TEXT_VIEW(c->vista),
                                 gtk_text_buffer_get_insert( b ),
                                 0.0, TRUE, 0.0, 1.0 );
}

static void on_nota( GtkButton *bt, Cu *c )
{
   GtkTextBuffer *b = gtk_text_view_get_buffer( GTK_TEXT_VIEW(c->nota) );
   GtkTextIter    i0, i1;
   gchar         *t;
   char           why[PR_RUTA + 64];

   (void) bt;
   gtk_text_buffer_get_bounds( b, &i0, &i1 );
   t = gtk_text_buffer_get_text( b, &i0, &i1, FALSE );
   if ( cu_nota( c->a->p->path, t, why, sizeof why ) != 0 )
      barra_pub( c->a, why );
   else
      {
      /* La nota se va SOLO si se escribio: si falla, sigue ahi. */
      gtk_text_buffer_set_text( b, "", -1 );
      barra_pub( c->a, "Nota añadida al cuaderno." );
      carga( c );
      }
   g_free( t );
}

static void on_releer( GtkButton *b, Cu *c ) { (void) b; carga( c ); }

static void on_fuera( GtkButton *b, Cu *c )
{
   GError *e = NULL;
   gchar  *uri;

   (void) b;
   if ( !g_file_test( c->md, G_FILE_TEST_EXISTS ) )
      { barra_pub( c->a, "Todavía no hay cuaderno que abrir." ); return; }
   uri = g_filename_to_uri( c->md, NULL, NULL );
   if ( uri == NULL || !gtk_show_uri_on_window( GTK_WINDOW(c->win), uri,
                                                GDK_CURRENT_TIME, &e ) )
      {
      gchar *m = g_strdup_printf( "No pude abrirlo fuera: %s",
                                  e ? e->message : c->md );

      barra_pub( c->a, m );
      g_free( m );
      }
   g_clear_error( &e );
   g_free( uri );
}

static void on_cerrar( GtkWidget *w, Cu *c )
{
   (void) w;
   if ( abierto == c ) abierto = NULL;
   g_free( c );
}

static gboolean on_foco( GtkWidget *w, GdkEventFocus *ev, Cu *c )
{
   /* Las entradas llegan de OTRAS ventanas (y de fue_gui, que es otro
      proceso): al volver aqui se relee.                              */
   (void) w; (void) ev;
   carga( c );
   return FALSE;
}

void atsw_cuaderno( Atsw *a )
{
   Cu        *c;
   GtkWidget *raiz, *sw, *fila, *b;
   gchar     *t;

   if ( !a->hay ) { barra_pub( a, "Abre un proyecto antes." ); return; }

   /* OTRO PROYECTO, OTRO CUADERNO: la ventana abierta se cierra. */
   if ( abierto )
      {
      char md[PR_RUTA];

      if ( cu_ruta( a->p->path, md, sizeof md ) == 0 && !strcmp( md, abierto->md ) )
         { carga( abierto ); gtk_window_present( GTK_WINDOW(abierto->win) ); return; }
      gtk_widget_destroy( abierto->win );
      }

   c = g_new0( Cu, 1 );
   c->a = a;
   if ( cu_ruta( a->p->path, c->md, sizeof c->md ) != 0 )
      { g_free( c ); barra_pub( a, "Este proyecto no tiene manifiesto en disco." );
        return; }
   abierto = c;

   c->win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
   t = g_strdup_printf( "Cuaderno — %s", a->p->id[0] ? a->p->id : "proyecto" );
   gtk_window_set_title( GTK_WINDOW(c->win), t );
   g_free( t );
   gtk_window_set_transient_for( GTK_WINDOW(c->win), GTK_WINDOW(a->ventana) );
   gtk_window_set_default_size( GTK_WINDOW(c->win), 820, 600 );

   raiz = gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 );
   gtk_container_set_border_width( GTK_CONTAINER(raiz), 8 );
   gtk_container_add( GTK_CONTAINER(c->win), raiz );

   c->l_ruta = gtk_label_new( NULL );
   t = g_markup_printf_escaped(
      "<small>%s\nLo que se miró, en orden. Lo escriben los botones «Al "
      "cuaderno» de las ventanas de análisis; nunca se trunca.</small>", c->md );
   gtk_label_set_markup( GTK_LABEL(c->l_ruta), t );
   g_free( t );
   gtk_label_set_xalign( GTK_LABEL(c->l_ruta), 0.0 );
   gtk_label_set_selectable( GTK_LABEL(c->l_ruta), TRUE );
   gtk_widget_set_can_focus( c->l_ruta, FALSE );
   gtk_box_pack_start( GTK_BOX(raiz), c->l_ruta, FALSE, FALSE, 0 );

   c->vista = gtk_text_view_new();
   gtk_text_view_set_editable( GTK_TEXT_VIEW(c->vista), FALSE );
   gtk_text_view_set_monospace( GTK_TEXT_VIEW(c->vista), TRUE );
   gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW(c->vista), GTK_WRAP_WORD_CHAR );
   gtk_text_view_set_left_margin( GTK_TEXT_VIEW(c->vista), 6 );
   sw = gtk_scrolled_window_new( NULL, NULL );
   gtk_container_add( GTK_CONTAINER(sw), c->vista );
   gtk_box_pack_start( GTK_BOX(raiz), sw, TRUE, TRUE, 0 );

   /* LA NOTA: lo que el analista piensa al mirar, que ninguna ventana sabe. */
   c->nota = gtk_text_view_new();
   gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW(c->nota), GTK_WRAP_WORD_CHAR );
   gtk_widget_set_tooltip_text( c->nota,
      "Una nota para el cuaderno: lo que piensas al mirar. Va con su fecha, "
      "detrás de lo último. Se puede escribir Markdown." );
   sw = gtk_scrolled_window_new( NULL, NULL );
   gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC );
   gtk_widget_set_size_request( sw, -1, 70 );
   gtk_container_add( GTK_CONTAINER(sw), c->nota );
   gtk_box_pack_start( GTK_BOX(raiz), sw, FALSE, FALSE, 0 );

   fila = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
   gtk_box_pack_start( GTK_BOX(raiz), fila, FALSE, FALSE, 0 );
   b = gtk_button_new_with_label( "Añadir la nota" );
   gtk_widget_set_tooltip_text( b, "La nota de arriba, al final del cuaderno, "
                                   "con la fecha." );
   g_signal_connect( b, "clicked", G_CALLBACK(on_nota), c );
   gtk_box_pack_start( GTK_BOX(fila), b, FALSE, FALSE, 0 );

   b = gtk_button_new_with_label( "Abrir fuera" );
   gtk_widget_set_tooltip_text( b, "Con el programa que el sistema tenga para "
      "Markdown: ése sí pinta las tablas y los gráficos." );
   g_signal_connect( b, "clicked", G_CALLBACK(on_fuera), c );
   gtk_box_pack_end( GTK_BOX(fila), b, FALSE, FALSE, 0 );

   b = gtk_button_new_with_label( "Releer" );
   gtk_widget_set_tooltip_text( b, "Por si ha llegado algo mientras mirabas." );
   g_signal_connect( b, "clicked", G_CALLBACK(on_releer), c );
   gtk_box_pack_end( GTK_BOX(fila), b, FALSE, FALSE, 0 );

   g_signal_connect( c->win, "destroy", G_CALLBACK(on_cerrar), c );
   g_signal_connect( c->win, "focus-in-event", G_CALLBACK(on_foco), c );
   gtk_widget_show_all( c->win );
   carga( c );
}
