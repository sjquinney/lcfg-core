/**
 * @file templates.c
 * @brief Functions for working with LCFG resource templates
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date$
 * $Revision$
 */

#define _GNU_SOURCE /* for asprintf */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>

#include "templates.h"
#include "utils.h"

static LCFGStatus invalid_template( char ** msg, const char * base, ... ) {

  const char * fmt = "Invalid template (%s)";

  va_list ap;
  va_start( ap, base );

  char * reason = NULL;
  int rc = vasprintf( &reason, base, ap );
  if ( rc < 0 ) {
    perror("Failed to allocate memory for error string");
    exit(EXIT_FAILURE);
  }

  lcfgutils_build_message( msg, fmt, reason );
  free(reason);

  return LCFG_STATUS_ERROR;
}

LCFGTemplate * lcfgtemplate_new(void) {

  LCFGTemplate * template = malloc( sizeof(LCFGTemplate) );
  if ( template == NULL ) {
    perror("Failed to allocate memory for LCFG resource template");
    exit(EXIT_FAILURE);
  }

  template->tmpl     = NULL;
  template->tmpl_len = 0;
  template->name_len = 0;

  int i;
  for ( i=0; i<LCFG_TAGS_MAX_DEPTH; i++ )
    template->places[i] = -1;

  template->pcount = 0;

  template->next = NULL;

  return template;
}

void lcfgtemplate_destroy(LCFGTemplate * head_template) {

  if ( head_template == NULL ) return;

  LCFGTemplate * cur_template = head_template;
  LCFGTemplate * next_template;

  while (cur_template != NULL) {
    next_template = cur_template->next;

    free(cur_template->tmpl);
    cur_template->tmpl = NULL;

    free(cur_template);

    cur_template = next_template;
  }

}

bool lcfgtemplate_is_valid ( const LCFGTemplate * template ) {
  return ( template != NULL && template->tmpl != NULL );
}

bool lcfgresource_valid_template( const char * tmpl ) {

  /* Extends the permitted character set for a resource name by
     including the '$' placeholder */

  /* MUST NOT be a NULL.
     MUST have non-zero length.
     First character MUST be in [A-Za-z] set. */

  bool valid = ( !isempty(tmpl) && isalpha(*tmpl) );

  /* All other characters MUST be in [A-Za-z0-9_$] set */

  unsigned int pcount = 0;

  char * ptr;
  for ( ptr = ((char *)tmpl) + 1; valid && *ptr != '\0'; ptr++ ) {
    if ( *ptr == LCFG_TEMPLATE_PLACEHOLDER ) {
      if ( ++pcount > LCFG_TAGS_MAX_DEPTH ) valid = false;
    } else if ( !isword(*ptr) ) {
      valid = false;
    }
  }

  /* Must be at least one placeholder */

  if ( pcount < 1 ) valid = false;

  return valid;
}

char * lcfgtemplate_get_tmpl( const LCFGTemplate * template ) {
  assert( template != NULL );
  return template->tmpl;
}

bool lcfgtemplate_set_tmpl( LCFGTemplate * template, char * new_tmpl ) {

  size_t new_name_len = 0;

  bool valid = lcfgresource_valid_template(new_tmpl);
  if ( valid ) {

    /* Splitting on the first instance of the '_$' string */
    const char * split_at = strstr( new_tmpl, "_$" );

    if ( split_at != NULL )
      new_name_len = split_at - new_tmpl;
    else
      valid = false;

  }

  bool ok = false;
  if (valid) {
    free(template->tmpl);

    size_t new_tmpl_len = strlen(new_tmpl);

    template->tmpl     = new_tmpl;
    template->tmpl_len = new_tmpl_len;
    template->name_len = new_name_len;

    /* Use an array to cache the locations of the placeholders. Avoids
       having to find them multiple times when actually building the
       resource names. */

    int i;
    for ( i=new_tmpl_len - 1; i >= 0; i-- ) {
      if ( new_tmpl[i] == LCFG_TEMPLATE_PLACEHOLDER ) {
        template->pcount++;
        template->places[template->pcount - 1] = i;
      }
    }

    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

ssize_t lcfgtemplate_to_string( const LCFGTemplate * head_template,
                                const char * prefix,
                                LCFGOption options,
                                char ** result, size_t * size ) {

  /* Calculate the required space */

  size_t new_len = 0;

  /* Optional prefix (e.g. something like "list: " ) */

  size_t prefix_len = 0;
  if ( !isempty(prefix) ) {
    prefix_len = strlen(prefix);
    new_len += prefix_len;
  }

  if ( head_template != NULL ) {

    /* Head */

    new_len += head_template->tmpl_len;

    /* Remainder of templates are added with an extra single space
       separator. */

    const LCFGTemplate * cur_template = NULL;
    for ( cur_template = head_template->next;
          cur_template != NULL;
          cur_template = cur_template->next ) {

      new_len += ( 1 + cur_template->tmpl_len ); /* +1 for single space */
    }

  }

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

  if ( prefix_len > 0 )
    to = stpncpy( to, prefix, prefix_len );

  if ( head_template != NULL ) {

    /* Head */

    to = stpncpy( to, head_template->tmpl, head_template->tmpl_len );

    /* Remainder of list */
 
    const LCFGTemplate * cur_template = NULL;
    for ( cur_template = head_template->next;
          cur_template != NULL;
          cur_template = cur_template->next ) {

      /* +1 for single space */

      *to = ' ';
      to++;

      to = stpncpy( to, cur_template->tmpl, cur_template->tmpl_len );
    }

  }

  if ( options&LCFG_OPT_NEWLINE )
    to = stpncpy( to, "\n", 1 );

  *to = '\0';

  assert( ( *result + new_len ) == to );

  return new_len;
}

LCFGStatus lcfgtemplate_from_string( const char * input,
                                     LCFGTemplate ** result,
                                     char ** msg ) {

  if ( isempty(input) )
    return invalid_template( msg, "empty string" );

  LCFGStatus status = LCFG_STATUS_OK;
  LCFGTemplate * head_template = NULL;
  LCFGTemplate * prev_template;

  char * remainder = strdup(input);
  char * saveptr;
  static const char * whitespace = " \n\r\t";

  char * token = strtok_r( remainder, whitespace, &saveptr );
  while ( status == LCFG_STATUS_OK && token ) {

    LCFGTemplate * new_template = lcfgtemplate_new();

    char * newtmpl = strdup(token);
    if ( !lcfgtemplate_set_tmpl( new_template, newtmpl ) ) {
      status = invalid_template( msg, "bad value '%s'", newtmpl );
      free(newtmpl);
    }

    if ( status == LCFG_STATUS_ERROR ) {
      lcfgtemplate_destroy(new_template);
      new_template = NULL;
      break;
    }

    /* Stash the templates */

    if ( head_template == NULL )
      head_template = new_template;
    else
      prev_template->next = new_template;

    /* Prepare for next loop */

    prev_template = new_template;

    token = strtok_r( NULL, whitespace, &saveptr );
  }

  free(remainder);

  if ( status == LCFG_STATUS_ERROR ) {
    lcfgtemplate_destroy(head_template);
    head_template = NULL;
  }

  *result = head_template;

  return status;
}

LCFGTemplate * lcfgtemplate_find( const LCFGTemplate * head_template,
                                  const char * field_name ) {
  assert( field_name != NULL );

  size_t field_len = isempty(field_name) ? 0 : strlen(field_name);

  LCFGTemplate * result = NULL;

  const LCFGTemplate * cur_tmpl = NULL;
  for ( cur_tmpl = head_template;
        cur_tmpl != NULL && result == NULL;
        cur_tmpl = cur_tmpl->next ) {

    if ( lcfgtemplate_is_valid(cur_tmpl) &&
         field_len == cur_tmpl->name_len &&
         strncmp( field_name, cur_tmpl->tmpl, field_len ) == 0 )
      result = (LCFGTemplate *) cur_tmpl;
  }

  return result;
}

char * lcfgresource_build_name( const LCFGTemplate * templates,
                                const LCFGTagList  * taglist,
                                const char * field_name,
                                char ** msg ) {

  /* Replaces each occurrence of the '$' placeholder with the next tag
     in the list. Work **backwards** from the tail of the list to the head. */

  const LCFGTemplate * res_tmpl = lcfgtemplate_find( templates, field_name );
  if ( res_tmpl == NULL ) {
    lcfgutils_build_message( msg, "Failed to find template for field '%s'\n",
                             field_name );
    return NULL;
  }

  const char * template  = lcfgtemplate_get_tmpl(res_tmpl);

  unsigned int pcount = res_tmpl->pcount;
  if ( lcfgtaglist_size(taglist) < pcount ) {
    lcfgutils_build_message( msg, "Insufficient tags for template '%s'\n",
                             template );
    return NULL;
  }

  /* Find the required length of the resource name */

  size_t new_len = res_tmpl->tmpl_len;

  LCFGTagIterator * tagiter = lcfgtagiter_new(taglist);

  unsigned int i;
  for ( i=0; i<pcount; i++ ) {

    const LCFGTag * cur_tag = lcfgtagiter_prev(tagiter);

    /* -1 for $ placeholder character which is replaced with the tag */
    new_len += ( lcfgtag_get_length(cur_tag) - 1 );
  }

  /* Allocate the necessary memory */

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror( "Failed to allocate memory for LCFG resource name" );
    exit(EXIT_FAILURE);
  }

  /* Build the resource name from the template and tags */

  size_t end = res_tmpl->tmpl_len - 1;
  size_t after;
  ssize_t after_len;
  size_t offset = new_len;

  lcfgtagiter_reset(tagiter);

  for ( i=0; i<pcount; i++ ) {
    int place = res_tmpl->places[i];
    after = place + 1;

    /* Copy any static parts of the template between this tag and             
       the next (or the end of the string). */

    after_len = end - after;
    if ( after_len > 0 ) {
      offset -= after_len;

      memcpy( result + offset, template + after, after_len );
    }

    end = place;

    /* Copy the required tag name */

    const LCFGTag * cur_tag = lcfgtagiter_prev(tagiter);

    size_t taglen  = lcfgtag_get_length(cur_tag);
    const char * tagname = lcfgtag_get_name(cur_tag);

    offset -= taglen;

    memcpy( result + offset, tagname, taglen );

  }

  lcfgtagiter_destroy(tagiter);

  /* Copy the rest which is static */

  memcpy( result, template, offset );

  *( result + new_len ) = '\0';

  return result;
}

/* eof */
