#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <lcfg/components.h>

int main(int argc, char * argv[] ) {

  const char * filename = argv[1];

  LCFGComponent * comp = NULL;
  char * msg = NULL;

  LCFGStatus rc = lcfgcomponent_from_statusfile( filename, &comp, NULL,
						 LCFG_OPT_NONE, &msg );

  bool ok = true;
  if ( rc == LCFG_STATUS_ERROR ) {
    ok = false;
    fprintf( stderr, "Error: %s\n", msg );
  } else {
    lcfgcomponent_sort(comp);
    ok = lcfgcomponent_print( comp, LCFG_RESOURCE_STYLE_SPEC, 0, stdout );
  }

  lcfgcomponent_relinquish(comp);

  free(msg);

  return ( ok ? 0 : 1 );
}


