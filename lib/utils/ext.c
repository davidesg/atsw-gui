/* ext.c -- la extension de un nombre de fichero.
 *
 * Estaba en utils.c, que es el cajon del GUI de fue: lo demas que hay alli
 * --inp_ok_to_load, inp_fits_gui-- arrastra inpcheck y las constantes de ese
 * GUI. getExt no arrastra nada, y lib/preview la necesita, asi que vive sola.
 * La declaracion sigue en utils.h: fue y fug no se enteran.
 */

#include <string.h>

#include "utils.h"

const char *getExt(const char *fspec) {
    const char *dot = strrchr(fspec, '.');
    if (!dot) return "";
    return dot;
}
