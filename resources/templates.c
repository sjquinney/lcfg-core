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

  template->name     = NULL;
  template->name_len = 0;
  template->tmpl     = NULL;
  template->tmpl_len = 0;

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

  while(cur_template != NULL) {
    next_template = cur_template->next;

    free(cur_template->name);
    cur_template->name = NULL;

    free(cur_template->tmpl);
    cur_template->tmpl = NULL;

    free(cur_template);
    cur_template = next_template;
  }

}

bool lcfgtemplate_is_valid ( const LCFGTemplate * template ) {
  return ( template       != NULL &&
           template->name != NULL &&
           template->tmpl != NULL );
}

char * lcfgtemplate_get_name( const LCFGTemplate * template ) {
  assert( template != NULL );
  return template->name;
}

bool lcfgtemplate_set_name( LCFGTemplate * template, char * new_name ) {
  assert( template != NULL );

  /* Must be a valid 'resource' name */

  bool ok = false;
  if ( lcfgresource_valid_name(new_name) ) {
    free(template->name);

    template->name     = new_name;
    template->name_len = strlen(new_name);

    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

bool lcfgresource_valid_template( const char * tmpl ) {

  /* Extends the permitted character set for a resource name by
     including the '$' placeholder */

  /* MUST NOT be a NULL.
     MUST have non-zero length.
     First character MUST be in [A-Za-z] set. */

  bool valid = ( !isempty(tmpl) && isalpha(*tmpl) );

  /* All other characters MUST be in [A-Za-z0-9_$] set */

  int pcount = 0;

  char * ptr;
  for ( ptr = ((char *)tmpl) + 1; *ptr != '\0'; ptr++ ) {
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

char * lcfgtemplate_get_tmpl( LCFGTemplate * template ) {
  assert( template != NULL );
  return template->tmpl;
}

bool lcfgtemplate_set_tmpl( LCFGTemplate * template, char * new_tmpl ) {

  /* As well as being a valid template it must begin with the same
     string as the template 'name' */

  bool ok = false;
  bool valid = ( lcfgresource_valid_template(new_tmpl) &&
                 strncmp( new_tmpl, template->name, template->name_len ) == 0 );
  if (valid) {
    free(template->tmpl);

    template->tmpl     = new_tmpl;
    template->tmpl_len = strlen(new_tmpl);

    /* Use an array to cache the locations of the placeholders. Avoids
       having to find them multiple times when actually building the
       resource names. */

    int i;
    for ( i=template->tmpl_len - 1; i >= 0; i-- ) {
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

    /* Splitting on the first instance of the '_$' string */
    const char * split_at = strstr( token, "_$" );
    if ( split_at == NULL ) {
      status = invalid_template( msg, "no placeholder in '%s'", token );
      break;
    }

    LCFGTemplate * new_template = lcfgtemplate_new();

    /* Stub resource name */
    char * newname = strndup(token, split_at - token);

    if ( !lcfgtemplate_set_name( new_template, newname ) ) {
      status = invalid_template( msg, "bad resource name '%s'", newname );
      free(newname);
    }

    if ( status != LCFG_STATUS_ERROR ) {

      /* Full template */
      char * newtmpl = strdup(token);

      if ( !lcfgtemplate_set_tmpl( new_template, newtmpl ) ) {
        status = invalid_template( msg, "bad value '%s'", newtmpl );
        free(newtmpl);

      }
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
         strncmp( field_name, cur_tmpl->name, field_len ) == 0 )
      result = (LCFGTemplate *) cur_tmpl;
  }

  return result;
}

/* eof */
