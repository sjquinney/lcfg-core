/**
 * @file bdb.h
 * @brief Functions for importing and export profiles to Berkeley DB
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date$
 * $Revision$
 */

#ifndef LCFG_CORE_BDB_H
#define LCFG_CORE_BDB_H

#include <sys/types.h>

#include <db.h>

#include "common.h"
#include "context.h"
#include "resources.h"
#include "profile.h"

DB * lcfgbdb_open_db( const char * filename,
                      u_int32_t flags,
                      char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

DB * lcfgbdb_init_reader( const char * filename,
                          char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

void lcfgbdb_close_db( DB * dbh );

DB * lcfgbdb_init_writer( const char * filename,
                          char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGStatus lcfgbdb_process_components( DB * dbh,
				       LCFGComponentList ** result,
				       const LCFGTagList * comps_wanted,
                                       const char * namespace,
				       char ** errmsg )
   __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGStatus lcfgprofile_from_bdb( const char * filename,
				 LCFGProfile ** result,
				 const LCFGTagList * comps_wanted,
                                 const char * namespace,
				 unsigned int options,
				 char ** errmsg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGStatus lcfgcomponent_from_bdb( const char * filename,
				   LCFGComponent ** result,
				   const char * compname,
                                   const char * namespace,
				   unsigned int options,
				   char ** errmsg )
  __attribute__((nonnull (1,3))) __attribute__((warn_unused_result));

LCFGStatus lcfgresource_to_bdb( const LCFGResource * resource,
                                const char * prefix,
                                const char * namespace,
                                DB * dbh,
                                char ** errmsg )
  __attribute__((nonnull (1,4))) __attribute__((warn_unused_result));

LCFGStatus lcfgcomponent_to_bdb( const LCFGComponent * component,
                                     const char * namespace,
                                     DB * dbh,
                                     char ** errmsg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomplist_to_bdb( const LCFGComponentList * components,
                                    const char * namespace,
                                    DB * dbh,
                                    char ** errmsg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgprofile_to_bdb( const LCFGProfile * profile,
                               const char * namespace,
                               const char * dbfile,
                               char ** errmsg )
   __attribute__((nonnull (1))) __attribute__((warn_unused_result));

#endif /* LCFG_CORE_BDB_H */

/* eof */
