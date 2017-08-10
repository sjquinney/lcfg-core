/**
 * @file packages/list.c
 * @brief Functions for working with lists of LCFG packages
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#define _WITH_GETLINE /* for BSD */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

#include "packages.h"
#include "utils.h"

#define lcfgpkglist_append(list, item) ( lcfgpkglist_insert_next( list, lcfgslist_tail(list), item ) )

static LCFGChange lcfgpkglist_remove_next( LCFGPackageList * list,
                                           LCFGSListNode   * node,
                                           LCFGPackage    ** item )
   __attribute__((warn_unused_result));

static LCFGChange lcfgpkglist_insert_next( LCFGPackageList * list,
                                           LCFGSListNode   * node,
                                           LCFGPackage     * item )
   __attribute__((warn_unused_result));

/**
 * @brief Create and initialise a new empty package list
 *
 * Creates a new @c LCFGPackageList which represents an empty
 * package list.
 *
 * If the memory allocation for the new structure is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgpkglist_relinquish() function should be called.
 *
 * @return Pointer to new @c LCFGPackageList
 *
 */

LCFGPackageList * lcfgpkglist_new(void) {

  LCFGPackageList * pkglist = malloc( sizeof(LCFGPackageList) );
  if ( pkglist == NULL ) {
    perror( "Failed to allocate memory for LCFG package list" );
    exit(EXIT_FAILURE);
  }

  pkglist->merge_rules = LCFG_MERGE_RULE_NONE;
  pkglist->primary_key = LCFG_PKGLIST_PK_NAME | LCFG_PKGLIST_PK_ARCH;
  pkglist->head        = NULL;
  pkglist->tail        = NULL;
  pkglist->size        = 0;
  pkglist->_refcount   = 1;

  return pkglist;
}

/**
 * @brief Destroy the package list
 *
 * When the specified @c LCFGPackageList is no longer required this
 * will free all associated memory.
 *
 * *Reference Counting:* There is support for very simple reference
 * counting which allows an @c LCFGPackageList to appear in multiple
 * situations. This is particular useful for code which needs to use
 * multiple iterators for a single list. Incrementing and decrementing
 * that reference counter is the responsibility of the container
 * code. See @c lcfgpkglist_acquire() and @c lcfgpkglist_relinquish()
 * for details.
 *
 * This will iterate through the list to remove and destroy each
 * @c LCFGSListNode item, it also calls @c lcfgpackage_relinquish()
 * for each package. Note that if the reference count on the package
 * reaches zero then the @c LCFGPackage will also be destroyed.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * package list which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList to be destroyed.
 *
 */

void lcfgpkglist_destroy(LCFGPackageList * pkglist) {

  if ( pkglist == NULL ) return;

  while ( lcfgslist_size(pkglist) > 0 ) {
    LCFGPackage * pkg = NULL;
    if ( lcfgpkglist_remove_next( pkglist, NULL, &pkg )
         == LCFG_CHANGE_REMOVED ) {
      lcfgpackage_relinquish(pkg);
    }
  }

  free(pkglist);
  pkglist = NULL;

}

/**
 * @brief Acquire reference to package list
 *
 * This is used to record a reference to the @c LCFGPackageList, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgpkglist_release() function should be
 * called.
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 *
 */

void lcfgpkglist_acquire( LCFGPackageList * pkglist ) {
  assert( pkglist != NULL );

  pkglist->_refcount += 1;
}

/**
 * @brief Release reference to package list
 *
 * This is used to release a reference to the @c LCFGPackageList,
 * it does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgpkglist_destroy() function
 * will be called to clean up the memory associated with the structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * package list which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 *
 */

void lcfgpkglist_relinquish( LCFGPackageList * pkglist ) {

  if ( pkglist == NULL ) return;

  if ( pkglist->_refcount > 0 )
    pkglist->_refcount -= 1;


  if ( pkglist->_refcount == 0 )
    lcfgpkglist_destroy(pkglist);

}

/**
 * @brief Set the package list merge rules
 *
 * A package list may have a set of rules which control how packages
 * should be 'merged' into the list when using the
 * @c lcfgpkglist_merge_package() and @c lcfgpkglist_merge_list()
 * functions. For full details, see the documentation for the
 * @c lcfgpkglist_merge_package() function. The following rules are
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
 * @param[in] pkglist Pointer to @c LCFGPackageList
 * @param[in] new_rules Integer merge rules
 *
 * @return boolean indicating success
 *
 */

bool lcfgpkglist_set_merge_rules( LCFGPackageList * pkglist,
				  LCFGMergeRule new_rules ) {
  assert( pkglist != NULL );

  pkglist->merge_rules = new_rules;

  return true;
}

/**
 * @brief Get the current package list merge rules
 *
 * A package list may have a set of rules which control how packages
 * should be 'merged' into the list when using the
 * @c lcfgpkglist_merge_package() and @c lcfgpkglist_merge_list()
 * functions. For full details, see the documentation for the
 * @c lcfgpkglist_merge_package() function.
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 *
 * @return Integer merge rules
 *
 */

LCFGMergeRule lcfgpkglist_get_merge_rules( const LCFGPackageList * pkglist ) {
  assert( pkglist != NULL );

  return pkglist->merge_rules;
}

/**
 * @brief Insert a package into the list
 *
 * This can be used to insert an @c LCFGPackage into the
 * specified package list. The package will be wrapped into an
 * @c LCFGSListNode using the @c lcfgpkgnode_new() function.
 *
 * The package will be inserted into the list immediately after the
 * specified @c LCFGSListNode. To insert the package at the
 * head of the list the @c NULL value should be passed for the node.
 *
 * If the package is successfully inserted into the list the
 * @c LCFG_CHANGE_ADDED value is returned, if an error occurs then
 * @c LCFG_CHANGE_ERROR is returned.
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 * @param[in] pkgnode Pointer to @c LCFGSListNode
 * @param[in] pkg Pointer to @c LCFGPackage
 * 
 * @return Integer value indicating type of change
 *
 */

static LCFGChange lcfgpkglist_insert_next( LCFGPackageList * list,
                                           LCFGSListNode   * node,
                                           LCFGPackage     * item ) {
  assert( list != NULL );
  assert( item != NULL );

  LCFGSListNode * new_node = lcfgslistnode_new(item);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgpackage_acquire(item);

  if ( node == NULL ) { /* HEAD */

    if ( lcfgslist_is_empty(list) )
      list->tail = new_node;

    new_node->next = list->head;
    list->head     = new_node;

  } else {

    if ( node->next == NULL )
      list->tail = new_node;

    new_node->next = node->next;
    node->next     = new_node;

  }

  list->size++;

  return LCFG_CHANGE_ADDED;
}

/**
 * @brief Remove a package from the list
 *
 * This can be used to remove an @c LCFGPackage from the specified
 * package list.
 *
 * The package removed from the list is immediately after the
 * specified @c LCFGSListNode. To remove the package from the
 * head of the list the @c NULL value should be passed for the node.
 *
 * If the package is successfully removed from the list the
 * @c LCFG_CHANGE_REMOVED value is returned, if an error occurs then
 * @c LCFG_CHANGE_ERROR is returned. If the list is already empty then
 * the @c LCFG_CHANGE_NONE value is returned.
 *
 * Note that, since a pointer to the @c LCFGPackage is returned
 * to the caller, the reference count will still be at least 1. To
 * avoid memory leaks, when the struct is no longer required it should
 * be released by calling @c lcfgpackage_relinquish().
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 * @param[in] pkgnode Pointer to @c LCFGSListNode
 * @param[out] pkg Pointer to @c LCFGPackage
 * 
 * @return Integer value indicating type of change
 *
 */

static LCFGChange lcfgpkglist_remove_next( LCFGPackageList * list,
                                           LCFGSListNode   * node,
                                           LCFGPackage    ** item ) {
  assert( list != NULL );

  if ( lcfgslist_is_empty(list) ) return LCFG_CHANGE_NONE;

  LCFGSListNode * old_node = NULL;

  if ( node == NULL ) { /* HEAD */

    old_node   = list->head;
    list->head = list->head->next;

    if ( lcfgslist_size(list) == 1 )
      list->tail = NULL;

  } else {

    if ( node->next == NULL ) return LCFG_CHANGE_ERROR;

    old_node   = node->next;
    node->next = node->next->next;

    if ( node->next == NULL )
      list->tail = node;

  }

  list->size--;

  *item = lcfgslist_data(old_node);

  lcfgslistnode_destroy(old_node);

  return LCFG_CHANGE_REMOVED;
}

/**
 * @brief Find the package list node with a given name and architecture
 *
 * This can be used to search through an @c LCFGPackageList to find
 * the first package node which has a matching name. Note that the
 * matching is done using strcmp(3) which is case-sensitive.
 * 
 * The value for the architecture can be set to the @c '*' (asterisk)
 * wildcard character. In which case the first package node which
 * matches the specified name and any architecture will be
 * returned. If the architecture is set to @c NULL or the empty string
 * then only a package @b without a value for the architecture will be
 * matched.
 *
 * A @c NULL value is returned if no matching node is found. Also, a
 * @c NULL value is returned if a @c NULL value or an empty list is
 * specified.
 *
 * @param[in] list Pointer to @c LCFGPackageList to be searched
 * @param[in] name The name of the required package node
 * @param[in] arch The architecture of the required package node
 *
 * @return Pointer to an @c LCFGSListNode (or the @c NULL value).
 *
 */

LCFGSListNode * lcfgpkglist_find_node( const LCFGPackageList * list,
                                       const char * name,
                                       const char * arch ) {

  assert( name != NULL );

  if ( lcfgslist_is_empty(list) ) return NULL;

  LCFGSListNode * result = NULL;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(list);
        cur_node != NULL && result == NULL;
        cur_node = lcfgslist_next(cur_node) ) {
        
    const LCFGPackage * item = lcfgslist_data(cur_node);

    if ( !lcfgpackage_is_valid(item) ) continue;

    if ( lcfgpackage_match( item, name, arch ) )
      result = (LCFGSListNode *) cur_node;

  }

  return result;
}

/**
 * @brief Find the package for a given name and architecture
 *
 * This can be used to search through an @c LCFGPackageList to find
 * the first package which has a matching name and architecture. Note
 * that the matching is done using strcmp(3) which is case-sensitive.
 * 
 * This uses the @c lcfgpkglist_find_node() to find the relevant node
 * and it behaves in a similar fashion so a @c NULL value is returned
 * if no matching node is found. Also, a @c NULL value is returned if
 * a @c NULL value or an empty list is specified.
 *
 * To ensure the returned @c LCFGPackage is not destroyed when
 * the parent @c LCFGPackageList list is destroyed you would need to
 * call the @c lcfgpackage_acquire() function.
 *
 * @param[in] list Pointer to @c LCFGPackageList to be searched
 * @param[in] name The name of the required package
 * @param[in] arch The architecture of the required package node
 *
 * @return Pointer to an @c LCFGPackage (or the @c NULL value).
 *
 */

LCFGPackage * lcfgpkglist_find_package( const LCFGPackageList * list,
                                        const char * name,
                                        const char * arch ) {
  assert( name != NULL );

  LCFGPackage * item = NULL;

  const LCFGSListNode * node = lcfgpkglist_find_node( list, name, arch );
  if ( node != NULL )
    item = lcfgslist_data(node);

  return item;
}

/**
 * @brief Check if a package list contains a particular package
 *
 * This can be used to search through an @c LCFGPackageList to check
 * if it contains a package with a matching name and
 * architecture. Note that the matching is done using strcmp(3) which
 * is case-sensitive.
 * 
 * This uses the @c lcfgpkglist_find_node() function to find the
 * relevant node. If a @c NULL value is specified for the list or the
 * list is empty then a false value will be returned.
 *
 * @param[in] list Pointer to @c LCFGPackageList to be searched
 * @param[in] name The name of the required package
 * @param[in] arch The architecture of the required package node
 *
 * @return Boolean value which indicates presence of package in list
 *
 */

bool lcfgpkglist_has_package( const LCFGPackageList * list,
                              const char * name,
                              const char * arch ) {
  assert( name != NULL );

  return ( lcfgpkglist_find_node( list, name, arch ) != NULL );
}

static bool same_context( const LCFGPackage * pkg1, const LCFGPackage * pkg2 ) {

  const char * ctx1 = or_default( pkg1->context, "" );
  const char * ctx2 = or_default( pkg2->context, "" );

  return ( strcmp( ctx1, ctx2 ) == 0 );
}

/**
 * @brief Merge package into a list
 *
 * Merges an @c LCFGPackage into an existing @c LCFGPackageList
 * according to the particular merge rules specified for the
 * list. 
 *
 * The action of merging a package into a list differs from simply
 * appending in that a search is done to check if a package with the
 * same name and architecture is already present in the list. By
 * default, with no rules specified, merging a package into a list
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
 * rejects a package then the next rule in the list is applied. If no
 * rule leads to the acceptance of a change then it is rejected.
 *
 * @b Prefix: This rule uses the package prefix (if any) to resolve
 * the conflict. This can be one of the following:
 *
 *   - @c +  Add package to list, replace any existing package of same name/arch
 *   - @c =  Similar to @c + but "pins" the version so it cannot be overridden
 *   - @c -  Remove any package from list which matches this name/arch
 *   - @c ?  Replace any existing package in list which matches this name/arch
 *   - @c ~  Add package to list if name/arch is not already present
 *
 * <b>Squash identical</b>: If the packages are the same, according to the
 * @c lcfgpackage_equals() function (which compares name,
 * architecture, version, release, flags and context), then the current
 * list entry is replaced with the new one (which effectively updates
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
 *   - @c LCFG_CHANGE_NONE - the list is unchanged
 *   - @c LCFG_CHANGE_ADDED - the new package was added
 *   - @c LCFG_CHANGE_REMOVED - the current package was removed
 *   - @c LCFG_CHANGE_REPLACED - the current package was replaced with the new one
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 * @param[in] new_pkg Pointer to @c LCFGPackage to be merged into list
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgpkglist_merge_package( LCFGPackageList * pkglist,
                                      LCFGPackage * new_pkg,
                                      char ** msg ) {
  assert( pkglist != NULL );

  if ( !lcfgpackage_is_valid(new_pkg) ) return LCFG_CHANGE_ERROR;

  /* Define these ahead of any jumps to the "apply" label */

  LCFGSListNode * prev_node = NULL;
  LCFGSListNode * cur_node  = NULL;
  LCFGPackage * cur_pkg  = NULL;

  /* Doing a search here rather than calling find_node so that the
     previous node can also be selected. That is needed for removals. */

  const char * match_name = new_pkg->name;
  const char * match_arch = LCFG_PACKAGE_WILDCARD;
  if ( pkglist->primary_key&LCFG_PKGLIST_PK_ARCH )
    match_arch = or_default( new_pkg->arch, "" );

  LCFGSListNode * node = NULL;
  for ( node = lcfgslist_head(pkglist);
        node != NULL && cur_node == NULL;
        node = lcfgslist_next(node) ) {

    const LCFGPackage * pkg = lcfgslist_data(node);

    if ( !lcfgpackage_is_valid(pkg) ) continue;

    if ( lcfgpackage_match( pkg, match_name, match_arch ) &&
         ( !(pkglist->primary_key&LCFG_PKGLIST_PK_CTX) ||
           same_context( pkg, new_pkg ) ) ) {
      cur_node = node;
      break;
    } else {
      prev_node = node; /* used later if a removal is required */
    }

  }

  LCFGMergeRule merge_rules = lcfgpkglist_get_merge_rules(pkglist);

  /* Actions */

  bool remove_old = false;
  bool append_new = false;
  bool accept     = false;

  /* 0. Avoid genuine duplicates */

  if ( cur_node != NULL ) {
    cur_pkg = lcfgslist_data(cur_node);

    /* Merging a struct which is already in the list is a no-op. Note
       that this does not prevent the same spec appearing multiple
       times in the list if they are in different structs. */

    if ( cur_pkg == new_pkg ) {
      accept = true;
      goto apply;
    }
  }

  /* 1. Apply any prefix rules */

  if ( merge_rules&LCFG_MERGE_RULE_USE_PREFIX ) {

    char cur_prefix = cur_pkg != NULL ? cur_pkg->prefix : '\0';

    if ( cur_prefix == '=' ) {
      *msg = lcfgpackage_build_message( cur_pkg, "Version is pinned" );
      goto apply;
    }

    char new_prefix = new_pkg->prefix;
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

  /* 2. If the package is not currently in the list then just append */

  if ( cur_pkg == NULL ) {
    append_new = true;
    accept     = true;
    goto apply;
  }

  /* 3. If the package in the list is identical then replace (updates
     the derivation) */

  if ( merge_rules&LCFG_MERGE_RULE_SQUASH_IDENTICAL ) {

    if ( lcfgpackage_equals( cur_pkg, new_pkg ) ) {
      remove_old = true;
      append_new = true;
      accept     = true;
      goto apply;
    }

  }

  /* 4. Might want to just keep everything */

  if ( merge_rules&LCFG_MERGE_RULE_KEEP_ALL ) {
    append_new = true;
    accept     = true;
    goto apply;
  }

  /* 5. Just replace existing with new */

  if ( merge_rules&LCFG_MERGE_RULE_REPLACE ) {
      remove_old = true;
      append_new = true;
      accept     = true;
      goto apply;
  }

  /* 6. Use the priorities from the context evaluations */

  if ( merge_rules&LCFG_MERGE_RULE_USE_PRIORITY ) {

    int priority  = new_pkg->priority;
    int opriority = cur_pkg->priority;

    /* same priority for both is a conflict */

    if ( priority > opriority ) {
      remove_old = true;
      append_new = true;
      accept     = true;
    } else if ( priority < opriority ) {
      accept     = true; /* no change, old pkg is higher priority */
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
        lcfgpackage_relinquish(old_pkg);
        result = LCFG_CHANGE_REMOVED;
      } else {
        lcfgutils_build_message( msg, "Failed to remove old package" );
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
        lcfgutils_build_message( msg, "Failed to append new package" );
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

/**
 * @brief Merge two package lists
 *
 * Merges the packages from one list into another. The merging is done
 * according to whatever rules have been specified for the first list
 * by using the @c lcfgpkglist_merge_package() function for each
 * package in the second list. See the documentation for that function
 * for full details.
 *
 * If the list is changed then the @c LCFG_CHANGE_MODIFIED value will
 * be returned, if there is no change the @c LCFG_CHANGE_NONE value
 * will be returned. If an error occurs then the @c LCFG_CHANGE_ERROR
 * value will be returned.
 *
 * @param[in] pkglist1 Pointer to @c LCFGPackageList into which second list is merged
 * @param[in] pkglist2 Pointer to @c LCFGPackageList to be merged 
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgpkglist_merge_list( LCFGPackageList * pkglist1,
                                   const LCFGPackageList * pkglist2,
                                   char ** msg ) {
  assert( pkglist1 != NULL );

  if ( lcfgslist_is_empty(pkglist2) ) return LCFG_CHANGE_NONE;

  LCFGChange change = LCFG_CHANGE_NONE;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(pkglist2);
        cur_node != NULL && change != LCFG_CHANGE_ERROR;
        cur_node = lcfgslist_next(cur_node) ) {

    LCFGPackage * pkg = lcfgslist_data(cur_node);

    /* Just ignore any invalid packages */
    if ( !lcfgpackage_is_valid(pkg) ) continue;

    char * merge_msg = NULL;
    LCFGChange merge_rc = lcfgpkglist_merge_package( pkglist1,
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
 * @brief Sort a list of packages
 *
 * This sorts the nodes of the @c LCFGPackageList by using the 
 * @c lcfgpackage_compare() function.
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 *
 */

void lcfgpkglist_sort( LCFGPackageList * pkglist ) {

  if ( lcfgslist_size(pkglist) < 2 ) return;

  /* Oo. Oo. bubble sort .oO .oO */

  bool swapped=true;
  while (swapped) {
    swapped=false;

    LCFGSListNode * cur_node = NULL;
    for ( cur_node = lcfgslist_head(pkglist);
          cur_node != NULL && cur_node->next != NULL;
          cur_node = lcfgslist_next(cur_node) ) {

      LCFGPackage * cur_pkg  = lcfgslist_data(cur_node);
      LCFGPackage * next_pkg = lcfgslist_data(cur_node->next);

      if ( lcfgpackage_compare( cur_pkg, next_pkg ) > 0 ) {
        cur_node->data       = next_pkg;
        cur_node->next->data = cur_pkg;
        swapped = true;
      }

    }
  }

}

/**
 * @brief Write list of formatted packages to file stream
 *
 * This uses @c lcfgpackage_to_string() to format each package as a
 * string. See the documentation for that function for full
 * details. The generated string is written to the specified file
 * stream which must have already been opened for writing.
 *
 * Packages which are invalid will be ignored. Packages which are
 * inactive (i.e. they have a negative priority) will also be ignored
 * unless the @c LCFG_OPT_ALL_PRIORITIES option is specified.
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] style Integer indicating required style of formatting
 * @param[in] options Integer that controls formatting
 * @param[in] out Stream to which the package list should be written
 *
 * @return Boolean indicating success
 *
 */

bool lcfgpkglist_print( const LCFGPackageList * pkglist,
                        const char * defarch,
                        LCFGPkgStyle style,
                        LCFGOption options,
                        FILE * out ) {
  assert( pkglist != NULL );

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

  size_t buf_size = 256;
  char * buffer = calloc( buf_size, sizeof(char) );
  if ( buffer == NULL ) {
    perror( "Failed to allocate memory for LCFG resource buffer" );
    exit(EXIT_FAILURE);
  }

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(pkglist);
        cur_node != NULL && ok;
        cur_node = lcfgslist_next(cur_node) ) {

    const LCFGPackage * pkg = lcfgslist_data(cur_node);

    if ( lcfgpackage_is_valid(pkg) ) {

      ssize_t rc = lcfgpackage_to_string( pkg, defarch, style, options,
                                          &buffer, &buf_size );

      if ( rc < 0 ) {
        ok = false;
      } else {

        if ( fputs( buffer, out ) < 0 )
          ok = false;

      }

    }
  }

  free(buffer);

  if ( ok && style == LCFG_PKG_STYLE_XML )
    ok = ( fputs( "  </packages>\n", out ) >= 0 );

  return ok;
}

/**
 * @brief Read package list from CPP file (as used by updaterpms)
 *
 * This processes an LCFG rpmcfg package file (as used by the
 * updaterpms package manager) and generates a new @c
 * LCFGPackageList. The file is pre-processed using the C
 * Pre-processor so the cpp tool must be available.
 *
 * An error is returned if the file does not exist or is not
 * readable. If the file exists but is empty then an empty @c
 * LCFGPackageList is returned.
 *
 * The following options are supported:
 *   - @c LCFG_OPT_USE_META - include any metadata (contexts and derivations)
 *   - @c LCFG_OPT_ALL_CONTEXTS - include packages for all contexts
 *
 * To avoid memory leaks, when the package list is no longer required
 * the @c lcfgpkglist_relinquish() function should be called.
 *
 * @param[in] filename Path to CPP package file
 * @param[out] result Reference to pointer for new @c LCFGPackageList
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] options Controls the behaviour of the process.
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgpkglist_from_cpp( const char * filename,
				 LCFGPackageList ** result,
				 const char * defarch,
                                 LCFGOption options,
				 char ** msg ) {

  if ( isempty(filename) ) {
    lcfgutils_build_message( msg, "Invalid filename" );
    return LCFG_STATUS_ERROR;
  }

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
    lcfgutils_build_message( msg, "File '%s' does not exist or is not readable",
	      filename );
    goto cleanup;
  }

  /* Temporary file for cpp output */

  tmpfile = lcfgutils_safe_tmpfile(NULL);

  int tmpfd = mkstemp(tmpfile);
  if ( tmpfd == -1 ) {
    ok = false;
    lcfgutils_build_message( msg, "Failed to create temporary file '%s'", tmpfile );
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
    lcfgutils_build_message( msg, "Failed to process '%s' using cpp",
              filename );
    goto cleanup;
  }

  fp = fdopen( tmpfd, "r" );
  if ( fp == NULL ) {
    ok = false;
    lcfgutils_build_message( msg, "Failed to open temporary file '%s'", tmpfile );
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

  LCFGMergeRule merge_rules = LCFG_MERGE_RULE_SQUASH_IDENTICAL;
  if (all_contexts)
    merge_rules = merge_rules | LCFG_MERGE_RULE_KEEP_ALL;

  if ( !lcfgpkglist_set_merge_rules( pkglist, merge_rules ) ) {
    ok = false;
    lcfgutils_build_message( msg, "Failed to set package merge rules" );
    goto cleanup;
  }

  char * pkg_deriv   = NULL;
  char * pkg_context = NULL;

  unsigned int linenum = 0;
  while( ok && getline( &line, &line_len, fp ) != -1 ) {
    linenum++;

    lcfgutils_string_trim(line);

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

    LCFGPackage * pkg = NULL;
    LCFGStatus parse_status
      = lcfgpackage_from_spec( line, &pkg, &error_msg );

    ok = ( parse_status != LCFG_STATUS_ERROR );

    if ( ok && !lcfgpackage_has_arch(pkg) && !isempty(defarch) ) {
      free(error_msg);
      error_msg = NULL;

      char * pkg_arch = strdup(defarch);
      if ( !lcfgpackage_set_arch( pkg, pkg_arch ) ) {
        free(pkg_arch);
	ok = false;
	lcfgutils_build_message( &error_msg,
                                 "Failed to set package architecture to '%s'",
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
          lcfgutils_build_message( &error_msg, "Invalid derivation '%s'",
                                   pkg_deriv );
        }
      }

      if ( ok && pkg_context != NULL ) {
        if ( lcfgpackage_set_context( pkg, pkg_context ) ) {
          pkg_context = NULL; /* Ensure memory is NOT immediately freed */
        } else {
          ok = false;
          lcfgutils_build_message( &error_msg, "Invalid context '%s'",
                                   pkg_context );
        }
      }

    }

    if (ok) {
      free(error_msg);
      error_msg = NULL;

      LCFGChange merge_status =
        lcfgpkglist_merge_package( pkglist, pkg, &error_msg );

      if ( merge_status == LCFG_CHANGE_ERROR )
        ok = false;

    }

    if (!ok) {

      if ( error_msg == NULL )
        lcfgutils_build_message( msg, "Error at line %u", linenum );
      else
        lcfgutils_build_message( msg, "Error at line %u: %s", linenum, error_msg );

    }

    lcfgpackage_relinquish(pkg);

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
      lcfgutils_build_message( msg, "Failed to process package list file" );

    lcfgpkglist_destroy(pkglist);
    pkglist = NULL;
  }

  *result = pkglist;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

#include <fnmatch.h>

/**
 * @brief Search package list for all matches
 *
 * Searches the specified @c LCFGPackageList and returns a new list
 * that contains all packages which match the specified
 * parameters. This can be used to match a package on @e name,
 * @e architecture, @e version and @e release. The matching is done using
 * the fnmatch(3) function so the @c '?' (question mark) and @c '*'
 * (asterisk) meta-characters are supported. To avoid matching on a
 * particular parameter specify the value as @c NULL.
 *
 * To avoid memory leaks, when the list of matches is no longer
 * required the @c lcfgpkglist_relinquish() function should be called.
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 * @param[in] name Package name to match (or @c NULL)
 * @param[in] arch Package architecture to match (or @c NULL)
 * @param[in] ver Package version to match (or @c NULL)
 * @param[in] rel Package release to match (or @c NULL)
 *
 * @return Pointer to new @c LCFGPackageList of matches
 *
 */

LCFGPackageList * lcfgpkglist_match( const LCFGPackageList * pkglist,
                                     const char * name,
                                     const char * arch,
                                     const char * ver,
                                     const char * rel ) {
  assert( pkglist != NULL );

  LCFGPackageList * result = lcfgpkglist_new();

  if ( lcfgslist_is_empty(pkglist) ) return result;

  /* Run the search */

  bool ok = true;

  LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(pkglist);
        cur_node != NULL && ok;
        cur_node = lcfgslist_next(cur_node) ) {

    LCFGPackage * pkg = lcfgslist_data(cur_node);

    bool matched = true;

    if ( matched && !isempty(name) ) {
      const char * pkg_name = lcfgpackage_get_name(pkg);
      matched = ( !isempty(pkg_name) && fnmatch( name, pkg_name, 0 ) == 0 );
    }

    if ( matched && !isempty(arch) ) {
      const char * pkg_arch = lcfgpackage_get_arch(pkg);
      matched = ( !isempty(pkg_arch) && fnmatch( arch, pkg_arch, 0 ) == 0 );
    }

    if ( matched && !isempty(ver) ) {
      const char * pkg_ver = lcfgpackage_get_version(pkg);
      matched = ( !isempty(pkg_ver) && fnmatch( ver, pkg_ver, 0 ) == 0 );
    }

    if ( matched && !isempty(rel) ) {
      const char * pkg_rel = lcfgpackage_get_release(pkg);
      matched = ( !isempty(pkg_rel) && fnmatch( rel, pkg_rel, 0 ) == 0 );
    }

    if ( matched ) {
      LCFGChange append_rc = lcfgpkglist_append( result, pkg );
      if ( append_rc == LCFG_CHANGE_ERROR )
        ok = false;
    }

  }

  if ( !ok ) {
    lcfgpkglist_destroy(result);
    result = NULL;
  }

  return result;
}

/**
 * @brief Retrieve first package in list
 *
 * Provides easy access to the first @c LCFGPackage in the @c
 * LCFGPackageList. If the list is empty this will return a @c NULL
 * value.
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 *
 * @return Pointer to first @c LCFGPackage in list
 *
 */

LCFGPackage * lcfgpkglist_first_package( const LCFGPackageList * pkglist ) {

  const LCFGSListNode * first_node = lcfgslist_head(pkglist);

  LCFGPackage * first = NULL;
  if ( first_node != NULL )
    first = lcfgslist_data(first_node);

  return first;
}

/* eof */
