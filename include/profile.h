/**
 * @file profile.h
 * @brief Functions for working with LCFG profiles
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date$
 * $Revision$
 */

#ifndef LCFG_CORE_PROFILE_H
#define LCFG_CORE_PROFILE_H

#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>

#include "common.h"
#include "packages.h"
#include "components.h"

#define ACTIVE_PACKAGE_RULES   ( LCFG_MERGE_RULE_SQUASH_IDENTICAL | LCFG_MERGE_RULE_USE_PRIORITY )
#define INACTIVE_PACKAGE_RULES ( LCFG_MERGE_RULE_SQUASH_IDENTICAL | LCFG_MERGE_RULE_KEEP_ALL )

/**
 * @brief A structure to represent an LCFG Profile
 */

struct LCFGProfile {
  char * published_by;        /**< Name of publisher */
  char * published_at;        /**< Timestamp for when profile was published */
  char * server_version;      /**< Version of server which generated profile */
  char * last_modified;       /**< Timestamp for most recently modified source file */
  char * last_modified_file;  /**< Name for most recently modified source file */
  LCFGPackageList   * active_packages;   /**< List of packages which are active in current contexts */
  LCFGPackageList   * inactive_packages; /**< List of packages which are inactive in current contexts */
  LCFGComponentList * components;        /**< List of components */
  time_t mtime;               /**< Modification time of input file (seconds since epoch) */
};

typedef struct LCFGProfile LCFGProfile;

LCFGProfile * lcfgprofile_new(void);

void lcfgprofile_destroy(LCFGProfile * profile);

char * lcfgprofile_get_published_by( const LCFGProfile * profile );

char * lcfgprofile_get_published_at( const LCFGProfile * profile );

char * lcfgprofile_get_server_version( const LCFGProfile * profile );

char * lcfgprofile_get_last_modified( const LCFGProfile * profile );

char * lcfgprofile_get_last_modified_file( const LCFGProfile * profile );

time_t lcfgprofile_get_mtime( const LCFGProfile * profile );

bool lcfgprofile_get_meta( const LCFGProfile * profile,
                           const char * metakey,
                           char ** metavalue )
  __attribute__((warn_unused_result));

char * lcfgprofile_nodename( const LCFGProfile * profile );

bool lcfgprofile_has_components( const LCFGProfile * profile );

LCFGComponentList * lcfgprofile_get_components( const LCFGProfile * profile );

bool lcfgprofile_has_component( const LCFGProfile * profile,
                                const char * name );

LCFGComponent * lcfgprofile_find_component( const LCFGProfile * profile,
                                            const char * name );

LCFGComponent * lcfgprofile_find_or_create_component( LCFGProfile * profile,
                                                      const char * name );

LCFGChange lcfgprofile_insert_or_replace_component( LCFGProfile   * profile,
                                                    LCFGComponent * new_comp,
                                                    char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgprofile_transplant_components( LCFGProfile * profile1,
					      const LCFGProfile * profile2,
					      char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgprofile_merge( LCFGProfile * profile1,
			      const LCFGProfile * profile2,
			      bool take_new_comps,
			      char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgprofile_write_rpmcfg( const LCFGProfile * profile,
                                     const char * defarch,
                                     const char * filename,
                                     const char * rpminc,
                                     char ** msg )
  __attribute__((warn_unused_result));

/* Output functions, mostly useful for debugging */

bool lcfgprofile_print_metadata( const LCFGProfile * profile, FILE * out )
  __attribute__((warn_unused_result));

bool lcfgprofile_print( const LCFGProfile * profile,
                        bool show_comps,
                        bool show_pkgs,
                        const char * defarch,
                        LCFGResourceStyle comp_style,
                        LCFGPkgStyle pkg_style,
                        FILE * out )
  __attribute__((warn_unused_result));

LCFGStatus lcfgprofile_from_status_dir( const char * status_dir,
                                        LCFGProfile ** result,
                                        const LCFGTagList * comps_wanted,
                                        LCFGOption options,
                                        char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgprofile_to_status_dir( const LCFGProfile * profile,
				      const char * status_dir,
                                      LCFGOption options,
				      char ** msg )
  __attribute__((warn_unused_result));

LCFGTagList * lcfgprofile_get_components_as_taglist(
                                               const LCFGProfile * profile );

LCFGTagList * lcfgprofile_ngeneric_components( const LCFGProfile * profile );

#endif /* LCFG_CORE_PROFILE_H */

/* eof */
