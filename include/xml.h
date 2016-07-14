#ifndef LCFG_XMLREADER_H
#define LCFG_XMLREADER_H

#include <stdbool.h>
#include <stdio.h>

#include <libxml/xmlreader.h>

#include "profile.h"

/* The depths in the LCFG XML profile at which the metadata attributes
   are values are defined. */

#define LCFGXML_ATTR_DEPTH 2
#define LCFGXML_ATTRVALUE_DEPTH 3

xmlTextReaderPtr lcfgxml_init_reader( const char * filename, char ** msg );

LCFGStatus lcfgxml_collect_metadata( xmlTextReaderPtr reader,
				     const  char * target_nodename,
				     LCFGProfile * profile,
				     char ** msg )
  __attribute__((nonnull (1,2,3))) __attribute__((warn_unused_result));

LCFGStatus lcfgprofile_from_xml( const char   * filename,
				 LCFGProfile ** profile,
				 const char   * base_context,
				 const char   * base_derivation,
				 const LCFGContextList * ctxlist,
				 const LCFGTagList     * comps_wanted,
				 bool           require_packages,
				 char        ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgprofile_overrides_xmldir( LCFGProfile  * profile,
                                         const char   * override_dir,
                                         const LCFGContextList * ctxlist,
                                         char        ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGStatus lcfgprofile_overrides_context( LCFGProfile * profile,
					  const char * override_dir,
                                          LCFGContextList * ctxlist,
                                          char       ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

#define LCFGXML_TOP_NODE             "lcfg"
#define LCFGXML_COMPS_PARENT_NODE    "components"
#define LCFGXML_COMPS_CHILD_NODE     "component"
#define LCFGXML_PACKAGES_PARENT_NODE "packages"
#define LCFGXML_PACKAGES_CHILD_NODE  "package"

bool lcfgxml_moveto_next_tag( xmlTextReaderPtr reader )
  __attribute__((nonnull (1)));

bool lcfgxml_moveto_node( xmlTextReaderPtr reader,
                          const char * target_nodename )
  __attribute__((nonnull (1,2)));

bool lcfgxml_correct_location( xmlTextReaderPtr reader,
                               const char * expected_nodename )
  __attribute__((nonnull (1,2)));

void lcfgxml_set_error_message( char ** msg, const char * fmt, ...);

LCFGStatus lcfgxml_gather_attributes( xmlTextReaderPtr reader,
				      LCFGResource * head_res,
				      const char * compname,
				      char ** msg )
  __attribute__((nonnull (1,2,3))) __attribute__((warn_unused_result));

LCFGStatus lcfgxml_process_record( xmlTextReaderPtr reader,
				   LCFGComponent * lcfgcomp,
				   const LCFGTemplate * templates,
				   char ** thistag,
				   const LCFGTagList * ancestor_tags,
				   const char * base_context,
				   const char * base_derivation,
				   const LCFGContextList * ctxlist,
				   char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGStatus lcfgxml_process_resource( xmlTextReaderPtr reader,
				     LCFGComponent * lcfgcomp,
				     const LCFGTemplate * templates,
				     char ** thistag,
				     const LCFGTagList * ancestor_tags,
				     const char * base_context,
				     const char * base_derivation,
				     const LCFGContextList * ctxlist,
				     char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGStatus lcfgxml_process_component( xmlTextReaderPtr reader,
				      const char * compname,
				      LCFGComponent ** result,
				      const char * base_context,
				      const char * base_derivation,
				      const LCFGContextList * ctxlist,
				      char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGStatus lcfgxml_process_components( xmlTextReaderPtr reader,
				       LCFGComponentList ** result,
				       const char * base_context,
				       const char * base_derivation,
				       const LCFGContextList * ctxlist,
				       const LCFGTagList * comps_wanted,
				       char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGStatus lcfgxml_process_package( xmlTextReaderPtr reader,
				    LCFGPackageSpec **result,
				    const char * base_context,
				    const char * base_derivation,
				    char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGStatus lcfgxml_process_packages( xmlTextReaderPtr reader,
				     LCFGPackageList ** active,
				     LCFGPackageList ** inactive,
				     const char * base_context,
				     const char * base_derivation,
				     const LCFGContextList * ctxlist,
				     char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

#endif /* LCFG_XMLREADER_H */

/* eof */
