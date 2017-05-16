#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "utils.h"

LCFGSListNode * lcfgslistnode_new( void * data ) {
  assert( data != NULL );

  LCFGSListNode * node = malloc( sizeof(LCFGSListNode) );
  if ( node == NULL ) {
    perror( "Failed to allocate memory for list node" );
    exit(EXIT_FAILURE);
  }

  /* Set default values */

  node->data = data;
  node->next = NULL;

  return node;
}

void lcfgslistnode_destroy(LCFGSListNode * node) {

  if ( node == NULL ) return;

  node->data = NULL;
  node->next = NULL;

  free(node);
  node = NULL;

}

/* eof */
