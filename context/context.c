#define _GNU_SOURCE /* for asprintf */

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#include "context.h"
#include "utils.h"

#define isempty(STR) ( STR == NULL || *(STR) == '\0' )
#define isword(CHR) ( isalnum(CHR) || CHR == '_' )

void lcfg_build_message( char ** strp, const char *fmt, ... ) {
  free( *strp );
  *strp = NULL;

  va_list ap;
  va_start( ap, fmt );

  if ( vasprintf( strp, fmt, ap ) < 0 ) {
    perror( "Failed to build error string" );
    exit(EXIT_FAILURE);
  }
}

static LCFGStatus invalid_context( char ** msg, const char * reason ) {
  lcfg_build_message( msg, "Invalid context (%s)", reason );
  return LCFG_STATUS_ERROR;
}

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
  ctx->_refcount = 1;

  return ctx;
}

void lcfgcontext_destroy( LCFGContext * ctx ) {

  if ( ctx == NULL ) return;

  free(ctx->name);
  ctx->name = NULL;

  free(ctx->value);
  ctx->value = NULL;

  free(ctx);
  ctx = NULL;

}

void lcfgcontext_acquire( LCFGContext * ctx ) {
  assert( ctx != NULL );

  ctx->_refcount += 1;
}

void lcfgcontext_release( LCFGContext * ctx ) {

  if ( ctx == NULL ) return;

  if ( ctx->_refcount > 0 )
    ctx->_refcount -= 1;

  if ( ctx->_refcount == 0 )
    lcfgcontext_destroy(ctx);

}

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

bool lcfgcontext_has_name( const LCFGContext * ctx ) {
  assert( ctx != NULL );

  return !isempty(ctx->name);
}

char * lcfgcontext_get_name( const LCFGContext * ctx ) {
  assert( ctx != NULL );

  return ctx->name;
}

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

bool lcfgcontext_valid_value( const char * value ) {

  /* Note that the empty string is intentionally valid */

  bool valid = ( value != NULL );

  return valid;
}

bool lcfgcontext_has_value( const LCFGContext * ctx ) {
  assert( ctx != NULL );

  return !isempty(ctx->value);
}

char * lcfgcontext_get_value( const LCFGContext * ctx ) {
  assert( ctx != NULL );

  return ctx->value;
}

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

bool lcfgcontext_is_true( const LCFGContext * ctx ) {
  return !lcfgcontext_is_false(ctx);
}

int lcfgcontext_get_priority( const LCFGContext * ctx ) {
  assert( ctx != NULL );

  return ctx->priority;
}

bool lcfgcontext_set_priority( LCFGContext * ctx, int priority ) {
  assert( ctx != NULL );

  ctx->priority = priority;
  return true;
}

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

bool lcfgcontext_same_name( const LCFGContext * ctx1,
                            const LCFGContext * ctx2 ) {
  assert( ctx1 != NULL && ctx2 != NULL );

  const char * name1 = lcfgcontext_has_name(ctx1) ?
                       lcfgcontext_get_name(ctx1) : "";
  const char * name2 = lcfgcontext_has_name(ctx2) ?
                       lcfgcontext_get_name(ctx2) : "";

  return ( strcmp( name1, name2 ) == 0 );
}

bool lcfgcontext_same_value( const LCFGContext * ctx1,
                             const LCFGContext * ctx2 ) {
  assert( ctx1 != NULL && ctx2 != NULL );

  const char * value1 = lcfgcontext_has_value(ctx1) ?
                        lcfgcontext_get_value(ctx1) : "";
  const char * value2 = lcfgcontext_has_value(ctx2) ?
                        lcfgcontext_get_value(ctx2) : "";

  return ( strcmp( value1, value2 ) == 0 );
}

bool lcfgcontext_equals( const LCFGContext * ctx1,
                         const LCFGContext * ctx2 ) {

  return ( lcfgcontext_same_name( ctx1, ctx2 ) &&
           lcfgcontext_same_value( ctx1, ctx2 ) );
}

bool lcfgcontext_identical( const LCFGContext * ctx1,
                            const LCFGContext * ctx2 ) {

  return ( lcfgcontext_same_name( ctx1, ctx2 )  &&
           lcfgcontext_same_value( ctx1, ctx2 ) &&
           ( lcfgcontext_get_priority(ctx1) ==
             lcfgcontext_get_priority(ctx2) ) );
}

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

bool lcfgcontext_valid_expression( const char * expr ) {
  /* TODO: this needs to hook into the new flex/bison parser */
  return ( expr != NULL );
}

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
