/**
 * @file common/derivation/map.c
 * @brief Functions for working with maps of LCFG derivation lists.
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2018 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#include "common.h"
#include "utils.h"
#include "derivation.h"
#include "farmhash.h"

static double lcfgderivmap_load_factor( const LCFGDerivationMap * drvmap ) {
  return ( (double) drvmap->entries / (double) drvmap->buckets );
}

static void lcfgderivmap_resize( LCFGDerivationMap * drvmap ) {

  double load_factor = lcfgderivmap_load_factor(drvmap);

  size_t want_buckets = drvmap->buckets;
  if ( load_factor >= LCFG_DRVMAP_LOAD_MAX ) {
    want_buckets = (size_t) 
      ( (double) drvmap->entries / LCFG_DRVMAP_LOAD_INIT ) + 1;
  }

  LCFGDerivationList ** cur_set = drvmap->derivations;
  size_t cur_buckets = drvmap->buckets;

  /* Decide if a resize is actually required */

  if ( cur_set != NULL && want_buckets <= cur_buckets ) return;

  /* Resize the hash */

  LCFGDerivationList ** new_set = calloc( (size_t) want_buckets,
                                          sizeof(LCFGDerivationList *) );
  if ( new_set == NULL ) {
    perror( "Failed to allocate memory for LCFG derivations map" );
    exit(EXIT_FAILURE);
  }

  drvmap->derivations = new_set;
  drvmap->entries     = 0;
  drvmap->buckets     = want_buckets;

  if ( cur_set != NULL ) {

    unsigned long i;
    for ( i=0; i<cur_buckets; i++ ) {
      LCFGDerivationList * drvlist = cur_set[i];

      if ( !lcfgderivlist_is_empty(drvlist) ) {

        char * insert_msg = NULL;

        LCFGChange insert_rc =
          lcfgdrvmap_insert_list( drvmap, drvlist, &insert_msg );

        if ( insert_rc == LCFG_CHANGE_ERROR ) {
          fprintf( stderr, "Failed to resize derivation map: %s\n", insert_msg );
          free(insert_msg);
          exit(EXIT_FAILURE);
        }

        free(insert_msg);
      }

      lcfgderivlist_relinquish(drvlist);
      cur_set[i] = NULL;
    }

    free(cur_set);
  }

}

/**
  * @brief Create and initialise a new derivation map
  *
  * Creates a new @c LCFGDerivationMap and initialises the parameters
  * to the default values.
  *
  * This is used as a fast lookup store for a set of @c
  * LCFGDerivationList structures.
  *
  * If the memory allocation for the new structure is not successful
  * the @c exit() function will be called with a non-zero value.
  *
  * The reference count for the structure is initialised to 1. To avoid
  * memory leaks, when it is no longer required the
  * @c lcfgderivmap_relinquish() function should be called.
  *
  * @return Pointer to new @c LCFGDerivationMap
  *
  */

LCFGDerivationMap * lcfgderivmap_new(void) {

  LCFGDerivationMap * drvmap = calloc( 1, sizeof(LCFGDerivationMap) );
  if ( drvmap == NULL ) {
    perror( "Failed to allocate memory for LCFG derivation map" );
    exit(EXIT_FAILURE);
  }

  /* Default values */

  drvmap->derivations = NULL;
  drvmap->entries     = 0;
  drvmap->buckets     = LCFG_DRVMAP_DEFAULT_SIZE;
  drvmap->_refcount   = 1;

  lcfgderivmap_resize(drvmap);

  return drvmap;
}

/**
 * @brief Destroy the derivation map
 *
 * When the specified @c LCFGDerivationMap is no longer required this
 * will free all associated memory. There is support for reference
 * counting so typically the @c lcfgderivmap_relinquish() function
 * should be used.
 *
 * This will call @c free() on each parameter of the structure and
 * then set each value to be @c NULL.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * derivation map which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] drvmap Pointer to @c LCFGDerivationMap to be destroyed.
 *
 */

static void lcfgderivmap_destroy(LCFGDerivationMap * drvmap) {

  if ( drvmap == NULL ) return;

  LCFGDerivationList ** derivations = drvmap->derivations;

  unsigned long i;
  for ( i=0; i<drvmap->buckets; i++ ) {
    if ( derivations[i] ) {
      lcfgderivlist_relinquish( derivations[i] );
      derivations[i] = NULL;
    }
  }

  free(drvmap->derivations);
  drvmap->derivations = NULL;

  free(drvmap);
  drvmap = NULL;
}

/**
 * @brief Acquire reference to derivation map
 *
 * This is used to record a reference to the @c LCFGDerivationMap, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgderivmap_relinquish() function should
 * be called.
 *
 * @param[in] drvmap Pointer to @c LCFGDerivationMap
 *
 */

void lcfgderivmap_acquire( LCFGDerivationMap * drvmap ) {
  assert( drvmap != NULL );

  drvmap->_refcount += 1;
}

/**
 * @brief Release reference to derivation map
 *
 * This is used to release a reference to the @c LCFGDerivationMap,
 * it does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgderivmap_destroy() function
 * will be called to clean up the memory associated with the structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * derivation map which has already been destroyed (or potentially
 * was never created).
 *
 * @param[in] drvmap Pointer to @c LCFGDerivationMap
 *
 */

void lcfgderivmap_relinquish( LCFGDerivationMap * drvmap ) {

  if ( drvmap == NULL ) return;

  if ( drvmap->_refcount > 0 )
    drvmap->_refcount -= 1;

  if ( drvmap->_refcount == 0 )
    lcfgderivmap_destroy(drvmap);

}

/**
 * @brief Check if there are multiple references to the derivation map
 *
 * The @c LCFGDerivationMap structure supports reference counting - see
 * @c lcfgderivmap_acquire() and @c lcfgderivmap_relinquish(). This
 * will return a boolean which indicates if there are multiple
 * references.
 *
 * @param[in] drvmap Pointer to @c LCFGDerivationMap
 *
 * @return Boolean which indicates if there are multiple references to this derivation map
 *
 */

bool lcfgderivmap_is_shared( const LCFGDerivationMap * drvmap ) {
  assert( drvmap != NULL );
  return ( drvmap->_refcount > 1 );
}

/**
 * @brief Insert derivation list into map
 *
 * This can be used to merge an @c LCFGDerivationList into a @c
 * LCFGDerivationMap. If there is not currently any entry in the map
 * with the same id then it will be inserted and @c LCFG_CHANGE_ADDED
 * will be returned. If an entry already exists in the map with the
 * same id it will simply be replaced and @c LCFG_CHANGE_REPLACED will
 * be returned.
 *
 * @param[in] drvmap Pointer to @c LCFGDerivationMap
 * @param[in] drvlist Pointer to @c LCFGDerivationList to be inserted
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgdrvmap_insert_list( LCFGDerivationMap * drvmap,
                                   LCFGDerivationList * drvlist,
                                   char ** msg ) {
  assert( drvmap != NULL );

  if ( lcfgderivlist_is_empty(drvlist) ) return LCFG_CHANGE_NONE;

  uint64_t id = drvlist->id;
  unsigned long hash = id % drvmap->buckets;

  LCFGDerivationList ** derivations = drvmap->derivations;

  bool done = false;
  unsigned long i, slot;
  for ( i = hash; !done && i < drvmap->buckets; i++ ) {

    if ( !derivations[i] || derivations[i]->id == id ) {
      done = true;
      slot = i;
    }

  }

  for ( i = 0; !done && i < hash; i++ ) {

    if ( !derivations[i] || derivations[i]->id == id ) {
      done = true;
      slot = i;
    }

  }

  LCFGChange change = LCFG_CHANGE_NONE;

  if ( !done ) {
    lcfgutils_build_message( msg,
                             "No free space for new entries in derivation map" );
    change = LCFG_CHANGE_ERROR;
  } else if ( derivations[slot] ) {

    LCFGDerivationList * old_drvlist = derivations[slot];
    lcfgderivlist_acquire(drvlist);
    derivations[slot] = drvlist;
    lcfgderivlist_relinquish(old_drvlist);
    change = LCFG_CHANGE_REPLACED;

  } else {

    lcfgderivlist_acquire(drvlist);
    derivations[slot] = drvlist;
    change = LCFG_CHANGE_ADDED;

    drvmap->entries += 1;
    lcfgderivmap_resize(drvmap);
  }

  return change;
}

/**
 * @brief Insert derivation string into map
 *
 * This takes a list of derivations in string form and does a lookup
 * in the @c LCFGDerivationMap to see if a @c LCFGDerivationList
 * already exists. The id for the string is found by hashing the
 * entire string using the @c farmhash64() function. If no entry with
 * the same id is found in the map then the string is parsed and a new
 * @c LCFGDerivationList is created which is stored into the map and
 * also returned to the caller.
 *
 * If the returned @c LCFGDerivationList must remain available after
 * the @c LCFGDerivationMap is destroyed the @c
 * lcfgderivlist_acquire() function must be used to register a
 * reference (and thus @c lcfgderivlist_relinquish() would need to be
 * called when the list is no longer required).
 *
 * If an error occurs this function will return a @c NULL value and a
 * diagnostic message will be given.
 *
 * @param[in] drvmap Pointer to @c LCFGDerivationMap
 * @param[in] deriv_as_str String of derivations
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Pointer to an @c LCFGDerivationList for the string
 *
 */

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
  unsigned long hash = id % drvmap->buckets;

  LCFGDerivationList ** derivations = drvmap->derivations;

  bool done = false;
  unsigned long i, slot;
  for ( i = hash; !done && i < drvmap->buckets; i++ ) {

    if ( !derivations[i] || derivations[i]->id == id ) {
      done = true;
      slot = i;
    }

  }

  for ( i = 0; !done && i < hash; i++ ) {

    if ( !derivations[i] || derivations[i]->id == id ) {
      done = true;
      slot = i;
    }

  }

  LCFGDerivationList * result = NULL;

  if ( !done ) {
    lcfgutils_build_message( msg,
                             "No free space for new entries in derivation map" );
  } else if ( derivations[slot] ) {
    result = derivations[slot];
  } else {

    LCFGDerivationList * drvlist = NULL;
    LCFGStatus parse_rc = lcfgderivlist_from_string( deriv_as_str,
                                                     &drvlist, msg );

    if ( parse_rc != LCFG_STATUS_ERROR && !lcfgderivlist_is_empty(drvlist) ) {
      drvlist->id = id;
      derivations[slot] = drvlist;

      drvmap->entries += 1;
      lcfgderivmap_resize(drvmap);

      result = drvlist;
    } else {
      lcfgderivlist_relinquish(drvlist);
    }

  }

  return result;
}

/* eof */
