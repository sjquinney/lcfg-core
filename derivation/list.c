#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <lcfg/common.h>
#include <lcfg/utils.h>
#include <assert.h>

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

  LCFGDerivationList * drvlist = malloc( sizeof(LCFGDerivationList) );
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

LCFGDerivationList * lcfgderivlist_clone( const LCFGDerivationList * drvlist ) {
  assert( drvlist != NULL );

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

LCFGChange lcfgderivlist_update( LCFGDerivationList * drvlist,
                                 LCFGDerivation     * new_drv ) {
  assert( drvlist != NULL );
  assert( new_drv != NULL );

  if ( !lcfgderivation_is_valid(new_drv) ) return LCFG_CHANGE_ERROR;

  LCFGDerivation * cur_drv =
    lcfgderivlist_find_derivation( drvlist, lcfgderivation_get_file(new_drv) );

  LCFGChange result = LCFG_CHANGE_ERROR;
  if ( cur_drv == NULL ) {
    result = lcfgderivlist_append( drvlist, new_drv );
  } else {
    result = lcfgderivation_merge_lines( cur_drv, new_drv );
  }

  return result;
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
      LCFGChange change = lcfgderivlist_update( drvlist, drv );
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

ssize_t lcfgderivlist_to_string( const LCFGDerivationList * drvlist,
                                 LCFGOption options,
                                 char ** result, size_t * size ) {
  assert( drvlist != NULL );

  size_t new_len = 0;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(drvlist);
        cur_node != NULL;
        cur_node = lcfgslist_next(cur_node) ) {

    const LCFGDerivation * drv = lcfgslist_data(cur_node);

    /* Ignore any derivations which do not have a name or value */
    if ( !lcfgderivation_is_valid(drv) ) continue;

    ssize_t drv_len = lcfgderivation_get_length(drv);
    if ( drv_len > 0 ) {
      new_len += drv_len;
      if ( cur_node != lcfgslist_head(drvlist) )
        new_len++; /* +1 for " " (single space) separator */
    }

  }

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

  for ( cur_node = lcfgslist_head(drvlist);
        cur_node != NULL;
        cur_node = lcfgslist_next(cur_node) ) {

    const LCFGDerivation * drv = lcfgslist_data(cur_node);

    /* Ignore any derivations which do not have a name or value */
    if ( !lcfgderivation_is_valid(drv) ) continue;

    ssize_t drv_len = lcfgderivation_get_length(drv);
    if ( drv_len > 0 ) {
      if ( cur_node != lcfgslist_head(drvlist) ) {
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
      if ( cur_node != lcfgslist_head(drvlist) ) {
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
