#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "packages.h"

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
  assert( pkglist != NULL );

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

  bool ok = true;

  LCFGPackageNode * cur_node = NULL;
  for ( cur_node = lcfgpkglist_head(pkglist);
        cur_node != NULL;
        cur_node = lcfgpkglist_next(cur_node) ) {

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
