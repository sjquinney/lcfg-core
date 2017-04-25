/**
 * @file taglist.c
 * @brief Functions for working with lists of LCFG resource tags
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date: 2017-04-25 13:32:07 +0100 (Tue, 25 Apr 2017) $
 * $Revision: 32488 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "utils.h"
#include "tags.h"

LCFGTagNode * lcfgtagnode_new(LCFGTag * tag) {

  LCFGTagNode * tagnode = malloc( sizeof(LCFGTagNode) );
  if ( tagnode == NULL ) {
    perror( "Failed to allocate memory for LCFG tag node" );
    exit(EXIT_FAILURE);
  }

  tagnode->tag  = tag;
  tagnode->prev = NULL;
  tagnode->next = NULL;

  return tagnode;
}

void lcfgtagnode_destroy(LCFGTagNode * tagnode) {

  if ( tagnode == NULL ) return;

  tagnode->tag  = NULL;
  tagnode->prev = NULL;
  tagnode->next = NULL;

  free(tagnode);
  tagnode = NULL;

}

LCFGTagList * lcfgtaglist_new(void) {

  LCFGTagList * taglist = malloc( sizeof(LCFGTagList) );
  if ( taglist == NULL ) {
    perror( "Failed to allocate memory for LCFG tag list" );
    exit(EXIT_FAILURE);
  }

  taglist->head        = NULL;
  taglist->tail        = NULL;
  taglist->size        = 0;
  taglist->_refcount   = 1;

  return taglist;
}

void lcfgtaglist_destroy(LCFGTagList * taglist) {

  if ( taglist == NULL ) return;

  while ( lcfgtaglist_size(taglist) > 0 ) {
    LCFGTag * tag = NULL;
    if ( lcfgtaglist_remove_tag( taglist, lcfgtaglist_tail(taglist), &tag )
         == LCFG_CHANGE_REMOVED ) {
      lcfgtag_relinquish(tag);
    }
  }

  free(taglist);
  taglist = NULL;

}

void lcfgtaglist_acquire( LCFGTagList * taglist ) {
  assert( taglist != NULL );

  taglist->_refcount += 1;
}

void lcfgtaglist_relinquish( LCFGTagList * taglist ) {

  if ( taglist == NULL ) return;

  if ( taglist->_refcount > 0 )
    taglist->_refcount -= 1;

  if ( taglist->_refcount == 0 )
    lcfgtaglist_destroy(taglist);

}

LCFGChange lcfgtaglist_insert_next( LCFGTagList * taglist,
                                    LCFGTagNode * tagnode,
                                    LCFGTag * tag ) {
  assert( taglist != NULL );

  /* Forbid the addition of bad tags */
  if ( !lcfgtag_is_valid(tag) ) return LCFG_CHANGE_ERROR;

  LCFGTagNode * new_node = lcfgtagnode_new(tag);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgtag_acquire(tag);

  if ( tagnode == NULL ) { /* HEAD */

    LCFGTagNode * cur_head = lcfgtaglist_head(taglist);

    new_node->prev = NULL;
    new_node->next = cur_head;

    if ( cur_head != NULL )
      cur_head->prev = new_node;

    taglist->head = new_node;

    if ( lcfgtaglist_is_empty(taglist) )
      taglist->tail = new_node;

  } else {

    new_node->prev = tagnode;
    new_node->next = tagnode->next;

    if ( new_node->next == NULL )
      taglist->tail = new_node;
    else
      tagnode->next->prev = new_node;

    tagnode->next = new_node;
  }

  taglist->size++;

  return LCFG_CHANGE_ADDED;
}

LCFGChange lcfgtaglist_remove_tag( LCFGTagList * taglist,
                                   LCFGTagNode * tagnode,
                                   LCFGTag ** tag ) {
  assert( taglist != NULL );

  if ( lcfgtaglist_is_empty(taglist) ) return LCFG_CHANGE_NONE;

  LCFGTagNode * cur_head = lcfgtaglist_head(taglist);
  if ( tagnode == NULL )
    tagnode = cur_head;

  if ( tagnode == cur_head ) {

    taglist->head = tagnode->next;

    if ( taglist->head == NULL )
      taglist->tail = NULL;
    else
      taglist->head->prev = NULL;

  } else {

    tagnode->prev->next = tagnode->next;

    if ( tagnode->next == NULL )
      taglist->tail = tagnode->prev;
    else
      tagnode->next->prev = tagnode->prev;

  }

  taglist->size--;

  *tag = lcfgtaglist_tag(tagnode);

  lcfgtagnode_destroy(tagnode);

  return LCFG_CHANGE_REMOVED;
}

LCFGChange lcfgtaglist_append_string( LCFGTagList * taglist,
                                      const char * tagname,
                                      char ** msg ) {
  LCFGChange change;

  LCFGTag * tag = NULL;

  LCFGStatus rc = lcfgtag_from_string( tagname, &tag, msg );
  if ( rc == LCFG_STATUS_ERROR )
    change = LCFG_CHANGE_ERROR;
  else
    change = lcfgtaglist_append_tag( taglist, tag );

  lcfgtag_relinquish(tag);

  return change;
}

LCFGTagNode * lcfgtaglist_find_node( const LCFGTagList * taglist,
                                     const char * name ) {
  assert( name != NULL );

  if ( lcfgtaglist_is_empty(taglist) ) return NULL;

  LCFGTagNode * result = NULL;

  const LCFGTagNode * cur_node = NULL;
  for ( cur_node = lcfgtaglist_head(taglist);
        cur_node != NULL && result == NULL;
        cur_node = lcfgtaglist_next(cur_node) ) {

    const LCFGTag * tag = lcfgtaglist_tag(cur_node);
    if ( lcfgtag_matches( tag, name ) ) 
      result = (LCFGTagNode *) cur_node;
  }

  return result;
}

LCFGTag * lcfgtaglist_find_tag( const LCFGTagList * taglist,
                                const char * name ) {
  assert( name != NULL );

  LCFGTag * tag = NULL;

  const LCFGTagNode * tagnode = lcfgtaglist_find_node( taglist, name );
  if ( tagnode != NULL )
    tag = lcfgtaglist_tag(tagnode);

  return tag;
}

bool lcfgtaglist_contains( const LCFGTagList * taglist,
                           const char * name ) {
  assert( name != NULL );

  return ( lcfgtaglist_find_node( taglist, name ) != NULL );
}

LCFGTagList * lcfgtaglist_clone(const LCFGTagList * taglist) {
  assert( taglist != NULL );

  bool ok = true;

  LCFGTagList * new_taglist = lcfgtaglist_new();

  const LCFGTagNode * cur_node = lcfgtaglist_head(taglist);
  for ( cur_node = lcfgtaglist_head(taglist);
        cur_node != NULL && ok;
        cur_node = lcfgtaglist_next(cur_node) ) {

    LCFGTag * cur_tag = lcfgtaglist_tag(cur_node);

    if ( lcfgtag_is_valid(cur_tag) ) {
      LCFGChange change = lcfgtaglist_append_tag( new_taglist, cur_tag );
      if ( change == LCFG_CHANGE_ERROR )
        ok = false;
    }

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
  assert( input != NULL );

  *result = NULL;

  if ( input == NULL ) {
    lcfgutils_build_message( msg, "Invalid taglist" );
    return LCFG_STATUS_ERROR;
  }

  LCFGTagList * new_taglist = lcfgtaglist_new();

  char * remainder = strdup(input);
  if ( remainder == NULL ) {
    perror( "Failed to allocate memory for LCFG tag list" );
    exit(EXIT_FAILURE);
  }

  char * saveptr = NULL;

  bool ok = true;

  char * token = strtok_r( remainder, tag_seps, &saveptr );
  while (token) {

    LCFGChange change = lcfgtaglist_append_string( new_taglist, token, msg );
    if ( change == LCFG_CHANGE_ERROR ) {
        ok = false;
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

  
  size_t new_len = 0;

  const LCFGTagNode * head_node = lcfgtaglist_head(taglist);

  const LCFGTagNode * cur_node = NULL;
  const LCFGTag * cur_tag      = NULL;

  if ( lcfgtaglist_size(taglist) > 0 ) {

    cur_tag = lcfgtaglist_tag(head_node);
    new_len += lcfgtag_get_length(cur_tag);

    for ( cur_node = lcfgtaglist_next(head_node);
          cur_node != NULL;
          cur_node = lcfgtaglist_next(cur_node) ) {

      cur_tag = lcfgtaglist_tag(cur_node);
      new_len += ( lcfgtag_get_length(cur_tag) + 1 ); /* +1 for single space */
    }

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

    cur_tag = lcfgtaglist_tag(head_node);
    const char * name = lcfgtag_get_name(cur_tag);
    size_t name_len   = lcfgtag_get_length(cur_tag);

    to = stpncpy( to, name, name_len );

    for ( cur_node = lcfgtaglist_next(head_node);
          cur_node != NULL;
          cur_node = lcfgtaglist_next(cur_node) ) {

      cur_tag = lcfgtaglist_tag(cur_node);

      name     = lcfgtag_get_name(cur_tag);
      name_len = lcfgtag_get_length(cur_tag);

      *to = ' ';
      to++;

      to = stpncpy( to, name, name_len );
    }

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

  if ( lcfgtaglist_size(taglist) < 2 ) return;

  /* Oo. Oo. bubble sort .oO .oO */

  bool swapped=true;

  while (swapped) {
    swapped=false;

    LCFGTagNode * cur_node = NULL;
    for ( cur_node = lcfgtaglist_head(taglist);
          cur_node != NULL && cur_node->next != NULL;
          cur_node = lcfgtaglist_next(cur_node) ) {

      LCFGTag * cur_tag  = lcfgtaglist_tag(cur_node);
      LCFGTag * next_tag = lcfgtaglist_tag(cur_node->next);

      if ( lcfgtag_compare( cur_tag, next_tag ) > 0 ) {
        cur_node->tag       = next_tag;
        cur_node->next->tag = cur_tag;
        swapped = true;
      }

    }
  }

}

LCFGChange lcfgtaglist_mutate_add( LCFGTagList * taglist, const char * name,
                                   char ** msg ) {

  LCFGChange change;
  if ( lcfgtaglist_contains( taglist, name ) ) {
    change = LCFG_CHANGE_NONE;
  } else {
    change = lcfgtaglist_append_string( taglist, name, msg );
  }

  return change;
}

LCFGChange lcfgtaglist_mutate_extra( LCFGTagList * taglist, const char * name,
                                     char ** msg ) {

  return lcfgtaglist_append_string( taglist, name, msg );
}

/* eof */
