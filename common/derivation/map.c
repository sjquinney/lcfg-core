#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#include "common.h"
#include "utils.h"
#include "derivation.h"
#include "farmhash.h"

static LCFGChange lcfgderivmap_insert_next( LCFGDerivationMap   * map,
                                            LCFGSListNode       * node,
                                            LCFGDerivationList  * item )
  __attribute__((warn_unused_result));

static LCFGChange lcfgderivmap_remove_next( LCFGDerivationMap   * map,
                                            LCFGSListNode       * node,
                                            LCFGDerivationList ** item )
  __attribute__((warn_unused_result));

#define lcfgderivmap_append(drvmap, drvlist) ( lcfgderivmap_insert_next( drvmap, lcfgslist_tail(drvmap), drvlist ) )

LCFGDerivationMap * lcfgderivmap_new(void) {

  LCFGDerivationMap * drvmap = calloc( 1, sizeof(LCFGDerivationMap) );
  if ( drvmap == NULL ) {
    perror( "Failed to allocate memory for LCFG derivation map" );
    exit(EXIT_FAILURE);
  }

  /* Default values */

  drvmap->size      = 0;
  drvmap->head      = NULL;
  drvmap->tail      = NULL;
  drvmap->_refcount = 1;

  return drvmap;
}

void lcfgderivmap_destroy(LCFGDerivationMap * drvmap) {

  if ( drvmap == NULL ) return;

  while ( lcfgslist_size(drvmap) > 0 ) {
    LCFGDerivationList * drvlist = NULL;
    if ( lcfgderivmap_remove_next( drvmap, NULL,
                                   &drvlist ) == LCFG_CHANGE_REMOVED ) {
      lcfgderivlist_relinquish(drvlist);
    }
  }

  free(drvmap);
  drvmap = NULL;

}

void lcfgderivmap_acquire( LCFGDerivationMap * drvmap ) {
  assert( drvmap != NULL );

  drvmap->_refcount += 1;
}

void lcfgderivmap_relinquish( LCFGDerivationMap * drvmap ) {

  if ( drvmap == NULL ) return;

  if ( drvmap->_refcount > 0 )
    drvmap->_refcount -= 1;

  if ( drvmap->_refcount == 0 )
    lcfgderivmap_destroy(drvmap);

}

static LCFGChange lcfgderivmap_insert_next( LCFGDerivationMap  * list,
                                            LCFGSListNode      * node,
                                            LCFGDerivationList * item ) {
  assert( list != NULL );
  assert( item != NULL );

  LCFGSListNode * new_node = lcfgslistnode_new(item);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgderivlist_acquire(item);

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

static LCFGChange lcfgderivmap_remove_next( LCFGDerivationMap   * list,
                                            LCFGSListNode       * node,
                                            LCFGDerivationList ** item ) {
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

LCFGSListNode * lcfgderivmap_find_node( const LCFGDerivationMap * drvmap,
                                        uint64_t want_id ) {

  if ( lcfgslist_is_empty(drvmap) ) return NULL;

  LCFGSListNode * result = NULL;

  LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(drvmap);
        cur_node != NULL && result == NULL;
        cur_node = lcfgslist_next(cur_node) ) {

    const LCFGDerivationList * drvlist = lcfgslist_data(cur_node);

    if ( drvlist != NULL && drvlist->id == want_id )
      result = cur_node;

  }

  return result;
}

LCFGDerivationList * lcfgderivmap_find_derivation(
                                              const LCFGDerivationMap * drvmap,
                                              uint64_t want_id ) {

  LCFGDerivationList * drvlist = NULL;

  const LCFGSListNode * node = lcfgderivmap_find_node( drvmap, want_id );
  if ( node != NULL )
    drvlist = lcfgslist_data(node);

  return drvlist;
}

bool lcfgderivmap_contains( const LCFGDerivationMap * drvmap,
                            uint64_t want_id ) {

  return ( lcfgderivmap_find_node( drvmap, want_id ) != NULL );
}

LCFGDerivationList * lcfgderivmap_find_or_insert_string(LCFGDerivationMap * drvmap,
                                                        const char * deriv_as_str,
                                                        char ** msg ) {
  assert( drvmap != NULL );

  if ( isempty(deriv_as_str) ) {
    lcfgutils_build_message( msg, "Empty derivation" );
    return NULL;
  }

  size_t len = strlen(deriv_as_str);
  uint64_t id = farmhash64( deriv_as_str, len );

  LCFGDerivationList * drvlist = lcfgderivmap_find_derivation( drvmap, id );

  if ( drvlist == NULL ) {

    bool ok = false;
    LCFGStatus parse_rc = lcfgderivlist_from_string( deriv_as_str,
                                                     &drvlist, msg );
    if ( parse_rc != LCFG_STATUS_ERROR ) {
      drvlist->id = id;
      LCFGChange append_rc = lcfgderivmap_append( drvmap, drvlist );
      ok = LCFGChangeOK(append_rc);
    }

    lcfgderivlist_relinquish(drvlist);

    if (!ok)
      drvlist = NULL;
  }

  return drvlist;
}

/* eof */
