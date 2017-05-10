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
  resdiff->_refcount = 1;

  return resdiff;
}

void lcfgdiffresource_destroy(LCFGDiffResource * resdiff) {

  if ( resdiff == NULL ) return;

  lcfgresource_relinquish(resdiff->old);
  resdiff->old = NULL;

  lcfgresource_relinquish(resdiff->new);
  resdiff->new = NULL;

  free(resdiff);
  resdiff = NULL;

}

void lcfgdiffresource_acquire( LCFGDiffResource * resdiff ) {
  assert( resdiff != NULL );

  resdiff->_refcount += 1;
}

void lcfgdiffresource_relinquish( LCFGDiffResource * resdiff ) {

  if ( resdiff == NULL ) return;

  if ( resdiff->_refcount > 0 )
    resdiff->_refcount -= 1;

  if ( resdiff->_refcount == 0 )
    lcfgdiffresource_destroy(resdiff);

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
  if ( lcfgdiffresource_has_old(resdiff) )
    res = lcfgdiffresource_get_old(resdiff);

  if ( !lcfgresource_is_valid(res) )
    res = lcfgdiffresource_get_new(resdiff);

  char * name = NULL;
  if ( lcfgresource_is_valid(res) )
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
  if ( name == NULL ) return -1;

  const char * old_value = NULL;
  if ( lcfgdiffresource_has_old(resdiff) ) {
    LCFGResource * old_res = lcfgdiffresource_get_old(resdiff);

    if ( lcfgresource_has_value(old_res) )
      old_value = lcfgresource_get_value(old_res);
  }

  if ( old_value == NULL )
    old_value = "";

  const char * new_value = NULL;
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

  if ( new_len == 0 ) return new_len;

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

/* eof */
