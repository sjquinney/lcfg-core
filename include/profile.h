
#ifndef LCFG_PROFILE_H
#define LCFG_PROFILE_H

#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>

#include "common.h"
#include "packages.h"
#include "components.h"

struct LCFGProfile {
  char * published_by;
  char * published_at;
  char * server_version;
  char * last_modified;
  char * last_modified_file;
  LCFGPackageList   * active_packages;
  LCFGPackageList   * inactive_packages;
  LCFGComponentList * components;
  time_t mtime;
};

typedef struct LCFGProfile LCFGProfile;

LCFGProfile * lcfgprofile_new(void);

void lcfgprofile_destroy(LCFGProfile * profile);

char * lcfgprofile_get_published_by( const LCFGProfile * profile )
  __attribute__((nonnull (1)));

char * lcfgprofile_get_published_at( const LCFGProfile * profile )
  __attribute__((nonnull (1)));

char * lcfgprofile_get_server_version( const LCFGProfile * profile )
  __attribute__((nonnull (1)));

char * lcfgprofile_get_last_modified( const LCFGProfile * profile )
  __attribute__((nonnull (1)));

char * lcfgprofile_get_last_modified_file( const LCFGProfile * profile )
  __attribute__((nonnull (1)));

time_t lcfgprofile_get_mtime( const LCFGProfile * profile )
  __attribute__((nonnull (1)));

bool lcfgprofile_get_meta( const LCFGProfile * profile,
                           const char * metakey,
                           char ** metavalue )
  __attribute__((nonnull (1,2))) __attribute__((warn_unused_result));

char * lcfgprofile_nodename( const LCFGProfile * profile )
  __attribute__((nonnull (1)));

bool lcfgprofile_has_components( const LCFGProfile * profile )
  __attribute__((nonnull (1)));

bool lcfgprofile_has_component( const LCFGProfile * profile,
                                const char * name )
  __attribute__((nonnull (1)));

LCFGComponent * lcfgprofile_find_component( const LCFGProfile * profile,
                                            const char * name )
  __attribute__((nonnull (1)));

LCFGComponent * lcfgprofile_find_or_create_component( LCFGProfile * profile,
                                                      const char * name )
  __attribute__((nonnull (1)));

LCFGChange lcfgprofile_insert_or_replace_component( LCFGProfile   * profile,
                                                    LCFGComponent * new_comp,
                                                    char ** msg )
  __attribute__((nonnull (1,2))) __attribute__((warn_unused_result));

LCFGStatus lcfgprofile_transplant_components( LCFGProfile * profile1,
					      const LCFGProfile * profile2,
					      char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGStatus lcfgprofile_apply_overrides( LCFGProfile * profile1,
					const LCFGProfile * profile2,
					char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGChange lcfgprofile_write_rpmcfg( const LCFGProfile * profile,
                                     const char * defarch,
                                     const char * filename,
                                     const char * rpminc,
                                     char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Output functions, mostly useful for debugging */

bool lcfgprofile_print_metadata( const LCFGProfile * profile, FILE * out )
  __attribute__((nonnull (1,2))) __attribute__((warn_unused_result));

bool lcfgprofile_print( const LCFGProfile * profile,
                        bool show_comps,
                        bool show_pkgs,
                        const char * defarch,
                        const char * comp_style,
                        FILE * out )
  __attribute__((nonnull (1,6))) __attribute__((warn_unused_result));

LCFGStatus lcfgprofile_from_status_dir( const char * status_dir,
                                        LCFGProfile ** result,
                                        const LCFGTagList * comps_wanted,
                                        char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgprofile_to_status_dir( const LCFGProfile * profile,
				      const char * status_dir,
				      char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

#endif /* LCFG_PROFILE_H */

/* eof */
