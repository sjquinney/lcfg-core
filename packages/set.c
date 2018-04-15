/**
 * @file packages/set.c
 * @brief Functions for iterating through LCFG package sets
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "packages.h"
#include "container.h"
#include "utils.h"

static unsigned long lcfgpkgset_hash_string( const LCFGPackageSet * pkgset,
                                             const char * string ) {
  assert( pkgset != NULL );
  assert( string != NULL );

  return lcfgutils_string_djbhash( string, NULL ) % pkgset->buckets;
}

static double lcfgpkgset_load_factor( const LCFGPackageSet * pkgset ) {
  return ( (double) pkgset->entries / (double) pkgset->buckets );
}

static void lcfgpkgset_resize( LCFGPackageSet * pkgset ) {

  double load_factor = lcfgpkgset_load_factor(pkgset);

  size_t want_buckets = pkgset->buckets;
  if ( load_factor >= LCFG_PKGSET_LOAD_MAX ) {
    want_buckets = (size_t) 
      ( (double) pkgset->entries / LCFG_PKGSET_LOAD_INIT ) + 1;
  }

  LCFGPackageList ** cur_set = pkgset->packages;
  size_t cur_buckets = pkgset->buckets;

  /* Decide if a resize is actually required */

  if ( cur_set != NULL && want_buckets <= cur_buckets ) return;

  /* Resize the hash */

  LCFGPackageList ** new_set = calloc( (size_t) want_buckets,
                                       sizeof(LCFGPackageList *) );
  if ( new_set == NULL ) {
    perror( "Failed to allocate memory for LCFG package set" );
    exit(EXIT_FAILURE);
  }

  pkgset->packages = new_set;
  pkgset->entries  = 0;
  pkgset->buckets  = want_buckets;

  if ( cur_set != NULL ) {

    unsigned long i;
    for ( i=0; i<cur_buckets; i++ ) {
      LCFGPackageList * pkgs_for_name = cur_set[i];

      if ( !lcfgpkglist_is_empty(pkgs_for_name) ) {

        char * merge_msg = NULL;

        LCFGChange merge_rc = 
          lcfgpkgset_merge_list( pkgset, pkgs_for_name, &merge_msg );

        if ( merge_rc == LCFG_CHANGE_ERROR ) {
          fprintf( stderr, "Failed to resize package set: %s\n", merge_msg );
          free(merge_msg);
          exit(EXIT_FAILURE);
        }

        free(merge_msg);

      }

      lcfgpkglist_relinquish(pkgs_for_name);
      cur_set[i] = NULL;
    }

    free(cur_set);
  }

}

/**
 * @brief Create and initialise a new empty package set
 *
 * Creates a new @c LCFGPackageSet which represents an empty
 * package set.
 *
 * If the memory allocation for the new structure is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgpkgset_relinquish() function should be called.
 *
 * @return Pointer to new @c LCFGPackageSet
 *
 */

LCFGPackageSet * lcfgpkgset_new() {

  LCFGPackageSet * pkgset = malloc( sizeof(LCFGPackageSet) );
  if ( pkgset == NULL ) {
    perror( "Failed to allocate memory for LCFG package set" );
    exit(EXIT_FAILURE);
  }

  pkgset->merge_rules = LCFG_MERGE_RULE_NONE;
  pkgset->primary_key = LCFG_PKGLIST_PK_NAME | LCFG_PKGLIST_PK_ARCH;
  pkgset->packages    = NULL;
  pkgset->entries     = 0;
  pkgset->buckets     = LCFG_PKGSET_DEFAULT_SIZE;
  pkgset->_refcount   = 1;

  lcfgpkgset_resize(pkgset);

  return pkgset;
}

/**
 * @brief Destroy the package set
 *
 * When the specified @c LCFGPackageSet is no longer required this
 * will free all associated memory.
 *
 * *Reference Counting:* There is support for very simple reference
 * counting which allows an @c LCFGPackageSet to appear in multiple
 * situations. Incrementing and decrementing that reference counter is
 * the responsibility of the container code. See @c
 * lcfgpkgset_acquire() and @c lcfgpkgset_relinquish() for details.
 *
 * This will iterate through the set and call @c
 * lcfgpackage_relinquish() for each package. Note that if the
 * reference count on the package reaches zero then the @c LCFGPackage
 * will also be destroyed.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * package set which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet to be destroyed.
 *
 */

void lcfgpkgset_destroy(LCFGPackageSet * pkgset) {

  if ( pkgset == NULL ) return;

  LCFGPackageList ** packages = pkgset->packages;

  unsigned long i;
  for (i=0; i < pkgset->buckets; i++ ) {
    if ( packages[i] ) {
      lcfgpkglist_relinquish(packages[i]);
      packages[i] = NULL;
    }
  }

  free(pkgset->packages);
  pkgset->packages = NULL;

  free(pkgset);
  pkgset = NULL;
}

/**
 * @brief Acquire reference to package set
 *
 * This is used to record a reference to the @c LCFGPackageSet, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgpkgset_relinquish() function should be
 * called.
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet
 *
 */

void lcfgpkgset_acquire(LCFGPackageSet * pkgset) {
  assert( pkgset != NULL );

  pkgset->_refcount += 1;
}

/**
 * @brief Release reference to package set
 *
 * This is used to release a reference to the @c LCFGPackageSet,
 * it does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgpkgset_destroy() function
 * will be called to clean up the memory associated with the structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * package set which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet
 *
 */

void lcfgpkgset_relinquish( LCFGPackageSet * pkgset ) {

  if ( pkgset == NULL ) return;

  if ( pkgset->_refcount > 0 )
    pkgset->_refcount -= 1;

  if ( pkgset->_refcount == 0 )
    lcfgpkgset_destroy(pkgset);

}

/**
 * @brief Get the number of packages in the package set
 *
 * This is a simple function which can be used to scan through the @c
 * LCFGPackageSet to get the total number of packages.
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet
 *
 * @return Integer Number of packages in the list
 *
 */

unsigned int lcfgpkgset_size( const LCFGPackageSet * pkgset ) {
  assert( pkgset != NULL );

  /* No point scanning the whole array if there are no entries */
  if ( pkgset->entries == 0 ) return 0;

  unsigned int size = 0;

  LCFGPackageList ** packages = pkgset->packages;

  unsigned long i;
  for ( i=0; i < pkgset->buckets; i++ ) {
    if ( packages[i] )
      size += lcfgpkglist_size(packages[i]);
  }

  return size;
}

/**
 * @brief Set the package set merge rules
 *
 * A package set may have a set of rules which control how packages
 * should be 'merged' into the set when using the
 * @c lcfgpkgset_merge_package() and @c lcfgpkgset_merge_list()
 * functions. For full details, see the documentation for the
 * @c lcfgpkgset_merge_package() function. The following rules are
 * supported: 
 *
 *   - LCFG_MERGE_RULE_NONE - null rule (the default)
 *   - LCFG_MERGE_RULE_KEEP_ALL - keep all packages
 *   - LCFG_MERGE_RULE_SQUASH_IDENTICAL - ignore additional identical versions of packages
 *   - LCFG_MERGE_RULE_USE_PRIORITY - resolve conflicts using context priority value
 *   - LCFG_MERGE_RULE_USE_PREFIX - resolve conflicts using the package prefix
 *   - LCFG_MERGE_RULE_REPLACE - replae any existing package which matches
 *
 * Rules can be used in any combination by using a @c '|' (bitwise
 * 'or').
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet
 * @param[in] new_rules Integer merge rules
 *
 * @return boolean indicating success
 *
 */

bool lcfgpkgset_set_merge_rules( LCFGPackageSet * pkgset,
                                 LCFGMergeRule new_rules ) {
  assert( pkgset != NULL );

  bool ok = true;

  pkgset->merge_rules = new_rules;

  LCFGPackageList ** packages = pkgset->packages;

  unsigned long i;
  for ( i=0; ok && i < pkgset->buckets; i++ ) {
    if ( packages[i] )
      ok = lcfgpkglist_set_merge_rules( packages[i], new_rules );
  }

  return ok;
}

/**
 * @brief Get the current package set merge rules
 *
 * A package set may have a set of rules which control how packages
 * should be 'merged' into the set when using the
 * @c lcfgpkgset_merge_package() and @c lcfgpkgset_merge_list()
 * functions. For full details, see the documentation for the
 * @c lcfgpkgset_merge_package() function.
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet
 *
 * @return Integer merge rules
 *
 */

LCFGMergeRule lcfgpkgset_get_merge_rules( const LCFGPackageSet * pkgset ) {
  assert( pkgset != NULL );

  return pkgset->merge_rules;
}

/**
 * @brief Merge package into a set
 *
 * Merges an @c LCFGPackage into an existing @c LCFGPackageSet
 * according to the particular merge rules specified for the
 * set. 
 *
 * The action of merging a package into a set differs from simply
 * appending in that a search is done to check if a package with the
 * same name and architecture is already present in the set. By
 * default, with no rules specified, merging a package into a set
 * when it is already present is not permitted. This behaviour can be
 * modified in various ways, the following rules are supported (in
 * this order):
 *
 *   - LCFG_MERGE_RULE_NONE - null rule (the default)
 *   - LCFG_MERGE_RULE_USE_PREFIX - resolve conflicts using the package prefix
 *   - LCFG_MERGE_RULE_SQUASH_IDENTICAL - ignore additional identical versions of packages
 *   - LCFG_MERGE_RULE_KEEP_ALL - keep all packages
 *   - LCFG_MERGE_RULE_USE_PRIORITY - resolve conflicts using context priority val
ue
 * 
 * Rules can be used in any combination by using a @c '|' (bitwise
 * 'or'), for example @c LCFG_MERGE_RULE_SQUASH_IDENTICAL can be
 * combined with @c LCFG_MERGE_RULE_KEEP_ALL to keep all packages which
 * are not identical. The combination of rules can result in some very
 * complex scenarios so care should be take to choose the best set of
 * rules.
 *
 * A rule controls whether a change is accepted or rejected. If it is
 * accepted the change can result in the removal, addition or
 * replacement of a package. If a rule neither explicitly accepts or
 * rejects a package then the next rule in the set is applied. If no
 * rule leads to the acceptance of a change then it is rejected.
 *
 * @b Prefix: This rule uses the package prefix (if any) to resolve
 * the conflict. This can be one of the following:
 *
 *   - @c +  Add package to set, replace any existing package of same name/arch
 *   - @c =  Similar to @c + but "pins" the version so it cannot be overridden
 *   - @c -  Remove any package from set which matches this name/arch
 *   - @c ?  Replace any existing package in set which matches this name/arch
 *   - @c ~  Add package to set if name/arch is not already present
 *
 * <b>Squash identical</b>: If the packages are the same, according to the
 * @c lcfgpackage_equals() function (which compares name,
 * architecture, version, release, flags and context), then the current
 * set entry is replaced with the new one (which effectively updates
 * the derivation information).
 *
 * <b>Keep all</b>: Keep all packages (i.e. ignore any conflicts).
 *
 * <b>Use priority</b>: Compare the values of the priority which is the
 * result of evaluating the context expression (if any) for the
 * package. If the new package has a greater priority then it replaces
 * the current one. If the current has a greater priority then the new
 * package is ignored. If the priorities are the same the conflict
 * is unresolved.
 *
 * The process can successfully result in any of the following being returned:
 *
 *   - @c LCFG_CHANGE_NONE - the set is unchanged
 *   - @c LCFG_CHANGE_ADDED - the new package was added
 *   - @c LCFG_CHANGE_REMOVED - the current package was removed
 *   - @c LCFG_CHANGE_REPLACED - the current package was replaced with the new one
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet
 * @param[in] new_pkg Pointer to @c LCFGPackage to be merged into set
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgpkgset_merge_package( LCFGPackageSet * pkgset,
                                     LCFGPackage * new_pkg,
                                     char ** msg ) {
  assert( pkgset != NULL );

  if ( !lcfgpackage_is_valid(new_pkg) ) return LCFG_CHANGE_ERROR;

  const char * new_name = new_pkg->name;
  unsigned long hash = lcfgpkgset_hash_string( pkgset, new_name );

  LCFGPackageList ** packages = pkgset->packages;

  bool done = false;
  unsigned long i, slot;
  for ( i = hash; !done && i < pkgset->buckets; i++ ) {
    if ( !packages[i] ) {
      done = true;
    } else {

      const LCFGPackageList * pkglist = packages[i];

      if ( !lcfgpkglist_is_empty(pkglist) ) {
        const LCFGPackage * first_pkg = lcfgpkglist_first_package(pkglist);
        if ( lcfgpackage_match( first_pkg, new_name, "*" ) )
          done = true;
      }

    }

    if (done) slot = i;
  }

  for ( i = 0; !done && i < hash; i++ ) {
    if ( !packages[i] ) {
      done = true;
    } else {

      const LCFGPackageList * pkglist = packages[i];

      if ( !lcfgpkglist_is_empty(pkglist) ) {
        const LCFGPackage * first_pkg = lcfgpkglist_first_package(pkglist);
        if ( lcfgpackage_match( first_pkg, new_name, "*" ) )
          done = true;
      }

    }

    if (done) slot = i;
  }

  LCFGChange change = LCFG_CHANGE_NONE;
  if ( !done ) {
    lcfgutils_build_message( msg,
                             "No free space for new entries in package set" );
    change = LCFG_CHANGE_ERROR;
  } else {

    bool new_entry = false;
    LCFGPackageList * pkglist = NULL;
    if ( packages[slot] ) {
      pkglist = packages[slot];
    } else {
      new_entry = true;

      pkglist = lcfgpkglist_new();
      pkglist->merge_rules = pkgset->merge_rules;
      pkglist->primary_key = pkgset->primary_key;
    }

    change = lcfgpkglist_merge_package( pkglist, new_pkg, msg );

    if (new_entry) {
      if ( LCFGChangeOK(change) && change != LCFG_CHANGE_NONE ) {
        packages[slot] = pkglist;
        pkgset->entries += 1;
        lcfgpkgset_resize(pkgset);
      } else {
        lcfgpkglist_relinquish(pkglist);
      }
    } else if ( lcfgpkglist_is_empty(pkglist) ) {
      packages[slot] = NULL;
      pkgset->entries -= 1;
      lcfgpkglist_relinquish(pkglist);
    }

  }

  return change;
}

/**
 * @brief Merge two package sets
 *
 * Merges the packages from one set into another. The merging is done
 * according to whatever rules have been specified for the first set
 * by using the @c lcfgpkgset_merge_package() function for each
 * package in the second set. See the documentation for that function
 * for full details.
 *
 * If the set is changed then the @c LCFG_CHANGE_MODIFIED value will
 * be returned, if there is no change the @c LCFG_CHANGE_NONE value
 * will be returned. If an error occurs then the @c LCFG_CHANGE_ERROR
 * value will be returned.
 *
 * @param[in] pkgset1 Pointer to @c LCFGPackageSet into which second set is merged
 * @param[in] pkgset2 Pointer to @c LCFGPackageSet to be merged 
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgpkgset_merge_set( LCFGPackageSet * pkgset1,
                                 const LCFGPackageSet * pkgset2,
                                 char ** msg ) {
  assert( pkgset1 != NULL );

  if ( lcfgpkgset_is_empty(pkgset2) ) return LCFG_CHANGE_NONE;

  LCFGChange change = LCFG_CHANGE_NONE;

  LCFGPackageList ** packages = pkgset2->packages;

  unsigned long i;
  for ( i=0; i<pkgset2->buckets && change != LCFG_CHANGE_ERROR; i++ ) {
    LCFGPackageList * pkglist_for_name = packages[i];

    if (pkglist_for_name) {

      char * merge_msg = NULL;
      LCFGChange merge_rc = lcfgpkgset_merge_list( pkgset1,
                                                   pkglist_for_name,
                                                   &merge_msg );

      if ( merge_rc == LCFG_CHANGE_ERROR ) {
        change = LCFG_CHANGE_ERROR;
        lcfgutils_build_message( msg, "Merge failure: %s", merge_msg );
      } else if ( merge_rc != LCFG_CHANGE_NONE ) {
        change = LCFG_CHANGE_MODIFIED;
      }

      free(merge_msg);

    }

  }

  return change;
}

/**
 * @brief Merge list of packages into set
 *
 * Merges the packages from a list into a set. The merging is done
 * according to whatever rules have been specified for the set by
 * using the @c lcfgpkgset_merge_package() function for each package
 * in the list. See the documentation for that function for full
 * details.
 *
 * If the set is changed then the @c LCFG_CHANGE_MODIFIED value will
 * be returned, if there is no change the @c LCFG_CHANGE_NONE value
 * will be returned. If an error occurs then the @c LCFG_CHANGE_ERROR
 * value will be returned.
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet into which list is merged
 * @param[in] pkglist Pointer to @c LCFGPackageList to be merged 
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgpkgset_merge_list( LCFGPackageSet * pkgset,
                                  const LCFGPackageList * pkglist,
                                  char ** msg ) {

  assert( pkgset != NULL );

  if ( lcfgpkglist_is_empty(pkglist) ) return LCFG_CHANGE_NONE;

  LCFGChange change = LCFG_CHANGE_NONE;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(pkglist);
        cur_node != NULL && change != LCFG_CHANGE_ERROR;
        cur_node = lcfgslist_next(cur_node) ) {

    LCFGPackage * pkg = lcfgslist_data(cur_node);

    /* Just ignore any invalid packages */
    if ( !lcfgpackage_is_valid(pkg) ) continue;

    char * merge_msg = NULL;
    LCFGChange merge_rc = lcfgpkgset_merge_package( pkgset,
                                                    pkg,
                                                    &merge_msg );

    if ( merge_rc == LCFG_CHANGE_ERROR ) {
      change = LCFG_CHANGE_ERROR;

      *msg = lcfgpackage_build_message( pkg,
                                        "Failed to merge package: %s",
                                        merge_msg );

    } else if ( merge_rc != LCFG_CHANGE_NONE ) {
      change = LCFG_CHANGE_MODIFIED;
    }

    free(merge_msg);
  }

  return change;

}

/**
 * @brief Find all packages with a given name
 *
 * This can be used to search through an @c LCFGPackageSet to find all
 * packages which have a matching name. Note that the matching is done
 * using strcmp(3) which is case-sensitive.
 * 
 * To ensure the returned @c LCFGPackageList is not destroyed when
 * the parent @c LCFGPackageSet list is destroyed you would need to
 * call the @c lcfgpkglist_acquire() function.
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet to be searched
 * @param[in] want_name The name of the required package
 *
 * @return Pointer to an @c LCFGPackageList (or the @c NULL value).
 *
 */

LCFGPackageList * lcfgpkgset_find_list( const LCFGPackageSet * pkgset,
                                        const char * want_name ) {

  assert( want_name != NULL );

  if ( lcfgpkgset_is_empty(pkgset) ) return NULL;

  unsigned long hash =
    lcfgutils_string_djbhash( want_name, NULL ) % pkgset->buckets;

  LCFGPackageList ** packages = pkgset->packages;
  LCFGPackageList * result = NULL;

  /* Hitting an empty bucket is an immediate "failure" */
  bool failed = false;

  unsigned long i;
  for ( i = hash; result == NULL && !failed && i < pkgset->buckets; i++ ) {

    if ( !packages[i] ) {
      failed = true;
    } else {
      const LCFGPackage * head = lcfgpkglist_first_package(packages[i]);
      if ( head != NULL && lcfgpackage_match( head, want_name, "*" ) )
        result = packages[i];
    }

  }

  for ( i = 0; result == NULL && !failed && i < hash; i++ ) {

    if ( !packages[i] ) {
      failed = true;
    } else {
      const LCFGPackage * head = lcfgpkglist_first_package(packages[i]);
      if ( head != NULL && lcfgpackage_match( head, want_name, "*" ) )
        result = packages[i];
    }

  }

  return result;
}

/**
 * @brief Find the package for a given name and architecture
 *
 * This can be used to search through an @c LCFGPackageSet to find
 * the first package which has a matching name and architecture. Note
 * that the matching is done using strcmp(3) which is case-sensitive.
 * 
 * To ensure the returned @c LCFGPackage is not destroyed when the
 * parent @c LCFGPackageSet is destroyed you would need to call the @c
 * lcfgpackage_acquire() function.
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet to be searched
 * @param[in] want_name The name of the required package
 * @param[in] want_arch The architecture of the required package node
 *
 * @return Pointer to an @c LCFGPackage (or the @c NULL value).
 *
 */

LCFGPackage * lcfgpkgset_find_package( const LCFGPackageSet * pkgset,
                                       const char * want_name,
                                       const char * want_arch ) {

  assert( want_name != NULL );

  const LCFGPackageList * pkglist_for_name = lcfgpkgset_find_list( pkgset, 
                                                                   want_name );

  LCFGPackage * result = NULL;
  if ( !lcfgpkglist_is_empty(pkglist_for_name) )
    result = lcfgpkglist_find_package( pkglist_for_name, want_name, want_arch );

  return result;
}

/**
 * @brief Check if a package set contains a particular package
 *
 * This can be used to search through an @c LCFGPackageSet to check
 * if it contains a package with a matching name and
 * architecture. Note that the matching is done using strcmp(3) which
 * is case-sensitive.
 * 
 * This uses the @c lcfgpkgset_find_package() function to find the
 * relevant node. If a @c NULL value is specified for the set or the
 * set is empty then a false value will be returned.
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet to be searched
 * @param[in] want_name The name of the required package
 * @param[in] want_arch The architecture of the required package
 *
 * @return Boolean value which indicates presence of package in set
 *
 */

bool lcfgpkgset_has_package( const LCFGPackageSet * pkgset,
                             const char * want_name,
                             const char * want_arch ) {
  assert( want_name != NULL );

  return ( lcfgpkgset_find_package( pkgset, want_name, want_arch ) != NULL );
}

struct LCFGPkgSetEntry {
  const char * name;
  unsigned int id;
};

typedef struct LCFGPkgSetEntry LCFGPkgSetEntry;

static int lcfgpkgset_entry_cmp( const void * a, const void * b ) {
  const char * a_name = ( (const LCFGPkgSetEntry *) a )->name;
  const char * b_name = ( (const LCFGPkgSetEntry *) b )->name;
  return strcasecmp( a_name, b_name );
}

static unsigned int lcfgpkgset_sorted_entries( const LCFGPackageSet * pkgset,
                                               LCFGPkgSetEntry ** result ) {

   LCFGPkgSetEntry * entries =
     calloc( pkgset->buckets, sizeof(LCFGPkgSetEntry) );

   LCFGPackageList ** packages = pkgset->packages;

   unsigned long count = 0;

   unsigned long i;
   for (i=0; i < pkgset->buckets; i++ ) {
     LCFGPackageList * pkgs_for_name = packages[i];
     if ( pkgs_for_name ) {
       const LCFGPackage * pkg = lcfgpkglist_first_package(pkgs_for_name);

       if ( lcfgpackage_is_valid(pkg) ) {
         lcfgpkglist_sort(pkgs_for_name);

         unsigned int k = count++;
         entries[k].name = pkg->name;
         entries[k].id   = i;

       }
     }
   }

   if ( count > 0 ) {
     qsort( entries, count, sizeof(LCFGPkgSetEntry), lcfgpkgset_entry_cmp );
   } else {
     free(entries);
     entries = NULL;
   }

   *result = entries;

   return count;
}

/**
 * @brief Write list of formatted packages to file stream
 *
 * This uses @c lcfgpackage_to_string() to format each package as a
 * string. See the documentation for that function for full
 * details. The generated string is written to the specified file
 * stream which must have already been opened for writing.
 *
 * Packages which are invalid will be ignored.
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] base String to be prepended to all package strings
 * @param[in] style Integer indicating required style of formatting
 * @param[in] options Integer that controls formatting
 * @param[in] out Stream to which the packages should be written
 *
 * @return Boolean indicating success
 *
 */

bool lcfgpkgset_print( const LCFGPackageSet * pkgset,
                       const char * defarch,
                       const char * base,
                       LCFGPkgStyle style,
                       LCFGOption options,
                       FILE * out ) {
  assert( pkgset != NULL );

  LCFGPkgSetEntry * entries = NULL;
  unsigned int count = lcfgpkgset_sorted_entries( pkgset, &entries );

  LCFGPackageList ** packages = pkgset->packages;

  /* For RPMs the default architecture is often required. For
     efficiency, look up the default architecture only once */

  switch (style)
    {
    case LCFG_PKG_STYLE_RPM:
      options |= LCFG_OPT_NEWLINE;
      if ( defarch == NULL ) defarch = default_architecture();
      break;
    case LCFG_PKG_STYLE_SPEC:
      options |= LCFG_OPT_NEWLINE;
      break;
    case LCFG_PKG_STYLE_XML:
    case LCFG_PKG_STYLE_CPP:
    case LCFG_PKG_STYLE_SUMMARY:
    case LCFG_PKG_STYLE_EVAL:
      /* nothing to do */
      break;
    }

  bool ok = true;

  if ( style == LCFG_PKG_STYLE_XML )
    ok = ( fputs( "  <packages>\n", out ) >= 0 );

  /* Derivation information is often enormous so initialise a much
     larger buffer when that option is enabled */

  size_t buf_size = options&LCFG_OPT_USE_META ?  16384 : 512;

  char * buffer = calloc( buf_size, sizeof(char) );
  if ( buffer == NULL ) {
    perror( "Failed to allocate memory for LCFG package buffer" );
    exit(EXIT_FAILURE);
  }

  unsigned long i;
  for ( i=0; i<count && ok; i++ ) {

    unsigned int id = entries[i].id;
    const LCFGPackageList * pkgs_for_name = packages[id];

    const LCFGSListNode * cur_node = NULL;
    for ( cur_node = lcfgslist_head(pkgs_for_name);
          cur_node != NULL && ok;
          cur_node = lcfgslist_next(cur_node) ) {

      const LCFGPackage * pkg = lcfgslist_data(cur_node);

      if ( lcfgpackage_is_valid(pkg) ) {

        ssize_t rc = lcfgpackage_to_string( pkg, defarch, style, options,
                                            &buffer, &buf_size );

        if ( rc < 0 ) {
          ok = false;
        } else {

          /* Optional base string */

          if ( !isempty(base) ) {
            if ( fputs( base, out ) < 0 )
              ok = false;
          }

          /* Package string */

          if (ok) {
            if ( fputs( buffer, out ) < 0 )
              ok = false;
          }

        }
      }

    }
  }

  free(entries);
  free(buffer);

  if ( ok && style == LCFG_PKG_STYLE_XML )
    ok = ( fputs( "  </packages>\n", out ) >= 0 );

  return ok;
}

/**
 * @brief Read package set from CPP file (as used by updaterpms)
 *
 * This processes an LCFG rpmcfg package file (as used by the
 * updaterpms package manager) and generates a new @c
 * LCFGPackageSet. The file is pre-processed using the C
 * Pre-processor so the cpp tool must be available.
 *
 * An error is returned if the file does not exist or is not
 * readable. If the file exists but is empty then an empty @c
 * LCFGPackageSet is returned.
 *
 * The following options are supported:
 *   - @c LCFG_OPT_USE_META - include any metadata (contexts and derivations)
 *   - @c LCFG_OPT_ALL_CONTEXTS - include packages for all contexts
 *
 * To avoid memory leaks, when the package set is no longer required
 * the @c lcfgpkgset_relinquish() function should be called.
 *
 * @param[in] filename Path to CPP package file
 * @param[out] result Reference to pointer for new @c LCFGPackageSet
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] options Controls the behaviour of the process.
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgpkgset_from_rpmcfg( const char * filename,
                                   LCFGPackageSet ** result,
                                   const char * defarch,
                                   LCFGOption options,
                                   char ** msg ) {

  LCFGPackageSet * pkgs = lcfgpkgset_new();

  LCFGMergeRule merge_rules = LCFG_MERGE_RULE_SQUASH_IDENTICAL;
  if ( options & LCFG_OPT_ALL_CONTEXTS )
    merge_rules = merge_rules | LCFG_MERGE_RULE_KEEP_ALL;

  char ** deps = NULL;

  LCFGChange change = LCFG_CHANGE_NONE;
  if ( !lcfgpkgset_set_merge_rules( pkgs, merge_rules ) ) {
    change = LCFG_CHANGE_ERROR;
    lcfgutils_build_message( msg, "Failed to set package merge rules" );
  } else {
    LCFGPkgContainer ctr;
    ctr.set = pkgs;

    change = lcfgpackages_from_cpp( filename,
                                    &ctr, LCFG_PKG_CONTAINER_SET,
                                    defarch, NULL, NULL, options,
                                    &deps, msg );
  }

  if ( LCFGChangeOK(change) ) {

    if ( *result == NULL ) {
      *result = pkgs;
    } else {
      change = lcfgpkgset_merge_set( *result, pkgs, msg );
      lcfgpkgset_relinquish(pkgs);
    }

  } else {
    lcfgpkgset_relinquish(pkgs);
    pkgs = NULL;
  }

  /* Not interested in keeping the list of dependencies */
  char ** ptr;
  for ( ptr=deps; *ptr!=NULL; ptr++ ) free(*ptr);
  free(deps);

  return change;
}

/**
 * @brief Read package set from CPP file (as used by LCFG server)
 *
 * This processes an LCFG packages file (as used by the LCFG server)
 * and either generates a new @c LCFGPackageSet or updates an existing
 * set. Packages are merged into the set according to any prefixes
 * with any identical duplicates being squashed. Multiple instances of
 * packages (based on name/architecture) are allowed for different
 * contexts. Any conflicts resulting from this would be resolved by
 * the client by applying local context information.
 *
 * Optionally the path to a file of macros can be specified which will
 * be passed to the cpp command using the @c -imacros option. If the
 * path does not exist or is not a file it will be ignored. That file
 * can be generated using the @c lcfgpackage_store_options function.
 *
 * Optionally a list of directories may also be specified, these will
 * be passed to the cpp command using the @c -I option. Any paths
 * which do not exist or are not directories will be ignored.
 *
 * The file is pre-processed using the C Pre-Processor so the cpp tool
 * must be available.
 *
 * An error is returned if the input file does not exist or is not
 * readable.
 *
 * The following options are supported:
 *   - @c LCFG_OPT_USE_META - include any metadata (category information)
 *
 * To avoid memory leaks, when the package set is no longer required
 * the @c lcfgpkgset_relinquish() function should be called.
 *
 * @param[in] filename Path to CPP package file
 * @param[out] result Reference to pointer for @c LCFGPackageSet
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] macros_file Optional file of CPP macros (may be @c NULL)
 * @param[in] incpath Optional list of include directories for CPP (may be @c NULL)
 * @param[in] options Controls the behaviour of the process.
 * @param[out] Reference to list of file dependencies
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgpkgset_from_pkgsfile( const char * filename,
                                     LCFGPackageSet ** result,
                                     const char * defarch,
                                     const char * macros_file,
                                     char ** incpath,
                                     LCFGOption options,
                                     char *** deps,
                                     char ** msg ) {

  LCFGPackageSet * pkgs = lcfgpkgset_new();

  /* Allow multiple instances of a name/arch package for different
     contexts */

  pkgs->primary_key = LCFG_PKGLIST_PK_NAME | LCFG_PKGLIST_PK_ARCH | LCFG_PKGLIST_PK_CTX;

  LCFGMergeRule merge_rules =
    LCFG_MERGE_RULE_SQUASH_IDENTICAL | LCFG_MERGE_RULE_USE_PREFIX;

  LCFGChange change = LCFG_CHANGE_NONE;
  if ( !lcfgpkgset_set_merge_rules( pkgs, merge_rules ) ) {
    change = LCFG_CHANGE_ERROR;
    lcfgutils_build_message( msg, "Failed to set package merge rules" );
  } else {
    LCFGPkgContainer ctr;
    ctr.set = pkgs;

    change = lcfgpackages_from_cpp( filename,
				    &ctr, LCFG_PKG_CONTAINER_SET,
				    defarch, macros_file, incpath,
                                    options, deps, msg );
  }

  if ( LCFGChangeOK(change) ) {

    if ( *result == NULL ) {
      *result = pkgs;
    } else {
      change = lcfgpkgset_merge_set( *result, pkgs, msg );
      lcfgpkgset_relinquish(pkgs);
    }

  } else {
    lcfgpkgset_relinquish(pkgs);
    pkgs = NULL;
  }

  return change;
}

#include <fnmatch.h>

/**
 * @brief Search package set for all matches
 *
 * Searches the specified @c LCFGPackageSet and returns a new set
 * that contains all packages which match the specified
 * parameters. This can be used to match a package on @e name,
 * @e architecture, @e version and @e release. The matching is done using
 * the fnmatch(3) function so the @c '?' (question mark) and @c '*'
 * (asterisk) meta-characters are supported. To avoid matching on a
 * particular parameter specify the value as @c NULL.
 *
 * To avoid memory leaks, when the set of matches is no longer
 * required the @c lcfgpkgset_relinquish() function should be called.
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet
 * @param[in] want_name Package name to match (or @c NULL)
 * @param[in] want_arch Package architecture to match (or @c NULL)
 * @param[in] want_ver Package version to match (or @c NULL)
 * @param[in] want_rel Package release to match (or @c NULL)
 *
 * @return Pointer to new @c LCFGPackageSet of matches
 *
 */

LCFGPackageSet * lcfgpkgset_match( const LCFGPackageSet * pkgset,
                                   const char * want_name,
                                   const char * want_arch,
                                   const char * want_ver,
                                   const char * want_rel ) {
  assert( pkgset != NULL );

  /* Create an empty set with the same primary key type and rules that
     will ensure all unique matching packages are stored. */

  LCFGPackageSet * result = lcfgpkgset_new();
  result->primary_key = pkgset->primary_key;
  result->merge_rules =
    LCFG_MERGE_RULE_SQUASH_IDENTICAL | LCFG_MERGE_RULE_KEEP_ALL;

  LCFGPackageList ** packages = pkgset->packages;

  if ( lcfgpkgset_is_empty(pkgset) ) return result;

  /* Run the search */

  bool ok = true;

  bool all_names = isempty(want_name);

  unsigned long i;
  for ( i=0; i<pkgset->buckets && ok; i++ ) {
    LCFGPackageList * pkgs_for_name = packages[i];

    if ( !lcfgpkglist_is_empty(pkgs_for_name) ) {
      const LCFGPackage * first_pkg =
        lcfgpkglist_first_package(pkgs_for_name);

      if ( lcfgpackage_is_valid(first_pkg) && 
           ( all_names || fnmatch( want_name, first_pkg->name, 0 ) == 0 ) ) {

        LCFGPackageList * matches = lcfgpkglist_match( pkgs_for_name,
                                                       want_name,
                                                       want_arch,
                                                       want_ver,
                                                       want_rel );
        char * merge_msg = NULL;
        LCFGChange merge_rc = lcfgpkgset_merge_list( result, matches,
                                                     &merge_msg );
        free(merge_msg);

        if ( merge_rc == LCFG_CHANGE_ERROR )
          ok = false;

        lcfgpkglist_relinquish(matches);
      }

    }

  }

  if ( !ok ) {
    lcfgpkgset_destroy(result);
    result = NULL;
  }

  return result;
}

