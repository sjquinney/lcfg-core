#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <lcfg/components.h>

int main(int argc, char * argv[] ) {

  const char * dirname = argv[1];

  LCFGComponentList * complist = NULL;
  char * msg = NULL;

  LCFGStatus rc = lcfgcomplist_from_status_dir( dirname, &complist, NULL, &msg );

  bool ok = true;
  if ( rc == LCFG_STATUS_ERROR ) {
    ok = false;
    fprintf( stderr, "Error: %s\n", msg );
  } else {
    if ( argc > 2 ) {
      int i;
      for ( i=2;i<argc;i++ ) {
        LCFGComponent * comp = lcfgcomplist_find_component( complist, argv[i] );
        if ( comp != NULL ) {
          lcfgcomponent_sort(comp);
          if ( !lcfgcomponent_print( comp, LCFG_RESOURCE_STYLE_SPEC, 0, stdout ) ) {
            ok = false;
            break;
          }
        }
      }

    } else {
      ok = lcfgcomplist_print( complist, LCFG_RESOURCE_STYLE_SPEC, 0, stdout );
    }
  }

  lcfgcomplist_destroy(complist);

  free(msg);

  return ( ok ? 0 : 1 );
}


