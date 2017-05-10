#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xml.h"
#include "bdb.h"
#include "differences.h"

int main(int argc, char* argv[]) {
  const char * filename = argv[1];

  char * base_context    = NULL;
  char * base_derivation = NULL;
  LCFGTagList * comps_wanted = NULL; /* process ALL */
  bool require_packages  = true;
  bool apply_local       = true; /* apply local contexts and overrides */

  char * override_dir = "/var/lcfg/conf/profile/local";
  char * context_dir  = "/var/lcfg/conf/profile/context";

  LCFGStatus status = LCFG_STATUS_OK;
  char * msg = NULL;

  LCFGProfile * new_profile = NULL;
  LCFGContextList * ctxlist = NULL;

  if ( apply_local ) {
    time_t modtime;
    if ( lcfgcontext_load_active( context_dir, &ctxlist, &modtime, &msg )
	 != LCFG_STATUS_OK ) {
      status = LCFG_STATUS_ERROR;
      fprintf( stderr, "Failed to load contexts: %s\n", msg );
      goto cleanup;
    }
  }

  status = lcfgprofile_from_xml( filename, &new_profile,
                                 base_context, base_derivation, ctxlist,
                                 comps_wanted, require_packages,
                                 &msg );

  if ( status != LCFG_STATUS_OK ) {
    fprintf( stderr, "Failed to process XML profile: %s\n", msg );
    goto cleanup;
  }

  /* Option local overrides */

  if ( apply_local ) {
    char * override_msg = NULL;

    LCFGChange ctx_change = lcfgprofile_overrides_context( new_profile,
                                                           context_dir,
                                                           ctxlist,
                                                           &override_msg );

    if ( ctx_change == LCFG_CHANGE_ERROR ) {
      fprintf( stderr, "Failed to apply context overrides to profile: %s\n",
               override_msg );
    }

    LCFGChange override_change = lcfgprofile_overrides_xmldir( new_profile,
                                                               override_dir,
                                                               ctxlist,
                                                               &override_msg );

    if ( override_change == LCFG_CHANGE_ERROR ) {
      fprintf( stderr, "Failed to apply context overrides to profile: %s\n",
               override_msg );
    }

    free(override_msg);
  }


  /* Write out the results */

  /* 1. Packages */

  LCFGChange change = lcfgprofile_write_rpmcfg( new_profile,
                                                NULL, 
                                                "/tmp/rpmcfg",
                                                NULL,
                                                &msg );

  switch(change)
    {
    case LCFG_CHANGE_NONE:    
      fprintf( stderr, "rpmcfg not updated\n" );
      break;
    case LCFG_CHANGE_MODIFIED:
      fprintf( stderr, "rpmcfg updated\n" );
      break;
    default:
      status = LCFG_STATUS_ERROR;
      fprintf( stderr, "Failed to update rpmcfg: %s\n", msg );
      goto cleanup;
    }

  /* 2. Resources */

  char * dbfile = "/tmp/profile.db";

  LCFGProfile * old_profile = NULL;
  status = lcfgprofile_from_bdb( dbfile,
				 &old_profile,
				 NULL,
                                 NULL,
				 LCFG_OPT_ALLOW_NOEXIST,
				 &msg );

  if ( status != LCFG_STATUS_OK ) {
    fprintf( stderr, "Failed to read from DB file '%s': %s\n",
	     dbfile, msg );
    goto cleanup;
  }

  LCFGTagList * changed_comps = NULL;
  LCFGTagList * added_comps   = NULL;
  LCFGTagList * removed_comps = NULL;

  LCFGChange diff = lcfgprofile_quickdiff( old_profile,
                                           new_profile,
                                           &changed_comps,
                                           &added_comps,
                                           &removed_comps );

  if ( diff == LCFG_CHANGE_ERROR ) {
    status = LCFG_STATUS_ERROR;
    fprintf( stderr, "Failed to diff profiles\n" );
    goto cleanup;
  } else if ( diff != LCFG_CHANGE_NONE ) {

    status = lcfgprofile_to_bdb( new_profile, NULL, dbfile, &msg );

    if ( status != LCFG_STATUS_OK ) {
      fprintf( stderr, "Failed to write to DB file '%s': %s\n",
	       dbfile, msg );
      goto cleanup;
    }

  }

  lcfgtaglist_relinquish(changed_comps);
  lcfgtaglist_relinquish(added_comps);
  lcfgtaglist_relinquish(removed_comps);

 cleanup:

  lcfgprofile_destroy(old_profile);
  lcfgprofile_destroy(new_profile);

  lcfgctxlist_destroy(ctxlist);

  free(msg);

  return ( status == LCFG_STATUS_OK ? 0 : 1 );
}

