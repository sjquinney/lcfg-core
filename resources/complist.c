
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>

#include "components.h"
#include "utils.h"

LCFGComponentNode * lcfgcomponentnode_new(LCFGComponent * comp) {
  assert( comp != NULL );

  LCFGComponentNode * compnode = malloc( sizeof(LCFGComponentNode) );
  if ( compnode == NULL ) {
    perror( "Failed to allocate memory for LCFG component node" );
    exit(EXIT_FAILURE);
  }

  compnode->component = comp;
  compnode->next      = NULL;

  return compnode;
}

void lcfgcomponentnode_destroy(LCFGComponentNode * compnode) {

  if ( compnode == NULL ) return;

  compnode->component = NULL;
  compnode->next      = NULL;

  free(compnode);
  compnode = NULL;

}

LCFGComponentList * lcfgcomplist_new(void) {

  LCFGComponentList * complist = malloc( sizeof(LCFGComponentList) );
  if ( complist == NULL ) {
    perror( "Failed to allocate memory for LCFG component list" );
    exit(EXIT_FAILURE);
  }

  complist->size = 0;
  complist->head = NULL;
  complist->tail = NULL;

  return complist;
}

void lcfgcomplist_destroy(LCFGComponentList * complist) {

  if ( complist == NULL ) return;

  while ( lcfgcomplist_size(complist) > 0 ) {
    LCFGComponent * comp = NULL;
    if ( lcfgcomplist_remove_next( complist, NULL, &comp )
         == LCFG_CHANGE_REMOVED ) {
      lcfgcomponent_relinquish(comp);
    }
  }

  free(complist);
  complist = NULL;
}

LCFGChange lcfgcomplist_insert_next( LCFGComponentList * complist,
				     LCFGComponentNode * compnode,
				     LCFGComponent     * comp ) {
  assert( complist != NULL );

  LCFGComponentNode * new_node = lcfgcomponentnode_new(comp);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgcomponent_acquire(comp);

  if ( compnode == NULL ) { /* HEAD */

    if ( lcfgcomplist_is_empty(complist) )
      complist->tail = new_node;

    new_node->next = complist->head;
    complist->head     = new_node;

  } else {
    
    if ( compnode->next == NULL )
      complist->tail = new_node;

    new_node->next = compnode->next;
    compnode->next = new_node;

  }

  complist->size++;

  return LCFG_CHANGE_ADDED;
}

LCFGChange lcfgcomplist_remove_next( LCFGComponentList * complist,
				     LCFGComponentNode * compnode,
				     LCFGComponent    ** comp ) {
  assert( complist != NULL );

  if ( lcfgcomplist_is_empty(complist) ) return LCFG_CHANGE_NONE;

  LCFGComponentNode * old_node = NULL;

  if ( compnode == NULL ) { /* HEAD */

    old_node = complist->head;
    complist->head = complist->head->next;

    if ( lcfgcomplist_size(complist) == 1 )
      complist->tail = NULL;

  } else {

    if ( compnode->next == NULL ) return LCFG_CHANGE_ERROR;

    old_node = compnode->next;
    compnode->next = compnode->next->next;

    if ( compnode->next == NULL )
      complist->tail = compnode;

  }

  complist->size--;

  *comp = lcfgcomplist_component(old_node);

  lcfgcomponentnode_destroy(old_node);

  return LCFG_CHANGE_REMOVED;
}

LCFGComponentNode * lcfgcomplist_find_node( const LCFGComponentList * complist,
                                            const char * want_name ) {
  assert( complist != NULL );

  if ( lcfgcomplist_is_empty(complist) ) return NULL;

  LCFGComponentNode * result = NULL;

  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = lcfgcomplist_head(complist);
	cur_node != NULL && result == NULL;
	cur_node = lcfgcomplist_next(cur_node) ) {

    const LCFGComponent * cur_comp = lcfgcomplist_component(cur_node);
    const char * cur_name = lcfgcomponent_get_name(cur_comp);

    if ( cur_name != NULL && strcmp( cur_name, want_name ) == 0 )
      result = (LCFGComponentNode *) cur_node;

  }

  return result;
}

bool lcfgcomplist_has_component(  const LCFGComponentList * complist,
                                  const char * name ) {
  assert( complist != NULL );

  return ( lcfgcomplist_find_node( complist, name ) != NULL );
}

LCFGComponent * lcfgcomplist_find_component( const LCFGComponentList * complist,
                                             const char * want_name ) {
  assert( complist != NULL );

  LCFGComponent * comp = NULL;

  const LCFGComponentNode * node =
    lcfgcomplist_find_node( complist, want_name );
  if ( node != NULL )
    comp = lcfgcomplist_component(node);

  return comp;
}

LCFGComponent * lcfgcomplist_find_or_create_component(
                                             LCFGComponentList * complist,
                                             const char * name ) {
  assert( complist != NULL );

  LCFGComponent * result = lcfgcomplist_find_component( complist, name );

  if ( result != NULL ) return result;

  /* If not found then create new component and add it to the list */
  result = lcfgcomponent_new();

  char * comp_name = strdup(name);
  bool ok = lcfgcomponent_set_name( result, comp_name );

  if ( !ok ) {
    free(comp_name);
  } else {

    if ( lcfgcomplist_append( complist, result ) == LCFG_CHANGE_ERROR )
      ok = false;

  }

  lcfgcomponent_relinquish(result);

  if (!ok)
    result = NULL;

  return result;
}

bool lcfgcomplist_print( const LCFGComponentList * complist,
                         LCFGResourceStyle style,
                         LCFGOption options,
                         FILE * out ) {
  assert( complist != NULL );

  if ( lcfgcomplist_is_empty(complist) ) return true;

  bool style_is_status = ( style == LCFG_RESOURCE_STYLE_STATUS );

  bool ok = true;

  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = lcfgcomplist_head(complist);
	cur_node != NULL && ok;
	cur_node = lcfgcomplist_next(cur_node) ) {

    LCFGComponent * cur_comp = lcfgcomplist_component(cur_node);

    char * msg = NULL;

    if (style_is_status) {
      /* Sort the list of resources so that the statusfile is always
	 produced in the same order - makes comparisons simpler. */

      lcfgcomponent_sort(cur_comp);

      ok = lcfgcomponent_to_statusfile( cur_comp, NULL, &msg );
    } else {
      ok = lcfgcomponent_print( cur_comp, style, options, out );
    }

    if ( msg != NULL ) {
      fprintf( stderr, "%s", msg );
      free(msg);
    }

  }

  return ok;
}

LCFGChange lcfgcomplist_insert_or_replace_component(
                                              LCFGComponentList * complist,
                                              LCFGComponent * new_comp,
                                              char ** msg ) {
  assert( complist != NULL );

  *msg = NULL;
  LCFGChange result = LCFG_CHANGE_ERROR;

  LCFGComponentNode * cur_node =
    lcfgcomplist_find_node( complist, lcfgcomponent_get_name(new_comp) );

  if ( cur_node == NULL ) {
    result = lcfgcomplist_append( complist, new_comp );
  } else {
    LCFGComponent * cur_comp = lcfgcomplist_component(cur_node);

    /* replace current version of resource with new one */

    lcfgcomponent_acquire(new_comp);
    cur_node->component = new_comp;

    lcfgcomponent_relinquish(cur_comp);

    result = LCFG_CHANGE_REPLACED;
  }

  return result;
}

LCFGStatus lcfgcomplist_apply_overrides( LCFGComponentList * list1,
                                         const LCFGComponentList * list2,
                                         char ** msg ) {
  assert( list1 != NULL );

  /* Only overriding existing components so nothing to do if list is empty */
  if ( lcfgcomplist_is_empty(list1) ) return LCFG_STATUS_OK;

  /* No overrides to apply if second list is empty */
  if ( lcfgcomplist_is_empty(list2) ) return LCFG_STATUS_OK;

  LCFGStatus status = LCFG_STATUS_OK;

  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = lcfgcomplist_head(list2);
	cur_node != NULL && status != LCFG_STATUS_ERROR;
	cur_node = lcfgcomplist_next(cur_node) ) {

    LCFGComponent * override_comp = lcfgcomplist_component(cur_node);
    const char * comp_name =  lcfgcomponent_get_name(override_comp);

    LCFGComponent * target_comp =
      lcfgcomplist_find_component( list1, comp_name );

    if ( target_comp != NULL )
      status = lcfgcomponent_apply_overrides( target_comp, override_comp, msg );

  }

  return status;
}

LCFGStatus lcfgcomplist_transplant_components( LCFGComponentList * list1,
					       const LCFGComponentList * list2,
					       char ** msg ) {
  assert( list1 != NULL );

  if ( lcfgcomplist_is_empty(list2) ) return LCFG_STATUS_OK;

  LCFGStatus status = LCFG_STATUS_OK;

  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = lcfgcomplist_head(list2);
	cur_node != NULL && status != LCFG_STATUS_ERROR;
	cur_node = lcfgcomplist_next(cur_node) ) {

    LCFGComponent * cur_comp = lcfgcomplist_component(cur_node);

    if ( lcfgcomplist_insert_or_replace_component( list1, cur_comp, msg )
         == LCFG_CHANGE_ERROR )
      status = LCFG_STATUS_ERROR;

  }

  return status;
}

LCFGStatus lcfgcomplist_from_status_dir( const char * status_dir,
                                         LCFGComponentList ** result,
                                         const LCFGTagList * comps_wanted,
                                         char ** msg ) {

  *msg = NULL;
  *result = NULL;

  if ( isempty(status_dir) ) {
    lcfgutils_build_message( msg, "Invalid status directory name" );
    return LCFG_STATUS_ERROR;
  }

  DIR * dh;
  if ( ( dh = opendir(status_dir) ) == NULL ) {

    if ( errno == ENOENT || errno == ENOTDIR ) {
      lcfgutils_build_message( msg, "Status directory '%s' does not exist", status_dir );
    } else {
      lcfgutils_build_message( msg, "Status directory '%s' is not readable", status_dir );
    }

    return LCFG_STATUS_ERROR;
  }

  LCFGStatus status = LCFG_STATUS_OK;

  LCFGComponentList * complist = lcfgcomplist_new();

  struct dirent *entry;
  struct stat sb;

  while( ( entry = readdir(dh) ) != NULL ) {

    char * comp_name = entry->d_name;

    if ( *comp_name == '.' ) continue; /* ignore any dot files */

    /* ignore any file which is not a valid component name */
    if ( !lcfgcomponent_valid_name(comp_name) ) continue;

    /* Ignore any filename which is not in the list of wanted components. */
    if ( comps_wanted != NULL &&
         !lcfgtaglist_contains( comps_wanted, comp_name ) ) {
      continue;
    }

    char * status_file  = lcfgutils_catfile( status_dir, comp_name );

    if ( stat( status_file, &sb ) == 0 && S_ISREG(sb.st_mode) ) {

      LCFGComponent * component = NULL;
      char * read_msg = NULL;
      status = lcfgcomponent_from_statusfile( status_file,
                                              &component,
                                              comp_name,
					      LCFG_OPT_NONE,
                                              &read_msg );

      if ( status == LCFG_STATUS_ERROR ) {
        lcfgutils_build_message( msg, "Failed to read status file '%s': %s",
                  status_file, read_msg );
      }

      free(read_msg);

      if ( status != LCFG_STATUS_ERROR &&
	   !lcfgcomponent_is_empty(component) ) {

        char * insert_msg = NULL;
        LCFGChange insert_rc =
          lcfgcomplist_insert_or_replace_component( complist, component,
                                                    &insert_msg );

        if ( insert_rc == LCFG_CHANGE_ERROR ) {
          status = LCFG_STATUS_ERROR;
          lcfgutils_build_message( msg, "Failed to read status file '%s': %s",
                    status_file, insert_msg );
        }

	free(insert_msg);

      }

      lcfgcomponent_relinquish(component);
    }

    free(status_file);
    status_file = NULL;

    if ( status != LCFG_STATUS_OK )
      break;
  }

  closedir(dh);

  if ( status != LCFG_STATUS_OK ) {
    lcfgcomplist_destroy(complist);
    complist = NULL;
  }

  *result = complist;

  return status;
}

LCFGStatus lcfgcomplist_to_status_dir( const LCFGComponentList * complist,
				       const char * status_dir,
				       char ** msg ) {
  assert( complist != NULL );

  if ( lcfgcomplist_is_empty(complist) ) return LCFG_STATUS_OK;

  LCFGStatus rc = LCFG_STATUS_OK;

  struct stat sb;
  if ( stat( status_dir, &sb ) != 0 ) {

    if ( errno == ENOENT ) {

      if ( mkdir( status_dir, 0700 ) != 0 ) {
	rc = LCFG_STATUS_ERROR;
	lcfgutils_build_message( msg, "Cannot write component status files into '%s', directory does not exist and cannot be created", status_dir );
      }

    } else {
      rc = LCFG_STATUS_ERROR;
      lcfgutils_build_message( msg, "Cannot write component status files into '%s', directory is not accessible", status_dir );
    }

  } else if ( !S_ISDIR(sb.st_mode) ) {
    rc = LCFG_STATUS_ERROR;
    lcfgutils_build_message( msg, "Cannot write component status files into '%s', path exists but is not a directory", status_dir );
  }

  if ( rc != LCFG_STATUS_OK ) return rc;

  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = lcfgcomplist_head(complist);
	cur_node != NULL && rc != LCFG_STATUS_ERROR;
	cur_node = lcfgcomplist_next(cur_node) ) {

    LCFGComponent * cur_comp = lcfgcomplist_component(cur_node);

    if ( !lcfgcomponent_has_name(cur_comp) ) continue;

    const char * comp_name = lcfgcomponent_get_name(cur_comp);

    char * statfile = lcfgutils_catfile( status_dir, comp_name );

    /* Sort the list of resources so that the statusfile is always
       produced in the same order - makes comparisons simpler. */

    lcfgcomponent_sort(cur_comp);

    char * comp_msg = NULL;
    rc = lcfgcomponent_to_statusfile( cur_comp, statfile, &comp_msg );

    if ( rc == LCFG_STATUS_ERROR ) {
      lcfgutils_build_message( msg, "Failed to write status file for '%s' component: %s",
		comp_name, comp_msg );
    }

    free(comp_msg);
    free(statfile);

  }

  return rc;
}

/* eof */
