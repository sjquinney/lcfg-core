#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "differences.h"

LCFGDiffResource * lcfgdiffresource_new(void) {

  LCFGDiffResource * resdiff = malloc(sizeof(LCFGDiffResource) );
  if ( resdiff == NULL ) {
    perror( "Failed to allocate memory for LCFG resource diff" );
    exit(EXIT_FAILURE);
  }

  resdiff->old  = NULL;
  resdiff->new  = NULL;
  resdiff->next = NULL;

  return resdiff;
}

void lcfgdiffresource_destroy(LCFGDiffResource * resdiff) {

  if ( resdiff == NULL )
    return;

  if ( lcfgdiffresource_has_old(resdiff) )
    lcfgdiffresource_set_old( resdiff, NULL );

  if ( lcfgdiffresource_has_new(resdiff) )
    lcfgdiffresource_set_new( resdiff, NULL );

  free(resdiff);
  resdiff = NULL;

}

bool lcfgdiffresource_has_old( const LCFGDiffResource * resdiff ) {
  return ( resdiff->old != NULL );
}

LCFGResource * lcfgdiffresource_get_old( const LCFGDiffResource * resdiff ) {
  return resdiff->old;
}

bool lcfgdiffresource_set_old( LCFGDiffResource * resdiff,
                               LCFGResource * res ) {

  LCFGResource * current = resdiff->old;
  lcfgresource_relinquish(current);

  if ( res != NULL )
    lcfgresource_acquire(res);

  resdiff->old = res;

  return true;
}

bool lcfgdiffresource_has_new( const LCFGDiffResource * resdiff ) {
  return ( resdiff->new != NULL );
}

LCFGResource * lcfgdiffresource_get_new( const LCFGDiffResource * resdiff ) {
  return resdiff->new;
}

bool lcfgdiffresource_set_new( LCFGDiffResource * resdiff,
                               LCFGResource * res ) {

  LCFGResource * current = resdiff->new;
  lcfgresource_relinquish(current);

  if ( res != NULL )
    lcfgresource_acquire(res);

  resdiff->new = res;

  return true;
}

char * lcfgdiffresource_get_name( const LCFGDiffResource * resdiff ) {
  
  LCFGResource * res = NULL;
  
  /* Check if there is an old resource with a name */
  if ( lcfgdiffresource_has_old(resdiff) ) {
    res = lcfgdiffresource_get_old(resdiff);

    if ( !lcfgresource_has_name(res) )
      res = NULL;
  }

  if ( res == NULL )
    res = lcfgdiffresource_get_new(resdiff);

  char * name = NULL;
  if ( res != NULL )
    name = lcfgresource_get_name(res);

  return name;
}

LCFGChange lcfgdiffresource_get_type( const LCFGDiffResource * resdiff ) {

  LCFGChange difftype = LCFG_CHANGE_NONE;

  if ( !lcfgdiffresource_has_old(resdiff) ) {

    if ( lcfgdiffresource_has_new(resdiff) ) {

      difftype = LCFG_CHANGE_ADDED;

    }

  } else {

    if ( !lcfgdiffresource_has_new(resdiff) ) {

      difftype = LCFG_CHANGE_REMOVED;

    } else {

      const LCFGResource * old = lcfgdiffresource_get_old(resdiff);
      const LCFGResource * new = lcfgdiffresource_get_new(resdiff);

      if ( !lcfgresource_same_value( old, new ) )
        difftype = LCFG_CHANGE_MODIFIED;

    }

  }

  return difftype;
}

bool lcfgdiffresource_is_changed( const LCFGDiffResource * resdiff ) {
  LCFGChange difftype = lcfgdiffresource_get_type(resdiff);
  return ( difftype == LCFG_CHANGE_ADDED   ||
	   difftype == LCFG_CHANGE_REMOVED ||
	   difftype == LCFG_CHANGE_MODIFIED );
}

bool lcfgdiffresource_is_nochange( const LCFGDiffResource * resdiff ) {
  return ( lcfgdiffresource_get_type(resdiff) == LCFG_CHANGE_NONE );
}

bool lcfgdiffresource_is_modified( const LCFGDiffResource * resdiff ) {
  return ( lcfgdiffresource_get_type(resdiff) == LCFG_CHANGE_MODIFIED );
}

bool lcfgdiffresource_is_added( const LCFGDiffResource * resdiff ) {
  return ( lcfgdiffresource_get_type(resdiff) == LCFG_CHANGE_ADDED );
}

bool lcfgdiffresource_is_removed( const LCFGDiffResource * resdiff ) {
  return ( lcfgdiffresource_get_type(resdiff) == LCFG_CHANGE_REMOVED );
}

ssize_t lcfgdiffresource_to_string( const LCFGDiffResource * resdiff,
				    const char * prefix,
				    const char * comments,
				    bool pending,
				    char ** result, size_t * size ) {

  size_t new_len = 0;

  const char * type;
  size_t type_len = 0;

  LCFGChange difftype = lcfgdiffresource_get_type(resdiff);
  switch(difftype)
    {
    case LCFG_CHANGE_ADDED:
      type     = "added";
      type_len = 5;
      break;
    case LCFG_CHANGE_REMOVED:
      type     = "removed";
      type_len = 7;
      break;
    case LCFG_CHANGE_MODIFIED:
      type     = "modified";
      type_len = 8;
      break;
    default:
      type     = "nochange";
      type_len = 8;
      break;
  }

  new_len += type_len;

  /* base of message */

  const char * base = "resource";
  size_t base_len = strlen(base);
  new_len += ( base_len + 1 ); /* + 1 for ' ' (space) separator */

  /* If the changes are being held we mark them as "pending" */

  if (pending)
    new_len += 8; /* " pending" including ' ' (space) separator */

  /* The prefix is typically the component name */

  size_t prefix_len = prefix != NULL ? strlen(prefix) : 0;
  if ( prefix_len > 0 )
    new_len += ( prefix_len + 1 ); /* + 1 for '.' separator */

  /* There is a tiny risk of neither resource having a useful name */

  const char * name = lcfgdiffresource_get_name(resdiff);
  size_t name_len = name != NULL ? strlen(name) : 0;
  new_len += ( name_len + 2 );    /* +2 for ": " separator */

  /* Optional comments */

  size_t comments_len = comments != NULL ? strlen(comments) : 0;
  if ( comments_len > 0 )
    new_len += ( comments_len + 3 ); /* +3 for brackets and ' ' separator */

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    *size = new_len + 1;

    *result = realloc( *result, ( *size * sizeof(char) ) );
    if ( *result == NULL ) {
      perror("Failed to allocate memory for LCFG resource diff string");
      exit(EXIT_FAILURE);
    }

  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  to = stpncpy( to, type, type_len );

  *to = ' ';
  to++;

  to = stpncpy( to, base, base_len );

  if (pending) {
    *to = ' ';
    to++;

    to = stpncpy( to, "pending", 7 );
  }

  to = stpncpy( to, ": ", 2 );

  if ( prefix_len > 0 ) {
    to = stpncpy( to, prefix, prefix_len );

    *to = '.';
    to++;
  }

  if ( name_len > 0 )
    to = stpncpy( to, name, name_len );

  if ( comments_len > 0 ) {
    to = stpncpy( to, " (", 2 );

    to = stpncpy( to, comments, comments_len );

    *to = ')';
    to++;
  }

  *to = '\0';

  assert( ( *result + new_len ) == to );

  return new_len;

}

ssize_t lcfgdiffresource_to_hold( const LCFGDiffResource * resdiff,
                                  const char * prefix,
                                  char ** result, size_t * size ) {

  const char * name = lcfgdiffresource_get_name(resdiff);
  if ( name == NULL )
    return -1;

  char * old_value = NULL;
  if ( lcfgdiffresource_has_old(resdiff) ) {
    LCFGResource * old_res = lcfgdiffresource_get_old(resdiff);

    if ( lcfgresource_has_value(old_res) )
      old_value = lcfgresource_get_value(old_res);
  }

  if ( old_value == NULL )
    old_value = "";

  char * new_value = NULL;
  if ( lcfgdiffresource_has_new(resdiff) ) {
    LCFGResource * new_res = lcfgdiffresource_get_new(resdiff);

    if ( lcfgresource_has_value(new_res) )
      new_value = lcfgresource_get_value(new_res);
  }

  if ( new_value == NULL )
    new_value = "";

  /* Find the required buffer size */

  size_t new_len = 0;
  if ( strcmp( old_value, new_value ) != 0 ) {

    if ( prefix != NULL )
      new_len += ( strlen(prefix) + 1 ); /* +1 for '.' separator */

    new_len += strlen(name) + 2;         /* +2 for ':' and newline */

    new_len += strlen(old_value) + 4;    /* +4 for ' - ' and newline */

    new_len += strlen(new_value) + 4;    /* +4 for ' - ' and newline */
  }

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    *size = new_len + 1;

    *result = realloc( *result, ( *size * sizeof(char) ) );
    if ( *result == NULL ) {
      perror("Failed to allocate memory for LCFG resource string");
      exit(EXIT_FAILURE);
    }

  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Adds where the new resource has no value and deletions where the
     old resource has no value are not worth reporting so simply avoid
     that here. */

  if ( new_len == 0 )
    return new_len;

  /* Build the new string - start at offset from the value line which
     was put there using lcfgresource_to_spec */

  char * to = *result;

  if ( prefix != NULL ) {
    to = stpcpy( to, prefix );
    *to = '.';
    to++;
  }

  to = stpcpy( to, name );
  to = stpncpy( to, ":\n", 2 );

  to = stpncpy( to, " - ", 3 );
  to = stpcpy( to, old_value );
  to = stpncpy( to, "\n",  1 );

  to = stpncpy( to, " + ", 3 );
  to = stpcpy( to, new_value );
  to = stpncpy( to, "\n",  1 );

  *to = '\0';

  assert( ( *result + new_len ) == to );

  return new_len;
}

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
      lcfgcomponent_find_resource( comp2, res1_name ) : NULL;

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
	 !lcfgcomponent_has_resource( comp1, res2_name ) ) {

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
      lcfgcomponent_find_resource( comp2, res1_name );

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

      if ( !lcfgcomponent_has_resource( comp1, res2_name ) ) {
        status = LCFG_CHANGE_MODIFIED;
        break;
      }

      cur_node = lcfgcomponent_next(cur_node);
    }

  }

  return status;
}

/* eof */
