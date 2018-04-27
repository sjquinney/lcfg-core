/**
 * @file common/derivation/list.c
 * @brief Functions for working with lists of LCFG derivations
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2018 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common.h"
#include "utils.h"
#include "derivation.h"

static LCFGChange lcfgderivlist_insert_next( LCFGDerivationList * list,
                                             LCFGSListNode      * node,
                                             LCFGDerivation     * item )
  __attribute__((warn_unused_result));

static LCFGChange lcfgderivlist_remove_next( LCFGDerivationList * list,
                                             LCFGSListNode      * node,
                                             LCFGDerivation    ** item )
  __attribute__((warn_unused_result));

#define lcfgderivlist_append(drvlist, drv) ( lcfgderivlist_insert_next( drvlist, lcfgslist_tail(drvlist), drv ) )

/**
 * @brief Create and initialise a new derivation list
 *
 * Creates a new @c LCFGDerivationList and initialises the parameters
 * to the default values.
 *
 * This is used to represent all the derivation information for an
 * LCFG resource or package which may be spread over multiple files,
 * possibly occurring on multiple lines within those files.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgderivlist_relinquish() function should be called.
 *
 * @return Pointer to new @c LCFGDerivationList
 *
 */

LCFGDerivationList * lcfgderivlist_new(void) {

  LCFGDerivationList * drvlist = calloc( 1, sizeof(LCFGDerivationList) );
  if ( drvlist == NULL ) {
    perror( "Failed to allocate memory for LCFG derivation list" );
    exit(EXIT_FAILURE);
  }

  /* Default values */

  drvlist->size      = 0;
  drvlist->head      = NULL;
  drvlist->tail      = NULL;
  drvlist->id        = 0;
  drvlist->_refcount = 1;

  return drvlist;
}

/**
 * @brief Destroy the derivation list
 *
 * When the specified @c LCFGDerivationList is no longer required this
 * will free all associated memory. There is support for reference
 * counting so typically the @c lcfgderivlist_relinquish() function
 * should be used.
 *
 * This will call @c free() on each parameter of the structure and
 * then set each value to be @c NULL.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * derivation list which has already been destroyed (or potentially
 * was never created).
 *
 * @param[in] drvlist Pointer to @c LCFGDerivationList to be destroyed.
 *
 */

void lcfgderivlist_destroy(LCFGDerivationList * drvlist) {

  if ( drvlist == NULL ) return;

  while ( lcfgslist_size(drvlist) > 0 ) {
    LCFGDerivation * drv = NULL;
    if ( lcfgderivlist_remove_next( drvlist, NULL,
                                    &drv ) == LCFG_CHANGE_REMOVED ) {
      lcfgderivation_relinquish(drv);
    }
  }

  free(drvlist);
  drvlist = NULL;

}

/**
 * @brief Acquire reference to derivation list
 *
 * This is used to record a reference to the @c LCFGDerivationList, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgderivlist_relinquish() function should
 * be called.
 *
 * @param[in] drvlist Pointer to @c LCFGDerivationList
 *
 */

void lcfgderivlist_acquire( LCFGDerivationList * drvlist ) {
  assert( drvlist != NULL );

  drvlist->_refcount += 1;
}

/**
 * @brief Release reference to derivation list
 *
 * This is used to release a reference to the @c LCFGDerivationList,
 * it does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgderivlist_destroy() function
 * will be called to clean up the memory associated with the structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * derivation list which has already been destroyed (or potentially
 * was never created).
 *
 * @param[in] drvlist Pointer to @c LCFGDerivationList
 *
 */

void lcfgderivlist_relinquish( LCFGDerivationList * drvlist ) {

  if ( drvlist == NULL ) return;

  if ( drvlist->_refcount > 0 )
    drvlist->_refcount -= 1;

  if ( drvlist->_refcount == 0 )
    lcfgderivlist_destroy(drvlist);

}

/**
 * @brief Check if there are multiple references to the derivation list
 *
 * The @c LCFGDerivationList structure supports reference counting - see
 * @c lcfgderivlist_acquire() and @c lcfgderivlist_relinquish(). This
 * will return a boolean which indicates if there are multiple
 * references.
 *
 * @param[in] drvlist Pointer to @c LCFGDerivationList
 *
 * @return Boolean which indicates if there are multiple references to this derivation list
 *
 */

bool lcfgderivlist_is_shared( const LCFGDerivationList * drvlist ) {
  return ( drvlist->_refcount > 1 );
}

/**
 * @brief Get the length of the serialised form of the derivation list
 *
 * It is sometimes necessary to know the length of the serialised form
 * of the derivation list. Serialising the @c LCFGDerivationList just
 * to calculate this length would be expensive so this just goes
 * through the motions.
 *
 * @param[in] drvlist Pointer to @c LCFGDerivationList
 *
 * @return Length of serialised form
 *
 */

size_t lcfgderivlist_get_string_length( const LCFGDerivationList * drvlist ) {
  if ( lcfgderivlist_is_empty(drvlist) ) return 0;

  size_t length = 0;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(drvlist);
        cur_node != NULL;
        cur_node = lcfgslist_next(cur_node) ) {

    LCFGDerivation * drv = lcfgslist_data(cur_node);

    ssize_t drvlen = lcfgderivation_get_length(drv);
    if ( drvlen > 0 )
      length += (size_t) drvlen + 1; /* for space sep */
  }

  /* n - 1 space separators */
  if ( length > 0 )
    length -= 1;

  return length;
}

/**
 * @brief Clone the derivation list
 *
 * This can be used to create a new @c LCFGDerivationList structure
 * which contains the same list of @c LCFGDerivation structures as the
 * original. There is support for Copy-On-Write in the various merge
 * functions, if an @c LCFGDerivation structure is shared between
 * multiple lists then it will be cloned before modifications are
 * made.
 *
 * If a @c NULL value is passed for the original then this behaves the
 * same as @c lcfgderivlist_new().
 *
 * @param[in] drvlist Pointer to original @c LCFGDerivationList
 *
 * @return Reference to a new @c LCFGDerivationList which is identical to original
 *
 */

LCFGDerivationList * lcfgderivlist_clone( const LCFGDerivationList * drvlist ) {
  /* This is designed to be safe when called with NULL */

  LCFGDerivationList * clone = lcfgderivlist_new();

  /* Note that this does NOT clone the derivations themselves only the nodes. */
  bool ok = true;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(drvlist);
        cur_node != NULL && ok;
        cur_node = lcfgslist_next(cur_node) ) {

    LCFGDerivation * drv = lcfgslist_data(cur_node);

    LCFGChange rc = lcfgderivlist_append( clone, drv );
    if ( rc != LCFG_CHANGE_ADDED )
      ok = false;

  }

  if (!ok) {
    lcfgderivlist_destroy(clone);
    clone = NULL;
  }

  return clone;
}

static LCFGChange lcfgderivlist_insert_next( LCFGDerivationList * list,
                                             LCFGSListNode      * node,
                                             LCFGDerivation     * item ) {
  assert( list != NULL );
  assert( item != NULL );

  if ( !lcfgderivation_is_valid(item) ) return LCFG_CHANGE_ERROR;

  LCFGSListNode * new_node = lcfgslistnode_new(item);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgderivation_acquire(item);

  if ( node == NULL ) { /* HEAD */

    if ( lcfgslist_is_empty(list) )
      list->tail = new_node;

    new_node->next = list->head;
    list->head     = new_node;

  } else {

    if ( node->next == NULL )
      list->tail = new_node;

    new_node->next = node->next;
    node->next     = new_node;

  }

  list->size++;

  return LCFG_CHANGE_ADDED;
}

static LCFGChange lcfgderivlist_remove_next( LCFGDerivationList * list,
                                             LCFGSListNode      * node,
                                             LCFGDerivation    ** item ) {
  assert( list != NULL );

  if ( lcfgslist_is_empty(list) ) return LCFG_CHANGE_NONE;

  LCFGSListNode * old_node = NULL;

  if ( node == NULL ) { /* HEAD */

    old_node   = list->head;
    list->head = list->head->next;

    if ( lcfgslist_size(list) == 1 )
      list->tail = NULL;

  } else {

    if ( node->next == NULL ) return LCFG_CHANGE_ERROR;

    old_node   = node->next;
    node->next = node->next->next;

    if ( node->next == NULL )
      list->tail = node;

  }

  list->size--;

  *item = lcfgslist_data(old_node);

  lcfgslistnode_destroy(old_node);

  return LCFG_CHANGE_REMOVED;
}

/**
 * @brief Find the derivation node with a given filename
 *
 * This can be used to search through an @c LCFGDerivationList to find
 * the first derivation node which has a matching file name. Note that
 * the matching is done using strcmp(3) which is case-sensitive.
 * 
 * A @c NULL value is returned if no matching node is found. Also, a
 * @c NULL value is returned if a @c NULL value or an empty list is
 * specified.
 *
 * @param[in] drvlist Pointer to @c LCFGDerivationList to be searched
 * @param[in] want_file The name of the required file
 *
 * @return Pointer to an @c LCFGSListNode (or the @c NULL value).
 *
 */

LCFGSListNode * lcfgderivlist_find_node( const LCFGDerivationList * drvlist,
                                         const char * want_file ) {
  assert( want_file != NULL );

  if ( lcfgslist_is_empty(drvlist) ) return NULL;

  LCFGSListNode * result = NULL;

  LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(drvlist);
        cur_node != NULL && result == NULL;
        cur_node = lcfgslist_next(cur_node) ) {

    const LCFGDerivation * drv = lcfgslist_data(cur_node);

    if ( !lcfgderivation_is_valid(drv) ) continue;

    if ( lcfgderivation_match( drv, want_file ) )
      result = cur_node;

  }

  return result;
}

/**
 * @brief  Find the derivation with a given filename
 *
 * This can be used to search through an @c LCFGDerivationList to find
 * the first @c LCFGDerivation which has a matching file name.  Note
 * that the matching is done using strcmp(3) which is case-sensitive.
 * 
 * This uses the @c lcfgderivlist_find_node() to find the relevant
 * node and it behaves in a similar fashion so a @c NULL value is
 * returned if no matching node is found. Also, a @c NULL value is
 * returned if a @c NULL value or an empty list is specified.
 *
 * To ensure the returned @c LCFGDerivation is not destroyed when the
 * parent @c LCFGDerivationList list is destroyed you would need to
 * call the @c lcfgderivation_acquire() function.
 *
 * @param[in] drvlist Pointer to @c LCFGDerivationList to be searched
 * @param[in] want_file The name of the required file
 *
 * @return Pointer to an @c LCFGDerivation (or the @c NULL value).
 *
 */

LCFGDerivation * lcfgderivlist_find_derivation( const LCFGDerivationList * drvlist,
                                                const char * want_file ) {
  assert( want_file != NULL );

  LCFGDerivation * derivation = NULL;

  const LCFGSListNode * node = lcfgderivlist_find_node( drvlist, want_file );
  if ( node != NULL )
    derivation = lcfgslist_data(node);

  return derivation;
}

/**
 * @brief Check if a derivation list contains a particular file
 *
 * This can be used to search through an @c LCFGDerivationList to
 * check if it contains a @c LCFGDerivation with a matching file name.
 * Note that the matching is done using strcmp(3) which is
 * case-sensitive.
 * 
 * This uses the @c lcfgderivlist_find_node() function to find the
 * relevant node. If a @c NULL value is specified for the list or the
 * list is empty then a false value will be returned.
 *
 * @param[in] drvlist Pointer to @c LCFGDerivationList to be searched
 * @param[in] want_file The name of the required file
 *
 * @return Boolean value which indicates presence of derivation in list
 *
 */

bool lcfgderivlist_contains( const LCFGDerivationList * drvlist,
                             const char * want_file ) {
  assert( want_file != NULL );

  return ( lcfgderivlist_find_node( drvlist, want_file ) != NULL );
}

/**
 * @brief Merge a single derivation into the list
 *
 * This can be used to merge a single @c LCFGDerivation into an
 * existing @c LCFGDerivationList. If the list does not already
 * contain a derivation with the same file name the @c LCFGDerivation
 * will be simply appended to the list. Otherwise the line numbers
 * from the new @c LCFGDerivation will be merged into the existing
 * structure. There is support for Copy-On-Write, if the matching @c
 * LCFGDerivation structure is shared between multiple lists then it
 * will be cloned before the merging is done.
 *
 * @param[in] drvlist Pointer to @c LCFGDerivationList
 * @param[in] new_drv Pointer to @c LCFGDerivation to be merged
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgderivlist_merge_derivation( LCFGDerivationList * drvlist,
                                           LCFGDerivation     * new_drv ) {
  assert( drvlist != NULL );
  assert( new_drv != NULL );

  if ( !lcfgderivation_is_valid(new_drv) ) return LCFG_CHANGE_ERROR;

  LCFGSListNode * node =
    lcfgderivlist_find_node( drvlist, lcfgderivation_get_file(new_drv) );

  LCFGChange result = LCFG_CHANGE_ERROR;
  if ( node == NULL ) {
    result = lcfgderivlist_append( drvlist, new_drv );
  } else {
    LCFGDerivation * cur_drv = lcfgslist_data(node);

    if ( cur_drv == new_drv ) {
      result = LCFG_CHANGE_NONE;
    } else if ( !lcfgderivation_is_shared(cur_drv) ) {
      result = lcfgderivation_merge_lines( cur_drv, new_drv );
    } else {
      LCFGDerivation * clone = lcfgderivation_clone(cur_drv);

      if ( clone != NULL )
        result = lcfgderivation_merge_lines( clone, new_drv );

      /* Only keep the clone if it was modified */
      if ( LCFGChangeOK(result) && result != LCFG_CHANGE_NONE ) {
        node->data = (void *) clone;
        lcfgderivation_relinquish(cur_drv);
      } else {
        lcfgderivation_relinquish(clone);
      }
    }

  }

  return result;
}

/**
 * @brief Merge derivation data into the list
 *
 * This can be used to merge derivation data for a single line in a
 * single file into an existing @c LCFGDerivationList. If the list
 * does not already contain a derivation with the same file name a new
 * @c LCFGDerivation will be simply appended to the list. Otherwise
 * the line number will be merged into the existing structure. There
 * is support for Copy-On-Write, if the matching @c LCFGDerivation
 * structure is shared between multiple lists then it will be cloned
 * before the line number merging is done.
 *
 * @param[in] drvlist Pointer to @c LCFGDerivationList
 * @param[in] filename File name for derivation
 * @param[in] line Line number for derivation (-1 to ignore)
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgderivlist_merge_file_line( LCFGDerivationList * drvlist,
                                          const char * filename,
                                          int line ) {
  assert( drvlist != NULL );

  if ( isempty(filename) ) return LCFG_CHANGE_NONE;

  LCFGChange result = LCFG_CHANGE_NONE;

  LCFGSListNode * node = lcfgderivlist_find_node( drvlist, filename );
  if ( node == NULL ) {

    LCFGDerivation * new_drv = lcfgderivation_new();

    char * filename_copy = strdup(filename);
    bool set_ok = lcfgderivation_set_file( new_drv, filename_copy );
    if ( !set_ok ) {
      result = LCFG_CHANGE_ERROR;
      free(filename_copy);
    } else {

      if ( line >= 0 )
        result = lcfgderivation_merge_line( new_drv, line );

      if ( LCFGChangeOK(result) )
        result = lcfgderivlist_append( drvlist, new_drv );

    }

    lcfgderivation_relinquish(new_drv);

  } else if ( line >= 0 ) {

    LCFGDerivation * cur_drv = lcfgslist_data(node);

    if ( lcfgderivation_has_line( cur_drv, line ) ) {
      result = LCFG_CHANGE_NONE;
    } else if ( lcfgderivation_is_shared(cur_drv) ) {
      LCFGDerivation * clone = lcfgderivation_clone(cur_drv);

      result = lcfgderivation_merge_line( clone, line );

      if ( LCFGChangeOK(result) && result != LCFG_CHANGE_NONE ) {
        node->data = (void *) clone;
        lcfgderivation_relinquish(cur_drv);
      } else {
        lcfgderivation_relinquish(clone);
      }
    } else {
      result = lcfgderivation_merge_line( cur_drv, line );
    }

  }

  return result;
}

/**
 * @brief Merge whole derivation list into the list
 *
 * This can be used to merge all the entries in an @c
 * LCFGDerivationList into another @c LCFGDerivationList. Each entry
 * is merged using the @c lcfgderivlist_merge_derivation() function,
 * for full details see the documentation on that function.
 *
 * @param[in] drvlist1 Pointer to @c LCFGDerivationList to be merged into
 * @param[in] drvlist2 Pointer to @c LCFGDerivationList to be merged from
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgderivlist_merge_list( LCFGDerivationList * drvlist1,
                                     const LCFGDerivationList * drvlist2 ) {
  assert( drvlist1 != NULL );

  if ( lcfgderivlist_is_empty(drvlist2) ) return LCFG_CHANGE_NONE;

  LCFGChange change = LCFG_CHANGE_NONE;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(drvlist2);
        LCFGChangeOK(change) && cur_node != NULL;
        cur_node = lcfgslist_next(cur_node) ) {

    LCFGDerivation * drv = lcfgslist_data(cur_node);

    /* Ignore any derivations which do not have a filename */
    if ( !lcfgderivation_is_valid(drv) ) continue;

    LCFGChange merge_rc = lcfgderivlist_merge_derivation( drvlist1, drv );

    if ( merge_rc == LCFG_CHANGE_ERROR )
      change = LCFG_CHANGE_ERROR;
    else if ( merge_rc != LCFG_CHANGE_NONE )
      change = LCFG_CHANGE_MODIFIED;

  }

  return change;
}

/**
 * @brief Load list of derivations from a string
 *
 * This parses a space-separated list of LCFG derivations in the form
 * "foo.rpms:1,5,9 bar.h:7,21" and creates a new @c LCFGDerivationList
 * structure. Each separate derivation is parsed using the @c
 * lcfgderivation_from_string() function. Any leading whitespace will
 * be ignored. The filename is always required, line numbers are
 * optional.
 *
 * To avoid memory leaks, when the newly created @c LCFGDerivationList
 * struct is no longer required you should call the @c
 * lcfgderivlist_relinquish() function.
 *
 * @param[in] input The derivation list string.
 * @param[out] result Reference to the pointer for the @c LCFGDerivationList struct.
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgderivlist_from_string( const char * input,
                                      LCFGDerivationList ** result,
                                      char ** msg ) {

  if ( isempty(input) ) {
    lcfgutils_build_message( msg, "Invalid derivation string" );
    return LCFG_STATUS_ERROR;
  }

  while( *input != '\0' && isspace(*input) ) input++;

  if ( isempty(input) ) {
    lcfgutils_build_message( msg, "Invalid derivation string" );
    return LCFG_STATUS_ERROR;
  }

  LCFGStatus status = LCFG_STATUS_OK;
  LCFGDerivationList * drvlist = lcfgderivlist_new();

  char * input2 = strdup(input);
  char * saveptr = NULL;
  static const char * whitespace = " \n\r\t";
  
  char * token = strtok_r( input2, whitespace, &saveptr );
  while ( status != LCFG_STATUS_ERROR && token ) {

    char * parse_msg = NULL;
    LCFGDerivation * drv = NULL;
    LCFGStatus parse_rc = lcfgderivation_from_string( token, &drv, &parse_msg );
    if ( parse_rc == LCFG_STATUS_ERROR ) {
      lcfgutils_build_message( msg, "Failed to parse derivation '%s': %s",
                               token, parse_msg );
      status = LCFG_STATUS_ERROR;
    } else {
      LCFGChange change = lcfgderivlist_merge_derivation( drvlist, drv );
      if ( LCFGChangeError(change) ) {
        lcfgutils_build_message( msg, "Failed to add derivation '%s' to list",
                                 token );
        status = LCFG_STATUS_ERROR;
      }
    }

    lcfgderivation_relinquish(drv);
    free(parse_msg);

    if ( status != LCFG_STATUS_ERROR )
      token = strtok_r( NULL, whitespace, &saveptr );
  }

  free(input2);

  if ( status == LCFG_STATUS_ERROR ) {
    lcfgderivlist_relinquish(drvlist);
    drvlist = NULL;
  }

  *result = drvlist;

  return status;
}

/**
 * @brief Merge list of derivations from string
 *
 * This can be used to merge all the derivation information in a
 * string into an existing @c LCFGDerivationList. The string is parsed
 * using the @c lcfgderivlist_from_string() function and the resulting
 * @c LCFGDerivationList is then merged into the current list using
 * the @c lcfgderivlist_merge_list() function, for full details see
 * the documentation on those functions.
 *
 * @param[in] drvlist Pointer to @c LCFGDerivationList to be merged into
 * @param[in] input The derivation list string.
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgderivlist_merge_string_list( LCFGDerivationList * drvlist,
                                            const char * input,
                                            char ** msg ) {
  assert( drvlist != NULL );

  LCFGChange change = LCFG_CHANGE_NONE;

  LCFGDerivationList * extra_drvlist = NULL;
  LCFGStatus parse_status = lcfgderivlist_from_string( input,
                                                       &extra_drvlist,
                                                       msg );

  if ( parse_status == LCFG_STATUS_ERROR )
    change = LCFG_CHANGE_ERROR;
  else
    change = lcfgderivlist_merge_list( drvlist, extra_drvlist );

  lcfgderivlist_relinquish(extra_drvlist);

  return change;
}

/**
 * @brief Serialise the derivation list as a string
 *
 * This can be used to serialise the entire list of derivations as a
 * string. Derivations from each file are serialised using the @c
 * lcfgderivation_to_string() function, this uses the format @c
 * example.h:3,7,28, with the list of line numbers sorted in numerical
 * order. The derivations are separated with single spaces.

 * The following options are supported:
 *   - @c LCFG_OPT_NEWLINE - Add a newline at the end of the string.
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many package strings, this can be a
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
 * @param[in] drvlist Pointer to @c LCFGDerivationList
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgderivlist_to_string( const LCFGDerivationList * drvlist,
                                 LCFGOption options,
                                 char ** result, size_t * size ) {

  size_t new_len = lcfgderivlist_get_string_length(drvlist);

  /* Optional newline at end of string */

  if ( options&LCFG_OPT_NEWLINE )
    new_len += 1;

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    size_t new_size = new_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG resource string");
      exit(EXIT_FAILURE);
    } else {
      *result = new_buf;
      *size   = new_size;
    }
  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  bool first = true;
  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(drvlist);
        cur_node != NULL;
        cur_node = lcfgslist_next(cur_node) ) {

    LCFGDerivation * drv = lcfgslist_data(cur_node);

    /* Ignore any derivations which do not have a name or value */
    if ( !lcfgderivation_is_valid(drv) ) continue;

    lcfgderivation_sort_lines(drv);

    ssize_t drv_len = lcfgderivation_get_length(drv);
    if ( drv_len > 0 ) {
      if ( first ) {
        first = false;
      } else {
        *to = ' ';
        to++;
      }

      size_t size = drv_len + 1; /* for nul-terminator */
      ssize_t len = lcfgderivation_to_string( drv, LCFG_OPT_NONE, &to, &size );
      to += len;
    }
  }

  if ( options&LCFG_OPT_NEWLINE )
    to = stpncpy( to, "\n", 1 );

  *to = '\0';

  assert( (*result + new_len ) == to );

  return new_len;
}

/**
 * @brief Write formatted list of derivations to file stream
 *
 * This uses @c lcfgderivation_to_string() to format each @c
 * LCFGDerivation as a string. See the documentation for that function
 * for full details. The generated string is written to the specified
 * file stream which must have already been opened for writing.
 *
 * Derivations which are invalid will be ignored.
 *
 * @param[in] drvlist Pointer to @c LCFGDerivationList
 * @param[in] out Stream to which the derivation list should be written
 *
 * @return Boolean indicating success
 *
 */

bool lcfgderivlist_print( const LCFGDerivationList * drvlist,
                          FILE * out ) {
  assert( drvlist != NULL );

  if ( lcfgslist_is_empty(drvlist) ) return true;

  /* Allocate a reasonable buffer which will be reused for each
     derivation. Note that if necessary it will be resized automatically. */

  size_t buf_size = 256;
  char * str_buf  = calloc( buf_size, sizeof(char) );
  if ( str_buf == NULL ) {
    perror("Failed to allocate memory for LCFG derivation string");
    exit(EXIT_FAILURE);
  }

  bool ok = true;

  bool first = true;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(drvlist);
        ok && cur_node != NULL;
        cur_node = lcfgslist_next(cur_node) ) {

    const LCFGDerivation * drv = lcfgslist_data(cur_node);

    /* Ignore any derivations which do not have a name or value */
    if ( !lcfgderivation_is_valid(drv) ) continue;

    if ( lcfgderivation_to_string( drv, LCFG_OPT_NONE,
                                   &str_buf, &buf_size ) < 0 ) {
      ok = false;
    }

    if (ok) {
      if (first) {
        first = false;
      } else {
        if ( fputc( ' ', out ) == EOF )
          ok = false;
      }
    }

    if (ok) {
      if ( fputs( str_buf, out ) < 0 )
        ok = false;
    }

  }

  free(str_buf);

  if ( fputs( "\n", out ) < 0 )
    ok = false;

  return ok;
}

/* eof */
