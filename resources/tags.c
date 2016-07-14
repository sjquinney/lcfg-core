#define _GNU_SOURCE /* for asprintf */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "tags.h"

LCFGTag * lcfgtag_new(char * name) {

  LCFGTag * tag = malloc( sizeof(LCFGTag) );
  if ( tag == NULL ) {
    perror("Failed to allocate memory for LCFG resource tag");
    exit(EXIT_FAILURE);
  }

  /* Default values */

  tag->name     = name;
  tag->name_len = name != NULL ? strlen(name) : 0;
  tag->prev     = NULL;
  tag->next     = NULL;

  return tag;
}

bool lcfgtag_valid_name( const char * value ) {

  /* MUST NOT be a NULL.
     MUST have non-zero length.
     MUST NOT contain whitespace characters */

  bool valid = ( value != NULL );

  char * ptr;
  for ( ptr = (char *) value; valid && *ptr != '\0'; ptr++ ) {
    if ( isspace(*ptr) ) valid = false;
  }

  return valid;
}

LCFGTagList * lcfgtaglist_new(void) {

  LCFGTagList * taglist = malloc( sizeof(LCFGTagList) );
  if ( taglist == NULL ) {
    perror( "Failed to allocate memory for LCFG resource tag list" );
    exit(EXIT_FAILURE);
  }

  /* Default values */

  taglist->size   = 0;
  taglist->head   = NULL;
  taglist->tail   = NULL;
  taglist->manage = true; /* If true, destroy() will free tag strings */

  return taglist;
}

void lcfgtaglist_destroy(LCFGTagList * taglist) {

  if ( taglist == NULL )
    return;

  while ( lcfgtaglist_size(taglist) > 0 ) {
    char * name = NULL;
    if ( lcfgtaglist_remove( taglist, lcfgtaglist_tail(taglist),
                             &name ) == LCFG_CHANGE_REMOVED ) {

      if ( taglist->manage )
        free(name);

    }
  }

  free(taglist);
  taglist = NULL;

}

LCFGChange lcfgtaglist_insert_next( LCFGTagList * taglist,
                                    LCFGTag * tag,
                                    char * name ) {

  /* Forbid the addition of bad tag names */
  if ( !lcfgtag_valid_name(name) )
    return LCFG_CHANGE_ERROR;

  if ( tag == NULL && lcfgtaglist_size(taglist) != 0 )
    return LCFG_CHANGE_ERROR;

  LCFGTag * new_tag = lcfgtag_new(name);
  if ( lcfgtaglist_is_empty(taglist) ) {

    taglist->head = new_tag;
    taglist->head->prev = NULL;
    taglist->head->next = NULL;
    taglist->tail = new_tag;

  } else {

    new_tag->next = tag->next;
    new_tag->prev = tag;

    if ( tag->next == NULL ) {
      taglist->tail = new_tag;
    } else {
      tag->next->prev = new_tag;
    }

    tag->next = new_tag;
  }

  taglist->size++;

  return LCFG_CHANGE_ADDED;
}


LCFGChange lcfgtaglist_remove( LCFGTagList * taglist,
                               LCFGTag * tag,
                               char ** name ) {

  if ( lcfgtaglist_is_empty(taglist) )
    return LCFG_CHANGE_NONE;

  *name = tag->name;

  if ( tag == taglist->head ) {
    taglist->head = tag->next;

    if ( taglist->head == NULL ) {
      taglist->tail = NULL;
    } else {
      tag->next->prev = NULL;
    }

    taglist->size = 0;

  } else {

    tag->prev->next = tag->next;

    if ( tag->next == NULL ) {
      taglist->tail = tag->prev;
    } else {
      tag->next->prev = tag->prev;
    }

    taglist->size--;

  }

  free(tag);

  return LCFG_CHANGE_REMOVED;
}

LCFGTag * lcfgtaglist_find_tag( const LCFGTagList * taglist,
                                const char * name ) {

  if ( lcfgtaglist_is_empty(taglist) )
    return NULL;

  size_t required_len = strlen(name);

  LCFGTag * match = NULL;

  /* The search is made more efficient by first comparing the stashed
     lengths and only doing the string comparison when necessary. */

  LCFGTag * cur_tag = lcfgtaglist_head(taglist);
  while ( cur_tag != NULL ) {
    size_t tag_len = lcfgtaglist_name_len(cur_tag);

    if ( required_len == tag_len &&
         strncmp( name, lcfgtaglist_name(cur_tag), tag_len ) == 0 ) {
      match = cur_tag;
      break;
    }

    cur_tag = lcfgtaglist_next(cur_tag);
  }

  return match;
}

bool lcfgtaglist_contains( const LCFGTagList * taglist,
                           const char * name ) {
  return ( lcfgtaglist_find_tag( taglist, name ) != NULL );
}

LCFGTagList * lcfgtaglist_clone(const LCFGTagList * taglist) {

  bool ok = true;

  LCFGTagList * new_taglist = lcfgtaglist_new();

  LCFGTag * cur_tag = lcfgtaglist_head(taglist);
  while ( cur_tag != NULL ) {
    size_t taglen  = lcfgtaglist_name_len(cur_tag);
    const char * cur_tagname = lcfgtaglist_name(cur_tag);

    /* Ignore any empty tags */
    if ( cur_tagname == NULL || taglen == 0 ) continue;

    char * tagname = strndup( cur_tagname, taglen );

    if ( lcfgtaglist_append( new_taglist, tagname )
         != LCFG_CHANGE_ADDED ) {
      ok = false;

      free(tagname);

      break;
    }

    cur_tag = lcfgtaglist_next(cur_tag);
  }

  if ( !ok ) {
    lcfgtaglist_destroy(new_taglist);
    new_taglist = NULL;
  }

  return new_taglist;
}

static const char * tag_seps = " \t\r\n";

LCFGStatus lcfgtaglist_from_string( const char * input,
                                    LCFGTagList ** result,
                                    char ** msg ) {

  *result = NULL;
  *msg = NULL;

  if ( input == NULL ) {
    asprintf( msg, "Invalid taglist" );
    return LCFG_STATUS_ERROR;
  }

  LCFGTagList * new_taglist = lcfgtaglist_new();

  char * remainder = strdup(input);
  if ( remainder == NULL ) {
    perror( "Failed to allocate memory for LCFG tag list" );
    exit(EXIT_FAILURE);
  }

  char * saveptr;

  bool ok = true;

  char * token = strtok_r( remainder, tag_seps, &saveptr );
  while (token) {
    char * tagname = strdup(token);

    if ( !lcfgtag_valid_name(tagname) ) {
      asprintf( msg, "Invalid tag '%s'", tagname );
      ok = false;
    } else {

      if ( lcfgtaglist_append( new_taglist, tagname )
           != LCFG_CHANGE_ADDED ) {
        asprintf( msg, "Failed to store tag '%s'", tagname );
        ok = false;
      }

    }

    if ( !ok ) {
      free(tagname);
      break;
    }

    token = strtok_r( NULL, tag_seps, &saveptr );
  }

  free(remainder);

  if ( !ok ) {
    lcfgtaglist_destroy(new_taglist);
    new_taglist = NULL;
  }

  *result = new_taglist;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

ssize_t lcfgtaglist_to_string( const LCFGTagList * taglist,
                               unsigned int options,
                               char ** result, size_t * size ) {

  /* Calculate the required space */

  LCFGTag * cur_tag = NULL;
  size_t new_len = 0;

  if ( lcfgtaglist_size(taglist) > 0 ) {
    cur_tag = lcfgtaglist_head(taglist);
    new_len = lcfgtaglist_name_len(cur_tag);

    cur_tag = lcfgtaglist_next(cur_tag);
  }

  while( cur_tag != NULL ) {
    new_len += ( lcfgtaglist_name_len(cur_tag) + 1 ); /* +1 for single space */

    cur_tag = lcfgtaglist_next(cur_tag);
  }

  /* Optional newline at end of string */
  if ( options&LCFG_OPT_NEWLINE )
    new_len += 1;

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    *size = new_len + 1;

    *result = realloc( *result, ( *size * sizeof(char) ) );
    if ( *result == NULL ) {
      perror("Failed to allocate memory for LCFG tag string");
      exit(EXIT_FAILURE);
    }

  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  if ( lcfgtaglist_size(taglist) > 0 ) {
    cur_tag = lcfgtaglist_head(taglist);

    const char * name = lcfgtaglist_name(cur_tag);
    if ( name != NULL )
      to = stpcpy( to, name );

    cur_tag = lcfgtaglist_next(cur_tag);
  } else {
    to = stpcpy( to, "" );

    cur_tag = NULL;
  }

  while ( cur_tag != NULL ) {

    /* +1 for single space */

    *to = ' ';
    to++;

    const char * name = lcfgtaglist_name(cur_tag);
    if ( name != NULL )
      to = stpcpy( to, name );

    cur_tag = lcfgtaglist_next(cur_tag);
  }

  /* Optional newline at end of string */
  if ( options&LCFG_OPT_NEWLINE )
    to = stpcpy( to, "\n" );

  *to = '\0';

  assert( (*result + new_len) == to );

  return new_len;
}

bool lcfgtaglist_print( const LCFGTagList * taglist, FILE * out ) {

  size_t buf_size = 0;
  char * str_buf  = NULL;

  bool ok = true;

  if ( lcfgtaglist_to_string( taglist, LCFG_OPT_NEWLINE,
                              &str_buf, &buf_size ) < 0 ) {
    ok = false;
  }

  if ( ok ) {

    if ( fputs( str_buf, out ) < 0 )
      ok = false;

  }

  free(str_buf);

  return ok;
}

void lcfgtaglist_sort( LCFGTagList * taglist ) {

  if ( lcfgtaglist_is_empty(taglist) )
    return;

  /* Oo. Oo. bubble sort .oO .oO */

  bool done = false;

  while (!done) {

    LCFGTag * cur_tag  = lcfgtaglist_head(taglist);
    LCFGTag * next_tag = lcfgtaglist_next(cur_tag);

    done = true;

    while ( next_tag != NULL ) {

      const char * cur_name  = lcfgtaglist_name(cur_tag);
      const char * next_name = lcfgtaglist_name(next_tag);

      if ( strcmp( cur_name, next_name ) > 0 ) {
        size_t cur_len  = cur_tag->name_len;
        size_t next_len = next_tag->name_len;

        /* Must swap the lengths as well as the pointers to the name strings */
        cur_tag->name      = (char *) next_name;
        cur_tag->name_len  = next_len;

        next_tag->name     = (char *) cur_name;
        next_tag->name_len = cur_len;

        done = false;
      }

      cur_tag  = next_tag;
      next_tag = lcfgtaglist_next(next_tag);
    }
  }

}

/* eof */
