/**
 * @file context/list.c
 * @brief Functions for working with lists of LCFG contexts
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date: 2017-04-25 15:04:02 +0100 (Tue, 25 Apr 2017) $
 * $Revision: 32496 $
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#include <assert.h>

#include "context.h"
#include "utils.h"

static LCFGChange lcfgctxlist_insert_next( LCFGContextList * list,
                                           LCFGSListNode   * node,
                                           LCFGContext     * item )
  __attribute__((warn_unused_result));

static LCFGChange lcfgctxlist_remove_next( LCFGContextList * list,
                                           LCFGSListNode   * node,
                                           LCFGContext    ** item )
  __attribute__((warn_unused_result));

/**
 * @brief Append a context to a list
 *
 * This is a simple macro wrapper around the
 * @c lcfgctxlist_insert_next() function which can be used to append
 * a context structure on to the end of the specified context list.
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList
 * @param[in] ctx Pointer to @c LCFGContext
 * 
 * @return Integer value indicating type of change
 *
 */

#define lcfgctxlist_append(ctxlist, ctx) ( lcfgctxlist_insert_next( ctxlist, lcfgslist_tail(ctxlist), ctx ) )

/**
 * @brief Create and initialise a new context list
 *
 * Creates a new @c LCFGContextList which represents an empty
 * context list.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * To avoid memory leaks, when the new structure is no longer required
 * the @c lcfgctxlist_destroy() function should be called.
 *
 * @return Pointer to new @c LCFGContextList
 *
 */

LCFGContextList * lcfgctxlist_new(void) {

  LCFGContextList * ctxlist = malloc( sizeof(LCFGContextList) );
  if ( ctxlist == NULL ) {
    perror( "Failed to allocate memory for LCFG context list" );
    exit(EXIT_FAILURE);
  }

  /* Default values */

  ctxlist->size = 0;
  ctxlist->head = NULL;
  ctxlist->tail = NULL;

  return ctxlist;
}

/**
 * @brief Destroy a context list
 *
 * When the specified @c LCFGContextList is no longer required
 * this can be used to free all associated memory.
 *
 * This will iterate through the list to remove and destroy each @c
 * LCFGSListNode item, it also calls @c lcfgcontext_relinquish() for
 * each context. Note that if the reference count on the context
 * reaches zero then the @c LCFGContext will also be destroyed.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * context list which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList to be destroyed.
 *
 */

void lcfgctxlist_destroy(LCFGContextList * ctxlist) {

  if ( ctxlist == NULL ) return;

  while ( lcfgslist_size(ctxlist) > 0 ) {
    LCFGContext * ctx = NULL;
    if ( lcfgctxlist_remove_next( ctxlist, NULL,
                                   &ctx ) == LCFG_CHANGE_REMOVED ) {
      lcfgcontext_relinquish(ctx);
    }
  }

  free(ctxlist);
  ctxlist = NULL;

}

/**
 * @brief Clone a context list
 *
 * This can be used to create a clone of the specified context
 * list. Note that this does @b NOT clone the @c LCFGContext structs
 * only the nodes, the contexts will be shared. This is mostly useful
 * in the situation where a list needs to be modified (i.e. adding
 * extra items or removing some) or sorted without the original list
 * being altered.
 *
 * This uses @c lcfgctxlist_new() to create a new empty list so if the
 * memory allocation for the new struct is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * If the cloning process fails this function will return a @c NULL
 * value to indicate that an error has occurred.
 *
 * To avoid memory leaks, when the new struct is no longer required
 * the @c lcfgctxlist_destroy() function should be called.
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList to be cloned
 *
 * @return Pointer to new clone @c LCFGContextList
 *
 */

LCFGContextList * lcfgctxlist_clone( const LCFGContextList * ctxlist ) {
  assert( ctxlist != NULL );

  LCFGContextList * clone = lcfgctxlist_new();

  /* Note that this does NOT clone the contexts themselves only the nodes. */
  bool ok = true;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(ctxlist);
        cur_node != NULL && ok;
        cur_node = lcfgslist_next(cur_node) ) {

    LCFGContext * ctx = lcfgslist_data(cur_node);

    LCFGChange rc = lcfgctxlist_append( clone, ctx );
    if ( rc != LCFG_CHANGE_ADDED )
      ok = false;

  }

  if (!ok) {
    lcfgctxlist_destroy(clone);
    clone = NULL;
  }

  return clone;
}

/**
 * @brief Insert a new context into a list
 *
 * This can be used to insert an @c LCFGContext into the
 * specified context list. The context will be wrapped into an
 * @c LCFGSListNode using the @c lcfgctxnode_new() function.
 *
 * The context will be inserted into the list immediately after the
 * specified @c LCFGSListNode. To insert the context at the
 * head of the list the @c NULL value should be passed for the node.
 *
 * If the context is successfully inserted into the list the
 * @c LCFG_CHANGE_ADDED value is returned, if an error occurs then
 * @c LCFG_CHANGE_ERROR is returned.
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList
 * @param[in] ctxnode Pointer to @c LCFGSListNode
 * @param[in] ctx Pointer to @c LCFGContext
 * 
 * @return Integer value indicating type of change
 */

static LCFGChange lcfgctxlist_insert_next( LCFGContextList * list,
                                           LCFGSListNode   * node,
                                           LCFGContext     * item ) {
  assert( list != NULL );
  assert( item != NULL );

  if ( !lcfgcontext_is_valid(item) ) return LCFG_CHANGE_ERROR;

  LCFGSListNode * new_node = lcfgslistnode_new(item);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgcontext_acquire(item);

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

/**
 * @brief Remove a context from a list
 *
 * This can be used to remove an @c LCFGContext from the
 * specified context list.
 *
 * The context removed from the list is immediately after the
 * specified @c LCFGSListNode. To remove the context from the
 * head of the list the @c NULL value should be passed for the node.
 *
 * If the context is successfully removed from the list the
 * @c LCFG_CHANGE_REMOVED value is returned, if an error occurs then
 * @c LCFG_CHANGE_ERROR is returned. If the list is already empty then
 * the @c LCFG_CHANGE_NONE value is returned.
 *
 * Note that, since a pointer to the @c LCFGContext is returned
 * to the caller, the reference count will still be at least 1. To
 * avoid memory leaks, when the struct is no longer required it should
 * be released by calling @c lcfgcontext_relinquish().
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList
 * @param[in] ctxnode Pointer to @c LCFGSListNode
 * @param[out] ctx Pointer to @c LCFGContext
 * 
 * @return Integer value indicating type of change
 */

static LCFGChange lcfgctxlist_remove_next( LCFGContextList * list,
                                           LCFGSListNode   * node,
                                           LCFGContext    ** item ) {
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
 * @brief Find the context list node for a given context name
 *
 * This can be used to search through an @c LCFGContextList to find
 * the first context node which has a matching name. Note that the
 * matching is done using strcmp(3) which is case-sensitive.
 * 
 * A @c NULL value is returned if no matching node is found. Also, a
 * @c NULL value is returned if a @c NULL value or an empty list is
 * specified.
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList to be searched
 * @param[in] want_name The name of the required context node
 *
 * @return Pointer to an @c LCFGSListNode (or the @c NULL value).
 *
 */

LCFGSListNode * lcfgctxlist_find_node( const LCFGContextList * ctxlist,
                                       const char * want_name ) {
  assert( want_name != NULL );

  if ( lcfgslist_is_empty(ctxlist) ) return NULL;

  LCFGSListNode * result = NULL;

  LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(ctxlist);
        cur_node != NULL && result == NULL;
        cur_node = lcfgslist_next(cur_node) ) {

    const LCFGContext * ctx = lcfgslist_data(cur_node);

    if ( !lcfgcontext_is_valid(ctx) ) continue;

    if ( lcfgcontext_match( ctx, want_name ) )
      result = cur_node;

  }

  return result;
}

/**
 * @brief Find the context for a given name
 *
 * This can be used to search through an @c LCFGContextList
 * to find the first context which has a matching name. Note
 * that the matching is done using strcmp(3) which is case-sensitive.
 * 
 * This uses the @c lcfgctxlist_find_node() to find the relevant node
 * and it behaves in a similar fashion so a @c NULL value is returned
 * if no matching node is found. Also, a @c NULL value is returned if
 * a @c NULL value or an empty list is specified.
 *
 * To ensure the returned @c LCFGContext is not destroyed when
 * the parent @c LCFGContextList list is destroyed you would need to
 * call the @c lcfgcontext_acquire() function.
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList to be searched
 * @param[in] name The name of the required context node
 *
 * @return Pointer to an @c LCFGContext (or the @c NULL value).
 *
 */

LCFGContext * lcfgctxlist_find_context( const LCFGContextList * ctxlist,
					const char * name ) {
  assert( name != NULL );

  LCFGContext * context = NULL;

  const LCFGSListNode * node = lcfgctxlist_find_node( ctxlist, name );
  if ( node != NULL )
    context = lcfgslist_data(node);

  return context;
}

/**
 * @brief Check if a context list contains a particular context
 *
 * This can be used to search through an @c LCFGContextList
 * to check if it contains a context with a matching name. Note
 * that the matching is done using strcmp(3) which is case-sensitive.
 * 
 * This uses the @c lcfgctxlist_find_node() function to find the
 * relevant node. If a @c NULL value is specified for the list or the
 * list is empty then a false value will be returned.
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList to be searched
 * @param[in] name The name of the required context node
 *
 * @return Boolean value which indicates presence of context in list
 *
 */

bool lcfgctxlist_contains( const LCFGContextList * ctxlist,
			   const char * name ) {
  assert( name != NULL );

  return ( lcfgctxlist_find_node( ctxlist, name ) != NULL );
}

/**
 * @brief Add or update a context in a list
 *
 * The list will be searched to check if a context with the same name
 * as that specified is already stored in the list. 
 *
 *    - If does not already appear in the list then it will be simply
 *      appended and @c LCFG_CHANGE_ADDED is returned.
 *    - If it does appear and the contexts are equal (according to the
 *      @c lcfgcontext_equals() function) then no change will occur
 *      and @c LCFG_CHANGE_NONE is returned.
 *    - If a context of the same name is already in the list but
 *      differs in value then the current @c LCFGContext will be
 *      replaced with the new one and @c LCFG_CHANGE_MODIFIED is returned.
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList
 * @param[in] new_ctx Pointer to @c LCFGContext
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgctxlist_update( LCFGContextList * ctxlist,
                               LCFGContext     * new_ctx ) {
  assert( ctxlist != NULL );
  assert( new_ctx != NULL );

  if ( !lcfgcontext_is_valid(new_ctx) ) return LCFG_CHANGE_ERROR;

  LCFGSListNode * cur_node =
    lcfgctxlist_find_node( ctxlist, lcfgcontext_get_name(new_ctx) );

  LCFGChange result = LCFG_CHANGE_ERROR;
  if ( cur_node == NULL ) {
    result = lcfgctxlist_append( ctxlist, new_ctx );
  } else {

    LCFGContext * cur_ctx = lcfgslist_data(cur_node);
    if ( lcfgcontext_equals( cur_ctx, new_ctx ) ) {
      result = LCFG_CHANGE_NONE;
    } else {

      /* This completely replaces a context held in a node rather than
	 modifying any values. This is particularly useful when a list
	 might be a clone of another and thus the context is shared -
	 modifying a context in one list would also change the other
	 list. */

      lcfgcontext_acquire(new_ctx);
      cur_node->data = new_ctx;

      lcfgcontext_relinquish(cur_ctx);

      result = LCFG_CHANGE_MODIFIED;
    }

  }

  return result;
}

/**
 * @brief Read list of contexts from file
 *
 * This will read a list of contexts from the specified file and store
 * them in a new @c LCFGContextList.
 *
 * Leading whitespace is ignored, as are empty lines and those
 * beginning with a @c # (hash) comment marker. Each line of content
 * is parsed using the @c lcfgcontext_from_string() function and thus
 * the expected format is "NAME = VALUE"
 *
 * The priority assigned to each context is based on the line number
 * in the file with the first entry having a priority of 1.
 *
 * An error is returned if the file does not exist unless the
 * @c LCFG_OPT_ALLOW_NOEXIST option is specified. If the file exists
 * but is empty then an empty @c LCFGContextList is returned.
 *
 * @param[in] filename The name of the file to be read.
 * @param[out] result Reference to the pointer for the @c LCFGContextList.
 * @param[out] modtime The modification time of the file.
 * @param[in] options Integer which controls behaviour.
 * @param[out] msg  Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgctxlist_from_file( const char * filename,
                                  LCFGContextList ** result,
				  time_t * modtime,
				  LCFGOption options,
                                  char ** msg ) {

  *result = NULL;
  *modtime = 0;

  LCFGStatus status = LCFG_STATUS_OK;

  FILE *file;
  if ((file = fopen(filename, "r")) == NULL) {

    if (errno == ENOENT) {
      if ( options&LCFG_OPT_ALLOW_NOEXIST ) {
	/* No file so just create an empty list */
	*result = lcfgctxlist_new();
      } else {
	lcfgutils_build_message( msg, "'%s' does not exist.", filename );
	status = LCFG_STATUS_ERROR;
      }
    } else {
      lcfgutils_build_message( msg, "'%s' is not readable.", filename );
      status = LCFG_STATUS_ERROR;
    }

    return status;
  }

  /* Collect the mtime for the file as often need to compare the times */

  struct stat buf;
  if ( stat( filename, &buf ) == 0 )
    *modtime = buf.st_mtime;

  LCFGContextList * ctxlist = lcfgctxlist_new();

  size_t line_len = 128;
  char * line = calloc( line_len, sizeof(char) );
  if ( line == NULL ) {
    perror("Failed to allocate memory for processing LCFG context file" );
    exit(EXIT_FAILURE);
  }

  int linenum = 0; /* The line number is used as the context priority */
  while( getline( &line, &line_len, file ) != -1 ) {
    linenum++;

    const char * ctx_str = line;

    /* skip past any leading whitespace */
    while ( isspace(*ctx_str) ) ctx_str++;

    /* ignore empty lines and comments */
    if ( *ctx_str == '\0' || *ctx_str == '#' ) continue;

    char * parse_msg = NULL;
    LCFGContext * ctx = NULL;
    LCFGStatus parse_rc = lcfgcontext_from_string( ctx_str, linenum,
                                                   &ctx, &parse_msg );
    if ( parse_rc != LCFG_STATUS_OK || ctx == NULL ) {
      lcfgutils_build_message( msg,
                "Failed to parse context '%s' on line %d of %s: %s",
                ctx_str, linenum, filename, parse_msg );
      status = LCFG_STATUS_ERROR;
    }

    free(parse_msg);

    if ( status == LCFG_STATUS_OK ) {

      LCFGChange rc = lcfgctxlist_update( ctxlist, ctx );

      if ( rc == LCFG_CHANGE_ERROR ) {
        lcfgutils_build_message( msg, "Failed to store context '%s'", ctx_str );
        status = LCFG_STATUS_ERROR;
      }

    }

    lcfgcontext_relinquish(ctx);

    if ( status != LCFG_STATUS_OK ) break;

  }

  fclose(file);
  free(line);

  if ( status != LCFG_STATUS_OK ) {
    lcfgctxlist_destroy(ctxlist);
    ctxlist = NULL;
  }

  *result = ctxlist;

  return status;
}

/**
 * @brief Write list of formatted contexts to file stream
 *
 * This uses @c lcfgcontext_to_string() to format each context as a
 * string with a trailing newline character. The generated string is
 * written to the specified file stream which must have already been
 * opened for writing.
 *
 * Contexts which do not have a name or value will be ignored.
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList
 * @param[in] out Stream to which the context list should be written
 *
 * @return Boolean indicating success
 */

bool lcfgctxlist_print( const LCFGContextList * ctxlist,
                        FILE * out ) {
  assert( ctxlist != NULL );

  if ( lcfgslist_is_empty(ctxlist) ) return true;

  /* Allocate a reasonable buffer which will be reused for each
     context. Note that if necessary it will be resized automatically. */

  size_t buf_size = 64;
  char * str_buf  = calloc( buf_size, sizeof(char) );
  if ( str_buf == NULL ) {
    perror("Failed to allocate memory for LCFG context string");
    exit(EXIT_FAILURE);
  }

  bool ok = true;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(ctxlist);
        cur_node != NULL;
        cur_node = lcfgslist_next(cur_node) ) {

    const LCFGContext * ctx = lcfgslist_data(cur_node);

    /* Ignore any contexts which do not have a name or value */
    if ( !lcfgcontext_is_valid(ctx) || !lcfgcontext_has_value(ctx) )
      continue;

    if ( lcfgcontext_to_string( ctx, LCFG_OPT_NEWLINE,
                                &str_buf, &buf_size ) < 0 ) {
      ok = false;
      break;
    }

    if ( fputs( str_buf, out ) < 0 ) {
      ok = false;
      break;
    }

  }

  free(str_buf);

  return ok;
}

/**
 * @brief Write list of formatted contexts to file
 *
 * This opens the specified file for writing and calls
 * @c lcfgctxlist_print() to write the list to the file. Before it is
 * written out the list is sorted into priority order (note that this
 * may alter the list). If the list is empty an empty file will be
 * created. If required the modification time for the file can be
 * specified, otherwise set the mtime to zero.
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList
 * @param[in] filename File to which the context list should be written
 * @param[in] mtime Modification time to set on the file
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change for file
 *
 */

LCFGChange lcfgctxlist_to_file( LCFGContextList * ctxlist,
                                const char * filename,
                                time_t mtime,
                                char ** msg ) {
  assert( ctxlist != NULL );
  assert( filename != NULL );

  LCFGChange change = LCFG_CHANGE_NONE;

  char * tmpfile = NULL;
  FILE * tmpfh = lcfgutils_safe_tmpfile( filename, &tmpfile );

  if ( tmpfh == NULL ) {
    change = LCFG_CHANGE_ERROR;
    lcfgutils_build_message( msg, "Failed to open context file" );
    goto cleanup;
  }

  lcfgctxlist_sort_by_priority(ctxlist);

  bool print_ok = lcfgctxlist_print( ctxlist, tmpfh );

  if (!print_ok) {
    change = LCFG_CHANGE_ERROR;
    lcfgutils_build_message( msg, "Failed to write context file" );
  }

  /* Always attempt to close temporary file */

  if ( fclose(tmpfh) != 0 ) {
    change = LCFG_CHANGE_ERROR;
    lcfgutils_build_message( msg, "Failed to close context file" );
  }

  if ( change != LCFG_CHANGE_ERROR )
    change = lcfgutils_file_update( filename, tmpfile, mtime );

 cleanup:

  /* This might have already gone but call unlink to ensure
     tidiness. Do not care about the result */

  if ( tmpfile != NULL ) {
    (void) unlink(tmpfile);
    free(tmpfile);
  }

  return change;
}

/**
 * @brief Find the highest priority in a list
 *
 * This scans through the specified context list and finds the
 * greatest priority value associated with any context. If the list is
 * empty or a @c NULL value is specified the value 0 (zero) is
 * returned.
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList
 * 
 * @return Integer for highest priority
 *
 */

int lcfgctxlist_max_priority( const LCFGContextList * ctxlist ) {

  int max_priority = 0;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(ctxlist);
        cur_node != NULL;
        cur_node = lcfgslist_next(cur_node) ) {

    int priority = lcfgcontext_get_priority(lcfgslist_data(cur_node));
    if ( priority > max_priority )
      max_priority = priority;

  }

  return max_priority;
}

/**
 * @brief Sort a context list by priority value
 *
 * This sorts the nodes of the @c LCFGContextList based on the value
 * of the priority in ascending order (i.e. the head has the lowest
 * priority).
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList
 *
 */

void lcfgctxlist_sort_by_priority( LCFGContextList * ctxlist ) {

  if ( lcfgslist_size(ctxlist) < 2 ) return;

  /* bubble sort since this should be a very short list */

  bool swapped=true;
  while (swapped) {
    swapped=false;

    LCFGSListNode * cur_node = NULL;
    for ( cur_node = lcfgslist_head(ctxlist);
          cur_node != NULL && cur_node->next != NULL;
          cur_node = lcfgslist_next(cur_node) ) {

      LCFGContext * cur_ctx  = lcfgslist_data(cur_node);
      LCFGContext * next_ctx = lcfgslist_data(cur_node->next);

      if ( lcfgcontext_get_priority(cur_ctx) >
           lcfgcontext_get_priority(next_ctx) ) {
        cur_node->data       = next_ctx;
        cur_node->next->data = cur_ctx;
        swapped = true;
      }

    }
  }

}

/**
 * @brief Compare two context lists
 *
 * This will compare the contents of two @c LCFGContextList and return
 * a boolean which indicates if they differ. Contexts which are found
 * in both lists are compared using the @c lcfgcontext_identical()
 * function. Note that the order of the contexts in the lists is not
 * important.
 *
 * If a directory for context-specific profiles is specified then the
 * modification times for any which are relevant will be compared with
 * that specified.
 *
 * @param[in] ctxlist1 Pointer to @c LCFGContextList
 * @param[in] ctxlist2 Pointer to @c LCFGContextList
 * @param[in] ctx_profile_dir Location of context-specific profile directory
 * @param[in] prevtime Modification time of previous profile
 *
 * @return boolean which indicates if lists differ
 *
 */

bool lcfgctxlist_diff( const LCFGContextList * ctxlist1,
                       const LCFGContextList * ctxlist2,
		       const char * ctx_profile_dir,
                       time_t prevtime ) {

  bool changed = false;
  const LCFGSListNode * cur_node;

  /* Check for missing nodes and also compare values for common nodes */

  for ( cur_node = lcfgslist_head(ctxlist1);
        cur_node != NULL && !changed;
        cur_node = lcfgslist_next(cur_node) ) {

    const LCFGContext * cur_ctx = lcfgslist_data(cur_node);

    /* Ignore nodes without a name */
    if ( !lcfgcontext_is_valid(cur_ctx) ) continue;

    const LCFGContext * other_ctx =
      lcfgctxlist_find_context( ctxlist2, lcfgcontext_get_name(cur_ctx) );

    if ( other_ctx == NULL ) {
      changed = true;
      break;
    }

    if ( !lcfgcontext_identical( cur_ctx, other_ctx ) ) {
      changed = true;
      break;
    }

    if ( ctx_profile_dir != NULL ) {
      /* A context may have an associated LCFG profile. Check if it has
	 been modified since the last run (just compare timestamps). */

      char * path = lcfgcontext_profile_path( cur_ctx,
					      ctx_profile_dir, ".xml" );
      if ( path != NULL ) {

	struct stat buffer;
	if ( stat ( path, &buffer ) == 0 &&
	     S_ISREG(buffer.st_mode) &&
	     buffer.st_mtime > prevtime ) {
	    changed = true;
	}
      }

      free(path);

      if (changed) break;
    }

  }

  /* If nothing changed so far then check for missing nodes the other way */

  for ( cur_node = lcfgslist_head(ctxlist2);
        cur_node != NULL && !changed;
        cur_node = lcfgslist_next(cur_node) ) {

    const LCFGContext * cur_ctx = lcfgslist_data(cur_node);

    /* Ignore nodes without a name */
    if ( !lcfgcontext_is_valid(cur_ctx) ) continue;

    const LCFGContext * other_ctx =
      lcfgctxlist_find_context( ctxlist1, lcfgcontext_get_name(cur_ctx) );

    if ( other_ctx == NULL ) {
      changed = true;
      break;
    }

  }

  return changed;
}

/**
 * @brief Evaluate a simple context query
 *
 * This can be used to evaluate a simple query which involves a single
 * @c LCFGContext stored in the specified @c LCFGContextList.
 *
 * The following conditions can be evaluated:
 *   - @c LCFG_TEST_ISTRUE : The specified context exists in the list
 *     and the value is true (according to @c lcfgcontext_is_true() ).
 *   - @c LCFG_TEST_ISFALSE : The specified context does not exist or
 *     the value is false (according to @c lcfgcontext_is_false() ).
 *   - @c LCFG_TEST_ISEQ : The value of the context is the same as
 *     that specified. 
 *   - @c LCFG_TEST_ISNE : The value of the context is not the same as
 *     that specified. 
 *
 * Note that with the string equality tests if the context is not
 * found in the list then the value is considered to be @e empty. This
 * @e empty value will match with the specified value if that is also
 * a @c NULL or an empty string.
 *
 * The magnitude of the value returned is the priority associated with
 * the context. If the context is not found in the list then the
 * default magnitude is 1 (one). The sign of the value returned
 * indicates the truth of the comparison (i.e. positive for true and
 * negative for false).
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList
 * @param[in] ctxq_name Name of context to be evaluated
 * @param[in] ctxq_val Value of context to be evaluated
 * @param[in] cmp Type of comparison.
 *
 * @return Integer priority value.
 */

int lcfgctxlist_simple_query( const LCFGContextList * ctxlist,
                              const char * ctxq_name,
                              const char * ctxq_val,
                              LCFGTest cmp ) {
  /* NOTE: May be called with a NULL ctxlist */
  assert( ctxq_name != NULL );

  const LCFGContext * ctx = lcfgctxlist_find_context( ctxlist, ctxq_name );

  int priority = 1;
  const char * ctx_value = NULL;

  if ( ctx != NULL ) {
    priority  = lcfgcontext_get_priority(ctx);
    ctx_value = lcfgcontext_get_value(ctx);
  }

  bool query_is_true = false;
  switch (cmp)
    {
    case LCFG_TEST_ISTRUE:
      query_is_true = lcfgcontext_is_true(ctx);
      break;
    case LCFG_TEST_ISFALSE:
      query_is_true = lcfgcontext_is_false(ctx);
      break;
    case LCFG_TEST_ISEQ:
    case LCFG_TEST_ISNE:
      ;

      bool ctxq_val_empty  = isempty(ctxq_val);
      bool ctx_value_empty = isempty(ctx_value);

      bool same_value = false;
      if ( ctxq_val_empty || ctx_value_empty )
        same_value = ( ctxq_val_empty && ctx_value_empty );
      else
        same_value = ( strcmp( ctxq_val, ctx_value ) == 0 );

      query_is_true = (  same_value && cmp == LCFG_TEST_ISEQ ) ||
                      ( !same_value && cmp == LCFG_TEST_ISNE );
      break;
    }

  return ( query_is_true ? priority : -1 * priority );
}

/* eof */
