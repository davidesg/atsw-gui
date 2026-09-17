/***************************************************************************
 *   Copyright (C) 2008 by David Guerrero                                  *
 *   warrior@warrior.ccee.ucm.es                                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "delop.h"
#include "fug.h"

void DelOp( int sp, int d, int ds, int *ifds, int ord, double *op )

{
   double *pol1, *pol2, *pol3, *pol4;
   int  i, j, k, pp, pp1, pp2;

/*****************************************************************************/
/* [1]: Multiply regular by (complete) annual differences (pol1[pp1]):       */
/*****************************************************************************/

   pp1   = d + ds * sp;
   pol1  = vector( 0, pp1 );
   pol2  = vector( 0, pp1 );
   for ( i = 1; i <= pp1; i++ ) pol1[i] = 0.0;
   pol1[0] = -1.0;
   pp      =    0;

   if ( ds > 0 )
      for ( k = 1; k <= ds; k++ )
          {
          for ( i = 0; i <= pp1; i++ ) pol2[i] = 0.0;
          for ( j = 0; j <= pp + sp; j++ )
              if ( (j >= 0) && (j < sp) )
                 pol2[j] = pol1[j];
              else if ( (j >= sp) && (j <= pp) )
                 pol2[j] = pol1[j] - pol1[j-sp];
              else if ( (j > pp) && (j <= pp + sp) )
                 pol2[j] = -pol1[j-sp];
          pp += sp;
          for ( i = 0; i <= pp; i++ ) pol1[i] = pol2[i];
          }
   if ( d > 0 )
      for ( k = 1; k <= d; k++ )
          {
          for ( i = 0; i <= pp1; i++ ) pol2[i] = 0.0;
          for ( j = 0; j <= pp + 1; j++ )
              if ( (j >= 0) && (j < 1) )
                 pol2[j] = pol1[j];
              else if ( (j >= 1) && (j <= pp) )
                 pol2[j] = pol1[j] - pol1[j-1];
              else if ( (j > pp) && (j <= pp + 1) )
                 pol2[j] = -pol1[j-1];
          pp += 1;
          for ( i = 0; i <= pp; i++ ) pol1[i] = pol2[i];
          }

   free_vector( pol2, 0, pp1 );

/*****************************************************************************/
/* [2]: Multiply individual factors of the annual difference (pol2[pp2]):    */
/*****************************************************************************/

   pp2  = ord - pp1;
   pol2 = vector( 0, pp2 );
   pol3 = vector( 0, pp2 );
   pol4 = vector( 0, 2 );
   for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
   pol3[0] = -1.0;
   pp      =    0;

   if (((sp == 12) && (ifds[0] == 1)) || ((sp == 4) && (ifds[0] == 1)))
      {
      pol4[0] = -1.0;
      pol4[1] =  1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 1; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 1;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }
   if ( (sp == 12) && (ifds[1] == 1) )
      {
      pol4[0] = -1.0;
      pol4[1] = sqrt( 3.0 );
      pol4[2] = -1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 2; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 2;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }
   if ( (sp == 12) && (ifds[2] == 1) )
      {
      pol4[0] = -1.0;
      pol4[1] =  1.0;
      pol4[2] = -1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 2; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 2;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }
   if (((sp == 12) && (ifds[3] == 1)) || ((sp == 4) && (ifds[1] == 1)))
      {
      pol4[0] = -1.0;
      pol4[1] =  0.0;
      pol4[2] = -1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 2; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 2;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }
   if ( (sp == 12) && (ifds[4] == 1) )
      {
      pol4[0] = -1.0;
      pol4[1] = -1.0;
      pol4[2] = -1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 2; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 2;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }
   if ( (sp == 12) && (ifds[5] == 1) )
      {
      pol4[0] = -1.0;
      pol4[1] = -sqrt( 3.0 );
      pol4[2] = -1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 2; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 2;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }
   if (((sp == 12) && (ifds[6] == 1)) || ((sp == 4) && (ifds[2] == 1)))
      {
      pol4[0] = -1.0;
      pol4[1] = -1.0;
      for ( i = 1; i <= pp2; i++ ) pol2[i] = 0.0;
      pol2[0] = -1.0;
      pol3[0] = -1.0;
      for ( i = 0; i <= pp; i++ )
          for ( j = 0; j <= 1; j++ )
              pol2[j+i] -= pol4[j] * pol3[i];
      pp += 1;
      for ( i = 1; i <= pp; i++ ) pol3[i] = pol2[i];
      }

   for ( i = 0; i <= pp; i++ ) pol2[i] = pol3[i];

   free_vector( pol4, 0, 2 );
   free_vector( pol3, 0, pp2 );

/*****************************************************************************/
/* [3]: Multiply pol1 by pol 2 and return non-stationary factors as op:      */
/*****************************************************************************/
/*
   fprintf( outputv, "\n" );
   fprintf( outputv, "Order of the differencing operator: %d\n", pp1 );
   for ( j = 0; j <= pp1; j++ )
       fprintf( outputv, "u1[%2d]: %5.2f\n", j, pol1[j] );
   fprintf( outputv, "\n" );
   fprintf( outputv, "Order of the annual diffs operator: %d\n", pp2 );
   for ( j = 0; j <= pp2; j++ )
       fprintf( outputv, "u2[%2d]: %5.2f\n", j, pol2[j] );
*/
   for ( i = 1; i <= ord; i++ ) op[i] = 0.0;
   op[0] = -1.0;
   for ( i = 0; i <= pp1; i++ )
       for ( j = 0; j <= pp2; j++ ) op[j+i] -= pol2[j] * pol1[i];

   free_vector( pol2, 0, pp2 );
   free_vector( pol1, 0, pp1 );
}

/*****************************************************************************/
