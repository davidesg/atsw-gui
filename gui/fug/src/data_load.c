#include "data_load.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include "inpfile.h"

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

/* Parse one number; accepts a decimal comma ("77,5") when there is no dot. */
static gboolean parse_value (const char *token, double *value)
{
	gchar *copy, *end, *comma;
	gboolean ok;

	copy = g_strdup (token);
	comma = strchr (copy, ',');
	if (comma != NULL && strchr (copy, '.') == NULL && strchr (comma + 1, ',') == NULL)
		*comma = '.';
	*value = g_ascii_strtod (copy, &end);
	ok = (end != copy && *end == '\0');
	g_free (copy);
	return ok;
}

/* Collect every number found in lines[first..]. Stops at the first token
 * that is not a number. Returns a newly allocated 0-based array. */
static double *collect_values (gchar **lines, int first, int *nvalues, GError **error,
                               const char *filename)
{
	GArray *values = g_array_new (FALSE, FALSE, sizeof (double));
	int i, j;
	double v;

	for (i = first; lines[i] != NULL; i++) {
		gchar **tokens = g_strsplit_set (lines[i], " \t\r;", -1);
		for (j = 0; tokens[j] != NULL; j++) {
			if (*tokens[j] == '\0')
				continue;
			if (!parse_value (tokens[j], &v)) {
				g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
				             "Invalid value \"%s\" at line %d of\n%s",
				             tokens[j], i + 1, filename);
				g_strfreev (tokens);
				g_array_free (values, TRUE);
				return NULL;
			}
			g_array_append_val (values, v);
		}
		g_strfreev (tokens);
	}
	*nvalues = values->len;
	return (double *) g_array_free (values, FALSE);
}

/* Read a plain data file: numbers separated by blanks or new lines.
 * Leading lines that do not start with a number (headers) are skipped. */
double *read_data_values (const char *filename, int *nvalues, GError **error)
{
	gchar *contents, **lines;
	double *data, v;
	int first = 0;

	clear_input_extras ();

	if (!g_file_get_contents (filename, &contents, NULL, error))
		return NULL;
	lines = g_strsplit (contents, "\n", -1);
	while (lines[first] != NULL) {
		gchar **tokens = g_strsplit_set (g_strstrip (lines[first]), " \t\r;", 2);
		gboolean numeric = (tokens[0] != NULL && parse_value (tokens[0], &v));
		gboolean blank = (tokens[0] == NULL || *tokens[0] == '\0');
		g_strfreev (tokens);
		if (numeric)
			break;
		if (!blank && first > 0)   /* only one header line is allowed */
			break;
		first++;
	}
	data = collect_values (lines, first, nvalues, error, filename);
	g_strfreev (lines);
	g_free (contents);
	return data;
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
	if (inp.model)
		status = same_series ? INPUT_SAME : INPUT_MODEL;
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
