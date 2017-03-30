#define _GNU_SOURCE /* for asprintf */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#include "context.h"
#include "utils.h"

/* Test if a string (i.e. a char *) is 'empty' */

#define isempty(STR) ( STR == NULL || *(STR) == '\0' )

/* Extends the standard alpha-numeric test to include the '_'
   (underscore) character. This is similar to the '\w' in Perl
   regexps, this gives the set of characters [A-Za-z0-9_] */

#define isword(CHR) ( isalnum(CHR) || CHR == '_' )

/* Simple internal wrapper function which can be used to generate a
   consistently formatted error message. Returns the error status
   value so that it can be called alongside return */

static LCFGStatus invalid_context( char ** msg, const char * reason ) {
  lcfgutils_build_message( msg, "Invalid context (%s)", reason );
  return LCFG_STATUS_ERROR;
}

/**
 * @brief Create and initialise a new context
 *
 * Creates a new @c LCFGContext and initialises the parameters to the
 * default values. The default priority is 1 (one).
 *
 * The reference count for the new struct is initialised to 1. To
 * avoid memory leaks, when you no longer require access to the struct
 * you should call @c lcfgcontext_release().
 *
 * If the memory allocation for the new struct is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * @return Pointer to new @c LCFGContext struct
 *
 */

LCFGContext * lcfgcontext_new(void) {

  LCFGContext * ctx = malloc( sizeof(LCFGContext) );
  if ( ctx == NULL ) {
    perror( "Failed to allocate memory for LCFG context" );
    exit(EXIT_FAILURE);
  }

  /* Default values */

  ctx->name      = NULL;
  ctx->value     = NULL;
  ctx->priority  = 1;
  ctx->_refcount = 1;

  return ctx;
}

/**
 * @brief Destroy the context
 *
 * When the specified @c LCFGContext is no longer required this can be
 * used to free all associated memory. It is important to note that
 * this function ignores the reference count and thus is almost
 * certainly @b NOT the function you need to call, you will nearly
 * always want @c lcfgcontext_release().
 *
 * This will call @c free(3) on each parameter of the struct and then
 * set each value to be @c NULL.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * context which has already been destroyed (or potentially was never
 * created).
 *
 * @param[in] ctx Pointer to @c LCFGContext to be destroyed.
 *
 */

void lcfgcontext_destroy( LCFGContext * ctx ) {

  if ( ctx == NULL ) return;

  free(ctx->name);
  ctx->name = NULL;

  free(ctx->value);
  ctx->value = NULL;

  free(ctx);
  ctx = NULL;

}

/**
 * @brief Acquire reference to context
 *
 * This is used to record a reference to the @c LCFGContext, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the struct is no
 * longer required the @c lcfgcontext_release() function should be
 * called.
 *
 * @param[in] ctx Pointer to @c LCFGContext
 *
 */

void lcfgcontext_acquire( LCFGContext * ctx ) {
  assert( ctx != NULL );

  ctx->_refcount += 1;
}

/**
 * @brief Release reference to context
 *
 * This is used to release a reference to the @c LCFGContext,
 * it does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgcontext_destroy() function
 * will be called to clean up the memory associated with the struct.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * context which has already been destroyed (or potentially was never
 * created).
 *
 * @param[in] ctx Pointer to @c LCFGContext
 *
 */

void lcfgcontext_release( LCFGContext * ctx ) {

  if ( ctx == NULL ) return;

  if ( ctx->_refcount > 0 )
    ctx->_refcount -= 1;

  if ( ctx->_refcount == 0 )
    lcfgcontext_destroy(ctx);

}

/**
 * @brief Check if a string is a valid LCFG context name
 *
 * Checks the contents of a specified string against the specification
 * for an LCFG context name.
 *
 * An LCFG context name MUST be at least one character in length. The
 * first character MUST be in the class @c [A-Za-z] and all other
 * characters MUST be in the class @c [A-Za-z0-9_].
 *
 * @param[in] name String to be tested
 *
 * @return boolean which indicates if string is a valid context name
 *
 */

bool lcfgcontext_valid_name( const char * name ) {

  /* MUST NOT be a NULL.
     MUST have non-zero length.
     First character MUST be in [A-Za-z] set. */

  bool valid = ( !isempty(name) && isalpha(*name) );

  /* All other characters MUST be in [A-Za-z0-9_] set */

  char * ptr;
  for ( ptr = ((char *)name) + 1; valid && *ptr != '\0'; ptr++ )
    if ( !isword(*ptr) ) valid = false;

  return valid;
}

/**
 * @brief Check if the context has a name
 *
 * Checks if the specified @c LCFGContext currently has a value set
 * for the @e name attribute.
 *
 * Although a name is required for an LCFG context to be considered
 * valid it is possible for the value of the name to be set to @c NULL
 * when the struct is first created.
 *
 * @param[in] ctx Pointer to an @c LCFGContext struct
 *
 * @return boolean which indicates if a context has a name
 *
 */

bool lcfgcontext_has_name( const LCFGContext * ctx ) {
  assert( ctx != NULL );

  return !isempty(ctx->name);
}

/**
 * @brief Get the name for the context
 *
 * This returns the value of the @e name parameter for the @c
 * LCFGContext struct. If the context does not currently have a
 * @e name then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e name for the
 * context.
 *
 * @param[in] ctx Pointer to an @c LCFGContext struct
 *
 * @return The name for the context (possibly NULL).
 */

char * lcfgcontext_get_name( const LCFGContext * ctx ) {
  assert( ctx != NULL );

  return ctx->name;
}

/**
 * @brief Set the name for the context
 *
 * Sets the value of the @e name parameter for the @c LCFGContext
 * to that specified. It is important to note that this does
 * NOT take a copy of the string. Furthermore, once the value is set
 * the context assumes "ownership", the memory will be freed if the
 * name is further modified or the context struct is destroyed.
 *
 * Before changing the value of the @e name to be the new string it
 * will be validated using the @c lcfgcontext_valid_name()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] ctx Pointer to an @c LCFGContext struct
 * @param[in] new_name String which is the new name
 *
 * @return boolean indicating success
 *
 */

bool lcfgcontext_set_name( LCFGContext * ctx, char * new_name ) {
  assert( ctx != NULL );

  bool ok = false;
  if ( lcfgcontext_valid_name(new_name) ) {
    free(ctx->name);

    ctx->name = new_name;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/**
 * @brief Check if a string is a valid LCFG context value
 *
 * Checks the contents of a specified string against the specification
 * for an LCFG context value.
 *
 * An LCFG context value MUST NOT be a @c NULL, anything else
 * including the empty string is permitted.
 *
 * @param[in] value String to be tested
 *
 * @return boolean which indicates if string is a valid context value
 *
 */

bool lcfgcontext_valid_value( const char * value ) {

  /* Note that the empty string is intentionally valid */

  bool valid = ( value != NULL );

  return valid;
}

/**
 * @brief Check if the context has a value
 *
 * Checks if the specified @c LCFGContext currently has a
 * value set for the @e value attribute.
 *
 * @param[in] ctx Pointer to an @c LCFGContext struct
 *
 * @return boolean which indicates if a context has a value
 *
 */

bool lcfgcontext_has_value( const LCFGContext * ctx ) {
  assert( ctx != NULL );

  return !isempty(ctx->value);
}

/**
 * @brief Get the value for the context
 *
 * This returns the value of the @e value parameter for the
 * @c LCFGContext struct. If the context does not currently have a
 * @e value then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e value for the
 * context.
 *
 * @param[in] ctx Pointer to an @c LCFGContext struct
 *
 * @return The value for the context (possibly NULL).
 */

char * lcfgcontext_get_value( const LCFGContext * ctx ) {
  assert( ctx != NULL );

  return ctx->value;
}

/**
 * @brief Set the value for the context
 *
 * Sets the value of the @e value parameter for the @c LCFGContext to
 * that specified. It is important to note that this does NOT take a
 * copy of the string. Furthermore, once the value is set the context
 * assumes "ownership", the memory will be freed if the value is
 * further modified or the context struct is destroyed.
 *
 * Before changing the value of the @e value to be the new string it
 * will be validated using the @c lcfgcontext_valid_value()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] ctx Pointer to an @c LCFGContext struct
 * @param[in] new_value String which is the new value
 *
 * @return boolean indicating success
 *
 */

bool lcfgcontext_set_value( LCFGContext * ctx, char * new_value ) {
  assert( ctx != NULL );

  bool ok=false;
  if ( lcfgcontext_valid_value(new_value) ) {
    free(ctx->value);

    ctx->value = new_value;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/**
 * @brief Remove the value for the context
 *
 * Sets the value of the @e value parameter for the @c LCFGContext to
 * @c NULL. If there was previously a value string the associated
 * memory will be freed.
 *
 * @param[in] ctx Pointer to an @c LCFGContext struct
 *
 * @return boolean indicating success
 *
 */

bool lcfgcontext_unset_value( LCFGContext * ctx ) {
  assert( ctx != NULL );

  free(ctx->value);
  ctx->value = NULL;

  return true;
}

static char * valid_false_values[] = {
  "false", "no", "off", "0", "",
  NULL
};

/**
 * @brief Check if the value for a context is @e false
 *
 * This can be used to check if the @e value for the context is
 * considered to be @e false. A @e value is considered to be @e false
 * if any of the following criteria are satisfied:
 *
 *   - The value of the LCFGContext pointer is @c NULL.
 *   - The context value is @c NULL.
 *   - The context value is an empty string.
 *   - The context value is any of false/no/off/0 (case-insensitive).
 *
 * @param[in] ctx Pointer to an @c LCFGContext struct
 *
 * @return boolean indicating whether the value is @e false
 * 
 */

bool lcfgcontext_is_false( const LCFGContext * ctx ) {

  if ( ctx == NULL || isempty(ctx->value) ) return true;

  bool is_false = false;

  char ** val_ptr;
  for ( val_ptr = valid_false_values; *val_ptr != NULL; val_ptr++ ) {
    if ( strcasecmp( ctx->value, *val_ptr ) == 0 ) {
      is_false = true;
      break;
    }
  }

  return is_false;
}

/**
 * @brief Check if the value for a context is @e true
 *
 * This can be used to check if the @e value for the context is
 * considered to be @e true. A @e value is considered to be @e true if
 * none of the criteria used by the @c lcfgcontext_is_false() function
 * are satisfied.
 *
 * @param[in] ctx Pointer to an @c LCFGContext struct
 *
 * @return boolean indicating whether the value is @e true
 * 
 */

bool lcfgcontext_is_true( const LCFGContext * ctx ) {
  return !lcfgcontext_is_false(ctx);
}

/**
 * @brief Get the priority for the context
 *
 * This returns the value of the integer @e priority attribute for the
 * @c LCFGContext struct. This is expected to be a positive integer
 * value.
 *
 * The priority is used to resolve conflicts when an LCFG resource has
 * multiple possible values associated with currently active contexts.
 *
 * @param[in] ctx Pointer to an @c LCFGContext struct
 *
 * @return The priority for the context.
 */

int lcfgcontext_get_priority( const LCFGContext * ctx ) {
  assert( ctx != NULL );

  return ctx->priority;
}

/**
 * @brief Set the priority for the context
 *
 * Sets the value of the @e priority attribute for the @c LCFGContext
 * to that specified. Note that although this is an integer
 * attribute only positive values are permitted. If the new value is
 * not valid then no change will occur, the @c errno will be set to
 * @c EINVAL and the function will return a @c false value.
 *
 * @param[in] ctx Pointer to an @c LCFGContext struct
 * @param[in] priority Integer which is the new context priority
 *
 * @return boolean indicating success
 *
 */

bool lcfgcontext_set_priority( LCFGContext * ctx, int priority ) {
  assert( ctx != NULL );

  /* The priority associated with a context is always positive. When a
     context query expression is evaluated the result might be
     negative, in that case the sign indicates truthiness. */

  bool ok=false;
  if ( priority >= 1 ) {
    ctx->priority = priority;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/**
 * @brief Create a new context from a string
 *
 * This parses an LCFG context specification as a string in the form
 * "NAME = VALUE" and creates a new @c LCFGContext struct. The context
 * name and @c = (equals) are required, the value is optional. Any
 * whitespace will be ignored.
 *
 * To avoid memory leaks, when the newly created context struct is no
 * longer required you should call the @c lcfgcontext_release()
 * function.
 *
 * @param[in] input The context specification string.
 * @param[in] priority The integer priority to be set for the new context.
 * @param[out] result Reference to the pointer for the @c LCFGContext struct.
 * @param[out] msg Pointer to any diagnostic messages.

 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcontext_from_string( const char * input, int priority,
                                    LCFGContext ** result,
                                    char ** msg ) {

  *result = NULL;

  if ( input == NULL )
    return invalid_context( msg, "undefined value" );

  char * ctx_str = (char *) input;

  /* skip past any leading whitespace */
  while ( isspace(*ctx_str) ) ctx_str++;

  if ( *ctx_str == '\0' )
    return invalid_context( msg, "empty string" );

  /* Find the '=' character which separates the context name and value */

  char * location = strchr( ctx_str, '=' );
  if ( location == NULL )
    return invalid_context( msg, "missing '=' assignment character" );

  /* Ignore any whitespace after the name (before the '=') */

  size_t name_len = location - ctx_str;
  while ( name_len > 0 && isspace( *( ctx_str + name_len - 1 ) ) ) name_len--;
  if ( name_len == 0 )
    return invalid_context( msg, "missing name" );

  LCFGContext * ctx = lcfgcontext_new();

  char * name = strndup( ctx_str, name_len );
  bool ok = lcfgcontext_set_name(  ctx, name );
  if ( ok ) {

    char * start = location + 1;

    /* skip past any leading whitespace */
    while ( isspace(*start) ) start++;

    if ( *start != '\0' ) {

      /* Move backwards past any trailing whitespace (including newline) */
      size_t value_len = strlen(start);
      while ( value_len > 0 && isspace( *( start + value_len - 1 ) ) )
        value_len--;

      if ( value_len > 0 ) {
        char * value = strndup( start, value_len );
        ok = lcfgcontext_set_value( ctx, value );

        if (!ok) {
          invalid_context( msg, "bad value" );
          free(value);
        }
      }

    }

  } else {
    invalid_context( msg, "bad name" );
    free(name);
  }

  if (ok)
    ok = lcfgcontext_set_priority( ctx, priority );

  if (!ok) {
    lcfgcontext_release(ctx);
    ctx = NULL;

    if ( *msg == NULL )
      invalid_context( msg, "unknown error" );
  }

  *result = ctx;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/**
 * @brief Get the context as a string.
 *
 * Generates a new string representation of the @c LCFGContext
 * struct. This will have the form of "NAME = VALUE". A value is
 * required for the @e name attribute, the @e value attribute is
 * optional.
 *
 * Specifying the @c LCFG_OPT_NEWLINE option will result in a newline
 * character being added at the end of the string.
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many context strings, this can be a
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
 * longer required.
 *
 * @param[in] ctx Pointer to @c LCFGContext struct
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgcontext_to_string( const LCFGContext * ctx,
                               unsigned int options,
                               char ** result, size_t * size ) {
  assert( ctx != NULL );

  const char * name = lcfgcontext_get_name(ctx);
  if ( name == NULL ) return -1;

  size_t name_len = strlen(name);

  /* Might have a value */

  char * value = NULL;
  size_t value_len = 0;
  if ( lcfgcontext_has_value(ctx) ) {
    value = lcfgcontext_get_value(ctx);
    value_len = strlen(value);
  }

  size_t new_len = name_len + 1 + value_len; /* +1 for '=' separator */

  /* Optional newline at end of string */

  if ( options&LCFG_OPT_NEWLINE )
    new_len += 1;

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    *size = new_len + 1;

    *result = realloc( *result, ( *size * sizeof(char) ) );
    if ( *result == NULL ) {
      perror("Failed to allocate memory for LCFG context string");
      exit(EXIT_FAILURE);
    }

  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  to = stpncpy( to, name, name_len );

  *to = '=';
  to++;

  if ( value_len > 0 )
    to = stpncpy( to, value, value_len );

  /* Optional newline at the end of the string */

  if ( options&LCFG_OPT_NEWLINE )
    to = stpcpy( to, "\n" );

  *to = '\0';

  assert( (*result + new_len) == to );

  return new_len;
}

/**
 * @brief Write formatted context to file stream
 *
 * This uses @c lcfgcontext_to_string() to format the context as a
 * string with a trailing newline character. The generated string is
 * written to the specified file stream which must have already been
 * opened for writing.
 *
 * @param[in] ctx Pointer to @c LCFGContext
 * @param[in] out Stream to which the context string should be written
 *
 * @return boolean indicating success
 */
 
bool lcfgcontext_print( const LCFGContext * ctx,
                        FILE * out ) {
  assert( ctx != NULL );

  char * as_str = NULL;
  size_t buf_size = 0;

  bool ok = true;

  if ( lcfgcontext_to_string( ctx, LCFG_OPT_NEWLINE,
                              &as_str, &buf_size ) < 0 ) {
    ok = false;
  }

  if (ok) {

    if ( fputs( as_str, out ) < 0 )
      ok = false;

  }

  free(as_str);

  return ok;
}

/**
 * @brief Compare names of two contexts
 *
 * This uses @c strcmp(3) to compare the values of the @e name
 * attribute for the two contexts. For simplicity, if the value of
 * the @e name attribute for a context is @c NULL then it is treated
 * as an empty string.
 *
 * @param[in] ctx1 Pointer to @c LCFGContext struct
 * @param[in] ctx1 Pointer to @c LCFGContext struct
 *
 * @return boolean which indicates if the names are the same
 *
 */

bool lcfgcontext_same_name( const LCFGContext * ctx1,
                            const LCFGContext * ctx2 ) {
  assert( ctx1 != NULL && ctx2 != NULL );

  const char * name1 = lcfgcontext_has_name(ctx1) ?
                       lcfgcontext_get_name(ctx1) : "";
  const char * name2 = lcfgcontext_has_name(ctx2) ?
                       lcfgcontext_get_name(ctx2) : "";

  return ( strcmp( name1, name2 ) == 0 );
}

/**
 * @brief Compare values of two contexts
 *
 * This uses @c strcmp(3) to compare the values of the @e value
 * attribute for the two contexts. For simplicity, if the value of
 * the @e value attribute for a context is @c NULL then it is treated
 * as an empty string.
 *
 * @param[in] ctx1 Pointer to @c LCFGContext struct
 * @param[in] ctx1 Pointer to @c LCFGContext struct
 *
 * @return boolean which indicates if the values are the same
 *
 */

bool lcfgcontext_same_value( const LCFGContext * ctx1,
                             const LCFGContext * ctx2 ) {
  assert( ctx1 != NULL && ctx2 != NULL );

  const char * value1 = lcfgcontext_has_value(ctx1) ?
                        lcfgcontext_get_value(ctx1) : "";
  const char * value2 = lcfgcontext_has_value(ctx2) ?
                        lcfgcontext_get_value(ctx2) : "";

  return ( strcmp( value1, value2 ) == 0 );
}

/**
 * @brief Test if two contexts are equal
 *
 * The equality of two contexts is checked by comparing the values for
 * the @e name and @e value attributes.
 *
 * @param[in] ctx1 Pointer to @c LCFGContext struct
 * @param[in] ctx1 Pointer to @c LCFGContext struct
 *
 * @return boolean which indicates if the contexts are equal
 *
 */

bool lcfgcontext_equals( const LCFGContext * ctx1,
                         const LCFGContext * ctx2 ) {

  return ( lcfgcontext_same_name( ctx1, ctx2 ) &&
           lcfgcontext_same_value( ctx1, ctx2 ) );
}

/**
 * @brief Test if two contexts are identical
 *
 * This is a stronger version of the test used in
 * @c lcfgcontext_equals with the values for the priority also being
 * compared.
 *
 * @param[in] ctx1 Pointer to @c LCFGContext struct
 * @param[in] ctx1 Pointer to @c LCFGContext struct
 *
 * @return boolean which indicates if the contexts are identical
 *
 */

bool lcfgcontext_identical( const LCFGContext * ctx1,
                            const LCFGContext * ctx2 ) {

  return ( lcfgcontext_same_name( ctx1, ctx2 )  &&
           lcfgcontext_same_value( ctx1, ctx2 ) &&
           ( lcfgcontext_get_priority(ctx1) ==
             lcfgcontext_get_priority(ctx2) ) );
}

/**
 * @brief Generate the path for a context-specific profile
 *
 * The LCFG client supports the use of context-specific XML
 * profiles. A context-specific profile can be used to add or modify
 * components and resources when a context is enabled.
 *
 * The path to the context profile is based on a combination of the
 * values for the @e name and @e value attributes for the context, if
 * either is missing then this function will return a @c NULL
 * value.
 *
 * Optionally a base directory and a file suffix (typically ".xml")
 * can be specified.
 *
 * The value for the context name is taken to be a directory name and
 * the context value is used as a file name. This makes it possible to
 * change groups of resources when the value for a context is
 * altered. For example, if the context name is "temperature" the
 * values of "hot", "medium" and "cold" would typically give
 * "basedir/temperature/hot.xml", "basedir/temperature/medium.xml" and
 * "basedir/temperature/cold.xml".
 * 
 * To avoid memory leaks the generated string should be freed when it
 * is no longer required.
 *
 * @param[in] ctx Pointer to @c LCFGContext struct
 * @param[in] basedir Optional base directory
 * @param[in] suffix Optional file suffix
 *
 * @return Pointer to new string (call @c free() when no longer required)
 *
 */

char * lcfgcontext_profile_path( const LCFGContext * ctx,
                                 const char * basedir,
                                 const char * suffix ) {
  assert( ctx != NULL );

  /* Name, must be non-empty string */
  const char * name = lcfgcontext_get_name(ctx);
  if ( isempty(name) ) return NULL;

  /* Value, must be non-empty string */
  const char * value = lcfgcontext_get_value(ctx);
  if ( isempty(value) ) return NULL;

  char * result = NULL;
  int rc = 0;
  if ( isempty(basedir) ) {
    rc = asprintf( &result, "%s/%s%s",
                   name, value, ( suffix != NULL ? suffix : "" ) );
  } else {
    rc = asprintf( &result, "%s/%s/%s%s",
                   basedir, name, value, ( suffix != NULL ? suffix : "" ) );
  }

  if ( rc < 0 ) {
    perror( "Failed to build context profile path" );
    exit(EXIT_FAILURE);
  }

  return result;
}

/**
 * @brief Test if a context query expression is valid
 *
 * This can be used to test whether a context query expression
 * (e.g. @c foo!=5 or @c ( install | booting ) is valid. The test is
 * done by calling @c lcfgctxlist_eval_expression() with a @c NULL
 * context list.
 *
 * This is used in functions such as @c lcfgresource_valid_context()
 * and @c lcfgpackage_valid_context()
 *
 * @param[in] expr Pointer to query expression string
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return boolean which indicates if expression is valid
 *
 */

bool lcfgcontext_valid_expression( const char * expr, char ** msg ) {

  /* NULL is forbidden but empty string is acceptable */

  if ( expr == NULL )
    return false;
  else if ( *expr == '\0' )
    return true;

  /* Anything else needs to be parsed (with a NULL context list) to
     check the validity. */

  return lcfgctxlist_eval_expression( NULL, expr, &result, msg );
}

/**
 * @brief Wrap a context query expression in brackets
 *
 * When combining arbitrary context query expressions (e.g. using AND
 * or OR) it is safest to first wrap them in brackets. This function
 * will generate a new string, where necessary the original expression
 * will be wrapped with additional brackets.
 *
 * To avoid memory leaks the generated string should be freed when it
 * is no longer required.
 *
 * @param[in] expr Context query expression string
 *
 * @return Pointer to new string (call @c free() when no longer required)
 *
 */

char * lcfgcontext_bracketify_expression( const char * expr ) {

  if ( expr == NULL ) return NULL;

  /* Simply return copy if already bracketed or empty */
  if ( *expr == '\0' || *expr == '(' )
    return strdup(expr);

  char * result = NULL;
  if ( asprintf( &result, "(%s)", expr ) < 0 ) {
    perror( "Failed to build LCFG context string" );
    exit(EXIT_FAILURE);
  }

  return result;
}

/**
 * @brief Combine two context query expressions
 *
 * This can be used to logically combine (using an AND) two context
 * query expressions.
 *
 * An attempt is made to ensure that the order in which the
 * expressions are specified as arguments to the function does not
 * affect the combined expression which is created.
 *
 * To ensure it is safe to combine the expressions they are wrapped
 * with brackets using the @c lcfgcontext_bracketify_expression()
 * function.
 *
 * If the query expression strings are identical a copy of only the
 * first specified will be returned.
 *
 * If either query expression is a @c NULL value or an empty string it
 * will be ignored and a copy of the other parameter is returned. If
 * both are empty then an empty string will be returned.
 * 
 * To avoid memory leaks the generated string should be freed when it
 * is no longer required.
 *
 * @param[in] expr1 Context query expression string
 * @param[in] expr2 Context query expression string
 *
 * @return Pointer to new string (call @c free() when no longer required)
 *
 */

char * lcfgcontext_combine_expressions( const char * expr1,
                                        const char * expr2 ) {

  char * new_expr = NULL;

  if ( isempty(expr1) ) {

    if ( isempty(expr2) )
      new_expr = strdup("");
    else
      new_expr = strdup(expr2);

  } else {

    if ( isempty(expr2) ) {
      new_expr = strdup(expr1);
    } else {

      /* If the exprs are identical then just return one of them.
         Otherwise combine them in sort order so that the combination
         of two exprs should always give the same result no matter
         the order in which they are specified. */

      if ( strcmp( expr1, expr2 ) == 0 ) {
        new_expr = strdup(expr1);
      } else {
        char * ctx1_safe = lcfgcontext_bracketify_expression(expr1);
        char * ctx2_safe = lcfgcontext_bracketify_expression(expr2);

        int cmp = strcmp( ctx1_safe, ctx2_safe );
        if ( cmp == 0 ) {
          new_expr = ctx1_safe;
          free(ctx2_safe);
        } else {

          if ( cmp == -1 )
            new_expr = lcfgutils_join_strings( " & ", ctx1_safe, ctx2_safe );
          else 
            new_expr = lcfgutils_join_strings( " & ", ctx2_safe, ctx1_safe );

          free(ctx1_safe);
          free(ctx2_safe);
        }

      }

    }
  }

  if ( new_expr == NULL ) {
    perror( "Failed to combine LCFG context strings" );
    exit(EXIT_FAILURE);
  }

  return new_expr;
}

/* eof */
