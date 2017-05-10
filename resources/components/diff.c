#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "utils.h"
#include "differences.h"

LCFGDiffComponent * lcfgdiffcomponent_new(void) {

  LCFGDiffComponent * compdiff = malloc( sizeof(LCFGDiffComponent) );
  if ( compdiff == NULL ) {
    perror( "Failed to allocate memory for LCFG component diff" );
    exit(EXIT_FAILURE);
  }

  /* Set default values */
  compdiff->name = NULL;
  compdiff->head = NULL;
  compdiff->tail = NULL;
  compdiff->size = 0;
  compdiff->change_type = LCFG_CHANGE_NONE;

  return compdiff;
}

void lcfgdiffcomponent_destroy(LCFGDiffComponent * compdiff ) {

  if ( compdiff == NULL ) return;

  while ( lcfgdiffcomponent_size(compdiff) > 0 ) {
    LCFGDiffResource * resdiff = NULL;
    if ( lcfgdiffcomponent_remove_next( compdiff, NULL, &resdiff )
         == LCFG_CHANGE_REMOVED ) {
      lcfgdiffresource_relinquish(resdiff);
    }
  }

  free(compdiff->name);
  compdiff->name = NULL;

  free(compdiff);
  compdiff = NULL;

}

void lcfgdiffcomponent_acquire( LCFGDiffResource * compdiff ) {
  assert( compdiff != NULL );

  compdiff->_refcount += 1;
}

void lcfgdiffcomponent_relinquish( LCFGDiffResource * compdiff ) {

  if ( compdiff == NULL ) return;

  if ( compdiff->_refcount > 0 )
    compdiff->_refcount -= 1;

  if ( compdiff->_refcount == 0 )
    lcfgdiffresource_destroy(compdiff);

}

bool lcfgdiffcomponent_has_name(const LCFGDiffComponent * compdiff) {
  return !isempty(compdiff->name);
}

char * lcfgdiffcomponent_get_name(const LCFGDiffComponent * compdiff) {
  return compdiff->name;
}

bool lcfgdiffcomponent_set_name( LCFGDiffComponent * compdiff,
                                 char * new_name ) {

  bool ok = false;
  if ( lcfgcomponent_valid_name(new_name) ) {
    free(compdiff->name);

    compdiff->name = new_name;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

void lcfgdiffcomponent_set_type( LCFGDiffComponent * compdiff,
                                 LCFGChange change_type ) {
  compdiff->change_type = change_type;
}

LCFGChange lcfgdiffcomponent_get_type( const LCFGDiffComponent * compdiff ) {
  return compdiff->change_type;
}

bool lcfgdiffcomponent_is_changed( const LCFGDiffComponent * compdiff ) {
  LCFGChange difftype = lcfgdiffcomponent_get_type(compdiff);
  return ( difftype == LCFG_CHANGE_ADDED   ||
	   difftype == LCFG_CHANGE_REMOVED ||
	   difftype == LCFG_CHANGE_MODIFIED );
}

bool lcfgdiffcomponent_is_nochange( const LCFGDiffComponent * compdiff ) {
  return ( compdiff->change_type == LCFG_CHANGE_NONE );
}

bool lcfgdiffcomponent_is_added( const LCFGDiffComponent * compdiff ) {
  return ( compdiff->change_type == LCFG_CHANGE_ADDED );
}

bool lcfgdiffcomponent_is_modified( const LCFGDiffComponent * compdiff ) {
  return ( compdiff->change_type == LCFG_CHANGE_MODIFIED );
}

bool lcfgdiffcomponent_is_removed( const LCFGDiffComponent * compdiff ) {
  return ( compdiff->change_type == LCFG_CHANGE_REMOVED );
}

LCFGChange lcfgdiffcomponent_insert_next( LCFGDiffComponent * list,
                                          LCFGSListNode     * node,
                                          LCFGDiffResource  * item ) {
  assert( list != NULL );

  LCFGSListNode * new_node = lcfgslistnode_new(item);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgdiffresource_acquire(item);

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

LCFGChange lcfgdiffcomponent_remove_next( LCFGDiffComponent * list,
                                          LCFGSListNode     * node,
                                          LCFGDiffResource ** item ) {
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

void lcfgdiffcomponent_sort( LCFGDiffComponent * list ) {
  assert( list != NULL );

  if ( lcfgslist_size(list) < 2 ) return;

  /* Oo. Oo. bubble sort .oO .oO */

  bool swapped=true;
  while (swapped) {
    swapped=false;

    LCFGSListNode * cur_node = NULL;
    for ( cur_node = lcfgslist_head(list);
          cur_node != NULL && lcfgslist_has_next(cur_node);
          cur_node = lcfgslist_next(cur_node) ) {

      LCFGDiffResource * data1 = lcfgslist_data(cur_node);
      LCFGDiffResource * data2 = lcfgslist_data(cur_node->next);

      if ( lcfgdiffresource_compare( data1, data2 ) > 0 ) {
        cur_node->data       = data2;
        cur_node->next->data = data1;
        swapped = true;
      }

    }
  }

}

LCFGSListNode * lcfgdiffcomponent_find_node( const LCFGDiffComponent * list,
                                             const char * want_name ) {
  assert( want_name != NULL );

  if ( lcfgslist_is_empty(list) ) return NULL;

  LCFGSListNode * result = NULL;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(list);
	cur_node != NULL && result == NULL;
	cur_node = lcfgslist_next(cur_node) ) {

    const LCFGDiffResource * resdiff = lcfgslist_data(cur_node); 

    if ( lcfgdiffresource_match( resdiff, want_name ) )
      result = (LCFGSListNode *) cur_node;

  }

  return result;
}

LCFGDiffResource * lcfgdiffcomponent_find_resource(
					  const LCFGDiffComponent * list,
					  const char * want_name ) {
  assert( want_name != NULL );

  LCFGDiffResource * resdiff = NULL;

  const LCFGSListNode * node = lcfgdiffcomponent_find_node( list, want_name );
  if ( node != NULL )
    resdiff = lcfgslist_data(node);

  return resdiff;
}

bool lcfgdiffcomponent_has_resource( const LCFGDiffComponent * list,
				     const char * want_name ) {
  assert( want_name != NULL );

  return ( lcfgdiffcomponent_find_node( list, want_name ) != NULL );
}

LCFGStatus lcfgdiffcomponent_to_holdfile( const LCFGDiffComponent * compdiff,
                                          FILE * holdfile,
                                          md5_state_t * md5state ) {

  if ( lcfgdiffcomponent_is_empty(compdiff) ) return LCFG_STATUS_OK;

  const char * prefix = lcfgdiffcomponent_has_name(compdiff) ?
                        lcfgdiffcomponent_get_name(compdiff) : NULL;

  size_t buf_size = 512;
  char * buffer = calloc( buf_size, sizeof(char) );
  if ( buffer == NULL ) {
    perror( "Failed to allocate memory for LCFG resource diff buffer" );
    exit(EXIT_FAILURE);
  }

  bool ok = true;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgdiffcomponent_head(compdiff);
	cur_node != NULL && ok;
	cur_node = lcfgdiffcomponent_next(cur_node) ) {

    const LCFGDiffResource * resdiff = lcfgdiffcomponent_resdiff(cur_node);

    ssize_t rc = lcfgdiffresource_to_hold( resdiff, prefix,
                                           &buffer, &buf_size );

    if ( rc > 0 ) {

      if ( md5state != NULL )
        lcfgutils_md5_append( md5state, (const md5_byte_t *) buffer, rc );

      if ( fputs( buffer, holdfile ) < 0 )
        ok = false;

    } else if ( rc < 0 ) {
      ok = false;
    }

  }

  free(buffer);

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

LCFGStatus lcfgcomponent_diff( const LCFGComponent * comp1,
                               const LCFGComponent * comp2,
                               LCFGDiffComponent ** result ) {

  LCFGDiffComponent * compdiff = lcfgdiffcomponent_new();

  const char * name = NULL;
  if ( comp1 != NULL && lcfgcomponent_has_name(comp1) ) {
    name = lcfgcomponent_get_name(comp1);
  } else if ( comp2 != NULL && lcfgcomponent_has_name(comp2) ) {
    name = lcfgcomponent_get_name(comp2);
  }

  bool ok = true;

  if ( name != NULL ) {
    char * new_name = strdup(name);
    if ( !lcfgdiffcomponent_set_name( compdiff, new_name ) ) {
      ok = false;
      free(new_name);
    }
  }

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp1);
	cur_node != NULL;
	cur_node = lcfgcomponent_next(cur_node) ) {

    LCFGResource * res1 = lcfgcomponent_resource(cur_node);

    /* Only diff 'active' resources which have a name attribute */
    if ( !lcfgresource_is_active(res1) || !lcfgresource_has_name(res1) )
      continue;

    const char * res1_name = lcfgresource_get_name(res1);

    LCFGResource * res2 = comp2 != NULL ?
      lcfgcomponent_find_resource( comp2, res1_name, false ) : NULL;

    LCFGDiffResource * resdiff = NULL;
    LCFGStatus rc = lcfgresource_diff( res1, res2, &resdiff );
    if ( rc == LCFG_STATUS_ERROR )
      ok = false;

    /* Just ignore anything where there are no differences */

    bool wanted = true;
    if ( ok && !lcfgdiffresource_is_nochange(resdiff) ) {

      if ( !lcfgdiffcomponent_append( compdiff, resdiff ) )
        ok = false;

    } else {
      wanted = false;
    }

    if ( !wanted || !ok )
      lcfgdiffresource_destroy(resdiff);

  }


  /* Look for resources which have been added */

  for ( cur_node = lcfgcomponent_head(comp1);
	cur_node != NULL;
	cur_node = lcfgcomponent_next(cur_node) ) {

    LCFGResource * res2 = lcfgcomponent_resource(cur_node);

    /* Only diff 'active' resources which have a name attribute */
    if ( !lcfgresource_is_active(res2) || !lcfgresource_has_name(res2) )
      continue;

    const char * res2_name = lcfgresource_get_name(res2);

    if ( comp1 == NULL ||
	 !lcfgcomponent_has_resource( comp1, res2_name, false ) ) {

      LCFGDiffResource * resdiff = NULL;
      LCFGStatus rc = lcfgresource_diff( NULL, res2, &resdiff );
      if ( rc == LCFG_STATUS_ERROR )
        ok = false;

      if (ok) {

        if ( !lcfgdiffcomponent_append( compdiff, resdiff ) )
          ok = false;

      }

      if ( !ok )
        lcfgdiffresource_destroy(resdiff);

    }

  }

  if ( ok ) {

    LCFGChange change_type = LCFG_CHANGE_NONE;
    if ( comp1 != NULL ) {
      if ( comp2 == NULL ) {
        change_type = LCFG_CHANGE_REMOVED;
      } else if ( compdiff->size > 0 ) {
        change_type = LCFG_CHANGE_MODIFIED;
      }
    } else if ( comp2 != NULL ) {
      change_type = LCFG_CHANGE_ADDED;
    }

    lcfgdiffcomponent_set_type( compdiff, change_type );

  } else {
    lcfgdiffcomponent_destroy(compdiff);
    compdiff = NULL;
  }

  *result = compdiff;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

LCFGChange lcfgcomplist_quickdiff( const LCFGComponentList * list1,
                                   const LCFGComponentList * list2,
                                   LCFGTagList ** modified,
                                   LCFGTagList ** added,
                                   LCFGTagList ** removed ) {

  *modified = NULL;
  *added    = NULL;
  *removed  = NULL;

  LCFGChange status = LCFG_CHANGE_NONE;

  /* Look for components which have been removed or modified */
  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = lcfgcomplist_head(list1);
	cur_node != NULL;
	cur_node = lcfgcomplist_next(cur_node) ) {

    const LCFGComponent * comp1 = lcfgcomplist_component(cur_node);
    const char * comp1_name     = lcfgcomponent_get_name(comp1);

    const LCFGComponent * comp2 =
      lcfgcomplist_find_component( list2, comp1_name );

    LCFGChange comp_diff = lcfgcomponent_quickdiff( comp1, comp2 );
    if ( comp_diff != LCFG_CHANGE_NONE ) {

      LCFGTagList ** taglist = NULL;

      switch(comp_diff)
        {
        case LCFG_CHANGE_ERROR:
          status = LCFG_CHANGE_ERROR;
          break;
        case LCFG_CHANGE_MODIFIED:
          printf("%s is modified\n",comp1_name);
          status  = LCFG_CHANGE_MODIFIED;
          taglist = &( *modified );
          break;
        case LCFG_CHANGE_REMOVED:
          printf("%s is removed\n",comp1_name);
          status  = LCFG_CHANGE_MODIFIED;
          taglist = &( *removed );
          break;
        }

      if ( taglist != NULL ) { /* A component name needs storing */

        if ( *taglist == NULL )
          *taglist = lcfgtaglist_new();

        char * tagmsg = NULL;
        if ( lcfgtaglist_mutate_add( *taglist, comp1_name, &tagmsg )
             == LCFG_CHANGE_ERROR ) {
          status = LCFG_CHANGE_ERROR;
        }
        free(tagmsg);

      }

    }

    if ( status == LCFG_CHANGE_ERROR )
      goto cleanup;

  }

  /* Look for components which have been added */

  for ( cur_node = lcfgcomplist_head(list2);
	cur_node != NULL;
	cur_node = lcfgcomplist_next(cur_node) ) {

    const LCFGComponent * comp2 = lcfgcomplist_component(cur_node);
    const char * comp2_name     = lcfgcomponent_get_name(comp2);

    if ( !lcfgcomplist_has_component( list1, comp2_name ) ) {

      if ( *added == NULL )
        *added = lcfgtaglist_new();

      char * tagmsg = NULL;
      if ( lcfgtaglist_mutate_add( *added, comp2_name, &tagmsg )
           == LCFG_CHANGE_ERROR ) {
        status = LCFG_CHANGE_ERROR;
      }
      free(tagmsg);

    }

    if ( status == LCFG_CHANGE_ERROR )
      goto cleanup;

  }

 cleanup:

  if ( status == LCFG_CHANGE_ERROR || status == LCFG_CHANGE_NONE ) {
    lcfgtaglist_relinquish(*modified);
    *modified = NULL;
    lcfgtaglist_relinquish(*added);
    *added    = NULL;
    lcfgtaglist_relinquish(*removed);
    *removed  = NULL;
  }

  return status;
}

LCFGChange lcfgcomponent_quickdiff( const LCFGComponent * comp1,
                                      const LCFGComponent * comp2 ) {

  if ( comp1 == NULL ) {

    if ( comp2 == NULL ) {
      return LCFG_CHANGE_NONE;
    } else if ( comp2 != NULL ) {
      return LCFG_CHANGE_ADDED;
    }

  } else {

    if ( comp2 == NULL ) {
      return LCFG_CHANGE_REMOVED;
    } else if ( lcfgcomponent_size(comp1) != lcfgcomponent_size(comp2) ) {
      return LCFG_CHANGE_MODIFIED;
    }

  }

  LCFGChange status = LCFG_CHANGE_NONE;

  /* Look for resources which have been removed or modified */
  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp1);
	cur_node != NULL;
	cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res1 = lcfgcomponent_resource(cur_node);

    /* Only diff 'active' resources which have a name attribute */
    if ( !lcfgresource_is_active(res1) || !lcfgresource_has_name(res1) )
      continue;

    const char * res1_name = lcfgresource_get_name(res1);

    const LCFGResource * res2 =
      lcfgcomponent_find_resource( comp2, res1_name, false );

    if ( res2 == NULL || !lcfgresource_same_value( res1, res2 ) ) {
      status = LCFG_CHANGE_MODIFIED;
      break;
    }

  }

  if ( status == LCFG_CHANGE_NONE ) {

    /* Look for resources which have been added */

    for ( cur_node = lcfgcomponent_head(comp2);
          cur_node != NULL;
          cur_node = lcfgcomponent_next(cur_node) ) {

      const LCFGResource * res2 = lcfgcomponent_resource(cur_node);

      /* Only diff 'active' resources which have a name attribute */
      if ( !lcfgresource_is_active(res2) || !lcfgresource_has_name(res2) )
	continue;

      const char * res2_name = lcfgresource_get_name(res2);

      if ( !lcfgcomponent_has_resource( comp1, res2_name, false ) ) {
        status = LCFG_CHANGE_MODIFIED;
        break;
      }

    }

  }

  return status;
}

/* eof */
