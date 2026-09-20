#include "data_load.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include "inpfile.h"
#include "datos.h"

/* Global structures shared by the callbacks */
struct Tseries Ts;
struct Tusmodel Tm;

int default_lags  ( int nobs, int freq )

/* Same rule as default_lags() in fug 1.13 (diagnose.c) */
{
int lags;
   if ( nobs < 3 * ( freq  + 1) )
      lags =  nobs -  freq  / 2;
   else if ( ( freq == 1 ) && ( nobs > 200 ) ) lags = 45;
   else if ( freq  == 1) lags = 9;
   else lags = 3 * ( freq  + 1);
   if ( lags > nobs - 2 ) lags = nobs - 2;
   if ( lags < 1 ) lags = 1;
   return (lags);
}

int default_nog (int freq )

{
	int nog;
	if (freq == 12) nog = 12;
	else if (freq == 4 ) nog = 8;
	else if (freq == 1) nog = 8;
	else nog = 8;
	return (nog);
}

const char *getExt (const char *fspec)
/* Find the extention of the filename                    */
/* example : if ( strcmp (".txt", getExt(inputf)) == 0 ) */
{
    const char *e;
	e = strrchr (fspec, '.');
    if (e == NULL)
        e = ""; // fast method, could also use &(fspec[strlen(fspec)]).
    return e;
}

/* Replace the series data owned by Ts (takes ownership of data). */
void set_series_data (double *data, int nobs)
{
	g_free (Ts.data);
	Ts.data = data;
	Ts.nobs = nobs;
}

/* What the last .inp file read had besides the series and the Box-Cox line:
 * kept when the GUI writes the .inp again. */
static int    input_ifadf[7];
static double input_cbands = 0.0, input_refactor = 1.0;

static void clear_input_extras (void)
{
	memset (input_ifadf, 0, sizeof input_ifadf);
	input_cbands = 0.0;
	input_refactor = 1.0;
}

/* Aqui vivian parse_value() y collect_values(), el lector que APLANABA todas
 * las columnas en un vector. Los dos se fueron con el a lib/datos, que hace lo
 * mismo con la coma decimal y ademas acierta con las columnas.          */

/* LOS DATOS ENTRAN POR lib/datos, QUE ES LA UNICA PUERTA.
 *
 * Aqui habia un lector del mismo formato que APLANABA todas las columnas de
 * todas las filas en un solo vector. fue, con el mismo fichero, se quedaba con
 * la primera columna: EL MISMO FICHERO DABA DOS SERIES DISTINTAS
 * (INVENTARIO-madre.md §1).
 *
 * Y la de aqui era la equivocada: aplanar dos columnas fabrica una serie que
 * no existe -- entrelaza dos. La decision esta razonada en DISENO-madre.md §4.
 *
 * ES UN CAMBIO DE COMPORTAMIENTO, y se nota: un fichero de dos columnas y 100
 * filas daba 200 valores y ahora da 100. Por eso las columnas que se
 * descartan SE DICEN, en vez de tragarselas: fug es univariante y no tiene
 * donde poner un regresor, pero el analista tiene que enterarse de que su
 * fichero traia mas cosas.
 *
 * Lo que se gana: un CSV de verdad se lee con sus comas, la coma decimal se
 * acepta, la cabecera da NOMBRE a la serie, y la frecuencia y la fecha salen
 * DEL FICHERO si las trae.
 *
 * aviso[naviso] recibe lo que haya que contar al cargar bien; puede ser NULL.
 * Devuelve el vector de la serie (g_free por el que llama) o NULL con error. */
double *read_data_values (const char *filename, int *nvalues, GError **error)
{
	return read_data_series (filename, nvalues, NULL, NULL, 0, error);
}

double *read_data_series (const char *filename, int *nvalues, DtDatos *fuera,
                          char *aviso, size_t naviso, GError **error)
{
	DtDatos *d;
	DtError  e;
	double  *v;
	char     b[256];
	int      i;

	if (aviso && naviso) aviso[0] = '\0';
	clear_input_extras ();

	d = g_new0 (DtDatos, 1);          /* 2 MB: en la pila no cabe */
	if (dt_leer (filename, d, &e) != 0) {
		dt_error_en (&e, b, sizeof b);
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
		             "%s\n%s", b, filename);
		g_free (d);
		return NULL;
	}

	v = (double *) g_malloc (sizeof (double) * (size_t) d->nobs);
	for (i = 0; i < d->nobs; i++) v[i] = d->v[0][i];
	*nvalues = d->nobs;

	/* LAS COLUMNAS QUE SE DESCARTAN, DICHAS. fug es univariante. */
	if (aviso && naviso && d->ncol > 1)
		snprintf (aviso, naviso,
		          "%d columns in the file: using the first%s%s%s. "
		          "fug is univariate.",
		          d->ncol,
		          d->nombre[0][0] ? " (" : "",
		          d->nombre[0][0] ? d->nombre[0] : "",
		          d->nombre[0][0] ? ")" : "");

	if (fuera) *fuera = *d;
	g_free (d);
	return v;
}

/* Read an .inp file (the .inp of fue, or of fug <= 1.14) into Ts and Tm,
 * with the same reader as fug (src/inpfile.c). */
gboolean load_input (const char *inputf, GError **error)
{
	InpFile inp;
	char message[256];
	double *data;

	if (inp_read (inputf, &inp, message, sizeof message) != 0) {
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
		             "Can not read the input file:\n%s\n%s", inputf, message);
		return FALSE;
	}
	if (inp.freq != 1 && inp.freq != 4 && inp.freq != 12) {
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
		             "The frequency %d of the input file is not 1, 4 or 12:\n%s",
		             inp.freq, inputf);
		inp_free (&inp);
		return FALSE;
	}
	data = g_new (double, inp.nobs);
	memcpy (data, inp.data, inp.nobs * sizeof (double));

	g_free ((gchar *) Ts.name);
	Ts.name    = g_strdup (inp.name);
	Ts.freq    = inp.freq;
	Ts.begtime = inp.begtime;
	Ts.outyear = inp.outyear;
	Ts.begyear = inp.begyear;
	Tm.boxlam  = inp.boxlam;
	Tm.boxm    = inp.boxm;
	Tm.nrdiff  = inp.nrdiff;
	Tm.nadiff  = inp.nadiff;
	set_series_data (data, inp.nobs);
	memcpy (input_ifadf, inp.ifadf, sizeof input_ifadf);
	input_cbands = inp.cbands;
	input_refactor = inp.refactor;
	inp_free (&inp);
	return TRUE;
}

/* The .inp that the GUI would write, from Ts and Tm (data not copied). */
static void current_input (InpFile *inp)
{
	memset (inp, 0, sizeof *inp);
	inp->fue      = 1;
	inp->freq     = Ts.freq;
	inp->nobs     = Ts.nobs;
	inp->begtime  = Ts.begtime;
	inp->outyear  = Ts.outyear;
	inp->begyear  = Ts.begyear;
	g_strlcpy (inp->name, Ts.name ? Ts.name : "", sizeof inp->name);
	inp->boxlam   = Tm.boxlam;
	inp->nrdiff   = Tm.nrdiff;
	inp->nadiff   = Tm.nadiff;
	memcpy (inp->ifadf, input_ifadf, sizeof inp->ifadf);
	inp->cbands   = input_cbands;
	inp->refactor = input_refactor;
	inp->data     = Ts.data;
}

/* How the .inp file at path compares with the series of the GUI. */
InputStatus compare_input (const char *path)
{
	InpFile inp, now;
	char message[256];
	gboolean same_series;
	InputStatus status;

	if (!g_file_test (path, G_FILE_TEST_EXISTS))
		return INPUT_MISSING;
	if (inp_read (path, &inp, message, sizeof message) != 0)
		return INPUT_UNREADABLE;
	current_input (&now);
	same_series = inp.freq == now.freq && inp.nobs == now.nobs && inp.begyear == now.begyear &&
	              (inp.freq > 1 ? inp.begtime == now.begtime : inp.outyear == now.outyear) &&
	              memcmp (inp.data, now.data, now.nobs * sizeof (double)) == 0;
	/* CON MODELO, LA TRANSFORMACION TAMBIEN CUENTA. Antes no se miraba: un
	 * .inp con modelo de fue y lambda 0, d 1 se daba por «igual» aunque la
	 * ventana dijera lambda 1, d 0 -- y el GUI lo dejaba intacto porque
	 * contaba con que "-B" llevara la transformacion por la orden. Quitado
	 * -B, esa discrepancia hay que verla.                              */
	if (inp.model)
		status = !same_series ? INPUT_MODEL
		       : (inp.boxlam == now.boxlam && inp.nrdiff == now.nrdiff &&
		          inp.nadiff == now.nadiff) ? INPUT_SAME : INPUT_MODEL_TRANSF;
	else
		status = (same_series && inp.fue && strcmp (inp.name, now.name) == 0 &&
		          inp.boxlam == now.boxlam && inp.nrdiff == now.nrdiff &&
		          inp.nadiff == now.nadiff) ? INPUT_SAME : INPUT_DIFFERENT;
	inp_free (&inp);
	return status;
}

/* Write Ts and Tm as a fue .inp without model (src/inpfile.c). */
gboolean save_input (const char *outputf, GError **error)
{
	InpFile inp;

	current_input (&inp);
	if (inp_write_bare (outputf, &inp) != 0) {
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_ACCES,
		             "Can not write the input file:\n%s\nHint: check write permissions.",
		             outputf);
		return FALSE;
	}
	return TRUE;
}
