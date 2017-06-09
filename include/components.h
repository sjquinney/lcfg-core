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

/**
 * @brief A structure to wrap an LCFG resource as a single-linked list item
 */

struct LCFGResourceNode {
  LCFGResource * resource;        /**< Pointer to the resource structure */
  struct LCFGResourceNode * next; /**< Next node in the list */
};

typedef struct LCFGResourceNode LCFGResourceNode;

LCFGResourceNode * lcfgresourcenode_new(LCFGResource * res);

void lcfgresourcenode_destroy(LCFGResourceNode * resnode);

/**
 * @brief A structure to represent an LCFG Component
 */

struct LCFGComponent {
  /*@{*/
  char * name;              /**< Name (required) */
  LCFGResourceNode * head;  /**< The first resource node in the list */
  LCFGResourceNode * tail;  /**< The last resource node in the list */
  LCFGMergeRule merge_rules; /**< Rules which control how resources are merged */
  unsigned int size;        /**< The length of the list */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGComponent LCFGComponent;

LCFGComponent * lcfgcomponent_new(void);

void lcfgcomponent_remove_all_resources( LCFGComponent * comp );
void lcfgcomponent_destroy(LCFGComponent * comp);
void lcfgcomponent_acquire(LCFGComponent * comp);
void lcfgcomponent_relinquish(LCFGComponent * comp);

LCFGMergeRule lcfgcomponent_get_merge_rules( const LCFGComponent * comp );
bool lcfgcomponent_set_merge_rules( LCFGComponent * comp,
                                    LCFGMergeRule new_rules )
  __attribute__((warn_unused_result));

LCFGComponent * lcfgcomponent_clone( LCFGComponent * comp, bool deep_copy );

bool lcfgcomponent_is_valid( const LCFGComponent * comp );

bool lcfgcomponent_has_name(const LCFGComponent * comp);
bool lcfgcomponent_valid_name(const char * name );
const char * lcfgcomponent_get_name(const LCFGComponent * comp);
bool lcfgcomponent_set_name( LCFGComponent * comp, char * new_name )
  __attribute__((warn_unused_result));

bool lcfgcomponent_print( const LCFGComponent * comp,
                          LCFGResourceStyle style,
                          LCFGOption options,
                          FILE * out )
  __attribute__((warn_unused_result));

bool lcfgcomponent_to_export( const LCFGComponent * comp,
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

LCFGStatus lcfgcomponent_to_status_file( const LCFGComponent * comp,
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

LCFGTagList * lcfgcomponent_get_resources_as_taglist(const LCFGComponent * comp,
                                                     LCFGOption options );

char * lcfgcomponent_get_resources_as_string( const LCFGComponent * comp,
                                              LCFGOption options );

LCFGChange lcfgcomponent_insert_next( LCFGComponent    * comp,
                                      LCFGResourceNode * resnode,
                                      LCFGResource     * res )
  __attribute__((warn_unused_result));

LCFGChange lcfgcomponent_remove_next( LCFGComponent    * comp,
                                      LCFGResourceNode * resnode,
                                      LCFGResource    ** res )
  __attribute__((warn_unused_result));

/**
 * @brief Retrieve the first resource node in the list
 *
 * This is a simple macro which can be used to get the first resource
 * node structure in the list. Note that if the list is empty this
 * will be the @c NULL value. To retrieve the resource from this node
 * use @c lcfgcomponent_resource()
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 * @return Pointer to first @c LCFGResourceNode structure in list
 *
 */

#define lcfgcomponent_head(comp) ( comp == NULL ? NULL : (comp)->head )

/**
 * @brief Retrieve the last resource node in the list
 *
 * This is a simple macro which can be used to get the last resource
 * node structure in the list. Note that if the list is empty this
 * will be the @c NULL value. To retrieve the resource from this node
 * use @c lcfgcomponent_resource()
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 * @return Pointer to last @c LCFGResourceNode structure in list
 *
 */

#define lcfgcomponent_tail(comp) ((comp)->tail)

/**
 * @brief Get the number of nodes in the list
 *
 * This is a simple macro which can be used to get the length of the
 * single-linked resource list.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 * @return Integer length of the list of resources
 *
 */

#define lcfgcomponent_size(comp) ((comp)->size)

/**
 * @brief Test if the component has no resources
 *
 * This is a simple macro which can be used to test if the
 * single-linked list of resources contains any nodes.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 * @return Boolean which indicates whether the list contains any nodes
 *
 */

#define lcfgcomponent_is_empty(comp) ( comp == NULL || (comp)->size == 0)

/**
 * @brief Retrieve the next resource node in the list
 *
 * This is a simple macro which can be used to fetch the next node in
 * the single-linked resource list for a given node. If the node
 * specified is the final item in the list this will return a @c NULL
 * value.
 *
 * @param[in] resnode Pointer to current @c LCFGResourceNode
 *
 * @return Pointer to next @c LCFGResourceNode
 */

#define lcfgcomponent_next(resnode)     ((resnode)->next)

/**
 * @brief Retrieve the resource for a list node
 *
 * This is a simple macro which can be used to get the resource
 * structure from the specified node. 
 *
 * Note that this does @b NOT increment the reference count for the
 * returned resource structure. To retain the resource call the
 * @c lcfgresource_acquire() function.
 *
 * @param[in] resnode Pointer to @c LCFGResourceNode
 *
 * @return Pointer to @c LCFGResource structure
 *
 */

#define lcfgcomponent_resource(resnode) ((resnode)->resource)

/**
 * @brief Append a resource to a list
 *
 * This is a simple macro wrapper around the @c
 * lcfgcomponent_insert_next() function which can be used to simply
 * append a resource structure on to the end of the specified
 * component. Depending on the situation it may be more appropriate to use
 * @c lcfgcomponent_merge_resource()
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] res Pointer to @c LCFGResource
 * 
 * @return Integer value indicating type of change
 *
 */

#define lcfgcomponent_append(comp, res) ( lcfgcomponent_insert_next( comp, lcfgcomponent_tail(comp), res ) )

LCFGResourceNode * lcfgcomponent_find_node( const LCFGComponent * comp,
                                            const char * name,
                                            bool all_priorities );

LCFGResource * lcfgcomponent_find_resource( const LCFGComponent * comp,
                                            const char * name,
                                            bool all_priorities );

bool lcfgcomponent_has_resource(  const LCFGComponent * comp,
                                  const char * name,
                                  bool all_priorities );

LCFGResource * lcfgcomponent_find_or_create_resource( LCFGComponent * comp,
                                                      const char * name,
                                                      bool all_priorities );

LCFGChange lcfgcomponent_merge_resource( LCFGComponent * comp,
                                         LCFGResource  * res,
                                         char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgcomponent_merge_component( LCFGComponent * comp,
					  const LCFGComponent * overrides,
					  char ** msg )
  __attribute__((warn_unused_result));

void lcfgcomponent_sort( LCFGComponent * comp );

unsigned long lcfgcomponent_hash( const LCFGComponent * comp );

bool lcfgcomponent_same_name( const LCFGComponent * comp1,
                              const LCFGComponent * comp2 );

int lcfgcomponent_compare( const LCFGComponent * comp1,
                           const LCFGComponent * comp2 );

bool lcfgcomponent_match( const LCFGComponent * comp,
                          const char * name );

LCFGStatus lcfgcomponent_subset( const LCFGComponent * comp,
                                 const LCFGTagList * res_wanted,
                                 LCFGComponent ** result,
                                 LCFGOption options,
                                 char ** msg )
  __attribute__((warn_unused_result));

bool lcfgcomponent_is_ngeneric( const LCFGComponent * comp );

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

/* Resource List iterator */
/**
 * @brief Simple iterator for lists of resources (i.e. a component)
 */

struct LCFGResourceIterator {
  LCFGComponent * list;       /**< The resource list */
  LCFGResourceNode * current; /**< Current location in the list */
};
typedef struct LCFGResourceIterator LCFGResourceIterator;

LCFGResourceIterator * lcfgresiter_new( LCFGComponent * list );

void lcfgresiter_destroy( LCFGResourceIterator * iterator );
void lcfgresiter_reset( LCFGResourceIterator * iterator );
bool lcfgresiter_has_next( LCFGResourceIterator * iterator );
LCFGResource * lcfgresiter_next(LCFGResourceIterator * iterator);


#endif /* LCFG_CORE_COMPONENT_H */

/* eof */
