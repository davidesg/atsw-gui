/*
 * End-to-end test of the GUI gtk_fmg: its real main() (compiled with
 * -Dmain=gui_main) is driven from a timer, clicking its buttons as a user
 * would. Run it with tests/gui/run_gui_test.sh (make check-gui).
 *
 * Usage: driver <data folder with D.txt>; the environment variable
 * GUI_TEST_LOG is the file where the fake viewers write what they open.
 */

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdlib.h>
#include "fug_run.h"
#include "preview.h"

int gui_main(int argc, char *argv[]);

#define STEPS 24

static int step = 0, fails = 0, warnings = 0, dialogs_seen = 0;
static gchar *other_model = NULL;
static const char *dir;

#define CHECK(c, ...) do { if (!(c)) { fails++; g_print("FAIL step %d: ", step); \
                           g_print(__VA_ARGS__); g_print("\n"); } } while (0)

static GtkWidget *toplevel(const char *title)
{
    GList *l, *all = gtk_window_list_toplevels();
    GtkWidget *w = NULL;

    for (l = all; l; l = l->next) {
        const char *t = gtk_window_get_title(GTK_WINDOW(l->data));
        if (t && strcmp(t, title) == 0)
            w = l->data;
    }
    g_list_free(all);
    return w;
}

static int windows(const char *title)
{
    GList *l, *all = gtk_window_list_toplevels();
    int n = 0;

    for (l = all; l; l = l->next) {
        const char *t = gtk_window_get_title(GTK_WINDOW(l->data));
        if (t && strcmp(t, title) == 0)
            n++;
    }
    g_list_free(all);
    return n;
}

typedef struct { GType type; const char *label; int index; GtkWidget *found; int count; } Find;

static void finder(GtkWidget *w, gpointer d)
{
    Find *f = d;

    if (f->found)
        return;
    if (G_OBJECT_TYPE(w) == f->type
        || (f->type == GTK_TYPE_BUTTON && GTK_IS_BUTTON(w) && !GTK_IS_TOGGLE_BUTTON(w)
            && G_OBJECT_TYPE(w) != GTK_TYPE_FILE_CHOOSER_BUTTON)
        || (f->type == GTK_TYPE_CHECK_BUTTON && GTK_IS_CHECK_BUTTON(w))) {
        gboolean match = TRUE;
        if (f->label) {
            const char *lb = gtk_button_get_label(GTK_BUTTON(w));
            match = lb && strcmp(lb, f->label) == 0;
        }
        if (match && f->count++ == f->index) {
            f->found = w;
            return;
        }
    }
    if (GTK_IS_CONTAINER(w))
        gtk_container_forall(GTK_CONTAINER(w), finder, d);
}

static GtkWidget *find(GtkWidget *top, GType type, const char *label, int index)
{
    Find f = { type, label, index, NULL, 0 };

    if (top)
        finder(top, &f);
    return f.found;
}

static GtkWidget *chooser(GtkWidget *top, GtkFileChooserAction action)
{
    GtkWidget *w;
    int i;

    for (i = 0; (w = find(top, GTK_TYPE_FILE_CHOOSER_BUTTON, NULL, i)) != NULL; i++)
        if (gtk_file_chooser_get_action(GTK_FILE_CHOOSER(w)) == action)
            return w;
    return NULL;
}

static void click(const char *win, const char *label)
{
    GtkWidget *b = find(toplevel(win), GTK_TYPE_BUTTON, label, 0);

    CHECK(b != NULL, "button %s in %s", label, win);
    if (b)
        gtk_button_clicked(GTK_BUTTON(b));
}

static gchar *path_of(const char *file)
{
    return g_build_filename(dir, file, NULL);
}

static gboolean exists(const char *file)
{
    gchar *p = path_of(file);
    gboolean e = g_file_test(p, G_FILE_TEST_EXISTS);

    g_free(p);
    return e;
}

/* File of the data folder beginning with head (the first bytes) */
static gboolean starts_with(const char *file, const char *head, gsize n)
{
    gchar *p = path_of(file), *c = NULL;
    gsize len = 0;
    gboolean r;

    g_file_get_contents(p, &c, &len, NULL);
    r = c != NULL && len > n && memcmp(c, head, n) == 0;
    g_free(c);
    g_free(p);
    return r;
}

static gboolean contains(const char *file, const char *text)
{
    gchar *p = path_of(file), *c = NULL;
    gboolean r;

    g_file_get_contents(p, &c, NULL, NULL);
    r = c != NULL && strstr(c, text) != NULL;
    g_free(c);
    g_free(p);
    return r;
}

static void copy(const char *from, const char *to)
{
    gchar *p = path_of(from), *q = path_of(to), *c = NULL;
    gsize len;

    if (g_file_get_contents(p, &c, &len, NULL))
        g_file_set_contents(q, c, len, NULL);
    g_free(c);
    g_free(p);
    g_free(q);
}

static gboolean same_file(const char *a, const char *b)
{
    gchar *p = path_of(a), *q = path_of(b), *c = NULL, *d = NULL;
    gboolean r;

    g_file_get_contents(p, &c, NULL, NULL);
    g_file_get_contents(q, &d, NULL, NULL);
    r = c != NULL && d != NULL && strcmp(c, d) == 0;
    g_free(c);
    g_free(d);
    g_free(p);
    g_free(q);
    return r;
}

static gboolean viewer_opened(const char *what)
{
    gchar *c = NULL;
    gboolean r;

    g_file_get_contents(g_getenv("GUI_TEST_LOG"), &c, NULL, NULL);
    r = c && strstr(c, what) != NULL;
    g_free(c);
    return r;
}

/* The graph window of file is open, with pages pages (0: any number) */
static gboolean shown(const char *file, guint pages)
{
    gchar *p = path_of(file), *title = g_strdup_printf("FUG: %s", file);
    guint n = preview_n_pages(p);
    GtkWidget *w = toplevel(title);
    gboolean r = w != NULL && gtk_widget_get_visible(w) && n > 0 && (pages == 0 || n == pages);

    if (!r)
        g_print("  %s: window %s, %u pages\n", file, w ? "open" : "missing", n);
    g_free(title);
    g_free(p);
    return r;
}

static gboolean save(const char *file, const char *as)
{
    gchar *p = path_of(file), *q = path_of(as);
    GError *error = NULL;
    gboolean ok = preview_save_as(p, q, &error);

    if (!ok) {
        g_print("  save %s as %s: %s\n", file, as, error->message);
        g_error_free(error);
    }
    g_free(p);
    g_free(q);
    return ok;
}

/* Show a file that is not in the workspace of the GUI (NULL: no app) */
static guint reopen(const char *file)
{
    gchar *p = path_of(file);
    guint n = preview_show(NULL, p) ? preview_n_pages(p) : 0;

    g_free(p);
    return n;
}

static gboolean close_messages(void)
{
    GList *l, *all = gtk_window_list_toplevels();
    gboolean any = FALSE;

    for (l = all; l; l = l->next)
        if (GTK_IS_MESSAGE_DIALOG(l->data) && gtk_widget_get_visible(l->data)) {
            gchar *t = NULL;
            g_object_get(l->data, "text", &t, NULL);
            g_print("  [message at step %d]: %.60s\n", step, t ? t : "");
            g_free(t);
            gtk_dialog_response(GTK_DIALOG(l->data), GTK_RESPONSE_CLOSE);
            any = TRUE;
            dialogs_seen++;
        }
    g_list_free(all);
    return any;
}

/* Choose the file type of the Save dialog whose name contains what */
static void set_filter(GtkFileChooser *chooser, const char *what)
{
    GSList *l, *all = gtk_file_chooser_list_filters(chooser);

    for (l = all; l; l = l->next)
        if (strstr(gtk_file_filter_get_name(l->data), what) != NULL)
            gtk_file_chooser_set_filter(chooser, l->data);
    g_slist_free(all);
}

/* The Save As dialog of a graph window: the name follows the file type */
static gboolean save_dialog(void)
{
    static int phase = 0, waits = 0;
    GtkWidget *dialog = toplevel("Save Graph As");
    GtkFileChooser *chooser;
    gchar *name, *base;

    if (dialog == NULL || !gtk_widget_get_visible(dialog))
        return FALSE;
    chooser = GTK_FILE_CHOOSER(dialog);
    name = gtk_file_chooser_get_filename(chooser);
    if (name == NULL && waits++ < 10)       /* the folder is still being read */
        return TRUE;
    base = name ? g_path_get_basename(name) : g_strdup("");
    switch (phase++) {
    case 0:
        CHECK(strcmp(base, "acf_d0lnD.pdf") == 0, "default name %s", base);
        set_filter(chooser, "PNG");
        break;
    case 1:
        CHECK(strcmp(base, "acf_d0lnD.png") == 0, "name after choosing PNG: %s", base);
        gtk_file_chooser_set_current_name(chooser, "acf_dialog");
        break;
    case 2:
        set_filter(chooser, "SVG");
        break;
    case 3:
        CHECK(strcmp(base, "acf_dialog.svg") == 0, "name after choosing SVG: %s", base);
        gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
        break;
    }
    g_free(name);
    g_free(base);
    return TRUE;
}

static gboolean drive(gpointer d)
{
    const char *MAIN = "FUG: Load and Save Data";
    GtkWidget *m = toplevel(MAIN), *w;
    static gboolean acting = FALSE;
    gchar *f;

    if (close_messages() || save_dialog())
        return TRUE;
    if (acting)
        return TRUE;
    acting = TRUE;
    switch (step) {
    case 0:
        f = path_of("D.txt");
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser(m, GTK_FILE_CHOOSER_ACTION_OPEN)), f);
        g_free(f);
        /* the GUI runs the fug that is next to it */
#ifdef G_OS_WIN32
        f = g_path_get_basename(fug_program());
        CHECK(g_path_is_absolute(fug_program()) && strcmp(f, "fug.exe") == 0, "engine %s", fug_program());
#else
        f = g_build_filename(g_getenv("GUI_TEST_DIR"), "fug", NULL);
        CHECK(strcmp(fug_program(), f) == 0, "engine %s (expected %s)", fug_program(), f);
#endif
        g_free(f);
        break;
    case 2:
        g_signal_emit_by_name(chooser(m, GTK_FILE_CHOOSER_ACTION_OPEN), "file-set");
        break;
    case 4:
        CHECK(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(find(m, GTK_TYPE_SPIN_BUTTON, NULL, 0))) == 216, "nobs");
        gtk_combo_box_set_active(GTK_COMBO_BOX(find(m, GTK_TYPE_COMBO_BOX_TEXT, NULL, 0)), 2);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(find(m, GTK_TYPE_SPIN_BUTTON, NULL, 1)), 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(find(m, GTK_TYPE_SPIN_BUTTON, NULL, 2)), 2002);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(find(m, GTK_TYPE_SPIN_BUTTON, NULL, 3)), 0);
        click(MAIN, "gtk-save");
        break;
    case 5:   /* the GUI writes the .inp of fue (without model) */
        CHECK(contains("D.inp", "** Number of deterministic variables"), "D.inp in the format of fue");
        click(MAIN, "Histogram");
        break;
    case 6: CHECK(shown("hist_d0lnD.eps", 1), "histogram"); click(MAIN, "Ts Data Plot"); break;
    case 7: click("TS Data Plot Options", "gtk-ok"); break;
    case 8: CHECK(shown("d0lnD.eps", 1), "ts plot"); click(MAIN, "Acf/Pacf Plots"); break;
    case 9: click("Acf/Pacf Plots Options", "gtk-ok"); break;
    case 10: CHECK(shown("acf_d0lnD.eps", 1), "acf"); click(MAIN, "Mean_Std. Dev. Plot"); break;
    case 11: click("Mean-Std. Dev Plot Option", "gtk-ok"); break;
    case 12: CHECK(shown("m_dt_d0lnD.eps", 1), "mdt"); click(MAIN, "ID Options Set"); break;
    case 13: click("ID Options Set", "gtk-ok"); break;
    case 14:
        f = path_of("D_fug.pdf");
        CHECK(shown("D_fug.pdf", 0) && preview_n_pages(f) >= 3, "iden set pdf");
        g_free(f);
        /* graphs are shown by the GUI, not by gv */
        CHECK(!viewer_opened(".eps") && !viewer_opened(".pdf"), "external viewer used for a graph");
        break;
    case 15:   /* Save As: the identification set and one EPS graph, in the four formats */
        CHECK(save("D_fug.pdf", "set_copy.pdf") && starts_with("set_copy.pdf", "%PDF-1.4", 8), "set as pdf");
        CHECK(save("D_fug.pdf", "set_page.eps") && starts_with("set_page.eps", "%!PS-Adobe-3.0 EPSF", 19), "set as eps");
        CHECK(save("D_fug.pdf", "set_page.png") && starts_with("set_page.png", "\211PNG", 4), "set as png");
        CHECK(save("D_fug.pdf", "set_page.svg") && starts_with("set_page.svg", "<?xml", 5), "set as svg");
        CHECK(save("acf_d0lnD.eps", "acf.pdf") && starts_with("acf.pdf", "%PDF-1.4", 8), "eps as pdf");
        CHECK(save("acf_d0lnD.eps", "acf.eps") && starts_with("acf.eps", "%!PS-Adobe-3.0 EPSF", 19), "eps as eps");
        CHECK(save("acf_d0lnD.eps", "acf.png") && starts_with("acf.png", "\211PNG", 4), "eps as png");
        CHECK(save("acf_d0lnD.eps", "acf.svg") && starts_with("acf.svg", "<?xml", 5), "eps as svg");
        CHECK(!save("acf_d0lnD.eps", "acf.jpg"), "unknown format accepted");
        break;
    case 16:   /* the files written by the GUI are read again */
        CHECK(reopen("set_copy.pdf") == preview_n_pages(f = path_of("D_fug.pdf")), "set_copy.pdf pages");
        g_free(f);
        CHECK(reopen("set_page.eps") == 1, "set_page.eps");
        CHECK(reopen("acf.pdf") == 1, "acf.pdf");
        /* an EPS that fug did not draw goes to the external viewer */
        f = path_of("other.eps");
        g_file_set_contents(f, "%!PS-Adobe-3.0 EPSF-3.0\n%%BoundingBox: 0 0 10 10\nshowpage\n", -1, NULL);
        g_free(f);
        CHECK(reopen("other.eps") == 0, "other.eps shown");
        click("FUG: acf_d0lnD.eps", "_Save As...");
        break;
    case 17:
        CHECK(starts_with("acf_dialog.svg", "<?xml", 5), "saved with the dialog");
        click(MAIN, "Output");
        break;
    case 18:
        w = toplevel("FUG Output");
        gtk_combo_box_set_active(GTK_COMBO_BOX(find(w, GTK_TYPE_COMBO_BOX_TEXT, NULL, 0)), 2);
        click("FUG Output", "gtk-ok");
        break;
    case 19: CHECK(exists("D_fug.out") && viewer_opened("gedit") && viewer_opened("D_fug.out") &&
                   !exists("D.out") && !exists("D.pdf"), "output set");
        click(MAIN, "Input");
        break;
    case 20:
        CHECK(viewer_opened("D.inp"), "input opened");
        dialogs_seen = 0;
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(find(m, GTK_TYPE_SPIN_BUTTON, NULL, 0)), 5000);
        click(MAIN, "gtk-save");
        break;
    case 21:
        CHECK(dialogs_seen == 1, "error dialog (%d)", dialogs_seen);
        /* running fug again reloads the graph window that is open */
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(find(m, GTK_TYPE_SPIN_BUTTON, NULL, 0)), 216);
        f = path_of("hist_d0lnD.eps");
        g_unlink(f);
        g_free(f);
        /* D.inp is now a fue model of the same series: it is kept as it is */
        copy("FULL.inp", "D.inp");
        click(MAIN, "Histogram");
        break;
    case 22:
        CHECK(dialogs_seen == 1 && exists("hist_d0lnD.eps") && shown("hist_d0lnD.eps", 1) &&
              windows("FUG: hist_d0lnD.eps") == 1, "histogram again");
        CHECK(same_file("D.inp", "FULL.inp"), "the fue model of the same series was changed");
        /* a fue model of another series: the GUI asks, and Cancel keeps it */
        f = path_of("D.inp");
        {
            gchar *c = NULL;
            g_file_get_contents(f, &c, NULL, NULL);
            *strstr(c, "\n77.7 0\n") = '\0';
            other_model = g_strconcat(c, "\n99.9 0\n", c + strlen(c) + strlen("\n77.7 0\n"), NULL);
            g_file_set_contents(f, other_model, -1, NULL);
            g_free(c);
        }
        g_free(f);
        f = path_of("hist_d0lnD.eps");
        g_unlink(f);
        g_free(f);
        dialogs_seen = 0;
        click(MAIN, "Histogram");
        break;
    case 23:
        f = path_of("D.inp");
        {
            gchar *c = NULL;
            g_file_get_contents(f, &c, NULL, NULL);
            CHECK(dialogs_seen == 1 && !exists("hist_d0lnD.eps") && c && strcmp(c, other_model) == 0,
                  "fue model of another series: dialogs %d", dialogs_seen);
            g_free(c);
        }
        g_free(f);
        gtk_widget_destroy(m);
        break;
    }
    step++;
    acting = FALSE;
    return TRUE;
}

static void count(const gchar *domain, GLogLevelFlags level, const gchar *msg, gpointer d)
{
    if (level & (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING)) {
        warnings++;
        g_print("%s warning: %s\n", domain ? domain : "", msg);
    }
}

int main(int argc, char *argv[])
{
    const char *domains[] = { "Gtk", "Gdk", "GLib-GObject", "GLib", "GLib-GIO", "Pango" };
    GSource *src;
    guint i;

    if (argc < 2) {
        g_printerr("usage: driver <data folder>\n");
        return 2;
    }
    dir = argv[1];
    for (i = 0; i < G_N_ELEMENTS(domains); i++)
        g_log_set_handler(domains[i], G_LOG_LEVEL_MASK, count, NULL);
    src = g_timeout_source_new(400);
    g_source_set_callback(src, drive, NULL, NULL);
    g_source_set_can_recurse(src, TRUE);
    g_source_attach(src, NULL);
    gui_main(1, argv);
    g_print("GUI test: reached step %d of %d, %d failures, %d warnings -> %s\n", step, STEPS,
            fails, warnings, (fails || warnings || step < STEPS) ? "FAILED" : "ALL OK");
    return fails || warnings || step < STEPS;
}
