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
 * @param[in] profile Pointer to @c LCFGProfile to be destroyed.
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

/**
 * @brief Get the name of the profile publisher
 *
 * This is typically the name of the server which generated the
 * profile. Note that this is typically only set when the profile has
 * been loaded from an XML file.
 *
 * @param[in] profile Pointer to @c LCFGProfile.
 *
 * @return Pointer to string holding name of publisher
 *
 */

char * lcfgprofile_get_published_by( const LCFGProfile * profile ) {
  assert( profile != NULL );

  return profile->published_by;
}

/**
 * @brief Get the time at which the profile was published
 *
 * This is a string which holds the timestamp (date and time) at which
 * the profile was generated. Note that this is typically only set
 * when the profile has been loaded from an XML file.
 *
 * @param[in] profile Pointer to @c LCFGProfile.
 *
 * @return Pointer to string holding timestamp for profile
 *
 */

char * lcfgprofile_get_published_at( const LCFGProfile * profile ) {
  assert( profile != NULL );

  return profile->published_at;
}

/**
 * @brief Get the version of the server which published the profile
 *
 * This is a string which holds the version string for the LCFG server
 * which generated the profile. Note that this is typically only set
 * when the profile has been loaded from an XML file.
 *
 * @param[in] profile Pointer to @c LCFGProfile.
 *
 * @return Pointer to string holding server version
 *
 */

char * lcfgprofile_get_server_version( const LCFGProfile * profile ) {
  assert( profile != NULL );

  return profile->server_version;
}

/**
 * @brief Get the latest modification time for the profile sources
 *
 * This is a string which holds the timestamp (date and time) of
 * modification for the most recently modified source file. The name
 * of the file is accessible using the @c
 * lcfgprofile_get_last_modified_file() function. Note that this is
 * typically only set when the profile has been loaded from an XML
 * file.
 *
 * @param[in] profile Pointer to @c LCFGProfile.
 *
 * @return Pointer to string holding timestamp of most recently modified source
 *
 */

char * lcfgprofile_get_last_modified( const LCFGProfile * profile ) {
  assert( profile != NULL );

  return profile->last_modified;
}

/**
 * @brief Get the latest modified file for the profile sources
 *
 * This is a string which holds the name of the most recently modified
 * source file for the profile. The name of the file is accessible
 * using the @c lcfgprofile_get_last_modified_file() function. Note
 * that this is typically only set when the profile has been loaded
 * from an XML file.
 *
 * @param[in] profile Pointer to @c LCFGProfile.
 *
 * @return Pointer to string holding name of most recently modified source
 *
 */

char * lcfgprofile_get_last_modified_file( const LCFGProfile * profile ) {
  assert( profile != NULL );

  return profile->last_modified_file;
}

/**
 * @brief Get the modification time of the profile
 *
 * This returns the modification time of the input file from which the
 * profile was loaded (e.g. XML file, Berkeley DB, status directory)
 * as an integer number of seconds since the epoch (see @c ctime(3)
 * manual page). If the @c lcfgprofile_get_last_modified() function
 * returns a value it will typically match with this value but that is
 * not guaranteed.
 *
 * @param[in] profile Pointer to @c LCFGProfile.
 *
 * @return Integer Modification time of profile (seconds since epoch)
 *
 */

time_t lcfgprofile_get_mtime( const LCFGProfile * profile ) {
  assert( profile != NULL );

  return profile->mtime;
}

/**
 * @brief Get the value for a profile meta-data key
 *
 * This can be used to fetch the value for a profile
 * resource. Slightly confusingly an LCFG profile typically contains a
 * component named "profile" which holds various meta-data resources
 * (e.g. the node name and domain name). This function provides a
 * convenient way to fetch the value for a resource in this component.
 *
 * Profiles loaded from an XML file or a Berkeley DB will have access
 * to the profile component resources but it will not usually be
 * available when loading from a status directory.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the value for the
 * resource.
 *
 * @param[in] profile Pointer to @c LCFGProfile.
 * @param[in] metakey Meta-data key name
 * @param[out] metavalue Reference to pointer to meta-data value
 *
 * @return Boolean which indicates success of fetch
 *
 */

bool lcfgprofile_get_meta( const LCFGProfile * profile,
                           const char * metakey,
                           char ** metavalue ) {
  assert( metakey != NULL );

  const LCFGComponent * profcomp =
    lcfgprofile_find_component( profile, "profile" );

  if ( profcomp == NULL ) return false;

  const LCFGResource * metares =
    lcfgcomponent_find_resource( profcomp, metakey, false );

  if ( metares == NULL ) return false;

  *metavalue = lcfgresource_get_value(metares);

  return true;
}

/**
 * @brief Get the nodename for the profile
 *
 * This can be used to fetch the nodename for the profile. This will
 * only work if the profile contains a "profile" component with a
 * value for the "node" resource. If the "domain" resource also has a
 * value then this function will return a fully-qualified node name by
 * concatenating the two strings with a @c '.' (period) separator.
 *
 * If successful this returns a new string which should be freed when
 * no longer required. If the necessary "profile" component is not
 * accessible or the resources are missing this will return a @c NULL
 * value.
 *
 * @param[in] profile Pointer to @c LCFGProfile.
 *
 * @return Pointer to new nodename string (call @c free() when no longer required)
 *
 */

char * lcfgprofile_nodename( const LCFGProfile * profile ) {

  const LCFGComponent * profcomp =
    lcfgprofile_find_component( profile, "profile" );

  if ( lcfgcomponent_is_empty(profcomp) ) return NULL;

  char * nodename = NULL;

  /* profile.node resource is required */

  const LCFGResource * node_res =
    lcfgcomponent_find_resource( profcomp, "node", false );

  if ( node_res != NULL && lcfgresource_has_value(node_res) ) {
    const char * node = lcfgresource_get_value(node_res);

    /* profile.domain resource is optional */

    const LCFGResource * domain_res =
      lcfgcomponent_find_resource( profcomp, "domain", false );

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

/**
 * @brief Check if the profile has any components
 *
 * This can be used to check if the @c LCFGProfile has any components.
 *
 * @param[in] profile Pointer to @c LCFGProfile
 *
 * @return Boolean which indicates whether the profile contains any components
 *
 */

bool lcfgprofile_has_components( const LCFGProfile * profile ) {
  return ( profile != NULL && !lcfgcomplist_is_empty(profile->components) );
}

/**
 * @brief Get the list of components for the profile
 *
 * This can be used to retrieve the @c LCFGComponentList for the @c
 * LCFGProfile.
 *
 * @param[in] profile Pointer to @c LCFGProfile
 *
 * @return The @c LCFGComponentList for the profile
 *
 */

LCFGComponentList * lcfgprofile_get_components( const LCFGProfile * profile ) {
  assert( profile != NULL );

  return profile->components;
}


/**
 * @brief Check if profile contains a particular component
 *
 * This can be used to search through the @c LCFGComponentList for the
 * @c LCFGProfile to check if it contains a component with a matching
 * name. Note that the matching is done using strcmp(3) which is
 * case-sensitive.
 *
 * @param[in] profile Pointer to @c LCFGProfile to be searched
 * @param[in] name The name of the required component
 *
 * @return Boolean value which indicates presence of component in list
 *
 */

bool lcfgprofile_has_component( const LCFGProfile * profile,
                                const char * name ) {
  assert( name != NULL );

  return ( lcfgprofile_has_components(profile) &&
           lcfgcomplist_has_component( profile->components, name ) );
}

/**
 * @brief Find the component for a given name
 *
 * This can be used to search through the @c LCFGComponentList for the
 * @c LCFGProfile to find the first component which has a matching
 * name. Note that the matching is done using strcmp(3) which is
 * case-sensitive.
 *
 * To ensure the returned @c LCFGComponent is not destroyed when
 * the parent @c LCFGComponentList is destroyed you would need to
 * call the @c lcfgcomponent_acquire() function.
 *
 * @param[in] profile Pointer to @c LCFGProfile to be searched
 * @param[in] name The name of the required component
 *
 * @return Pointer to an @c LCFGComponent (or the @c NULL value).
 *
 */

LCFGComponent * lcfgprofile_find_component( const LCFGProfile * profile,
                                            const char * name ) {
  assert( name != NULL );

  LCFGComponent * comp = NULL;
  if ( profile != NULL )
    comp = lcfgcomplist_find_component( profile->components, name );

  return comp;
}

/**
 * @brief Find or create a new component
 *
 * Searches the @c LCFGComponentList for the @c LCFGProfile to find an
 * @c LCFGComponent with the required name. If none is found then a
 * new @c LCFGComponent is created and added to the @c LCFGComponentList.
 *
 * If the @c LCFGProfile does not already have an @c LCFGComponentList
 * an empty list will be created.
 *
 * If an error occurs during the creation of a new component a @c NULL
 * value will be returned.
 *
 * @param[in] profile Pointer to @c LCFGComponentList
 * @param[in] name The name of the required component
 *
 * @return The required @c LCFGComponent (or @c NULL)
 *
 */

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

/**
 * @brief Insert or replace a component
 *
 * Searches the @c LCFGComponentList for the @c LCFGProfile to find a
 * matching @c LCFGComponent with the same name. If none is found the
 * component is simply added and @c LCFG_CHANGE_ADDED is returned. If
 * there is a match then the new component will replace the current
 * and @c LCFG_CHANGE_REPLACED is returned.
 *
 * If the @c LCFGProfile does not already have an @c LCFGComponentList
 * an empty list will be created.
 *
 * @param[in] profile Pointer to @c LCFGComponentList
 * @param[in] new_comp Pointer to @c LCFGComponent
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

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

/**
 * @brief Merge lists of components and packages for profiles
 *
 * This will @e merge the components and packages from the second
 * profile into the first. This is done using the @c
 * lcfgcomplist_merge_components() and @c lcfgpkglist_merge_list() functions.
 *
 * If a component from the second profile does NOT exist in the first
 * then it will only be added when the @c take_new_comps parameter is
 * set to true. When the @c take_new_comps parameter is false this is
 * effectively an "override" mode which only changes existing
 * components.
 *
 * @param[in] profile1 Pointer to @c LCFGProfile to be merged to
 * @param[in] profile2 Pointer to @c LCFGProfile to be merged from
 * @param[in] take_new_comps Boolean which controls whether components are added
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgprofile_merge( LCFGProfile * profile1,
			      const LCFGProfile * profile2,
			      bool take_new_comps,
			      char ** msg ) {
  assert( profile1 != NULL );

  if ( profile2 != NULL ) return LCFG_CHANGE_NONE;

  /* Overrides are only applied to components already in target profile */

  LCFGChange change = LCFG_CHANGE_NONE;

  if ( lcfgprofile_has_components(profile2) &&
       ( lcfgprofile_has_components(profile1) || take_new_comps ) ) {

    /* Create complist for profile1 if necessary */
    if ( profile1->components == NULL && take_new_comps )
      profile1->components = lcfgcomplist_new();

    LCFGChange merge_rc = lcfgcomplist_merge_components( profile1->components,
                                                         profile2->components,
                                                         take_new_comps,
                                                         msg );

    if ( merge_rc == LCFG_CHANGE_ERROR ) {
      change = LCFG_CHANGE_ERROR;
    } else if ( merge_rc != LCFG_CHANGE_NONE ) {
      change = merge_rc;
    }
  }

  if ( change == LCFG_CHANGE_ERROR ) return change;

  /* default rules, only used when creating new empty lists */
  LCFGMergeRule active_merge_rules =
    LCFG_MERGE_RULE_SQUASH_IDENTICAL | LCFG_MERGE_RULE_USE_PRIORITY;
  LCFGMergeRule inactive_merge_rules =
    LCFG_MERGE_RULE_SQUASH_IDENTICAL | LCFG_MERGE_RULE_KEEP_ALL;

  /* Merge active packages lists */

  if ( !lcfgpkglist_is_empty(profile2->active_packages) ) {

    /* Create active package list for profile1 if necessary */
    if ( profile1->active_packages == NULL ) {
      profile1->active_packages = lcfgpkglist_new();
      lcfgpkglist_set_merge_rules( profile1->active_packages,
				   active_merge_rules );
    }

    LCFGChange merge_rc = lcfgpkglist_merge_list( profile1->active_packages,
                                                  profile2->active_packages,
                                                  msg );

    if ( merge_rc == LCFG_CHANGE_ERROR ) {
      change = LCFG_CHANGE_ERROR;
    } else if ( merge_rc != LCFG_CHANGE_NONE ) {
      change = merge_rc;
    }
  }

  if ( change == LCFG_CHANGE_ERROR ) return change;

  /* Merge inactive packages lists */

  if ( !lcfgpkglist_is_empty(profile2->inactive_packages) ) {

    /* Create inactive package list for profile1 if necessary */
    if ( profile1->inactive_packages == NULL ) {
      profile1->inactive_packages = lcfgpkglist_new();
      lcfgpkglist_set_merge_rules( profile1->inactive_packages,
				   inactive_merge_rules );
    }

    LCFGChange merge_rc = lcfgpkglist_merge_list( profile1->inactive_packages,
                                                  profile2->inactive_packages,
                                                  msg );

    if ( merge_rc == LCFG_CHANGE_ERROR ) {
      change = LCFG_CHANGE_ERROR;
    } else if ( merge_rc != LCFG_CHANGE_NONE ) {
      change = merge_rc;
    }
  }

  return change;
}

/**
 * @brief Copy components from one profile to another
 *
 * This will copy all the components in the second profile into the
 * first. If the component already exists in the target profile it
 * will be replaced if not the component is simply added. This is done
 * using the @c lcfgcomplist_transplant_components() function.
 *
 * If the @c LCFGProfile does not already have an @c LCFGComponentList
 * an empty list will be created.
 *
 * @param[in] profile1 Pointer to @c LCFGProfile to be copied to
 * @param[in] profile2 Pointer to @c LCFGProfile to be copied from
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

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

/**
 * @brief Write summary of profile metadata to file stream
 *
 * Writes out a summary of the "Published by", "Published at", "Server
 * version", "Last modified" and "Last modified file" information for
 * an @c LCFGProfile.
 *
 * @param[in] profile Pointer to @c LCFGProfile
 * @param[in] out Stream to which the list of resources should be written
 * @return Boolean indicating success
 *
 */

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

/**
 * @brief Write packages list to an rpmcfg file
 *
 * This can be used to create an rpmcfg file which is used as input by
 * the updaterpms tool. The file is intended to be passed through the
 * C Preprocessor (cpp). The file is generated using the @c
 * lcfgpkglist_to_rpmcfg() function.
 *
 * @param[in] profile Pointer to @c LCFGProfile
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] filename Path of file to be created
 * @param[in] rpminc Extra file to be included by cpp
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change for file
 *
 */

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

/**
 * @brief Write out profile to file stream
 *
 * This can be used to write out an entire @c LCFGProfile to the
 * specified file stream.
 *
 * @param[in] profile Pointer to @c LCFGProfile
 * @param[in] show_comps Boolean to control whether components are printed
 * @param[in] show_pkgs Boolean to control where packages are printed
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] comp_style Integer indicating required style of resource formatting
 * @param[in] pkg_style Integer indicating required style of package formatting
 * @param[in] out  Stream to which the @c LCFGProfile should be written
 *
 * @return Boolean indicating success
 *
 */


bool lcfgprofile_print(const LCFGProfile * profile,
                       bool show_comps,
                       bool show_pkgs,
                       const char * defarch,
                       LCFGResourceStyle comp_style,
                       LCFGPkgStyle pkg_style,
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
                              defarch, pkg_style,
			      LCFG_OPT_NONE, out );
    }

  }

  return ok;
}

/**
 * @brief Load profile from a status directory
 *
 * This creates a new @c LCFGProfile and loads the data for the
 * components from the specified directory using the @c
 * lcfgcomplist_from_status_dir() function.
 *
 * It is expected that the file names will be valid component names,
 * any files with invalid names will simply be ignored. Empty files
 * will also be ignored.
 *
 * To limit which components are loaded a set of required names can be
 * specified as a @c LCFGTagList. If the list is empty or a @c NULL
 * value is passed in all components will be loaded.
 *
 * If the status directory does not exist an error will be returned
 * unless the @c LCFG_ALLOW_NOEXIST option is specified, in that case
 * an empty @c LCFGComponentList will be returned.

 * @param[in] status_dir Path to directory of status files
 * @param[out] result Reference to pointer for new @c LCFGProfile
 * @param[in] comps_wanted List of names for required components
 * @param[in] options Controls the behaviour of the process
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgprofile_from_status_dir( const char * status_dir,
                                        LCFGProfile ** result,
                                        const LCFGTagList * comps_wanted,
                                        LCFGOption options,
                                        char ** msg ) {
  assert( status_dir != NULL );

  LCFGProfile * new_profile = NULL;

  LCFGComponentList * components = NULL;
  LCFGStatus rc = lcfgcomplist_from_status_dir( status_dir,
                                                &components,
                                                comps_wanted,
						options,
                                                msg );

  /* It is NOT a failure if the directory does not contain any files
     thus might get an empty components list returned. */

  if ( rc != LCFG_STATUS_ERROR ) {
    new_profile = lcfgprofile_new();

    lcfgcomplist_acquire(components);
    new_profile->components = components;

    struct stat sb;
    if ( stat( status_dir, &sb ) == 0 )
      new_profile->mtime = sb.st_mtime;

  }

  lcfgcomplist_relinquish(components);

  *result = new_profile;

  return rc;
}

/**
 * @brief Write out status files for all components in profile
 *
 * For each @c LCFGComponent in the @c LCFGProfile this will call the
 * @c lcfgcomponent_to_status_file() function to write out the
 * resource state as a status file. Any options specified will be
 * passed on to that function.
 *
 * @param[in] profile Pointer to @c LCFGProfile (may be @c NULL)
 * @param[in] status_dir Path to directory for status files
 * @param[in] options Controls the behaviour of the process
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgprofile_to_status_dir( const LCFGProfile * profile,
				      const char * status_dir,
                                      LCFGOption options,
				      char ** msg ) {
  assert( status_dir != NULL );

  /* Nothing to do if there are no components for the profile */
  if ( !lcfgprofile_has_components(profile) ) return LCFG_STATUS_OK;

  return lcfgcomplist_to_status_dir( profile->components,
				     status_dir,
				     options,
				     msg );

}

/**
 * @brief Get the list of component names as a taglist
 *
 * This generates a new @c LCFGTagList which contains a list of
 * component names for the @c LCFGProfile. If the list is empty
 * then an empty tag list will be returned. Will return @c NULL if an
 * error occurs. Uses @c lcfgcomplist_get_components_as_taglist() to do
 * the work.
 *
 * To avoid memory leaks, when the list is no longer required the 
 * @c lcfgtaglist_relinquish() function should be called.
 *
 * @param[in] complist Pointer to @c LCFGProfile
 *
 * @return Pointer to a new @c LCFGTagList of component names
 *
 */

LCFGTagList * lcfgprofile_get_components_as_taglist(
                                               const LCFGProfile * profile ) {

  LCFGTagList * names = NULL;
  if ( !lcfgprofile_has_components(profile) ) {
    names = lcfgtaglist_new();
  } else {
    names = lcfgcomplist_get_components_as_taglist(profile->components);
  }

  return names;
}

/**
 * @brief Get the list of ngeneric component names as a taglist
 *
 * This generates a new @c LCFGTagList which contains a list of names
 * for components in the @c LCFGProfile which have 'ngeneric'
 * resources. If the list is empty then an empty tag list will be
 * returned.
 *
 * To avoid memory leaks, when the list is no longer required the 
 * @c lcfgtaglist_relinquish() function should be called.
 *
 * @param[in] profile Pointer to @c LCFGProfile
 *
 * @return Pointer to a new @c LCFGTagList of ngeneric component names
 *
 */

LCFGTagList * lcfgprofile_ngeneric_components( const LCFGProfile * profile ) {

  LCFGTagList * comp_names = lcfgtaglist_new();

  if ( !lcfgprofile_has_components(profile) )
    return comp_names;

  bool ok = true;

  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = lcfgcomplist_head(profile->components);
        cur_node != NULL && ok;
        cur_node = lcfgcomplist_next(cur_node) ) {

    const LCFGComponent * cur_comp = lcfgcomplist_component(cur_node);

    if ( !lcfgcomponent_is_valid(cur_comp) ) continue;

    if ( lcfgcomponent_has_resource( cur_comp, "ng_statusdisplay", false ) ) {
      const char * comp_name = lcfgcomponent_get_name(cur_comp);

      char * msg = NULL;

      LCFGChange change = lcfgtaglist_mutate_add( comp_names, comp_name, &msg );
      if ( change == LCFG_CHANGE_ERROR )
        ok = false;

      free(msg); /* Just ignoring any message */
    }

  }

  if (ok) {
    lcfgtaglist_sort(comp_names);
  } else {
    lcfgtaglist_relinquish(comp_names);
    comp_names = NULL;
  }

  return comp_names;
}

/* eof */
