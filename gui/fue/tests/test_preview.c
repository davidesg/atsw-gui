/* test_preview.c -- the graph window without a window.
 *
 * src/preview.c reads back the PDF content stream that fugdraw writes and
 * draws it with Cairo. Everything up to the drawing is plain code: this
 * test includes the file to reach it.
 *
 *   test_preview make <out.pdf> <out.eps>
 *       draws a page with fugdraw, the same way fue and fuf draw theirs,
 *       and writes it both ways.
 *
 *   test_preview show <file> <dpi> [out.png]
 *       loads it back, draws it into an image and prints what it found:
 *
 *           pages 1
 *           points 200.00 100.00
 *           ink 41 375 41 166
 *
 * tests/run_tests.sh checks that the ink lands where it was drawn and that
 * the PDF and the EPS give the same picture -- one round trip through
 * fugdraw and back.
 */

#include "preview.c"

/* What the program gives the window; there is no window here, so these
 * have to exist.                                                         */
void preview_open_external(PreviewApp *app, const gchar *path) { (void) app; (void) path; }
void preview_show_status(PreviewApp *app, const gchar *format, ...) { (void) app; (void) format; }

#define PW 200.0
#define PH 100.0

/* A page with ink at known places: a frame from (20,20) to (180,80), a
 * diagonal, a disc and a line of text.                                    */
/* Una pagina con un solo disco, para comprobar la lupa: si el
 * desplazamiento esta bien, el disco sale centrado en el cristal.        */
static int make_dot(const char *path) {
    FDFig *fig = fd_fig_new(PW, PH);
    FDPdf *pdf;
    double zero = 0.0, one = 1.0;
    int failed = 0;

    if (fig == NULL) return 1;
    fd_disc(fig, 100.0, 50.0, 6.0);
    pdf = fd_pdf_open(path);
    if (pdf == NULL) failed = 1;
    else {
        fd_pdf_page(pdf, PW, PH, &fig, &zero, &zero, &one, 1);
        failed |= fd_pdf_close(pdf);
    }
    fd_fig_free(fig);
    return failed;
}

static int make_fixture(const char *pdf_path, const char *eps_path) {
    FDFig *fig = fd_fig_new(PW, PH);
    FDPdf *pdf;
    double x[5] = { 20.0, 180.0, 180.0, 20.0, 20.0 };
    double y[5] = { 20.0,  20.0,  80.0, 80.0, 20.0 };
    double zero = 0.0, one = 1.0;
    int failed;

    if (fig == NULL) return 1;
    fd_linewidth(fig, 1.0);
    fd_polyline(fig, x, y, 5);
    fd_line(fig, 20.0, 20.0, 180.0, 80.0);
    fd_disc(fig, 100.0, 50.0, 6.0);
    fd_text(fig, 100.0, 30.0, FD_HELV, 10.0, FD_CENTER, "fugdraw");

    failed = fd_write_eps(fig, eps_path);
    pdf = fd_pdf_open(pdf_path);
    if (pdf == NULL) failed = 1;
    else {
        fd_pdf_page(pdf, PW, PH, &fig, &zero, &zero, &one, 1);
        failed |= fd_pdf_close(pdf);
    }
    fd_fig_free(fig);
    return failed;
}

static int show(const char *path, double dpi, const char *png_path) {
    GArray *pages;
    gboolean is_pdf = FALSE;
    const Page *pg;
    cairo_surface_t *surface;
    cairo_t *cr;
    unsigned char *data;
    double s = dpi / 72.0;
    int w, h, stride, i, j, x0 = -1, x1 = -1, y0 = -1, y1 = -1;

    pages = load_file(path, &is_pdf);
    if (pages == NULL) {
        fprintf(stderr, "not a file drawn by fugdraw: %s\n", path);
        return 1;
    }
    pg = &g_array_index(pages, Page, 0);
    printf("pages %u\npoints %.2f %.2f\n", pages->len, pg->w, pg->h);

    w = (int) (s * pg->w + 0.5);
    h = (int) (s * pg->h + 0.5);
    surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);
    draw_page(cr, pg, 0.0, 0.0, s);
    cairo_destroy(cr);
    cairo_surface_flush(surface);

    /* where the ink is */
    data   = cairo_image_surface_get_data(surface);
    stride = cairo_image_surface_get_stride(surface);
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (data[j * stride + 4 * i] < 128) {      /* the blue byte */
                if (x0 < 0 || i < x0) x0 = i;
                if (i > x1) x1 = i;
                if (y0 < 0) y0 = j;
                y1 = j;
            }
    printf("ink %d %d %d %d\n", x0, x1, y0, y1);

    if (png_path != NULL) cairo_surface_write_to_png(surface, png_path);
    cairo_surface_destroy(surface);
    pages_free(pages);
    return 0;
}

/* Lo que dibuja la lupa: el punto (px, py) de la pagina, en el centro del
 * cristal, a escala s. Es la cuenta de on_glass_draw(), y GLASS es el de
 * preview.c, que se incluye arriba.                                      */

static int glass(const char *path, double px, double py, double s, const char *png_path) {
    GArray *pages;
    gboolean is_pdf = FALSE;
    const Page *pg;
    cairo_surface_t *surface;
    cairo_t *cr;
    unsigned char *data;
    int stride, i, j, x0 = -1, x1 = -1, y0 = -1, y1 = -1;

    pages = load_file(path, &is_pdf);
    if (pages == NULL) return 1;
    pg = &g_array_index(pages, Page, 0);

    surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, GLASS, GLASS);
    cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);
    draw_page(cr, pg, GLASS / 2.0 - s * px, GLASS / 2.0 - s * (pg->h - py), s);
    cairo_destroy(cr);
    cairo_surface_flush(surface);

    data   = cairo_image_surface_get_data(surface);
    stride = cairo_image_surface_get_stride(surface);
    for (j = 0; j < GLASS; j++)
        for (i = 0; i < GLASS; i++)
            if (data[j * stride + 4 * i] < 128) {
                if (x0 < 0 || i < x0) x0 = i;
                if (i > x1) x1 = i;
                if (y0 < 0) y0 = j;
                y1 = j;
            }
    printf("glass %d\nink %d %d %d %d\n", GLASS, x0, x1, y0, y1);
    if (png_path != NULL) cairo_surface_write_to_png(surface, png_path);
    cairo_surface_destroy(surface);
    pages_free(pages);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 4 && strcmp(argv[1], "make") == 0)
        return make_fixture(argv[2], argv[3]) || make_dot("g.pdf");
    if (argc >= 6 && strcmp(argv[1], "glass") == 0)
        return glass(argv[2], atof(argv[3]), atof(argv[4]), atof(argv[5]),
                     (argc > 6) ? argv[6] : NULL);
    if (argc >= 4 && strcmp(argv[1], "show") == 0)
        return show(argv[2], atof(argv[3]), (argc > 4) ? argv[4] : NULL);
    fprintf(stderr, "usage: test_preview make <out.pdf> <out.eps>\n"
                    "       test_preview show <file> <dpi> [out.png]\n"
                    "       test_preview glass <file> <px> <py> <scale> [out.png]\n");
    return 2;
}
