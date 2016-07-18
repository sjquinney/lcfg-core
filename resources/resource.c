#define _GNU_SOURCE /* for asprintf */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#include "context.h"
#include "resources.h"
#include "utils.h"

/* The list of resource type names MUST match the ordering of the
   LCFGResourceType enum. */

static const char * lcfgresource_type_names[] = {
  "string",
  "integer",
  "boolean",
  "list",
  "publish",
  "subscribe"
};

/**
 * @brief Create and initialise new resource
 *
 * Creates a new @c LCFGResource struct and initialises the parameters
 * to the default values.
 *
 * If the memory allocation for the new struct is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * @return Pointer to new @c LCFGResource struct
 *
 */

LCFGResource * lcfgresource_new(void) {

  LCFGResource * res = malloc( sizeof(LCFGResource) );
  if ( res == NULL ) {
    perror( "Failed to allocate memory for LCFG resource" );
    exit(EXIT_FAILURE);
  }

  /* Set default values */

  res->name       = NULL;
  res->value      = NULL;
  res->type       = LCFG_RESOURCE_TYPE_STRING;
  res->template   = NULL;
  res->context    = NULL;
  res->derivation = NULL;
  res->comment    = NULL;
  res->priority   = 0;
  res->_refcount  = 0;

  return res;
}

/**
 * @brief Clone a resource
 *
 * Creates a new @c LCFGResource struct and copies the values of the
 * parameters from the specified resource. The values for the
 * parameters are copied (e.g. strings are duplicated using
 * @c strdup() ) so that a later change to a parameter in the source
 * resource does not affect the new clone resource.
 *
 * If the memory allocation for the new struct is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * @param res Pointer to @c LCFGResource struct to be cloned.
 *
 * @return Pointer to new @c LCFGResource struct or NULL if copy fails.
 *
 */

LCFGResource * lcfgresource_clone(const LCFGResource * res) {

  LCFGResource * clone = lcfgresource_new();
  if ( clone == NULL )
    return NULL;

  /* To simplify memory management the string attributes are
     duplicated. */

  bool ok = true;

  if ( res->name != NULL ) {
    char * new_name = strdup(res->name);
    ok = lcfgresource_set_name( clone, new_name );
    if ( !ok )
      free(new_name);
  }

  clone->type = res->type;

  if ( ok && res->value != NULL ) {
    char * new_value = strdup(res->value);
    ok = lcfgresource_set_value( clone, new_value );
    if ( !ok )
      free(new_value);
  }

  /* The template is a pointer to an LCFGTemplate struct so this needs
     to be cloned separately. The simplest approach is to serialise it
     into a string and then recreate a new struct using that as the
     source string. */

  if ( ok && res->template != NULL ) {
    char * tmpl_as_str = lcfgresource_get_template_as_string(res);
    char * tmpl_msg = NULL;
    ok = lcfgresource_set_template_as_string( clone, tmpl_as_str, &tmpl_msg );

    free(tmpl_as_str);

    /* Should be impossible to get an error since the template will
       have been previously validated so just throw away any error message */

    free(tmpl_msg);
  }

  clone->priority = res->priority;

  if ( ok && res->comment != NULL ) {
    char * new_comment = strdup(res->comment);
    ok = lcfgresource_set_comment( clone, new_comment );
    if ( !ok )
      free(new_comment);
  }

  if ( ok && res->context != NULL ) {
    char * new_context = strdup(res->context);
    ok = lcfgresource_set_context( clone, new_context );
    if ( !ok )
      free(new_context);
  }

  if ( ok && res->derivation != NULL ) {
    char * new_deriv = strdup(res->derivation);
    ok = lcfgresource_set_derivation( clone, new_deriv );
    if ( !ok )
      free(new_deriv);
  }

  /* Cloning should never fail */
  if ( !ok ) {
    lcfgresource_destroy(clone);
    clone = NULL;
  }

  return clone;
}

/**
 * @brief Destroy a resource
 *
 * When the specified @c LCFGResource struct is no longer required
 * this will free all associated memory.
 *
 * *Reference Counting:* There is support for very simple reference
 * counting which allows an @c LCFGResource struct to appear in
 * multiple lists. Incrementing and decrementing that reference
 * counter is the responsibility of the container code. If the
 * reference count for the specified resource is greater than zero the
 * function will not do anything.
 *
 * This will call @c free() on each parameter of the struct (or @c
 * lcfgtemplate_destroy for the template parameter ) and then set each
 * value to be @c NULL.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * resource which has already been destroyed (or potentially was never
 * created).
 *
 * @param res Pointer to @c LCFGResource struct to be destroyed.
 *
 */

void lcfgresource_destroy(LCFGResource * res) {

  if ( res == NULL )
    return;

  if ( res->_refcount > 0 )
    return;

  free(res->name);
  res->name = NULL;

  free(res->value);
  res->value = NULL;

  lcfgtemplate_destroy(res->template);
  res->template = NULL;

  free(res->context);
  res->context = NULL;

  free(res->derivation);
  res->derivation = NULL;

  free(res->comment);
  res->comment = NULL;

  free(res);
  res = NULL;

}

/* Names */

inline static bool isword( const char chr ) {
  return ( isalnum(chr) || chr == '_' );
}

/**
 * @brief Check if a string is a valid LCFG resource name
 *
 * Checks the contents of a specified string against the specification
 * for an LCFG resource name.
 *
 * An LCFG resource name MUST be at least one character in length. The
 * first character MUST be in the class @c [A-Za-z] and all other
 * characters MUST be in the class @c [A-Za-z0-9_]. This means they
 * are safe to use as variable names for languages such as bash.
 *
 * @param name String to be tested
 *
 * @return boolean which indicates if string is a valid resource name
 *
 */

bool lcfgresource_valid_name( const char * name ) {

  /* MUST NOT be a NULL.
     MUST have non-zero length.
     First character MUST be in [A-Za-z] set. */

  bool valid = ( name != NULL && *name != '\0' && isalpha(*name) );

  /* All other characters MUST be in [A-Za-z0-9_] set */

  char * ptr;
  for ( ptr = ((char *)name) + 1; valid && *ptr != '\0'; ptr++ ) {
    if ( !isword(*ptr) )
      valid = false;
  }

  return valid;
}

/**
 * @brief Check if a resource has a name
 *
 * Checks if the specified @c LCFGResource struct currently has a
 * value set for the name attribute. Although a name is required for
 * an LCFG resource to be valid it is possible for the value of the
 * name to be set to @c NULL when the struct is first created.
 *
 * @param res Pointer to an @c LCFGResource struct
 *
 * @return boolean which indicates if a resource has a name
 *
 */

bool lcfgresource_has_name( const LCFGResource * res ) {
  return ( res->name != NULL );
}

/**
 * @brief Get the name of the resource
 *
 * This returns the value of the name parameter for the @c
 * LCFGResource struct. If the resource does not currently have a name
 * then the value will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the name of the resource.
 *
 * @param res Pointer to an @c LCFGResource struct
 *
 * @return The name of the resource (possibly NULL).
 */

char * lcfgresource_get_name( const LCFGResource * res ) {
  return res->name;
}

/**
 * @brief Set the name of the resource
 *
 * Sets the value of the name parameter for the @c LCFGResource struct
 * to that specified. It is important to note that this does NOT take
 * a copy of the string.
 *
 * Before changing the value of the name to be the new string it will
 * be validated using the @c lcfgresource_valid_name() function. If
 * the new string is not valid then no change will occur, the @c errno
 * will be set to @c EINVAL and the function will return a @c false
 * value.
 *
 * @param res Pointer to an @c LCFGResource struct
 * @param new_name String which is the new name
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_set_name( LCFGResource * res, char * new_name ) {

  bool ok = false;
  if ( lcfgresource_valid_name(new_name) ) {
    free(res->name);

    res->name = new_name;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/* Types */

LCFGResourceType lcfgresource_get_type( const LCFGResource * res ) {
  return res->type;
}

bool lcfgresource_set_type( LCFGResource * res, LCFGResourceType new_type ) {

  if ( res->type == new_type ) /* no-op */
    return true;

  /* If the type is to be changed then there either must not be an
     existing value or that value must be compatible with the new
     type. For example, if the current value is "fish" and the
     requested new type is "integer" then that change is illegal. */

  bool ok = false;
  if ( !lcfgresource_has_value(res) ||
       lcfgresource_valid_value_for_type( new_type,
                                          lcfgresource_get_value(res) ) ) {

    res->type = new_type;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

bool lcfgresource_set_type_as_string( LCFGResource * res,
                                      const char * new_value,
                                      char ** errmsg ) {

  *errmsg = NULL;

  bool ok = true;

  /* If the new type string is empty then the resource is considered
     to be the default string type */

  LCFGResourceType new_type = LCFG_RESOURCE_TYPE_STRING;

  char * type_str = (char *) new_value;

  /* Spin past any leading whitespace */
  if ( type_str != NULL )
    while ( *type_str != '\0' && isspace(*type_str) ) type_str++;

  if ( type_str != NULL && *type_str != '\0' ) {

    /* If the type string begins with the type symbol character '%'
       (percent) then step past it and start the subsequent comparisons
       from the next char */

    if ( *type_str == LCFG_RESOURCE_SYMBOL_TYPE )
      type_str++;

    /* It is intentional that only the start of the type string is
       compared. The type string could potentially contain more than
       we need (e.g. "list: foo_$ bar_$") */

    if (        strncmp( type_str, "integer",   7 ) == 0 ) {
      new_type = LCFG_RESOURCE_TYPE_INTEGER;
    } else if ( strncmp( type_str, "boolean",   7 ) == 0 ) {
      new_type = LCFG_RESOURCE_TYPE_BOOLEAN;
    } else if ( strncmp( type_str, "list",      4 ) == 0 ) {
      new_type = LCFG_RESOURCE_TYPE_LIST;
    } else if ( strncmp( type_str, "publish",   7 ) == 0 ) {
      new_type = LCFG_RESOURCE_TYPE_PUBLISH;
    } else if ( strncmp( type_str, "subscribe", 9 ) == 0 ) {
      new_type = LCFG_RESOURCE_TYPE_SUBSCRIBE;
    } else if ( strncmp( type_str, "string",    6 ) != 0 ) {
      errno = EINVAL;
      ok = false;
    }
  }

  if (ok)
    ok = lcfgresource_set_type( res, new_type );

  /* Check if there is a comment string for the resource. This would
     be after the type string and enclosed in brackets - ( ) */

  char * posn = type_str;

  if ( ok && type_str != NULL ) {

    char * comment_start = strchr( type_str, '(' );
    if ( comment_start != NULL ) {
      char * comment_end = strchr( comment_start, ')' );

      if ( comment_end != NULL ) {
        posn = comment_end + 1;

        if ( comment_end - comment_start > 1 ) {
          char * comment = strndup( comment_start + 1,
                                    comment_end - 1 - comment_start );
          ok = lcfgresource_set_comment( res, comment );

          if ( !ok )
            free(comment);
        }

      }
    }
  }

  /* List types might also have templates */

  if ( ok && new_type == LCFG_RESOURCE_TYPE_LIST ) {
    char * tmpl_start = strstr( posn, ": " );
    if ( tmpl_start != NULL ) {
      tmpl_start += 2;

      /* Spin past any further whitespace */
      while( *tmpl_start != '\0' && isspace(*tmpl_start) ) tmpl_start++;

      /* A list type might be declared like "list: " (i.e. with no
         templates) in which case it is a simple list of tags. */

      if ( *tmpl_start != '\0' )
        ok = lcfgresource_set_template_as_string( res, tmpl_start, errmsg );
    }
  }

  return ok;
}

/* Since 'publish' and 'subscribe' resources hold any value which is
   to be mapped between profiles they are considered to be string-like
   for most operations. This does mean that sometimes extra care must
   be taken to handle them correctly. */

bool lcfgresource_is_string( const LCFGResource * res ) {
  LCFGResourceType res_type = lcfgresource_get_type(res);
  return ( res_type == LCFG_RESOURCE_TYPE_STRING    ||
           res_type == LCFG_RESOURCE_TYPE_SUBSCRIBE ||
           res_type == LCFG_RESOURCE_TYPE_PUBLISH );
}

bool lcfgresource_is_integer( const LCFGResource * res ) {
  return ( lcfgresource_get_type(res) == LCFG_RESOURCE_TYPE_INTEGER );
}

bool lcfgresource_is_boolean( const LCFGResource * res ) {
  return ( lcfgresource_get_type(res) == LCFG_RESOURCE_TYPE_BOOLEAN );
}

bool lcfgresource_is_list( const LCFGResource * res ) {
  return ( lcfgresource_get_type(res) == LCFG_RESOURCE_TYPE_LIST );
}

char * lcfgresource_get_type_as_string( const LCFGResource * res,
                                        unsigned int options ) {

  LCFGResourceType res_type = lcfgresource_get_type(res);
  const char * type_string = lcfgresource_type_names[res_type];
  size_t type_len = strlen(type_string);

  size_t new_len = type_len;

  char * comment = NULL;
  size_t comment_len = 0;
  if ( lcfgresource_has_comment(res) ) {
    comment = lcfgresource_get_comment(res);
    comment_len = strlen(comment);

    new_len += ( comment_len + 2 ); /* + 2 for enclosing ( ) */
  }

  /* A list resource might have a template */

  char * tmpl_as_str = NULL;
  size_t tmpl_len = 0;
  if ( lcfgresource_is_list(res) &&
       !(options&LCFG_OPT_NOTEMPLATES) ) {

    /* Always adds the ': ' separator even when there are no templates */

    new_len += 2;  /* +2 for ': ' separator */

    if ( lcfgresource_has_template(res) ) {
      tmpl_as_str = lcfgresource_get_template_as_string(res);

      if ( tmpl_as_str != NULL ) {
	tmpl_len = strlen(tmpl_as_str);
        new_len += tmpl_len;
      }
    }

  }

  /* Allocate the required space */

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate memory for LCFG resource type string");
    exit(EXIT_FAILURE);
  }

  /* Build the new string */

  char * to = result;

  to = stpncpy( to, type_string, type_len );

  if ( comment != NULL ) {
    *to = '(';
    to++;

    to = stpncpy( to, comment, comment_len );

    *to = ')';
    to++;
  }

  if ( lcfgresource_is_list(res) &&
       !(options&LCFG_OPT_NOTEMPLATES) ) {

    to = stpncpy( to, ": ", 2 );

    if ( tmpl_as_str != NULL )
      to = stpncpy( to, tmpl_as_str, tmpl_len );
  }

  free(tmpl_as_str);

  *to = '\0';

  assert( ( result + new_len ) == to );

  return result;
}

/* Templates */

bool lcfgresource_has_template( const LCFGResource * res ) {
  return ( res->template != NULL );
}

LCFGTemplate * lcfgresource_get_template( const LCFGResource * res ) {
  return res->template;
}

char * lcfgresource_get_template_as_string( const LCFGResource * res ) {

  char * as_str = NULL;

  if ( lcfgresource_has_template(res) ) {

    size_t buf_size = 0;
    ssize_t rc = lcfgtemplate_to_string( lcfgresource_get_template(res),
                                         NULL, 0,
                                         &as_str, &buf_size );

    if ( rc < 0 ) {
      free(as_str);
      as_str = NULL;
    }

  }

  return as_str;
}

bool lcfgresource_set_template( LCFGResource * res,
                                LCFGTemplate * new_value ) {

  free(res->template);

  res->template = new_value;

  return true;
}

bool lcfgresource_set_template_as_string(  LCFGResource * res,
                                           const char * new_value,
                                           char ** msg ) {

  *msg = NULL;

  if ( new_value == NULL || *new_value == '\0' ) {
    errno = EINVAL;
    return false;
  }

  LCFGTemplate * new_template = NULL;
  char * parse_msg = NULL;
  LCFGStatus rc = lcfgtemplate_from_string( new_value, &new_template,
                                            &parse_msg );

  bool ok = ( rc == LCFG_STATUS_OK && new_template != NULL );

  if (ok)
    ok = lcfgresource_set_template( res, new_template );


  if (!ok) {
    asprintf( msg, "Invalid template '%s': %s", new_value,
              ( parse_msg != NULL ? parse_msg : "unknown error" ) );

    lcfgtemplate_destroy(new_template);

    errno = EINVAL;
  }

  return ok;
}

/* Values */

bool lcfgresource_value_is_empty( const char * value ) {
  return ( value == NULL || *value == '\0' );
}

bool lcfgresource_has_value( const LCFGResource * res ) {
  return !lcfgresource_value_is_empty(res->value);
}

char * lcfgresource_get_value( const LCFGResource * res ) {
  return res->value;
}

char * lcfgresource_enc_value( const LCFGResource * res ) {

  const char * value = lcfgresource_get_value(res);

  if ( value == NULL )
    return NULL;

  size_t value_len = strlen(value);

  /* The following characters need to be encoded to ensure they do not
     corrupt the format of status files. */

  char * cr  = "&#xD;";   /* +4 \r */
  char * lf  = "&#xA;";   /* +4 \n */
  char * amp = "&#x26;";  /* +5    */

  size_t extend = 0;
  char * ptr;
  for ( ptr=(char *) value; *ptr!='\0'; ptr++ ) {
    switch(*ptr)
      {
      case '\r':
      case '\n':
	extend += 4;
	break;
      case '&':
	extend += 5;
	break;
      }
  }

  if ( extend == 0 )
    return strdup(value);

  /* Some characters need encoding */

  size_t new_len = value_len + extend;

  char * enc_value = calloc( ( new_len + 1 ), sizeof(char) );
  if ( enc_value == NULL ) {
    perror("Failed to allocate memory for LCFG resource value");
    exit(EXIT_FAILURE);
  }

  char * to = enc_value;

  for ( ptr=(char *) value; *ptr!='\0'; ptr++ ) {
    switch(*ptr)
      {
      case '\r':
	to = stpncpy( to, cr, 5 );
      case '\n':
	to = stpncpy( to, lf, 5 );
	break;
      case '&':
	to = stpncpy( to, amp, 6 );
	break;
      default:
	*to = *ptr;
	to++;
      }
  }

  *to = '\0';

  assert( ( enc_value + new_len ) == to );

  return enc_value;
}

bool lcfgresource_valid_boolean( const char * value ) {
  /* MUST NOT be NULL. MUST be empty string or "yes" */
  return ( value != NULL && 
           ( *value == '\0' || strcmp( value, "yes" ) == 0 ) );
}

static char * valid_false_values[] = {
  "false", "no", "off", "0", "",
  NULL
};

static char * valid_true_values[] = {
  "true",  "yes", "on", "1",
  NULL
};

char * lcfgresource_canon_boolean( const char * value ) {

  char * result = NULL;

  /* Nothing to do but return a copy of the value in most cases */

  if ( lcfgresource_valid_boolean(value) ) {
    result = strdup(value);
  } else {

    /* Take a copy so that the string can be modified. Map a NULL onto
       an empty string (which will be considered as false). */

    char * value2 = value != NULL ? strdup(value) : strdup("");
    if ( value2 == NULL ) {
      perror( "Failed to allocate memory for LCFG resource value" );
      exit(EXIT_FAILURE);
    }

    /* Lower case the value for comparison */

    char * ptr;
    for ( ptr = value2; *ptr != '\0'; ptr++ ) *ptr = tolower(*ptr);

    char ** val_ptr;

    bool canon_value;
    bool valid = false;

    /* Compare with true values */
    for ( val_ptr = valid_true_values; *val_ptr != NULL; val_ptr++ ) {
      if ( strcmp( value2, *val_ptr ) == 0 ) {
        valid = true;
        canon_value = true;
        break;
      }
    }

    /* If not found compare with false values */
    if ( !valid ) {
      for ( val_ptr = valid_false_values; *val_ptr != NULL; val_ptr++ ) {
        if ( strcmp( value2, *val_ptr ) == 0 ) {
          valid = true;
          canon_value = false;
          break;
        }
      }
    }

    if (valid) {
      if (canon_value) {
        result = strdup("yes");
      } else {
        result = strdup("");
      }
    }

    free(value2);
  }

  return result;
}

bool lcfgresource_valid_integer( const char * value ) {

  /* MUST NOT be a NULL. */

  if ( value == NULL )
    return false;

  /* First character may be a negative-sign, if so walk past */

  char * ptr = (char *) value;
  if ( *ptr == '-' )
    ptr++;

  /* MUST be further characters */

  bool valid = ( *ptr != '\0' );

  /* All other characters MUST be digits */

  for ( ; valid && *ptr != '\0'; ptr++ ) {
    if ( !isdigit(*ptr) )
      valid = false;
  }

  return valid;
}

bool lcfgresource_valid_list( const char * value ) {

  /* MUST NOT be a NULL */

  bool valid = ( value != NULL );

  /* Empty list is fine.
     Tags MUST be characters in [A-Za-z0-9_] set.
     Separators MUST be spaces */

  char * ptr;
  for ( ptr = (char *) value; valid && *ptr != '\0'; ptr++ ) {
    if ( !isword(*ptr) && *ptr != ' ' )
      valid = false;
  }

  return valid;
}

bool lcfgresource_valid_value_for_type( LCFGResourceType type,
                                        const char * value ) {

  /* Check if the specified value is valid for the current type of the
     resource. */

  if ( value == NULL )
    return false;

  bool valid = true;
  switch(type)
    {
    case LCFG_RESOURCE_TYPE_INTEGER:
      valid = lcfgresource_valid_integer(value);
      break;
    case LCFG_RESOURCE_TYPE_BOOLEAN:
      valid = lcfgresource_valid_boolean(value);
      break;
    case LCFG_RESOURCE_TYPE_LIST:
      valid = lcfgresource_valid_list(value);
      break;
    }

  return valid;
}

bool lcfgresource_valid_value( const LCFGResource * res,
                               const char * value ) {

  return lcfgresource_valid_value_for_type( lcfgresource_get_type(res),
                                            value );
}

bool lcfgresource_set_value( LCFGResource * res, char * new_value ) {

  bool ok = false;
  if ( lcfgresource_valid_value( res, new_value ) ) {
    free(res->value);

    res->value = new_value;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/* It is not possible to set the value back to NULL by calling
   set_value since it is an illegal value so an explicit unset
   function is provided. */

bool lcfgresource_unset_value( LCFGResource * res ) {

  free(res->value);
  res->value = NULL;

  return true;
}

/* Derivations */

bool lcfgresource_has_derivation( const LCFGResource * res ) {
  return ( res->derivation != NULL && *( res->derivation ) != '\0' );
}

char * lcfgresource_get_derivation( const LCFGResource * res ) {
  return res->derivation;
}

bool lcfgresource_set_derivation( LCFGResource * res, char * new_value ) {

  /* Currently no validation of the derivation */

  free(res->derivation);

  res->derivation = new_value;

  return true;
}

bool lcfgresource_add_derivation( LCFGResource * resource,
                                  const char * extra_deriv ) {

  if ( extra_deriv == NULL || *extra_deriv == '\0' )
    return true;

  char * new_deriv = NULL;
  if ( !lcfgresource_has_derivation(resource) ) {
    new_deriv = strdup(extra_deriv);
  } else if ( strstr( resource->derivation, extra_deriv ) == NULL ) {

    new_deriv =
      lcfgutils_join_strings( resource->derivation, extra_deriv, " " );
    if ( new_deriv == NULL ) {
      perror( "Failed to build LCFG derivation string" );
      exit(EXIT_FAILURE);
    }
  }

  /* If the extra derivation does not need to be added (since it is
     already present) then new_deriv will be NULL which means ok needs
     to be true. */

  bool ok = true;
  if ( new_deriv != NULL ) {
    ok = lcfgresource_set_derivation( resource, new_deriv );

    if ( !ok )
      free(new_deriv);
  }

  return ok;
}

/* Context Expression */

bool lcfgresource_valid_context( const char * expr ) {
  return lcfgcontext_valid_expression(expr);
}

bool lcfgresource_has_context( const LCFGResource * res ) {
  return ( res->context != NULL && *( res->context ) != '\0' );
}

char * lcfgresource_get_context( const LCFGResource * res ) {
  return res->context;
}

bool lcfgresource_set_context( LCFGResource * res, char * new_value ) {

  bool ok = false;
  if ( lcfgresource_valid_context(new_value) ) {
    free(res->context);

    res->context = new_value;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

bool lcfgresource_add_context( LCFGResource * res,
                               const char * extra_context ) {

  if ( extra_context == NULL || *extra_context == '\0' )
    return true;

  char * new_context;
  if ( !lcfgresource_has_context(res) ) {
    new_context = strdup(extra_context);
  } else {
    new_context =
      lcfgcontext_combine_expressions( res->context, extra_context );
  }

  bool ok = lcfgresource_set_context( res, new_context );
  if ( !ok )
    free(new_context);

  return ok;
}

/* Comments */

bool lcfgresource_has_comment( const LCFGResource * res ) {
  return ( res->comment != NULL && *( res->comment ) != '\0' );
}

char * lcfgresource_get_comment( const LCFGResource * res ) {
  return res->comment;
}

bool lcfgresource_set_comment( LCFGResource * res, char * new_value ) {

  free(res->comment);

  res->comment = new_value;

  return true;
}

/* Priority */

int lcfgresource_get_priority( const LCFGResource * res ) {
  return res->priority;
}

char * lcfgresource_get_priority_as_string( const LCFGResource * res ) {

  char * as_str = NULL;
  if ( asprintf( &as_str, "%d", lcfgresource_get_priority(res) ) == -1 ) {
    as_str = NULL;
  }

  return as_str;
}

bool lcfgresource_set_priority( LCFGResource * res, int priority ) {
  res->priority = priority;
  return true;
}

bool lcfgresource_eval_priority( LCFGResource * res,
                                 const LCFGContextList * ctxlist,
                                 char ** errmsg ) {

  *errmsg = NULL;
  bool ok = true;

  int priority = 0;
  if ( lcfgresource_has_context(res) ) {

    /* Calculate the priority using the context expression for this
       resource. */

    ok = lcfgctxlist_eval_expression( ctxlist,
                                      lcfgresource_get_context(res),
                                      &priority, errmsg );

  }

  if (ok)
    ok = lcfgresource_set_priority( res, priority );

  return ok;
}

bool lcfgresource_is_active( const LCFGResource * res ) {
  return ( lcfgresource_get_priority(res) >= 0 );
}

/* Output */

bool lcfgresource_to_env( const LCFGResource * res,
                          const char * prefix,
                          unsigned int options ) {

  /* Name is required */

  if ( !lcfgresource_has_name(res) )
    return false;

  const char * name = lcfgresource_get_name(res);
  size_t name_len = strlen(name);

  size_t key_len = name_len;

  /* Optional prefix (usually LCFG_compname_) */

  size_t prefix_len = 0;
  if ( prefix != NULL ) {
    prefix_len = strlen(prefix);
    key_len += prefix_len;
  }

  char * key = calloc( ( key_len + 1 ), sizeof(char) );
  if ( key == NULL ) {
    perror("Failed to allocate memory for LCFG resource variable name");
    exit(EXIT_FAILURE);
  }

  /* Build the new string */

  char * to = key;

  if ( prefix != NULL )
    to = stpncpy( to, prefix, prefix_len );

  to = stpncpy( to, name, name_len );

  *to = '\0';

  assert( ( key + key_len ) == to );

  const char * value = lcfgresource_has_value(res) ?
                       lcfgresource_get_value(res) : "";

  int rc = setenv( key, value, 1 ); /* overwrite value if key already set */

  free(key);

  return ( rc == 0 );
}

ssize_t lcfgresource_to_status( const LCFGResource * res,
                                const char * prefix,
                                unsigned int options,
                                char ** result, size_t * size ) {

  /* The entry for the value is the standard stringified form. This
     writes directly into the result buffer, often this is all that
     the to_status function needs to do (if there is no type or
     derivation information). */

  ssize_t value_len = lcfgresource_to_string( res, prefix,
                                              options|LCFG_OPT_NEWLINE|LCFG_OPT_ENCODE,
                                              result, size );

  if ( value_len < 0 )
    return value_len;

  size_t new_len = value_len;

  /* Resource Meta Data */

  /* Type - Only output the type when the resource is NOT a string */

  char * type_as_str = NULL;
  size_t type_len = 0;
  if ( lcfgresource_get_type(res) != LCFG_RESOURCE_TYPE_STRING ||
       lcfgresource_has_comment(res) ) {

    type_as_str = lcfgresource_get_type_as_string( res, 0 );

    if ( type_as_str != NULL ) {
      type_len = strlen(type_as_str);

      ssize_t key_len =
        lcfgresource_compute_key_length( res, prefix, NULL,
                                         LCFG_RESOURCE_SYMBOL_TYPE );

      new_len += ( key_len + type_len + 2 ); /* +2 for '=' and newline */
    }
  }

  /* Derivation */

  char * derivation = NULL;
  size_t deriv_len = 0;
  if ( lcfgresource_has_derivation(res) ) {
    derivation = lcfgresource_get_derivation(res);
    deriv_len = strlen(derivation);

    if ( deriv_len > 0 ) {
      ssize_t key_len =
	lcfgresource_compute_key_length( res, prefix, NULL,
					 LCFG_RESOURCE_SYMBOL_DERIVATION );

      new_len += ( key_len + deriv_len + 2 ); /* +2 for '=' and newline */
    }
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

  /* Build the new string - start at offset from the value line which
     was put there using lcfgresource_to_string */

  char * to = *result + value_len;

  /* Type */

  if ( type_as_str != NULL ) {

    ssize_t write_len =
      lcfgresource_insert_key( res, prefix, NULL,
                               LCFG_RESOURCE_SYMBOL_TYPE, to );

    if ( write_len > 0 ) {
      to += write_len;

      *to = '=';
      to++;

      to = stpncpy( to, type_as_str, type_len );

      to = stpcpy( to, "\n" );
    }

    free(type_as_str);
  }

  /* Derivation */

  if ( lcfgresource_has_derivation(res) ) {

    ssize_t write_len =
      lcfgresource_insert_key( res, prefix, NULL,
                               LCFG_RESOURCE_SYMBOL_DERIVATION, to );

    if ( write_len > 0 ) {
      to += write_len;

      *to = '=';
      to++;

      to = stpncpy( to, derivation, deriv_len );

      to = stpcpy( to, "\n" );
    }
  }

  *to = '\0';

  assert( (*result + new_len ) == to );

  return new_len;
}

ssize_t lcfgresource_to_string( const LCFGResource * res,
                                const char * prefix,
                                unsigned int options,
                                char ** result, size_t * size ) {

  ssize_t key_len =
    lcfgresource_compute_key_length( res, prefix, NULL, 
                                     LCFG_RESOURCE_SYMBOL_VALUE );

  if ( key_len < 0 )
    return key_len;

  size_t new_len = key_len;

  /* Value */

  /* When the NOVALUE option is set then the string is assembled
     without a value and also without the '= separator. It will be
     just the resource name, possibly with a prefix, and possibly with
     a context.

     Normally if there is no value specified for the resource the '='
     separator is still added. */

  char * value = NULL;
  size_t value_len = 0;
  if ( !(options&LCFG_OPT_NOVALUE) ) {

    if ( options&LCFG_OPT_ENCODE ) {
      value = lcfgresource_enc_value(res);
    } else {
      value = lcfgresource_get_value(res);
    }

    value_len = value != NULL ? strlen(value) : 0;

    new_len += ( 1 + value_len ); /* +1 for '=' separator */
  }

  /* Context */

  char * context = NULL;
  size_t context_len = 0;

  if ( !(options&LCFG_OPT_NOCONTEXT) &&
       lcfgresource_has_context(res) ) {

    context = lcfgresource_get_context(res);
    context_len = strlen(context);

    if ( context_len > 0 )
      new_len += ( 2 + context_len ); /* +2 for '[' and ']' */

  }

  /* Optional newline at end of string */

  if ( options&LCFG_OPT_NEWLINE )
    new_len += 1;

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

  /* Build the new string */

  char * to = *result;

  ssize_t write_len = lcfgresource_insert_key( res, prefix, NULL,
                                               LCFG_RESOURCE_SYMBOL_VALUE, to );

  if ( write_len != key_len )
    return -1;

  to += write_len;

  /* Optional context */

  if ( context_len > 0 ) {
    *to = '[';
    to++;

    to = stpncpy( to, context, context_len );

    *to = ']';
    to++;
  }

  /* Optional value string */

  if ( !(options&LCFG_OPT_NOVALUE) ) {
    *to = '=';
    to++;

    if ( value_len > 0 )
      to = stpncpy( to, value, value_len );

    if ( options&LCFG_OPT_ENCODE )
      free(value);
  }

  /* Optional newline at the end of the string */

  if ( options&LCFG_OPT_NEWLINE )
    to = stpcpy( to, "\n" );

  assert( (*result + new_len ) == to );

  return new_len;
}

bool lcfgresource_print( const LCFGResource * res,
                         const char * prefix,
                         const char * style,
                         unsigned int options,
                         FILE * out ) {

  size_t buf_size = 0;
  char * lcfgres = NULL;
  ssize_t rc = 0;

  if ( style != NULL && strcmp( style, "status" ) == 0 ) {
    rc = lcfgresource_to_status( res, prefix, options,
                                 &lcfgres, &buf_size );
  } else {
    rc = lcfgresource_to_string( res, prefix, options|LCFG_OPT_NEWLINE,
                                 &lcfgres, &buf_size );
  }

  bool ok = ( rc > 0 );

  if ( ok ) {

    if ( fputs( lcfgres, out ) < 0 )
      ok = false;

  }

  free(lcfgres);

  return ok;
}

char * lcfgresource_build_name( const LCFGTemplate * templates,
                                const LCFGTagList  * taglist,
                                const char * field_name,
                                char ** msg ) {

  /* Replaces each occurrence of the '$' placeholder with the next tag
     in the list. Work **backwards** from the tail of the list to the head. */

  const LCFGTemplate * res_tmpl = lcfgtemplate_find( templates, field_name );
  if ( res_tmpl == NULL ) {
    asprintf( msg, "Failed to find template for field '%s'\n", field_name );
    return NULL;
  }

  const char * template  = res_tmpl->tmpl;

  unsigned int pcount = res_tmpl->pcount;
  if ( lcfgtaglist_size(taglist) < pcount ) {
    asprintf( msg, "Insufficient tags for template '%s'\n", template );
    return NULL;
  }

  /* Find the required length of the resource name */

  size_t new_len = res_tmpl->tmpl_len;

  LCFGTag * cur_tag = lcfgtaglist_tail(taglist);

  unsigned int i;
  for ( i=0; i<pcount; i++ ) {

    /* -1 for $ placeholder character which is replaced with the tag */
    new_len += ( lcfgtaglist_name_len(cur_tag) - 1 );

    cur_tag = lcfgtaglist_prev(cur_tag);
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

  cur_tag = lcfgtaglist_tail(taglist);

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

    size_t taglen  = lcfgtaglist_name_len(cur_tag);

    offset -= taglen;

    memcpy( result + offset, lcfgtaglist_name(cur_tag), taglen );

    cur_tag = lcfgtaglist_prev(cur_tag);
  }

  /* Copy the rest which is static */

  memcpy( result, template, offset );

  *( result + new_len ) = '\0';

  return result;
}

int lcfgresource_compare( const LCFGResource * res1,
                          const LCFGResource * res2 ) {

  const char * name1 = lcfgresource_has_name(res1) ? lcfgresource_get_name(res1) : LCFG_RESOURCE_NOVALUE;
  const char * name2 = lcfgresource_has_name(res2) ? lcfgresource_get_name(res2) : LCFG_RESOURCE_NOVALUE;

  return strcmp( name1, name2 );
}

bool lcfgresource_same_value( const LCFGResource * res1,
                              const LCFGResource * res2 ) {

  const char * value1 = lcfgresource_has_value(res1) ? lcfgresource_get_value(res1) : LCFG_RESOURCE_NOVALUE;
  const char * value2 = lcfgresource_has_value(res2) ? lcfgresource_get_value(res2) : LCFG_RESOURCE_NOVALUE;

  return ( strcmp( value1, value2 ) == 0 );
}

bool lcfgresource_same_type( const LCFGResource * res1,
                             const LCFGResource * res2 ) {

  return ( lcfgresource_get_type(res1) == lcfgresource_get_type(res2) );
}

bool lcfgresource_equals( const LCFGResource * res1,
                          const LCFGResource * res2 ) {

  /* Name */

  const char * name1 = lcfgresource_has_name(res1) ? lcfgresource_get_name(res1) : LCFG_RESOURCE_NOVALUE;
  const char * name2 = lcfgresource_has_name(res2) ? lcfgresource_get_name(res2) : LCFG_RESOURCE_NOVALUE;

  bool equals = ( strcmp( name1, name2 ) == 0 );

  /* Value */

  if ( equals )
    equals = lcfgresource_same_value( res1, res2 );

  /* Context */

  if ( equals ) {
    const char * context1 = lcfgresource_has_context(res1) ? lcfgresource_get_context(res1) : LCFG_RESOURCE_NOVALUE;
    const char * context2 = lcfgresource_has_context(res2) ? lcfgresource_get_context(res2) : LCFG_RESOURCE_NOVALUE;

    equals = ( strcmp( context1, context2 ) == 0 );
  }

  /* The template, type and derivation are NOT compared */

  return equals;
}

char * lcfgresource_build_message( const LCFGResource * res,
                                   const char * component,
                                   const char *fmt, ... ) {

  /* This is rather messy and probably somewhat inefficient. It is
     intended to be used primarily for generating error messages,
     usually just before failing entirely. */

  char * result = NULL;

  /* First assemble the base of the message using the format string
     and any varargs passed in by the caller */

  char * msg_base;
  va_list ap;

  va_start(ap, fmt);
  /* The BSD version apparently sets ptr to NULL on fail.  GNU loses. */
  if (vasprintf(&msg_base, fmt, ap) < 0)
    msg_base = NULL;
  va_end(ap);

  char * type_as_str = NULL;
  char * res_as_str  = NULL;

  if ( res != NULL ) {

    /* Not interested in the resource type if it is the default 'string' */
    if ( lcfgresource_get_type(res) != LCFG_RESOURCE_TYPE_STRING ) {
      type_as_str = 
        lcfgresource_get_type_as_string( res, LCFG_OPT_NOTEMPLATES );
    }

    if ( lcfgresource_has_name(res) ) {
      size_t buf_size = 0;
      ssize_t str_rc = lcfgresource_to_string( res, component,
                                               LCFG_OPT_NOVALUE,
                                               &res_as_str, &buf_size );
    }
  }

  int rc = 0;

  /* Build the most useful summary possible using all the available
     information for the resource. */

  char * msg_mid = NULL;
  if ( type_as_str != NULL ) {

    if ( res_as_str != NULL ) {
      rc = asprintf( &msg_mid, "for %s resource '%s'", 
                     type_as_str, res_as_str );
    } else {
      if ( component != NULL ) {
        rc = asprintf( &msg_mid, "for %s resource in component '%s'", 
                       type_as_str, component );
      } else {
        rc = asprintf( &msg_mid, "for %s resource", 
                       type_as_str );
      }
    }

  } else if ( res_as_str != NULL ) { 
    rc = asprintf( &msg_mid, "for resource '%s'", 
                   res_as_str );
  } else {
    if ( component != NULL ) {
      rc = asprintf( &msg_mid, "for resource in component '%s'",
                     component );
    } else {
      msg_mid = strdup("for resource"); 
    }
  }

  if ( rc < 0 ) {
    perror("Failed to build LCFG resource message");
    exit(EXIT_FAILURE);
  }

  /* Final string, possibly with derivation information */

  if ( res != NULL && lcfgresource_has_derivation(res) ) {
    rc = asprintf( &result, "%s %s at %s",
                   msg_base, msg_mid,
                   lcfgresource_get_derivation(res) );
  } else {
    rc = asprintf( &result, "%s %s",
                   msg_base, msg_mid );
  }

  if ( rc < 0 ) {
    perror("Failed to build LCFG resource message");
    exit(EXIT_FAILURE);
  }

  /* Tidy up */

  free(msg_base);
  free(msg_mid);
  free(res_as_str);
  free(type_as_str);

  return result;
}

ssize_t lcfgresource_compute_key_length( const LCFGResource * res,
                                         const char * component,
                                         const char * namespace,
                                         char type_symbol ) {

  if ( !lcfgresource_has_name(res) )
    return -1;

  size_t length = 0;

  if ( type_symbol != '\0' )
    length++;

  if ( namespace != NULL && *namespace != '\0' )
    length += ( strlen(namespace) + 1 ); /* +1 for '.' (period) separator */

  if ( component != NULL && *component != '\0' )
    length += ( strlen(component) + 1 ); /* +1 for '.' (period) separator */

  length += strlen( lcfgresource_get_name(res) );

  return length;
}

ssize_t lcfgresource_insert_key( const LCFGResource * res,
                                 const char * component,
                                 const char * namespace,
                                 char type_symbol,
                                 char * result ) {

  if ( !lcfgresource_has_name(res) )
    return -1;

  char * to = result;

  if ( type_symbol != '\0' ) {
    *to = type_symbol;
    to++;
  }

  if ( namespace != NULL && *namespace != '\0' ) {
    to = stpcpy( to, namespace );

    *to = '.';
    to++;
  }

  if ( component != NULL && *component != '\0' ) {
    to = stpcpy( to, component );

    *to = '.';
    to++;
  }

  to = stpcpy( to, lcfgresource_get_name(res) );

  return ( to - result );
}

ssize_t lcfgresource_build_key( const LCFGResource * res,
                                const char * component,
                                const char * namespace,
                                char type_symbol,
                                char ** result,
                                size_t * size ) {

  ssize_t need_len = lcfgresource_compute_key_length( res,
                                                      component,
                                                      namespace,
                                                      type_symbol );

  if ( need_len < 0 )
    return need_len;

  /* Allocate the required space */

  if ( *result == NULL || *size < ( (size_t) need_len + 1 ) ) {
    *size = need_len + 1;

    *result = realloc( *result, ( *size * sizeof(char) ) );
    if ( *result == NULL ) {
      perror("Failed to allocate memory for LCFG tag string");
      exit(EXIT_FAILURE);
    }

  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  ssize_t write_len = lcfgresource_insert_key( res,
                                               component,
                                               namespace,
                                               type_symbol,
                                               *result );

  return ( write_len == need_len ? write_len : -1 );
}

bool lcfgresource_parse_key( char  * key,
                             char ** hostname,
                             char ** compname,
                             char ** resname,
                             char  * type ) {

  *hostname = NULL;
  *compname = NULL;
  *resname  = NULL;
  *type     = LCFG_RESOURCE_SYMBOL_VALUE;

  if ( key == NULL || *key == '\0' )
    return false;

  char * start = key;

  while ( *start != '\0' && isspace(*start) ) start++;

  if ( *start == '\0' )
    return false;

  if ( *start == LCFG_RESOURCE_SYMBOL_DERIVATION ||
       *start == LCFG_RESOURCE_SYMBOL_TYPE       ||
       *start == LCFG_RESOURCE_SYMBOL_PRIORITY ) {
    *type = *start;
    start++;
  }

  /* Resource name - finds the *last* separator */

  char * sep = strrchr( start, '.' );
  if ( sep != NULL ) {

    if ( *( sep + 1 ) != '\0' ) {
      *sep = '\0';
      *resname = sep + 1;
    } else {
      return false;
    }

  } else {
    *resname = start;
  }

  /* Component name - finds the *last* separator */

  if ( *resname != start ) {

    sep = strrchr( start, '.' );
    if ( sep != NULL ) {

      if ( *( sep + 1 ) != '\0' ) {
        *sep = '\0';
        *compname = sep + 1;
      } else {
        return false;
      }

    } else {
      *compname = start;
    }

    /* Anything left is the hostname / namespace */

    if ( *compname != start )
      *hostname = start;

  }

  return true;
}

bool lcfgresource_set_attribute( LCFGResource * res,
                                 char type_symbol,
                                 char * value,
                                 char ** msg ) {

  *msg = NULL;
  bool ok = false;

  /* Apply the action which matches with the symbol at the start of
     the status line or assume this is a simple specification of the
     resource value. */

  switch (type_symbol)
    {
    case LCFG_RESOURCE_SYMBOL_DERIVATION:

      ok = lcfgresource_set_derivation( res, value );
      if ( !ok )
        asprintf( msg, "Invalid derivation '%s'", value );

      break;
    case LCFG_RESOURCE_SYMBOL_TYPE:
      ok = lcfgresource_set_type_as_string( res, value, msg );

      /* The original value is no longer required here so it must be
         disposed of correctly. */
      if (ok) {
	free(value);
	value = NULL;
      }

      break;
    case LCFG_RESOURCE_SYMBOL_CONTEXT:

      ok = lcfgresource_set_context( res, value );
      if ( !ok )
        asprintf( msg, "Invalid context '%s'", value );

      break;
    case LCFG_RESOURCE_SYMBOL_PRIORITY:
      /* Be careful to only convert string to int if it looks safe */
      ok = lcfgresource_valid_integer(value);

      if (ok) {
        int priority = atoi(value);
        ok = lcfgresource_set_priority( res, priority );
      }

      if ( !ok )
        asprintf( msg, "Invalid priority '%s'", value );

      break;
    default:        /* value line */

      ok = lcfgresource_set_value( res, value );

      if (!ok)
        asprintf( msg, "Invalid value '%s'", value );

      break;
    }

  return ok;
}

/* eof */
