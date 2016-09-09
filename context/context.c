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

LCFGContext * lcfgcontext_new(void) {

  LCFGContext * ctx = malloc( sizeof(LCFGContext) );
  if ( ctx == NULL ) {
    perror( "Failed to allocate memory for LCFG context" );
    exit(EXIT_FAILURE);
  }

  /* Default values */

  ctx->name      = NULL;
  ctx->value     = NULL;
  ctx->priority  = 0;
  ctx->_refcount = 0;

  return ctx;
}

void lcfgcontext_destroy( LCFGContext * ctx ) {

  if ( ctx == NULL )
    return;

  if ( ctx->_refcount > 0 )
    return;

  free(ctx->name);
  ctx->name = NULL;

  free(ctx->value);
  ctx->value = NULL;

  free(ctx);
  ctx = NULL;

}

inline static bool isword( const char chr ) {
  return ( isalnum(chr) || chr == '_' );
}

bool lcfgcontext_valid_name( const char * name ) {

  /* MUST NOT be a NULL.
     MUST have non-zero length.
     First character MUST be in [A-Za-z] set. */

  bool valid = ( name != NULL && *name != '\0' && isalpha(*name) );

  /* All other characters MUST be in [A-Za-z0-9_] set */

  char * ptr;
  for ( ptr = ((char *)name) + 1; valid && *ptr != '\0'; ptr++ ) {
    if ( !isword(*ptr) ) valid = false;
  }

  return valid;
}

bool lcfgcontext_has_name( const LCFGContext * ctx ) {
  return ( ctx->name != NULL && *( ctx->name ) != '\0' );
}

char * lcfgcontext_get_name( const LCFGContext * ctx ) {
  return ctx->name;
}

bool lcfgcontext_set_name( LCFGContext * ctx, char * new_name ) {

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

static const char * allowed_chars = ".,%:";

bool lcfgcontext_valid_value( const char * value ) {

  /* Note that the empty string is intentionally valid */

  bool valid = ( value != NULL );

  /* As well as word [A-Za-z0-9_] allow some other characters in values */

  char * ptr;
  for ( ptr = ((char *)value); valid && *ptr != '\0'; ptr++ ) {
    if ( !isword(*ptr) && strchr( allowed_chars, *ptr ) == NULL )
      valid = false;
  }

  return valid;
}

bool lcfgcontext_has_value( const LCFGContext * ctx ) {
  return ( ctx->value != NULL && *( ctx->value ) != '\0' );
}

char * lcfgcontext_get_value( const LCFGContext * ctx ) {
  return ctx->value;
}

bool lcfgcontext_set_value( LCFGContext * ctx, char * new_value ) {

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

bool lcfgcontext_unset_value( LCFGContext * ctx ) {
  free(ctx->value);
  ctx->value = NULL;

  return true;
}

static char * valid_false_values[] = {
  "false", "no", "off", "0", "",
  NULL
};

bool lcfgcontext_is_false( const LCFGContext * ctx ) {

  if ( ctx == NULL || ctx->value == NULL || *( ctx->value ) == '\0' )
    return true;

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

bool lcfgcontext_is_true( const LCFGContext * ctx ) {

  return !lcfgcontext_is_false(ctx);
}

int lcfgcontext_get_priority( const LCFGContext * ctx ) {
  return ctx->priority;
}

bool lcfgcontext_set_priority( LCFGContext * ctx, int priority ) {
  ctx->priority = priority;
  return true;
}

LCFGStatus lcfgcontext_from_string( const char * input, int priority,
                                    LCFGContext ** result,
                                    char ** msg ) {

  *result = NULL;
  *msg    = NULL;

  if ( input == NULL ) {
    asprintf( msg, "Invalid context (undefined value)" );
    return LCFG_STATUS_ERROR;
  }

  char * ctx_str = (char *) input;

  /* skip past any leading whitespace */
  while ( isspace(*ctx_str) ) ctx_str++;

  if ( *ctx_str == '\0' ) {
    asprintf( msg, "Invalid context (empty string)" );
    return LCFG_STATUS_ERROR;
  }

  /* Find the '=' character which separates the context name and value */

  char * location = strchr( ctx_str, '=' );
  if ( location == NULL ) {
    asprintf( msg, "Invalid context (missing '=' assignment character)" );
    return LCFG_STATUS_ERROR;
  }

  /* Ignore any whitespace after the name (before the '=') */

  size_t name_len = location - ctx_str;
  while ( name_len > 0 && isspace( *( ctx_str + name_len - 1 ) ) ) name_len--;
  if ( name_len == 0 ) {
    asprintf( msg, "Invalid context (missing name)" );
    return LCFG_STATUS_ERROR;
  }

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
      while( value_len > 0 && isspace( *( start + value_len - 1 ) ) ) value_len--;

      if ( value_len > 0 ) {
        char * value = strndup( start, value_len );
        ok = lcfgcontext_set_value( ctx, value );

        if (!ok) {
          asprintf( msg, "Invalid context (bad value '%s')", start );
	  free(value);
        }
      }

    }

  } else {
    asprintf( msg, "Invalid context (bad name '%s')", name );
    free(name);
  }

  if (ok)
    ok = lcfgcontext_set_priority( ctx, priority );

  if (!ok) {
    lcfgcontext_destroy(ctx);
    ctx = NULL;
  }

  *result = ctx;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

ssize_t lcfgcontext_to_string( const LCFGContext * ctx,
                               unsigned int options,
                               char ** result, size_t * size ) {

  const char * name = lcfgcontext_get_name(ctx);
  if ( name == NULL )
    return -1;

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

bool lcfgcontext_print( const LCFGContext * ctx,
                        FILE * out ) {

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

bool lcfgcontext_equals( const LCFGContext * ctx1,
                         const LCFGContext * ctx2 ) {

  const char * name1 = lcfgcontext_get_name(ctx1);
  const char * name2 = lcfgcontext_get_name(ctx2);

  bool equal = ( strcmp( name1, name2 ) == 0 );

  if (equal) {

    const char * value1 = lcfgcontext_has_value(ctx1) ? lcfgcontext_get_value(ctx1) : "";
    const char * value2 = lcfgcontext_has_value(ctx2) ? lcfgcontext_get_value(ctx2) : "";

    equal = ( strcmp( value1, value2 ) == 0 );
  }

  return equal;
}

bool lcfgcontext_identical( const LCFGContext * ctx1,
                            const LCFGContext * ctx2 ) {

  bool identical = lcfgcontext_equals( ctx1, ctx2 );

  if (identical)
    identical = ( lcfgcontext_get_priority(ctx1) == lcfgcontext_get_priority(ctx2) );

  return identical;
}

char * lcfgcontext_profile_path( const LCFGContext * ctx,
				 const char * basedir,
				 const char * suffix ) {

  /* Name, must be non-empty string */
  const char * name = lcfgcontext_get_name(ctx);
  if ( name == NULL || *name == '\0' )
    return NULL;

  size_t name_len = strlen(name);

  /* Value, must be non-empty string */
  const char * value = lcfgcontext_get_value(ctx);
  if ( value == NULL || *value == '\0' )
    return NULL;

  size_t value_len = strlen(value);

  size_t new_len = name_len + 1 + value_len; /* +1 for '/' */

  /* Optional file name suffix */

  size_t suffix_len = 0;
  if ( suffix != NULL ) {
    suffix_len = strlen(suffix);
    new_len += suffix_len;
  }

  /* Optional base directory */

  size_t base_len = 0;
  if ( basedir != NULL ) {
    base_len = strlen(basedir);
    if ( base_len > 0 )
      new_len += ( base_len + 1 ); /* +1 for '/' */
  }

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate memory for LCFG context profile name");
    exit(EXIT_FAILURE);
  }

  char * to = result;

  if ( base_len > 0 ) {
    to = stpncpy( to, basedir, base_len );

    *to = '/';
    to++;
  }

  to = stpncpy( to, name, name_len );

  *to = '/';
  to++;

  to = stpncpy( to, value, value_len );

  if ( suffix_len > 0 )
    to = stpncpy( to, suffix, suffix_len );

  *to = '\0';

  assert( (result + new_len) == to );

  return result;
}

static const char * expr_chars = "()&|=!";

bool lcfgcontext_valid_expression( const char * expr ) {

  /* This really needs to be a lot more sophisticated and actually
     check the syntax. A check of the individual characters
     will have to do for now. */

  bool valid = ( expr != NULL );

  char * ptr;
  for ( ptr = (char *) expr; valid && *ptr != '\0'; ptr++ ) {
    if ( !isspace(*ptr) &&
         !isword(*ptr)  &&
         strchr( allowed_chars, *ptr ) == NULL &&
         strchr( expr_chars, *ptr )    == NULL ) {
      valid = false;
    }
  }

  return valid;
}

char * lcfgcontext_bracketify_expression( const char * expr ) {

  char * result = NULL;

  if ( expr != NULL ) {

    size_t expr_len = strlen(expr);

    /* Simply copy if already bracketed or empty */
    if ( *expr == '(' || expr_len == 0 ) {
      result = strndup(expr, expr_len);
    } else {

      size_t new_len = expr_len + 2; /* +2 for '(' and ')' */
      result = calloc( ( new_len + 1 ), sizeof(char) );
      if ( result == NULL ) {
        perror( "Failed to allocate memory for LCFG context string" );
        exit(EXIT_FAILURE);
      }

      char * to = result;

      *to = '(';
      to++;

      to = stpncpy( to, expr, expr_len );

      *to = ')';
      to++;

      *to = '\0';

      assert( (result + new_len) == to );

    }

  }

  return result;
}


char * lcfgcontext_combine_expressions( const char * expr1,
                                        const char * expr2 ) {

  char * new_expr;

  if ( expr1 == NULL || strlen(expr1) == 0 ) {

    if ( expr2 == NULL || strlen(expr2) == 0 ) {
      new_expr = strdup("");
    } else {
      new_expr = strdup(expr2);
    }

  } else {

    if ( expr2 == NULL || strlen(expr2) == 0 ) {
      new_expr = strdup(expr1);
    } else {

      /* If the exprs are identical then just return one of them.
         Otherwise combine them in sort order so that the combination
         of two exprs should always give the same result no matter
         the order in which they are specified. */

      int cmp = strcmp( expr1, expr2 );
      if ( cmp == 0 ) {
        new_expr = strdup(expr1);
      } else {
        char * ctx1_safe = lcfgcontext_bracketify_expression(expr1);
        char * ctx2_safe = lcfgcontext_bracketify_expression(expr2);
        cmp = strcmp( ctx1_safe, ctx2_safe );

        if ( cmp == 0 ) {
          new_expr = ctx1_safe;
          free(ctx2_safe);
        } else {
          if ( cmp == -1 ) {
            new_expr = lcfgutils_join_strings( ctx1_safe, ctx2_safe, " & " );
          } else {
            new_expr = lcfgutils_join_strings( ctx2_safe, ctx1_safe, " & " );
          }
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

