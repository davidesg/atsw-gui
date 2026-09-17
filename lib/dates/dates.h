/*
 * dates.h -- la aritmetica de fechas de una serie temporal.
 *
 * Una observacion es un par (periodo, subperiodo): (1996, 3) es el tercer
 * trimestre de 1996. Estas dos funciones traducen entre ese par y el indice
 * 1..nobs, y son la misma cuenta en todos los programas de la suite.
 *
 * DateToObs vivia dentro de drtran.c, con el main(), y por eso no se podia
 * usar desde otro programa sin arrastrarlo entero. Vive aqui para que el GUI
 * de drtran pueda enlazar el lector del motor -- fue_pre_reader.c -- en vez de
 * escribir el septimo lector del formato.
 *
 * ObsToDate salio del diagnose.c de drtran por la misma razon. Sigue copiada
 * en los otros cuatro motores: traerlas es la siguiente mudanza, y hay que
 * comprobar antes que las cinco son la misma cuenta.
 */

#ifndef ATSW_DATES_H
#define ATSW_DATES_H

/* (per, sub) -> numero de observacion, 1..nobs */
void DateToObs(int beg_per, int beg_sub, int per, int sub, int freq,
               int *obs_no);

/* numero de observacion 1..nobs -> (per, sub) */
void ObsToDate(int beg_per, int beg_sub, int obs_no, int freq,
               int *per, int *sub);

#endif /* ATSW_DATES_H */
