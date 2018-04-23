/**
 * @file common/derivation/derivation.c
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
  drvlist->hash      = 0;
  drvlist->_refcount = 1;

  return drvlist;
}

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

void lcfgderivlist_acquire( LCFGDerivationList * drvlist ) {
  assert( drvlist != NULL );

  drvlist->_refcount += 1;
}

void lcfgderivlist_relinquish( LCFGDerivationList * drvlist ) {

  if ( drvlist == NULL ) return;

  if ( drvlist->_refcount > 0 )
    drvlist->_refcount -= 1;

  if ( drvlist->_refcount == 0 )
    lcfgderivlist_destroy(drvlist);

}

bool lcfgderivlist_is_shared( const LCFGDerivationList * drvlist ) {
  return ( drvlist->_refcount > 1 );
}

size_t lcfgderivlist_get_string_length( const LCFGDerivationList * drvlist ) {
  if ( lcfgderivlist_is_empty(drvlist) ) return 0;

  size_t length = 0;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(drvlist);
        cur_node != NULL;
        cur_node = lcfgslist_next(cur_node) ) {

    LCFGDerivation * drv = lcfgslist_data(cur_node);

    if ( lcfgderivation_is_valid(drv) )
      length += (size_t) lcfgderivation_get_length(drv) + 1; /* for space sep */
  }

  /* n - 1 space separators */
  length -= 1;

  return length;
}

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

LCFGDerivation * lcfgderivlist_find_derivation( const LCFGDerivationList * drvlist,
                                                const char * want_file ) {
  assert( want_file != NULL );

  LCFGDerivation * derivation = NULL;

  const LCFGSListNode * node = lcfgderivlist_find_node( drvlist, want_file );
  if ( node != NULL )
    derivation = lcfgslist_data(node);

  return derivation;
}

bool lcfgderivlist_contains( const LCFGDerivationList * drvlist,
                             const char * want_file ) {
  assert( want_file != NULL );

  return ( lcfgderivlist_find_node( drvlist, want_file ) != NULL );
}

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

    if ( !lcfgderivation_is_shared(cur_drv) ) {
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

ssize_t lcfgderivlist_to_string( const LCFGDerivationList * drvlist,
                                 LCFGOption options,
                                 char ** result, size_t * size ) {
  assert( drvlist != NULL );

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
