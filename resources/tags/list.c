/**
 * @file resources/tags/list.c
 * @brief Functions for working with lists of LCFG resource tags
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "utils.h"
#include "tags.h"

/**
 * @brief Create and initialise a new tag list node
 *
 * This creates a simple wrapper @c LCFGTagNode node which
 * is used to hold a pointer to an @c LCFGTag as an item in a
 * doubly-linked @c LCFGTagList data structure.
 *
 * It is typically not necessary to call this function. The usual
 * approach is to use the @c lcfgtaglist_insert_next() or @c
 * lcfgtaglist_append() functions to add @c LCFGTag structures to
 * the list.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * To avoid memory leaks, when the new structure is no longer required
 * the @c lcfgtagnode_destroy() function should be called.
 *
 * @param[in] tag Pointer to @c LCFGTag
 *
 * @return Pointer to new @c LCFGTagNode
 *
 */

LCFGTagNode * lcfgtagnode_new(LCFGTag * tag) {

  LCFGTagNode * tagnode = malloc( sizeof(LCFGTagNode) );
  if ( tagnode == NULL ) {
    perror( "Failed to allocate memory for LCFG tag node" );
    exit(EXIT_FAILURE);
  }

  tagnode->tag  = tag;
  tagnode->prev = NULL;
  tagnode->next = NULL;

  return tagnode;
}

/**
 * @brief Destroy a tag list node
 *
 * When the specified @c LCFGTagNode is no longer required this
 * can be used to free all associated memory. This will call
 * @c free(3) on each parameter of the struct and then set each value to
 * be @c NULL.
 *
 * Note that destroying an @c LCFGTagNode does not destroy the
 * associated @c LCFGTag, that must be done separately.
 *
 * It is typically not necessary to call this function. The usual
 * approach is to use the @c lcfgtaglist_remove_tag() function to
 * remove a @c LCFGTag from the list.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * tag node which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] tagnode Pointer to @c LCFGTagNode to be destroyed.
 *
 */

void lcfgtagnode_destroy(LCFGTagNode * tagnode) {

  if ( tagnode == NULL ) return;

  tagnode->tag  = NULL;
  tagnode->prev = NULL;
  tagnode->next = NULL;

  free(tagnode);
  tagnode = NULL;

}

/**
 * @brief Create and initialise a new empty tag list
 *
 * Creates a new @c LCFGTagList which represents an empty
 * tag list.
 *
 * If the memory allocation for the new structure is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgtaglist_relinquish() function should be called.
 *
 * @return Pointer to new @c LCFGTagList
 *
 */

LCFGTagList * lcfgtaglist_new(void) {

  LCFGTagList * taglist = malloc( sizeof(LCFGTagList) );
  if ( taglist == NULL ) {
    perror( "Failed to allocate memory for LCFG tag list" );
    exit(EXIT_FAILURE);
  }

  taglist->head        = NULL;
  taglist->tail        = NULL;
  taglist->size        = 0;
  taglist->_refcount   = 1;

  return taglist;
}

/**
 * @brief Destroy the tag list
 *
 * When the specified @c LCFGTagList is no longer required this
 * will free all associated memory.
 *
 * *Reference Counting:* There is support for very simple reference
 * counting which allows an @c LCFGTagList to appear in multiple
 * situations. This is particular useful for code which needs to use
 * multiple iterators for a single list. Incrementing and decrementing
 * that reference counter is the responsibility of the container
 * code. See @c lcfgtaglist_acquire() and @c lcfgtaglist_relinquish()
 * for details.
 *
 * This will iterate through the list to remove and destroy each
 * @c LCFGTagNode item, it also calls @c lcfgtag_relinquish()
 * for each tag. Note that if the reference count on the tag
 * reaches zero then the @c LCFGTag will also be destroyed.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * tag list which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] taglist Pointer to @c LCFGTagList to be destroyed.
 *
 */

void lcfgtaglist_destroy(LCFGTagList * taglist) {

  if ( taglist == NULL ) return;

  while ( lcfgtaglist_size(taglist) > 0 ) {
    LCFGTag * tag = NULL;
    if ( lcfgtaglist_remove_tag( taglist, lcfgtaglist_tail(taglist), &tag )
         == LCFG_CHANGE_REMOVED ) {
      lcfgtag_relinquish(tag);
    }
  }

  free(taglist);
  taglist = NULL;

}

/**
 * @brief Acquire reference to tag list
 *
 * This is used to record a reference to the @c LCFGTagList, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgtaglist_release() function should be
 * called.
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 *
 */

void lcfgtaglist_acquire( LCFGTagList * taglist ) {
  assert( taglist != NULL );

  taglist->_refcount += 1;
}

/**
 * @brief Release reference to tag list
 *
 * This is used to release a reference to the @c LCFGTagList,
 * it does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgtaglist_destroy() function
 * will be called to clean up the memory associated with the structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * tag list which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 *
 */

void lcfgtaglist_relinquish( LCFGTagList * taglist ) {

  if ( taglist == NULL ) return;

  if ( taglist->_refcount > 0 )
    taglist->_refcount -= 1;

  if ( taglist->_refcount == 0 )
    lcfgtaglist_destroy(taglist);

}

/**
 * @brief Insert a tag into the list
 *
 * This can be used to insert an @c LCFGTag into the
 * specified tag list. The tag will be wrapped into an
 * @c LCFGTagNode using the @c lcfgtagnode_new() function.
 *
 * The tag will be inserted into the list immediately after the
 * specified @c LCFGTagNode. To insert the tag at the head of the list
 * the @c NULL value should be passed for the node. For convenience
 * the @c lcfgtaglist_append() and @c lcfgtaglist_prepend() macros are
 * provided.
 *
 * If the tag is successfully inserted into the list the
 * @c LCFG_CHANGE_ADDED value is returned, if an error occurs then
 * @c LCFG_CHANGE_ERROR is returned.
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 * @param[in] tagnode Pointer to @c LCFGTagNode
 * @param[in] tag Pointer to @c LCFGTag
 * 
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgtaglist_insert_next( LCFGTagList * taglist,
                                    LCFGTagNode * tagnode,
                                    LCFGTag * tag ) {
  assert( taglist != NULL );

  /* Forbid the addition of bad tags */
  if ( !lcfgtag_is_valid(tag) ) return LCFG_CHANGE_ERROR;

  LCFGTagNode * new_node = lcfgtagnode_new(tag);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgtag_acquire(tag);

  if ( tagnode == NULL ) { /* HEAD */

    LCFGTagNode * cur_head = lcfgtaglist_head(taglist);

    new_node->prev = NULL;
    new_node->next = cur_head;

    if ( cur_head != NULL )
      cur_head->prev = new_node;

    taglist->head = new_node;

    if ( lcfgtaglist_is_empty(taglist) )
      taglist->tail = new_node;

  } else {

    new_node->prev = tagnode;
    new_node->next = tagnode->next;

    if ( new_node->next == NULL )
      taglist->tail = new_node;
    else
      tagnode->next->prev = new_node;

    tagnode->next = new_node;
  }

  taglist->size++;

  return LCFG_CHANGE_ADDED;
}

/**
 * @brief Remove a tag from the list
 *
 * This can be used to remove an @c LCFGTag from the specified
 * tag list.
 *
 * If the tag is successfully removed from the list the
 * @c LCFG_CHANGE_REMOVED value is returned, if an error occurs then
 * @c LCFG_CHANGE_ERROR is returned. If the list is already empty then
 * the @c LCFG_CHANGE_NONE value is returned.
 *
 * Note that, since a pointer to the @c LCFGTag is returned
 * to the caller, the reference count will still be at least 1. To
 * avoid memory leaks, when the struct is no longer required it should
 * be released by calling @c lcfgtag_relinquish().
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 * @param[in] tagnode Pointer to @c LCFGTagNode
 * @param[out] tag Pointer to @c LCFGTag
 * 
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgtaglist_remove_tag( LCFGTagList * taglist,
                                   LCFGTagNode * tagnode,
                                   LCFGTag ** tag ) {
  assert( taglist != NULL );

  if ( lcfgtaglist_is_empty(taglist) ) return LCFG_CHANGE_NONE;

  LCFGTagNode * cur_head = lcfgtaglist_head(taglist);
  if ( tagnode == NULL )
    tagnode = cur_head;

  if ( tagnode == cur_head ) {

    taglist->head = tagnode->next;

    if ( taglist->head == NULL )
      taglist->tail = NULL;
    else
      taglist->head->prev = NULL;

  } else {

    tagnode->prev->next = tagnode->next;

    if ( tagnode->next == NULL )
      taglist->tail = tagnode->prev;
    else
      tagnode->next->prev = tagnode->prev;

  }

  taglist->size--;

  *tag = lcfgtaglist_tag(tagnode);

  lcfgtagnode_destroy(tagnode);

  return LCFG_CHANGE_REMOVED;
}

/**
 * @brief Find the tag node with a given name
 *
 * This can be used to search through an @c LCFGTagList to find
 * the first tag node which has a matching name. Note that the
 * matching is done using strcmp(3) which is case-sensitive.
 *
 * A @c NULL value is returned if no matching node is found. Also, a
 * @c NULL value is returned if a @c NULL value or an empty tag list
 * is specified.
 *
 * @param[in] taglist Pointer to @c LCFGTagList to be searched
 * @param[in] want_name The name of the required tag node
 *
 * @return Pointer to an @c LCFGTagNode (or the @c NULL value).
 *
 */

LCFGTagNode * lcfgtaglist_find_node( const LCFGTagList * taglist,
                                     const char * want_name ) {
  assert( want_name != NULL );

  if ( lcfgtaglist_is_empty(taglist) ) return NULL;

  LCFGTagNode * result = NULL;

  const LCFGTagNode * cur_node = NULL;
  for ( cur_node = lcfgtaglist_head(taglist);
        cur_node != NULL && result == NULL;
        cur_node = lcfgtaglist_next(cur_node) ) {

    const LCFGTag * tag = lcfgtaglist_tag(cur_node);

    if ( lcfgtag_match(tag, want_name) ) 
      result = (LCFGTagNode *) cur_node;
  }

  return result;
}

/**
 * @brief Find the tag for a given name
 *
 * This can be used to search through an @c LCFGTagList to find
 * the first tag which has a matching name. Note
 * that the matching is done using strcmp(3) which is case-sensitive.
 *
 * To ensure the returned @c LCFGTag is not destroyed when
 * the parent @c LCFGTagList is destroyed you would need to
 * call the @c lcfgtag_acquire() function.
 *
 * @param[in] taglist Pointer to @c LCFGTagList to be searched
 * @param[in] name The name of the required tag
 *
 * @return Pointer to an @c LCFGTag (or the @c NULL value).
 *
 */

LCFGTag * lcfgtaglist_find_tag( const LCFGTagList * taglist,
                                const char * name ) {
  assert( name != NULL );

  LCFGTag * tag = NULL;

  const LCFGTagNode * tagnode = lcfgtaglist_find_node( taglist, name );
  if ( tagnode != NULL )
    tag = lcfgtaglist_tag(tagnode);

  return tag;
}

/**
 * @brief Check if a tag list contains a particular tag name
 *
 * This can be used to search through an @c LCFGTagList to check if
 * it contains a tag with a matching name. Note that the matching
 * is done using strcmp(3) which is case-sensitive.
 * 
 * This uses the @c lcfgtaglist_find_node() function to find the
 * relevant node. If a @c NULL value is specified for the list or the
 * list is empty then a false value will be returned.
 *
 * @param[in] taglist Pointer to @c LCFGTagList to be searched
 * @param[in] name The name of the required tag
 *
 * @return Boolean value which indicates presence of tag in list
 *
 */

bool lcfgtaglist_contains( const LCFGTagList * taglist,
                           const char * name ) {
  assert( name != NULL );

  return ( lcfgtaglist_find_node( taglist, name ) != NULL );
}

/**
 * @brief Clone the tag list
 *
 * Creates a new @c LCFGTagList with the same list of tags.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * @param[in] taglist Pointer to @c LCFGTagList to be cloned.
 *
 * @return Pointer to new @c LCFGTagList or @c NULL if copy fails.
 *
 */

LCFGTagList * lcfgtaglist_clone(const LCFGTagList * taglist) {
  assert( taglist != NULL );

  bool ok = true;

  LCFGTagList * new_taglist = lcfgtaglist_new();

  const LCFGTagNode * cur_node = lcfgtaglist_head(taglist);
  for ( cur_node = lcfgtaglist_head(taglist);
        cur_node != NULL && ok;
        cur_node = lcfgtaglist_next(cur_node) ) {

    LCFGTag * cur_tag = lcfgtaglist_tag(cur_node);

    if ( lcfgtag_is_valid(cur_tag) ) {
      LCFGChange change = lcfgtaglist_append_tag( new_taglist, cur_tag );
      if ( change == LCFG_CHANGE_ERROR )
        ok = false;
    }

  }

  if ( !ok ) {
    lcfgtaglist_destroy(new_taglist);
    new_taglist = NULL;
  }

  return new_taglist;
}

/**
 * @brief Create a new tag list from an array of strings
 *
 * This will create a new @c LCFGTagList using the contents of the
 * array of strings. The tags are added to the list in the order in
 * which they appear in the array. If the array is empty an empty tag
 * list will be returned. The final element of the array MUST be a @c
 * NULL value.
 *
 * The tokens must be valid tag names according to the
 * @c lcfgresource_valid_tag() function.
 *
 * @param[in] input The array of tag names
 * @param[out] result Reference to pointer to the new @c LCFGTagList
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgtaglist_from_array( const char ** input,
                                   LCFGTagList ** result,
                                   char ** msg ) {

  LCFGStatus status = LCFG_STATUS_OK;

  LCFGTagList * new_taglist = lcfgtaglist_new();

  int i;
  for ( i=0; input[i] != NULL; i++ ) {

    LCFGChange change = lcfgtaglist_mutate_append( new_taglist, input[i], msg );
    if ( change == LCFG_CHANGE_ERROR ) {
      status = LCFG_STATUS_ERROR;
      break;
    }

  }

  if ( status == LCFG_STATUS_ERROR ) {
    lcfgtaglist_relinquish(new_taglist);
    new_taglist = NULL;
  }

  *result = new_taglist;

  return status;
}

static const char * tag_seps = " \t\r\n";

/**
 * @brief Create a new tag list from a string
 *
 * This splits the specified string on whitespace and creates a new
 * @c LCFGTag for each token found. The tags are added to the list in the
 * order in which they appear in the string. If the string is empty an
 * empty tag list will be returned.
 *
 * The tokens must be valid tag names according to the
 * @c lcfgresource_valid_tag() function.
 *
 * @param[in] input The string of tag names
 * @param[out] result Reference to pointer to the new @c LCFGTagList
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgtaglist_from_string( const char * input,
                                    LCFGTagList ** result,
                                    char ** msg ) {
  assert( input != NULL );

  *result = NULL;

  if ( input == NULL ) {
    lcfgutils_build_message( msg, "Invalid taglist" );
    return LCFG_STATUS_ERROR;
  }

  LCFGTagList * new_taglist = lcfgtaglist_new();

  char * remainder = strdup(input);
  if ( remainder == NULL ) {
    perror( "Failed to allocate memory for LCFG tag list" );
    exit(EXIT_FAILURE);
  }

  char * saveptr = NULL;

  bool ok = true;

  char * token = strtok_r( remainder, tag_seps, &saveptr );
  while (token) {

    LCFGChange change = lcfgtaglist_mutate_append( new_taglist, token, msg );
    if ( change == LCFG_CHANGE_ERROR ) {
        ok = false;
        break;
    }

    token = strtok_r( NULL, tag_seps, &saveptr );
  }

  free(remainder);

  if ( !ok ) {
    lcfgtaglist_relinquish(new_taglist);
    new_taglist = NULL;
  }

  *result = new_taglist;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/**
 * @brief Serialise the list of tags as a string
 *
 * This generates a new string representation of the @c LCFGTagList by
 * concatenating together all the @c LCFGTag names using a
 * single-space separator (e.g. @c "foo bar baz quux"). To add a final
 * newline character specify the @c LCFG_OPT_NEWLINE option.
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many resource strings, this can be a
 * huge performance benefit. If the buffer is initially unallocated
 * then it MUST be set to @c NULL. The current size of the buffer must
 * be passed and should be specified as zero if the buffer is
 * initially unallocated. If the generated string would be too long
 * for the current buffer then it will be resized and the size
 * parameter is updated.
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] taglist Pointer to @c LCFGTagList to be cloned.
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgtaglist_to_string( const LCFGTagList * taglist,
                               unsigned int options,
                               char ** result, size_t * size ) {

  /* Calculate the required space */

  
  size_t new_len = 0;

  const LCFGTagNode * head_node = lcfgtaglist_head(taglist);

  const LCFGTagNode * cur_node = NULL;
  const LCFGTag * cur_tag      = NULL;

  if ( lcfgtaglist_size(taglist) > 0 ) {

    cur_tag = lcfgtaglist_tag(head_node);
    new_len += lcfgtag_get_length(cur_tag);

    for ( cur_node = lcfgtaglist_next(head_node);
          cur_node != NULL;
          cur_node = lcfgtaglist_next(cur_node) ) {

      cur_tag = lcfgtaglist_tag(cur_node);
      new_len += ( lcfgtag_get_length(cur_tag) + 1 ); /* +1 for single space */
    }

  }

  /* Optional newline at end of string */
  if ( options&LCFG_OPT_NEWLINE )
    new_len += 1;

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    *size = new_len + 1;

    *result = realloc( *result, ( *size * sizeof(char) ) );
    if ( *result == NULL ) {
      perror("Failed to allocate memory for LCFG tag string");
      exit(EXIT_FAILURE);
    }

  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  if ( lcfgtaglist_size(taglist) > 0 ) {

    cur_tag = lcfgtaglist_tag(head_node);
    const char * name = lcfgtag_get_name(cur_tag);
    size_t name_len   = lcfgtag_get_length(cur_tag);

    to = stpncpy( to, name, name_len );

    for ( cur_node = lcfgtaglist_next(head_node);
          cur_node != NULL;
          cur_node = lcfgtaglist_next(cur_node) ) {

      cur_tag = lcfgtaglist_tag(cur_node);

      name     = lcfgtag_get_name(cur_tag);
      name_len = lcfgtag_get_length(cur_tag);

      *to = ' ';
      to++;

      to = stpncpy( to, name, name_len );
    }

  }

  /* Optional newline at end of string */
  if ( options&LCFG_OPT_NEWLINE )
    to = stpcpy( to, "\n" );

  *to = '\0';

  assert( (*result + new_len) == to );

  return new_len;
}

/**
 * Write the list of tags to file stream
 *
 * This serialises the list of tags using the
 * @c lcfgtaglist_to_string() function and then writes out the generated
 * string to the specified file stream.
 *
 * @param[in] taglist Pointer to @c LCFGTagList to be searched
 * @param[in] out Stream to which the list of tags should be written
 *
 * @return Boolean indicating success
 */

bool lcfgtaglist_print( const LCFGTagList * taglist, FILE * out ) {

  size_t buf_size = 0;
  char * str_buf  = NULL;

  bool ok = true;

  if ( lcfgtaglist_to_string( taglist, LCFG_OPT_NEWLINE,
                              &str_buf, &buf_size ) < 0 ) {
    ok = false;
  }

  if ( ok ) {

    if ( fputs( str_buf, out ) < 0 )
      ok = false;

  }

  free(str_buf);

  return ok;
}

/**
 * @brief Sort the list of tags by name
 *
 * This sorts the nodes of the list of tags for an @c LCFGTag by using
 * the @c lcfgtag_compare() function.
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 *
 */

void lcfgtaglist_sort( LCFGTagList * taglist ) {

  if ( lcfgtaglist_size(taglist) < 2 ) return;

  /* Oo. Oo. bubble sort .oO .oO */

  bool swapped=true;

  while (swapped) {
    swapped=false;

    LCFGTagNode * cur_node = NULL;
    for ( cur_node = lcfgtaglist_head(taglist);
          cur_node != NULL && cur_node->next != NULL;
          cur_node = lcfgtaglist_next(cur_node) ) {

      LCFGTag * cur_tag  = lcfgtaglist_tag(cur_node);
      LCFGTag * next_tag = lcfgtaglist_tag(cur_node->next);

      if ( lcfgtag_compare( cur_tag, next_tag ) > 0 ) {
        cur_node->tag       = next_tag;
        cur_node->next->tag = cur_tag;
        swapped = true;
      }

    }
  }

}

/**
 * @brief Mutator: Replace tags in list
 *
 * This will replace tags in the list which have the @c old_name with
 * the required @c new_name. There is the option to replace a single
 * tag or all tags with the name. Note that the new name must be a
 * valid tag name.
 *
 * If any tags are replaced the @c LCFG_CHANGE_REPLACED value will be
 * returned, if there is no change then @c LCFG_CHANGE_NONE will be
 * returned. If an error occurs the @ LCFG_CHANGE_ERROR will be
 * returned along with a diagnostic message.
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 * @param[in] match The tag name which will be replaced
 * @param[in] replace The replacement tag name
 * @param[in] global Boolean which controls whether to replace single or all tags.
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgtaglist_mutate_replace( LCFGTagList * taglist,
				       const char * match,
				       const char * replace,
				       bool global,
				       char ** msg ) {
  assert( taglist != NULL );
  assert( match != NULL );
  assert( replace != NULL );

  if ( lcfgtaglist_is_empty(taglist) ) return LCFG_CHANGE_NONE;

  if ( !lcfgresource_valid_tag(replace) ) {
    lcfgutils_build_message( msg, "Invalid replacement tag '%s'", replace );
    return LCFG_CHANGE_ERROR;
  }

  LCFGChange change = LCFG_CHANGE_NONE;

  LCFGTag * new_tag = NULL;

  LCFGTagNode * cur_node = NULL;
  for ( cur_node = lcfgtaglist_head(taglist);
        cur_node != NULL && change != LCFG_CHANGE_ERROR;
        cur_node = lcfgtaglist_next(cur_node) ) {

    LCFGTag * cur_tag = lcfgtaglist_tag(cur_node);

    if ( lcfgtag_match( cur_tag, match ) ) {

      /* Only create the tag when it is required */

      if ( new_tag == NULL ) {
        LCFGStatus rc = lcfgtag_from_string( replace, &new_tag, msg );
        if ( rc == LCFG_STATUS_ERROR )
          change = LCFG_CHANGE_ERROR;
      }

      if ( change != LCFG_CHANGE_ERROR ) {
	lcfgtag_acquire(new_tag);
	cur_node->tag = new_tag;

	lcfgtag_relinquish(cur_tag);
	change = LCFG_CHANGE_REPLACED;
      }

      if (!global) break;
    }

  }

  lcfgtag_relinquish(new_tag);

  return change;
}

/**
 * @brief Mutator: Append tag to list
 *
 * This will append an @c LCFGTag with the specified name to the
 * @c LCFGTagList. Note that the list does NOT take "ownership" of the
 * string for the specified name.
 *
 * If the tag is successfully appended the @c LCFG_CHANGE_ADDED value
 * will be returned, if an error occurs the @c LCFG_CHANGE_ERROR will
 * be returned along with a diagnostic message.
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 * @param[in] tagname The tag name which will be appended
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgtaglist_mutate_append( LCFGTagList * taglist,
                                      const char * tagname,
                                      char ** msg ) {
  LCFGChange change;

  LCFGTag * new_tag = NULL;

  LCFGStatus rc = lcfgtag_from_string( tagname, &new_tag, msg );
  if ( rc == LCFG_STATUS_ERROR )
    change = LCFG_CHANGE_ERROR;
  else
    change = lcfgtaglist_append_tag( taglist, new_tag );

  lcfgtag_relinquish(new_tag);

  return change;
}

/**
 * @brief Mutator: Prepend tag to list
 *
 * This will prepend an @c LCFGTag with the specified name to the
 * @c LCFGTagList. Note that the list does NOT take "ownership" of the
 * string for the specified name.
 *
 * If the tag is successfully prepended the @c LCFG_CHANGE_ADDED value
 * will be returned, if an error occurs the @c LCFG_CHANGE_ERROR will
 * be returned along with a diagnostic message.
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 * @param[in] tagname The tag name which will be prepended
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgtaglist_mutate_prepend( LCFGTagList * taglist,
				       const char * tagname,
				       char ** msg ) {
  LCFGChange change;

  LCFGTag * new_tag = NULL;

  LCFGStatus rc = lcfgtag_from_string( tagname, &new_tag, msg );
  if ( rc == LCFG_STATUS_ERROR )
    change = LCFG_CHANGE_ERROR;
  else
    change = lcfgtaglist_prepend_tag( taglist, new_tag );

  lcfgtag_relinquish(new_tag);

  return change;
}

/**
 * @brief Mutator: Append tag to list if not already present
 *
 * This will append an @c LCFGTag with the specified name to the
 * @c LCFGTagList if the list does not already contain a tag with the
 * same name. Note that the list does NOT take "ownership" of the
 * string for the specified name.
 *
 * If the tag is successfully prepended the @c LCFG_CHANGE_ADDED value
 * will be returned, if an error occurs the @c LCFG_CHANGE_ERROR will
 * be returned along with a diagnostic message.
 *
 * @param[in] taglist Pointer to @c LCFGTagList
 * @param[in] tagname The tag name which will be added
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgtaglist_mutate_add( LCFGTagList * taglist,
				   const char * tagname,
                                   char ** msg ) {

  LCFGChange change = LCFG_CHANGE_NONE;
  if ( !lcfgtaglist_contains( taglist, tagname ) )
    change = lcfgtaglist_mutate_append( taglist, tagname, msg );

  return change;
}

LCFGTagList * lcfgtaglist_set_unique( const LCFGTagList * taglist ) {
  assert( taglist1 != NULL );

  LCFGChange change = LCFG_CHANGE_NONE;

  LCFGTagList * result = lcfgtaglist_new();

  const LCFGTagNode * cur_node = NULL;
  for ( cur_node = lcfgtaglist_head(taglist);
        cur_node != NULL && change != LCFG_CHANGE_ERROR;
        cur_node = lcfgtaglist_next(cur_node) ) {

    LCFGTag * cur_tag = lcfgtaglist_tag(cur_node);

    const char * name = NULL;
    if ( lcfgtag_is_valid(cur_tag) )
      name = lcfgtag_get_name(cur_tag);

    if ( name != NULL && !lcfgtaglist_contains( result, name ) )
      change = lcfgtaglist_append_tag( result, cur_tag );

  }

  if ( change == LCFG_CHANGE_ERROR ) {
    lcfgtaglist_relinquish(result);
    result = NULL;
  }

  return result;
}

LCFGTagList * lcfgtaglist_set_union( const LCFGTagList * taglist1,
                                     const LCFGTagList * taglist2 ) {
  assert( taglist1 != NULL );
  assert( taglist2 != NULL );

  LCFGChange change = LCFG_CHANGE_NONE;

  /* Start by making a copy of the unique set of tags in the first list */

  LCFGTagList * result;
  if ( lcfgtaglist_is_empty(taglist1) )
    result = lcfgtaglist_new();
  else
    result = lcfgtaglist_set_unique(taglist1);

  /* Copy in any tags from the second list which are not already present */

  const LCFGTagNode * cur_node = NULL;
  for ( cur_node = lcfgtaglist_head(taglist2);
        cur_node != NULL && change != LCFG_CHANGE_ERROR;
        cur_node = lcfgtaglist_next(cur_node) ) {

    const LCFGTag * cur_tag = lcfgtaglist_tag(cur_node);

    const char * name = NULL;
    if ( lcfgtag_is_valid(cur_tag) )
      name = lcfgtag_get_name(cur_tag);

    if ( name != NULL && !lcfgtaglist_contains( result, name ) )
      change = lcfgtaglist_append_tag( result, cur_tag );

  }

  if ( change == LCFG_CHANGE_ERROR ) {
    lcfgtaglist_relinquish(result);
    result = NULL;
  }

  return result;
}

LCFGTagList * lcfgtaglist_set_intersection( const LCFGTagList * taglist1,
                                            const LCFGTagList * taglist2 ) {
  assert( taglist1 != NULL );
  assert( taglist2 != NULL );

  LCFGChange change = LCFG_CHANGE_NONE;

  LCFGTagList * result = lcfgtaglist_new();

  const LCFGTagNode * cur_node = NULL;
  for ( cur_node = lcfgtaglist_head(taglist1);
        cur_node != NULL && change != LCFG_CHANGE_ERROR;
        cur_node = lcfgtaglist_next(cur_node) ) {

    LCFGTag * cur_tag = lcfgtaglist_tag(cur_node);

    const char * name = NULL;
    if ( lcfgtag_is_valid(cur_tag) )
      name = lcfgtag_get_name(cur_tag);

    if ( name != NULL &&
         lcfgtaglist_contains( taglist2, name ) &&
         !lcfgtaglist_contains( result, cur_tag ) )
      change = lcfgtaglist_append_tag( result, cur_tag );
  }

  if ( change == LCFG_CHANGE_ERROR ) {
    lcfgtaglist_relinquish(result);
    result = NULL;
  }

  return result;
}

LCFGTagList * lcfgtaglist_set_subtract( const LCFGTagList * taglist1,
                                        const LCFGTagList * taglist2 ) {
  assert( taglist1 != NULL );
  assert( taglist2 != NULL );

  LCFGChange change = LCFG_CHANGE_NONE;

  LCFGTagList * result = lcfgtaglist_new();

  const LCFGTagNode * cur_node = NULL;
  for ( cur_node = lcfgtaglist_head(taglist1);
        cur_node != NULL && change != LCFG_CHANGE_ERROR;
        cur_node = lcfgtaglist_next(cur_node) ) {

    LCFGTag * cur_tag = lcfgtaglist_tag(cur_node);

    const char * name = NULL;
    if ( lcfgtag_is_valid(cur_tag) )
      name = lcfgtag_get_name(cur_tag);

    if ( name != NULL &&
         !lcfgtaglist_contains( taglist2, name ) &&
         !lcfgtaglist_contains( result, cur_tag ) )
      change = lcfgtaglist_append_tag( result, cur_tag );

  }

  if ( change == LCFG_CHANGE_ERROR ) {
    lcfgtaglist_relinquish(result);
    result = NULL;
  }

  return result;
}

/* eof */
