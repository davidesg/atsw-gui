/* ganchos.h -- las funciones de GTK y GLib que la prueba sustituye.
 *
 * Se fuerza con -include al compilar el codigo del GUI. PRIMERO van las
 * cabeceras de GTK y DESPUES las macros: si las macros fueran con -D en la
 * linea de ordenes, renombrarian tambien las DECLARACIONES de GTK, y en
 * Windows esas llevan __declspec(dllimport): el enlazador buscaba
 * __imp_test_spawn_async en las DLL de GLib y no enlazaba. Lo vio la CI de
 * Windows. Las de verdad estan en test_gui.c.                           */
#ifndef FMG_GANCHOS_H
#define FMG_GANCHOS_H

#include <gtk/gtk.h>

gint     test_dialog_run(GtkDialog *dialog);
void     test_show_all(GtkWidget *widget);
void     test_window_present(GtkWindow *window);
gboolean test_show_uri(GdkScreen *screen, const gchar *uri, guint32 when,
                       GError **error);
gboolean test_spawn_async(const gchar *dir, gchar **argv, gchar **envp,
                          GSpawnFlags flags, GSpawnChildSetupFunc setup,
                          gpointer data, GPid *pid, GError **error);

#define gtk_dialog_run       test_dialog_run
#define gtk_widget_show_all  test_show_all
#define gtk_window_present   test_window_present
#define gtk_show_uri         test_show_uri
#define g_spawn_async        test_spawn_async

#endif
