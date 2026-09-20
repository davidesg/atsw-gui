/*
 * preview.c
 *
 * Graph window: shows the EPS and PDF files made by fue and fuf, saves them as PDF,
 * EPS, PNG or SVG and prints them.
 *
 * fue and fuf draw every page as a PDF content stream (src/fugdraw.c) and writes
 * that stream in an EPS file or in the pages of a PDF file. This window
 * reads the stream back and draws it with Cairo, so no external viewer is
 * needed. PDF and EPS copies are written by fugdraw itself, exactly as fug
 * writes them; PNG and SVG are drawn by Cairo.
 *
 * This is the file of fug (gui/src/preview.c), ported to GTK+3 and with
 * what the host supplies moved to previewhost.h.
 */

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gio/gio.h>
#include <cairo.h>
#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
#endif
#include "preview.h"
#include "previewhost.h"
#include "fugdraw.h"

#define PNG_DPI 300.0          /* resolution of the PNG files            */
#define MARGIN  16             /* around the page in the window (pixels) */
#define ZOOM_MIN   0.10
#define ZOOM_MAX  16.00
#define ZOOM_STEP  1.25        /* one notch of the zoom                   */
#define GLASS     640          /* the magnifier, in pixels (a square): at  */
#define GLASS_X     4.0        /* GLASS_X it shows GLASS/GLASS_X points of */
                               /* the page, so it takes in the whole of an */
                               /* incident and not only the point of it    */

typedef struct {
    double w, h;               /* page size (points)                      */
    gchar *content;            /* content stream (NUL terminated)         */
    gsize  len;
} Page;

typedef struct {
    PreviewApp *app;
    GtkWidget  *window, *area, *scroller, *prev, *next, *page_label, *page_item;
    GtkWidget  *zoom_label;
    GtkWidget  *vbox;          /* para colgarle un pie                    */
    GtkWidget  *footer;        /* los controles del que la abrio, o NULL  */
    gchar      *path;          /* file shown                              */
    gboolean    is_pdf;
    GArray     *pages;         /* of Page                                 */
    guint       current;
    double      zoom;          /* 0: the page fits the window; else the    */
                               /* scale, 1.0 being one point one pixel     */
    cairo_surface_t *cache;    /* the page drawn at the window size       */
    int         cache_w, cache_h;
    guint       cache_page;
    double      cache_scale;
    /* Panning with button 1 when the page is larger than the window */
    gboolean    panning;
    double      pan_x, pan_y;
    /* The magnifier, as gv has it: button 3 over the page                 */
    GtkWidget  *glass;         /* a window without decoration             */
    GtkWidget  *glass_area;
    double      glass_px, glass_py;   /* the point of the page it is on    */
    double      glass_zoom;
    int         glass_size;    /* GLASS, or less on a small screen         */
} Preview;

static GHashTable *previews = NULL;          /* path -> Preview           */
static GtkPrintSettings *print_settings = NULL;

enum { FMT_PDF, FMT_EPS, FMT_PNG, FMT_SVG, N_FORMATS };
static const struct { const gchar *name, *ext; } formats[N_FORMATS] = {
    { "PDF document (vector)",            ".pdf" },
    { "EPS - Encapsulated PostScript",    ".eps" },
    { "PNG image (300 dpi)",              ".png" },
    { "SVG image (vector)",               ".svg" },
};

/* ---------------------------------------------------------------------- */
/* Reading the files of fugdraw                                           */
/* ---------------------------------------------------------------------- */

static void pages_free(GArray *pages)
{
    guint i;

    if (pages == NULL)
        return;
    for (i = 0; i < pages->len; i++)
        g_free(g_array_index(pages, Page, i).content);
    g_array_free(pages, TRUE);
}

static const gchar *find(const gchar *from, const gchar *end, const gchar *what)
{
    gsize n = strlen(what);

    for (; from + n <= end; from++)
        if (*from == *what && memcmp(from, what, n) == 0)
            return from;
    return NULL;
}

/* EPS: the stream is between %%EndSetup and the final "end showpage" */
static gboolean load_eps(const gchar *data, gsize len, GArray *pages)
{
    const gchar *end = data + len, *bbox, *begin, *stop, *p;
    gchar *after;
    Page page;

    if (find(data, MIN(end, data + 400), "%%Creator: FUG (fugdraw)") == NULL)
        return FALSE;
    bbox = find(data, end, "%%HiResBoundingBox:");
    begin = find(data, end, "%%EndSetup\n");
    stop = NULL;
    for (p = begin; p != NULL; p = find(p + 1, end, "end\nshowpage"))
        stop = p;
    if (bbox == NULL || begin == NULL || stop == NULL || stop == begin)
        return FALSE;

    /* %%HiResBoundingBox: 0 0 w h */
    p = bbox + strlen("%%HiResBoundingBox:");
    g_ascii_strtod(p, &after);
    g_ascii_strtod(after, &after);
    page.w = g_ascii_strtod(after, &after);
    page.h = g_ascii_strtod(after, &after);
    if (page.w <= 0.0 || page.h <= 0.0)
        return FALSE;

    begin += strlen("%%EndSetup\n");
    page.len = stop - begin;
    page.content = g_strndup(begin, page.len);
    g_array_append_val(pages, page);
    return TRUE;
}

/* Inflate a zlib stream; NULL if it is not valid */
static gchar *inflate(const guchar *in, gsize inlen, gsize *outlen)
{
    GConverter *z = G_CONVERTER(g_zlib_decompressor_new(G_ZLIB_COMPRESSOR_FORMAT_ZLIB));
    GByteArray *out = g_byte_array_new();
    GConverterResult r;
    GError *error = NULL;
    guchar buf[16384];
    gsize nread, nwritten;

    do {
        r = g_converter_convert(z, in, inlen, buf, sizeof buf, G_CONVERTER_INPUT_AT_END,
                                &nread, &nwritten, &error);
        if (r == G_CONVERTER_ERROR || (r != G_CONVERTER_FINISHED && nread == 0 && nwritten == 0))
            break;
        g_byte_array_append(out, buf, nwritten);
        in += nread;
        inlen -= nread;
    } while (r != G_CONVERTER_FINISHED);
    g_object_unref(z);
    if (error != NULL)
        g_error_free(error);
    if (r != G_CONVERTER_FINISHED) {
        g_byte_array_free(out, TRUE);
        return NULL;
    }
    *outlen = out->len;
    g_byte_array_append(out, (const guchar *) "", 1);
    return (gchar *) g_byte_array_free(out, FALSE);
}

/* PDF written by fugdraw: each page is its content stream followed by the
 * page object with its /MediaBox */
static gboolean load_pdf(const gchar *data, gsize len, GArray *pages)
{
    static const gchar *head = "%PDF-1.4\n%\342\343\317\323\n3 0 obj\n"
                               "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica ";
    const gchar *end = data + len, *p = data, *s, *mb;
    gchar *after;
    Page page;
    long length;
    gboolean flate;

    if (len < strlen(head) || memcmp(data, head, strlen(head)) != 0)
        return FALSE;
    while ((p = find(p, end, "/Length ")) != NULL) {
        length = strtol(p + strlen("/Length "), &after, 10);
        s = find(p, end, "stream");
        if (s == NULL || length <= 0)
            return FALSE;
        flate = find(p, s, "/FlateDecode") != NULL;
        s += strlen("stream");
        if (s < end && *s == '\r')
            s++;
        if (s < end && *s == '\n')
            s++;
        if (length > end - s)
            return FALSE;
        if (flate)
            page.content = inflate((const guchar *) s, length, &page.len);
        else {
            page.content = g_strndup(s, length);
            page.len = length;
        }
        if (page.content == NULL)
            return FALSE;

        /* /MediaBox [0 0 w h] */
        mb = find(s + length, end, "/MediaBox");
        if (mb == NULL || (mb = find(mb, end, "[")) == NULL) {
            g_free(page.content);
            return FALSE;
        }
        g_ascii_strtod(mb + 1, &after);
        g_ascii_strtod(after, &after);
        page.w = g_ascii_strtod(after, &after);
        page.h = g_ascii_strtod(after, &after);
        if (page.w <= 0.0 || page.h <= 0.0) {
            g_free(page.content);
            return FALSE;
        }
        g_array_append_val(pages, page);
        p = s + length;
    }
    return pages->len > 0;
}

/* The pages of an EPS or PDF file of fug, or NULL */
static GArray *load_file(const gchar *path, gboolean *is_pdf)
{
    GArray *pages = g_array_new(FALSE, FALSE, sizeof(Page));
    gchar *data = NULL;
    gsize len;
    gboolean ok = FALSE;

    if (g_file_get_contents(path, &data, &len, NULL)) {
        *is_pdf = len > 5 && memcmp(data, "%PDF-", 5) == 0;
        ok = *is_pdf ? load_pdf(data, len, pages) : load_eps(data, len, pages);
    }
    g_free(data);
    if (!ok) {
        pages_free(pages);
        return NULL;
    }
    return pages;
}

/* ---------------------------------------------------------------------- */
/* Drawing a content stream with Cairo                                    */
/* ---------------------------------------------------------------------- */

/* Families for the fonts of fugdraw (the first one found is used). They
 * have the widths of Helvetica and Times: on Linux Nimbus Sans and Nimbus
 * Roman (the fonts of Ghostscript), on Windows Arial and Times New Roman. */
static const gchar *font_family[] = {
#ifdef G_OS_WIN32
    "Arial,Liberation Sans,Helvetica,Nimbus Sans,Sans",
    "Arial,Liberation Sans,Helvetica,Nimbus Sans,Sans",
    "Times New Roman,Liberation Serif,Times,Nimbus Roman,Serif",
    "Times New Roman,Liberation Serif,Times,Nimbus Roman,Serif",
    "Times New Roman,Liberation Serif,Serif",
#else
    "Helvetica,Nimbus Sans,Arial,Liberation Sans,Sans",
    "Helvetica,Nimbus Sans,Arial,Liberation Sans,Sans",
    "Times,Nimbus Roman,Times New Roman,Liberation Serif,Serif",
    "Times,Nimbus Roman,Times New Roman,Liberation Serif,Serif",
    "Standard Symbols PS,Times New Roman,DejaVu Serif,Serif",
#endif
};

/* Symbol font: letters are Greek */
static const gunichar greek_upper[26] = {
    0x391, 0x392, 0x3A7, 0x394, 0x395, 0x3A6, 0x393, 0x397, 0x399, 0x3D1, 0x39A, 0x39B, 0x39C,
    0x39D, 0x39F, 0x3A0, 0x398, 0x3A1, 0x3A3, 0x3A4, 0x3A5, 0x3C2, 0x3A9, 0x39E, 0x3A8, 0x396 };
static const gunichar greek_lower[26] = {
    0x3B1, 0x3B2, 0x3C7, 0x3B4, 0x3B5, 0x3C6, 0x3B3, 0x3B7, 0x3B9, 0x3D5, 0x3BA, 0x3BB, 0x3BC,
    0x3BD, 0x3BF, 0x3C0, 0x3B8, 0x3C1, 0x3C3, 0x3C4, 0x3C5, 0x3D6, 0x3C9, 0x3BE, 0x3C8, 0x3B6 };

static gunichar symbol_char(guchar c)
{
    switch (c) {
    case 0x2D: return 0x2212;           /* minus         */
    case 0xA3: return 0x2264;           /* lessequal     */
    case 0xA5: return 0x221E;           /* infinity      */
    case 0xB1: return 0x00B1;           /* plusminus     */
    case 0xB3: return 0x2265;           /* greaterequal  */
    case 0xB4: return 0x00D7;           /* multiply      */
    case 0xB6: return 0x2202;           /* partialdiff   */
    case 0xB9: return 0x2260;           /* notequal      */
    case 0xBB: return 0x2248;           /* approxequal   */
    case 0xD6: return 0x221A;           /* radical       */
    case 0xE5: return 0x2211;           /* summation     */
    }
    if (c >= 'A' && c <= 'Z')
        return greek_upper[c - 'A'];
    if (c >= 'a' && c <= 'z')
        return greek_lower[c - 'a'];
    return c;
}

/* Text of fugdraw (Latin-1, or the Symbol font) in UTF-8 */
static gchar *text_utf8(int font, const gchar *s)
{
    GString *u = g_string_new(NULL);

    for (; *s != '\0'; s++)
        g_string_append_unichar(u, font == FD_SYMBOL ? symbol_char((guchar) *s) : (guchar) *s);
    return g_string_free(u, FALSE);
}

typedef struct {
    double fill, stroke;       /* gray levels */
    int    font;
    double size;
} GState;

/* Draw text at (x, y) of the current user space; it is stretched to the
 * width of the Adobe metrics, so it is placed exactly as in the EPS/PDF. */
static void draw_text(cairo_t *cr, PangoContext *pango, const GState *gs,
                      double x, double y, const gchar *s);

#define SYMBOL_NABLA '\321'

/* The nabla of the Symbol font, drawn with its outline (Standard Symbols PS;
 * Arial and Times New Roman have no nabla) */
static void draw_nabla(cairo_t *cr, const GState *gs, double x, double y)
{
    static const double outline[2][3][2] = {
        { { 681, 688 }, { 339, 0 }, { 36, 688 } },
        { { 610, 636 }, { 165, 636 }, { 376, 178 } } };
    double k = gs->size / 1000.0;
    int i, j;

    cairo_save(cr);
    cairo_new_path(cr);
    for (i = 0; i < 2; i++) {
        cairo_move_to(cr, x + k * outline[i][0][0], y + k * outline[i][0][1]);
        for (j = 1; j < 3; j++)
            cairo_line_to(cr, x + k * outline[i][j][0], y + k * outline[i][j][1]);
        cairo_close_path(cr);
    }
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_set_source_rgb(cr, gs->fill, gs->fill, gs->fill);
    cairo_fill(cr);
    cairo_restore(cr);
}

/* Symbol font text with nablas: the nablas are drawn apart */
static void draw_symbol_text(cairo_t *cr, PangoContext *pango, const GState *gs,
                             double x, double y, const gchar *s)
{
    const gchar *nabla;
    gchar *part;

    while ((nabla = strchr(s, SYMBOL_NABLA)) != NULL) {
        part = g_strndup(s, nabla - s);
        draw_text(cr, pango, gs, x, y, part);
        x += fd_text_width(FD_SYMBOL, gs->size, part);
        g_free(part);
        draw_nabla(cr, gs, x, y);
        x += fd_text_width(FD_SYMBOL, gs->size, FD_SYM_NABLA);
        s = nabla + 1;
    }
    draw_text(cr, pango, gs, x, y, s);
}

static void draw_text(cairo_t *cr, PangoContext *pango, const GState *gs,
                      double x, double y, const gchar *s)
{
    PangoFontDescription *desc;
    PangoLayout *layout;
    PangoRectangle ink, logical;
    gchar *utf8;
    double afm, width, baseline;

    if (*s == '\0' || gs->size <= 0.0)
        return;
    if (gs->font == FD_SYMBOL && strchr(s, SYMBOL_NABLA) != NULL) {
        draw_symbol_text(cr, pango, gs, x, y, s);
        return;
    }
    utf8 = text_utf8(gs->font, s);
    layout = pango_layout_new(pango);
    desc = pango_font_description_new();
    pango_font_description_set_family(desc, font_family[gs->font]);
    pango_font_description_set_weight(desc, gs->font == FD_HELV_BOLD ? PANGO_WEIGHT_BOLD
                                                                     : PANGO_WEIGHT_NORMAL);
    pango_font_description_set_style(desc, gs->font == FD_TIMES_ITALIC ? PANGO_STYLE_ITALIC
                                                                       : PANGO_STYLE_NORMAL);
    pango_font_description_set_absolute_size(desc, gs->size * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, utf8, -1);
    pango_layout_get_extents(layout, &ink, &logical);
    width = logical.width / (double) PANGO_SCALE;
    baseline = pango_layout_get_baseline(layout) / (double) PANGO_SCALE;
    afm = fd_text_width(gs->font, gs->size, s);

    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, (width > 0.0 && afm > 0.0) ? afm / width : 1.0, -1.0);
    cairo_move_to(cr, 0.0, -baseline);
    cairo_set_source_rgb(cr, gs->fill, gs->fill, gs->fill);
    pango_cairo_show_layout(cr, layout);
    cairo_restore(cr);

    pango_font_description_free(desc);
    g_object_unref(layout);
    g_free(utf8);
}

/* PostScript string (...) at p; returns the position after it */
static const gchar *read_string(const gchar *p, const gchar *end, GString *out)
{
    int depth = 1, v, k;
    gchar c;

    g_string_truncate(out, 0);
    for (p++; p < end; ) {
        c = *p++;
        if (c == '\\' && p < end) {
            c = *p++;
            if (c >= '0' && c <= '7') {
                for (v = c - '0', k = 0; k < 2 && p < end && *p >= '0' && *p <= '7'; k++)
                    v = 8 * v + (*p++ - '0');
                g_string_append_c(out, (gchar) v);
            } else if (c == 'n') g_string_append_c(out, '\n');
            else if (c == 'r') g_string_append_c(out, '\r');
            else if (c == 't') g_string_append_c(out, '\t');
            else if (c != '\n') g_string_append_c(out, c);
        } else if (c == '(') {
            depth++;
            g_string_append_c(out, c);
        } else if (c == ')') {
            if (--depth == 0)
                break;
            g_string_append_c(out, c);
        } else {
            g_string_append_c(out, c);
        }
    }
    return p;
}

#define MAX_OPERANDS 8
#define MAX_DEPTH    32

/* Draw a content stream of fugdraw: the current user space must be the
 * PDF one (points, y upwards). Only the operators of fugdraw are used. */
static void draw_content(cairo_t *cr, const gchar *s, gsize len)
{
    const gchar *p = s, *end = s + len, *w;
    double num[MAX_OPERANDS], dash[8], line_x = 0.0, line_y = 0.0, tx = 0.0, ty = 0.0;
    int nnum = 0, ndash = 0, depth = 0, name_font = 0;
    gboolean in_array = FALSE, clip = FALSE;
    GState gs = { 0.0, 0.0, 0, 10.0 }, stack[MAX_DEPTH];
    GString *str = g_string_new(NULL);
    PangoContext *pango = pango_font_map_create_context(pango_cairo_font_map_get_default());
    cairo_font_options_t *options = cairo_font_options_create();
    cairo_matrix_t m;
    gchar op[8];

    /* The text is laid out in the user space of the figure, with no
     * hinting and no rounding, so it is the same at every scale */
    cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_OFF);
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
    pango_cairo_context_set_font_options(pango, options);
    cairo_font_options_destroy(options);
#if PANGO_VERSION_CHECK(1, 44, 0)
    pango_context_set_round_glyph_positions(pango, FALSE);
#endif

    cairo_save(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
    cairo_set_miter_limit(cr, 10.0);
    cairo_set_dash(cr, NULL, 0, 0.0);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

#define NEED(k) if (nnum < (k)) goto next
#define A(i, k) num[nnum - (k) + (i)]
    while (p < end) {
        if (g_ascii_isspace(*p)) {
            p++;
        } else if (*p == '%') {
            while (p < end && *p != '\n')
                p++;
        } else if (g_ascii_isdigit(*p) || *p == '-' || *p == '+' || *p == '.') {
            gchar *after;
            double v = g_ascii_strtod(p, &after);
            if (after == p) {
                p++;
                continue;
            }
            p = after;
            if (in_array) {
                if (ndash < 8)
                    dash[ndash++] = v;
            } else {
                if (nnum == MAX_OPERANDS) {
                    memmove(num, num + 1, (MAX_OPERANDS - 1) * sizeof(double));
                    nnum--;
                }
                num[nnum++] = v;
            }
        } else if (*p == '[') {
            in_array = TRUE;
            ndash = 0;
            p++;
        } else if (*p == ']') {
            in_array = FALSE;
            p++;
        } else if (*p == '/') {
            for (w = ++p; p < end && g_ascii_isalnum(*p); p++)
                ;
            if (p - w >= 2 && *w == 'F')
                name_font = CLAMP(atoi(w + 1) - 1, 0, FD_SYMBOL);
        } else if (*p == '(') {
            p = read_string(p, end, str);
        } else if (g_ascii_isalpha(*p) || *p == '*') {
            for (w = p; p < end && (g_ascii_isalpha(*p) || *p == '*'); p++)
                ;
            g_strlcpy(op, w, MIN((gsize) (p - w) + 1, sizeof op));

            if (!strcmp(op, "m")) { NEED(2); cairo_move_to(cr, A(0, 2), A(1, 2)); }
            else if (!strcmp(op, "l")) { NEED(2); cairo_line_to(cr, A(0, 2), A(1, 2)); }
            else if (!strcmp(op, "c")) {
                NEED(6);
                cairo_curve_to(cr, A(0, 6), A(1, 6), A(2, 6), A(3, 6), A(4, 6), A(5, 6));
            }
            else if (!strcmp(op, "h")) cairo_close_path(cr);
            else if (!strcmp(op, "re")) { NEED(4); cairo_rectangle(cr, A(0, 4), A(1, 4), A(2, 4), A(3, 4)); }
            else if (!strcmp(op, "W") || !strcmp(op, "W*")) {
                clip = TRUE;
                cairo_set_fill_rule(cr, op[1] ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
            }
            else if (!strcmp(op, "S") || !strcmp(op, "f") || !strcmp(op, "f*") || !strcmp(op, "n")) {
                if (clip)
                    cairo_clip_preserve(cr);
                clip = FALSE;
                if (op[0] == 'S') {
                    cairo_set_source_rgb(cr, gs.stroke, gs.stroke, gs.stroke);
                    cairo_stroke(cr);
                } else if (op[0] == 'f') {
                    cairo_set_fill_rule(cr, op[1] ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
                    cairo_set_source_rgb(cr, gs.fill, gs.fill, gs.fill);
                    cairo_fill(cr);
                } else {
                    cairo_new_path(cr);
                }
            }
            else if (!strcmp(op, "q")) {
                if (depth < MAX_DEPTH) {
                    stack[depth++] = gs;
                    cairo_save(cr);
                }
            }
            else if (!strcmp(op, "Q")) {
                if (depth > 0) {
                    gs = stack[--depth];
                    cairo_restore(cr);
                }
            }
            else if (!strcmp(op, "w")) { NEED(1); cairo_set_line_width(cr, A(0, 1)); }
            else if (!strcmp(op, "d")) { NEED(1); cairo_set_dash(cr, dash, ndash, A(0, 1)); }
            else if (!strcmp(op, "j")) {
                NEED(1);
                cairo_set_line_join(cr, A(0, 1) == 1 ? CAIRO_LINE_JOIN_ROUND :
                                        A(0, 1) == 2 ? CAIRO_LINE_JOIN_BEVEL : CAIRO_LINE_JOIN_MITER);
            }
            else if (!strcmp(op, "J")) {
                NEED(1);
                cairo_set_line_cap(cr, A(0, 1) == 1 ? CAIRO_LINE_CAP_ROUND :
                                       A(0, 1) == 2 ? CAIRO_LINE_CAP_SQUARE : CAIRO_LINE_CAP_BUTT);
            }
            else if (!strcmp(op, "g")) { NEED(1); gs.fill = A(0, 1); }
            else if (!strcmp(op, "G")) { NEED(1); gs.stroke = A(0, 1); }
            else if (!strcmp(op, "cm")) {
                NEED(6);
                cairo_matrix_init(&m, A(0, 6), A(1, 6), A(2, 6), A(3, 6), A(4, 6), A(5, 6));
                cairo_transform(cr, &m);
            }
            else if (!strcmp(op, "BT")) { line_x = line_y = tx = ty = 0.0; }
            else if (!strcmp(op, "Tf")) { NEED(1); gs.font = name_font; gs.size = A(0, 1); }
            else if (!strcmp(op, "Td")) {
                NEED(2);
                tx = line_x += A(0, 2);
                ty = line_y += A(1, 2);
            }
            else if (!strcmp(op, "Tj")) {
                draw_text(cr, pango, &gs, tx, ty, str->str);
                tx += fd_text_width(gs.font, gs.size, str->str);
            }
        next:
            nnum = 0;
        } else {
            p++;
        }
    }
#undef NEED
#undef A

    while (depth-- > 0)
        cairo_restore(cr);
    cairo_restore(cr);
    g_object_unref(pango);
    g_string_free(str, TRUE);
}

/* Draw page pg with its lower left corner at (x, y) of the device space
 * (y downwards) and scale s */
static void draw_page(cairo_t *cr, const Page *pg, double x, double y, double s)
{
    cairo_save(cr);
    cairo_translate(cr, x, y + s * pg->h);
    cairo_scale(cr, s, -s);
    cairo_rectangle(cr, 0.0, 0.0, pg->w, pg->h);
    cairo_clip(cr);
    draw_content(cr, pg->content, pg->len);
    cairo_restore(cr);
}

/* ---------------------------------------------------------------------- */
/* Saving                                                                 */
/* ---------------------------------------------------------------------- */

/* Error message over the graph window */
static void message(Preview *pv, const gchar *format, ...) G_GNUC_PRINTF(2, 3);
static void message(Preview *pv, const gchar *format, ...)
{
    GtkWidget *dialog;
    gchar *text;
    va_list ap;

    va_start(ap, format);
    text = g_strdup_vprintf(format, ap);
    va_end(ap);
    dialog = gtk_message_dialog_new(GTK_WINDOW(pv->window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", text);
    gtk_window_set_title(GTK_WINDOW(dialog), "FUG");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free(text);
}

/* A fugdraw figure with the content stream of a page */
static FDFig *page_figure(const Page *pg)
{
    FDFig *fig = fd_fig_new(pg->w, pg->h);

    free(fig->buf);
    fig->buf = malloc(pg->len + 1);
    memcpy(fig->buf, pg->content, pg->len + 1);
    fig->len = pg->len;
    fig->cap = pg->len + 1;
    return fig;
}

static gboolean copy_file(const gchar *from, const gchar *to, GError **error)
{
    GFile *src = g_file_new_for_path(from), *dst = g_file_new_for_path(to);
    gboolean ok = g_file_equal(src, dst) ||
                  g_file_copy(src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, error);

    g_object_unref(src);
    g_object_unref(dst);
    return ok;
}

static int format_of(const gchar *filename)
{
    const gchar *ext = getExt(filename);
    int i;

    for (i = 0; i < N_FORMATS; i++)
        if (g_ascii_strcasecmp(ext, formats[i].ext) == 0)
            return i;
    return -1;
}

static gboolean save_as(Preview *pv, const gchar *filename, GError **error)
{
    const Page *pg = &g_array_index(pv->pages, Page, pv->current);
    cairo_surface_t *surface;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_t *cr;
    FDFig *fig;
    FDPdf *pdf;
    double zero = 0.0, one = 1.0, s;
    int format = format_of(filename), failed = 0;

    switch (format) {
    case FMT_PDF:
        if (pv->is_pdf)
            return copy_file(pv->path, filename, error);
        fig = page_figure(pg);
        pdf = fd_pdf_open(filename);
        if (pdf != NULL) {
            fd_pdf_page(pdf, pg->w, pg->h, &fig, &zero, &zero, &one, 1);
            failed = fd_pdf_close(pdf);
        } else
            failed = 1;
        fd_fig_free(fig);
        break;
    case FMT_EPS:
        if (!pv->is_pdf)
            return copy_file(pv->path, filename, error);
        fig = page_figure(pg);
        failed = fd_write_eps(fig, filename);
        fd_fig_free(fig);
        break;
    case FMT_PNG:
        s = PNG_DPI / 72.0;
        surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, (int) ceil(s * pg->w),
                                             (int) ceil(s * pg->h));
        cr = cairo_create(surface);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_paint(cr);
        draw_page(cr, pg, 0.0, 0.0, s);
        cairo_destroy(cr);
        status = cairo_surface_write_to_png(surface, filename);
        cairo_surface_destroy(surface);
        break;
    case FMT_SVG:
#ifdef CAIRO_HAS_SVG_SURFACE
        surface = cairo_svg_surface_create(filename, pg->w, pg->h);
        cr = cairo_create(surface);
        draw_page(cr, pg, 0.0, 0.0, 1.0);
        cairo_destroy(cr);
        cairo_surface_finish(surface);
        status = cairo_surface_status(surface);
        cairo_surface_destroy(surface);
#else
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "SVG is not available in this build");
        return FALSE;
#endif
        break;
    default:
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                    "Unknown file type: use .pdf, .eps, .png or .svg");
        return FALSE;
    }

    if (failed || status != CAIRO_STATUS_SUCCESS) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Can not write %s%s%s", filename,
                    status != CAIRO_STATUS_SUCCESS ? ": " : "",
                    status != CAIRO_STATUS_SUCCESS ? cairo_status_to_string(status) : "");
        return FALSE;
    }
    return TRUE;
}

gboolean preview_save_as(const gchar *path, const gchar *filename, GError **error)
{
    Preview *pv = previews ? g_hash_table_lookup(previews, path) : NULL;

    if (pv == NULL) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "%s is not shown", path);
        return FALSE;
    }
    return save_as(pv, filename, error);
}

guint preview_n_pages(const gchar *path)
{
    Preview *pv = previews ? g_hash_table_lookup(previews, path) : NULL;

    return pv ? pv->pages->len : 0;
}

gboolean preview_set_footer(const gchar *path, GtkWidget *footer)
{
    Preview *pv = previews ? g_hash_table_lookup(previews, path) : NULL;

    if (pv == NULL || pv->vbox == NULL) return FALSE;

    if (pv->footer != NULL) {
        gtk_widget_destroy(pv->footer);
        pv->footer = NULL;
    }
    if (footer != NULL) {
        /* pack_end: AL PIE, debajo del dibujo. */
        gtk_box_pack_end(GTK_BOX(pv->vbox), footer, FALSE, FALSE, 0);
        gtk_widget_show_all(footer);
        pv->footer = footer;
    }
    return TRUE;
}

/* Change the extension of the name typed when the file type is changed */
static void on_filter_changed(GObject *chooser, GParamSpec *spec, gpointer data)
{
    GtkFileFilter *filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(chooser));
    gchar *filename, *base, *name;
    int format;

    if (filter == NULL)
        return;
    format = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(filter), "format"));
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    if (filename == NULL)
        return;
    base = g_path_get_basename(filename);
    if (format_of(base) >= 0)
        base[strlen(base) - strlen(getExt(base))] = '\0';
    name = g_strconcat(base, formats[format].ext, NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), name);
    g_free(name);
    g_free(base);
    g_free(filename);
}

static void save_dialog(Preview *pv)
{
    GtkWidget *dialog, *note;
    GtkFileFilter *filter, *pdf_filter = NULL;
    gchar *folder, *base, *name, *filename, *text;
    GError *error = NULL;
    int i, format;

    dialog = gtk_file_chooser_dialog_new("Save Graph As", GTK_WINDOW(pv->window),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Save",   GTK_RESPONSE_ACCEPT, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    for (i = 0; i < N_FORMATS; i++) {
#ifndef CAIRO_HAS_SVG_SURFACE
        if (i == FMT_SVG)
            continue;
#endif
        filter = gtk_file_filter_new();
        text = g_strconcat("*", formats[i].ext, NULL);
        gtk_file_filter_set_name(filter, formats[i].name);
        gtk_file_filter_add_pattern(filter, text);
        g_free(text);
        g_object_set_data(G_OBJECT(filter), "format", GINT_TO_POINTER(i));
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
        if (i == FMT_PDF)
            pdf_filter = filter;
    }
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), pdf_filter);

    if (pv->pages->len > 1) {
        text = g_strdup_printf("PDF keeps all the pages; EPS, PNG and SVG keep the page shown "
                               "(%u of %u).", pv->current + 1, pv->pages->len);
        note = gtk_label_new(text);
        g_free(text);
        gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dialog), note);
    }

    folder = g_path_get_dirname(pv->path);
    base = g_path_get_basename(pv->path);
    base[strlen(base) - strlen(getExt(base))] = '\0';
    name = g_strconcat(base, ".pdf", NULL);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), folder);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), name);
    g_signal_connect(dialog, "notify::filter", G_CALLBACK(on_filter_changed), NULL);
    g_free(folder);
    g_free(base);
    g_free(name);

    while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename == NULL)
            continue;
        /* no known extension: the one of the file type chosen */
        if (format_of(filename) < 0) {
            filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(dialog));
            format = filter ? GPOINTER_TO_INT(g_object_get_data(G_OBJECT(filter), "format")) : FMT_PDF;
            name = g_strconcat(filename, formats[format].ext, NULL);
            g_free(filename);
            filename = name;
            if (g_file_test(filename, G_FILE_TEST_EXISTS)) {
                GtkWidget *ask = gtk_message_dialog_new(GTK_WINDOW(dialog), GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                                     "A file named \"%s\" already exists. Replace it?", filename);
                gint answer = gtk_dialog_run(GTK_DIALOG(ask));
                gtk_widget_destroy(ask);
                if (answer != GTK_RESPONSE_OK) {
                    g_free(filename);
                    continue;
                }
            }
        }
        if (save_as(pv, filename, &error)) {
            preview_show_status(pv->app, "Saved %s", filename);
            g_free(filename);
            break;
        }
        message(pv, "%s", error->message);
        g_clear_error(&error);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

/* ---------------------------------------------------------------------- */
/* Printing                                                               */
/* ---------------------------------------------------------------------- */

static void on_draw_page(GtkPrintOperation *op, GtkPrintContext *context, gint n, Preview *pv)
{
    const Page *pg = &g_array_index(pv->pages, Page, n);
    cairo_t *cr = gtk_print_context_get_cairo_context(context);
    double w = gtk_print_context_get_width(context), h = gtk_print_context_get_height(context);
    double s = MIN(1.0, MIN(w / pg->w, h / pg->h));

    draw_page(cr, pg, (w - s * pg->w) / 2.0, (h - s * pg->h) / 2.0, s);
}

/* Print the graph with the print dialog, or export it to the PDF file
 * export_to (used by the tests) */
static void print_graph(Preview *pv, const gchar *export_to)
{
    GtkPrintOperation *op = gtk_print_operation_new();
    GtkPageSetup *setup = gtk_page_setup_new();
    const Page *pg = &g_array_index(pv->pages, Page, 0);
    GtkPrintOperationResult result;
    GError *error = NULL;
    gchar *base = g_path_get_basename(pv->path);

    gtk_page_setup_set_orientation(setup, pg->w > pg->h ? GTK_PAGE_ORIENTATION_LANDSCAPE
                                                        : GTK_PAGE_ORIENTATION_PORTRAIT);
    gtk_print_operation_set_default_page_setup(op, setup);
    if (print_settings != NULL)
        gtk_print_operation_set_print_settings(op, print_settings);
    gtk_print_operation_set_n_pages(op, pv->pages->len);
    gtk_print_operation_set_current_page(op, pv->current);
    gtk_print_operation_set_unit(op, GTK_UNIT_POINTS);
    gtk_print_operation_set_job_name(op, base);
    g_signal_connect(op, "draw-page", G_CALLBACK(on_draw_page), pv);

    if (export_to != NULL)
        gtk_print_operation_set_export_filename(op, export_to);
    result = gtk_print_operation_run(op, export_to ? GTK_PRINT_OPERATION_ACTION_EXPORT
                                                   : GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                     GTK_WINDOW(pv->window), &error);
    if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
        message(pv, "Can not print %s:\n%s", base, error->message);
        g_error_free(error);
    } else if (result == GTK_PRINT_OPERATION_RESULT_APPLY && export_to == NULL) {
        if (print_settings != NULL)
            g_object_unref(print_settings);
        print_settings = g_object_ref(gtk_print_operation_get_print_settings(op));
    }
    g_free(base);
    g_object_unref(setup);
    g_object_unref(op);
}

/* ---------------------------------------------------------------------- */
/* The window                                                             */
/* ---------------------------------------------------------------------- */

static void invalidate(Preview *pv)
{
    if (pv->cache != NULL)
        cairo_surface_destroy(pv->cache);
    pv->cache = NULL;
    if (gtk_widget_get_window(pv->area) != NULL)
        gtk_widget_queue_draw(pv->area);
}

/* ---------------------------------------------------------------------- */
/* Zoom                                                                   */
/* ---------------------------------------------------------------------- */

/* The scale the page is drawn at: pv->zoom, or the one that makes it fit
 * the window when pv->zoom is 0.                                         */
static double page_scale(Preview *pv)
{
    const Page *pg = &g_array_index(pv->pages, Page, pv->current);
    GtkAllocation a;
    double s;

    if (pv->zoom > 0.0) return pv->zoom;
    gtk_widget_get_allocation(pv->scroller, &a);
    s = MIN((a.width - 2.0 * MARGIN) / pg->w, (a.height - 2.0 * MARGIN) / pg->h);
    return (s > 0.0) ? s : 1.0;
}

/* Where the page is drawn inside the area: in the middle when it is
 * smaller than the window, against the margin when it is bigger.         */
static void page_origin(Preview *pv, double s, double *x, double *y)
{
    const Page *pg = &g_array_index(pv->pages, Page, pv->current);
    GtkAllocation a;

    gtk_widget_get_allocation(pv->area, &a);
    *x = floor(MAX(MARGIN, (a.width  - s * pg->w) / 2.0));
    *y = floor(MAX(MARGIN, (a.height - s * pg->h) / 2.0));
}

static void update_zoom(Preview *pv)
{
    const Page *pg = &g_array_index(pv->pages, Page, pv->current);
    double s = page_scale(pv);
    gchar *text;

    /* The area is as large as the page, so that the scrolled window
     * scrolls when it does not fit; when it fits, it follows the window. */
    if (pv->zoom > 0.0)
        gtk_widget_set_size_request(pv->area, (int) (s * pg->w) + 2 * MARGIN,
                                              (int) (s * pg->h) + 2 * MARGIN);
    else
        gtk_widget_set_size_request(pv->area, 240, 160);

    text = g_strdup_printf(" %d%% ", (int) (s * 100.0 + 0.5));
    gtk_label_set_text(GTK_LABEL(pv->zoom_label), text);
    g_free(text);
    invalidate(pv);
}

/* Zoom around a point of the window (the pointer), so that what is under
 * it stays under it.                                                     */
static void zoom_to(Preview *pv, double want, double at_x, double at_y)
{
    GtkAdjustment *ha, *va;
    double s0 = page_scale(pv), x0, y0, px, py, s1, x1, y1;

    want = CLAMP(want, ZOOM_MIN, ZOOM_MAX);
    if (want == pv->zoom) return;
    page_origin(pv, s0, &x0, &y0);
    ha = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(pv->scroller));
    va = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(pv->scroller));
    /* the point of the page under (at_x, at_y) */
    px = (gtk_adjustment_get_value(ha) + at_x - x0) / s0;
    py = (gtk_adjustment_get_value(va) + at_y - y0) / s0;

    pv->zoom = want;
    update_zoom(pv);

    s1 = page_scale(pv);
    page_origin(pv, s1, &x1, &y1);
    gtk_adjustment_set_value(ha, px * s1 + x1 - at_x);
    gtk_adjustment_set_value(va, py * s1 + y1 - at_y);
}

static void zoom_by(Preview *pv, double factor)
{
    GtkAllocation a;

    gtk_widget_get_allocation(pv->scroller, &a);
    zoom_to(pv, page_scale(pv) * factor, a.width / 2.0, a.height / 2.0);
}

static void zoom_fit(Preview *pv)
{
    pv->zoom = 0.0;
    update_zoom(pv);
}

static void update_pages(Preview *pv)
{
    gchar *text;
    gboolean several = pv->pages->len > 1;

    text = g_strdup_printf(" Page %u of %u ", pv->current + 1, pv->pages->len);
    gtk_label_set_text(GTK_LABEL(pv->page_label), text);
    g_free(text);
    gtk_widget_set_visible(pv->page_item, several);
    gtk_widget_set_sensitive(pv->prev, pv->current > 0);
    gtk_widget_set_sensitive(pv->next, pv->current + 1 < pv->pages->len);
    update_zoom(pv);
}

static void go_to(Preview *pv, gint page)
{
    page = CLAMP(page, 0, (gint) pv->pages->len - 1);
    if ((guint) page != pv->current) {
        pv->current = page;
        update_pages(pv);
    }
}

static gboolean on_draw(GtkWidget *area, cairo_t *cr, Preview *pv)
{
    GtkAllocation a;
    const Page *pg = &g_array_index(pv->pages, Page, pv->current);
    cairo_t *cache_cr;
    double s = page_scale(pv), x, y;

    gtk_widget_get_allocation(area, &a);
    if (pv->cache == NULL || pv->cache_w != a.width || pv->cache_h != a.height ||
        pv->cache_page != pv->current || pv->cache_scale != s) {
        if (pv->cache != NULL)
            cairo_surface_destroy(pv->cache);
        pv->cache = gdk_window_create_similar_surface(gtk_widget_get_window(area),
                                                      CAIRO_CONTENT_COLOR, a.width, a.height);
        pv->cache_w = a.width;
        pv->cache_h = a.height;
        pv->cache_page = pv->current;
        pv->cache_scale = s;

        cache_cr = cairo_create(pv->cache);
        cairo_set_source_rgb(cache_cr, 0.62, 0.62, 0.62);
        cairo_paint(cache_cr);
        if (s > 0.0) {
            page_origin(pv, s, &x, &y);
            cairo_set_source_rgb(cache_cr, 0.40, 0.40, 0.40);   /* shadow */
            cairo_rectangle(cache_cr, x + 3, y + 3, s * pg->w, s * pg->h);
            cairo_fill(cache_cr);
            cairo_set_source_rgb(cache_cr, 1.0, 1.0, 1.0);
            cairo_rectangle(cache_cr, x, y, s * pg->w, s * pg->h);
            cairo_fill(cache_cr);
            draw_page(cache_cr, pg, x, y, s);
        }
        cairo_destroy(cache_cr);
    }

    /* GTK+3 hands the cairo context in, already clipped to what needs
     * redrawing: the page is only redrawn into the cache when the window
     * changes size, the page changes or the zoom changes.                */
    cairo_set_source_surface(cr, pv->cache, 0, 0);
    cairo_paint(cr);
    return TRUE;
}

/* ---------------------------------------------------------------------- */
/* The magnifier, as gv has it: hold button 3 over the page and the bit    */
/* under the pointer is drawn again, larger, in a window that follows it.  */
/* It is drawn from the content stream, not from the cache, so what it     */
/* shows has the resolution of the page, not of the screen: that is what   */
/* lets one look at an incident in the data.                               */
/* ---------------------------------------------------------------------- */

static void preview_screen_size(GtkWidget *window, int *w, int *h);

static gboolean on_glass_draw(GtkWidget *w, cairo_t *cr, Preview *pv)
{
    const Page *pg = &g_array_index(pv->pages, Page, pv->current);
    double s = page_scale(pv) * pv->glass_zoom;
    double half = pv->glass_size / 2.0;

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);
    /* the point of the page under the pointer, in the middle of the glass */
    draw_page(cr, pg, half - s * pv->glass_px,
                      half - s * (pg->h - pv->glass_py), s);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, 1.0, 1.0, pv->glass_size - 2.0, pv->glass_size - 2.0);
    cairo_stroke(cr);
    return TRUE;
}

/* Where the pointer is, in points of the page (y from the bottom, as the
 * page has it).                                                          */
static void glass_point(Preview *pv, double wx, double wy)
{
    const Page *pg = &g_array_index(pv->pages, Page, pv->current);
    double s = page_scale(pv), x, y;

    page_origin(pv, s, &x, &y);
    pv->glass_px = (wx - x) / s;
    pv->glass_py = pg->h - (wy - y) / s;
}

static void glass_move(Preview *pv)
{
    GdkDisplay *display = gtk_widget_get_display(pv->window);
    GdkSeat *seat = gdk_display_get_default_seat(display);
    GdkDevice *mouse = gdk_seat_get_pointer(seat);
    int rx, ry;

    gdk_device_get_position(mouse, NULL, &rx, &ry);
    gtk_window_move(GTK_WINDOW(pv->glass), rx - pv->glass_size / 2,
                                           ry - pv->glass_size / 2);
    gtk_widget_queue_draw(pv->glass_area);
}

static void glass_show(Preview *pv, double wx, double wy)
{
    if (pv->glass == NULL) {
        int screen_w = 1280, screen_h = 1024;

        /* GLASS, unless the screen is small */
        preview_screen_size(pv->window, &screen_w, &screen_h);
        pv->glass_size = MIN(GLASS, (int) (0.7 * MIN(screen_w, screen_h)));
        pv->glass = gtk_window_new(GTK_WINDOW_POPUP);
        gtk_window_set_transient_for(GTK_WINDOW(pv->glass), GTK_WINDOW(pv->window));
        gtk_widget_set_size_request(pv->glass, pv->glass_size, pv->glass_size);
        pv->glass_area = gtk_drawing_area_new();
        gtk_container_add(GTK_CONTAINER(pv->glass), pv->glass_area);
        g_signal_connect(pv->glass_area, "draw", G_CALLBACK(on_glass_draw), pv);
        gtk_widget_show_all(pv->glass_area);
    }
    pv->glass_zoom = GLASS_X;
    glass_point(pv, wx, wy);
    gtk_widget_show(pv->glass);
    glass_move(pv);
}

static void glass_hide(Preview *pv)
{
    if (pv->glass != NULL)
        gtk_widget_hide(pv->glass);
}

/* ---------------------------------------------------------------------- */
/* Mouse: button 1 drags the page when it does not fit, button 3 is the    */
/* magnifier, and the wheel with Ctrl zooms where the pointer is.          */
/* ---------------------------------------------------------------------- */

/* With "fit", the scale follows the window: the label has to follow too. */
static void on_area_resize(GtkWidget *w, GdkRectangle *alloc, Preview *pv)
{
    if (pv->zoom == 0.0 && pv->zoom_label != NULL) {
        gchar *text = g_strdup_printf(" %d%% ", (int) (page_scale(pv) * 100.0 + 0.5));

        gtk_label_set_text(GTK_LABEL(pv->zoom_label), text);
        g_free(text);
    }
}

static gboolean on_button_press(GtkWidget *area, GdkEventButton *ev, Preview *pv)
{
    if (ev->button == 3 || (ev->button == 1 && (ev->state & GDK_SHIFT_MASK))) {
        glass_show(pv, ev->x, ev->y);
        return TRUE;
    }
    if (ev->button == 1) {
        pv->panning = TRUE;
        pv->pan_x = ev->x_root;
        pv->pan_y = ev->y_root;
        gdk_window_set_cursor(gtk_widget_get_window(area),
                              gdk_cursor_new_from_name(gtk_widget_get_display(area), "grabbing"));
        return TRUE;
    }
    return FALSE;
}

static gboolean on_button_release(GtkWidget *area, GdkEventButton *ev, Preview *pv)
{
    glass_hide(pv);
    if (pv->panning) {
        pv->panning = FALSE;
        gdk_window_set_cursor(gtk_widget_get_window(area), NULL);
    }
    return TRUE;
}

static gboolean on_motion(GtkWidget *area, GdkEventMotion *ev, Preview *pv)
{
    if (pv->glass != NULL && gtk_widget_get_visible(pv->glass)) {
        glass_point(pv, ev->x, ev->y);
        glass_move(pv);
        return TRUE;
    }
    if (pv->panning) {
        GtkAdjustment *ha = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(pv->scroller));
        GtkAdjustment *va = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(pv->scroller));

        gtk_adjustment_set_value(ha, gtk_adjustment_get_value(ha) - (ev->x_root - pv->pan_x));
        gtk_adjustment_set_value(va, gtk_adjustment_get_value(va) - (ev->y_root - pv->pan_y));
        pv->pan_x = ev->x_root;
        pv->pan_y = ev->y_root;
        return TRUE;
    }
    return FALSE;
}

static gboolean on_scroll(GtkWidget *area, GdkEventScroll *ev, Preview *pv)
{
    if ((ev->state & GDK_CONTROL_MASK) == 0)
        return FALSE;                         /* sin Ctrl, que se desplace */
    if (ev->direction == GDK_SCROLL_UP)
        zoom_to(pv, page_scale(pv) * ZOOM_STEP, ev->x, ev->y);
    else if (ev->direction == GDK_SCROLL_DOWN)
        zoom_to(pv, page_scale(pv) / ZOOM_STEP, ev->x, ev->y);
    else if (ev->direction == GDK_SCROLL_SMOOTH && ev->delta_y != 0.0)
        zoom_to(pv, page_scale(pv) * pow(ZOOM_STEP, -ev->delta_y), ev->x, ev->y);
    return TRUE;
}

static void on_zoom_in(GtkButton *b, Preview *pv)  { zoom_by(pv, ZOOM_STEP); }
static void on_zoom_out(GtkButton *b, Preview *pv) { zoom_by(pv, 1.0 / ZOOM_STEP); }
static void on_zoom_fit(GtkButton *b, Preview *pv) { zoom_fit(pv); }
static void on_zoom_one(GtkButton *b, Preview *pv)
{
    GtkAllocation a;

    gtk_widget_get_allocation(pv->scroller, &a);
    zoom_to(pv, 1.0, a.width / 2.0, a.height / 2.0);
}

static void on_save(GtkButton *b, Preview *pv)    { save_dialog(pv); }
static void on_print(GtkButton *b, Preview *pv)   { print_graph(pv, NULL); }
static void on_viewer(GtkButton *b, Preview *pv)  { preview_open_external(pv->app, pv->path); }
static void on_prev(GtkButton *b, Preview *pv)    { go_to(pv, (gint) pv->current - 1); }
static void on_next(GtkButton *b, Preview *pv)    { go_to(pv, (gint) pv->current + 1); }
static void on_close(GtkButton *b, Preview *pv)   { gtk_widget_destroy(pv->window); }

static gboolean on_key(GtkWidget *w, GdkEventKey *event, Preview *pv)
{
    gboolean ctrl = (event->state & GDK_CONTROL_MASK) != 0;

    switch (event->keyval) {
    case GDK_KEY_Page_Up: case GDK_KEY_Left: case GDK_KEY_Up: case GDK_KEY_BackSpace:
        go_to(pv, (gint) pv->current - 1);
        return TRUE;
    case GDK_KEY_Page_Down: case GDK_KEY_Right: case GDK_KEY_Down: case GDK_KEY_space:
        go_to(pv, (gint) pv->current + 1);
        return TRUE;
    case GDK_KEY_Home:
        go_to(pv, 0);
        return TRUE;
    case GDK_KEY_End:
        go_to(pv, (gint) pv->pages->len - 1);
        return TRUE;
    case GDK_KEY_Escape:
        gtk_widget_destroy(pv->window);
        return TRUE;
    case GDK_KEY_plus: case GDK_KEY_equal: case GDK_KEY_KP_Add:
        zoom_by(pv, ZOOM_STEP);
        return TRUE;
    case GDK_KEY_minus: case GDK_KEY_KP_Subtract:
        zoom_by(pv, 1.0 / ZOOM_STEP);
        return TRUE;
    case GDK_KEY_0: case GDK_KEY_KP_0:
        zoom_fit(pv);
        return TRUE;
    case GDK_KEY_1: case GDK_KEY_KP_1:
        on_zoom_one(NULL, pv);
        return TRUE;
    case GDK_KEY_s: case GDK_KEY_S:
        if (ctrl) { save_dialog(pv); return TRUE; }
        break;
    case GDK_KEY_p: case GDK_KEY_P:
        if (ctrl) { print_graph(pv, NULL); return TRUE; }
        break;
    case GDK_KEY_w: case GDK_KEY_W:
        if (ctrl) { gtk_widget_destroy(pv->window); return TRUE; }
        break;
    }
    return FALSE;
}

static void on_destroy(GtkWidget *w, Preview *pv)
{
    g_hash_table_remove(previews, pv->path);
    if (pv->glass != NULL)
        gtk_widget_destroy(pv->glass);
    if (pv->cache != NULL)
        cairo_surface_destroy(pv->cache);
    pages_free(pv->pages);
    g_free(pv->path);
    g_free(pv);
}

/* A flat button of the bar with an icon, and a label if label != NULL.
 * (Plain buttons: GtkToolbar gives warnings with the Windows theme.) */
static GtkWidget *bar_button(GtkWidget *bar, const gchar *icon, const gchar *label,
                             const gchar *tip, gboolean at_end, GCallback callback, Preview *pv)
{
    GtkWidget *button = label ? gtk_button_new_with_mnemonic(label) : gtk_button_new();

    gtk_button_set_image(GTK_BUTTON(button), gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_BUTTON));
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_widget_set_focus_on_click(button, FALSE);
    gtk_widget_set_tooltip_text(button, tip);
    g_signal_connect(button, "clicked", callback, pv);
    if (at_end)
        gtk_box_pack_end(GTK_BOX(bar), button, FALSE, FALSE, 0);
    else
        gtk_box_pack_start(GTK_BOX(bar), button, FALSE, FALSE, 0);
    return button;
}

/* How big the screen is: gdk_screen_get_width() is gone in GTK+3.22 and the
 * window may not have a monitor yet, so there is a sane default.         */
static void preview_screen_size(GtkWidget *window, int *w, int *h)
{
#if GTK_CHECK_VERSION(3, 22, 0)
    GdkDisplay *display = gtk_widget_get_display(window);
    GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
    GdkRectangle geometry;

    if (monitor == NULL && gdk_display_get_n_monitors(display) > 0)
        monitor = gdk_display_get_monitor(display, 0);
    if (monitor != NULL) {
        gdk_monitor_get_geometry(monitor, &geometry);
        *w = geometry.width;
        *h = geometry.height;
    }
#else
    GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(window));

    *w = gdk_screen_get_width(screen);
    *h = gdk_screen_get_height(screen);
#endif
}

static Preview *preview_new(PreviewApp *app, const gchar *path)
{
    Preview *pv = g_new0(Preview, 1);
    GtkWidget *vbox, *bar, *zoom_item;

    pv->app = app;
    pv->path = g_strdup(path);
    pv->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_role(GTK_WINDOW(pv->window), "atsw-graph");

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(pv->window), vbox);
    pv->vbox = vbox;
    bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_container_set_border_width(GTK_CONTAINER(bar), 3);
    gtk_box_pack_start(GTK_BOX(vbox), bar, FALSE, FALSE, 0);

    bar_button(bar, "document-save-as", "_Save As...", "Save the graph as PDF, EPS, PNG or SVG (Ctrl+S)",
               FALSE, G_CALLBACK(on_save), pv);
    bar_button(bar, "document-print", "_Print...", "Print the graph (Ctrl+P)",
               FALSE, G_CALLBACK(on_print), pv);
    bar_button(bar, "document-open", "_External Viewer", "Open the file with the viewer of the system",
               FALSE, G_CALLBACK(on_viewer), pv);
    zoom_item = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(bar), zoom_item, FALSE, FALSE, 12);
    bar_button(zoom_item, "zoom-out", NULL, "Smaller (-)",
               FALSE, G_CALLBACK(on_zoom_out), pv);
    pv->zoom_label = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(zoom_item), pv->zoom_label, FALSE, FALSE, 0);
    bar_button(zoom_item, "zoom-in", NULL, "Larger (+)",
               FALSE, G_CALLBACK(on_zoom_in), pv);
    bar_button(zoom_item, "zoom-fit-best", NULL, "Fit the page in the window (0)",
               FALSE, G_CALLBACK(on_zoom_fit), pv);
    bar_button(zoom_item, "zoom-original", NULL, "One point, one pixel (1)",
               FALSE, G_CALLBACK(on_zoom_one), pv);

    pv->page_item = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(bar), pv->page_item, FALSE, FALSE, 12);
    pv->prev = bar_button(pv->page_item, "go-previous", NULL, "Previous page (Page Up)",
                          FALSE, G_CALLBACK(on_prev), pv);
    pv->page_label = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(pv->page_item), pv->page_label, FALSE, FALSE, 0);
    pv->next = bar_button(pv->page_item, "go-next", NULL, "Next page (Page Down)",
                          FALSE, G_CALLBACK(on_next), pv);
    bar_button(bar, "window-close", "_Close", "Close the window (Esc)",
               TRUE, G_CALLBACK(on_close), pv);

    /* The page goes inside a scrolled window: when the zoom makes it
     * larger than the window there is something to scroll.               */
    pv->scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pv->scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), pv->scroller, TRUE, TRUE, 0);

    pv->area = gtk_drawing_area_new();
    gtk_widget_set_app_paintable(pv->area, TRUE);
    gtk_widget_add_events(pv->area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                                    GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK |
                                    GDK_SMOOTH_SCROLL_MASK);
    gtk_container_add(GTK_CONTAINER(pv->scroller), pv->area);
    g_signal_connect(pv->area, "draw", G_CALLBACK(on_draw), pv);
    g_signal_connect(pv->area, "button-press-event", G_CALLBACK(on_button_press), pv);
    g_signal_connect(pv->area, "button-release-event", G_CALLBACK(on_button_release), pv);
    g_signal_connect(pv->area, "motion-notify-event", G_CALLBACK(on_motion), pv);
    g_signal_connect(pv->area, "scroll-event", G_CALLBACK(on_scroll), pv);
    g_signal_connect(pv->area, "size-allocate", G_CALLBACK(on_area_resize), pv);
    g_signal_connect(pv->window, "key-press-event", G_CALLBACK(on_key), pv);
    g_signal_connect(pv->window, "destroy", G_CALLBACK(on_destroy), pv);

    gtk_widget_show_all(vbox);
    return pv;
}

gboolean preview_show(PreviewApp *app, const gchar *path)
{
    Preview *pv;
    GArray *pages;
    gboolean is_pdf = FALSE;
    const Page *pg;
    gchar *base, *title;
    double s;
    int screen_w = 1280, screen_h = 1024;

    pages = load_file(path, &is_pdf);
    if (pages == NULL)
        return FALSE;

    if (previews == NULL)
        previews = g_hash_table_new(g_str_hash, g_str_equal);
    pv = g_hash_table_lookup(previews, path);
    if (pv == NULL) {
        pv = preview_new(app, path);
        g_hash_table_insert(previews, pv->path, pv);

        /* size: the first page, as large as it fits in 80% of the screen */
        pg = &g_array_index(pages, Page, 0);
        preview_screen_size(pv->window, &screen_w, &screen_h);
        s = MIN(1.4, MIN(0.8 * screen_w / pg->w, 0.8 * screen_h / pg->h));
        gtk_widget_set_size_request(pv->area, 240, 160);
        gtk_window_set_default_size(GTK_WINDOW(pv->window), (int) (s * pg->w) + 2 * MARGIN,
                                    (int) (s * pg->h) + 2 * MARGIN + 40);
    } else {
        pages_free(pv->pages);            /* the engine made the file again */
    }
    pv->pages = pages;
    pv->is_pdf = is_pdf;
    if (pv->current >= pages->len)
        pv->current = 0;

    base = g_path_get_basename(path);
    title = g_strdup_printf("%s", base);
    gtk_window_set_title(GTK_WINDOW(pv->window), title);
    g_free(title);
    g_free(base);

    update_pages(pv);
    gtk_window_present(GTK_WINDOW(pv->window));
    return TRUE;
}
