/**
 * @file profile/diff.c
 * @brief Functions for finding the differences between LCFG profiles
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date: 2017-05-12 10:44:21 +0100 (Fri, 12 May 2017) $
 * $Revision: 32703 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "profile.h"
#include "differences.h"
#include "utils.h"

/**
 * @brief Check for differences between two profiles
 *
 * This compares the @c LCFGComponentList for the two profiles and
 * returns lists of names of components which have been removed, added
 * or modified. It does not return any details about which resources
 * have changed just that something has changed. This is done using
 * the @c lcfgcomplist_quickdiff() function.
 *
 * Note that this does NOT compare the package lists.
 *
 * This will return @c LCFG_CHANGE_MODIFIED if there are any
 * differences and @c LCFG_CHANGE_NONE otherwise.
 *
 * @param[in] profile1 Pointer to the @e old @c LCFGProfile (may be @c NULL)
 * @param[in] profile2 Pointer to the @e new @c LCFGProfile (may be @c NULL)
 * @param[out] modified Reference to pointer to @c LCFGTagList of components which are modified
 * @param[out] added Reference to pointer to @c LCFGTagList of components which are newly added
 * @param[out] removed Reference to pointer to @c LCFGTagList of components which are removed
 *
 * @return Integer value indicating type of difference
 *
 */

LCFGChange lcfgprofile_quickdiff( const LCFGProfile * profile1,
                                  const LCFGProfile * profile2,
                                  LCFGTagList ** modified,
                                  LCFGTagList ** added,
                                  LCFGTagList ** removed ) {

  const LCFGComponentList * list1 = lcfgprofile_has_components(profile1) ?
                                    lcfgprofile_get_components(profile1) : NULL;

  const LCFGComponentList * list2 = lcfgprofile_has_components(profile2) ?
                                    lcfgprofile_get_components(profile2) : NULL;

  return lcfgcomplist_quickdiff( list1,
                                 list2,
                                 modified,
                                 added,
                                 removed );

}

/**
 * @brief Find all differences between two profiles
 *
 * This takes two @c LCFGProfile and creates a new @c LCFGDiffProfile
 * to represent all the differences (if any) between the profiles.
 *
 * To avoid memory leaks, when it is no longer required the @c
 * lcfgdiffprofile_destroy() function should be called.
 *
 * @param[in] profile1 Pointer to the @e old @c LCFGProfile (may be @c NULL)
 * @param[in] profile2 Pointer to the @e new @c LCFGProfile (may be @c NULL)
 * @param[out] result Reference to pointer to the new @c LCFGDiffProfile
 *
 * @return Integer representing the type of differences
 *
 */

LCFGChange lcfgprofile_diff( const LCFGProfile * profile1,
                             const LCFGProfile * profile2,
                             LCFGDiffProfile ** result ) {

  LCFGDiffProfile * profdiff = lcfgdiffprofile_new();

  const LCFGComponentList * list1 = lcfgprofile_has_components(profile1) ?
                                    lcfgprofile_get_components(profile1) : NULL;

  const LCFGComponentList * list2 = lcfgprofile_has_components(profile2) ?
                                    lcfgprofile_get_components(profile2) : NULL;

  bool ok = true;
  bool modified = false;

  /* Look for components which have been removed or modified */

  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = list1 != NULL ? lcfgcomplist_head(list1) : NULL;
	cur_node != NULL && ok;
	cur_node = lcfgcomplist_next(cur_node) ) {

    const LCFGComponent * comp1 = lcfgcomplist_component(cur_node);
    if ( !lcfgcomponent_is_valid(comp1) ) continue;

    const char * comp1_name = lcfgcomponent_get_name(comp1);

    const LCFGComponent * comp2 =
      lcfgcomplist_find_component( list2, comp1_name );

    LCFGDiffComponent * compdiff = NULL;
    LCFGChange comp_change = lcfgcomponent_diff( comp1, comp2, &compdiff );

    if ( comp_change == LCFG_CHANGE_ERROR ) {
      ok = false;
    } else if ( comp_change != LCFG_CHANGE_NONE ) {
      modified = true;

      LCFGChange append_rc = lcfgdiffprofile_append( profdiff, compdiff );
      if ( append_rc == LCFG_CHANGE_ERROR )
        ok = false;

    }

    lcfgdiffcomponent_relinquish(compdiff);

  }

  /* Look for components which have been added */

  for ( cur_node = list2 != NULL ? lcfgcomplist_head(list2) : NULL;
	cur_node != NULL && ok;
	cur_node = lcfgcomplist_next(cur_node) ) {

    const LCFGComponent * comp2 = lcfgcomplist_component(cur_node);
    if ( !lcfgcomponent_is_valid(comp2) ) continue;

    const char * comp2_name = lcfgcomponent_get_name(comp2);

    /* Only interested in components which are NOT in first list */

    if ( !lcfgcomplist_has_component( list1, comp2_name ) ) {

      LCFGDiffComponent * compdiff = NULL;
      LCFGChange comp_change = lcfgcomponent_diff( NULL, comp2, &compdiff );

      if ( comp_change == LCFG_CHANGE_ERROR ) {
        ok = false;
      } else if ( comp_change != LCFG_CHANGE_NONE ) {
        modified = true;

	LCFGChange append_rc = lcfgdiffprofile_append( profdiff, compdiff );
	if ( append_rc == LCFG_CHANGE_ERROR )
	  ok = false;

      }

      lcfgdiffcomponent_relinquish(compdiff);

    }

  }

  LCFGChange change_type = LCFG_CHANGE_NONE;

  if ( ok ) {
    if (modified)
      change_type = LCFG_CHANGE_MODIFIED;
  } else {
    change_type = LCFG_CHANGE_ERROR;
    lcfgdiffprofile_destroy(profdiff);
    profdiff = NULL; 
  }

  *result = profdiff;

  return change_type;
}

/**
 * @brief Create and initialise a new profile diff
 *
 * Creates a new @c LCFGDiffProfile and initialises the parameters to
 * the default values.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgdiffprofile_destroy() function should be called.
 *
 * @return Pointer to new @c LCFGDiffProfile
 *
 */

LCFGDiffProfile * lcfgdiffprofile_new() {

  LCFGDiffProfile * profdiff = malloc( sizeof(LCFGDiffProfile) );
  if ( profdiff == NULL ) {
    perror( "Failed to allocate memory for LCFG profile diff" );
    exit(EXIT_FAILURE);
  }

  /* Set default values */

  profdiff->head = NULL;
  profdiff->tail = NULL;
  profdiff->size = 0;

  return profdiff;
}

/**
 * @brief Destroy the profile diff
 *
 * When the specified @c LCFGProfileDiff is no longer required this
 * will free all associated memory.
 *
 * This will call @c lcfgdiffcomponent_relinquish() on each component
 * diff and thus also calls @c lcfgresource_relinquish() for the old
 * and new resources in the @c LCFGDiffResources structures. If the
 * reference count on a resource drops to zero it will also be
 * destroyed
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * component diff which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] profdiff Pointer to @c LCFGDiffProfile to be destroyed.
 *
 */

void lcfgdiffprofile_destroy(LCFGDiffProfile * profdiff ) {

  if ( profdiff == NULL ) return;

  while ( lcfgdiffprofile_size(profdiff) > 0 ) {
    LCFGDiffComponent * compdiff = NULL;
    if ( lcfgdiffprofile_remove_next( profdiff, NULL, &compdiff )
         == LCFG_CHANGE_REMOVED ) {
      lcfgdiffcomponent_relinquish(compdiff);
    }
  }

  free(profdiff);
  profdiff = NULL;

}

/**
 * @brief Insert a component diff into the list
 *
 * This can be used to insert an @c LCFGDiffComponent into the
 * specified @c LCFGDiffProfile. The component will be wrapped into
 * an @c LCFGSListNode using the @c lcfgslistnode_new() function.
 *
 * The component diff will be inserted into the profile diff
 * immediately after the specified @c LCFGSListNode. To insert the
 * component diff at the head of the list the @c NULL value should be
 * passed for the node.
 *
 * If the component is successfully inserted the @c LCFG_CHANGE_ADDED
 * value is returned, if an error occurs then @c LCFG_CHANGE_ERROR is
 * returned.
 *
 * @param[in] list Pointer to @c LCFGDiffProfile
 * @param[in] node Pointer to @c LCFGSListNode
 * @param[in] item Pointer to @c LCFGDiffComponent
 * 
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgdiffprofile_insert_next( LCFGDiffProfile    * list,
                                        LCFGSListNode      * node,
                                        LCFGDiffComponent  * item ) {

  assert( list != NULL );

  LCFGSListNode * new_node = lcfgslistnode_new(item);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgdiffcomponent_acquire(item);

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
 * @brief Remove a component diff from the list
 *
 * This can be used to remove an @c LCFGDiffComponent from the
 * specified @c LCFGDiffProfile.
 *
 * The component diff removed from the profile diff is immediately
 * after the specified @c LCFGSListNode. To remove the component diff
 * from the head of the list the @c NULL value should be passed for
 * the node.
 *
 * If the component diff is successfully removed the @c
 * LCFG_CHANGE_REMOVED value is returned, if an error occurs then @c
 * LCFG_CHANGE_ERROR is returned. If the list is already empty then
 * the @c LCFG_CHANGE_NONE value is returned.
 *
 * Note that, since a pointer to the @c LCFGDiffComponent is returned
 * to the caller, the reference count will still be at least 1. To
 * avoid memory leaks, when the struct is no longer required it should
 * be released by calling @c lcfgdiffcomponent_relinquish().
 *
 * @param[in] list Pointer to @c LCFGDiffProfile
 * @param[in] node Pointer to @c LCFGSListNode
 * @param[out] item Pointer to @c LCFGDiffComponent
 * 
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgdiffprofile_remove_next( LCFGDiffProfile    * list,
                                        LCFGSListNode      * node,
                                        LCFGDiffComponent ** item ) {

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
 * @brief Write the profile diff to a @e hold file
 *
 * The LCFG client supports a @e secure mode which can be used to hold
 * back resource changes pending a manual review by the
 * administrator. To assist in the review process it produces a @e
 * hold file which contains a summary of all resource changes
 * (additions, removals and modifications of values). This function
 * can be used to serialise the profile diff and write it into the
 * specified file.
 *
 * @param[in] profdiff Pointer to @c LCFGDiffProfile
 * @param[in] holdfile File name to which diff should be written
 * @param[out] signature MD5 signature for the changes
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgdiffprofile_to_holdfile( LCFGDiffProfile * profdiff,
                                        const char * holdfile,
                                        char ** signature,
                                        char ** msg ) {

  if ( lcfgdiffprofile_is_empty(profdiff) ) return LCFG_STATUS_OK;

  char * tmpfile = lcfgutils_safe_tmpfile(holdfile);

  LCFGStatus status = LCFG_STATUS_OK;

  int fd = mkstemp(tmpfile);
  FILE * out = fdopen( fd, "w" );
  if ( out == NULL ) {
    lcfgutils_build_message( msg, "Failed to open temporary status file '%s'",
                             tmpfile );
    status = LCFG_STATUS_ERROR;
    goto cleanup;
  }

  /* Sort so that the order is the same each time the function is called */

  lcfgdiffprofile_sort(profdiff);

  /* Initialise the MD5 support */

  md5_state_t md5state;
  lcfgutils_md5_init(&md5state);

  /* Iterate through the list of components with differences */

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgdiffprofile_head(profdiff);
        cur_node != NULL && status != LCFG_STATUS_ERROR;
        cur_node = lcfgdiffprofile_next(cur_node) ) {

    LCFGDiffComponent * cur_compdiff = lcfgdiffprofile_compdiff(cur_node);
    if ( cur_compdiff == NULL ) continue;

    /* Sort so that the order is the same each time the function is called */

    lcfgdiffcomponent_sort(cur_compdiff);

    status = lcfgdiffcomponent_to_holdfile( cur_compdiff, out, &md5state );

    if ( status == LCFG_STATUS_ERROR ) {
      lcfgutils_build_message( msg,
                         "Failed to generate holdfile data for '%s' component",
                               lcfgdiffcomponent_get_name(cur_compdiff) );
    }

  }

  /* Store the signature into the hold file and pass it back to the caller */

  if ( status != LCFG_STATUS_ERROR ) {

    md5_byte_t digest[16];
    lcfgutils_md5_finish( &md5state, digest );

    char * hex_output = NULL;
    if ( lcfgutils_md5_hexdigest( digest, &hex_output ) ) {

      if ( fprintf( out, "signature: %s\n", hex_output ) < 0 )
        status = LCFG_STATUS_ERROR;

    } else {
      status = LCFG_STATUS_ERROR;
    }

    if ( status == LCFG_STATUS_ERROR ) {
      lcfgutils_build_message( msg, "Failed to store MD5 signature" );

      free(hex_output);
      hex_output = NULL;
    }

    *signature = hex_output;
  }

  if ( fclose(out) != 0 ) {
    lcfgutils_build_message( msg, "Failed to close hold file" );
    status = LCFG_STATUS_ERROR;
  }

  /* Finish by renaming the temporary file to the real hold file */

  if ( status != LCFG_STATUS_ERROR ) {
    if ( rename( tmpfile, holdfile ) != 0 ) {
      lcfgutils_build_message( msg,
                               "Failed to rename temporary hold file to '%s'",
                               holdfile );
      status = LCFG_STATUS_ERROR;
    }
  }

 cleanup:

  if ( tmpfile != NULL ) {
    unlink(tmpfile);
    free(tmpfile);
  }

  return status;
}

/**
 * @brief Find the list node with a given name
 *
 * This can be used to search through an @c LCFGDiffProfile to find
 * the first node which has a matching name. Note that the
 * matching is done using strcmp(3) which is case-sensitive.
 *
 * A @c NULL value is returned if no matching node is found. Also, a
 * @c NULL value is returned if a @c NULL value or an empty list is
 * specified.
 *
 * @param[in] list Pointer to @c LCFGDiffProfile to be searched
 * @param[in] want_name The name of the required node
 *
 * @return Pointer to an @c LCFGSListNode (or the @c NULL value).
 *
 */

LCFGSListNode * lcfgdiffprofile_find_node( const LCFGDiffProfile * list,
                                           const char * want_name ) {
  assert( want_name != NULL );

  if ( lcfgslist_is_empty(list) ) return NULL;

  LCFGSListNode * result = NULL;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(list);
	cur_node != NULL && result == NULL;
	cur_node = lcfgslist_next(cur_node) ) {

    const LCFGDiffComponent * item = lcfgslist_data(cur_node); 

    if ( lcfgdiffcomponent_match( item, want_name ) )
      result = (LCFGSListNode *) cur_node;

  }

  return result;
}

/**
 * @brief Find the component diff for a given name
 *
 * This can be used to search through an @c LCFGDiffProfile to find
 * the first @c LCFGDiffComponent which has a matching name. Note that
 * the matching is done using strcmp(3) which is case-sensitive.
 *
 * To ensure the returned @c LCFGDiffComponent is not destroyed when
 * the parent @c LCFGDiffProfile is destroyed you would need to call
 * the @c lcfgdiffcomponent_acquire() function.
 *
 * @param[in] list Pointer to @c LCFGDiffProfile to be searched
 * @param[in] want_name The name of the required component
 *
 * @return Pointer to an @c LCFGDiffComponent (or the @c NULL value).
 *
 */

LCFGDiffComponent * lcfgdiffprofile_find_component(const LCFGDiffProfile * list,
                                                   const char * want_name ) {
  assert( want_name != NULL );

  LCFGDiffComponent * item = NULL;

  const LCFGSListNode * node = lcfgdiffprofile_find_node( list, want_name );
  if ( node != NULL )
    item = lcfgslist_data(node);

  return item;
}

/**
 * @brief Check if a profile diff contains a particular component
 *
 * This can be used to search through an @c LCFGDiffProfile to check
 * if it contains an @c LCFGDiffComponent with a matching name. Note
 * that the matching is done using strcmp(3) which is case-sensitive.
 * 
 * This uses the @c lcfgdiffprofile_find_node() function to find the
 * relevant node. If a @c NULL value is specified for the list or the
 * list is empty then a false value will be returned.
 *
 * It is important to note that the existence of an @c
 * LCFGDiffComponent in the list is not sufficient proof that it is in
 * any way changed. To check that for a specific component diff use a
 * function like @c lcfgdiffcomponent_is_changed().
 *
 * @param[in] list Pointer to @c LCFGDiffProfile to be searched (may be @c NULL)
 * @param[in] want_name The name of the required component
 *
 * @return Boolean value which indicates presence of component in profile diff
 *
 */

bool lcfgdiffprofile_has_component( const LCFGDiffProfile * list,
				    const char * want_name ) {
  assert( want_name != NULL );

  return ( lcfgdiffprofile_find_node( list, want_name ) != NULL );
}

/**
 * @brief Sort a list of components for a profile diff
 *
 * This does an in-place sort of the nodes of the list of @c
 * LCFGDiffComponent for an @c LCFGDiffProfile by using the @c
 * lcfgdiffcomponent_compare() function.
 *
 * @param[in] list Pointer to @c LCFGDiffProfile
 *
 */

void lcfgdiffprofile_sort( LCFGDiffProfile * list ) {
  assert( list != NULL );

  if ( lcfgslist_size(list) < 2 ) return;

  /* Oo. Oo. bubble sort .oO .oO */

  bool swapped=true;
  while (swapped) {
    swapped=false;

    LCFGSListNode * cur_node = NULL;
    for ( cur_node = lcfgslist_head(list);
          cur_node != NULL && lcfgslist_has_next(cur_node);
          cur_node = lcfgslist_next(cur_node) ) {

      LCFGDiffComponent * item1 = lcfgslist_data(cur_node);
      LCFGDiffComponent * item2 = lcfgslist_data(cur_node->next);

      if ( lcfgdiffcomponent_compare( item1, item2 ) > 0 ) {
        cur_node->data       = item2;
        cur_node->next->data = item1;
        swapped = true;
      }

    }
  }

}

LCFGStatus lcfgdiffprofile_names_for_type( const LCFGDiffProfile * profdiff,
                                           LCFGChange change_type,
                                           LCFGTagList ** result ) {

  LCFGTagList * comp_names = lcfgtaglist_new();

  bool ok = true;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgdiffprofile_head(profdiff);
	cur_node != NULL && ok;
	cur_node = lcfgdiffprofile_next(cur_node) ) {

    const LCFGDiffComponent * compdiff = lcfgdiffprofile_compdiff(cur_node);

    const char * comp_name = lcfgdiffcomponent_get_name(compdiff);

    if ( comp_name != NULL && 
         change_type & lcfgdiffcomponent_get_type(compdiff) ) {

      char * msg = NULL;
      LCFGChange change = lcfgtaglist_mutate_add( comp_names, comp_name, &msg );
      if ( change == LCFG_CHANGE_ERROR )
        ok = false;

      free(msg); /* Just ignoring any message */
    }

  }

  if (ok) {
    lcfgtaglist_sort(comp_names);
  } else {
    lcfgtaglist_relinquish(comp_names);
    comp_names = NULL;
  }

  *result = comp_names;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

LCFGStatus lcfgdiffprofile_changed( const LCFGDiffProfile * profdiff,
                                    LCFGTagList ** comp_names ) {

  return lcfgdiffprofile_names_for_type( profdiff,
                    LCFG_CHANGE_ADDED|LCFG_CHANGE_REMOVED|LCFG_CHANGE_MODIFIED,
                                         comp_names );
}

LCFGStatus lcfgdiffprofile_added( const LCFGDiffProfile * profdiff,
                                  LCFGTagList ** comp_names ) {

  return lcfgdiffprofile_names_for_type( profdiff,
                                         LCFG_CHANGE_ADDED,
                                         comp_names);
}

LCFGStatus lcfgdiffprofile_removed( const LCFGDiffProfile * profdiff,
                                    LCFGTagList ** comp_names ) {

  return lcfgdiffprofile_names_for_type( profdiff,
                                         LCFG_CHANGE_REMOVED,
                                         comp_names );
}

LCFGStatus lcfgdiffprofile_modified( const LCFGDiffProfile * profdiff,
                                     LCFGTagList ** comp_names ) {

  return lcfgdiffprofile_names_for_type( profdiff,
                                         LCFG_CHANGE_MODIFIED,
                                         comp_names );
}


/* eof */
