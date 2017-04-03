#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include "profile.h"
#include "utils.h"

LCFGProfile * lcfgprofile_new(void) {

  LCFGProfile * profile = malloc( sizeof(LCFGProfile) );
  if ( profile == NULL ) {
    perror( "Failed to allocate memory for LCFG profile" );
    exit(EXIT_FAILURE);
  }

  /* Packages */

  profile->active_packages   = NULL;
  profile->inactive_packages = NULL;

  /* Components */

  profile->components = NULL;

  /* metadata */

  profile->published_by       = NULL;
  profile->published_at       = NULL;
  profile->server_version     = NULL;
  profile->last_modified      = NULL;
  profile->last_modified_file = NULL;
  profile->mtime              = 0;

  return profile;
}

void lcfgprofile_destroy(LCFGProfile * profile) {

  if ( profile == NULL ) return;

  lcfgcomplist_destroy(profile->components);
  profile->components = NULL;

  lcfgpkglist_relinquish(profile->active_packages);
  profile->active_packages = NULL;

  lcfgpkglist_relinquish(profile->inactive_packages);
  profile->inactive_packages = NULL;

  free(profile->published_by);
  profile->published_by = NULL;

  free(profile->published_at);
  profile->published_at = NULL;

  free(profile->server_version);
  profile->server_version = NULL;

  free(profile->last_modified);
  profile->last_modified = NULL;

  free(profile->last_modified_file);
  profile->last_modified_file = NULL;

  free(profile);
  profile = NULL;

}

char * lcfgprofile_get_published_by( const LCFGProfile * profile ) {
  return profile->published_by;
}

char * lcfgprofile_get_published_at( const LCFGProfile * profile ) {
  return profile->published_at;
}

char * lcfgprofile_get_server_version( const LCFGProfile * profile ) {
  return profile->server_version;
}

char * lcfgprofile_get_last_modified( const LCFGProfile * profile ) {
  return profile->last_modified;
}

char * lcfgprofile_get_last_modified_file( const LCFGProfile * profile ) {
  return profile->last_modified_file;
}

time_t lcfgprofile_get_mtime( const LCFGProfile * profile ) {
  return profile->mtime;
}

bool lcfgprofile_get_meta( const LCFGProfile * profile,
                           const char * metakey,
                           char ** metavalue ) {

  const LCFGComponent * profcomp =
    lcfgprofile_find_component( profile, "profile" );

  if ( profcomp == NULL )
    return false;

  const LCFGResource * metares =
    lcfgcomponent_find_resource( profcomp, metakey );

  if ( metares == NULL )
    return false;

  *metavalue = lcfgresource_get_value(metares);

  return true;
}

char * lcfgprofile_nodename( const LCFGProfile * profile ) {

  const LCFGComponent * profcomp =
    lcfgprofile_find_component( profile, "profile" );

  if ( profcomp == NULL || lcfgcomponent_is_empty(profcomp) )
    return NULL;

  char * nodename = NULL;

  /* profile.node resource is required */

  const LCFGResource * node_res =
    lcfgcomponent_find_resource( profcomp, "node" );

  if ( node_res != NULL && lcfgresource_has_value(node_res) ) {
    const char * node = lcfgresource_get_value(node_res);

    /* profile.domain resource is optional */

    const LCFGResource * domain_res =
      lcfgcomponent_find_resource( profcomp, "domain" );

    if ( domain_res != NULL && lcfgresource_has_value(domain_res) ) {
      const char * domain = lcfgresource_get_value(domain_res);

      nodename = lcfgutils_join_strings( ".", node, domain );
    } else {
      nodename = strdup(node);
    }
  }

  return nodename;
}

/* Convenience wrappers around the component list functions */

bool lcfgprofile_has_components( const LCFGProfile * profile ) {
  return ( profile->components != NULL &&
           !lcfgcomplist_is_empty(profile->components) );
}

bool lcfgprofile_has_component( const LCFGProfile * profile,
                                const char * name ) {
  return ( profile->components != NULL &&
	   lcfgcomplist_has_component( profile->components, name ) );
}

LCFGComponent * lcfgprofile_find_component( const LCFGProfile * profile,
                                            const char * name ) {

  if ( profile->components == NULL )
    return NULL;

  return lcfgcomplist_find_component( profile->components, name );
}

LCFGComponent * lcfgprofile_find_or_create_component( LCFGProfile * profile,
                                                      const char * name ) {

  if ( profile->components == NULL )
    profile->components = lcfgcomplist_new();

  LCFGComponent * comp = NULL;
  if ( profile->components != NULL )
    comp = lcfgcomplist_find_or_create_component( profile->components, name );

  return comp;
}

LCFGChange lcfgprofile_insert_or_replace_component( LCFGProfile   * profile,
                                                    LCFGComponent * new_comp,
                                                    char ** msg ) {

  if ( profile->components == NULL )
    profile->components = lcfgcomplist_new();

  return lcfgcomplist_insert_or_replace_component( profile->components,
                                                   new_comp,
                                                   msg );
}

LCFGStatus lcfgprofile_apply_overrides( LCFGProfile * profile1,
					const LCFGProfile * profile2,
					char ** msg ) {

  if ( profile2 == NULL )
    return LCFG_STATUS_OK;

  /* Overrides are only applied to components already in target profile */

  LCFGStatus status = LCFG_STATUS_OK;
  if ( lcfgprofile_has_components(profile1) &&
       lcfgprofile_has_components(profile2) ) {
    status = lcfgcomplist_apply_overrides( profile1->components,
					   profile2->components,
					   msg );
  }

  /* default rules, only used when creating new empty lists */
  unsigned int active_merge_rules =
    LCFG_PKGS_OPT_SQUASH_IDENTICAL | LCFG_PKGS_OPT_USE_PRIORITY;
  unsigned int inactive_merge_rules =
    LCFG_PKGS_OPT_SQUASH_IDENTICAL | LCFG_PKGS_OPT_KEEP_ALL;

  /* Merge active packages lists */

  if ( profile2->active_packages != NULL ) {

    if ( profile1->active_packages == NULL ) {
      profile1->active_packages = lcfgpkglist_new();
      lcfgpkglist_set_merge_rules( profile1->active_packages,
				   active_merge_rules );
    }

    LCFGChange change = lcfgpkglist_merge_list( profile1->active_packages,
                                                profile2->active_packages,
                                                msg );

    if ( change == LCFG_CHANGE_ERROR )
      status = LCFG_STATUS_ERROR;
  }

  /* Merge inactive packages lists */

  if ( profile2->inactive_packages != NULL ) {

    if ( profile1->inactive_packages == NULL ) {
      profile1->inactive_packages = lcfgpkglist_new();
      lcfgpkglist_set_merge_rules( profile1->inactive_packages,
				   inactive_merge_rules );
    }

    LCFGChange change = lcfgpkglist_merge_list( profile1->inactive_packages,
                                                profile2->inactive_packages,
                                                msg );

    if ( change == LCFG_CHANGE_ERROR )
      status = LCFG_STATUS_ERROR;
  }

  return status;
}

LCFGStatus lcfgprofile_transplant_components( LCFGProfile * profile1,
					      const LCFGProfile * profile2,
					      char ** msg ) {

  if ( profile2 == NULL || !lcfgprofile_has_components(profile2) )
    return true;

  if ( profile1->components == NULL )
    profile1->components = lcfgcomplist_new();

  return lcfgcomplist_transplant_components( profile1->components,
                                             profile2->components,
                                             msg );

}

bool lcfgprofile_print_metadata( const LCFGProfile * profile, FILE * out ) {

  int rc = fprintf( out, "Published by: %s\nPublished at: %s\nServer version: %s\nLast modified: %s\nLast modified file: %s\n",
                    profile->published_by,
                    profile->published_at,
                    profile->server_version,
                    profile->last_modified,
                    profile->last_modified_file );

  return ( rc >= 0 );
}

LCFGChange lcfgprofile_write_rpmcfg( const LCFGProfile * profile,
                                     const char * defarch,
                                     const char * filename,
                                     const char * rpminc,
                                     char ** msg ) {

  return lcfgpkglist_to_rpmcfg( profile->active_packages,
                                profile->inactive_packages,
                                defarch,
                                filename,
                                rpminc,
                                profile->mtime,
                                msg );

}

bool lcfgprofile_print(const LCFGProfile * profile,
                       bool show_comps,
                       bool show_pkgs,
                       const char * defarch,
                       const char * comp_style,
                       FILE * out ) {

  bool ok = lcfgprofile_print_metadata( profile, out );

  if ( ok && show_comps && lcfgprofile_has_components(profile) ) {
    bool print_all_resources = false;

    if ( fputs( "\n", out ) < 0 )
      ok = false;

    if (ok) {
      ok = lcfgcomplist_print( profile->components, comp_style,
                               print_all_resources, out );
    }

  }

  if ( ok && show_pkgs && !lcfgpkglist_is_empty(profile->active_packages) ) {

    if ( fputs( "\n", out ) < 0 )
      ok = false;

    if (ok) {
      ok = lcfgpkglist_print( profile->active_packages,
                              defarch, LCFG_PKG_STYLE_DEFAULT, 0, out );
    }

  }

  return ok;
}

LCFGStatus lcfgprofile_from_status_dir( const char * status_dir,
                                        LCFGProfile ** result,
                                        const LCFGTagList * comps_wanted,
                                        char ** msg ) {

  *msg = NULL;
  *result = lcfgprofile_new();

  LCFGComponentList * components = NULL;
  LCFGStatus rc = lcfgcomplist_from_status_dir( status_dir,
                                                &components,
                                                comps_wanted,
                                                msg );

  /* It is NOT a failure if the directory does not contain any files
     thus might get an empty components list returned. */

  if ( rc == LCFG_STATUS_OK ) {
    ( *result )->components = components;

    struct stat sb;
    if ( stat( status_dir, &sb ) == 0 )
      ( *result )->mtime = sb.st_mtime;

  } else {
    lcfgprofile_destroy(*result);
    *result = NULL;
  }

  return rc;
}

LCFGStatus lcfgprofile_to_status_dir( const LCFGProfile * profile,
				      const char * status_dir,
				      char ** msg ) {

  return lcfgcomplist_to_status_dir( profile->components,
				     status_dir,
				     msg );

}

/* eof */
