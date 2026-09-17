/*
 * dates.c -- ver dates.h.
 */

#include <stdlib.h>   /* div_t, div() */
#include "dates.h"

void DateToObs(int beg_per, int beg_sub, int per, int sub, int freq,
               int *obs_no)
{
    int srest, pcad, sad;

    srest = freq - beg_sub + 1;
    if (sub == freq) {
        pcad = per - beg_per;
        *obs_no = srest + freq * pcad;
    } else {
        pcad = per - beg_per - 1;
        sad  = sub;
        *obs_no = srest + freq * pcad + sad;
    }
}

void ObsToDate(int beg_per, int beg_sub, int obs_no, int freq,
                       int *per, int *sub) {
    div_t cad;
    if (obs_no + beg_sub - 1 <= freq) {
        *per = beg_per;
        *sub = beg_sub + obs_no - 1;
    } else {
        cad = div(obs_no - (freq - beg_sub + 1), freq);
        if (cad.rem > 0) {
            *per = beg_per + cad.quot + 1;
            *sub = cad.rem;
        } else {
            *per = beg_per + cad.quot;
            *sub = freq;
        }
    }
}
