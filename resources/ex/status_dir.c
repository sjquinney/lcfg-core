#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <lcfg/components.h>

int main(int argc, char * argv[] ) {

  const char * dirname = argv[1];

  LCFGComponentSet * compset = NULL;
  char * msg = NULL;

  LCFGStatus rc = lcfgcompset_from_status_dir( dirname, &compset, NULL, 0, &msg );

  bool ok = true;
  if ( rc == LCFG_STATUS_ERROR ) {
    ok = false;
    fprintf( stderr, "Error: %s\n", msg );
  } else {
    if ( argc > 2 ) {
      int i;
      for ( i=2;i<argc;i++ ) {
        LCFGComponent * comp = lcfgcompset_find_component( compset, argv[i] );
        if ( comp != NULL ) {
          lcfgcomponent_sort(comp);
          if ( !lcfgcomponent_print( comp, LCFG_RESOURCE_STYLE_SPEC, 0, stdout ) ) {
            ok = false;
            break;
          }
        }
      }

    } else {
      ok = lcfgcompset_print( compset, LCFG_RESOURCE_STYLE_SPEC, 0, stdout );
    }
  }

  char * names = 
    lcfgcompset_get_components_as_string(compset);
  printf("%s\n", names );
  free(names);

  lcfgcompset_relinquish(compset);

  free(msg);

  return ( ok ? 0 : 1 );
}


