/**
 * @file profile/profile.c
 * @brief Functions for working with LCFG profiles
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date: 2017-04-26 17:35:05 +0100 (Wed, 26 Apr 2017) $
 * $Revision: 32553 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>

#include "profile.h"
#include "utils.h"

/**
 * @brief Create and initialise a new empty profile
 *
 * Creates a new @c LCFGProfile which represents an empty profile.
 *
 * If the memory allocation for the new structure is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * To avoid memory leaks, when it is no longer required the @c
 * lcfgprofile_destroy() function should be called.
 *
 * @return Pointer to new @c LCFGProfile
 *
 */

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

/**
 * @brief Destroy the profile
 *
 * When the specified @c LCFGProfile is no longer required this
 * will free all associated memory.
 *
 * This will call @c lcfgcomplist_relinquish() for the list of
 * components. It will also call @c lcfgpkglist_relinquish() for the
 * lists of active and inactive packages.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * profile which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] comp Pointer to @c LCFGProfile to be destroyed.
 *
 */

void lcfgprofile_destroy(LCFGProfile * profile) {

  if ( profile == NULL ) return;

  lcfgcomplist_relinquish(profile->components);
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
  assert( profile != NULL );

  return profile->published_by;
}

char * lcfgprofile_get_published_at( const LCFGProfile * profile ) {
  assert( profile != NULL );

  return profile->published_at;
}

char * lcfgprofile_get_server_version( const LCFGProfile * profile ) {
  assert( profile != NULL );

  return profile->server_version;
}

char * lcfgprofile_get_last_modified( const LCFGProfile * profile ) {
  assert( profile != NULL );

  return profile->last_modified;
}

char * lcfgprofile_get_last_modified_file( const LCFGProfile * profile ) {
  assert( profile != NULL );

  return profile->last_modified_file;
}

time_t lcfgprofile_get_mtime( const LCFGProfile * profile ) {
  assert( profile != NULL );

  return profile->mtime;
}

bool lcfgprofile_get_meta( const LCFGProfile * profile,
                           const char * metakey,
                           char ** metavalue ) {
  assert( metakey != NULL );

  const LCFGComponent * profcomp =
    lcfgprofile_find_component( profile, "profile" );

  if ( profcomp == NULL ) return false;

  const LCFGResource * metares =
    lcfgcomponent_find_resource( profcomp, metakey );

  if ( metares == NULL ) return false;

  *metavalue = lcfgresource_get_value(metares);

  return true;
}

char * lcfgprofile_nodename( const LCFGProfile * profile ) {

  const LCFGComponent * profcomp =
    lcfgprofile_find_component( profile, "profile" );

  if ( lcfgcomponent_is_empty(profcomp) ) return NULL;

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

      nodename = lcfgutils_string_join( ".", node, domain );
    } else {
      nodename = strdup(node);
    }
  }

  return nodename;
}

/* Convenience wrappers around the component list functions */

bool lcfgprofile_has_components( const LCFGProfile * profile ) {
  return !lcfgcomplist_is_empty(profile->components);
}

bool lcfgprofile_has_component( const LCFGProfile * profile,
                                const char * name ) {
  assert( name != NULL );

  return lcfgcomplist_has_component( profile->components, name );
}

LCFGComponent * lcfgprofile_find_component( const LCFGProfile * profile,
                                            const char * name ) {
  assert( name != NULL );

  return lcfgcomplist_find_component( profile->components, name );
}

LCFGComponent * lcfgprofile_find_or_create_component( LCFGProfile * profile,
                                                      const char * name ) {
  assert( profile != NULL );
  assert( name != NULL );

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
  assert( profile != NULL );
  assert( new_comp != NULL );

  if ( profile->components == NULL )
    profile->components = lcfgcomplist_new();

  return lcfgcomplist_insert_or_replace_component( profile->components,
                                                   new_comp,
                                                   msg );
}

LCFGStatus lcfgprofile_merge( LCFGProfile * profile1,
			      const LCFGProfile * profile2,
			      bool take_new_comps,
			      char ** msg ) {
  assert( profile1 != NULL );

  if ( profile2 != NULL ) return LCFG_STATUS_OK;

  /* Overrides are only applied to components already in target profile */

  LCFGStatus status = LCFG_STATUS_OK;
  if ( lcfgprofile_has_components(profile2) &&
       ( lcfgprofile_has_components(profile1) || take_new_comps ) ) {

    /* Create complist for profile1 if necessary */
    if ( profile1->components == NULL && take_new_comps )
      profile1->components = lcfgcomplist_new();

    LCFGChange change = lcfgcomplist_merge( profile1->components,
					    profile2->components,
					    take_new_comps,
					    msg );

    if ( change == LCFG_CHANGE_ERROR )
      status = LCFG_STATUS_ERROR;
  }

  /* default rules, only used when creating new empty lists */
  LCFGPkgRule active_merge_rules =
    LCFG_PKG_RULE_SQUASH_IDENTICAL | LCFG_PKG_RULE_USE_PRIORITY;
  LCFGPkgRule inactive_merge_rules =
    LCFG_PKG_RULE_SQUASH_IDENTICAL | LCFG_PKG_RULE_KEEP_ALL;

  /* Merge active packages lists */

  if ( !lcfgpkglist_is_empty(profile2->active_packages) ) {

    /* Create active package list for profile1 if necessary */
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

  if ( !lcfgpkglist_is_empty(profile2->inactive_packages) ) {

    /* Create inactive package list for profile1 if necessary */
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

LCFGChange lcfgprofile_transplant_components( LCFGProfile * profile1,
					      const LCFGProfile * profile2,
					      char ** msg ) {
  assert( profile1 != NULL );

  if ( !lcfgprofile_has_components(profile2) ) return LCFG_CHANGE_NONE;

  if ( profile1->components == NULL )
    profile1->components = lcfgcomplist_new();

  return lcfgcomplist_transplant_components( profile1->components,
                                             profile2->components,
                                             msg );

}

bool lcfgprofile_print_metadata( const LCFGProfile * profile, FILE * out ) {
  assert( profile != NULL );

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
  assert( profile != NULL );
  assert( filename != NULL );

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
                       LCFGResourceStyle comp_style,
                       FILE * out ) {
  assert( profile != NULL );

  bool ok = lcfgprofile_print_metadata( profile, out );

  if ( ok && show_comps && lcfgprofile_has_components(profile) ) {

    if ( fputs( "\n", out ) < 0 )
      ok = false;

    if (ok) {
      ok = lcfgcomplist_print( profile->components, comp_style,
                               LCFG_OPT_USE_META, out );
    }

  }

  if ( ok && show_pkgs && !lcfgpkglist_is_empty(profile->active_packages) ) {

    if ( fputs( "\n", out ) < 0 )
      ok = false;

    if (ok) {
      ok = lcfgpkglist_print( profile->active_packages,
                              defarch, LCFG_PKG_STYLE_SPEC,
			      LCFG_OPT_NONE, out );
    }

  }

  return ok;
}

LCFGStatus lcfgprofile_from_status_dir( const char * status_dir,
                                        LCFGProfile ** result,
                                        const LCFGTagList * comps_wanted,
                                        char ** msg ) {
  assert( status_dir != NULL );

  LCFGProfile * new_profile = NULL;

  LCFGComponentList * components = NULL;
  LCFGStatus rc = lcfgcomplist_from_status_dir( status_dir,
                                                &components,
                                                comps_wanted,
						LCFG_OPT_NONE,
                                                msg );

  /* It is NOT a failure if the directory does not contain any files
     thus might get an empty components list returned. */

  if ( rc != LCFG_STATUS_ERROR ) {
    new_profile = lcfgprofile_new();

    lcfgcomplist_acquire(components);
    new_profile->components = components;

    struct stat sb;
    if ( stat( status_dir, &sb ) == 0 )
      ( *result )->mtime = sb.st_mtime;

  }

  lcfgcomplist_relinquish(components);

  *result = new_profile;

  return rc;
}

LCFGStatus lcfgprofile_to_status_dir( const LCFGProfile * profile,
				      const char * status_dir,
				      char ** msg ) {
  assert( profile != NULL );
  assert( status_dir != NULL );

  return lcfgcomplist_to_status_dir( profile->components,
				     status_dir,
				     LCFG_OPT_NONE,
				     msg );

}

/* eof */
