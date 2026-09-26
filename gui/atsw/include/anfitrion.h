/*
 * anfitrion.h -- lo que la madre le da a lib/analisis.
 *
 * APARTE DE atsw.h A PROPOSITO. analisis.h incluye preview.h, que incluye el
 * previewhost.h de este programa, que incluye atsw.h: si atsw.h incluyera
 * analisis.h el circulo se cerraria y PreviewApp se declararia antes que
 * Atsw. El anfitrion lo necesitan dos ficheros, no todos.
 */

#ifndef ATSW_ANFITRION_H
#define ATSW_ANFITRION_H

#include "atsw.h"
#include "analisis.h"

/* El anfitrion de esta madre: su manifiesto, su ventana, su barra de estado
 * y «abre este modelo», que aqui es el editor o fue_gui segun se pida.   */
AnHost atsw_host( Atsw *a );

#endif /* ATSW_ANFITRION_H */
