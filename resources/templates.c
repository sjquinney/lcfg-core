#define _GNU_SOURCE /* for asprintf */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "templates.h"

LCFGTemplate * lcfgtemplate_new( char * name, char * tmpl, char ** msg ) {

  LCFGTemplate * template = malloc( sizeof(LCFGTemplate) );
  if ( template == NULL ) {
    perror("Failed to allocate memory for LCFG resource template");
    exit(EXIT_FAILURE);
  }

  /* Defaults */

  template->name     = name;
  template->name_len = name != NULL ? strlen(name) : 0;
  template->tmpl     = tmpl;
  template->tmpl_len = tmpl != NULL ? strlen(tmpl) : 0;

  int i;
  for ( i=0; i<LCFG_TAGS_MAX_DEPTH; i++ )
    template->places[i] = -1;

  template->pcount = 0;

  template->next = NULL;

  /* The template MUST be at least one character long */

  bool valid = ( tmpl != NULL && *tmpl != '\0' );

  /* Use array to cache the locations of the placeholders. Avoids
     having to find them multiple times when actually building the
     resource names. */

  /* Note that this works **backwards** from the tail of the list to
     the head. */

  if (valid) {

    for ( i=template->tmpl_len - 1; i >= 0; i-- ) {
      if ( tmpl[i] == LCFG_TEMPLATE_PLACEHOLDER ) {

        template->pcount++;
        if ( template->pcount > LCFG_TAGS_MAX_DEPTH ) {
          asprintf( msg, "Too many placeholders (%d).\n", template->pcount );
          valid = false;
          break;
        }

        template->places[template->pcount - 1] = i;
      }
    }

  }

  if ( !valid ) {
    free(template);
    template = NULL;
  }

  return template;
}

void lcfgtemplate_destroy(LCFGTemplate * head_template) {

  if ( head_template == NULL )
    return;

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

ssize_t lcfgtemplate_to_string( const LCFGTemplate * head_template,
                                const char * prefix,
                                unsigned int options,
                                char ** result, size_t * size ) {

  LCFGTemplate * cur_template;

  /* Calculate the required space */

  cur_template = (LCFGTemplate *) head_template;
  size_t new_len = cur_template->tmpl_len;

  size_t prefix_len = 0;
  if ( prefix != NULL ) {
    prefix_len = strlen(prefix);
    new_len += prefix_len;
  }

  /* All additional templates are added with an extra single space
     separator. */

  cur_template = cur_template->next;
  while ( cur_template       != NULL &&
          cur_template->tmpl != NULL &&
          cur_template->tmpl_len > 0 ) {

    new_len += ( 1 + cur_template->tmpl_len ); /* +1 for single space */

    cur_template = cur_template->next;
  }

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

  if ( prefix != NULL && prefix_len > 0 )
    to = stpncpy( to, prefix, prefix_len );

  cur_template = (LCFGTemplate *) head_template;

  if (  cur_template       != NULL &&
	cur_template->tmpl != NULL &&
	cur_template->tmpl_len > 0 ) {
    to = stpncpy( to, cur_template->tmpl, cur_template->tmpl_len );
  }

  cur_template = cur_template->next;
  while ( cur_template       != NULL &&
          cur_template->tmpl != NULL &&
          cur_template->tmpl_len > 0 ) {

    /* +1 for single space */

    *to = ' ';
    to++;

    to = stpncpy( to, cur_template->tmpl, cur_template->tmpl_len );

    cur_template = cur_template->next;
  }

  *to = '\0';

  assert( ( *result + new_len ) == to );

  return new_len;
}

LCFGStatus lcfgtemplate_from_string( const char * input,
                                     LCFGTemplate ** result,
                                     char ** msg ) {

  *result = NULL;
  *msg    = NULL;

  LCFGTemplate * head_template = NULL;
  LCFGTemplate * prev_template;

  char * remainder = strdup(input);
  char * saveptr;
  static const char * whitespace = " \n\r\t";

  bool ok = true;

  char * token = strtok_r( remainder, whitespace, &saveptr );
  while (token) {

    /* Splitting on the first instance of the '_$' string */
    char * split_at = strstr( token, "_$" );
    if ( split_at == NULL ) {
      ok = false;
      asprintf( msg, "No placeholder found in template '%s'", token );
      break;
    }

    char * newname = strndup(token, split_at - token);
    char * newtmpl = strdup(token);

    char * new_msg = NULL;
    LCFGTemplate * new_template =
      lcfgtemplate_new( newname, newtmpl, &new_msg );
    if ( new_template == NULL ) {
      ok = false;

      if ( new_msg != NULL ) {
        asprintf( msg, "Failed to parse template '%s': %s", newtmpl, new_msg );
        free(new_msg);
      } else {
        asprintf( msg, "Failed to parse template '%s'", newtmpl );
      }

      free(newname);
      free(newtmpl);

      break;
    }

    if ( head_template == NULL ) {
      head_template = new_template;
    } else {
      prev_template->next = new_template;
    }
    prev_template = new_template;

    token = strtok_r( NULL, whitespace, &saveptr );
  }

  free(remainder);

  if ( !ok ) {
    lcfgtemplate_destroy(head_template);
    head_template = NULL;
  }

  *result = head_template;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

LCFGTemplate * lcfgtemplate_find( const LCFGTemplate * head_template,
                                  const char * field_name ) {

  LCFGTemplate * template = NULL;
  size_t field_len = strlen(field_name);

  LCFGTemplate * cur_template = (LCFGTemplate *) head_template;
  while ( cur_template != NULL ) {

    if ( field_len == cur_template->name_len &&
         strncmp( field_name, cur_template->name, field_len ) == 0 ) {
      template = cur_template;
      break;
    }

    cur_template = cur_template->next;
  }

  return template;
}

/* eof */
