#include <stdio.h>
#include <stdlib.h>

#include <lcfg/profile.h>
#include <lcfg/bdb.h>

int main(int argc, char * argv[] ) {

  const char * dbfile    = argv[1];
  const char * node_name = argv[2];
  const char * comp_name = argv[3];
  const char * res_name  = argv[4];

  printf( "%s == %s == %s\n", dbfile, comp_name, res_name );

  LCFGTagList * comps_wanted = lcfgtaglist_new();
  char * msg = NULL;
  lcfgtaglist_mutate_append( comps_wanted, comp_name, &msg );
  free(msg);
  msg = NULL;

  lcfgtaglist_print( comps_wanted, stdout );

  LCFGProfile * profile = NULL;
  LCFGStatus rc = lcfgprofile_from_bdb( dbfile, &profile, comps_wanted,
                                        node_name, 0, &msg );

  if ( rc == LCFG_STATUS_ERROR ) {
    fprintf( stderr, "Failed to read db: %s\n", msg );
  } else {

    const LCFGComponent * component =
      lcfgprofile_find_component( profile, comp_name );
    printf("comp name: '%s'\n", lcfgcomponent_get_name(component) );

    const LCFGResource * resource =
      lcfgcomponent_find_resource( component, res_name, false );
    printf("res name: '%s'\n", lcfgresource_get_name(resource) );
    printf ("has value: %s\n", lcfgresource_has_value(resource) ? "yes" : "no" );
    lcfgresource_print(resource, comp_name,
                       LCFG_RESOURCE_STYLE_SUMMARY, LCFG_OPT_USE_META, stdout );
  }

  free(msg);
  msg = NULL;

  lcfgtaglist_relinquish(comps_wanted);
  lcfgprofile_destroy(profile);

  return 0;
}
