
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "components.h"

LCFGResourceIterator * lcfgresiter_new( LCFGComponent * component,
                                        bool manage ) {

  LCFGResourceIterator * iterator = malloc( sizeof(LCFGResourceIterator) );
  if ( iterator == NULL ) {
    perror( "Failed to allocate memory for LCFG resource iterator" );
    exit(EXIT_FAILURE);
  }

  iterator->component = component;
  iterator->current   = NULL;
  iterator->done      = false;
  iterator->manage    = manage;

  return iterator;
}

void lcfgresiter_destroy( LCFGResourceIterator * iterator ) {

  if ( iterator == NULL )
    return;

  if ( iterator->manage && iterator->component != NULL ) {
    lcfgcomponent_destroy(iterator->component);
  }

  iterator->component = NULL;
  iterator->current = NULL;

  free(iterator);
  iterator = NULL;

}

void lcfgresiter_reset( LCFGResourceIterator * iterator ) {
  iterator->current = NULL;
  iterator->done    = false;
}

bool lcfgresiter_has_next( LCFGResourceIterator * iterator ) {

  bool result = false;

  if ( !iterator->done ) {

    /* not yet started and something available */
    if ( iterator->current == NULL &&
         iterator->component != NULL && lcfgcomponent_size(iterator->component) > 0 ) {
      result = true;
    } else {

      /* started and more to come */
      if ( iterator->current != NULL &&
           lcfgcomponent_next(iterator->current) != NULL ) {
        result = true;
      }
    }

  }

  return result;
}

LCFGResource * lcfgresiter_next(LCFGResourceIterator * iterator) {

  if ( iterator->done ) {
    return NULL;
  }

  if ( !lcfgresiter_has_next(iterator) ) {
    iterator->done = true;
    return NULL;
  }

  if ( iterator->current == NULL ) {
    iterator->current = lcfgcomponent_head(iterator->component);
  } else {
    iterator->current = lcfgcomponent_next(iterator->current);
  }

  LCFGResource * res = NULL;
  if ( iterator->current != NULL ) {
    res = lcfgcomponent_resource(iterator->current);
  }

  return res;
}

/* eof */
