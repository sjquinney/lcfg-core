#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "common.h"
#include "resources.h"
#include "reslist.h"

#define lcfgreslist_append(LIST, ITEM) ( lcfgreslist_insert_next( LIST, lcfgslist_tail(LIST), ITEM ) )

static LCFGChange lcfgreslist_remove_next( LCFGResourceList * list,
                                           LCFGSListNode    * node,
                                           LCFGResource    ** item )
   __attribute__((warn_unused_result));

static LCFGChange lcfgreslist_insert_next( LCFGResourceList * list,
                                           LCFGSListNode    * node,
                                           LCFGResource     * item )
   __attribute__((warn_unused_result));

LCFGResourceList * lcfgreslist_new(void) {

  LCFGResourceList * list = calloc( 1, sizeof(LCFGResourceList) );
  if ( list == NULL ) {
    perror( "Failed to allocate memory for LCFG resource list" );
    exit(EXIT_FAILURE);
  }

  list->merge_rules = LCFG_MERGE_RULE_NONE;
  list->primary_key = LCFG_COMP_PK_NAME;
  list->head        = NULL;
  list->tail        = NULL;
  list->size        = 0;
  list->_refcount   = 1;

  return list;
}

LCFGResourceList * lcfgreslist_clone( const LCFGResourceList * list ) {

  LCFGResourceList * clone = lcfgreslist_new();
  clone->merge_rules = list->merge_rules;
  clone->primary_key = list->primary_key;

  LCFGChange change = LCFG_CHANGE_NONE;

  /* This will result in the resources being shared between the lists */

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(list);
        cur_node != NULL && LCFGChangeOK(change);
        cur_node = lcfgslist_next(cur_node) ) {

    LCFGResource * resource = lcfgslist_data(cur_node);
    change = lcfgreslist_append( clone, resource );
  }

  if ( LCFGChangeError(change) ) {
    lcfgreslist_relinquish(clone);
    clone = NULL;
  }

  return clone;
}

static void lcfgreslist_destroy( LCFGResourceList * list ) {

  if ( list == NULL ) return;

  /* Remove head until the list is empty */

  while ( lcfgslist_size(list) > 0 ) {
    LCFGResource * resource = NULL;
    if ( lcfgreslist_remove_next( list, NULL, &resource )
         == LCFG_CHANGE_REMOVED ) {
      lcfgresource_relinquish(resource);
    }
  }

  free(list);
  list = NULL;

}

void lcfgreslist_acquire( LCFGResourceList * list ) {
  assert( list != NULL );

  list->_refcount += 1;
}

void lcfgreslist_relinquish( LCFGResourceList * list ) {

  if ( list == NULL ) return;

  if ( list->_refcount > 0 )
    list->_refcount -= 1;

  if ( list->_refcount == 0 )
    lcfgreslist_destroy(list);

}

bool lcfgreslist_is_shared( const LCFGResourceList * list ) {
  assert( list != NULL );
  return ( list->_refcount > 1 );
}

bool lcfgreslist_set_merge_rules( LCFGResourceList * list,
                                  LCFGMergeRule new_rules ) {
  assert( list != NULL );

  list->merge_rules = new_rules;

  return true;
}

static LCFGChange lcfgreslist_insert_next( LCFGResourceList * list,
                                           LCFGSListNode    * node,
                                           LCFGResource     * item ) {
  assert( list != NULL );
  assert( item != NULL );

  LCFGSListNode * new_node = lcfgslistnode_new(item);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgresource_acquire(item);

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

static LCFGChange lcfgreslist_remove_next( LCFGResourceList * list,
                                           LCFGSListNode    * node,
                                           LCFGResource    ** item ) {
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

const LCFGResource * lcfgreslist_first_resource(const LCFGResourceList * list) {

  const LCFGSListNode * head_node = lcfgslist_head(list);

  const LCFGResource * resource = NULL;
  if ( head_node != NULL )
    resource = lcfgslist_data(head_node);

  return resource;
}

const char * lcfgreslist_name( const LCFGResourceList * list ) {

  const LCFGResource * resource = lcfgreslist_first_resource(list);

  const char * name = NULL;
  if ( resource != NULL )
    name = lcfgresource_name(resource);

  return name;
}

LCFGChange lcfgreslist_merge_resource( LCFGResourceList * list,
                                       LCFGResource * new_res,
                                       char ** msg ) {
  assert( list != NULL );

  if ( !lcfgresource_is_valid(new_res) ) {
    lcfgutils_build_message( msg, "Resource is invalid" );
    return LCFG_CHANGE_ERROR;
  }

  /* Define these ahead of any jumps to the "apply" label */

  LCFGSListNode * prev_node = NULL;
  LCFGSListNode * cur_node  = NULL;
  LCFGResource * cur_res    = NULL;

  /* Doing a search here rather than calling find_node so that the
     previous node can also be selected. That is needed for removals. */

  bool ignore_context = !( list->primary_key & LCFG_COMP_PK_CTX );

  LCFGSListNode * node = NULL;
  for ( node = lcfgslist_head(list);
        node != NULL && cur_node == NULL;
        node = lcfgslist_next(node) ) {

    const LCFGResource * res = lcfgslist_data(node);

    if ( lcfgresource_same_name( res, new_res ) &&
         ( ignore_context || lcfgresource_same_context( res, new_res ) ) ) {
      cur_node = node;
      break;
    } else {
      prev_node = node; /* used later if a removal is required */
    }

  }

  LCFGMergeRule merge_rules = list->merge_rules;

  /* Actions */

  bool remove_old = false;
  bool append_new = false;
  bool accept     = false;

  /* 0. Avoid genuine duplicates */

  if ( cur_node != NULL ) {
    cur_res = lcfgslist_data(cur_node);

    /* Merging a struct which is already in the list is a no-op. Note
       that this does not prevent the same spec appearing multiple
       times in the list if they are in different structs. */

    if ( cur_res == new_res ) {
      accept = true;
      goto apply;
    }
  }

  /* 1. TODO: Apply any prefix rules (mutations) */

  /* 2. If the resource is not currently in the list then just append */

  if ( cur_res == NULL ) {
    append_new = true;
    accept     = true;
    goto apply;
  }

  /* 3. If the resource in the list is identical then replace
        (updates the derivation) */

  if ( merge_rules&LCFG_MERGE_RULE_SQUASH_IDENTICAL ) {

    if ( lcfgresource_equals( cur_res, new_res ) ) {
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

    int priority  = lcfgresource_get_priority(new_res);
    int opriority = lcfgresource_get_priority(cur_res);

    /* same priority for both is a conflict */

    if ( priority > opriority ) {
      remove_old = true;
      append_new = true;
      accept     = true;
    } else if ( priority < opriority ) {
      accept     = true; /* no change, old res is higher priority */
    }

    goto apply;
  }

 apply:
  ;

  /* Note that is permissible for a new resource to be "accepted"
     without any changes occurring to the list */

  LCFGChange result = LCFG_CHANGE_NONE;

  if ( accept ) {

    if ( remove_old && cur_node != NULL ) {

      LCFGResource * old_res = NULL;
      LCFGChange remove_rc =
        lcfgreslist_remove_next( list, prev_node, &old_res );

      if ( remove_rc == LCFG_CHANGE_REMOVED ) {
        lcfgresource_relinquish(old_res);
        result = LCFG_CHANGE_REMOVED;
      } else {
        lcfgutils_build_message( msg, "Failed to remove old resource" );
        result = LCFG_CHANGE_ERROR;
      }

    }

    if ( append_new && LCFGChangeOK(result) ) {
      LCFGChange append_rc = lcfgreslist_append( list, new_res );

      if ( append_rc == LCFG_CHANGE_ADDED ) {

        if ( result == LCFG_CHANGE_REMOVED ) {
          result = LCFG_CHANGE_REPLACED;
        } else {
          result = LCFG_CHANGE_ADDED;
        }

      } else {
        lcfgutils_build_message( msg, "Failed to append new resource" );
        result = LCFG_CHANGE_ERROR;
      }

    }

  } else {
    result = LCFG_CHANGE_ERROR;

    if ( *msg == NULL )
      *msg = lcfgresource_build_message( cur_res, NULL, "conflict" );

  }

  return result;
}

LCFGChange lcfgreslist_merge_list( LCFGResourceList * list1,
                                   const LCFGResourceList * list2,
                                   char ** msg ) {
  assert( list1 != NULL );

  LCFGChange change = LCFG_CHANGE_NONE;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(list2);
        cur_node != NULL && LCFGChangeOK(change);
        cur_node = lcfgslist_next(cur_node) ) {

    LCFGResource * resource = lcfgslist_data(cur_node);

    LCFGChange merge_rc = lcfgreslist_merge_resource( list1, resource, msg );

    if ( LCFGChangeError(merge_rc) )
      change = merge_rc;
    else if ( merge_rc != LCFG_CHANGE_NONE )
      change = LCFG_CHANGE_MODIFIED;

  }

  return change;
}

void lcfgreslist_sort_by_priority( LCFGResourceList * list ) {

  if ( lcfgslist_size(list) < 2 ) return;

  /* Oo. Oo. bubble sort .oO .oO */

  bool swapped=true;
  while (swapped) {
    swapped=false;

    LCFGSListNode * cur_node = NULL;
    for ( cur_node = lcfgslist_head(list);
          cur_node != NULL && cur_node->next != NULL;
          cur_node = lcfgslist_next(cur_node) ) {

      LCFGResource * cur_item  = lcfgslist_data(cur_node);
      LCFGResource * next_item = lcfgslist_data(cur_node->next);

      if ( cur_item->priority < next_item->priority ) {
        cur_node->data       = next_item;
        cur_node->next->data = cur_item;
        swapped = true;
      }

    }
  }

}

bool lcfgreslist_print( const LCFGResourceList * list,
                        const char * compname,
                        LCFGResourceStyle style,
                        LCFGOption options,
                        char ** buffer, size_t * buf_size,
                        FILE * out ) {

  if ( lcfgreslist_is_empty(list) ) return true;

  bool all_priorities = (options&LCFG_OPT_ALL_PRIORITIES);
  bool all_values     = (options&LCFG_OPT_ALL_VALUES);

  options |= LCFG_OPT_NEWLINE;

  bool ok   = true;
  bool done = false;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(list);
        cur_node != NULL && ok && !done;
        cur_node = lcfgslist_next(cur_node) ) {

    const LCFGResource * res = lcfgslist_data(cur_node);

    if ( all_values || lcfgresource_has_value(res) ) {

      ssize_t rc = lcfgresource_to_string( res, compname, style, options,
                                           buffer, buf_size );

      if ( rc < 0 )
        ok = false;

      if (ok) {
        if ( fputs( *buffer, out ) < 0 )
          ok = false;
      }

    }

    /* usually only need to print the head of the list */

    if ( !all_priorities )
      done = true;

  }

  return ok;
}

/* eof */
