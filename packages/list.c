#define _GNU_SOURCE   /* asprintf */
#define _WITH_GETLINE /* for BSD */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "packages.h"
#include "utils.h"

LCFGPackageNode * lcfgpkgnode_new(LCFGPackage * pkg) {

  LCFGPackageNode * pkgnode = malloc( sizeof(LCFGPackageNode) );
  if ( pkgnode == NULL ) {
    perror( "Failed to allocate memory for LCFG package node" );
    exit(EXIT_FAILURE);
  }

  pkgnode->pkg  = pkg;
  pkgnode->next = NULL;

  return pkgnode;
}

void lcfgpkgnode_destroy(LCFGPackageNode * pkgnode) {

  if ( pkgnode == NULL ) return;

  pkgnode->pkg  = NULL;
  pkgnode->next = NULL;

  free(pkgnode);
  pkgnode = NULL;

}

LCFGPackageList * lcfgpkglist_new(void) {

  LCFGPackageList * pkglist = malloc( sizeof(LCFGPackageList) );
  if ( pkglist == NULL ) {
    perror( "Failed to allocate memory for LCFG package list" );
    exit(EXIT_FAILURE);
  }

  pkglist->merge_rules = 0;
  pkglist->size = 0;
  pkglist->head = NULL;
  pkglist->tail = NULL;

  return pkglist;
}

void lcfgpkglist_destroy(LCFGPackageList * pkglist) {

  if ( pkglist == NULL ) return;

  while ( lcfgpkglist_size(pkglist) > 0 ) {
    LCFGPackage * pkg = NULL;
    if ( lcfgpkglist_remove_next( pkglist, NULL, &pkg )
         == LCFG_CHANGE_REMOVED ) {
      lcfgpackage_release(pkg);
    }
  }

  free(pkglist);
  pkglist = NULL;

}

bool lcfgpkglist_set_merge_rules( LCFGPackageList * pkglist,
				  unsigned int new_rules ) {

  pkglist->merge_rules = new_rules;

  return true;
}

unsigned int lcfgpkglist_get_merge_rules( const LCFGPackageList * pkglist ) {
  return pkglist->merge_rules;
}

LCFGChange lcfgpkglist_insert_next( LCFGPackageList * pkglist,
                                    LCFGPackageNode * pkgnode,
                                    LCFGPackage * pkg ) {

  LCFGPackageNode * new_node = lcfgpkgnode_new(pkg);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgpackage_acquire(pkg);

  if ( pkgnode == NULL ) { /* HEAD */

    if ( lcfgpkglist_is_empty(pkglist) )
      pkglist->tail = new_node;

    new_node->next = pkglist->head;
    pkglist->head  = new_node;

  } else {
    
    if ( pkgnode->next == NULL ) /* TAIL */
      pkglist->tail = new_node;

    new_node->next = pkgnode->next;
    pkgnode->next  = new_node;

  }

  pkglist->size++;

  return LCFG_CHANGE_ADDED;
}

LCFGChange lcfgpkglist_remove_next( LCFGPackageList * pkglist,
                                    LCFGPackageNode * pkgnode,
                                    LCFGPackage ** pkg ) {

  if ( lcfgpkglist_is_empty(pkglist) ) return LCFG_CHANGE_ERROR;

  LCFGPackageNode * old_node;

  if ( pkgnode == NULL ) { /* HEAD */

    old_node = pkglist->head;
    pkglist->head = pkglist->head->next;

    if ( lcfgpkglist_size(pkglist) == 1 )
      pkglist->tail = NULL;

  } else {

    if ( pkgnode->next == NULL ) return LCFG_CHANGE_ERROR;

    old_node = pkgnode->next;
    pkgnode->next = pkgnode->next->next;

    if ( pkgnode->next == NULL )
      pkglist->tail = pkgnode;

  }

  pkglist->size--;

  *pkg = old_node->pkg;

  lcfgpkgnode_destroy(old_node);

  return LCFG_CHANGE_REMOVED;
}

LCFGPackageNode * lcfgpkglist_find_node( const LCFGPackageList * pkglist,
                                         const char * name,
                                         const char * arch ) {

  if ( pkglist == NULL || lcfgpkglist_is_empty(pkglist) )
    return NULL;

  const char * match_arch = arch != NULL ? arch : LCFG_PACKAGE_NOVALUE;
  bool arch_match_iswild = 
    ( strcmp( LCFG_PACKAGE_WILDCARD, match_arch ) == 0 );

  LCFGPackageNode * result = NULL;

  LCFGPackageNode * cur_node = lcfgpkglist_head(pkglist);
  while ( cur_node != NULL ) {

    const LCFGPackage * pkg = lcfgpkglist_package(cur_node);

    if ( !lcfgpackage_is_active(pkg) ) continue;

    const char * pkg_name = lcfgpackage_get_name(pkg);

    if ( pkg_name != NULL && strcmp( pkg_name, name ) == 0 ) {
      const char * pkg_arch = lcfgpackage_has_arch(pkg) ?
                   lcfgpackage_get_arch(pkg) : LCFG_PACKAGE_NOVALUE;

      if (  arch_match_iswild || strcmp( pkg_arch, match_arch ) == 0 ) {
        result = cur_node;
        break;
      }
    }

    cur_node = lcfgpkglist_next(cur_node);
  }

  return result;

}

LCFGPackage * lcfgpkglist_find_package( const LCFGPackageList * pkglist,
                                            const char * name,
                                            const char * arch ) {

  LCFGPackage * pkg = NULL;

  LCFGPackageNode * pkgnode = lcfgpkglist_find_node( pkglist, name, arch );
  if ( pkgnode != NULL )
    pkg = lcfgpkglist_package(pkgnode);

  return pkg;
}

LCFGChange lcfgpkglist_merge_package( LCFGPackageList * pkglist,
                                      LCFGPackage * new_pkg,
                                      char ** msg ) {

  unsigned int merge_rules = lcfgpkglist_get_merge_rules(pkglist);

  *msg = NULL;

  /* Define these ahead of any jumps to the "apply" label */

  LCFGPackageNode * prev_node = NULL;
  LCFGPackageNode * cur_node  = NULL;
  LCFGPackage * cur_pkg  = NULL;

  /* Actions */

  bool remove_old = false;
  bool append_new = false;
  bool accept     = false;

  /* Doing a search here rather than calling find_node so that the
     previous node can also be selected. That is needed for removals. */

  if ( !lcfgpackage_has_name(new_pkg) ) {
    asprintf( msg, "New package does not have a name" );
    goto apply;
  }

  const char * match_name = lcfgpackage_get_name(new_pkg);
  const char * match_arch = lcfgpackage_has_arch(new_pkg) ?
    lcfgpackage_get_arch(new_pkg) : LCFG_PACKAGE_NOVALUE;

  LCFGPackageNode * node = lcfgpkglist_head(pkglist);
  while ( node != NULL ) {

    const LCFGPackage * pkg = lcfgpkglist_package(node);

    if ( !lcfgpackage_is_active(pkg) ) continue;

    const char * name = lcfgpackage_get_name(pkg);
    if ( name != NULL && strcmp( name, match_name ) == 0 ) {

      const char * arch = lcfgpackage_has_arch(pkg) ?
	lcfgpackage_get_arch(pkg) : LCFG_PACKAGE_NOVALUE;

      if ( strcmp( arch, match_arch ) == 0 ) {
	cur_node = node;
	break;
      }
    }

    prev_node = node;
    node = lcfgpkglist_next(node);
  }

  if ( cur_node != NULL ) {
    cur_pkg = lcfgpkglist_package(cur_node);

    /* Merging a struct which is already in the list is a no-op. Note
       that this does not prevent the same spec appearing multiple
       times in the list if they are in different structs. */

    if ( cur_pkg == new_pkg ) {
      accept = true;
      goto apply;
    }
  }

  /* Apply any prefix rules */

  if ( merge_rules&LCFG_PKGS_OPT_USE_PREFIX ) {

    char cur_prefix = cur_pkg != NULL ?
                      lcfgpackage_get_prefix(cur_pkg) : '\0';

    if ( cur_prefix == '=' ) {
      *msg = lcfgpackage_build_message( cur_pkg, "Version is pinned" );
      goto apply;
    }

    char new_prefix = lcfgpackage_get_prefix(new_pkg);
    if ( new_prefix != '\0' ) {

      switch(new_prefix)
	{
	case '-':
	  remove_old = true;
	  accept     = true;
	  break;
	case '+':
	case '=':
	  remove_old = true;
	  append_new = true;
	  accept     = true;
	  break;
	case '~':
	  if ( cur_pkg == NULL ) {
	    append_new = true;
	  }
	  accept = true;
	  break;
	case '?':
	  if ( cur_pkg != NULL ) {
	    remove_old = true;
	    append_new = true;
	  }
	  accept = true;
	  break;
	default:
	  *msg = lcfgpackage_build_message( new_pkg,
					    "Invalid prefix '%c'", new_prefix );
	}

      goto apply;
    }

  }

  /* If the package is not currently in the list then just append */

  if ( cur_pkg == NULL ) {
    append_new = true;
    accept     = true;
    goto apply;
  }

  /* If the package in the list is identical then replace (updates
     the derivation) */

  if ( merge_rules&LCFG_PKGS_OPT_SQUASH_IDENTICAL ) {

    if ( lcfgpackage_equals( cur_pkg, new_pkg ) ) {
      remove_old = true;
      append_new = true;
      accept     = true;
      goto apply;
    }

  }

  /* Might want to just keep everything */

  if ( merge_rules&LCFG_PKGS_OPT_KEEP_ALL ) {
    append_new = true;
    accept     = true;
    goto apply;
  }

  /* Use the priorities from the context evaluations */

  if ( merge_rules&LCFG_PKGS_OPT_USE_PRIORITY ) {

    int priority  = lcfgpackage_get_priority(new_pkg);
    int opriority = lcfgpackage_get_priority(cur_pkg);

    /* same priority for both is a conflict */

    if ( priority > opriority ) {
      remove_old = true;
      append_new = true;
      accept     = true;
    } else if ( opriority < priority ) {
      accept     = true;
    }

    goto apply;
  }

 apply:
  ;

  /* Note that is permissible for a new spec to be "accepted" without
     any changes occurring to the list */

  LCFGChange result = LCFG_CHANGE_NONE;

  if ( accept ) {

    if ( remove_old && cur_node != NULL ) {

      LCFGPackage * old_pkg = NULL;
      LCFGChange remove_rc =
        lcfgpkglist_remove_next( pkglist, prev_node, &old_pkg );

      if ( remove_rc == LCFG_CHANGE_REMOVED ) {
        lcfgpackage_destroy(old_pkg);
        result = LCFG_CHANGE_REMOVED;
      } else {
        asprintf( msg, "Failed to remove old package" );
        result = LCFG_CHANGE_ERROR;
      }

    }

    if ( append_new && result != LCFG_CHANGE_ERROR ) {
      LCFGChange append_rc = lcfgpkglist_append( pkglist, new_pkg );

      if ( append_rc == LCFG_CHANGE_ADDED ) {

        if ( result == LCFG_CHANGE_REMOVED ) {
          result = LCFG_CHANGE_REPLACED;
        } else {
          result = LCFG_CHANGE_ADDED;
        }

      } else {
        asprintf( msg, "Failed to append new package" );
        result = LCFG_CHANGE_ERROR;
      }

    }

  } else {
    result = LCFG_CHANGE_ERROR;

    if ( *msg == NULL )
      *msg = lcfgpackage_build_message( cur_pkg, "Version conflict" );

  }

  return result;
}

LCFGChange lcfgpkglist_merge_list( LCFGPackageList * pkglist1,
                                   const LCFGPackageList * pkglist2,
                                   char ** msg ) {

  *msg = NULL;

  if ( lcfgpkglist_is_empty(pkglist2) )
    return LCFG_CHANGE_NONE;

  LCFGChange change = LCFG_CHANGE_NONE;

  LCFGPackageNode * cur_node = lcfgpkglist_head(pkglist2);
  while ( cur_node != NULL ) {
    LCFGPackage * pkg = lcfgpkglist_package(cur_node);

    if ( !lcfgpackage_is_active(pkg) ) continue;

    char * merge_msg = NULL;
    LCFGChange merge_rc = lcfgpkglist_merge_package( pkglist1,
                                                     pkg,
                                                     &merge_msg );

    if ( merge_rc == LCFG_CHANGE_ERROR ) {
      change = LCFG_CHANGE_ERROR;

      *msg = lcfgpackage_build_message( pkg,
                                        "Failed to merge package lists: %s",
                                        merge_msg );

      free(merge_msg);

      break;
    } else if ( merge_rc != LCFG_CHANGE_NONE ) {
      change = LCFG_CHANGE_ADDED;
    }

    cur_node = lcfgpkglist_next(cur_node);
  }

  return change;
}

void lcfgpkglist_sort( LCFGPackageList * pkglist ) {

  if ( lcfgpkglist_is_empty(pkglist) )
    return;

  /* Oo. Oo. bubble sort .oO .oO */

  bool done = false;

  while (!done) {

    LCFGPackageNode * cur_node  = lcfgpkglist_head(pkglist);
    LCFGPackageNode * next_node = lcfgpkglist_next(cur_node);

    done = true;

    while ( next_node != NULL ) {

      LCFGPackage * cur_pkg  = lcfgpkglist_package(cur_node);
      LCFGPackage * next_pkg = lcfgpkglist_package(next_node);

      if ( lcfgpackage_compare( cur_pkg, next_pkg ) > 0 ) {
        cur_node->pkg  = next_pkg;
        next_node->pkg = cur_pkg;
        done = false;
      }

      cur_node  = next_node;
      next_node = lcfgpkglist_next(next_node);
    }
  }

}

bool lcfgpkglist_print( const LCFGPackageList * pkglist,
                        const char * defarch,
                        LCFGPkgStyle style,
                        unsigned int options,
                        FILE * out ) {

  bool ok = true;

  if ( style == LCFG_PKG_STYLE_XML )
    ok = ( fputs( "  <packages>\n", out ) >= 0 );

  LCFGPackageNode * cur_node = NULL;
  for ( cur_node = lcfgpkglist_head(pkglist);
        ok && cur_node != NULL;
        cur_node = lcfgpkglist_next(cur_node) ) {

    const LCFGPackage * pkg = lcfgpkglist_package(cur_node);

    if ( !lcfgpackage_is_active(pkg) ) continue;

    ok = lcfgpackage_print( pkg, defarch, style, options, out );

    if (!ok) break;
  }

  if ( ok && style == LCFG_PKG_STYLE_XML )
    ok = ( fputs( "  </packages>\n", out ) >= 0 );

  return ok;
}

/* Search stuff hidden here as it's a private API */

typedef char * (*LCFGPackageMatchField)(const LCFGPackage * pkg);
typedef bool (*LCFGPackageMatchFunc)( const char * field,
                                      const char * match_string );

struct LCFGPackageMatch {
  LCFGPackageMatchFunc  matcher;
  LCFGPackageMatchField fetcher;
  const char * string;
};
typedef struct LCFGPackageMatch LCFGPackageMatch;

static bool lcfgpkglist_match_exact( const char * field,
                              const char * match_string ) {
  return ( field != NULL && strcmp( field, match_string ) == 0 );
}

static bool lcfgpkglist_match_startswith( const char * field,
                                   const char * match_string ) {
  const char * match = field == NULL ? NULL : strstr( field, match_string );
  return ( match != NULL && match == field );
}

static bool lcfgpkglist_match_endswith( const char * field,
                                 const char * match_string ) {
  const char * match = field == NULL ? NULL : strstr( field, match_string );
  return ( match != NULL && strlen(match) == strlen(match_string) );
}

static bool lcfgpkglist_match_contains( const char * field,
                                 const char * match_string ) {
  const char * match = field == NULL ? NULL : strstr( field, match_string );
  return ( match != NULL );
}

static LCFGPackageMatch * lcfgpkglist_build_match( const char * field_name,
                                                   const char * match_expr ) {

  LCFGPackageMatch * match = malloc( sizeof(LCFGPackageMatch) );
  if ( match == NULL ) {
    perror( "Failed to allocate memory for LCFG package search" );
    exit(EXIT_FAILURE);
  }

  if ( strcmp( field_name, "name" ) == 0 ) {
    match->fetcher = &lcfgpackage_get_name;
  } else if ( strcmp( field_name, "arch" ) == 0 ) {
    match->fetcher = &lcfgpackage_get_arch;
  } else if ( strcmp( field_name, "version" ) == 0 ) {
    match->fetcher = &lcfgpackage_get_version;
  } else if ( strcmp( field_name, "release" ) == 0 ) {
    match->fetcher = &lcfgpackage_get_release;
  } else {
    fprintf( stderr, "Cannot search for packages with unsupported field '%s'\n", field_name );
    exit(EXIT_FAILURE);
  }

  size_t expr_len = strlen(match_expr);

  if ( match_expr[0] == '^' ) {

    if ( match_expr[expr_len-1] == '$' ) {      /* exact */
      match->matcher = &lcfgpkglist_match_exact;
      match->string  = strndup( match_expr + 1, expr_len - 2 );
    } else {                                    /* starts with */
      match->matcher = lcfgpkglist_match_startswith;
      match->string  = strdup(match_expr + 1);
    }

  } else if ( match_expr[expr_len-1] == '$' ) { /* ends with */
    match->matcher = lcfgpkglist_match_endswith;
    match->string  = strndup( match_expr, expr_len - 1 );
  } else {                                      /* contains */
    match->matcher = lcfgpkglist_match_contains;
    match->string  = strdup(match_expr);
  }

  return match;
}

static void lcfgpkglist_destroy_match( LCFGPackageMatch * match ) {
  if ( match != NULL ) {
    free( (char *) match->string );
    free(match);
    match = NULL;
  }
}

inline static bool lcfgpkglist_match_required( const char * expr ) {
  return ( expr  != NULL &&
           *expr != '\0' &&
           strcmp( expr, LCFG_PACKAGE_WILDCARD ) != 0 );
}

LCFGPackageList * lcfgpkglist_search( const LCFGPackageList * pkglist,
                                      const char * pkgname,
                                      const char * pkgarch,
                                      const char * pkgver,
                                      const char * pkgrel ) {

  LCFGPackageList * result = lcfgpkglist_new();

  if ( lcfgpkglist_size(pkglist) == 0 )
    return result;

  /* Assemble list of required matches */

  LCFGPackageMatch * matches[4] = { NULL, NULL, NULL, NULL };
  int match_id = 0;
  if ( lcfgpkglist_match_required(pkgname) ) {
    matches[match_id]   = lcfgpkglist_build_match( "name",    pkgname );
  }
  if ( lcfgpkglist_match_required(pkgarch) ) {
    matches[++match_id] = lcfgpkglist_build_match( "arch",    pkgarch );
    match_id++;
  }
  if ( lcfgpkglist_match_required(pkgver) ) {
    matches[++match_id] = lcfgpkglist_build_match( "version", pkgver );
  }
  if ( lcfgpkglist_match_required(pkgrel) ) {
    matches[++match_id] = lcfgpkglist_build_match( "release", pkgrel );
  }

  /* Run the search */

  LCFGPackageNode * cur_node = lcfgpkglist_head(pkglist);

  bool ok = true;

  while ( cur_node != NULL ) {

    LCFGPackage * pkg = lcfgpkglist_package(cur_node);

    bool matched = true;

    int i = 0;
    for ( i=0; i<=match_id; i++ ) {
      const LCFGPackageMatch * match = matches[i];
      if ( match != NULL &&
           !match->matcher( match->fetcher(pkg), match->string ) ) {
        matched = false;
        break;
      }
    }

    if ( matched ) {
      if ( lcfgpkglist_append( result, pkg ) != LCFG_CHANGE_ADDED ) {
	ok = false;
	break;
      }
    }

    cur_node = lcfgpkglist_next(cur_node);
  }

  if (ok) {
    /* Sort so that the result list is always the same */

    lcfgpkglist_sort(result);

    int i = 0;
    for ( i=0; i<=match_id; i++ ) {
      if ( matches[i] != NULL )
      lcfgpkglist_destroy_match(matches[i]);
    }

  } else {
    lcfgpkglist_destroy(result);
    result = NULL;
  }

  return result;
}

LCFGStatus lcfgpkglist_from_cpp( const char * filename,
				 LCFGPackageList ** result,
				 const char * defarch,
                                 unsigned int options,
				 char ** msg ) {

  bool include_meta = options & LCFG_OPT_USE_META;
  bool all_contexts = options & LCFG_OPT_ALL_CONTEXTS;
  bool ok = true;

  /* Variables which need to be declared ahead of any jumps to 'cleanup' */

  LCFGPackageList * pkglist = NULL;
  char * tmpfile = NULL;
  FILE * fp = NULL;

  /* Simple check to see if the file is readable at this point */

  if ( !lcfgutils_file_readable(filename) ) {
    ok = false;
    asprintf( msg, "File '%s' does not exist or is not readable",
	      filename );
    goto cleanup;
  }

  /* Temporary file for cpp output, honour any TMPDIR env variable */

  char * tmpdir = getenv("TMPDIR");
  if ( tmpdir == NULL )
    tmpdir = "/tmp";

  tmpfile = lcfgutils_catfile( tmpdir, ".lcfg.XXXXXX" );

  int tmpfd = mkstemp(tmpfile);
  if ( tmpfd == -1 ) {
    ok = false;
    asprintf( msg, "Failed to create temporary file '%s'", tmpfile );
    goto cleanup;
  }

  pid_t pid = fork();
  if ( pid == -1 ) {
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  } else if ( pid == 0 ) {

    char * cpp_cmd[] = { "cpp", "-undef", "-nostdinc", 
                         "-Wall", "-Wundef",
			 NULL, NULL, NULL, NULL, NULL };
    int i = 4;

    if ( all_contexts )
      cpp_cmd[++i] = "-DALL_CONTEXTS";

    if ( include_meta )
      cpp_cmd[++i] = "-DINCLUDE_META";

    cpp_cmd[++i] = (char *) filename; /* input */
    cpp_cmd[++i] = tmpfile;           /* output */

    execvp( cpp_cmd[0], cpp_cmd ); 

    _exit(errno); /* Not normally reached */
  }

  int status = 0;
  waitpid( pid, &status, 0 );
  if ( WIFEXITED(status) && WEXITSTATUS(status) != 0 ) {
    ok = false;
    asprintf( msg, "Failed to process '%s' using cpp",
              filename );
    goto cleanup;
  }

  fp = fdopen( tmpfd, "r" );
  if ( fp == NULL ) {
    ok = false;
    asprintf( msg, "Failed to open temporary file '%s'", tmpfile );
    goto cleanup;
  }

  /* Setup the getline buffer */

  size_t line_len = 128;
  char * line = malloc( line_len * sizeof(char) );
  if ( line == NULL ) {
    perror( "Failed to allocate memory whilst processing package list file" );
    exit(EXIT_FAILURE);
  }

  /* Results */

  pkglist = lcfgpkglist_new();

  unsigned int merge_rules = LCFG_PKGS_OPT_SQUASH_IDENTICAL;
  if (all_contexts)
    merge_rules = merge_rules | LCFG_PKGS_OPT_KEEP_ALL;

  lcfgpkglist_set_merge_rules( pkglist, merge_rules );

  char * pkg_deriv   = NULL;
  char * pkg_context = NULL;

  unsigned int linenum = 0;
  while( ok && getline( &line, &line_len, fp ) != -1 ) {
    linenum++;

    lcfgutils_trim_whitespace(line);

    if ( *line == '\0' ) continue;

    if ( *line == '#' ) {
      if ( strncmp( line, "#pragma LCFG ", 13 ) == 0 && include_meta ) {

        if ( strncmp( line + 13, "derive \"", 8 ) == 0 ) {
          free(pkg_deriv);
          const char * value = line + 13 + 8;
          size_t len = strlen(value);
          pkg_deriv = strndup( value, len - 1 ); /* Ignore final '"' */
        } else if ( strncmp( line + 13, "context \"", 9 ) == 0 ) {
          free(pkg_context);
          const char * value = line + 13 + 9;
          size_t len = strlen(value);
          pkg_context = strndup( value, len - 1 ); /* Ignore final '"' */
        }

      }

      continue;
    }

    char * error_msg = NULL;
    bool keep_package = true;

    LCFGPackage * pkg = NULL;
    LCFGStatus parse_status
      = lcfgpackage_from_string( line, &pkg, &error_msg );

    ok = ( parse_status != LCFG_STATUS_ERROR );

    if ( ok && !lcfgpackage_has_arch(pkg) && defarch != NULL ) {
      free(error_msg);
      error_msg = NULL;

      if ( !lcfgpackage_set_arch( pkg, strdup(defarch) ) ) {
	ok = false;
	asprintf( &error_msg, "Failed to set package architecture to '%s'",
		  defarch );
      }
    }

    if ( ok && include_meta ) {
      free(error_msg);
      error_msg = NULL;

      if ( ok && pkg_deriv != NULL ) {
        if ( lcfgpackage_set_derivation( pkg, pkg_deriv ) ) {
          pkg_deriv = NULL; /* Ensure memory is NOT immediately freed */
        } else {
          ok = false;
          error_msg = strdup("Invalid derivation");
        }
      }

      if ( ok && pkg_context != NULL ) {
        if ( lcfgpackage_set_context( pkg, pkg_context ) ) {
          pkg_context = NULL; /* Ensure memory is NOT immediately freed */
        } else {
          ok = false;
          error_msg = strdup("Invalid context");
        }
      }

    }

    if (ok) {
      free(error_msg);
      error_msg = NULL;

      LCFGChange merge_status =
        lcfgpkglist_merge_package( pkglist, pkg, &error_msg );

      if ( merge_status == LCFG_CHANGE_ERROR ) {
        ok = false;
      } else if ( merge_status == LCFG_CHANGE_NONE ) {
        keep_package = false;
      }

    }

    if (!ok) {
      keep_package = false;

      if ( error_msg == NULL ) {
        asprintf( msg, "Error at line %u", linenum );
      } else {
        asprintf( msg, "Error at line %u: %s", linenum, error_msg );
      }

    }

    if (!keep_package)
      lcfgpackage_destroy(pkg);

    free(error_msg);
  }

  free(pkg_deriv);
  free(pkg_context);
  free(line);

 cleanup:

  if ( fp != NULL )
    fclose(fp);

  if ( tmpfile != NULL ) {
    unlink(tmpfile);
    free(tmpfile);
  }

  if ( !ok ) {

    if ( *msg == NULL )
      asprintf( msg, "Failed to process package list file" );

    lcfgpkglist_destroy(pkglist);
    pkglist = NULL;
  }

  *result = pkglist;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/* eof */
