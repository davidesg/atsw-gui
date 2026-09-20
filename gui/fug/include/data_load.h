#ifndef DATA_LOAD_H
#define DATA_LOAD_H

#include <gtk/gtk.h>
#include "datos.h"
#include "nlutils.h"

/* Series and model currently loaded in the GUI (defined in data_load.c).
 * Ts.data holds Ts.nobs observations, 0-based, allocated with g_new(). */
extern struct Tseries Ts;
extern struct Tusmodel Tm;

int default_lags(int nobs, int freq);
int default_nog(int freq);
const char *getExt(const char *fspec);

/* How an .inp file compares with the series of the GUI (compare_input) */
typedef enum {
    INPUT_MISSING,      /* there is no file                                   */
    INPUT_SAME,         /* same series (and, without model, same transformation) */
    INPUT_DIFFERENT,    /* another series or transformation, without model    */
    INPUT_MODEL,        /* a fue model of another series                      */
    INPUT_MODEL_TRANSF, /* la MISMA serie con modelo de fue, y la ventana pide
                         * OTRA transformacion. Se dice y se para: ver
                         * SaveInpFile.                                     */
    INPUT_UNREADABLE    /* not an .inp file                                   */
} InputStatus;

double *read_data_values(const char *filename, int *nvalues, GError **error);

/* La misma lectura, pero contando lo que paso: *fuera recibe todo lo que el
 * fichero dijo --frecuencia, fecha, nombres-- y aviso[naviso] lo que hay que
 * contarle al analista AUNQUE LA CARGA FUERA BIEN (por ejemplo, que su fichero
 * traia tres columnas y fug es univariante). Los dos pueden ser NULL.      */
double *read_data_series(const char *filename, int *nvalues, DtDatos *fuera,
                         char *aviso, size_t naviso, GError **error);
gboolean load_input(const char *inputf, GError **error);
InputStatus compare_input(const char *path);
gboolean save_input(const char *outputf, GError **error);
void set_series_data(double *data, int nobs);

#endif
