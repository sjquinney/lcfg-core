/**
 * @file differences.h
 * @brief Functions for finding the differences between resources, components and profiles.
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#ifndef LCFG_CORE_DIFFERENCES_H
#define LCFG_CORE_DIFFERENCES_H

#include <stdio.h>
#include <stdbool.h>

#include "common.h"
#include "resources.h"
#include "components.h"
#include "profile.h"
#include "utils.h"

/* Differences */

/**
 * @brief A structure to represent the differences between two resources
 */

struct LCFGDiffResource {
  /*@{*/
  LCFGResource * old; /**< The 'old' resource */
  LCFGResource * new; /**< The 'new' resource */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGDiffResource LCFGDiffResource;

LCFGDiffResource * lcfgdiffresource_new(void);

void lcfgdiffresource_destroy(LCFGDiffResource * resdiff);

void lcfgdiffresource_acquire( LCFGDiffResource * resdiff );
void lcfgdiffresource_relinquish( LCFGDiffResource * resdiff );

bool lcfgdiffresource_has_old( const LCFGDiffResource * resdiff );

LCFGResource * lcfgdiffresource_get_old( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_set_old( LCFGDiffResource * resdiff,
                               LCFGResource * res );

bool lcfgdiffresource_has_new( const LCFGDiffResource * resdiff );

LCFGResource * lcfgdiffresource_get_new( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_set_new( LCFGDiffResource * resdiff,
                               LCFGResource * res );

const char * lcfgdiffresource_get_name( const LCFGDiffResource * resdiff );

LCFGChange lcfgdiffresource_get_type( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_is_changed( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_is_nochange( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_is_modified( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_is_added( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_is_removed( const LCFGDiffResource * resdiff );

ssize_t lcfgdiffresource_to_string( const LCFGDiffResource * resdiff,
				    const char * prefix,
				    bool pending,
				    char ** result, size_t * size );

ssize_t lcfgdiffresource_to_hold( const LCFGDiffResource * resdiff,
                                  const char * prefix,
                                  char ** result, size_t * size )
  __attribute__((warn_unused_result));

bool lcfgdiffresource_match( const LCFGDiffResource * resdiff,
                             const char * want_name );

int lcfgdiffresource_compare( const LCFGDiffResource * resdiff1, 
                              const LCFGDiffResource * resdiff2 );

/**
 * @brief A structure to represent the differences between two components
 */

struct LCFGDiffComponent {
  /*@{*/
  char * name;            /**< Name of component */
  LCFGSListNode * head;   /**< The first node in the list */
  LCFGSListNode * tail;   /**< The last node in the list */
  unsigned int size;      /**< The length of the list */
  LCFGChange change_type; /**< The type of differences (e.g. added, removed, modified) */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGDiffComponent LCFGDiffComponent;

LCFGDiffComponent * lcfgdiffcomponent_new(void);

void lcfgdiffcomponent_destroy(LCFGDiffComponent * compdiff );
void lcfgdiffcomponent_acquire( LCFGDiffComponent * compdiff );
void lcfgdiffcomponent_relinquish( LCFGDiffComponent * compdiff );

bool lcfgdiffcomponent_has_name(const LCFGDiffComponent * compdiff);

char * lcfgdiffcomponent_get_name(const LCFGDiffComponent * compdiff);

bool lcfgdiffcomponent_set_name( LCFGDiffComponent * compdiff,
                                 char * new_name )
  __attribute__((warn_unused_result));

bool lcfgdiffcomponent_match( const LCFGDiffComponent * compdiff,
                             const char * want_name );
int lcfgdiffcomponent_compare( const LCFGDiffComponent * compdiff1, 
                               const LCFGDiffComponent * compdiff2 );

void lcfgdiffcomponent_set_type( LCFGDiffComponent * compdiff,
                                 LCFGChange change_type );

LCFGChange lcfgdiffcomponent_get_type( const LCFGDiffComponent * compdiff );

bool lcfgdiffcomponent_is_nochange( const LCFGDiffComponent * compdiff );

bool lcfgdiffcomponent_is_changed( const LCFGDiffComponent * compdiff );

bool lcfgdiffcomponent_is_added( const LCFGDiffComponent * compdiff );

bool lcfgdiffcomponent_is_modified( const LCFGDiffComponent * compdiff );

bool lcfgdiffcomponent_is_removed( const LCFGDiffComponent * compdiff );

bool lcfgdiffcomponent_resource_is_changed( const LCFGDiffComponent * compdiff,
					    const char * res_name );

LCFGChange lcfgdiffcomponent_insert_next( LCFGDiffComponent * list,
                                          LCFGSListNode     * node,
                                          LCFGDiffResource  * item )
  __attribute__((warn_unused_result));

LCFGChange lcfgdiffcomponent_remove_next( LCFGDiffComponent * list,
                                          LCFGSListNode     * node,
                                          LCFGDiffResource ** item )
  __attribute__((warn_unused_result));

#define lcfgdiffcomponent_head     lcfgslist_head
#define lcfgdiffcomponent_tail     lcfgslist_tail
#define lcfgdiffcomponent_size     lcfgslist_size
#define lcfgdiffcomponent_is_empty lcfgslist_is_empty

#define lcfgdiffcomponent_has_next lcfgslist_has_next
#define lcfgdiffcomponent_next     lcfgslist_next
#define lcfgdiffcomponent_resdiff  lcfgslist_data

#define lcfgdiffcomponent_append(list, item) ( lcfgdiffcomponent_insert_next( list, lcfgslist_tail(list), item ) )

LCFGSListNode * lcfgdiffcomponent_find_node( const LCFGDiffComponent * list,
                                             const char * want_name );

LCFGDiffResource * lcfgdiffcomponent_find_resource(
					  const LCFGDiffComponent * compdiff,
					  const char * want_name );

bool lcfgdiffcomponent_has_resource( const LCFGDiffComponent * compdiff,
				     const char * want_name );

void lcfgdiffcomponent_sort( LCFGDiffComponent * compdiff );

LCFGStatus lcfgdiffcomponent_to_holdfile( const LCFGDiffComponent * compdiff,
                                          FILE * holdfile )
  __attribute__((warn_unused_result));

LCFGStatus lcfgdiffcomponent_names_for_type(const LCFGDiffComponent * compdiff,
                                            LCFGChange change_type,
                                            LCFGTagList ** result )
  __attribute__((warn_unused_result));

LCFGStatus lcfgdiffcomponent_changed( const LCFGDiffComponent * compdiff,
                                      LCFGTagList ** res_names )
 __attribute__((warn_unused_result));

LCFGStatus lcfgdiffcomponent_added( const LCFGDiffComponent * compdiff,
                                      LCFGTagList ** res_names )
 __attribute__((warn_unused_result));

LCFGStatus lcfgdiffcomponent_removed( const LCFGDiffComponent * compdiff,
                                      LCFGTagList ** res_names )
 __attribute__((warn_unused_result));

LCFGStatus lcfgdiffcomponent_modified( const LCFGDiffComponent * compdiff,
                                       LCFGTagList ** res_names )
 __attribute__((warn_unused_result));

LCFGChange lcfgresource_diff( LCFGResource * old_res,
                              LCFGResource * new_res,
                              LCFGDiffResource ** resdiff )
  __attribute__((warn_unused_result));

LCFGChange lcfgcomponent_quickdiff( const LCFGComponent * comp1,
                                    const LCFGComponent * comp2 )
  __attribute__((warn_unused_result));

LCFGChange lcfgcompset_quickdiff( const LCFGComponentSet * set1,
                                  const LCFGComponentSet * set2,
                                  LCFGTagList ** modified,
                                  LCFGTagList ** added,
                                  LCFGTagList ** removed )
  __attribute__((warn_unused_result));

LCFGChange lcfgprofile_quickdiff( const LCFGProfile * profile1,
                                  const LCFGProfile * profile2,
                                  LCFGTagList ** modified,
                                  LCFGTagList ** added,
                                  LCFGTagList ** removed )
  __attribute__((warn_unused_result));

/**
 * @brief A structure to represent the differences between two profiles
 */

struct LCFGDiffProfile {
  LCFGSListNode * head; /**< The first node in the list */
  LCFGSListNode * tail; /**< The last node in the list */
  unsigned int size;    /**< The length of the list */
};

typedef struct LCFGDiffProfile LCFGDiffProfile;

LCFGDiffProfile * lcfgdiffprofile_new(void);

void lcfgdiffprofile_destroy(LCFGDiffProfile * profdiff );

LCFGChange lcfgdiffprofile_insert_next( LCFGDiffProfile    * list,
                                        LCFGSListNode      * node,
                                        LCFGDiffComponent  * item )
  __attribute__((warn_unused_result));

LCFGChange lcfgdiffprofile_remove_next( LCFGDiffProfile    * list,
                                        LCFGSListNode      * node,
                                        LCFGDiffComponent ** item )
  __attribute__((warn_unused_result));

#define lcfgdiffprofile_head     lcfgslist_head
#define lcfgdiffprofile_tail     lcfgslist_tail
#define lcfgdiffprofile_size     lcfgslist_size
#define lcfgdiffprofile_is_empty lcfgslist_is_empty

#define lcfgdiffprofile_has_next lcfgslist_has_next
#define lcfgdiffprofile_next     lcfgslist_next
#define lcfgdiffprofile_compdiff lcfgslist_data

#define lcfgdiffprofile_append(list, item) ( lcfgdiffprofile_insert_next( list, lcfgslist_tail(list), item ) )

LCFGSListNode * lcfgdiffprofile_find_node( const LCFGDiffProfile * profdiff,
                                           const char * want_name );

LCFGDiffComponent * lcfgdiffprofile_find_component(
					    const LCFGDiffProfile * profdiff,
					    const char * want_name );

bool lcfgdiffprofile_has_component( const LCFGDiffProfile * profdiff,
				    const char * want_name );

void lcfgdiffprofile_sort( LCFGDiffProfile * list );

LCFGStatus lcfgdiffprofile_to_holdfile( LCFGDiffProfile * profdiff,
                                        const char * holdfile,
                                        const char * signature,
                                        char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgcomponent_diff( const LCFGComponent * comp1,
                               const LCFGComponent * comp2,
                               LCFGDiffComponent ** compdiff )
  __attribute__((warn_unused_result));

LCFGChange lcfgprofile_diff( const LCFGProfile * profile1,
                             const LCFGProfile * profile2,
                             LCFGDiffProfile ** result )
  __attribute__((warn_unused_result));


LCFGStatus lcfgdiffprofile_names_for_type( const LCFGDiffProfile * profdiff,
                                           LCFGChange change_type,
                                           LCFGTagList ** result )
  __attribute__((warn_unused_result));

LCFGStatus lcfgdiffprofile_changed( const LCFGDiffProfile * profdiff,
                                    LCFGTagList ** comp_names )
  __attribute__((warn_unused_result));

LCFGStatus lcfgdiffprofile_added( const LCFGDiffProfile * profdiff,
                                  LCFGTagList ** comp_names )
  __attribute__((warn_unused_result));

LCFGStatus lcfgdiffprofile_removed( const LCFGDiffProfile * profdiff,
                                    LCFGTagList ** comp_names )
  __attribute__((warn_unused_result));

LCFGStatus lcfgdiffprofile_modified( const LCFGDiffProfile * profdiff,
                                    LCFGTagList ** comp_names )
  __attribute__((warn_unused_result));

bool lcfgdiffcomponent_was_prodded( const LCFGDiffComponent * compdiff );

bool lcfgdiffprofile_component_was_prodded( const LCFGDiffProfile * profdiff,
					    const char * comp_name );

bool lcfgdiffprofile_component_is_changed( const LCFGDiffProfile * profdiff,
					   const char * comp_name );

#endif /* LCFG_CORE_DIFFERENCES_H */

/* eof */
