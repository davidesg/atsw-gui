/*
 * ofgraf.h -- el grafico de la prevision de fuf (lib/fufplot) a partir de la
 * tabla del .out (lib/outfcst): la de drtran y la de la escalera de drvarma,
 * que es la misma tabla, la de fuf.
 *
 * Lo usan drtran_gui y sim_gui: una sola conversion de la tabla al dibujo,
 * para que los dos dibujen lo mismo que fue.
 */
#ifndef ATSW_OFGRAF_H
#define ATSW_OFGRAF_H

#include "outfcst.h"
#include "fugdraw.h"

/* Arriba, la variacion anual (la LRC anual en %, con lambda = 0) de las L
 * ultimas observaciones y las L previsiones, con la banda de UNA desviacion;
 * con lambda < 0, el NIVEL con la de dos, deshaciendo la transformacion
 * (como forecast_graphic_BC de fuf). Debajo, ERR, con su banda de dos sigma:
 * sigma es la desviacion de la prevision a un periodo, que es la del error a
 * un periodo de la tabla. tiene_lam: si se sabe el lambda de la serie (si
 * no, el titulo es «Annual change»). NULL si la tabla no da para el dibujo. */
FDFig *of_grafico( const OfSerie *S, double lam, int tiene_lam, int freq );

#endif
