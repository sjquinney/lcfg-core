/**
 * @file resources/resource.c
 * @brief Functions for working with LCFG resources
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#define _GNU_SOURCE /* for asprintf */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#include "derivation.h"
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
 * @brief Create and initialise a new resource
 *
 * Creates a new @c LCFGResource and initialises the parameters to the
 * default values.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgresource_relinquish() function should be called.
 *
 * @return Pointer to new @c LCFGResource
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
  res->type       = LCFG_RESOURCE_DEFAULT_TYPE;
  res->template   = NULL;
  res->context    = NULL;
  res->derivation = NULL;
  res->comment    = NULL;
  res->priority   = LCFG_RESOURCE_DEFAULT_PRIORITY;
  res->_refcount  = 1;

  return res;
}

/**
 * @brief Clone the resource
 *
 * Creates a new @c LCFGResource and copies the values of the
 * parameters from the specified resource. The values for the
 * parameters are copied (e.g. strings are duplicated using
 * @c strdup() ) so that a later change to a parameter in the source
 * resource does not affect the new clone resource.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * @param[in] res Pointer to @c LCFGResource to be cloned.
 *
 * @return Pointer to new @c LCFGResource or @c NULL if copy fails.
 *
 */

LCFGResource * lcfgresource_clone(const LCFGResource * res) {
  assert( res != NULL );

  LCFGResource * clone = lcfgresource_new();
  if ( clone == NULL )
    return NULL;

  /* To simplify memory management the string attributes are
     duplicated. */

  bool ok = true;

  if ( !isempty(res->name) ) {
    char * new_name = strdup(res->name);
    ok = lcfgresource_set_name( clone, new_name );
    if ( !ok )
      free(new_name);
  }

  clone->type = res->type;

  if ( ok && !isempty(res->value) ) {
    char * new_value = strdup(res->value);
    ok = lcfgresource_set_value( clone, new_value );
    if ( !ok )
      free(new_value);
  }

  /* The template is a pointer to an LCFGTemplate so this needs to be
     cloned separately. The simplest approach is to serialise it into
     a string and then recreate a new structure using that as the
     source string. */

  if ( ok && lcfgresource_has_template(res) ) {
    char * tmpl_as_str = NULL;
    size_t tmpl_size = 0;
    ssize_t tmpl_len = lcfgresource_get_template_as_string( res, LCFG_OPT_NONE,
                                                    &tmpl_as_str, &tmpl_size );
    if ( tmpl_len > 0 ) {
      char * tmpl_msg = NULL;
      ok = lcfgresource_set_template_as_string( clone, tmpl_as_str, &tmpl_msg );

      /* Should be impossible to get an error since the template will
         have been previously validated so just throw away any error message */

      free(tmpl_msg);
    }

    free(tmpl_as_str);
  }

  clone->priority = res->priority;

  if ( ok && !isempty(res->comment) ) {
    char * new_comment = strdup(res->comment);
    ok = lcfgresource_set_comment( clone, new_comment );
    if ( !ok )
      free(new_comment);
  }

  if ( ok && !isempty(res->context) ) {
    char * new_context = strdup(res->context);
    ok = lcfgresource_set_context( clone, new_context );
    if ( !ok )
      free(new_context);
  }

  /* Original and clone resources will share the derivation list */
  if ( ok && lcfgresource_has_derivation(res) )
    ok = lcfgresource_set_derivation( clone, lcfgresource_get_derivation(res));

  /* Cloning should never fail */
  if ( !ok ) {
    lcfgresource_destroy(clone);
    clone = NULL;
  }

  return clone;
}

/**
 * @brief Acquire reference to resource
 *
 * This is used to record a reference to the @c LCFGResource, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgresource_relinquish() function should be
 * called.
 *
 * @param[in] res Pointer to @c LCFGResource
 *
 */

void lcfgresource_acquire( LCFGResource * res ) {
  assert( res != NULL );

  res->_refcount += 1;
}

/**
 * @brief Release reference to resource
 *
 * This is used to release a reference to the @c LCFGResource,
 * it does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgresource_destroy() function
 * will be called to clean up the memory associated with the structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * resource which has already been destroyed (or potentially was never
 * created).
 *
 * @param[in] res Pointer to @c LCFGResource
 *
 */

void lcfgresource_relinquish( LCFGResource * res ) {

  if ( res == NULL ) return;

  if ( res->_refcount > 0 )
    res->_refcount -= 1;

  if ( res->_refcount == 0 )
    lcfgresource_destroy(res);

}

/**
 * @brief Destroy the resource
 *
 * When the specified @c LCFGResource is no longer required this will
 * free all associated memory. There is support for reference counting
 * so typically the @c lcfgresource_relinquish() function should be used.
 *
 * This will call @c free() on each parameter of the structure (or
 * @c lcfgtemplate_destroy() for the template parameter ) and then set each
 * value to be @c NULL.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * resource which has already been destroyed (or potentially was never
 * created).
 *
 * @param[in] res Pointer to @c LCFGResource to be destroyed.
 *
 */

void lcfgresource_destroy(LCFGResource * res) {

  if ( res == NULL ) return;

  free(res->name);
  res->name = NULL;

  free(res->value);
  res->value = NULL;

  lcfgtemplate_destroy(res->template);
  res->template = NULL;

  free(res->context);
  res->context = NULL;

  lcfgderivlist_relinquish(res->derivation);
  res->derivation = NULL;

  free(res->comment);
  res->comment = NULL;

  free(res);
  res = NULL;

}

/* Names */

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
 * @param[in] name String to be tested
 *
 * @return boolean which indicates if string is a valid resource name
 *
 */

bool lcfgresource_valid_name( const char * name ) {

  /* MUST NOT be a NULL.
     MUST have non-zero length.
     First character MUST be in [A-Za-z] set. */

  bool valid = ( !isempty(name) && isalpha(*name) );

  /* All other characters MUST be in [A-Za-z0-9_] set */

  const char * ptr;
  for ( ptr = name + 1; valid && *ptr != '\0'; ptr++ )
    if ( !isword(*ptr) ) valid = false;

  return valid;
}

/**
 * @brief Check validity of resource
 *
 * Checks the specified @c LCFGResource to ensure that it is valid. For a
 * resource to be considered valid the pointer must not be @c NULL and
 * the resource must have a name.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return Boolean which indicates if resource is valid
 *
 */

bool lcfgresource_is_valid( const LCFGResource * res ) {
  return ( res != NULL && !isempty(res->name) );
}

/**
 * @brief Check if the resource has a name
 *
 * Checks if the specified @c LCFGResource currently has a value set
 * for the @e name attribute. Although a name is required for an LCFG
 * resource to be valid it is possible for the value of the name to be
 * set to @c NULL when the structure is first created.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return boolean which indicates if a resource has a name
 *
 */

bool lcfgresource_has_name( const LCFGResource * res ) {
  assert( res != NULL );

  return !isempty(res->name);
}

/**
 * @brief Get the name for the resource
 *
 * This returns the value of the @e name parameter for the
 * @c LCFGResource. If the resource does not currently have a @e
 * name then the pointer returned will be @c NULL.
 *
 * It is important to note that this is @b NOT a copy of the string,
 * changing the returned string will modify the @e name of the
 * resource.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return The @e name for the resource (possibly @c NULL).
 */

const char * lcfgresource_get_name( const LCFGResource * res ) {
  assert( res != NULL );

  return res->name;
}

/**
 * @brief Set the name for the resource
 *
 * Sets the value of the @e name parameter for the @c LCFGResource
 * to that specified. It is important to note that this does
 * @b NOT take a copy of the string. Furthermore, once the value is set
 * the resource assumes "ownership", the memory will be freed if the
 * name is further modified or the resource is destroyed.
 *
 * Before changing the value of the @e name to be the new string it
 * will be validated using the @c lcfgresource_valid_name()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] new_name String which is the new name
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_set_name( LCFGResource * res, char * new_name ) {
  assert( res != NULL );

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

/**
 * @brief Get the type for the resource as an integer
 *
 * This returns the value of the type parameter for the
 * @c LCFGResource.
 *
 * An LCFG resource will be one of several types (e.g. string,
 * integer, boolean, list) which controls what validation is done when
 * a value is set. Internally the type is stored as an integer, see
 * the @c LCFGResourceType enum for the full list of supported
 * values. If you require the type as a string then use @c
 * lcfgresource_get_type_as_string()
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return The type of the resource as an integer
 */

LCFGResourceType lcfgresource_get_type( const LCFGResource * res ) {
  assert( res != NULL );

  return res->type;
}

/**
 * @brief Set the type of the resource
 *
 * Changes the type of the resource to that specified. The type of the
 * resource controls what validation will be done when a new value is
 * set. By default a resource is considered to be a "string" which
 * means that any value is permitted. Changing the type to something
 * like "integer" or "boolean" will thus restrict the valid values.

 * If a resource already has a value then it is only possible to
 * change the type to something more restrictive if the value is valid
 * for the new type. For instance, if a resource of type "string" has
 * a value of "foo" the type could be changed to "list" but not
 * "integer". If it is not possible to change the type for a resource
 * then @c errno will be set to @c EINVAL and this function will
 * return false.
 *
 * To set the type for a resource using a string rather than an
 * integer use @c lcfgresource_set_type_as_string()
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] new_type The new resource type as an integer
 *
 * @return boolean indicating success
 */

bool lcfgresource_set_type( LCFGResource * res, LCFGResourceType new_type ) {
  assert( res != NULL );

  if ( res->type == new_type ) return true; /* no-op */

  /* If the type is to be changed then there either must not be an
     existing value or that value must be compatible with the new
     type. For example, if the current value is "fish" and the
     requested new type is "integer" then that change is illegal. */

  bool ok = false;
  if ( res->value == NULL ||
       lcfgresource_valid_value_for_type( new_type, res->value ) ) {

    res->type = new_type;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/**
 * @brief Set the type of the resource to the default
 *
 * Sets the value of the @e type parameter for the @c LCFGResource to
 * the value of @c LCFG_RESOURCE_DEFAULT_TYPE (which is @c string). If
 * the resource type was not previously @c string then any templates
 * or comments associated with the previous type will be erased.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return boolean indicating success
 */

bool lcfgresource_set_type_default( LCFGResource * res ) {
  assert( res != NULL );

  bool ok = true;
  if ( lcfgresource_get_type(res) != LCFG_RESOURCE_DEFAULT_TYPE ) {

    ok = lcfgresource_set_type( res, LCFG_RESOURCE_DEFAULT_TYPE );

    /* Clear any previous templates and comments */
    if (ok) {
      bool tmpl_ok = lcfgresource_set_template( res, NULL );
      bool cmt_ok  = lcfgresource_set_comment( res, NULL );

      if ( !tmpl_ok || !cmt_ok )
	ok = false;
    }
  }

  return ok;
}

/**
 * @brief Set the type of the resource as a string
 *
 * Parses the specified type string and sets the type for the resource
 * (and optionally sets the comment and template parameters). The type
 * is actually set using @c lcfgresource_set_type() so see that
 * function for further details.
 *
 * If the specified type string is NULL or empty then the type
 * defaults to "string". Otherwise the type will be taken from the
 * initial part of the string (if the first character is the LCFG
 * marker '%%' (percent) it will be ignored).
 *
 * Currently supported types are:
 *   + string
 *   + integer
 *   + boolean
 *   + list
 *   + publish
 *   + subscribe
 *
 * The "publish" and "subscribe" types are effectively string
 * sub-types and are only relevant for spanning maps. If any other
 * unsupported type is specified then @c errno will be set to @c
 * EINVAL and the function will return false.
 *
 * Further to the type of the resource a comment may be specified. Any
 * comment will be enclosed in brackets - '(' .. ')'. Typically these
 * only appear with string resources which have additional validation
 * specified by the schema author, the comment then describes the
 * expected format of the value for the resource. For example a type
 * specification for a string resource might be `%%string (MAC
 * address)`.
 *
 * If a resource is a list then there may also be a set of templates
 * specified. These follow a ':' (colon) separator and are a
 * space-separated list of templates of the form @c foo_$_$ where the
 * '$' (dollar) symbols are placeholders which are used to be build the
 * names of sub-resources given the "tags" in the resource value. For
 * example, a type specification for a list resource might be `%%list:
 * foo_$_$ bar_$_$`
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] type_str The new type as a string
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return boolean indicating success
 */

bool lcfgresource_set_type_as_string( LCFGResource * res,
                                      const char * type_str,
                                      char ** msg ) {
  assert( res != NULL );

  bool ok = true;

  /* If the new type string is empty then the resource is considered
     to be the default string type */

  LCFGResourceType new_type = LCFG_RESOURCE_DEFAULT_TYPE;

  /* Spin past any leading whitespace */
  if ( !isempty(type_str) )
    while ( *type_str != '\0' && isspace(*type_str) ) type_str++;

  if ( !isempty(type_str) ) {

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
      lcfgutils_build_message( msg, "invalid resource type '%s'", type_str );
      ok = false;
    }
  }

  if (ok)
    ok = lcfgresource_set_type( res, new_type );

  /* Check if there is a comment string for the resource. This would
     be after the type string and enclosed in brackets - ( ) */

  const char * posn = type_str;

  if ( ok && !isempty(type_str) ) {

    const char * comment_start = strchr( type_str, '(' );
    if ( comment_start != NULL ) {
      const char * comment_end = strchr( comment_start, ')' );

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
    const char * tmpl_start = strstr( posn, ": " );
    if ( tmpl_start != NULL ) {
      tmpl_start += 2;

      /* Spin past any further whitespace */
      while( *tmpl_start != '\0' && isspace(*tmpl_start) ) tmpl_start++;

      /* A list type might be declared like "list: " (i.e. with no
         templates) in which case it is a simple list of tags. */

      if ( *tmpl_start != '\0' )
        ok = lcfgresource_set_template_as_string( res, tmpl_start, msg );
    }
  }

  return ok;
}

/**
 * @brief Check if the resource is a string
 *
 * Checks if the type parameter for the @c LCFGResource is one of:
 *
 *    - @c LCFG_RESOURCE_TYPE_STRING
 *    - @c LCFG_RESOURCE_TYPE_SUBSCRIBE
 *    - @c LCFG_RESOURCE_TYPE_PUBLISH
 *
 * Since "publish" and "subscribe" resources can hold any value which
 * is to be mapped between profiles they are considered to be
 * string-like for most operations. This does mean that sometimes
 * extra care must be taken to handle them correctly.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return boolean indicating if string-type
 */

bool lcfgresource_is_string( const LCFGResource * res ) {
  assert( res != NULL );

  return ( res->type == LCFG_RESOURCE_TYPE_STRING    ||
           res->type == LCFG_RESOURCE_TYPE_SUBSCRIBE ||
           res->type == LCFG_RESOURCE_TYPE_PUBLISH );
}

/**
 * @brief Check if the resource is a integer
 *
 * Checks if the type parameter for the @c LCFGResource is 
 * @c LCFG_RESOURCE_TYPE_INTEGER.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return boolean indicating if integer-type
 */

bool lcfgresource_is_integer( const LCFGResource * res ) {
  assert( res != NULL );

  return ( lcfgresource_get_type(res) == LCFG_RESOURCE_TYPE_INTEGER );
}

/**
 * @brief Check if the resource is a boolean
 *
 * Checks if the type parameter for the @c LCFGResource is 
 * @c LCFG_RESOURCE_TYPE_BOOLEAN.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return boolean indicating if boolean-type
 */

bool lcfgresource_is_boolean( const LCFGResource * res ) {
  assert( res != NULL );

  return ( lcfgresource_get_type(res) == LCFG_RESOURCE_TYPE_BOOLEAN );
}

/**
 * @brief Check if the resource is a list
 *
 * Checks if the type parameter for the @c LCFGResource is
 * @c LCFG_RESOURCE_TYPE_LIST.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return boolean indicating if list-type
 */

bool lcfgresource_is_list( const LCFGResource * res ) {
  assert( res != NULL );

  return ( lcfgresource_get_type(res) == LCFG_RESOURCE_TYPE_LIST );
}

/**
 * @brief Check if the resource value is true
 *
 * Checks if the value parameter for the @c LCFGResource can be
 * considered to be "true". This uses similar rules to Perl. For
 * resources of the boolean type the canonical value of "yes" is
 * considered to be true and anything else is false. For all other
 * types of resource if the value is @c NULL, "" (empty string), or "0"
 * (zero) it will considered to be false and otherwise true.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return boolean indicating if the value is true
 */

bool lcfgresource_is_true( const LCFGResource * res ) {
  assert( res != NULL );

  bool is_true = false;

  const char * value = res->value;
  if ( !isempty(value) ) {

    if ( lcfgresource_is_boolean(res) )
      is_true = ( strcmp( value, "yes" ) == 0 );
    else
      is_true = ( strcmp( value, "0" ) != 0 );

  }

  return is_true;
}

/**
 * @brief Get the resource type as a string
 *
 * Generates a new LCFG type string based on the values for the @e
 * type, @e comment and @e template parameters.
 *
 * This converts the integer @e type parameter for the resource into
 * an LCFG type string which is formatted as described in @c
 * lcfgresource_set_type_as_string(). For a list resource type the
 * templates section can be disabled by specifying the @c
 * LCFG_OPT_NOTEMPLATES option.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return New string containing resource type information (call @c free() when no longer required)
 *
 */

ssize_t lcfgresource_get_type_as_string( const LCFGResource * res,
                                         LCFGOption options,
                                         char ** result, size_t * size ) {
  assert( res != NULL );

  const char * type_string = lcfgresource_type_names[res->type];
  size_t type_len = strlen(type_string);

  size_t new_len = type_len;

  const char * comment = res->comment;
  size_t comment_len = 0;
  if ( !isempty(comment) ) {
    comment_len = strlen(comment);

    new_len += ( comment_len + 2 ); /* + 2 for enclosing ( ) */
  }

  /* A list resource might have a template */

  char * tmpl_as_str = NULL;
  ssize_t tmpl_len = 0;
  if ( lcfgresource_is_list(res) &&
       !(options&LCFG_OPT_NOTEMPLATES) ) {

    /* Always adds the ': ' separator even when there are no templates */

    new_len += 2;  /* +2 for ': ' separator */

    if ( lcfgresource_has_template(res) ) {
      size_t tmpl_size = 0;
      tmpl_len = lcfgresource_get_template_as_string( res, LCFG_OPT_NONE,
                                                     &tmpl_as_str, &tmpl_size );

      if ( tmpl_len > 0 ) /* Avoid adding negative number */
        new_len += tmpl_len;
    }

  }

  if ( options&LCFG_OPT_NEWLINE )
    new_len += 1;

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    size_t new_size = new_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG type string");
      exit(EXIT_FAILURE);
    } else {
      *result = new_buf;
      *size   = new_size;
    }
  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  to = stpncpy( to, type_string, type_len );

  if ( comment_len > 0 ) {
    *to = '(';
    to++;

    to = stpncpy( to, comment, comment_len );

    *to = ')';
    to++;
  }

  if ( lcfgresource_is_list(res) &&
       !(options&LCFG_OPT_NOTEMPLATES) ) {

    to = stpncpy( to, ": ", 2 );

    if ( tmpl_len > 0 )
      to = stpncpy( to, tmpl_as_str, tmpl_len );
  }

  free(tmpl_as_str);

  /* Optional newline at the end of the string */

  if ( options&LCFG_OPT_NEWLINE )
    to = stpncpy( to, "\n", 1 );

  *to = '\0';

  assert( ( *result + new_len ) == to );

  return new_len;
}

/* Templates */

/**
 * @brief Check if the resource has a set of templates
 *
 * Checks if the specified @c LCFGResource currently has a value set
 * for the template attribute. This is only relevant for list
 * resources. The templates are used to convert the list of LCFG tags
 * into the names of associated sub-resources.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return boolean which indicates if a resource has a template
 *
 */

bool lcfgresource_has_template( const LCFGResource * res ) {
  assert( res != NULL );

  return ( res->template != NULL );
}

/**
 * @brief Get the template for the resource
 *
 * This returns the value of the @e template parameter for the
 * @c LCFGResource. If the resource does not currently have a @e
 * template then the pointer returned will be @c NULL.
 *
 * Templates for a list resource are stored as a linked-list using the
 * @c LCFGTemplate. To get them as a formatted string use the
 * @c lcfgresource_get_template_as_string() function.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return A pointer to an @c LCFGTemplate (or a @c NULL value).
 */

LCFGTemplate * lcfgresource_get_template( const LCFGResource * res ) {
  assert( res != NULL );

  return res->template;
}

/**
 * @brief Get the template for the resource as a string
 *
 * If the @c LCFGResource has a value for the @e template attribute it
 * will be transformed into a new string using the @c
 * lcfgtemplate_to_string() function. If the resource does not have a
 * template then zero will be returned and the buffer will not have
 * been modified.
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many resource strings, this can be a
 * huge performance benefit. If the buffer is initially unallocated
 * then it MUST be set to @c NULL. The current size of the buffer must
 * be passed and should be specified as zero if the buffer is
 * initially unallocated. If the generated string would be too long
 * for the current buffer then it will be resized and the size
 * parameter is updated.
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgresource_get_template_as_string( const LCFGResource * res,
                                             LCFGOption options,
                                             char ** result, size_t * size ) {
  assert( res != NULL );

  if ( lcfgresource_has_template(res) ) {

    return lcfgtemplate_to_string( lcfgresource_get_template(res),
                                   NULL, options,
                                   result, size );
  } else {
    return 0;
  }

}

/**
 * @brief Set the template for the resource.
 *
 * Sets the specified @c LCFGTemplate as the value for the @e
 * template attribute in the @c LCFGResource.
 *
 * The use of templates is only relevant for list resources. They are
 * used to transform a list of tags into the names of associated
 * sub-resources.
 *
 * Usually the template will be specified as a string which needs to
 * be converted into an @c LCFGTemplate, in that situation use
 * the @c lcfgresource_set_template_as_string() function.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] new_tmpl Pointer to an @c LCFGTemplate
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_set_template( LCFGResource * res,
                                LCFGTemplate * new_tmpl ) {
  assert( res != NULL );

  /* Allow setting the template to NULL which is basically an "unset" */

  bool ok = false;
  if ( new_tmpl == NULL || lcfgtemplate_is_valid(new_tmpl) ) {
    lcfgtemplate_destroy(res->template);

    res->template = new_tmpl;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/**
 * @brief Set the template for a resource as a string.
 *
 * Converts the specified string into a linked-list of @c LCFGTemplate
 * structures. The pointer to the head of the new template list is set as
 * the value for the @e template attribute in the @c LCFGResource.
 *
 * The templates string is expected to be a space-separated list of
 * parts of the form @c foo_$_$ where the '$' (dollar) placeholders
 * are replaced with tag names when the resource names are
 * generated. The string is parsed using the @c
 * lcfgtemplate_from_string() function, see that for full details. If
 * the string cannot be parsed then @c errno will be set to @c EINVAL
 * and a false value will be returned.
 *
 * The use of templates is only relevant for list resources. They are
 * used to transform a list of tags into the names of associated
 * sub-resources.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] new_tmpl_str String of LCFG resource templates
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_set_template_as_string(  LCFGResource * res,
                                           const char * new_tmpl_str,
                                           char ** msg ) {
  assert( res != NULL );

  if ( new_tmpl_str == NULL ) {
    errno = EINVAL;
    return false;
  }

  if (  *new_tmpl_str == '\0' )
    return lcfgresource_set_template( res, NULL );

  LCFGTemplate * new_template = NULL;
  char * parse_msg = NULL;
  LCFGStatus rc = lcfgtemplate_from_string( new_tmpl_str, &new_template,
                                            &parse_msg );

  bool ok = ( rc == LCFG_STATUS_OK && new_template != NULL );

  if (ok)
    ok = lcfgresource_set_template( res, new_template );


  if (!ok) {
    lcfgutils_build_message( msg, "Invalid template '%s': %s", new_tmpl_str,
              ( parse_msg != NULL ? parse_msg : "unknown error" ) );

    lcfgtemplate_destroy(new_template);

    errno = EINVAL;
  }

  free(parse_msg);

  return ok;
}

/* Values */

/**
 * @brief Check if the resource has a value
 *
 * Checks to see if the @e value parameter for the @c LCFGResource
 * is considered to be a non-empty value.
 *
 * @param[in] res Pointer to an LCFGResource
 *
 * @return A boolean which indicates if the resource has a non-empty value
 */

bool lcfgresource_has_value( const LCFGResource * res ) {
  assert( res != NULL );

  return !isempty(res->value);
}

/**
 * @brief Get the value for the resource
 *
 * This returns the value of the @e value parameter for the
 * @c LCFGResource. If the resource does not currently have a
 * @e value then the returned pointer will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e value for the
 * resource.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return The @e value string for the resource (possibly NULL).
 */

const char * lcfgresource_get_value( const LCFGResource * res ) {
  assert( res != NULL );

  return res->value;
}

static const char unsafe_chars[] = "\r\n&";

/**
 * Test if the resource values needs encoding in status files
 *
 * This can be used to test if the @e value for the resource contains
 * any characters which need to be encoded before insertion into @e
 * status and @e hold files. This checks for significant whitespace
 * (e.g. newlines and carriage returns) as well as the @c '&'
 * (ampersand) character which is used when encoding characters.
 * 
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return Boolean which indicates if encoding is required
 *
 */

bool lcfgresource_value_needs_encode( const LCFGResource * res ) {
  assert( res != NULL );

  if ( !lcfgresource_has_value(res) ) return false;

  bool needs_encode = false;

  const char * value = lcfgresource_get_value(res);
  const char * ptr;
  for ( ptr = value; *ptr != '\0'; ptr++ ) {
    if ( strchr( unsafe_chars, *ptr ) ) {
      needs_encode = true;
      break;
    }
  }

  return needs_encode;
}

/**
 * @brief Get an encoded version of the value for the resource
 *
 * Generates a new string which is an encoded version of the @e value
 * parameter for the @c LCFGResource. The encoded value is made
 * safe for use in LCFG status files by escaping newline and
 * carriage-return characters (and also ampersands). Note that this is
 * NOT a general encoding function and it does not make the value safe
 * for direct inclusion in XML or HTML.
 *
 * If the current value for the @e value parameter is @c NULL then
 * this function will return a @c NULL value.
 *
 * When the returned string is no longer required the memory must be
 * freed.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return The encoded @e value string for the resource (call @c free() when no longer required) or a @c NULL value
 */

char * lcfgresource_enc_value( const LCFGResource * res ) {
  assert( res != NULL );

  const char * value = lcfgresource_get_value(res);

  if ( value == NULL ) return NULL;

  size_t value_len = strlen(value);

  /* The following characters need to be encoded to ensure they do not
     corrupt the format of status files. */

  static const char cr[]  = "&#xD;";   /* \r */
  static const char lf[]  = "&#xA;";   /* \n */
  static const char amp[] = "&#x26;";  /* &  */

  static const size_t cr_len  = sizeof(cr)  - 1;
  static const size_t lf_len  = sizeof(lf)  - 1;
  static const size_t amp_len = sizeof(amp) - 1;

  size_t extend = 0;
  const char * ptr;
  for ( ptr=value; *ptr!='\0'; ptr++ ) {
    switch(*ptr)
      {
      case '\r':
        extend += ( cr_len - 1 );
        break;
      case '\n':
	extend += ( lf_len - 1 );
	break;
      case '&':
	extend += ( amp_len - 1 );
	break;
      }
  }

  if ( extend == 0 ) return strdup(value);

  /* Some characters need encoding */

  size_t new_len = value_len + extend;

  char * enc_value = calloc( ( new_len + 1 ), sizeof(char) );
  if ( enc_value == NULL ) {
    perror("Failed to allocate memory for LCFG resource value");
    exit(EXIT_FAILURE);
  }

  char * to = enc_value;

  for ( ptr=value; *ptr!='\0'; ptr++ ) {
    switch(*ptr)
      {
      case '\r':
	to = stpncpy( to, cr, cr_len );
	break;
      case '\n':
	to = stpncpy( to, lf, lf_len );
	break;
      case '&':
	to = stpncpy( to, amp, amp_len );
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

/**
 * @brief Check if a value is a valid boolean
 *
 * Checks whether the string contains a valid LCFG boolean
 * value. There are various acceptable forms for a value for a boolean
 * LCFG resource (see @c lcfgresource_canon_boolean() for details) but
 * only the empty string and "yes" are considered to be @e valid
 * boolean values. Anything else must be canonicalised from an
 * accepted form into the equivalent valid value.
 *
 * @param[in] value A string to be checked
 *
 * @return A boolean which indicates if the string contains a boolean
 */

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

/**
 * @brief Canonicalise a boolean value
 *
 * Converts various acceptable forms of the boolean value into the
 * equivalent valid forms. Input can be any of:
 *
 *   + false, no, off, 0, ""
 *   + true, yes, on, 1
 *
 * the characters of the input string can be in any case (upper, lower
 * or mixed). The strings are canonicalised into "yes" for true values
 * and "" (empty string) for false values.
 *
 * A new string will be returned, the input will not be modified. When
 * no longer required the newly created string should be freed. If the
 * input cannot be canonicalised the function will return a @c NULL
 * value.
 *
 * @param[in] value The boolean value string to be canonicalised
 *
 * @return A new string (call @c free() when no longer required) or a @c NULL value if the input could not be canonicalised.
 */

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

/**
 * @brief Check if a value is a valid integer
 *
 * Checks whether the string contains a valid LCFG integer value. The
 * string may begin with a negative sign '-', otherwise all characters
 * MUST be digits - @c [0-9].
 *
 * @param[in] value A string to be checked
 *
 * @return A boolean which indicates if the string contains an integer
 */

bool lcfgresource_valid_integer( const char * value ) {

  /* MUST NOT be a NULL. */

  if ( value == NULL ) return false;

  /* First character may be a negative-sign, if so walk past */

  const char * ptr = value;
  if ( *ptr == '-' )
    ptr++;

  /* MUST be further characters.
     MUST not have leading zeroes unless it is just "0" */

  bool valid = ( *ptr != '\0' &&
                 ( *ptr != '0' || strcmp( ptr, "0" ) == 0 ) );

  /* All other characters MUST be digits */

  for ( ; valid && *ptr != '\0'; ptr++ )
    if ( !isdigit(*ptr) ) valid = false;

  return valid;
}

/**
 * @brief Check if a value is a valid list
 *
 * Checks whether the string contains a valid list of LCFG tags. An
 * LCFG tag is used to generate resource names along with a template,
 * consequently tags MUST only contain characters which are valid in
 * LCFG resource names (@c [A-Za-z0-9_]). Tags in a list are separated
 * using space characters. If any other characters are found the
 * string is not a valid tag list.
 *
 * @param[in] value A string to be checked
 *
 * @return A boolean which indicates if the string contains a list of tags
 */

bool lcfgresource_valid_list( const char * value ) {

  /* MUST NOT be a NULL */

  bool valid = ( value != NULL );

  /* Empty list is fine.
     Tags MUST be characters in [A-Za-z0-9_] set.
     Separators MUST be spaces */

  const char * ptr;
  for ( ptr = value; valid && *ptr != '\0'; ptr++ )
    if ( !isword(*ptr) && *ptr != ' ' ) valid = false;

  return valid;
}

/**
 * @brief Check if a value is valid for the type
 *
 * Checks whether a string contains a value which is valid for the
 * specified LCFG resource type. This will call the relevant
 * validation function for the specified type (e.g. @c
 * lcfgresource_valid_boolean), if there is one, and return the
 * result. If no validation function is available for the type this
 * will just return true.
 *
 * @param[in] type Integer LCFG resource type
 * @param[in] value A string to be checked
 *
 * @return A boolean which indicates if the string is valid for the type
 *
 */

bool lcfgresource_valid_value_for_type( LCFGResourceType type,
                                        const char * value ) {

  /* Check if the specified value is valid for the current type of the
     resource. */

  if ( value == NULL ) return false;

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
    case LCFG_RESOURCE_TYPE_STRING:
    case LCFG_RESOURCE_TYPE_PUBLISH:
    case LCFG_RESOURCE_TYPE_SUBSCRIBE:
      valid = true;
      break;
    }

  return valid;
}

/**
 * @brief Check if a value is valid for the resource
 *
 * It can often be useful to test the validity of a potential new
 * value for a resource before proceeding with further work. This
 * checks to see if the specified value is valid for the type of the
 * @c LCFGResource, this is done using the @c
 * lcfgresource_valid_value_for_type() function.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] value A string to be checked
 *
 * @return A boolean which indicates if the string is valid for the resource
 *
 */

bool lcfgresource_valid_value( const LCFGResource * res,
                               const char * value ) {
  assert( res != NULL );

  return lcfgresource_valid_value_for_type( lcfgresource_get_type(res),
                                            value );
}

/**
 * @brief Set the value for the resource
 *
 * Sets the value of the @e value parameter for the @c LCFGResource
 * to that specified. It is important to note that this does
 * NOT take a copy of the string. Furthermore, once the value is set
 * the resource assumes "ownership", the memory will be freed if the
 * value is further modified or the resource is destroyed.
 *
 * Before changing the value of the @e value parameter to be the new
 * string it will be validated using the @c lcfgresource_valid_value()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] new_value String which is the new value
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_set_value( LCFGResource * res, char * new_value ) {
  assert( res != NULL );

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

/**
 * @brief Unset the value for the resource
 *
 * Sets the value for the @e value parameter of the @c LCFGResource
 * back to @c NULL and frees any memory associated with a
 * previous value.
 *
 * This is useful since it is not permitted to set the value for an
 * LCFG resource to be @c NULL via the @c lcfgresource_set_value
 * function.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_unset_value( LCFGResource * res ) {
  assert( res != NULL );

  free(res->value);
  res->value = NULL;

  return true;
}

/* Derivations */

/**
 * @brief Check if the resource has a derivation
 *
 * Checks if there is any derivation information stored in the
 * @c LCFGResource.
 *
 * Derivation information is typically a space-separated list of files
 * and line numbers where the resource value has been modified.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return Boolean which indicates if the resource has derivation information
 *
 */

bool lcfgresource_has_derivation( const LCFGResource * res ) {
  assert( res != NULL );

  return ( res->derivation != NULL &&
           !lcfgderivlist_is_empty(res->derivation));
}

/**
 * @brief Get the derivation for the resource
 *
 * This returns the value of the @e derivation parameter for the @c
 * LCFGResource. This will be a pointer to an @c LCFGDerivationList
 * structure. If the resource does not currently have a @e derivation
 * then the pointer returned will be @c NULL.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return The @e derivation for the resource (possibly NULL).
 */

LCFGDerivationList * lcfgresource_get_derivation( const LCFGResource * res ) {
  assert( res != NULL );

  return res->derivation;
}

/**
 * @brief Get the derivation for the resource as a string
 *
 * This returns the stringified value of the @e derivation parameter
 * for the @c LCFGResource. The serialisation is done using the @c
 * lcfgderivlist_to_string() function. If the resource does not
 * currently have a @e derivation then the pointer returned will be @c
 * NULL.
 *
 * This function uses a string buffer which may be pre-allocated if
 * necessary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many strings, this can be a huge
 * performance benefit. If the buffer is initially unallocated then it
 * MUST be set to @c NULL. The current size of the buffer must be
 * passed and should be specified as zero if the buffer is initially
 * unallocated. If the generated string would be too long for the
 * current buffer then it will be resized and the size parameter is
 * updated.
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 * 
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgresource_get_derivation_as_string( const LCFGResource * res,
                                               LCFGOption options,
                                               char ** result, size_t * size ){
  assert( res != NULL );

  const LCFGDerivationList * drvlist = lcfgresource_get_derivation(res);
  return lcfgderivlist_to_string( drvlist, options, result, size );
}

/**
 * @brief Get the length of the resource derivation string
 *
 * It is sometimes necessary to know the length of the derivation for
 * the @c LCFGResource as a string before it is actually stored. This
 * simply calls the @c lcfgderivlist_get_string_length() function on
 * the @c LCFGDerivationList structure for the resource.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return The length of the derivation string
 *
 */
 
size_t lcfgresource_get_derivation_length( const LCFGResource * res ) {
   assert( res != NULL );

   if ( !lcfgresource_has_derivation(res) ) return 0;

   const LCFGDerivationList * drvlist = lcfgresource_get_derivation(res);
   return lcfgderivlist_get_string_length(drvlist);
}

/**
 * @brief Set the derivation for the resource
 *
 * Sets the value of the @e derivation parameter for the @c
 * LCFGResource to the specified @c LCFGDerivationList. 
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] new_deriv Pointer to new @c LCFGDerivationList structure
 *
 * @return boolean indicating success
 *
 */
 
bool lcfgresource_set_derivation( LCFGResource * res,
                                  LCFGDerivationList * new_deriv ) {

  lcfgderivlist_relinquish(res->derivation);

  if ( new_deriv != NULL )  /* Supports unsetting the derivation */
    lcfgderivlist_acquire(new_deriv);

  res->derivation = new_deriv;

  return true;
}

/**
 * @brief Set the derivation for the resource as a string
 *
 * Sets the value of the @e derivation parameter for the @c
 * LCFGResource to that specified. The derivation string is parsed
 * using @c lcfgderivlist_from_string().
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] new_deriv String which is the new derivation
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_set_derivation_as_string( LCFGResource * res,
                                            const char * new_deriv ) {
  assert( res != NULL );

  bool ok = true;

  LCFGDerivationList * new_drvlist = NULL;
  if ( new_deriv != NULL ) { /* Supports unsetting the derivation */

    char * msg = NULL;

    LCFGStatus status = lcfgderivlist_from_string( new_deriv, &new_drvlist,
                                                   &msg );
    if ( status == LCFG_STATUS_ERROR )
      ok = false;

    free(msg);
  }

  if (ok)
    ok = lcfgresource_set_derivation( res, new_drvlist );

  lcfgderivlist_relinquish(new_drvlist);

  return ok;
}

/**
 * @brief Merge derivations for one resource into another
 *
 * This can be used to merge the @c LCFGDerivationList from one @c
 * LCFGResource into another. There is support for Copy On Write so
 * that if a derivation list is shared between multiple resources it
 * will be cloned before the merging is done. This is used when
 * merging resource specifications by prefix.
 *
 * @param[in] res1 Pointer to @c LCFGResource into which derivation is merged
 * @param[in] res2 Pointer to @c LCFGResource from which derivation is merged
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_merge_derivation( LCFGResource * res1,
                                    const LCFGResource * res2 ) {
  assert( res1 != NULL );

  LCFGDerivationList * drvlist1 = lcfgresource_get_derivation(res1);

  if ( res2 == NULL || !lcfgresource_has_derivation(res2) ) return true;
  LCFGDerivationList * drvlist2 = lcfgresource_get_derivation(res2);

  if ( lcfgderivlist_is_empty(drvlist1) )
    return lcfgresource_set_derivation( res1, drvlist2 );

  /* The main aim of the cloning is to provide COW for shared
     derivations. Keeping it simple by doing the clone for all
     merges. This also provides extra safety against corruption. */

  bool ok = false;

  LCFGDerivationList * clone = lcfgderivlist_clone(drvlist1);
  if ( clone != NULL ) {
    LCFGChange merge_rc = lcfgderivlist_merge_list( clone, drvlist2 );

    if ( LCFGChangeOK(merge_rc) ) {
      /* Only keep the clone if it was modified */
      if ( merge_rc == LCFG_CHANGE_NONE )
        ok = true;
      else
        ok = lcfgresource_set_derivation( res1, clone );
    }

    lcfgderivlist_relinquish(clone);
  }

  return ok;
}

/**
 * @brief Add extra derivation information for the resource
 *
 * Adds the extra derivation information to the value for the @e
 * derivation parameter in the @c LCFGResource if it is not already
 * found. This is done using the @c lcfgderivlist_merge_string_list()
 * function. There is support for Copy On Write so that if a
 * derivation list is shared between multiple resources it will be
 * cloned before the merging is done.

 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] extra_deriv String which is the additional derivation
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_add_derivation_string( LCFGResource * res,
                                         const char * extra_deriv ) {
  assert( res != NULL );

  if ( isempty(extra_deriv) ) return true;

  bool ok = true;
  LCFGDerivationList * drvlist = lcfgresource_get_derivation(res);
  LCFGDerivationList * clone   = lcfgderivlist_clone(drvlist);
  if ( clone != NULL ) {
    char * merge_msg = NULL;
    LCFGChange merge_rc = 
      lcfgderivlist_merge_string_list( clone, extra_deriv, &merge_msg );
    free(merge_msg);

    if ( LCFGChangeOK(merge_rc) ) {
      /* Only keep the clone if it was modified */
      if ( merge_rc == LCFG_CHANGE_NONE )
        ok = true;
      else
        ok = lcfgresource_set_derivation( res, clone );
    }

    lcfgderivlist_relinquish(clone);
  }

  return ok;
}

/**
 * @brief Add extra derivation information for the resource
 *
 * Adds the extra filename to the @c LCFGDerivationList for the @c
 * LCFGResource if it is not already found. If the specified line
 * number is non-negative it will be added to the list of line numbers
 * for the specified file. This is done using the @c
 * lcfgderivlist_merge_file_line() function. There is support for Copy
 * On Write so that if a derivation list is shared between multiple
 * resources it will be cloned before the merging is done.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] filename Name of derivation file
 * @param[in] line Line number of derivation
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_add_derivation_file_line( LCFGResource * res,
                                            const char * filename,
                                            int line ) {
  assert( res != NULL );

  if ( isempty(filename) ) return true;

  bool ok = true;
  LCFGDerivationList * drvlist = lcfgresource_get_derivation(res);
  LCFGDerivationList * clone   = lcfgderivlist_clone(drvlist);
  if ( clone != NULL ) {
    LCFGChange merge_rc =
      lcfgderivlist_merge_file_line( clone, filename, line );

    if ( LCFGChangeOK(merge_rc) ) {
      /* Only keep the clone if it was modified */
      if ( merge_rc == LCFG_CHANGE_NONE )
        ok = true;
      else
        ok = lcfgresource_set_derivation( res, clone );
    }

    lcfgderivlist_relinquish(clone);
  }

  return ok;
}

/* Context Expression */

/**
 * @brief Check if a string is a valid LCFG context expression
 *
 * Checks the contents of a specified string against the specification
 * for an LCFG context expression. See @c lcfgcontext_valid_expression
 * for details.
 *
 * @param[in] ctx String to be tested
 *
 * @return boolean which indicates if string is a valid context
 *
 */

bool lcfgresource_valid_context( const char * ctx ) {
  char * msg = NULL;
  bool valid = lcfgcontext_valid_expression( ctx, &msg );
  free(msg); /* Just ignore any error message */
  return valid;
}

/**
 * @brief Check if the resource has a context
 *
 * Checks if there is any context information associated with the
 * @c LCFGResource.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return Boolean which indicates if the resource has a context
 *
 */

bool lcfgresource_has_context( const LCFGResource * res ) {
  assert( res != NULL );

  return !isempty(res->context);
}

/**
 * @brief Get the context for the resource
 *
 * This returns the value of the @e context parameter for the
 * @c LCFGResource. If the resource does not currently have a
 * value for the @e context then the pointer returned will be @c
 * NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e context for the
 * resource.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return The @e context for the resource (possibly NULL).
 */

const char * lcfgresource_get_context( const LCFGResource * res ) {
  assert( res != NULL );

  return res->context;
}

/**
 * @brief Set the context for the resource
 *
 * Sets the value of the @e context parameter for the @c
 * LCFGResource to that specified. It is important to note that
 * this does NOT take a copy of the string. Furthermore, once the
 * value is set the resource assumes "ownership", the memory will be
 * freed if the context is further modified or the resource is
 * destroyed.
 *
 * Before changing the value of the @e context to be the new string it
 * will be validated using the @c lcfgresource_valid_context()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] new_ctx String which is the new context
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_set_context( LCFGResource * res, char * new_ctx ) {
  assert( res != NULL );

  bool ok = false;
  if ( new_ctx == NULL || lcfgresource_valid_context(new_ctx) ) {
    free(res->context);

    res->context = new_ctx;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/**
 * @brief Add extra context information for the resource
 *
 * Adds the extra context information to the value for the @e
 * context parameter in the @c LCFGResource if it is not
 * already found in the string.
 *
 * If not already present in the existing information a new context
 * string is built which is the combination of any existing string
 * with the new string appended, the strings are combined using @c
 * lcfgcontext_combine_expressions(). The new string is passed to @c
 * lcfgresource_set_context(), unlike that function this does NOT
 * assume "ownership" of the input string.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] extra_context String which is the additional context
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_add_context( LCFGResource * res,
                               const char * extra_context ) {
  assert( res != NULL );

  if ( isempty(extra_context) ) return true;

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

/**
 * @brief Check if the resource has a comment
 *
 * Checks if there is any comment stored in the @c LCFGResource.
 *
 * Typically only string resources with additional validation added by
 * the schema author will have a comment. The comment often describes
 * the expected format for the value (e.g. MAC address) so that a
 * helpful error can be printed when an invalid value is specified.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return Boolean which indicates if the resource has a comment
 *
 */

bool lcfgresource_has_comment( const LCFGResource * res ) {
  assert( res != NULL );

  return !isempty(res->comment);
}

/**
 * @brief Get the comment for the resource
 *
 * This returns the value of the @e comment parameter for the @c
 * LCFGResource. If the resource does not currently have a
 * value for the @e comment then the pointer returned will be @c
 * NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e comment for the
 * resource.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return The @e comment for the resource (possibly NULL).
 */

const char * lcfgresource_get_comment( const LCFGResource * res ) {
  assert( res != NULL );

  return res->comment;
}

/**
 * @brief Set the comment for the resource
 *
 * Sets the value of the @e comment parameter for the
 * @c LCFGResource to that specified. It is important to note that
 * this does NOT take a copy of the string. Furthermore, once the
 * value is set the resource assumes "ownership", the memory will be
 * freed if the comment is further modified or the resource is
 * destroyed.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] new_comment String which is the new comment
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_set_comment( LCFGResource * res, char * new_comment ) {
  assert( res != NULL );

  free(res->comment);

  res->comment = new_comment;

  return true;
}

/* Priority */

/**
 * @brief Get the priority for the resource
 *
 * This returns the value of the integer @e priority parameter for the
 * @c LCFGResource. The priority is calculated using the context
 * expression for the resource (if any) along with the current active
 * set of contexts for the system.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return The priority for the resource as an integer
 *
 */

int lcfgresource_get_priority( const LCFGResource * res ) {
  assert( res != NULL );

  return res->priority;
}

/**
 * @brief Get the priority for the resource as a string
 *
 * This returns the stringified value of the integer @e priority
 * parameter for the @c LCFGResource. The priority is calculated using
 * the context expression for the resource (if any) along with the
 * current active set of contexts for the system.
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many resource strings, this can be a
 * huge performance benefit. If the buffer is initially unallocated
 * then it MUST be set to @c NULL. The current size of the buffer must
 * be passed and should be specified as zero if the buffer is
 * initially unallocated. If the generated string would be too long
 * for the current buffer then it will be resized and the size
 * parameter is updated.
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgresource_get_priority_as_string( const LCFGResource * res,
                                             LCFGOption options,
                                             char ** result, size_t * size ) {
  assert( res != NULL );

  size_t new_len = 0;

  int priority = lcfgresource_get_priority(res);
  if ( priority < 0 ) {
    new_len++;
    priority *= -1;
  }
  while ( priority > 9 ) { new_len++; priority/=10; }
  new_len++;

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    size_t new_size = new_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG priority string");
      exit(EXIT_FAILURE);
    } else {
      *result = new_buf;
      *size   = new_size;
    }
  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  int rc =
    snprintf( *result, new_len + 1, "%d", lcfgresource_get_priority(res) );

  assert( rc > 0 && (size_t) rc == new_len );

  return new_len;
}

/**
 * @brief Set the priority for the resource
 *
 * Sets the value of the @e priority parameter for the @c LCFGResource
 * to that specified. 
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] new_prio Integer which is the new priority
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_set_priority( LCFGResource * res, int new_prio ) {
  assert( res != NULL );

  res->priority = new_prio;
  return true;
}

/**
 * @brief Set the priority for the resource to the default value
 *
 * Sets the value of the @e priority parameter for the @c LCFGResource
 * to the value of @c LCFG_RESOURCE_DEFAULT_PRIORITY (which is zero). 
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_set_priority_default( LCFGResource * res ) {
  assert( res != NULL );

  return lcfgresource_set_priority( res, LCFG_RESOURCE_DEFAULT_PRIORITY );
}

/**
 * @brief Evaluate the priority for the resource for a list of contexts
 *
 * This will evaluate and update the value of the @e priority
 * attribute for the LCFG resource using the value set for the
 * @e context attribute (if any) and the list of LCFG contexts passed in
 * as an argument. The priority is evaluated using
 * @c lcfgctxlist_eval_expression().
 *
 * The default value for the priority is zero, if the resource is
 * applicable for the specified list of contexts the priority will be
 * positive otherwise it will be negative.
 *
 * @param[in] res Pointer to an @c LCFGResource
 * @param[in] ctxlist List of LCFG contexts
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_eval_priority( LCFGResource * res,
                                 const LCFGContextList * ctxlist,
				 char ** msg ) {
  assert( res != NULL );

  bool ok = true;

  int priority = LCFG_RESOURCE_DEFAULT_PRIORITY;
  if ( lcfgresource_has_context(res) ) {

    /* Calculate the priority using the context expression for this
       resource. */

    ok = lcfgctxlist_eval_expression( ctxlist,
                                      lcfgresource_get_context(res),
                                      &priority, msg );

  }

  if (ok)
    ok = lcfgresource_set_priority( res, priority );

  return ok;
}

/**
 * @brief Check if the resource is considered to be active
 *
 * Checks if the current value for the @e priority attribute in the
 * @c LCFGResource is greater than or equal to zero.
 *
 * The priority is calculated using the value for the @e context
 * attribute and the list of currently active contexts, see
 * @c lcfgresource_eval_priority() for details.
 *
 * @param[in] res Pointer to an @c LCFGResource
 *
 * @return boolean indicating if the resource is active
 *
 */

bool lcfgresource_is_active( const LCFGResource * res ) {
  assert( res != NULL );

  return ( lcfgresource_get_priority(res) >= 0 );
}

/**
 * @brief Import a resource from the environment
 *
 * This checks the environment for variables which hold resource value
 * and type information for the given name and creates a new @c
 * LCFGResource. The variable names are a simple concatenation of the
 * resource name and any prefix specified.
 *
 * The value prefix will typically be like @c LCFG_comp_ and the type
 * prefix will typically be like @c LCFGTYPE_comp_ where @c comp is
 * the name of the component. If the type prefix is @c NULL then no
 * attempt will be made to load type information from the environment.
 *
 * If there is no variable for the resource value in the environment
 * an error will be returned unless the @c LCFG_OPT_ALLOW_NOEXIST
 * option is specified. When the option is specified and there is no
 * variable the value is assumed to be an empty string. Note that this
 * may not be a valid value for some resource types.
 *
 * To avoid memory leaks, when the newly created resource structure is
 * no longer required you should call the @c lcfgresource_relinquish()
 * function.
 *
 * @param[in] name The name of the resource
 * @param[in] compname The name of the component
 * @param[in] val_pfx The prefix for the value variable name
 * @param[in] type_pfx The prefix for the type variable name
 * @param[out] result Reference to the pointer for the @c LCFGResource
 * @param[in] options Integer which controls behaviour
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgresource_from_env( const char * name,
                                  const char * compname,
				  const char * val_pfx, const char * type_pfx,
				  LCFGResource ** result, 
                                  LCFGOption options, char ** msg ) {

  /* Declared before any cleanups */

  LCFGStatus status = LCFG_STATUS_OK;

  char * env_key      = NULL;
  size_t env_key_size = 0;
  ssize_t env_key_len = -1;

  LCFGResource * res = lcfgresource_new();
  char * resname = strdup(name);
  if ( !lcfgresource_set_name( res, resname ) ) {
    status = LCFG_STATUS_ERROR;
    *msg = lcfgresource_build_message( res, compname, "Invalid name '%s'",
                                       resname );
    free(resname);
    goto cleanup;
  }

  /* Type */

  env_key_len = lcfgresource_build_env_var( name, compname,
                                            LCFG_RESOURCE_ENV_TYPE_PFX,
                                            type_pfx,
                                            &env_key, &env_key_size );

  if ( env_key_len <= 0 ) {
    status = LCFG_STATUS_ERROR;
    *msg = lcfgresource_build_message( res, compname,
                "Failed to build environment variable name from '%s'",
                ( type_pfx != NULL ? type_pfx : LCFG_RESOURCE_ENV_TYPE_PFX ) );
    goto cleanup;
  }

  const char * type = getenv(env_key);

  if ( !isempty(type) ) {

    char * type_msg = NULL;
    if ( !lcfgresource_set_type_as_string( res, type, &type_msg ) ) {
      status = LCFG_STATUS_ERROR;
      *msg = lcfgresource_build_message( res, compname, "Invalid type '%s': %s",
                                         type, type_msg );
      free(type_msg);
      goto cleanup;
    }

    free(type_msg);
  }

  /* Value */

  env_key_len = lcfgresource_build_env_var( name, compname,
                                            LCFG_RESOURCE_ENV_VAL_PFX,
                                            val_pfx,
                                            &env_key, &env_key_size );

  if ( env_key_len <= 0 ) {
    status = LCFG_STATUS_ERROR;
    *msg = lcfgresource_build_message( res, compname,
                "Failed to build environment variable name from '%s'",
                ( val_pfx != NULL ? val_pfx : LCFG_RESOURCE_ENV_VAL_PFX ) );
    goto cleanup;
  }

  const char * value = getenv(env_key);

  if ( value == NULL ) {
    if ( options&LCFG_OPT_ALLOW_NOEXIST ) {
      value = ""; /* Assume it is an empty string */
    } else {
      status = LCFG_STATUS_ERROR;
      *msg = lcfgresource_build_message( res, compname, 
                                        "Could not find value in environment" );
    }
  }

  if ( status != LCFG_STATUS_ERROR ) {
    char * res_value = strdup(value);
    if ( !lcfgresource_set_value( res, res_value ) ) {
      status = LCFG_STATUS_ERROR;
      *msg = lcfgresource_build_message( res, NULL, "Invalid value '%s'",
                                         res_value );
      free(res_value);
      goto cleanup;
    }
  }

 cleanup:

  free(env_key);

  if ( status == LCFG_STATUS_ERROR ) {
    lcfgresource_destroy(res);
    res = NULL;
  }

  *result = res;

  return status;
}

/**
 * @brief Load resource from specification string
 *
 * This can be used to parse an LCFG resource specification string and
 * create a new @c LCFGResource using the information.
 *
 * As well as returning the new resource if the hostname or component
 * name are included in the key they will also be returned.
 *
 * All of these are valid resource specifications:
 *
\verbatim
host.client.ack=yes
client.ack=yes
ack=yes
\endverbatim
 *
 * To avoid memory leaks, when the newly created resource structure is
 * no longer required you should call the @c lcfgresource_relinquish()
 * function.
 *
 * @param[in] spec The resource specification string
 * @param[out] result Reference to pointer to new @c LCFGResource
 * @param[out] hostname Reference to pointer to host name string (may be @c NULL)
 * @param[out] compname Reference to pointer to component name string (may be @c NULL)
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgresource_from_spec( const char * spec, LCFGResource ** result,
				   char ** hostname, char ** compname,
				   char ** msg ) {

  /* The input string is mangled by the spec parser so we need to use a copy */

  char * input = strdup(spec);

  const char * spec_host = NULL;
  const char * spec_comp = NULL;
  const char * spec_res  = NULL;
  const char * spec_val  = NULL;
  char spec_type;

  LCFGStatus status = lcfgresource_parse_spec( input, &spec_host,
					       &spec_comp, &spec_res,
					       &spec_val, &spec_type,
					       msg );

  LCFGResource * res = NULL;

  /* Create new resource and set the name */

  if ( status != LCFG_STATUS_ERROR ) {
    res = lcfgresource_new();
    char * res_name = strdup(spec_res);
    if ( !lcfgresource_set_name( res, res_name ) ) {
      free(res_name);

      lcfgutils_build_message( msg, "invalid resource name '%s'", spec_res );
      status = LCFG_STATUS_ERROR;
    }

  }

  /* Set the value for the attribute type */

  if ( status != LCFG_STATUS_ERROR ) {
    char * set_msg = NULL;
    size_t val_len = strlen(spec_val);
    if ( !lcfgresource_set_attribute( res, spec_type,
				      spec_val, val_len, &set_msg ) ) {
      lcfgutils_build_message( msg, "bad value for resource attribute (%s)",
			       set_msg );
      status = LCFG_STATUS_ERROR;
    }
    free(set_msg);
  }

  /* Return the results */

  if ( status != LCFG_STATUS_ERROR ) {

    *hostname = isempty(spec_host) ? NULL : strdup(spec_host);
    *compname = isempty(spec_comp) ? NULL : strdup(spec_comp);
    *result   = res;

  } else {
    lcfgresource_relinquish(res);
    res = NULL;
  }

  /* Free after making copies of any results from the parse */

  free(input);

  return status;
}

/* Output */

/**
 * @brief Format the resource as a string
 *
 * Generates a new string representation of the @c LCFGResource in
 * various styles. The following styles are supported:
 *
 *   - @c LCFG_RESOURCE_STYLE_SUMMARY - uses @c lcfgresource_to_summary()
 *   - @c LCFG_RESOURCE_STYLE_STATUS - uses @c lcfgresource_to_status()
 *   - @c LCFG_RESOURCE_STYLE_SPEC - uses @c lcfgresource_to_spec()
 *   - @c LCFG_RESOURCE_STYLE_VALUE - uses @c lcfgresource_to_value()
 *
 * See the documentation for each function to see which options are
 * supported.
 *
 * These functions use a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many resource strings, this can be a
 * huge performance benefit. If the buffer is initially unallocated
 * then it MUST be set to @c NULL. The current size of the buffer must
 * be passed and should be specified as zero if the buffer is
 * initially unallocated. If the generated string would be too long
 * for the current buffer then it will be resized and the size
 * parameter is updated. 
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] prefix Prefix, usually the component name (may be @c NULL)
 * @param[in] style Integer indicating required style of formatting
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgresource_to_string( const LCFGResource * res,
				const char * prefix,
				LCFGResourceStyle style,
				LCFGOption options,
				char ** result, size_t * size ) {
  assert( res != NULL );

  /* Select the appropriate string function */

  LCFGResStrFunc str_func = NULL;

  switch (style)
    {
    case LCFG_RESOURCE_STYLE_VALUE:
      str_func = &lcfgresource_to_value;
      break;
    case LCFG_RESOURCE_STYLE_SUMMARY:
      str_func = &lcfgresource_to_summary;
      break;
    case LCFG_RESOURCE_STYLE_STATUS:
      str_func = &lcfgresource_to_status;
      break;
    case LCFG_RESOURCE_STYLE_SPEC:
    default:
      str_func = &lcfgresource_to_spec;
    }

  return (*str_func)( res, prefix, options, result, size );
}

ssize_t lcfgresource_build_env_var( const char * resname,
                                    const char * compname,
                                    const char * default_base,
                                    const char * base,
                                    char ** result, size_t * size ) {
  *result = NULL;

  if ( base == NULL ) base = default_base;

  char * base2 = NULL;
  if ( compname != NULL &&
       strstr( base, LCFG_RESOURCE_ENV_PHOLDER ) != NULL ) {
    base2 = lcfgutils_string_replace( base,
                                      LCFG_RESOURCE_ENV_PHOLDER,
                                      compname );
    base = base2;
  }

  size_t base_len = strlen(base);

  size_t new_len = base_len;

  /* Optional resource name */

  size_t res_len = 0;
  if ( resname != NULL ) {
    res_len = strlen(resname);
    new_len += res_len;
  }

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    size_t new_size = new_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG resource string");
      exit(EXIT_FAILURE);
    } else {
      *result = new_buf;
      *size   = new_size;
    }
  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  to = stpncpy( to, base, base_len );

  if ( res_len > 0 )
    to = stpncpy( to, resname, res_len );

  *to = '\0';

  free(base2);

  assert( ( *result + new_len ) == to );

  return new_len;
}

/**
 * @brief Check if a string is a valid environment variable name
 *
 * This can be used to validate an environment variable name to ensure
 * it is safe for use. The specification requires that the first
 * character is in the set [a-zA-Z_] and that any subsequent
 * characters are in the set [a-zA-Z0-9_] (i.e. it does not start with
 * a digit). We go further by requiring that where there are leading
 * underscores there must be an additional character in the set
 * [a-zA-Z0-9]. For example '__foo' is allowed but '___' is not.
 *
 * @param[in] name String to be checked
 *
 * @return Boolean indicating validity of variable name
 *
 */

bool lcfgresource_valid_env_var( const char * name ) {

  if ( isempty(name) ) return false;

  bool valid = true;

  bool need_extra_char  = false;
  bool found_extra_char = false;

  if ( !isalpha(*name) ) {

    if ( *name == '_' && *(name+1) != '\0' )
      need_extra_char = true;
    else
      valid = false;
  }

  const char * ptr;
  for ( ptr=name+1; valid && *ptr!='\0'; ptr++ ) {
    if ( isalnum(*ptr) )
      found_extra_char = true;
    else if ( *ptr != '_' )
      valid = false;
  }

  if ( need_extra_char && !found_extra_char )
    valid = false;

  return valid;
}

/**
 * @brief Export a resource to the environment
 *
 * This exports value and type information for the @c LCFGResource as
 * environment variables. The variable names are a combination of the
 * resource name and any prefix specified.
 *
 * The value prefix will typically be like @c LCFG_comp_ and the type
 * prefix will typically be like @c LCFGTYPE_comp_ where @c comp is
 * the name of the component. Often only the value variable is
 * required so, for efficiency, the type variable will only be set
 * when the @c LCFG_OPT_USE_META option is specified.
 *
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] compname Optional component name (may be @c NULL)
 * @param[in] val_pfx The prefix for the value variable name
 * @param[in] type_pfx The prefix for the type variable name
 * @param[in] options Integer which controls behaviour

 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgresource_to_env( const LCFGResource * res,
                                const char * compname,
                                const char * val_pfx, const char * type_pfx,
                                LCFGOption options ) {
  assert( res != NULL );

  /* Name is required */
  if ( !lcfgresource_is_valid(res) ) return LCFG_STATUS_ERROR;

  const char * name = res->name;

  /* Declared before any cleanups */

  LCFGStatus status = LCFG_STATUS_OK;

  char * env_key      = NULL;
  size_t env_key_size = 0;
  ssize_t env_key_len = -1;
  char * type_as_str = NULL;
  char * msg = NULL;

  /* Value */

  env_key_len = lcfgresource_build_env_var( name, compname,
                                            LCFG_RESOURCE_ENV_VAL_PFX,
                                            val_pfx,
                                            &env_key, &env_key_size );

  if ( env_key_len <= 0 ) {
    status = LCFG_STATUS_ERROR;
    msg = lcfgresource_build_message( res, compname,
                "Failed to build environment variable name from '%s'",
                ( val_pfx != NULL ? val_pfx : LCFG_RESOURCE_ENV_VAL_PFX ) );
    goto cleanup;
  } else if ( !lcfgresource_valid_env_var(env_key) ) {
    status = LCFG_STATUS_ERROR;
    msg = lcfgresource_build_message( res, compname,
                "Invalid environment variable name '%s'", env_key );
    goto cleanup;
  }

  const char * value = or_default( res->value, "" );

  if ( setenv( env_key, value, 1 ) != 0 ) {
    status = LCFG_STATUS_ERROR;
    goto cleanup;
  }

  /* Type - optional */

  if ( options&LCFG_OPT_USE_META ) {

    if ( lcfgresource_get_type(res) != LCFG_RESOURCE_TYPE_STRING ||
	 lcfgresource_has_comment(res) ) {

      size_t type_size = 0;
      ssize_t type_len =
        lcfgresource_get_type_as_string( res, LCFG_OPT_NONE,
                                         &type_as_str, &type_size );
      if ( type_len <= 0 ) {
        free(type_as_str);
        type_as_str = NULL;
      }
    }

    if ( !isempty(type_as_str) ) {

      env_key_len = lcfgresource_build_env_var( name, compname,
                                                LCFG_RESOURCE_ENV_TYPE_PFX,
                                                type_pfx,
                                                &env_key, &env_key_size );

      if ( env_key_len <= 0 ) {
        status = LCFG_STATUS_ERROR;
        msg = lcfgresource_build_message( res, compname,
                "Failed to build environment variable name from '%s'",
                ( type_pfx != NULL ? type_pfx : LCFG_RESOURCE_ENV_TYPE_PFX ) );
        goto cleanup;
      } else if ( !lcfgresource_valid_env_var(env_key) ) {
        status = LCFG_STATUS_ERROR;
        msg = lcfgresource_build_message( res, compname,
                "Invalid environment variable name '%s'", env_key );
        goto cleanup;
      }

      if ( setenv( env_key, type_as_str, 1 ) != 0 ) {
        status = LCFG_STATUS_ERROR;
        goto cleanup;
      }

    }

  }

 cleanup:

  free(type_as_str);
  free(env_key);

  if ( status == LCFG_STATUS_ERROR && msg != NULL ) {
    fprintf( stderr, "%s\n", msg );
  }
  free(msg);

  return status;
}

static const char env_fn_name[] = "export";
static const size_t env_fn_len = sizeof(env_fn_name) - 1;

/**
 * @brief Format resource information for shell evaluation
 *
 * This generates a new string representation of the @e value and
 * @e type information for the @c LCFGResource as environment variables
 * which can be evaluated in the bash shell. The variable names are a
 * combination of the resource name and any prefix specified.
 *
 The output will look something like:
 *
\verbatim
export LCFG_client_ack='yes'
export LCFGTYPE_client_ack='boolean'
\endverbatim
 *
 * The value prefix will typically be like @c LCFG_comp_ and the type
 * prefix will typically be like @c LCFGTYPE_comp_ where @c comp is
 * the name of the component. Often only the value variable is
 * required so, for efficiency, the type variable will only be set
 * when the @c LCFG_OPT_USE_META option is specified.
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many resource strings, this can be a
 * huge performance benefit. If the buffer is initially unallocated
 * then it MUST be set to @c NULL. The current size of the buffer must
 * be passed and should be specified as zero if the buffer is
 * initially unallocated. If the generated string would be too long
 * for the current buffer then it will be resized and the size
 * parameter is updated.
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] compname Optional component name (may be @c NULL)
 * @param[in] val_pfx The prefix for the value variable name
 * @param[in] type_pfx The prefix for the type variable name
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgresource_to_export( const LCFGResource * res,
                                const char * compname,
                                const char * val_pfx, const char * type_pfx,
                                LCFGOption options,
                                char ** result, size_t * size ) {
  assert( res != NULL );

  /* Name is required */

  if ( !lcfgresource_is_valid(res) ) return -1;

  const char * name = lcfgresource_get_name(res);

  /* Declare before jumps to cleanup */

  bool ok = true;
  char * msg = NULL;

  char * val_key  = NULL;
  char * type_key = NULL;
  char * type_as_str = NULL;

  /* Value */

  size_t val_key_size  = 0;
  ssize_t val_key_len = lcfgresource_build_env_var( name, compname,
                                                    LCFG_RESOURCE_ENV_VAL_PFX,
                                                    val_pfx,
                                                    &val_key, &val_key_size );

  if ( val_key_len <= 0 ) {
    ok = false;
    msg = lcfgresource_build_message( res, compname,
                   "Failed to build environment variable name from '%s'",
                   ( val_pfx != NULL ? val_pfx : LCFG_RESOURCE_ENV_VAL_PFX ) );
    goto cleanup;
  } else if ( !lcfgresource_valid_env_var(val_key) ) {
    ok = false;
    msg = lcfgresource_build_message( res, compname,
                   "Invalid environment variable name '%s'", val_key );
    goto cleanup;
  }

  static const char escaped[] = "'\"'\"'";
  static const size_t escaped_len = sizeof(escaped) - 1;

  const char * value = NULL;
  size_t value_len = 0;
  if ( lcfgresource_has_value(res) ) {
    value = lcfgresource_get_value(res);
    value_len = strlen(value);

    const char * ptr;
    for ( ptr = value; *ptr != '\0'; ptr++ ) {
      if ( *ptr == '\'' )
        value_len += ( escaped_len - 1 );
    }

  }

  /* +1 space, +1 =, +2 '', +1 '\n' == 5 */
  size_t new_len = ( env_fn_len + val_key_len + value_len + 5 );

  /* Type - optional */

  ssize_t type_len = 0;
  ssize_t type_key_len = 0;

  if ( options&LCFG_OPT_USE_META ) {

    if ( lcfgresource_get_type(res) != LCFG_RESOURCE_TYPE_STRING ||
         lcfgresource_has_comment(res) ) {
      size_t type_size = 0;
      type_len = lcfgresource_get_type_as_string( res, LCFG_OPT_NONE,
                                                  &type_as_str, &type_size );
      if ( type_len <= 0 ) {
        free(type_as_str);
        type_as_str = NULL;
      }
    }

    if ( !isempty(type_as_str) ) {

      size_t type_key_size = 0;
      type_key_len = lcfgresource_build_env_var( name, compname,
                                                 LCFG_RESOURCE_ENV_TYPE_PFX,
                                                 type_pfx,
                                                 &type_key, &type_key_size );

      if ( type_key_len <= 0 ) {
        ok = false;
        msg = lcfgresource_build_message( res, compname,
                "Failed to build environment variable name from '%s'",
                ( type_pfx != NULL ? type_pfx : LCFG_RESOURCE_ENV_TYPE_PFX ) );
        goto cleanup;
      } else if ( !lcfgresource_valid_env_var(type_key) ) {
        ok = false;
        msg = lcfgresource_build_message( res, compname,
                "Invalid environment variable name '%s'", type_key );
        goto cleanup;
      }

      if ( strcmp( val_key, type_key ) == 0 ) {
        ok = false;
        msg = lcfgresource_build_message( res, compname,
                                          "Must not have identical environment variables for value and type information" );
        goto cleanup;
      }

      const char * ptr;
      for ( ptr = type_as_str; *ptr != '\0'; ptr++ ) {
        if ( *ptr == '\'' )
          type_len += ( escaped_len - 1 );
      }

      /* +1 space, +1 =, +2 '', +1 '\n' == 5 */
      new_len += ( env_fn_len + type_key_len + type_len + 5 );
    }
  }

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    size_t new_size = new_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG resource string");
      exit(EXIT_FAILURE);
    } else {
      *result = new_buf;
      *size   = new_size;
    }
  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  /* Type - optional - emit this first so that when the exported stuff
     is evaluated any type information is loaded first */

  if ( type_len > 0 ) {

    to = stpncpy( to, env_fn_name, env_fn_len );

    *to = ' ';
    to++;

    to = stpncpy( to, type_key, type_key_len );

    to = stpncpy( to, "='", 2 );

    const char * ptr;
    for ( ptr = type_as_str; *ptr != '\0'; ptr++ ) {
      if ( *ptr == '\'' ) {
        to = stpncpy( to, escaped, escaped_len );
      } else {
        *to = *ptr;
        to++;
      }
    }

    to = stpncpy( to, "'\n", 2 );
  }

  /* Value */

  to = stpncpy( to, env_fn_name, env_fn_len );

  *to = ' ';
  to++;

  to = stpncpy( to, val_key, val_key_len );

  to = stpncpy( to, "='", 2 );

  if ( value != NULL ) {
    const char * ptr;
    for ( ptr = value; *ptr != '\0'; ptr++ ) {
      if ( *ptr == '\'' ) {
        to = stpncpy( to, escaped, escaped_len );
      } else {
        *to = *ptr;
        to++;
      }
    }
  }

  to = stpncpy( to, "'\n", 2 );

  *to = '\0';

  assert( ( *result + new_len ) == to );

 cleanup:

  if ( !ok && msg != NULL )
    fprintf( stderr, "%s\n", msg );

  free(type_as_str);
  free(val_key);
  free(type_key);
  free(msg);

  if (ok)
    return new_len;
  else
    return -1;

}

/**
 * @brief Summarise the resource information
 *
 * Summarises the @c LCFGResource as a string in the verbose key-value
 * style used by the qxprof tool. The output will look something like:
 *
\verbatim
client.ack:
   value=yes
    type=boolean
  derive=/var/lcfg/conf/server/defaults/client-4.def:47
\endverbatim
 *
 * The following options are supported:
 *   - @c LCFG_OPT_USE_META - Include any derivation and context information.
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many resource strings, this can be a
 * huge performance benefit. If the buffer is initially unallocated
 * then it MUST be set to @c NULL. The current size of the buffer must
 * be passed and should be specified as zero if the buffer is
 * initially unallocated. If the generated string would be too long
 * for the current buffer then it will be resized and the size
 * parameter is updated.
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] prefix Prefix, usually the component name (may be @c NULL)
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgresource_to_summary( LCFG_RES_TOSTR_ARGS ) {
  assert( res != NULL );

  static const char format[] = " %7s=%s\n";
  static const size_t base_len = 10; /* 1 for indent + 7 for key + 1 for '=' +1 for newline */

  ssize_t key_len = lcfgresource_to_spec( res, prefix,
                                          LCFG_OPT_NOVALUE|LCFG_OPT_NOCONTEXT,
                                          result, size );

  if ( key_len < 0 ) return key_len;

  size_t new_len = key_len + 2; /* for ':' (colon) and newline */

  /* Value */

  const char * value = or_default( res->value, "" );
  size_t value_len = strlen(value);

  new_len += ( base_len + value_len );

  /* Type */

  const char * type = "default";
  size_t type_len = 7;

  char * type_as_str = NULL;
  if ( lcfgresource_get_type(res) != LCFG_RESOURCE_TYPE_STRING ||
       lcfgresource_has_comment(res) ) {

    size_t type_size = 0;
    ssize_t type_str_len =
      lcfgresource_get_type_as_string( res, LCFG_OPT_NONE,
                                       &type_as_str, &type_size );

    if ( type_len > 0 ) {
      type     = type_as_str;
      type_len = (size_t) type_str_len;
    } else {
      free(type_as_str);
      type_as_str = NULL;
    }
  }

  new_len += ( base_len + type_len );

  /* Optional meta-data */

  char * deriv_as_str = NULL;
  ssize_t deriv_len = 0;

  const char * context = NULL;
  size_t ctx_len = 0;
  
  if ( options&LCFG_OPT_USE_META ) {

    if ( lcfgresource_has_derivation(res) ) {
      size_t deriv_size = 0;
      deriv_len = lcfgresource_get_derivation_as_string( res, LCFG_OPT_NONE,
                                                         &deriv_as_str,
                                                         &deriv_size );
      if ( deriv_len > 0 )
        new_len += ( base_len + deriv_len );
    }

    if ( lcfgresource_has_context(res) ) {
      context = lcfgresource_get_context(res);
      ctx_len = strlen(context);
      if ( ctx_len > 0 )
        new_len += ( base_len + ctx_len );
    }

  }

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    size_t new_size = new_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG resource string");
      exit(EXIT_FAILURE);
    } else {
      *result = new_buf;
      *size   = new_size;
    }
  }

  /* Build the new string - start at offset from the value line which
     was put there using lcfgresource_to_spec */

  char * to = *result + key_len;

  to = stpncpy( to, ":\n", 2 );

  int rc;

  /* Value */

  rc = sprintf( to, format, "value", value );
  to += rc;

  /* Type */

  rc = sprintf( to, format, "type", type );
  to += rc;

  /* Derivation */

  if ( deriv_len > 0 ) {
    rc = sprintf( to, format, "derive", deriv_as_str );
    to += rc;
  }

  /* Context */

  if ( ctx_len > 0 ) {
    rc = sprintf( to, format, "context", context );
    to += rc;
  }

  *to = '\0';

  free(type_as_str);
  free(deriv_as_str);

  assert( (*result + new_len ) == to );

  return new_len;
}

/**
 * @brief Format the resource as @e status
 *
 * Generates an LCFG @e status representation of the @c LCFGResource.
 * This is used by the LCFG components when the current state of the
 * resources is stored in a file. For safety, the resource value will
 * have any newline and ampersand characters HTML-encoded. The various
 * information is keyed using the standard prefix characters:
 *
 *   - type - @c '%'
 *   - derivation - @c '#'
 *   - value - no prefix
 *
 * The output will look something like:
 *
\verbatim
client.ack=yes
%client.ack=boolean
\endverbatim
 *
 * The following options are supported:
 *   - @c LCFG_OPT_USE_META - Include any derivation information.
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many resource strings, this can be a
 * huge performance benefit. If the buffer is initially unallocated
 * then it MUST be set to @c NULL. The current size of the buffer must
 * be passed and should be specified as zero if the buffer is
 * initially unallocated. If the generated string would be too long
 * for the current buffer then it will be resized and the size
 * parameter is updated.
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] prefix Prefix, usually the component name (may be @c NULL)
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgresource_to_status( LCFG_RES_TOSTR_ARGS ) {
  assert( res != NULL );

  /* The entry for the value is the standard stringified form. This
     writes directly into the result buffer, often this is all that
     the to_status function needs to do (if there is no type or
     derivation information). */

  ssize_t value_len = lcfgresource_to_spec( res, prefix,
                                            options|LCFG_OPT_NEWLINE|LCFG_OPT_ENCODE,
                                            result, size );

  if ( value_len < 0 ) return value_len;

  size_t new_len = value_len;

  /* Resource Meta Data */

  /* Type - Only output the type when the resource is NOT a string */

  char * type_as_str = NULL;
  ssize_t type_len = 0;
  if ( lcfgresource_get_type(res) != LCFG_RESOURCE_TYPE_STRING ||
       lcfgresource_has_comment(res) ) {

    size_t type_size = 0;
    type_len = lcfgresource_get_type_as_string( res, LCFG_OPT_NONE,
                                                &type_as_str, &type_size );

    if ( type_len > 0 ) {

      ssize_t key_len =
        lcfgresource_compute_key_length( res->name, prefix, NULL,
                                         LCFG_RESOURCE_SYMBOL_TYPE );

      new_len += ( key_len + type_len + 2 ); /* +2 for '=' and newline */

    } else {
      free(type_as_str);
      type_as_str = NULL;
    }
  }

  /* Derivation */

  size_t deriv_len = 0;
  if ( options&LCFG_OPT_USE_META && lcfgresource_has_derivation(res) ) {
    deriv_len = lcfgresource_get_derivation_length(res);
    if ( deriv_len > 0 ) {
      ssize_t key_len =
	lcfgresource_compute_key_length( res->name, prefix, NULL,
					 LCFG_RESOURCE_SYMBOL_DERIVATION );

      new_len += ( key_len + deriv_len + 2 ); /* +2 for '=' and newline */
    }
  }

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    size_t new_size = new_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG resource string");
      exit(EXIT_FAILURE);
    } else {
      *result = new_buf;
      *size   = new_size;
    }

  }

  /* Build the new string - start at offset from the value line which
     was put there using lcfgresource_to_spec */

  char * to = *result + value_len;

  /* Type */

  if ( type_as_str != NULL ) {

    ssize_t write_len =
      lcfgresource_insert_key( res->name, prefix, NULL,
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

  if ( deriv_len > 0 ) {

    ssize_t write_len =
      lcfgresource_insert_key( res->name, prefix, NULL,
                               LCFG_RESOURCE_SYMBOL_DERIVATION, to );

    if ( write_len > 0 ) {
      to += write_len;

      *to = '=';
      to++;

      deriv_len++; /* include nul-terminator */
      ssize_t inserted_len =
        lcfgresource_get_derivation_as_string( res, LCFG_OPT_NONE,
                                               &to, &deriv_len );

      if ( inserted_len > 0 )
        to += inserted_len;

      to = stpcpy( to, "\n" );
    }
  }

  *to = '\0';

  assert( (*result + new_len ) == to );

  return new_len;
}

/**
 * @brief Format the resource as an LCFG specification
 *
 * Generates a new string representation for the @c LCFGResource. This
 * is the standard @c key=value style used by tools such as qxprof.
 *
 * The following options are supported:
 *   - @c LCFG_OPT_NOCONTEXT - do not include any context information
 *   - @c LCFG_OPT_NOVALUE - do not include any value
 *   - @c LCFG_OPT_ENCODE - encode any newline characters in the value
 *   - @c LCFG_OPT_NEWLINE - append a final newline character
 *   - @c LCFG_OPT_NOPREFIX - do not include prefix (usually component name)
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many resource strings, this can be a
 * huge performance benefit. If the buffer is initially unallocated
 * then it MUST be set to @c NULL. The current size of the buffer must
 * be passed and should be specified as zero if the buffer is
 * initially unallocated. If the generated string would be too long
 * for the current buffer then it will be resized and the size
 * parameter is updated.
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] prefix Prefix, usually the component name (may be @c NULL)
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgresource_to_spec( LCFG_RES_TOSTR_ARGS ) {
  assert( res != NULL );

  if ( options&LCFG_OPT_NOPREFIX )
    prefix = NULL;

  ssize_t key_len =
    lcfgresource_compute_key_length( res->name, prefix, NULL, 
                                     LCFG_RESOURCE_SYMBOL_VALUE );

  if ( key_len < 0 ) return key_len;

  size_t new_len = key_len;

  /* Value */

  /* When the NOVALUE option is set then the string is assembled
     without a value and also without the '= separator. It will be
     just the resource name, possibly with a prefix, and possibly with
     a context.

     Normally if there is no value specified for the resource the '='
     separator is still added. */

  const char * value = NULL;
  char * value_enc = NULL;
  size_t value_len = 0;
  if ( !(options&LCFG_OPT_NOVALUE) ) {

    if ( options&LCFG_OPT_ENCODE && lcfgresource_value_needs_encode(res) ) {
      value_enc = lcfgresource_enc_value(res);
      value = value_enc;
    } else {
      value = lcfgresource_get_value(res);
    }

    value_len = value != NULL ? strlen(value) : 0;

    new_len += ( 1 + value_len ); /* +1 for '=' separator */
  }

  /* Context */

  const char * context = NULL;
  size_t context_len = 0;

  if ( !(options&LCFG_OPT_NOCONTEXT) && lcfgresource_has_context(res) ) {

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
    size_t new_size = new_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG resource string");
      exit(EXIT_FAILURE);
    } else {
      *result = new_buf;
      *size   = new_size;
    }
  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  ssize_t write_len = lcfgresource_insert_key( res->name, prefix, NULL,
                                               LCFG_RESOURCE_SYMBOL_VALUE, to );

  if ( write_len != key_len ) {
    free(value_enc);
    return -1;
  }

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

  }

  free(value_enc);

  /* Optional newline at the end of the string */

  if ( options&LCFG_OPT_NEWLINE )
    to = stpcpy( to, "\n" );

  assert( (*result + new_len ) == to );

  return new_len;
}

/**
 * @brief Format the resource value
 *
 * Generates a new string representation for value of the @c LCFGResource.
 *
 * The following options are supported:
 *   - @c LCFG_OPT_ENCODE - encode any newline characters in the value
 *   - @c LCFG_OPT_NEWLINE - append a final newline character
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many resource strings, this can be a
 * huge performance benefit. If the buffer is initially unallocated
 * then it MUST be set to @c NULL. The current size of the buffer must
 * be passed and should be specified as zero if the buffer is
 * initially unallocated. If the generated string would be too long
 * for the current buffer then it will be resized and the size
 * parameter is updated.
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] prefix Prefix, usually the component name (not used)
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgresource_to_value( LCFG_RES_TOSTR_ARGS ) {
  assert( res != NULL );

  const char * value = NULL;
  char * value_enc = NULL;

  if ( options&LCFG_OPT_ENCODE && lcfgresource_value_needs_encode(res) ) {
    value_enc = lcfgresource_enc_value(res);
    value = value_enc;
  } else {
    value = lcfgresource_get_value(res);
  }

  size_t value_len = value != NULL ? strlen(value) : 0;
  size_t new_len = value_len;

  /* Optional newline at end of string */

  if ( options&LCFG_OPT_NEWLINE )
    new_len += 1;

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    size_t new_size = new_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG resource string");
      exit(EXIT_FAILURE);
    } else {
      *result = new_buf;
      *size   = new_size;
    }
  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  if ( value_len > 0 )
    to = stpncpy( to, value, value_len );

  free(value_enc);

  /* Optional newline at the end of the string */

  if ( options&LCFG_OPT_NEWLINE )
    to = stpcpy( to, "\n" );

  assert( (*result + new_len ) == to );

  return new_len;
}

/**
 * @brief Write formatted resource to file stream
 *
 * This can be used to write out the resource in various formats to the
 * specified file stream which must have already been opened for
 * writing. The following styles are supported:
 * 
 *   - @c LCFG_RESOURCE_STYLE_SUMMARY - uses @c lcfgresource_to_summary()
 *   - @c LCFG_RESOURCE_STYLE_STATUS - uses @c lcfgresource_to_status()
 *   - @c LCFG_RESOURCE_STYLE_SPEC - uses @c lcfgresource_to_spec()
 *
 * See the documentation for each function to see which options are
 * supported.
 *
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] prefix Prefix, usually the component name (may be @c NULL)
 * @param[in] style Integer indicating required style of formatting
 * @param[in] options Integer for any additional options
 * @param[in] out Stream to which the resource string should be written
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_print( const LCFGResource * res,
                         const char * prefix,
                         LCFGResourceStyle style,
                         LCFGOption options,
                         FILE * out ) {
  assert( res != NULL );

  char * lcfgres = NULL;
  size_t buf_size = 0;

  options |= LCFG_OPT_NEWLINE;

  ssize_t rc = lcfgresource_to_string( res, prefix, style, options,
                                       &lcfgres, &buf_size );

  bool ok = ( rc > 0 );

  if ( ok ) {

    if ( fputs( lcfgres, out ) < 0 )
      ok = false;

  }

  free(lcfgres);

  return ok;
}

/**
 * @brief Compare the resource names
 *
 * This compares the names for two resources, this is mostly useful
 * for sorting lists of resources. An integer value is returned which
 * indicates lesser than, equal to or greater than in the same way as
 * @c strcmp(3).
 *
 * @param[in] res1 Pointer to @c LCFGResource
 * @param[in] res2 Pointer to @c LCFGResource
 * 
 * @return Integer (-1,0,+1) indicating lesser,equal,greater
 *
 */

int lcfgresource_compare_names( const LCFGResource * res1, 
                                const LCFGResource * res2 ) {
  assert( res1 != NULL );
  assert( res2 != NULL );

  const char * name1 = or_default( res1->name, "" );
  const char * name2 = or_default( res2->name, "" );

  return strcmp( name1, name2 );
}

/**
 * @brief Test if resources have same name
 *
 * Compares the @e name for the two resources using the 
 * @c lcfgresource_compare_names() function.
 *
 * @param[in] res1 Pointer to @c LCFGResource
 * @param[in] res2 Pointer to @c LCFGResource
 *
 * @return boolean indicating equality of names
 *
 */

bool lcfgresource_same_name( const LCFGResource * res1, 
                              const LCFGResource * res2 ) {
  assert( res1 != NULL );
  assert( res2 != NULL );

  return ( lcfgresource_compare_names( res1, res2 ) == 0 );
}

/**
 * @brief Compare the resource values
 *
 * This compares the values for two resources, this is mostly useful
 * for sorting lists of resources. An integer value is returned which
 * indicates lesser than, equal to or greater than in the same way as
 * @c strcmp(3).
 *
 * Comparison rules are:
 *   - True is greater than false if both are booleans
 *   - Integers are compared numerically
 *   - Fall back to simple @c strcmp(3)
 *
 * @param[in] res1 Pointer to @c LCFGResource
 * @param[in] res2 Pointer to @c LCFGResource
 * 
 * @return Integer (-1,0,+1) indicating lesser,equal,greater
 *
 */

int lcfgresource_compare_values( const LCFGResource * res1,
                                 const LCFGResource * res2 ) {
  assert( res1 != NULL );
  assert( res2 != NULL );

  const char * value1_str = or_default( res1->value, "" );
  const char * value2_str = or_default( res2->value, "" );

  int result = 0;

  if ( lcfgresource_is_boolean(res1) &&
       lcfgresource_same_type( res1, res2 ) ) {

    bool value1_istrue = ( strcmp( value1_str, "yes" ) == 0 );
    bool value2_istrue = ( strcmp( value2_str, "yes" ) == 0 );

    /* true is greater than false */

    if ( value1_istrue ) {
      if ( !value2_istrue )
        result = 1;
    } else {
      if ( value2_istrue )
        result = -1;
    }

  } else if ( lcfgresource_is_integer(res1) &&
              lcfgresource_same_type( res1, res2 ) ) {

    /* If value cannot be converted to integer will return zero */

    long int value1 = strtol( value1_str, NULL, 10 );
    long int value2 = strtol( value2_str, NULL, 10 );

    result = ( value1 == value2 ? 0 : ( value1 > value2 ? 1 : -1 ) );

  } else {
    result = strcmp( value1_str, value2_str );
  }

  return result;
}

/**
 * @brief Test if resource matches name
 *
 * This compares the @e name for the @c LCFGResource with the
 * specified string.
 *
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] name The name to check for a match
 *
 * @return boolean indicating equality of values
 *
 */

bool lcfgresource_match( const LCFGResource * res, const char * name ) {
  assert( res != NULL );
  assert( name != NULL );

  const char * res_name = or_default( res->name, "" );
  return ( strcmp( res_name, name ) == 0 );
}

/**
 * @brief Compare resources
 *
 * This compares two resources using the values for the @e name,
 * @e priority, @e value and @e context attributes (in that order). This
 * is mostly useful for sorting lists of resources. Priorities are
 * compared as integers, all other comparisons are done simply using
 * strcmp().
 *
 * @param[in] res1 Pointer to @c LCFGResource
 * @param[in] res2 Pointer to @c LCFGResource
 * 
 * @return Integer (-1,0,+1) indicating lesser,equal,greater
 *
 */

int lcfgresource_compare( const LCFGResource * res1,
                          const LCFGResource * res2 ) {
  assert( res1 != NULL );
  assert( res2 != NULL );

  /* Name */

  int result = lcfgresource_compare_names( res1, res2 );

  /* Priority */

  if ( result == 0 ) {
    int prio1 = res1->priority;
    int prio2 = res2->priority;
    result = prio1 == prio2 ? 0 : ( prio1 < prio2 ? -1 : 1 );
  }

  /* Value - explicitly doing a string comparison rather than type-based */

  if ( result == 0 ) {

    const char * value1 = or_default( res1->value, "" );
    const char * value2 = or_default( res2->value, "" );

    result = strcmp( value1, value2 );
  }

  /* Context */

  if ( result == 0 ) {

    const char * context1 = or_default( res1->value, "" );
    const char * context2 = or_default( res2->value, "" );

    result = strcmp( context1, context2 );
  }

  /* The template, type and derivation are NOT compared */

  return result;
}

/**
 * @brief Test if resources have same value
 *
 * Compares the @e value for the two resources using the 
 * @c lcfgresource_compare_values() function.
 *
 * @param[in] res1 Pointer to @c LCFGResource
 * @param[in] res2 Pointer to @c LCFGResource
 *
 * @return boolean indicating equality of values
 *
 */

bool lcfgresource_same_value( const LCFGResource * res1,
                              const LCFGResource * res2 ) {
  assert( res1 != NULL );
  assert( res2 != NULL );

  return ( lcfgresource_compare_values( res1, res2 ) == 0 );
}

/**
 * @brief Test if resources have same type
 *
 * Compares the @e type for the two resources. Note that this will
 * return true for two list resources which have the same type but
 * different sets of templates.
 *
 * @param[in] res1 Pointer to @c LCFGResource
 * @param[in] res2 Pointer to @c LCFGResource
 *
 * @return boolean indicating equality of types
 *
 */

bool lcfgresource_same_type( const LCFGResource * res1,
                             const LCFGResource * res2 ) {
  assert( res1 != NULL );
  assert( res2 != NULL );

  return ( lcfgresource_get_type(res1) == lcfgresource_get_type(res2) );
}

/**
 * @brief Test if resources have same context
 *
 * Compares the @e context for the two resources. Note that this only
 * does a string comparison, it does NOT evaluate the context
 * expressions thus two logically equivalent expressions with
 * different string representations will be considered to be
 * different.
 *
 * @param[in] res1 Pointer to @c LCFGResource
 * @param[in] res2 Pointer to @c LCFGResource
 *
 * @return boolean indicating equality of contexts
 *
 */

bool lcfgresource_same_context( const LCFGResource * res1,
                                const LCFGResource * res2 ) {

  return ( lcfgcontext_compare_expressions( res1->context,
                                            res2->context ) == 0 );
}

/**
 * @brief Test if resources are considered equal
 *
 * Compares the two resources using the @c lcfgresource_compare()
 * function.
 *
 * @param[in] res1 Pointer to @c LCFGResource
 * @param[in] res2 Pointer to @c LCFGResource
 *
 * @return boolean indicating equality of resources
 *
 */

bool lcfgresource_equals( const LCFGResource * res1,
                          const LCFGResource * res2 ) {
  assert( res1 != NULL );
  assert( res2 != NULL );

  return ( lcfgresource_compare( res1, res2 ) == 0 );
}

/**
 * @brief Assemble a resource-specific message
 *
 * This can be used to assemble resource-specific message, typically
 * this is used for generating diagnostic error messages.
 *
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] component Component name string (or @c NULL)
 * @param[in] fmt Format string for message (and any additional arguments)
 *
 * @return Pointer to message string (call @c free(3) when no longer required)
 *
 */

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
      size_t type_size = 0;
      ssize_t type_len = 
        lcfgresource_get_type_as_string( res, LCFG_OPT_NOTEMPLATES,
                                         &type_as_str, &type_size );

      if ( type_len <= 0 ) {
        free(type_as_str);
        type_as_str = NULL;
      }
    }

    if ( lcfgresource_has_name(res) ) {
      size_t buf_size = 0;
      ssize_t str_rc = lcfgresource_to_spec( res, component,
					     LCFG_OPT_NOVALUE,
					     &res_as_str, &buf_size );

      if ( str_rc < 0 ) {
        perror("Failed to build LCFG resource message");
        exit(EXIT_FAILURE);
      }

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

  char * deriv_as_str = NULL;
  ssize_t deriv_len = 0;
  if ( res != NULL && lcfgresource_has_derivation(res) ) {
    size_t deriv_size = 0;
    deriv_len = lcfgresource_get_derivation_as_string( res, LCFG_OPT_NONE,
                                                       &deriv_as_str,
                                                       &deriv_size );
  }

  if ( deriv_len > 0 ) {
    rc = asprintf( &result, "%s %s at %s",
                   msg_base, msg_mid,
                   deriv_as_str );
  } else {
    rc = asprintf( &result, "%s %s",
                   msg_base, msg_mid );
  }

  free(deriv_as_str);

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

/**
 * @brief Calculate the required size for a resource key
 *
 * This calculates the size requirements for a resource key including
 * the optional component and namespace.
 *
 * The key is generated by combining the part using a @c '.' (period)
 * character so will be something like @c namespace.component.resname
 * with an optional single-character type symbol prefix (e.g. @c '%',
 * @c '#' or @c '^'). This is used by the @c lcfgresource_build_key()
 * function.
 *
 * Clearly care must be taken to ensure no strings are altered after
 * the key size has been calculated prior to allocating the space for
 * the key.
 *
 * @param[in] resource Name of resource (required)
 * @param[in] component Name of component (optional)
 * @param[in] namespace Namespace, typically a hostname (optional)
 * @param[in] type_symbol The symbol for the particular key type
 *
 * @return Size of required key
 *
 */
 
ssize_t lcfgresource_compute_key_length( const char * resource,
                                         const char * component,
                                         const char * namespace,
                                         char type_symbol ) {

  if ( isempty(resource) ) return -1;

  size_t length = 0;

  if ( type_symbol != '\0' )
    length++;

  if ( !isempty(namespace) )
    length += ( strlen(namespace) + 1 ); /* +1 for '.' (period) separator */

  if ( !isempty(component) )
    length += ( strlen(component) + 1 ); /* +1 for '.' (period) separator */

  length += strlen(resource);

  return length;
}

/**
 * @brief Insert resource key into a string
 *
 * This inserts the resource key into a pre-allocated string which
 * must be sufficiently large (use the 
 * @c lcfgresource_compute_key_length() function). 
 *
 * The key is generated by combining the part using a @c '.' (period)
 * character so will be something like @c namespace.component.resname
 * with an optional single-character type symbol prefix (e.g. @c '%',
 * @c '#' or @c '^'). This is used by the @c lcfgresource_build_key()
 * function.
 *
 * If an error occurs this function will return -1.
 *
 * @param[in] resource Name of resource (required)
 * @param[in] component Name of component (optional)
 * @param[in] namespace Namespace, typically a hostname (optional)
 * @param[in] type_symbol The symbol for the particular key type
 * @param[out] result The string into which the key should be written
 *
 * @return Size of required key (or -1 for error)
 *
 */

ssize_t lcfgresource_insert_key( const char * resource,
                                 const char * component,
                                 const char * namespace,
                                 char type_symbol,
                                 char * result ) {

  if ( isempty(resource) ) return -1;

  char * to = result;

  if ( type_symbol != '\0' ) {
    *to = type_symbol;
    to++;
  }

  if ( !isempty(namespace) ) {
    to = stpcpy( to, namespace );

    *to = '.';
    to++;
  }

  if ( !isempty(component) ) {
    to = stpcpy( to, component );

    *to = '.';
    to++;
  }

  to = stpcpy( to, resource );

  *to = '\0';

  return ( to - result );
}

/**
 * @brief Build a resource key
 *
 * Generates a new key for the @c LCFGResource. This key is used by
 * the client to generate unique keys for storing current resource
 * data into the DB.
 *
 * The key is generated by combining the part using a @c '.' (period)
 * character so will be something like @c namespace.component.resname
 * with an optional single-character type symbol prefix (e.g. @c '%',
 * @c '#' or @c '^'). 
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many resource strings, this can be a
 * huge performance benefit. If the buffer is initially unallocated
 * then it MUST be set to @c NULL. The current size of the buffer must
 * be passed and should be specified as zero if the buffer is
 * initially unallocated. If the generated string would be too long
 * for the current buffer then it will be resized and the size
 * parameter is updated. 
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] resource Name of resource (required)
 * @param[in] component Name of component (optional)
 * @param[in] namespace Namespace, typically a hostname (optional)
 * @param[in] type_symbol The symbol for the particular key type
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return Size of required key (or -1 for an error)
 *
 */

ssize_t lcfgresource_build_key( const char * resource,
                                const char * component,
                                const char * namespace,
                                char type_symbol,
                                char ** result,
                                size_t * size ) {

  if ( isempty(resource) ) return -1;

  ssize_t need_len = lcfgresource_compute_key_length( resource,
                                                      component,
                                                      namespace,
                                                      type_symbol );

  if ( need_len < 0 ) return need_len;

  /* Allocate the required space */

  if ( *result == NULL || *size < ( (size_t) need_len + 1 ) ) {
    size_t new_size = need_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG resource key");
      exit(EXIT_FAILURE);
    } else {
      *result = new_buf;
      *size   = new_size;
    }

  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  ssize_t write_len = lcfgresource_insert_key( resource,
                                               component,
                                               namespace,
                                               type_symbol,
                                               *result );

  return ( write_len == need_len ? write_len : -1 );
}

/**
 * @brief Parse a resource specification
 *
 * This parses the given resource specification string into the
 * constituent (hostname, component name, resource name and value)
 * parts. Note that this function modifies the given string in-place
 * and returns pointers to the various chunks of interest.
 *
 * @param[in] spec Pointer to the resource spec (will be modified in place)
 * @param[out] hostname Reference to a pointer to the hostname part of the key (optional)
 * @param[out] compname Reference to a pointer to the component name part of the key (optional)
 * @param[out] resname Reference to a pointer to the resource name
 * @param[out] value Reference to a pointer to the resource value (might be empty)
 * @param[out] type The key type symbol
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgresource_parse_spec( char * spec,
                                    const char ** hostname,
                                    const char ** compname,
                                    const char ** resname,
                                    const char ** value,
                                    char  * type,
                                    char ** msg ) {
  assert( spec != NULL );

  LCFGStatus status = LCFG_STATUS_OK;

  while ( *spec != '\0' && isspace(*spec) ) spec++;

  if ( isempty(spec) ) {
    lcfgutils_build_message( msg, "empty resource specification" );
    status = LCFG_STATUS_ERROR;
    goto cleanup;
  }

  /* Search for the '=' which separates status keys and values */

  char * sep = strchr( spec, '=' );
  if ( sep == NULL ) {
    lcfgutils_build_message( msg, "missing '=' character" );
    status = LCFG_STATUS_ERROR;
    goto cleanup;
  }

  /* Replace the '=' separator with a NULL so that can avoid
     unnecessarily dupping the string */

  *sep = '\0';

  /* The value is everything after the separator (could just be an
     empty string) */

  *value = sep + 1;

  if ( !lcfgresource_parse_key( spec, 
                                hostname, compname, 
                                resname, type ) ) {
    lcfgutils_build_message( msg, "invalid resource key '%s'", spec );
    status = LCFG_STATUS_ERROR;
    goto cleanup;
  }

  /* Validation */

  if ( !lcfgresource_valid_name(*resname) ) {
    lcfgutils_build_message( msg, "invalid resource name '%s'", *resname );
    status = LCFG_STATUS_ERROR;
    goto cleanup;
  }

 cleanup:

  return status;
}

/**
 * @brief Parse a resource key
 *
 * This parses the given resource key into the constituent (hostname,
 * component name and resource name) parts. Note that this function
 * modifies the given string in-place and returns pointers to the
 * various chunks of interest.
 *
 * @param[in] key Pointer to the resource key (will be modified in place)
 * @param[out] hostname Reference to a pointer to the hostname part of the key (optional)
 * @param[out] compname Reference to a pointer to the component name part of the key (optional)
 * @param[out] resname Reference to a pointer to the resource name
 * @param[out] type The key type symbol
 *
 * @return boolean indicating success
 *
 */
 
bool lcfgresource_parse_key( char  * key,
                             const char ** hostname,
                             const char ** compname,
                             const char ** resname,
                             char  * type ) {

  *hostname = NULL;
  *compname = NULL;
  *resname  = NULL;
  *type     = LCFG_RESOURCE_SYMBOL_VALUE;

  if ( isempty(key) ) return false;

  const char * start = key;

  /* Ignore any leading whitespace */
  while ( *start != '\0' && isspace(*start) ) start++;

  if ( *start == '\0' ) return false;

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


/**
 * @brief Set a value for the required attribute
 *
 * This sets a value for the relevant resource attribute for a given
 * type symbol:
 *
 *   - type - @c '%'
 *   - context - @c '='
 *   - derivation - @c '#'
 *   - priority - @c '^'
 *   - value - nul
 *
 * This will take a copy of the new attribute value string where
 * necessary.
 *
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] type_symbol The symbol for the required attribute type
 * @param[in] value The new value for the attribute
 * @param[in] value_len The length of the new value string for the attribute
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return boolean indicating success
 *
 */

bool lcfgresource_set_attribute( LCFGResource * res,
                                 char type_symbol,
                                 const char * value,
				 size_t value_len,
                                 char ** msg ) {
  assert( res != NULL );

  free(*msg);
  *msg = NULL;

  bool ok = false;

  /* Apply the action which matches with the symbol at the start of
     the status line or assume this is a simple specification of the
     resource value. */

  /* Always take a copy to ensure the value string is null-terminated */

  char * value_copy = NULL;
  if ( value_len > 0 )
    value_copy = strndup( value, value_len );

  bool free_copy = true; /* Will value_copy need freeing at end? */

  const char * attr_name = NULL; /* used for error messages */

  switch (type_symbol)
    {
    case LCFG_RESOURCE_SYMBOL_DERIVATION:
      ;
      attr_name = "derivation";

      if ( value_len > 0 )
        ok = lcfgresource_set_derivation_as_string( res, value_copy );
      else
        ok = lcfgresource_set_derivation_as_string( res, NULL ); /* unset */

      break;
    case LCFG_RESOURCE_SYMBOL_TYPE:
      ;
      attr_name = "type";

      if ( value_len > 0 )
        ok = lcfgresource_set_type_as_string( res, value_copy, msg );
      else
        ok = lcfgresource_set_type_default(res);

      break;
    case LCFG_RESOURCE_SYMBOL_CONTEXT:
      ;
      attr_name = "context";

      if ( value_len > 0 ) {
        ok = lcfgresource_set_context( res, value_copy );
        if (ok) free_copy = false;
      } else {
        ok = lcfgresource_set_context( res, NULL ); /* unset */
      }

      break;
    case LCFG_RESOURCE_SYMBOL_PRIORITY:
      ;
      attr_name = "priority";

      if ( value_len > 0 ) {
        /* Be careful to only convert string to int if it looks safe */
        ok = lcfgresource_valid_integer(value_copy);

        if (ok) {
          int priority = atoi(value_copy);
          ok = lcfgresource_set_priority( res, priority );
        }
      } else {
        ok = lcfgresource_set_priority_default(res);
      }

      break;
    case  LCFG_RESOURCE_SYMBOL_VALUE:
    default:        /* value line */
      ;
      attr_name = "value";

      if ( value_len > 0 ) {
        /* Value strings may be html encoded as they can contain
           whitespace characters which would otherwise corrupt the status
           file formatting. */

        lcfgutils_decode_html_entities_utf8( value_copy, NULL );

        ok = lcfgresource_set_value( res, value_copy );
      } else {
        value_copy = strdup("");
        ok = lcfgresource_set_value( res, value_copy );
      }

      if (ok) free_copy = false;

      break;
    }

  if ( !ok && *msg == NULL ) {
    lcfgutils_build_message( msg, "Invalid %s '%s'", attr_name,
			     ( value_len > 0 ? value_copy : "(empty string)" ));
  }

  if (free_copy)
    free(value_copy);

  return ok;
}

/**
 * @brief Calculate the hash for a resource
 *
 * This will calculate the hash for the resource using the value for
 * the @e name parameter. It does this using the @c
 * lcfgutils_string_djbhash() function.
 *
 * @param[in] res Pointer to @c LCFGResource
 *
 * @return The hash for the resource name
 *
 */

unsigned long lcfgresource_hash( const LCFGResource * res ) {
  return lcfgutils_string_djbhash( res->name, NULL );
}

/**
 * @brief Get a list of all child resources for a specific tag
 *
 * This generates an @c LCFGTagList of all possible child resource
 * names for an @c LCFGResource. Note that no test is done to see if
 * the resources actually exist. The tags passed in are applied to
 * each template for the resource to generate the names. It thus only
 * makes sense to call this on a @e list resource which has a set of
 * templates.
 *
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] tags  Pointer to @c LCFGTagList of tags
 * @param[out] result Reference to pointer to @c LCFGTagList of child names
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success
 *
 */

LCFGStatus lcfgresource_child_names( const LCFGResource * res,
				     LCFGTagList * tags,
				     LCFGTagList ** result,
				     char ** msg ) {
  assert( res != NULL );

  size_t size = 64;
  char * child_name = calloc( size, sizeof(char) );
  if ( child_name == NULL ) {
    perror("Failed to allocate memory for LCFG resource name");
    exit(EXIT_FAILURE);
  }

  LCFGTagList * child_list = lcfgtaglist_new();

  LCFGStatus status = LCFG_STATUS_OK;
  const LCFGTemplate * cur_tmpl = NULL;
  for ( cur_tmpl = lcfgresource_get_template(res);
        cur_tmpl != NULL && status != LCFG_STATUS_ERROR;
        cur_tmpl = cur_tmpl->next ) {

    ssize_t len = lcfgtemplate_substitute( cur_tmpl, tags,
					   &child_name, &size, msg );

    if ( len > 0 ) {
      LCFGChange rc = lcfgtaglist_mutate_append( child_list, child_name, msg );
      if ( rc == LCFG_CHANGE_ERROR )
	status = LCFG_STATUS_ERROR;
    } else {
      status = LCFG_STATUS_ERROR;
    }
  }

  free(child_name);

  if ( status == LCFG_STATUS_ERROR ) {
    lcfgtaglist_relinquish(child_list);
    child_list = NULL;
  }
  
  *result = child_list;

  return status;
}

/* eof */
