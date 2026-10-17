/*
 * vuelta_gui.c -- «Devolver los univariantes…»: los bloques univariantes que
 * drtran reestimo junto a la transferencia, de vuelta a fue (DISENO-casos §3,
 * lib/vuelta).
 *
 * CON UN CASO, cada serie gana un MODELO DERIVADO, hijo del que entro, con su
 * .inp y la razon de donde viene: el arbol de la serie dice que ese modelo
 * salio de drtran, y la madre lo enseña como cualquier otro sin estimar. El
 * trabajo es de lib/vuelta (vu_caso), el mismo que hace la madre. SIN CASO
 * --sin proyecto, o con series de fuera-- los .inp van a la carpeta que se
 * elija.
 *
 * SON .inp, NO .pre: optimos CON la transferencia, no solos. El dialogo lo
 * dice antes de escribir nada, y la razon lo deja escrito.
 */

#include <string.h>

#include "gui.h"
#include "previewhost.h"
#include "vuelta.h"

/* Lo que falta para poder devolver, o NULL si nada. */
const char *vuelta_falta( Mtram *m )
{
    if (m->est.corriendo)               return "El motor está corriendo.";
    if (!m->dia.vale || !m->dia.hay_par) return "Todavía no hay modelo estimado.";
    /* EL MODELO QUE HAY DELANTE, no el modo de la casilla: el baseline deja
     * el modo como estaba y el .out que se ve puede ser el diagonal.     */
    if (!vu_con_transferencia( &m->dia.par ))
        return "Este modelo es el diagonal: sin transferencia, los bloques son "
               "los de fue y no hay nada que devolver.";
    return NULL;
}

/* El siguiente nombre libre junto a la carpeta: base.1.inp, base.2.inp... */
static gchar *libre_en( const char *dir, const char *pre )
{
    gchar *b = g_path_get_basename( pre ), *p = strrchr( b, '.' ), *r = NULL;
    int    i;

    if (p) *p = '\0';
    for (i = 1; i < 1000; i++) {
        gchar *nom = g_strdup_printf( "%s.%d.inp", b, i );

        r = g_build_filename( dir, nom, NULL );
        g_free( nom );
        if (!g_file_test( r, G_FILE_TEST_EXISTS )) break;
        g_free( r ); r = NULL;
    }
    g_free( b );
    return r;
}

static gboolean confirma( Mtram *m, const char *texto )
{
    GtkWidget *d = gtk_message_dialog_new( GTK_WINDOW(m->ventana_p),
                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                       GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, "%s",
                       "Devolver los univariantes" );
    gint r;

    gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG(d), "%s", texto );
    r = gtk_dialog_run( GTK_DIALOG(d) );
    gtk_widget_destroy( d );
    return r == GTK_RESPONSE_OK;
}

static const char *QUE_SON =
    "Son .inp, NO .pre: estos bloques son óptimos CON la transferencia, no "
    "solos, y en fue los números se moverán. Son un punto de partida: "
    "estímalos con fue, que devolverá .pre de verdad.\n\n"
    "La transferencia no va en ellos: es un fichero univariante.";

/* SIN PROYECTO: a una carpeta. */
static void a_carpeta( Mtram *m )
{
    GtkWidget *d;
    gchar     *dir = NULL;
    GString   *hecho = g_string_new( NULL );
    int        i, malos = 0;
    char       why[512];

    d = gtk_file_chooser_dialog_new( "Devolver los univariantes: ¿a qué carpeta?",
            GTK_WINDOW(m->ventana_p), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
            "_Cancelar", GTK_RESPONSE_CANCEL, "_Escribir", GTK_RESPONSE_ACCEPT, NULL );
    if (gtk_dialog_run( GTK_DIALOG(d) ) == GTK_RESPONSE_ACCEPT)
        dir = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(d) );
    gtk_widget_destroy( d );
    if (!dir) return;

    for (i = 0; i < m->c.n; i++) {
        gchar *dst = libre_en( dir, m->c.s[i]->path );
        int    mov = 0;

        if (!dst || vu_escribe( m->c.s[i]->path, &m->dia.par, i + 1, dst, &mov,
                                why, sizeof why ) != 0) {
            g_string_append_printf( hecho, "%s%s: %s", hecho->len ? " · " : "",
                                    m->c.s[i]->ts.name, dst ? why : "sin nombre libre" );
            malos++;
        } else {
            gchar *b = g_path_get_basename( dst );

            g_string_append_printf( hecho, "%s%s", hecho->len ? " · " : "", b );
            g_free( b );
        }
        g_free( dst );
    }
    preview_show_status( m, "%s %s — en %s. Son .inp: estímalos con fue.",
                         malos ? "Devueltos, con problemas:" : "Devueltos:",
                         hecho->str, dir );
    g_string_free( hecho, TRUE );
    g_free( dir );
}

/* CON PROYECTO Y CASO: un modelo derivado por entrada, hijo del que entro.
 * El trabajo es de lib/vuelta (vu_caso), el mismo que hace la madre.    */
static void al_proyecto( Mtram *m )
{
    const PrCaso *c = pr_caso_ver( m->proy, m->caso );
    Proyecto     *nuevo;
    PrError       e;
    GString      *lista = g_string_new( NULL );
    char          hechos[1024], why[512];
    gchar        *txt;
    int           k, rc;

    for (k = 0; c && k < c->nen; k++)
        g_string_append_printf( lista, "\n   %s: un modelo nuevo, hijo de %s",
                                c->en[k].serie, c->en[k].modelo );
    txt = g_strdup_printf( "Cada serie del caso %s gana un modelo derivado con su "
                           "bloque univariante reestimado junto a la transferencia "
                           "(modelo %s):\n%s\n\n%s", m->caso, m->corrida,
                           lista->str, QUE_SON );
    g_string_free( lista, TRUE );
    if (!confirma( m, txt )) { g_free( txt ); return; }
    g_free( txt );

    /* EL MANIFIESTO DE AHORA, no el de cuando se abrio: la madre puede
     * haberlo cambiado, y guardar el nuestro encima pisaria lo suyo.    */
    nuevo = g_new0( Proyecto, 1 );
    if (pr_leer( m->proy->path, nuevo, &e ) == 0) {
        char path[PR_RUTA];

        snprintf( path, sizeof path, "%s", m->proy->path );
        *m->proy = *nuevo;
        snprintf( m->proy->path, sizeof m->proy->path, "%s", path );
    }
    g_free( nuevo );

    rc = vu_caso( m->proy, m->caso, m->corrida, hechos, sizeof hechos, why, sizeof why );
    if (!mtram_proyecto_guarda( m, why, sizeof why ))
        preview_show_status( m, "Los .inp están, pero no pude guardar el proyecto: %s", why );
    else if (rc)
        preview_show_status( m, "Devueltos: %s. Con problemas: %s", hechos[0] ? hechos : "ninguno", why );
    else
        preview_show_status( m, "Devueltos: %s. Son .inp: estímalos con fue desde la madre.",
                             hechos );
}

void vuelta_devuelve( Mtram *m )
{
    const char *falta = vuelta_falta( m );

    if (falta) { preview_show_status( m, "%s", falta ); return; }
    if (m->hay_proy && m->proy && m->caso[0] && m->corrida[0]) al_proyecto( m );
    else {
        gchar *t = g_strdup_printf( "Se escribe un .inp por serie, en la carpeta que "
                                    "elijas.\n\n%s", QUE_SON );
        gboolean si = confirma( m, t );

        g_free( t );
        if (si) a_carpeta( m );
    }
}
