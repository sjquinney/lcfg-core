/**
 * @file tags.h
 * @brief Functions for working with LCFG resource tags
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date$
 * $Revision$
 *
 * Doubly linked-list structure for ordered lists of LCFG "tags". Also
 * intended to be reasonably efficient for set-like operations.
 *
 */

#ifndef LCFG_CORE_TAGS_H
#define LCFG_CORE_TAGS_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"

/**
 * @brief The maximum supported tag depth
 */

#define LCFG_TAGS_MAX_DEPTH 5

/* Tag */

/**
 * @brief A structure to represent a single LCFG resource tag
 *
 * This holds the tag name and, for efficiency, the length of the name
 * as it is frequently required.
 *
 * This structure supports reference counting so a tag may appear in
 * multiple lists. See @c lcfgtag_acquire() and 
 * @c lcfgtag_relinquish() for details.
 *
 */

struct LCFGTag {
  /*@{*/
  char * name;     /**< The tag name */
  size_t name_len; /**< The length of the tag name */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGTag LCFGTag;

LCFGTag * lcfgtag_new();
void lcfgtag_destroy( LCFGTag * tag );
void lcfgtag_acquire( LCFGTag * tag );
void lcfgtag_relinquish( LCFGTag * tag );

bool lcfgtag_is_valid( const LCFGTag * tag );
bool lcfgresource_valid_tag( const char * value );

bool lcfgtag_has_name( const LCFGTag * tag );

bool lcfgtag_set_name( LCFGTag * tag, char * new_name )
  __attribute__((warn_unused_result));

char * lcfgtag_get_name( const LCFGTag * tag );

size_t lcfgtag_get_length( const LCFGTag * tag );

LCFGStatus lcfgtag_from_string( const char * input,
                                LCFGTag ** result,
                                char ** msg )
  __attribute__((warn_unused_result));

int lcfgtag_compare( const LCFGTag * tag, const LCFGTag * other );
bool lcfgtag_matches( const LCFGTag * tag, const char * name );

/* List of Tags */

/**
 * @brief Retrieve the first tag in the list
 *
 * This is a simple macro which can be used to get the first tag
 * node structure in the list. Note that if the list is empty this
 * will be the @c NULL value. To retrieve the tag from this node
 * use @c lcfgtaglist_tag()
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 *
 * @return Pointer to first @c LCFGTagNode structure in list
 *
 */

#define lcfgtaglist_head(taglist) ((taglist)->head)

/**
 * @brief Retrieve the last tag in the list
 *
 * This is a simple macro which can be used to get the last tag
 * node structure in the list. Note that if the list is empty this
 * will be the @c NULL value. To retrieve the tag from this node
 * use @c lcfgtaglist_tag()
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 *
 * @return Pointer to last @c LCFGTagNode structure in list
 *
 */

#define lcfgtaglist_tail(taglist) ((taglist)->tail)

/**
 * @brief Get the number of items in the tag list
 *
 * This is a simple macro which can be used to get the length of the
 * doubly-linked tag list.
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 *
 * @return Integer length of the tag list
 *
 */

#define lcfgtaglist_size(taglist) ((taglist)->size)

/**
 * @brief Test if the tag list is empty
 *
 * This is a simple macro which can be used to test if the
 * doubly-linked tag list contains any items.
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 *
 * @return Boolean which indicates whether the list contains any items
 *
 */

#define lcfgtaglist_is_empty(taglist) ( taglist == NULL || (taglist)->size == 0)

/**
 * @brief Retrieve the next tag node in the list
 *
 * This is a simple macro which can be used to fetch the next node in
 * the doubly-linked tag list for a given node. If the node
 * specified is the final item in the list this will return a @c NULL
 * value.
 *
 * @param[in] tagnode Pointer to current @c LCFGTagNode
 *
 * @return Pointer to next @c LCFGTagNode
 */

#define lcfgtaglist_next(tagnode) ((tagnode)->next)

/**
 * @brief Retrieve the previous tag node in the list
 *
 * This is a simple macro which can be used to fetch the previous node
 * in the doubly-linked tag list for a given node. If the node
 * specified is the first item in the list this will return a @c NULL
 * value.
 *
 * @param[in] tagnode Pointer to current @c LCFGTagNode
 *
 * @return Pointer to previous @c LCFGTagNode
 */

#define lcfgtaglist_prev(tagnode) ((tagnode)->prev)

/**
 * @brief Retrieve the tag from a list node
 *
 * This is a simple macro which can be used to get the tag
 * structure from the specified node. 
 *
 * Note that this does @b NOT increment the reference count for the
 * returned tag structure. To retain the tag call the
 * @c lcfgtag_acquire() function.
 *
 * @param[in] tagnode Pointer to @c LCFGTagNode
 *
 * @return Pointer to @c LCFGTag structure
 *
 */

#define lcfgtaglist_tag(tagnode)  ((tagnode)->tag)

/**
 * @brief Append a tag to a list
 *
 * This is a simple macro wrapper around the
 * @c lcfgtaglist_insert_next() function which can be used to append
 * a tag structure on to the end of the specified tag list.
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 * @param[in] tag Pointer to @c LCFGTag
 * 
 * @return Integer value indicating type of change
 *
 */

#define lcfgtaglist_append_tag(taglist, tag) ( lcfgtaglist_insert_next( taglist, lcfgtaglist_tail(taglist), tag ) )

/**
 * @brief Prepend a tag to a list
 *
 * This is a simple macro wrapper around the
 * @c lcfgtaglist_insert_next() function which can be used to prepend
 * a tag structure on to the start of the specified tag list.
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 * @param[in] tag Pointer to @c LCFGTag
 * 
 * @return Integer value indicating type of change
 *
 */

#define lcfgtaglist_prepend_tag(taglist, tag) ( lcfgtaglist_insert_next( taglist, NULL, tag ) )

/**
 * @brief A structure to wrap an LCFG tag as a doubly-linked list item
 */

struct LCFGTagNode {
  LCFGTag * tag;             /**< Pointer to the tag structure */
  struct LCFGTagNode * prev; /**< Previous item in the tag list */
  struct LCFGTagNode * next; /**< Next item in the tag list */
};

typedef struct LCFGTagNode LCFGTagNode;

LCFGTagNode * lcfgtagnode_new(LCFGTag * tag);
void lcfgtagnode_destroy(LCFGTagNode * tagnode);

/**
 * @brief A structure for storing LCFG tags as a doubly-linked list
 */

struct LCFGTagList {
  /*@{*/
  LCFGTagNode * head; /**< The first tag in the list */
  LCFGTagNode * tail; /**< The last tag in the list */
  unsigned int size;  /**< The length of the list */
  /*@}*/
  unsigned int _refcount;
};
typedef struct LCFGTagList LCFGTagList;

LCFGTagList * lcfgtaglist_new(void);
void lcfgtaglist_destroy(LCFGTagList * taglist);
void lcfgtaglist_acquire( LCFGTagList * taglist );
void lcfgtaglist_relinquish( LCFGTagList * taglist );

LCFGTagList * lcfgtaglist_clone(const LCFGTagList * taglist);

LCFGChange lcfgtaglist_insert_next( LCFGTagList * taglist,
                                    LCFGTagNode * tagnode,
                                    LCFGTag * tag )
  __attribute__((warn_unused_result));

LCFGChange lcfgtaglist_remove_tag( LCFGTagList * taglist,
                                   LCFGTagNode * tagnode,
                                   LCFGTag ** tag )
  __attribute__((warn_unused_result));

LCFGTagNode * lcfgtaglist_find_node( const LCFGTagList * taglist,
                                     const char * name );

LCFGTag * lcfgtaglist_find_tag( const LCFGTagList * taglist,
                                const char * name );

bool lcfgtaglist_contains( const LCFGTagList * taglist,
                           const char * name );

LCFGStatus lcfgtaglist_from_string( const char * input,
                                    LCFGTagList ** result,
                                    char ** msg )
  __attribute__((warn_unused_result));

ssize_t lcfgtaglist_to_string( const LCFGTagList * taglist,
                               unsigned int options,
                               char ** result, size_t * size )
  __attribute__((warn_unused_result));

bool lcfgtaglist_print( const LCFGTagList * taglist, FILE * out )
  __attribute__((warn_unused_result));

void lcfgtaglist_sort( LCFGTagList * taglist );

/* Mutators */

LCFGChange lcfgtaglist_mutate_append( LCFGTagList * taglist,
                                      const char * tagname,
                                      char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgtaglist_mutate_prepend( LCFGTagList * taglist,
                                      const char * tagname,
                                      char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgtaglist_mutate_add( LCFGTagList * taglist,
				   const char * tagname,
                                   char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgtaglist_mutate_replace( LCFGTagList * taglist,
				       const char * old_name,
				       const char * new_name,
				       bool global,
				       char ** msg )
  __attribute__((warn_unused_result));

/**
 * @brief Simple iterator for tag lists
 */

struct LCFGTagIterator {
  LCFGTagList * taglist;     /**< The tag list */
  LCFGTagNode * current;     /**< Current location in the tag list */
};
typedef struct LCFGTagIterator LCFGTagIterator;

LCFGTagIterator * lcfgtagiter_new( LCFGTagList * taglist );

void lcfgtagiter_destroy( LCFGTagIterator * iterator );

void lcfgtagiter_reset( LCFGTagIterator * iterator );

bool lcfgtagiter_has_next( LCFGTagIterator * iterator );

LCFGTag * lcfgtagiter_next(LCFGTagIterator * iterator);

bool lcfgtagiter_has_prev( LCFGTagIterator * iterator );

LCFGTag * lcfgtagiter_prev(LCFGTagIterator * iterator);

#endif /* LCFG_CORE_TAGS_H */

/* eof */
