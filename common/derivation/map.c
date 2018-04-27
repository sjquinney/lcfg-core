/**
 * @file common/derivation/map.c
 * @brief Functions for working with lists of LCFG derivations
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

#define LCFG_DRVMAP_DEFAULT_SIZE 1000

LCFGDerivationMap * lcfgderivmap_new(void) {

  LCFGDerivationMap * drvmap = calloc( 1, sizeof(LCFGDerivationMap) );
  if ( drvmap == NULL ) {
    perror( "Failed to allocate memory for LCFG derivation map" );
    exit(EXIT_FAILURE);
  }

  /* Default values */

  drvmap->buckets     = LCFG_DRVMAP_DEFAULT_SIZE;
  drvmap->derivations = calloc( (size_t) drvmap->buckets,
                                sizeof(LCFGDerivationList *) );
  if ( drvmap->derivations == NULL ) {
    perror( "Failed to allocate memory for LCFG derivations map" );
    exit(EXIT_FAILURE);
  }

  drvmap->_refcount  = 1;

  return drvmap;
}

void lcfgderivmap_destroy(LCFGDerivationMap * drvmap) {

  if ( drvmap == NULL ) return;

  LCFGDerivationList ** derivations = drvmap->derivations;

  unsigned int i;
  for ( i=0; i<drvmap->buckets; i++ ) {
    lcfgderivlist_relinquish( derivations[i] );
    derivations[i] = NULL;
  }

  free(drvmap->derivations);
  drvmap->derivations = NULL;

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

LCFGDerivationList * lcfgderivmap_find_or_insert_string(LCFGDerivationMap * drvmap,
                                                        const char * deriv_as_str,
                                                        char ** msg ) {
  assert( drvmap != NULL );
  LCFGDerivationList * result = NULL;

  if ( isempty(deriv_as_str) ) {
    lcfgutils_build_message( msg, "Empty derivation" );
    return result;
  }

  size_t len = strlen(deriv_as_str);
  uint64_t id = farmhash64( deriv_as_str, len );

  LCFGDerivationList ** derivations = drvmap->derivations;

  unsigned int entry;
  for ( entry=0; result == NULL && entry<drvmap->buckets; entry++ ) {

    if ( !derivations[entry] )
      break;
    else if ( derivations[entry]->id == id )
      result = derivations[entry];

  }

  if ( result == NULL ) {

    if ( entry == drvmap->buckets ) {

      size_t new_size = drvmap->buckets * 2;
      LCFGDerivationList ** new_list =
        realloc( drvmap->derivations,
                 new_size * sizeof(LCFGDerivationList *) );
      if ( new_list == NULL ) {
        perror( "Failed to reallocate memory for LCFG derivation map" );
        exit(EXIT_FAILURE);
      }

      drvmap->buckets     = new_size;
      drvmap->derivations = new_list;

      /* initialise new entries */
      unsigned int i;
      for ( i=entry; i<drvmap->buckets; i++ )
        (drvmap->derivations)[i] = NULL;
    }

    LCFGDerivationList * drvlist = NULL;
    LCFGStatus parse_rc = lcfgderivlist_from_string( deriv_as_str,
                                                     &drvlist, msg );

    if ( parse_rc != LCFG_STATUS_ERROR && !lcfgderivlist_is_empty(drvlist) ) {
      drvlist->id = id;
      derivations[entry] = drvlist;
      result = drvlist;
    } else {
      lcfgderivlist_relinquish(drvlist);
    }

  }

  return result;
}

/* eof */
