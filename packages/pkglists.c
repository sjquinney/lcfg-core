#define _GNU_SOURCE   /* asprintf */
#define _WITH_GETLINE /* for BSD */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "packages.h"
#include "utils.h"

LCFGPackageNode * lcfgpkgnode_new(LCFGPackage * pkgspec) {

  LCFGPackageNode * pkgnode = malloc( sizeof(LCFGPackageNode) );
  if ( pkgnode == NULL ) {
    perror( "Failed to allocate memory for LCFG package node" );
    exit(EXIT_FAILURE);
  }

  pkgnode->pkgspec = pkgspec;
  pkgnode->next    = NULL;

  lcfgpackage_inc_ref(pkgspec);

  return pkgnode;
}

void lcfgpkgnode_destroy(LCFGPackageNode * pkgnode) {

  if ( pkgnode == NULL )
    return;

  /* It is intentional that the pkgspec is NOT destroyed here, that is
     handled elsewhere (e.g. lcfgpkglist_destroy) when required. */

  if ( pkgnode->pkgspec != NULL )
    lcfgpackage_dec_ref(pkgnode->pkgspec);

  pkgnode->pkgspec = NULL;
  pkgnode->next    = NULL;

  free(pkgnode);
  pkgnode = NULL;

}

LCFGPackageList * lcfgpkglist_new(void) {

  LCFGPackageList * pkglist = malloc( sizeof(LCFGPackageList) );
  if ( pkglist == NULL ) {
    perror( "Failed to allocate memory for LCFG package list" );
    exit(EXIT_FAILURE);
  }

  pkglist->size = 0;
  pkglist->head = NULL;
  pkglist->tail = NULL;

  return pkglist;
}

void lcfgpkglist_destroy(LCFGPackageList * pkglist) {

  if ( pkglist == NULL )
    return;

  LCFGPackage * pkgspec;

  while ( lcfgpkglist_size(pkglist) > 0 ) {
    if ( lcfgpkglist_remove_next( pkglist, NULL, &pkgspec )
         == LCFG_CHANGE_REMOVED ) {
      lcfgpackage_destroy(pkgspec);
    }
  }

  free(pkglist);
  pkglist = NULL;

}

LCFGChange lcfgpkglist_insert_next( LCFGPackageList * pkglist,
                                    LCFGPackageNode * pkgnode,
                                    LCFGPackage * pkgspec ) {

  LCFGPackageNode * new_node = lcfgpkgnode_new(pkgspec);
  if ( new_node == NULL )
    return LCFG_CHANGE_ERROR;

  if ( pkgnode == NULL ) { /* HEAD */

    if ( lcfgpkglist_is_empty(pkglist) )
      pkglist->tail = new_node;

    new_node->next = pkglist->head;
    pkglist->head  = new_node;

  } else {
    
    if ( pkgnode->next == NULL )
      pkglist->tail = new_node;

    new_node->next = pkgnode->next;
    pkgnode->next  = new_node;

  }

  pkglist->size++;

  return LCFG_CHANGE_ADDED;
}

LCFGChange lcfgpkglist_remove_next( LCFGPackageList * pkglist,
                                    LCFGPackageNode * pkgnode,
                                    LCFGPackage ** pkgspec ) {

  if ( lcfgpkglist_is_empty(pkglist) )
    return LCFG_CHANGE_ERROR;

  LCFGPackageNode * old_node;

  if ( pkgnode == NULL ) { /* HEAD */

    old_node = pkglist->head;
    pkglist->head = pkglist->head->next;

    if ( lcfgpkglist_size(pkglist) == 1 )
      pkglist->tail = NULL;

  } else {

    if ( pkgnode->next == NULL )
      return LCFG_CHANGE_ERROR;

    old_node = pkgnode->next;
    pkgnode->next = pkgnode->next->next;

    if ( pkgnode->next == NULL ) {
      pkglist->tail = pkgnode;
    }

  }

  pkglist->size--;

  *pkgspec = old_node->pkgspec;

  lcfgpkgnode_destroy(old_node);

  return LCFG_CHANGE_REMOVED;
}

LCFGPackageNode * lcfgpkglist_find_node( const LCFGPackageList * pkglist,
                                         const char * name,
                                         const char * arch ) {

  if ( lcfgpkglist_is_empty(pkglist) )
    return NULL;

  const char * match_arch = arch != NULL ? arch : LCFG_PACKAGE_NOVALUE;

  LCFGPackageNode * result = NULL;

  LCFGPackageNode * cur_node = lcfgpkglist_head(pkglist);
  while ( cur_node != NULL ) {

    const LCFGPackage * cur_spec = lcfgpkglist_pkgspec(cur_node);
    const char * pkg_name = lcfgpackage_get_name(cur_spec);

    if ( strcmp( pkg_name, name ) == 0 ) {
      const char * pkg_arch = lcfgpackage_has_arch(cur_spec) ?
                   lcfgpackage_get_arch(cur_spec) : LCFG_PACKAGE_NOVALUE;

      if ( strcmp( LCFG_PACKAGE_WILDCARD, match_arch ) == 0 ||
           strcmp( pkg_arch, match_arch ) == 0 ) {
        result = cur_node;
        break;
      }
    }

    cur_node = lcfgpkglist_next(cur_node);
  }

  return result;

}

LCFGPackage * lcfgpkglist_find_pkgspec( const LCFGPackageList * pkglist,
                                            const char * name,
                                            const char * arch ) {

  LCFGPackage * pkgspec = NULL;

  LCFGPackageNode * pkgnode = lcfgpkglist_find_node( pkglist, name, arch );
  if ( pkgnode != NULL )
    pkgspec = lcfgpkglist_pkgspec(pkgnode);

  return pkgspec;
}

LCFGChange lcfgpkglist_merge_pkgspec( LCFGPackageList * pkglist,
                                      LCFGPackage * new_spec,
                                      unsigned int options,
                                      char ** msg ) {

  *msg = NULL;

  /* Define these ahead of any jumps to the "apply" label */

  LCFGPackageNode * prev_node = NULL;
  LCFGPackageNode * cur_node  = NULL;
  LCFGPackage * cur_spec  = NULL;

  /* Actions */

  bool remove_old = false;
  bool append_new = false;
  bool accept     = false;

  /* Doing a search here rather than calling find_node so that the
     previous node can also be selected. That is needed for removals. */

  if ( !lcfgpackage_has_name(new_spec) ) {
    asprintf( msg, "New package does not have a name" );
    goto apply;
  }

  const char * match_name = lcfgpackage_get_name(new_spec);
  const char * match_arch = lcfgpackage_has_arch(new_spec) ?
    lcfgpackage_get_arch(new_spec) : LCFG_PACKAGE_NOVALUE;

  LCFGPackageNode * node = lcfgpkglist_head(pkglist);
  while ( node != NULL ) {

    const LCFGPackage * spec = lcfgpkglist_pkgspec(node);

    const char * name = lcfgpackage_get_name(spec);
    if ( strcmp( name, match_name ) == 0 ) {

      const char * arch = lcfgpackage_has_arch(spec) ?
	lcfgpackage_get_arch(spec) : LCFG_PACKAGE_NOVALUE;

      if ( strcmp( arch, match_arch ) == 0 ) {
	cur_node = node;
	break;
      }
    }

    prev_node = node;
    node = lcfgpkglist_next(node);
  }

  if ( cur_node != NULL ) {
    cur_spec = lcfgpkglist_pkgspec(cur_node);

    /* Merging a struct which is already in the list is a no-op. Note
       that this does not prevent the same spec appearing multiple
       times in the list if they are in different structs. */

    if ( cur_spec == new_spec ) {
      accept = true;
      goto apply;
    }
  }

  /* Might want to just keep everything */

  if ( options&LCFG_PKGS_OPT_KEEP_ALL ) {
    append_new = true;
    accept     = true;
    goto apply;
  }

  /* Apply any prefix rules */

  if ( options&LCFG_PKGS_OPT_USE_PREFIX ) {

    char cur_prefix = cur_spec != NULL ?
                      lcfgpackage_get_prefix(cur_spec) : '\0';

    if ( cur_prefix == '=' ) {
      *msg = lcfgpackage_build_message( cur_spec, "Version is pinned" );
      goto apply;
    }

    char new_prefix = lcfgpackage_get_prefix(new_spec);
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
	  if ( cur_spec == NULL ) {
	    append_new = true;
	  }
	  accept = true;
	  break;
	case '?':
	  if ( cur_spec != NULL ) {
	    remove_old = true;
	    append_new = true;
	  }
	  accept = true;
	  break;
	default:
	  *msg = lcfgpackage_build_message( new_spec,
					    "Invalid prefix '%c'", new_prefix );
	}

      goto apply;
    }

  }

  /* If the package is not currently in the list then just append */

  if ( cur_spec == NULL ) {
    append_new = true;
    accept     = true;
    goto apply;
  }

  /* If the package in the list is identical then replace (updates
     the derivation) */

  if ( options&LCFG_PKGS_OPT_SQUASH_IDENTICAL ) {

    if ( lcfgpackage_equals( cur_spec, new_spec ) ) {
      remove_old = true;
      append_new = true;
      accept     = true;
      goto apply;
    }

  }

  if ( options&LCFG_PKGS_OPT_USE_PRIORITY ) {

    int priority  = lcfgpackage_get_priority(new_spec);
    int opriority = lcfgpackage_get_priority(cur_spec);

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

      LCFGPackage * oldspec = NULL;
      LCFGChange remove_rc =
        lcfgpkglist_remove_next( pkglist, prev_node, &oldspec );

      if ( remove_rc == LCFG_CHANGE_REMOVED ) {
        lcfgpackage_destroy(oldspec);
        result = LCFG_CHANGE_REMOVED;
      } else {
        asprintf( msg, "Failed to remove old package" );
        result = LCFG_CHANGE_ERROR;
      }

    }

    if ( append_new && result != LCFG_CHANGE_ERROR ) {
      LCFGChange append_rc = lcfgpkglist_append( pkglist, new_spec );

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
      *msg = lcfgpackage_build_message( cur_spec, "Version conflict" );

  }

  return result;
}

LCFGChange lcfgpkglist_merge_list( LCFGPackageList * pkglist1,
                                   const LCFGPackageList * pkglist2,
                                   unsigned int options,
                                   char ** msg ) {

  *msg = NULL;

  if ( lcfgpkglist_is_empty(pkglist2) )
    return LCFG_CHANGE_NONE;

  LCFGChange change = LCFG_CHANGE_NONE;

  LCFGPackageNode * cur_node = lcfgpkglist_head(pkglist2);
  while ( cur_node != NULL ) {
    LCFGPackage * cur_spec = lcfgpkglist_pkgspec(cur_node);

    char * merge_msg = NULL;
    LCFGChange merge_rc = lcfgpkglist_merge_pkgspec( pkglist1,
                                                     cur_spec,
                                                     options,
                                                     &merge_msg );

    if ( merge_rc == LCFG_CHANGE_ERROR ) {
      change = LCFG_CHANGE_ERROR;

      *msg = lcfgpackage_build_message( cur_spec,
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

      LCFGPackage * cur_spec  = lcfgpkglist_pkgspec(cur_node);
      LCFGPackage * next_spec = lcfgpkglist_pkgspec(next_node);

      if ( lcfgpackage_compare( cur_spec, next_spec ) > 0 ) {
        cur_node->pkgspec  = next_spec;
        next_node->pkgspec = cur_spec;
        done = false;
      }

      cur_node  = next_node;
      next_node = lcfgpkglist_next(next_node);
    }
  }

}

bool lcfgpkglist_print( const LCFGPackageList * pkglist,
                        const char * defarch,
                        const char * style,
                        unsigned int options,
                        FILE * out ) {

  if ( lcfgpkglist_is_empty(pkglist) )
    return true;

  LCFGPackageNode * cur_node = lcfgpkglist_head(pkglist);

  bool ok = true;
  while ( cur_node != NULL ) {
    ok = lcfgpackage_print( lcfgpkglist_pkgspec(cur_node), defarch, style, options, out );

    if (!ok)
      break;

    cur_node = lcfgpkglist_next(cur_node);
  }

  return ok;
}

/* Search stuff hidden here as it's a private API */

typedef char * (*LCFGPackageMatchField)(const LCFGPackage * pkgspec);
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

    LCFGPackage * pkgspec = lcfgpkglist_pkgspec(cur_node);

    bool matched = true;

    int i = 0;
    for ( i=0; i<=match_id; i++ ) {
      const LCFGPackageMatch * match = matches[i];
      if ( match != NULL &&
           !match->matcher( match->fetcher(pkgspec), match->string ) ) {
        matched = false;
        break;
      }
    }

    if ( matched ) {
      if ( lcfgpkglist_append( result, pkgspec ) != LCFG_CHANGE_ADDED ) {
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

/* eof */
