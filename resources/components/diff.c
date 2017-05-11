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
  compdiff->_refcount = 1;

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

void lcfgdiffcomponent_acquire( LCFGDiffComponent * compdiff ) {
  assert( compdiff != NULL );

  compdiff->_refcount += 1;
}

void lcfgdiffcomponent_relinquish( LCFGDiffComponent * compdiff ) {

  if ( compdiff == NULL ) return;

  if ( compdiff->_refcount > 0 )
    compdiff->_refcount -= 1;

  if ( compdiff->_refcount == 0 )
    lcfgdiffcomponent_destroy(compdiff);

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

      LCFGDiffResource * item1 = lcfgslist_data(cur_node);
      LCFGDiffResource * item2 = lcfgslist_data(cur_node->next);

      if ( lcfgdiffresource_compare( item1, item2 ) > 0 ) {
        cur_node->data       = item2;
        cur_node->next->data = item1;
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

    const LCFGDiffResource * item = lcfgslist_data(cur_node); 

    if ( lcfgdiffresource_match( item, want_name ) )
      result = (LCFGSListNode *) cur_node;

  }

  return result;
}

LCFGDiffResource * lcfgdiffcomponent_find_resource(
					  const LCFGDiffComponent * list,
					  const char * want_name ) {
  assert( want_name != NULL );

  LCFGDiffResource * item = NULL;

  const LCFGSListNode * node = lcfgdiffcomponent_find_node( list, want_name );
  if ( node != NULL )
    item = lcfgslist_data(node);

  return item;
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

  /* Try to get the name from either component */

  const char * name = NULL;
  if ( comp1 != NULL && lcfgcomponent_has_name(comp1) )
    name = lcfgcomponent_get_name(comp1);
  else if ( comp2 != NULL && lcfgcomponent_has_name(comp2) )
    name = lcfgcomponent_get_name(comp2);

  LCFGStatus status = LCFG_STATUS_OK;

  if ( name != NULL ) {
    char * new_name = strdup(name);
    if ( !lcfgdiffcomponent_set_name( compdiff, new_name ) ) {
      status = LCFG_STATUS_ERROR;
      free(new_name);
    }
  }

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = comp1 != NULL ? lcfgcomponent_head(comp1) : NULL;
	cur_node != NULL && status != LCFG_STATUS_ERROR;
	cur_node = lcfgcomponent_next(cur_node) ) {

    LCFGResource * res1 = lcfgcomponent_resource(cur_node);

    /* Only diff 'active' resources which have a name attribute */
    if ( !lcfgresource_is_active(res1) || !lcfgresource_is_valid(res1) )
      continue;

    const char * res1_name = lcfgresource_get_name(res1);

    LCFGResource * res2 = comp2 != NULL ?
      lcfgcomponent_find_resource( comp2, res1_name, false ) : NULL;

    LCFGDiffResource * resdiff = NULL;
    status = lcfgresource_diff( res1, res2, &resdiff );

    /* Just ignore anything where there are no differences */

    if ( status != LCFG_STATUS_ERROR &&
         !lcfgdiffresource_is_nochange(resdiff) ) {

      LCFGChange append_rc = lcfgdiffcomponent_append( compdiff, resdiff );
      if ( append_rc == LCFG_CHANGE_ERROR )
        status = LCFG_STATUS_ERROR;

    }

    lcfgdiffresource_relinquish(resdiff);
  }

  /* Look for resources which have been added */

  for ( cur_node = comp2 != NULL ? lcfgcomponent_head(comp2) : NULL;
	cur_node != NULL && status != LCFG_STATUS_ERROR;
	cur_node = lcfgcomponent_next(cur_node) ) {

    LCFGResource * res2 = lcfgcomponent_resource(cur_node);

    /* Only diff 'active' resources which have a name attribute */
    if ( !lcfgresource_is_active(res2) || !lcfgresource_is_valid(res2) )
      continue;

    const char * res2_name = lcfgresource_get_name(res2);

    /* Only interested in resources which are NOT in first component */

    if ( comp1 == NULL ||
	 !lcfgcomponent_has_resource( comp1, res2_name, false ) ) {

      LCFGDiffResource * resdiff = NULL;
      status = lcfgresource_diff( NULL, res2, &resdiff );

      if ( status != LCFG_STATUS_ERROR ) {

        LCFGChange append_rc = lcfgdiffcomponent_append( compdiff, resdiff );
        if ( append_rc == LCFG_CHANGE_ERROR )
          status = LCFG_STATUS_ERROR;

      }

      lcfgdiffresource_relinquish(resdiff);

    }

  }

  if ( status != LCFG_STATUS_ERROR ) {

    LCFGChange change_type = LCFG_CHANGE_NONE;
    if ( lcfgcomponent_is_empty(comp1) ) {

      if ( !lcfgcomponent_is_empty(comp2) )
        change_type = LCFG_CHANGE_ADDED;

    } else {

      if ( lcfgcomponent_is_empty(comp2) )
        change_type = LCFG_CHANGE_REMOVED;
      else if ( !lcfgdiffcomponent_is_empty( compdiff ) )
        change_type = LCFG_CHANGE_MODIFIED;

    }

    lcfgdiffcomponent_set_type( compdiff, change_type );

  } else {
    lcfgdiffcomponent_relinquish(compdiff);
    compdiff = NULL;
  }

  *result = compdiff;

  return status;
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
  for ( cur_node = list1 != NULL ? lcfgcomplist_head(list1) : NULL;
	cur_node != NULL && status != LCFG_CHANGE_ERROR;
	cur_node = lcfgcomplist_next(cur_node) ) {

    const LCFGComponent * comp1 = lcfgcomplist_component(cur_node);
    if ( !lcfgcomponent_is_valid(comp1) ) continue;

    const char * comp1_name = lcfgcomponent_get_name(comp1);

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
          status  = LCFG_CHANGE_MODIFIED;
          taglist = &( *modified );
          break;
        case LCFG_CHANGE_REMOVED:
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

  }

  /* Look for components which have been added */

  for ( cur_node = list2 != NULL ? lcfgcomplist_head(list2) : NULL;
	cur_node != NULL && status != LCFG_CHANGE_ERROR;
	cur_node = lcfgcomplist_next(cur_node) ) {

    const LCFGComponent * comp2 = lcfgcomplist_component(cur_node);
    if ( !lcfgcomponent_is_valid(comp2) ) continue;

    const char * comp2_name = lcfgcomponent_get_name(comp2);

    if ( !lcfgcomplist_has_component( list1, comp2_name ) ) {
      status = LCFG_CHANGE_MODIFIED;

      if ( *added == NULL )
        *added = lcfgtaglist_new();

      char * tagmsg = NULL;
      if ( lcfgtaglist_mutate_add( *added, comp2_name, &tagmsg )
           == LCFG_CHANGE_ERROR ) {
        status = LCFG_CHANGE_ERROR;
      }
      free(tagmsg);

    }

  }

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

    if ( comp2 == NULL )
      return LCFG_CHANGE_NONE;
    else if ( comp2 != NULL )
      return LCFG_CHANGE_ADDED;

  } else {

    if ( comp2 == NULL )
      return LCFG_CHANGE_REMOVED;
    else if ( lcfgcomponent_size(comp1) != lcfgcomponent_size(comp2) )
      return LCFG_CHANGE_MODIFIED;

  }

  LCFGChange status = LCFG_CHANGE_NONE;

  /* Look for resources which have been removed or modified */
  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = comp1 != NULL ? lcfgcomponent_head(comp1) : NULL;
	cur_node != NULL;
	cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res1 = lcfgcomponent_resource(cur_node);

    /* Only diff 'active' resources which have a name attribute */
    if ( !lcfgresource_is_active(res1) || !lcfgresource_is_valid(res1) )
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

    for ( cur_node = comp2 != NULL ? lcfgcomponent_head(comp2) : NULL;
          cur_node != NULL;
          cur_node = lcfgcomponent_next(cur_node) ) {

      const LCFGResource * res2 = lcfgcomponent_resource(cur_node);

      /* Only diff 'active' resources which have a name attribute */
      if ( !lcfgresource_is_active(res2) || !lcfgresource_is_valid(res2) )
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

bool lcfgdiffcomponent_match( const LCFGDiffComponent * compdiff,
                              const char * want_name ) {
  assert( compdiff != NULL );
  assert( want_name != NULL );

  const char * name = lcfgdiffcomponent_get_name(compdiff);

  return ( !isempty(name) && strcmp( name, want_name ) == 0 );
}

int lcfgdiffcomponent_compare( const LCFGDiffComponent * compdiff1, 
                               const LCFGDiffComponent * compdiff2 ) {

  const char * name1 = lcfgdiffcomponent_get_name(compdiff1);
  if ( name1 == NULL )
    name1 = "";

  const char * name2 = lcfgdiffcomponent_get_name(compdiff2);
  if ( name2 == NULL )
    name2 = "";

  return strcmp( name1, name2 );
}

/* eof */
