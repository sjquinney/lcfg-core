#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

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

  if ( compdiff == NULL )
    return;

  while ( lcfgdiffcomponent_size(compdiff) > 0 ) {
    LCFGDiffResource * resdiff;
    if ( lcfgdiffcomponent_remove_next( compdiff, NULL, &resdiff )
         == LCFG_CHANGE_REMOVED ) {
      lcfgdiffresource_destroy(resdiff);
    }
  }

  free(compdiff);
  compdiff = NULL;

}

bool lcfgdiffcomponent_valid_name(const char * name) {
  return lcfgcomponent_valid_name(name);
}

bool lcfgdiffcomponent_has_name(const LCFGDiffComponent * compdiff) {
  return ( compdiff->name != NULL && *( compdiff->name ) != '\0' );
}

char * lcfgdiffcomponent_get_name(const LCFGDiffComponent * compdiff) {
  return compdiff->name;
}

bool lcfgdiffcomponent_set_name( LCFGDiffComponent * compdiff,
                                 char * new_name ) {

  bool ok = false;
  if ( lcfgdiffcomponent_valid_name(new_name) ) {
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

LCFGChange lcfgdiffcomponent_insert_next(
                                         LCFGDiffComponent * compdiff,
                                         LCFGDiffResource  * current,
                                         LCFGDiffResource  * new ) {

  if ( new == NULL )
    return LCFG_CHANGE_ERROR;

  if ( current == NULL ) { /* HEAD */

    if ( lcfgdiffcomponent_is_empty(compdiff) )
      compdiff->tail = new;

    new->next      = compdiff->head;
    compdiff->head = new;

  } else {

    if ( current->next == NULL )
      compdiff->tail = new;

    new->next     = current->next;
    current->next = new;

  }

  lcfgdiffresource_inc_ref(new);

  compdiff->size++;

  return LCFG_CHANGE_ADDED;
}

LCFGChange lcfgdiffcomponent_remove_next( LCFGDiffComponent * compdiff,
                                          LCFGDiffResource  * current,
                                          LCFGDiffResource ** old ) {

  if ( lcfgdiffcomponent_is_empty(compdiff) )
    return LCFG_CHANGE_ERROR;

  if ( current == NULL ) { /* HEAD */

    *old           = compdiff->head;
    compdiff->head = compdiff->head->next;

    if ( lcfgdiffcomponent_size(compdiff) == 1 )
      compdiff->tail = NULL;

  } else {

    if ( current->next == NULL )
      return LCFG_CHANGE_ERROR;

    *old          = current->next;
    current->next = current->next->next;

    if ( current->next == NULL )
      compdiff->tail = current;

  }

  lcfgdiffresource_dec_ref(*old);

  compdiff->size--;

  return LCFG_CHANGE_REMOVED;
}

void lcfgdiffcomponent_sort( LCFGDiffComponent * compdiff ) {

  if ( lcfgdiffcomponent_is_empty(compdiff) )
    return;

  /* Oo. Oo. bubble sort .oO .oO */

  bool done = false;

  while (!done) {

    LCFGDiffResource * current  = lcfgdiffcomponent_head(compdiff);
    LCFGDiffResource * next     = lcfgdiffcomponent_next(current);

    done = true;

    while ( next != NULL ) {

      char * cur_name  = lcfgdiffresource_get_name(current);
      if ( cur_name == NULL )
        cur_name = "";

      char * next_name = lcfgdiffresource_get_name(next);
      if ( next_name == NULL )
        next_name = "";

      if ( strcmp( cur_name, next_name ) > 0 ) {

        /* Done this way to avoid the overhead of function calls and
           having to deal with the ref counting. */

        LCFGResource * cur_old = current->old;
        LCFGResource * cur_new = current->new;

        current->old = next->old;
        current->new = next->new;

        next->old    = cur_old;
        next->new    = cur_new;

        done = false;
      }

      current = next;
      next    = lcfgdiffcomponent_next(current);
    }
  }

}

LCFGDiffResource * lcfgdiffcomponent_find_resource(
					  const LCFGDiffComponent * compdiff,
					  const char * want_name ) {

  if ( lcfgdiffcomponent_is_empty(compdiff) )
    return NULL;

  LCFGDiffResource * result = NULL;

  LCFGDiffResource * cur_resdiff = lcfgdiffcomponent_head(compdiff);
  while ( cur_resdiff != NULL ) {
    const char * cur_name = lcfgdiffresource_get_name(cur_resdiff);

    if ( cur_name != NULL && strcmp( cur_name, want_name ) == 0 ) {
      result = cur_resdiff;
      break;
    }

    cur_resdiff = lcfgdiffcomponent_next(cur_resdiff);
  }

  return result;
}

bool lcfgdiffcomponent_has_resource( const LCFGDiffComponent * compdiff,
				     const char * res_name ) {

  return ( lcfgdiffcomponent_find_resource( compdiff, res_name ) != NULL );
}

LCFGStatus lcfgdiffcomponent_to_holdfile( const LCFGDiffComponent * compdiff,
                                          FILE * holdfile,
                                          md5_state_t * md5state ) {

  if ( lcfgdiffcomponent_is_empty(compdiff) )
    return LCFG_STATUS_OK;

  const char * prefix = lcfgdiffcomponent_has_name(compdiff) ?
                        lcfgdiffcomponent_get_name(compdiff) : NULL;

  LCFGDiffResource * cur_resdiff = lcfgdiffcomponent_head(compdiff);

  size_t buf_size = 512;
  char * buffer = calloc( buf_size, sizeof(char) );
  if ( buffer == NULL ) {
    perror( "Failed to allocate memory for LCFG resource diff buffer" );
    exit(EXIT_FAILURE);
  }

  bool ok = true;
  while ( cur_resdiff != NULL ) {

    ssize_t rc = lcfgdiffresource_to_hold( cur_resdiff, prefix,
                                           &buffer, &buf_size );

    if ( rc > 0 ) {
      if ( md5state != NULL ) {
        lcfgutils_md5_append( md5state, (const md5_byte_t *) buffer, rc );
      }

      if ( fputs( buffer, holdfile ) < 0 )
        ok = false;

    } else if ( rc < 0 ) {
      ok = false;
    }

    if ( !ok )
      break;

    cur_resdiff = lcfgdiffcomponent_next(cur_resdiff);
  }

  free(buffer);

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

LCFGStatus lcfgresource_diff( LCFGResource * old_res,
                              LCFGResource * new_res,
                              LCFGDiffResource ** result ) {

  bool ok = true;
  if ( old_res != NULL && new_res != NULL ) {

    char * old_name = lcfgresource_has_name(old_res) ?
                      lcfgresource_get_name(old_res) : "";
    char * new_name = lcfgresource_has_name(new_res) ?
                      lcfgresource_get_name(new_res) : "";

    if ( strcmp( old_name, new_name ) != 0 )
      ok = false;

  }

  LCFGDiffResource * resdiff = NULL;

  if (ok) {
    resdiff = lcfgdiffresource_new();
    lcfgdiffresource_set_old( resdiff, old_res );
    lcfgdiffresource_set_new( resdiff, new_res );
  }

  *result = resdiff;

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

  LCFGResourceNode * cur_node =
    comp1 != NULL ? lcfgcomponent_head(comp1) : NULL;

  while ( ok && cur_node != NULL ) {
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

    cur_node = lcfgcomponent_next(cur_node);
  }


  /* Look for resources which have been added */

  cur_node = comp2 != NULL ? lcfgcomponent_head(comp2) : NULL;

  while ( ok && cur_node != NULL ) {
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

    cur_node = lcfgcomponent_next(cur_node);
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

  LCFGComponentNode * cur_node =
    list1 != NULL ? lcfgcomplist_head(list1) : NULL;

  while ( cur_node != NULL ) {
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

    cur_node = lcfgcomplist_next(cur_node);
  }

  /* Look for components which have been added */

  cur_node = list2 != NULL ? lcfgcomplist_head(list2) : NULL;

  while ( cur_node != NULL ) {
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

    cur_node = lcfgcomplist_next(cur_node);
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

  LCFGResourceNode * cur_node = lcfgcomponent_head(comp1);
  while ( cur_node != NULL ) {
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

    cur_node = lcfgcomponent_next(cur_node);
  }

  if ( status == LCFG_CHANGE_NONE ) {

    /* Look for resources which have been added */

    cur_node = lcfgcomponent_head(comp2);
    while ( cur_node != NULL ) {
      const LCFGResource * res2 = lcfgcomponent_resource(cur_node);

      /* Only diff 'active' resources which have a name attribute */
      if ( !lcfgresource_is_active(res2) || !lcfgresource_has_name(res2) )
	continue;

      const char * res2_name = lcfgresource_get_name(res2);

      if ( !lcfgcomponent_has_resource( comp1, res2_name, false ) ) {
        status = LCFG_CHANGE_MODIFIED;
        break;
      }

      cur_node = lcfgcomponent_next(cur_node);
    }

  }

  return status;
}

/* eof */
