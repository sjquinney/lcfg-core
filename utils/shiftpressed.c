/*************************************************************************

  Exit status 0 if shift key pressed

*************************************************************************/

#include <stdio.h>
#include "utils.h"

/************************************************************************/
  int main ( int argc, char *argv[] )
/************************************************************************/
{
  int shift = LCFG_ShiftPressed();
  return ( (shift==1) ? 0 : (shift==-1) ? 1 : 2 );
}
