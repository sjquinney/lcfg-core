/**
 * @file components.h
 * @brief Functions for working with LCFG components
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#ifndef LCFG_CORE_COMPONENT_H
#define LCFG_CORE_COMPONENT_H

#include <stdbool.h>
#include <stdio.h>

#include "resources.h"

/* Components */

typedef enum {
  LCFG_COMP_PK_NAME = 1,
  LCFG_COMP_PK_CTX  = 2
} LCFGComponentPK;

#define LCFG_COMP_DEFAULT_SIZE 113
#define LCFG_COMP_LOAD_INIT 0.5
#define LCFG_COMP_LOAD_MAX  0.7

/**
 * @brief A structure to represent an LCFG Component
 */

struct LCFGResourceList {
  /*@{*/
  LCFGSListNode * head;      /**< The first node in the list */
  LCFGSListNode * tail;      /**< The last node in the list */
  unsigned int size;         /**< The length of the list */
  LCFGComponentPK primary_key; /**< Controls which package fields are used as primary key */
  LCFGMergeRule merge_rules; /**< Rules which control how resources are merged */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGResourceList LCFGResourceList;

struct LCFGComponent {
  /*@{*/
  char * name;                   /**< Name (required) */
  LCFGResourceList ** resources; /**< Array of resource lists */
  unsigned long buckets;         /**< Array of derivation lists */
  unsigned long entries;         /**< Number of full buckets in map */
  LCFGComponentPK primary_key;     /**< Controls which resource fields are used as primary key */
  LCFGMergeRule merge_rules;     /**< Rules which control how resources are merged */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGComponent LCFGComponent;

LCFGComponent * lcfgcomponent_new(void);

void lcfgcomponent_remove_all_resources( LCFGComponent * comp );

void lcfgcomponent_acquire(LCFGComponent * comp);
void lcfgcomponent_relinquish(LCFGComponent * comp);
bool lcfgcomponent_is_shared(const LCFGComponent * comp);

LCFGMergeRule lcfgcomponent_get_merge_rules( const LCFGComponent * comp );
bool lcfgcomponent_set_merge_rules( LCFGComponent * comp,
                                    LCFGMergeRule new_rules )
  __attribute__((warn_unused_result));

LCFGComponent * lcfgcomponent_clone( const LCFGComponent * comp );

bool lcfgcomponent_is_valid( const LCFGComponent * comp );

bool lcfgcomponent_has_name(const LCFGComponent * comp);
bool lcfgcomponent_valid_name(const char * name );
const char * lcfgcomponent_get_name(const LCFGComponent * comp);
bool lcfgcomponent_set_name( LCFGComponent * comp, char * new_name )
  __attribute__((warn_unused_result));

unsigned int lcfgcomponent_size( const LCFGComponent * comp );
#define lcfgcomponent_is_empty(COMP) ( COMP == NULL || lcfgcomponent_size(COMP) == 0)

bool lcfgcomponent_print( const LCFGComponent * comp,
                          LCFGResourceStyle style,
                          LCFGOption options,
                          FILE * out )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomponent_to_export( const LCFGComponent * comp,
                                    const char * val_pfx, const char * type_pfx,
                                    LCFGOption options,
                                    FILE * out )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomponent_from_status_file( const char * filename,
					   LCFGComponent ** result,
					   const char * compname,
					   LCFGOption options,
					   char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgcomponent_to_status_file( const LCFGComponent * comp,
					 const char * filename,
					 LCFGOption options,
					 char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomponent_from_env( const char * compname,
                                   const char * val_pfx, const char * type_pfx,
                                   LCFGComponent ** comp,
                                   LCFGOption options,
                                   char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomponent_to_env( const LCFGComponent * comp,
				 const char * val_pfx, const char * type_pfx,
				 LCFGOption options,
                                 char ** msg )
  __attribute__((warn_unused_result));

LCFGTagList * lcfgcomponent_get_resources_as_taglist(const LCFGComponent * comp);

char * lcfgcomponent_get_resources_as_string( const LCFGComponent * comp);

const LCFGResource * lcfgcomponent_find_resource( const LCFGComponent * comp,
                                                  const char * want_name );

bool lcfgcomponent_has_resource(  const LCFGComponent * comp,
                                  const char * want_name );

LCFGChange lcfgcomponent_merge_resource( LCFGComponent * comp,
                                         LCFGResource  * res,
                                         char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgcomponent_merge_component( LCFGComponent * comp,
					  const LCFGComponent * overrides,
					  char ** msg )
  __attribute__((warn_unused_result));

unsigned long lcfgcomponent_hash( const LCFGComponent * comp );

bool lcfgcomponent_same_name( const LCFGComponent * comp1,
                              const LCFGComponent * comp2 );

int lcfgcomponent_compare( const LCFGComponent * comp1,
                           const LCFGComponent * comp2 );

bool lcfgcomponent_match( const LCFGComponent * comp,
                          const char * name );

LCFGStatus lcfgcomponent_select( const LCFGComponent * comp,
				 LCFGTagList * res_wanted,
                                 LCFGComponent ** result,
                                 LCFGOption options,
                                 char ** msg )
  __attribute__((warn_unused_result));

bool lcfgcomponent_is_ngeneric( const LCFGComponent * comp );

bool lcfgcomponent_update_signature( const LCFGComponent * comp,
                                     md5_state_t * md5state,
                                     char ** buffer, size_t * buf_size )
  __attribute__((warn_unused_result));

/* Component Set */

struct LCFGComponentSet {
  LCFGComponent ** components;
  size_t buckets;
  size_t entries;
  unsigned int _refcount;
};

typedef struct LCFGComponentSet LCFGComponentSet;

#define lcfgcompset_is_empty(COMPSET) ( COMPSET == NULL || (COMPSET)->entries == 0 )

LCFGComponentSet * lcfgcompset_new();
void lcfgcompset_destroy(LCFGComponentSet * compset);

void lcfgcompset_acquire(LCFGComponentSet * compset);
void lcfgcompset_relinquish( LCFGComponentSet * compset );

LCFGComponent * lcfgcompset_find_component( const LCFGComponentSet * compset,
                                            const char * want_name );

bool lcfgcompset_has_component( const LCFGComponentSet * compset,
                                const char * want_name );

LCFGChange lcfgcompset_insert_component( LCFGComponentSet * compset,
                                         LCFGComponent * comp )
  __attribute__((warn_unused_result));

LCFGChange lcfgcompset_merge_components( LCFGComponentSet * compset1,
                                         const LCFGComponentSet * compset2,
                                         bool take_new,
                                         char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgcompset_transplant_components( LCFGComponentSet * compset1,
                                              const LCFGComponentSet * compset2,
                                              char ** msg )
  __attribute__((warn_unused_result));

LCFGComponent * lcfgcompset_find_or_create_component(
                                                     LCFGComponentSet * compset,
                                                     const char * name )
  __attribute__((warn_unused_result));

LCFGTagList * lcfgcompset_get_components_as_taglist(
                                            const LCFGComponentSet * compset );

char * lcfgcompset_get_components_as_string(
                                            const LCFGComponentSet * compset );

bool lcfgcompset_print( const LCFGComponentSet * compset,
                        LCFGResourceStyle style,
                        LCFGOption options,
                        FILE * out )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcompset_from_status_dir( const char * status_dir,
                                        LCFGComponentSet ** result,
                                        const LCFGTagList * comps_wanted,
                                        LCFGOption options,
                                        char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcompset_to_status_dir( const LCFGComponentSet * compset,
                                      const char * status_dir,
                                      LCFGOption options,
                                      char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcompset_from_env( const char * val_pfx, const char * type_pfx,
                                 LCFGComponentSet ** result,
                                 LCFGTagList * comps_wanted,
                                 LCFGOption options,
                                 char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcompset_to_env( const LCFGComponentSet * compset,
                               const char * val_pfx, const char * type_pfx,
                               LCFGOption options,
                               char ** msg )
  __attribute__((warn_unused_result));

LCFGTagList * lcfgcompset_ngeneric_components( const LCFGComponentSet * compset );

char * lcfgcompset_signature( const LCFGComponentSet * compset );

/* Component resource iterator */
/**
 * @brief Simple iterator for set of resources (i.e. a component)
 */

struct LCFGResourceListIterator {
  LCFGResourceList * list; /**< The resource list */
  LCFGSListNode * current; /**< Current location in the list */
};
typedef struct LCFGResourceListIterator LCFGResourceListIterator;

struct LCFGComponentIterator {
  LCFGComponent * comp;       /**< The resource list */
  LCFGResourceListIterator * listiter; /**< Resource List iterator */
  long current;               /**< Current location in the set */
  bool all_priorities;        /**< Include all states of resource */
};
typedef struct LCFGComponentIterator LCFGComponentIterator;

LCFGComponentIterator * lcfgcompiter_new( LCFGComponent * comp,
                                          bool all_priorities );

void lcfgcompiter_destroy( LCFGComponentIterator * iterator );
void lcfgcompiter_reset( LCFGComponentIterator * iterator );
bool lcfgcompiter_has_next( LCFGComponentIterator * iterator );
const LCFGResource * lcfgcompiter_next(LCFGComponentIterator * iterator);


#endif /* LCFG_CORE_COMPONENT_H */

/* eof */
