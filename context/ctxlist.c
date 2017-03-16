#define _GNU_SOURCE /* for asprintf */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>

#include "context.h"
#include "utils.h"

LCFGContextNode * lcfgctxnode_new(LCFGContext * ctx) {

  LCFGContextNode * ctxnode = malloc( sizeof(LCFGContextNode) );
  if ( ctxnode == NULL ) {
    perror( "Failed to allocate memory for LCFG context node" );
    exit(EXIT_FAILURE);
  }

  ctxnode->context = ctx;
  ctxnode->next    = NULL;

  return ctxnode;
}

void lcfgctxnode_destroy(LCFGContextNode * ctxnode) {

  if ( ctxnode == NULL ) return;

  ctxnode->context = NULL;
  ctxnode->next    = NULL;

  free(ctxnode);
  ctxnode = NULL;

}

LCFGContextList * lcfgctxlist_new(void) {

  LCFGContextList * ctxlist = malloc( sizeof(LCFGContextList) );
  if ( ctxlist == NULL ) {
    perror( "Failed to allocate memory for LCFG context list" );
    exit(EXIT_FAILURE);
  }

  /* Default values */

  ctxlist->size = 0;
  ctxlist->head = NULL;
  ctxlist->tail = NULL;

  return ctxlist;
}

void lcfgctxlist_destroy(LCFGContextList * ctxlist) {

  if ( ctxlist == NULL )
    return;

  while ( lcfgctxlist_size(ctxlist) > 0 ) {
    LCFGContext * ctx = NULL;
    if ( lcfgctxlist_remove_after( ctxlist, NULL,
                                   &ctx ) == LCFG_CHANGE_REMOVED ) {
      lcfgcontext_release(ctx);
    }
  }

  free(ctxlist);
  ctxlist = NULL;

}

LCFGContextList * lcfgctxlist_clone( const LCFGContextList * ctxlist ) {

  LCFGContextList * clone = lcfgctxlist_new();

  /* Note that this does NOT clone the contexts themselves only the nodes. */
  bool ok = true;

  LCFGContextNode * cur_node = lcfgctxlist_head(ctxlist);
  while ( cur_node != NULL ) {
    LCFGContext * ctx = lcfgctxlist_context(cur_node);

    LCFGChange rc = lcfgctxlist_append( clone, ctx );
    if ( rc != LCFG_CHANGE_ADDED ) {
      ok = false;
      break;
    }

    cur_node = lcfgctxlist_next(cur_node);
  }

  if (!ok) {
    lcfgctxlist_destroy(clone);
    clone = NULL;
  }

  return clone;
}

LCFGChange lcfgctxlist_insert_after( LCFGContextList * ctxlist,
                                     LCFGContextNode * ctxnode,
                                     LCFGContext     * ctx ) {

  LCFGContextNode * new_node = lcfgctxnode_new(ctx);
  if ( new_node == NULL )
    return LCFG_CHANGE_ERROR;

  lcfgcontext_acquire(ctx);

  if ( ctxnode == NULL ) { /* New HEAD */

    if ( lcfgctxlist_is_empty(ctxlist) )
      ctxlist->tail = new_node;

    new_node->next = ctxlist->head;
    ctxlist->head  = new_node;

  } else {

    if ( ctxnode->next == NULL ) /* New TAIL */
      ctxlist->tail = new_node;

    new_node->next = ctxnode->next;
    ctxnode->next  = new_node;

  }

  ctxlist->size++;

  return LCFG_CHANGE_ADDED;
}

LCFGChange lcfgctxlist_remove_after( LCFGContextList * ctxlist,
                                     LCFGContextNode * ctxnode,
                                     LCFGContext    ** ctx ) {

  if ( lcfgctxlist_is_empty(ctxlist) )
    return LCFG_CHANGE_NONE;

  LCFGContextNode * old_node;

  if ( ctxnode == NULL ) { /* HEAD */

    old_node      = ctxlist->head;
    ctxlist->head = ctxlist->head->next;

    if ( lcfgctxlist_size(ctxlist) == 1 )
      ctxlist->tail = NULL;

  } else {

    if ( ctxnode->next == NULL )
      return LCFG_CHANGE_ERROR;

    old_node      = ctxnode->next;
    ctxnode->next = ctxnode->next->next;

    if ( ctxnode->next == NULL ) {
      ctxlist->tail = ctxnode;
    }

  }

  ctxlist->size--;

  *ctx = old_node->context;

  lcfgctxnode_destroy(old_node);

  return LCFG_CHANGE_REMOVED;
}

LCFGContextNode * lcfgctxlist_find_node( const LCFGContextList * ctxlist,
                                         const char * name ) {

  if ( ctxlist == NULL || lcfgctxlist_is_empty(ctxlist) )
    return NULL;

  LCFGContextNode * result = NULL;

  LCFGContextNode * cur_node = lcfgctxlist_head(ctxlist);
  while ( cur_node != NULL ) {

    if ( strcmp( cur_node->context->name, name ) == 0 ) {
      result = cur_node;
      break;
    }

    cur_node = lcfgctxlist_next(cur_node);
  }

  return result;
}

LCFGContext * lcfgctxlist_find_context( const LCFGContextList * ctxlist,
					const char * name ) {

  LCFGContext * context = NULL;

  LCFGContextNode * node = lcfgctxlist_find_node( ctxlist, name );
  if ( node != NULL )
    context = lcfgctxlist_context(node);

  return context;
}

bool lcfgctxlist_contains( const LCFGContextList * ctxlist,
			   const char * name ) {

  return ( lcfgctxlist_find_node( ctxlist, name ) != NULL );
}

LCFGChange lcfgctxlist_update( LCFGContextList * ctxlist,
                               LCFGContext     * new_ctx ) {

  LCFGContextNode * cur_node =
    lcfgctxlist_find_node( ctxlist, lcfgcontext_get_name(new_ctx) );

  LCFGChange result = LCFG_CHANGE_ERROR;
  if ( cur_node == NULL ) {
    result = lcfgctxlist_append( ctxlist, new_ctx );
  } else {

    LCFGContext * cur_ctx = lcfgctxlist_context(cur_node);
    if ( lcfgcontext_equals( cur_ctx, new_ctx ) ) {
      result = LCFG_CHANGE_NONE;
    } else {

      /* This completely replaces a context held in a node rather than
	 modifying any values. This is particularly useful when a list
	 might be a clone of another and thus the context is shared -
	 modifying a context in one list would also change the other
	 list. */

      lcfgcontext_acquire(new_ctx);
      cur_node->context = new_ctx;

      lcfgcontext_release(cur_ctx);

      result = LCFG_CHANGE_MODIFIED;
    }

  }

  return result;
}

LCFGStatus lcfgctxlist_from_file( const char * filename,
                                  LCFGContextList ** result,
				  time_t * modtime,
				  unsigned int options,
                                  char ** msg ) {

  *result = NULL;
  *modtime = 0;

  LCFGStatus status = LCFG_STATUS_OK;

  FILE *file;
  if ((file = fopen(filename, "r")) == NULL) {

    if (errno == ENOENT) {
      if ( options&LCFG_OPT_ALLOW_NOEXIST ) {
	/* No file so just create an empty list */
	*result = lcfgctxlist_new();
      } else {
	asprintf( msg, "'%s' does not exist.", filename );
	status = LCFG_STATUS_ERROR;
      }
    } else {
      asprintf( msg, "'%s' is not readable.", filename );
      status = LCFG_STATUS_ERROR;
    }

    return status;
  }

  /* Collect the mtime for the file as often need to compare the times */

  struct stat buf;
  if ( stat( filename, &buf ) == 0 )
    *modtime = buf.st_mtime;

  LCFGContextList * ctxlist = lcfgctxlist_new();

  size_t line_len = 128;
  char * line = calloc( line_len, sizeof(char) );
  if ( line == NULL ) {
    perror("Failed to allocate memory for processing LCFG context file" );
    exit(EXIT_FAILURE);
  }

  int linenum = 0; /* The line number is used as the context priority */
  while( getline( &line, &line_len, file ) != -1 ) {
    linenum++;

    char * ctx_str = line;

    /* skip past any leading whitespace */
    while ( isspace(*ctx_str) ) ctx_str++;

    /* ignore empty lines and comments */
    if ( *ctx_str == '\0' || *ctx_str == '#' ) continue;

    char * parse_msg = NULL;
    LCFGContext * ctx = NULL;
    LCFGStatus parse_rc = lcfgcontext_from_string( ctx_str, linenum,
                                                   &ctx, &parse_msg );
    if ( parse_rc != LCFG_STATUS_OK || ctx == NULL ) {
      asprintf( msg, "Failed to parse context '%s' on line %d of %s: %s",
                ctx_str, linenum, filename, parse_msg );
      status = LCFG_STATUS_ERROR;
    }

    free(parse_msg);

    if ( status == LCFG_STATUS_OK ) {

      LCFGChange rc = lcfgctxlist_update( ctxlist, ctx );

      if ( rc == LCFG_CHANGE_ERROR ) {
        asprintf( msg, "Failed to store context '%s'", ctx_str );
        status = LCFG_STATUS_ERROR;
      }

    }

    lcfgcontext_release(ctx);

    if ( status != LCFG_STATUS_OK ) break;

  }

  fclose(file);
  free(line);

  if ( status != LCFG_STATUS_OK ) {
    lcfgctxlist_destroy(ctxlist);
    ctxlist = NULL;
  }

  *result = ctxlist;

  return status;
}

bool lcfgctxlist_print( const LCFGContextList * ctxlist,
                        FILE * out ) {

  if ( lcfgctxlist_is_empty(ctxlist) )
    return true;

  /* Allocate a reasonable buffer which will be reused for each
     context. Note that if necessary it will be resized automatically. */

  size_t buf_size = 32;
  char * str_buf  = calloc( buf_size, sizeof(char) );
  if ( str_buf == NULL ) {
    perror("Failed to allocate memory for LCFG context string");
    exit(EXIT_FAILURE);
  }

  bool ok = true;

  LCFGContextNode * cur_node = lcfgctxlist_head(ctxlist);
  while ( cur_node != NULL && ok ) {
    const LCFGContext * ctx = lcfgctxlist_context(cur_node);

    /* No need to write out those contexts which do not have a value */
    if ( lcfgcontext_has_value(ctx) ) {

      if ( lcfgcontext_to_string( ctx, LCFG_OPT_NEWLINE,
                                  &str_buf, &buf_size ) < 0 ) {
        ok = false;
      }

      if ( fputs( str_buf, out ) < 0 )
        ok = false;

    }

    cur_node = lcfgctxlist_next(cur_node);
  }

  free(str_buf);

  return ok;
}

LCFGStatus lcfgctxlist_to_file( LCFGContextList * ctxlist,
                                const char * filename,
                                time_t mtime,
                                char ** errmsg ) {

  lcfgctxlist_sort_by_priority(ctxlist);

  FILE *file;
  if ((file = fopen(filename, "w")) == NULL) {
    asprintf( errmsg, "Failed to open file '%s' for writing", filename );
    return LCFG_STATUS_ERROR;
  }

  bool ok = lcfgctxlist_print( ctxlist, file );

  if (ok) {
    if ( fclose(file) != 0 ) {
      asprintf( errmsg, "Failed to close file '%s'", filename );
      ok = false;
    }

    /* Should this WARN if it fails? */
    if ( ok && mtime != 0 ) {
      struct utimbuf times;
      times.actime  = mtime;
      times.modtime = mtime;
      utime( filename, &times );
    }
  }

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

int lcfgctxlist_max_priority( const LCFGContextList * ctxlist ) {

  int max_priority = 0;

  LCFGContextNode * cur_node = lcfgctxlist_head(ctxlist);

  while ( cur_node != NULL ) {
    int priority = lcfgcontext_get_priority(lcfgctxlist_context(cur_node));
    if ( priority > max_priority )
      max_priority = priority;

    cur_node = lcfgctxlist_next(cur_node);
  }

  return max_priority;
}

void lcfgctxlist_sort_by_priority( LCFGContextList * ctxlist ) {

  /* bubble sort since this should be a very short list */

  bool swapped=true;
  while (swapped) {
    swapped=false;
    LCFGContextNode * cur_node = lcfgctxlist_head(ctxlist);

    while ( cur_node != NULL && cur_node->next != NULL ) {
      LCFGContext * cur_ctx  = lcfgctxlist_context(cur_node);
      LCFGContext * next_ctx = lcfgctxlist_context(cur_node->next);

      if ( lcfgcontext_get_priority(cur_ctx) >
           lcfgcontext_get_priority(next_ctx) ) {
        cur_node->context       = next_ctx;
        cur_node->next->context = cur_ctx;
        swapped = true;
      }

      cur_node = lcfgctxlist_next(cur_node);
    }
  }

}

bool lcfgctxlist_diff( const LCFGContextList * ctxlist1,
                       const LCFGContextList * ctxlist2,
		       const char * ctx_profile_dir,
                       time_t prevtime ) {

  bool changed = false;
  LCFGContextNode * cur_node;

  /* Check for missing nodes and also compare values for common nodes */

  cur_node = lcfgctxlist_head(ctxlist1);
  while ( cur_node != NULL && !changed ) {
    const LCFGContext * cur_ctx = lcfgctxlist_context(cur_node);

    const LCFGContext * other_ctx =
      lcfgctxlist_find_context( ctxlist2, lcfgcontext_get_name(cur_ctx) );

    if ( other_ctx == NULL ) {
      changed = true;
      break;
    }

    if ( !lcfgcontext_identical( cur_ctx, other_ctx ) ) {
      changed = true;
      break;
    }

    if ( ctx_profile_dir != NULL ) {
      /* A context may have an associated LCFG profile. Check if it has
	 been modified since the last run (just compare timestamps). */

      char * path = lcfgcontext_profile_path( cur_ctx,
					      ctx_profile_dir, ".xml" );
      if ( path != NULL ) {

	struct stat buffer;
	if ( stat ( path, &buffer ) == 0 &&
	     S_ISREG(buffer.st_mode) &&
	     buffer.st_mtime > prevtime ) {
	    changed = true;
	}
      }

      free(path);

      if (changed) break;
    }

    cur_node = lcfgctxlist_next(cur_node);
  }

  /* If nothing changed so far then check for missing nodes the other way */

  if ( !changed ) {
    cur_node = lcfgctxlist_head(ctxlist2);
    while ( cur_node != NULL ) {
      const LCFGContext * cur_ctx = lcfgctxlist_context(cur_node);

      const LCFGContext * other_ctx =
        lcfgctxlist_find_context( ctxlist1, lcfgcontext_get_name(cur_ctx) );

      if ( other_ctx == NULL ) {
        changed = true;
        break;
      }

      cur_node = lcfgctxlist_next(cur_node);
    }
  }

  return changed;
}

int lcfgctxlist_simple_query( const LCFGContextList * ctxlist,
                              const char * ctxq_name,
                              const char * ctxq_val,
                              LCFGTest cmp ) {

  LCFGContext * ctx = lcfgctxlist_find_context( ctxlist, ctxq_name );

  int priority = 1;
  char * ctx_value = NULL;

  if ( ctx != NULL ) {
    priority  = lcfgcontext_get_priority(ctx);
    ctx_value = lcfgcontext_get_value(ctx);
  }

  bool query_is_true = false;
  switch (cmp)
    {
    case LCFG_TEST_ISTRUE:
      query_is_true = lcfgcontext_is_true(ctx);
      break;
    case LCFG_TEST_ISFALSE:
      query_is_true = lcfgcontext_is_false(ctx);
      break;
    case LCFG_TEST_ISEQ:
    case LCFG_TEST_ISNE:
      ;

      bool same_value = false;
      if ( ctx_value == NULL ) {
        same_value = ( ctxq_val == NULL );
      } else if ( ctxq_val != NULL ) {
        same_value = ( strcmp( ctxq_val, ctx_value ) == 0 );
      }

      query_is_true = (  same_value && cmp == LCFG_TEST_ISEQ ) ||
                      ( !same_value && cmp == LCFG_TEST_ISNE );
      break;
    }

  return ( query_is_true ? priority : -1 * priority );
}

bool lcfgctxlist_eval_expression( const LCFGContextList * ctxlist,
                                  const char * expr,
                                  int * result,
                                  char ** errmsg ) {

  *errmsg = NULL;
  bool ok = true;
  *result = 0;

  int factor = 1;

  char * start = ( char * ) expr;

  /* skip past any leading whitespace */
  while ( isspace(*start) ) start++;

  if ( *start == '!' ) { /* negate entire result */
    factor *= -1;
    start++;
  }

  char * ctx_name  = NULL;
  char * ctx_value = NULL;

  char * match = strstr( start, "!=" );

  if ( match != NULL ) { /* negative comparison */
    factor *= -1;

    ctx_name  = strndup( start, match - start );
    ctx_value = strdup( match + 2 );
  } else {
    match = strchr( start, '=' );
    if ( match != NULL ) { /* positive comparison */

      ctx_name  = strndup( start, match - start );
      ctx_value = strdup( match + 1 );
    } else {               /* boolean match */

      ctx_name = strdup(start);
    }
  }

  lcfgutils_trim_whitespace(ctx_name);
  if ( ctx_value != NULL ) {
    lcfgutils_trim_whitespace(ctx_value);
  }

  if ( !lcfgcontext_valid_name(ctx_name) ) {
    ok = false;
    asprintf( errmsg, "Invalid context name '%s'", ctx_name );
  } else if ( ctx_value != NULL && !lcfgcontext_valid_value(ctx_value) ) {
    ok = false;
    asprintf( errmsg, "Invalid context value '%s'", ctx_value );
  }

  if (ok) {
    LCFGContext * context = lcfgctxlist_find_context( ctxlist, ctx_name );

    if ( ctx_value == NULL || strcmp( ctx_value, "true" ) == 0 ) {
      if ( lcfgcontext_is_false( context ) ) {
        factor *= -1;
      }
    } else if ( strcmp( ctx_value, "false" ) == 0 ) {
      if ( lcfgcontext_is_true( context ) ) {
        factor *= -1;
      }
    } else {
      const char * cur_value = ( context != NULL && lcfgcontext_has_value(context) ) ? lcfgcontext_get_value(context) : "";

      if ( strcmp( cur_value, ctx_value ) != 0 ) {
        factor *= -1;
      }
    }

    *result = ( context != NULL ? lcfgcontext_get_priority(context) : 1 )
            * factor;
  }

  free(ctx_name);
  free(ctx_value);

  return ok;
}

/* eof */
