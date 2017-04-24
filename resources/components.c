#define _GNU_SOURCE /* for asprintf */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "components.h"
#include "utils.h"

/* Used when creating environment variables from resources */

static const char * default_val_pfx  = "LCFG_%s_";
static const char * default_type_pfx = "LCFGTYPE_%s_";
static const char * env_placeholder  = "%s";
static const char * reslist_keyname  = "_RESOURCES";

LCFGResourceNode * lcfgresourcenode_new(LCFGResource * res) {
  assert( res != NULL );

  LCFGResourceNode * resnode = malloc( sizeof(LCFGResourceNode) );
  if ( resnode == NULL ) {
    perror( "Failed to allocate memory for LCFG resource node" );
    exit(EXIT_FAILURE);
  }

  /* Set default values */

  resnode->resource = res;
  resnode->next     = NULL;

  return resnode;
}

void lcfgresourcenode_destroy(LCFGResourceNode * resnode) {

  if ( resnode == NULL ) return;

  resnode->resource = NULL;
  resnode->next     = NULL;

  free(resnode);
  resnode = NULL;

}

LCFGComponent * lcfgcomponent_new(void) {

  LCFGComponent * comp = malloc( sizeof(LCFGComponent) );
  if ( comp == NULL ) {
    perror( "Failed to allocate memory for LCFG component" );
    exit(EXIT_FAILURE);
  }

  /* Set default values */

  comp->name = NULL;
  comp->size = 0;
  comp->head = NULL;
  comp->tail = NULL;
  comp->_refcount = 1;

  return comp;
}

void lcfgcomponent_destroy(LCFGComponent * comp) {

  if ( comp == NULL ) return;

  while ( lcfgcomponent_size(comp) > 0 ) {
    LCFGResource * resource = NULL;
    if ( lcfgcomponent_remove_next( comp, NULL, &resource )
         == LCFG_CHANGE_REMOVED ) {
      lcfgresource_relinquish(resource);
    }
  }

  free(comp->name);
  comp->name = NULL;

  free(comp);
  comp = NULL;

}

void lcfgcomponent_acquire(LCFGComponent * comp) {
  assert( comp != NULL );

  comp->_refcount += 1;
}

void lcfgcomponent_relinquish(LCFGComponent * comp) {

  if ( comp == NULL ) return;

  if ( comp->_refcount > 0 )
    comp->_refcount -= 1;

  if ( comp->_refcount == 0 )
    lcfgcomponent_destroy(comp);

}

bool lcfgcomponent_valid_name(const char * name) {
  return lcfgresource_valid_name(name);
}

bool lcfgcomponent_has_name(const LCFGComponent * comp) {
  assert( comp != NULL );

  return !isempty(comp->name);
}

char * lcfgcomponent_get_name(const LCFGComponent * comp) {
  assert( comp != NULL );

  return comp->name;
}

bool lcfgcomponent_set_name( LCFGComponent * comp, char * new_name ) {
  assert( comp != NULL );

  bool ok = false;
  if ( lcfgcomponent_valid_name(new_name) ) {
    free(comp->name);

    comp->name = new_name;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

LCFGChange lcfgcomponent_insert_next( LCFGComponent    * comp,
                                      LCFGResourceNode * resnode,
                                      LCFGResource     * res ) {
  assert( comp != NULL );

  LCFGResourceNode * new_node = lcfgresourcenode_new(res);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgresource_acquire(res);

  if ( resnode == NULL ) { /* HEAD */

    if ( lcfgcomponent_is_empty(comp) )
      comp->tail = new_node;

    new_node->next = comp->head;
    comp->head     = new_node;

  } else {
    
    if ( resnode->next == NULL )
      comp->tail = new_node;

    new_node->next = resnode->next;
    resnode->next  = new_node;

  }

  comp->size++;

  return LCFG_CHANGE_ADDED;
}

LCFGChange lcfgcomponent_remove_next( LCFGComponent    * comp,
                                      LCFGResourceNode * resnode,
                                      LCFGResource    ** res ) {
  assert( comp != NULL );

  if ( lcfgcomponent_is_empty(comp) ) return LCFG_CHANGE_NONE;

  LCFGResourceNode * old_node = NULL;

  if ( resnode == NULL ) { /* HEAD */

    old_node   = comp->head;
    comp->head = comp->head->next;

    if ( lcfgcomponent_size(comp) == 1 )
      comp->tail = NULL;

  } else {

    if ( resnode->next == NULL ) return LCFG_CHANGE_ERROR;

    old_node = resnode->next;
    resnode->next = resnode->next->next;

    if ( resnode->next == NULL )
      comp->tail = resnode;

  }

  comp->size--;

  *res = lcfgcomponent_resource(old_node);

  lcfgresourcenode_destroy(old_node);

  return LCFG_CHANGE_REMOVED;
}

bool lcfgcomponent_print( const LCFGComponent * comp,
                          LCFGResourceStyle style,
                          LCFGOption options,
                          FILE * out ) {
  assert( comp != NULL );

  if ( lcfgcomponent_is_empty(comp) ) return true;

  bool all_priorities = (options&LCFG_OPT_ALL_PRIORITIES);
  bool all_values     = (options&LCFG_OPT_ALL_VALUES);

  options |= LCFG_OPT_NEWLINE;

  const char * comp_name = lcfgcomponent_get_name(comp);

  char * val_pfx  = NULL;
  char * type_pfx = NULL;
  bool print_export = false;

  LCFGTagList * export_res = NULL;
  if ( style == LCFG_RESOURCE_STYLE_EXPORT ) {
    print_export = true;
    val_pfx  = lcfgutils_string_replace( default_val_pfx,
                                         env_placeholder, comp_name );
    type_pfx = lcfgutils_string_replace( default_type_pfx,
                                         env_placeholder, comp_name );

    export_res = lcfgtaglist_new();
  }

  bool ok = true;

  size_t buf_size = 256;
  char * buffer = calloc( buf_size, sizeof(char) );
  if ( buffer == NULL ) {
    perror( "Failed to allocate memory for LCFG resource buffer" );
    exit(EXIT_FAILURE);
  }

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
	cur_node != NULL && ok;
	cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res = lcfgcomponent_resource(cur_node);

    /* Not interested in resources for inactive contexts. Only print
       resources without values if the print_all option is specified */

    if ( ( all_values     || lcfgresource_has_value(res) ) &&
         ( all_priorities || lcfgresource_is_active(res) ) ) {

      ssize_t rc;
      if ( print_export ) {
        rc = lcfgresource_to_export( res, val_pfx, type_pfx, options,
                                     &buffer, &buf_size );

        /* Stash the resource name so we can create an env variable
           which holds the list of names. */

        if ( rc > 0 ) {
          const char * name = lcfgresource_get_name(res);
          char * add_msg = NULL;
          LCFGChange change = 
            lcfgtaglist_mutate_add( export_res, name, add_msg );
          if ( change == LCFG_CHANGE_ERROR )
            ok = false;

          free(add_msg);
        }

      } else {
        rc = lcfgresource_to_string( res, comp_name, style, options,
                                     &buffer, &buf_size );
      }

      if ( rc < 0 )
        ok = false;

      if (ok) {
        if ( fputs( buffer, out ) < 0 )
          ok = false;
      }

    }
  }

  /* Export style also needs a list of resource names for the component */

  if ( ok && print_export ) {

    lcfgtaglist_sort(export_res);

    char * reslist = NULL;
    size_t bufsize = 0;
    ssize_t len = lcfgtaglist_to_string( export_res, LCFG_OPT_NONE,
                                         &reslist, &bufsize );
    if ( len < 0 ) {
      ok = false;
    } else {
      int rc = fprintf( out, "export %s%s='%s'\n", val_pfx, reslist_keyname, reslist );
      if ( rc < 0 )
        ok = false;
    }

    free(reslist);
  }

  free(buffer);

  lcfgtaglist_relinquish(export_res);
  free(val_pfx);
  free(type_pfx);

  return ok;
}

void lcfgcomponent_sort( LCFGComponent * comp ) {
  assert( comp != NULL );

  if ( lcfgcomponent_size(comp) < 2 ) return;

  /* Oo. Oo. bubble sort .oO .oO */

  bool swapped=true;
  while (swapped) {
    swapped=false;

    LCFGResourceNode * cur_node = NULL;
    for ( cur_node = lcfgcomponent_head(comp);
          cur_node != NULL && cur_node->next != NULL;
          cur_node = lcfgcomponent_next(cur_node) ) {

      LCFGResource * cur_res  = lcfgcomponent_resource(cur_node);
      LCFGResource * next_res = lcfgcomponent_resource(cur_node->next);

      if ( lcfgresource_compare( cur_res, next_res ) > 0 ) {
        cur_node->resource       = next_res;
        cur_node->next->resource = cur_res;
        swapped = true;
      }

    }
  }

}

LCFGStatus lcfgcomponent_from_statusfile( const char * filename,
                                          LCFGComponent ** result,
                                          const char * compname_in,
                                          char ** msg ) {

  *result = NULL;
  *msg = NULL;

  LCFGComponent * comp = NULL;
  bool ok = true;

  /* Need a copy of the component name to insert into the
     LCFGComponent struct */

  char * compname = NULL;
  if ( compname_in != NULL ) {
    compname = strdup(compname_in);
  } else {
    if ( filename != NULL ) {
      compname = lcfgutils_basename( filename, NULL );
    } else {
      ok = false;
      lcfgutils_build_message( msg, "Either the component name or status file path MUST be specified" );
      goto cleanup;
    }
  }

  /* Create the new empty component which will eventually be returned */

  comp = lcfgcomponent_new();
  if ( !lcfgcomponent_set_name( comp, compname ) ) {
    ok = false;
    lcfgutils_build_message( msg, "Invalid name for component '%s'", compname );

    free(compname);

    goto cleanup;
  }

  const char * statusfile = filename != NULL ? filename : compname;

  FILE *fp;
  if ( (fp = fopen(statusfile, "r")) == NULL ) {
    ok = false;

    if (errno == ENOENT) {
      lcfgutils_build_message( msg, "Component status file '%s' does not exist",
			       statusfile );
    } else {
      lcfgutils_build_message( msg, "Component status file '%s' is not readable",
			       statusfile );
    }

    goto cleanup;
  }

  size_t line_len = 128;
  char * line = malloc( line_len * sizeof(char) );
  if ( line == NULL ) {
    perror( "Failed to allocate memory for status parser buffer" );
    exit(EXIT_FAILURE);
  }

  char * statusline = NULL;

  int linenum = 1;
  while( getline( &line, &line_len, fp ) != -1 ) {

    free(statusline);
    statusline = strdup(line);
    if ( statusline == NULL ) {
      perror( "Failed to copy status line" );
      exit(EXIT_FAILURE);
    }

    lcfgutils_chomp(statusline);

    /* Search for the '=' which separates status keys and values */

    char * sep = strchr( statusline, '=' );
    if ( sep == NULL ) {
      lcfgutils_build_message( msg, "Failed to parse line %d (missing '=' character)",
                linenum );
      ok = false;
      break;
    }

    /* Replace the '=' separator with a NULL so that can avoid
       unnecessarily dupping the string */

    *sep = '\0';

    /* The value is everything after the separator (could just be an
       empty string) */

    char * status_value = sep + 1;

    /* Find the component name (if any) and the resource name */

    char * this_hostname = NULL;
    char * this_compname = NULL;
    char * this_resname  = NULL;
    char this_type       = LCFG_RESOURCE_SYMBOL_VALUE;

    if ( !lcfgresource_parse_key( statusline, &this_hostname, &this_compname,
                                  &this_resname, &this_type ) ) {
      lcfgutils_build_message( msg, "Failed to parse line %d (invalid key '%s')",
                linenum, statusline );
      ok = false;
      break;
    }

    /* Check for valid resource name */

    if ( !lcfgresource_valid_name(this_resname) ) {
      lcfgutils_build_message( msg, "Failed to parse line %d (invalid resource name '%s')",
                linenum, this_resname );
      ok = false;
      break;
    }

    /* Insist on the component names matching */

    if ( this_compname != NULL && strcmp( this_compname, compname ) != 0 ) {
      lcfgutils_build_message( msg, "Failed to parse line %d (invalid component name '%s')",
                linenum, this_compname );
      ok = false;
      break;
    }

    /* Grab the resource or create a new one if necessary */

    LCFGResource * res =
      lcfgcomponent_find_or_create_resource( comp, this_resname );

    if ( res == NULL ) {
      lcfgutils_build_message( msg, "Failed to parse line %d of status file '%s'",
		linenum, statusfile );
      ok = false;
      break;
    }

    /* Apply the action which matches with the symbol at the start of
       the status line or assume this is a simple specification of the
       resource value. */

    char * this_value = strdup(status_value);

    /* Value strings may be html encoded as they can contain
       whitespace characters which would otherwise corrupt the status
       file formatting. */

    if ( this_type == LCFG_RESOURCE_SYMBOL_VALUE )
      lcfgutils_decode_html_entities_utf8( this_value, NULL );

    char * set_msg = NULL;
    bool set_ok = lcfgresource_set_attribute( res, this_type, this_value,
                                              &set_msg );

    if ( !set_ok ) {

      if ( set_msg != NULL ) {
        lcfgutils_build_message( msg, "Failed to process line %d (%s)",
                  linenum, set_msg );

        free(set_msg);
      } else {
        lcfgutils_build_message( msg, 
                  "Failed to process line %d (bad value '%s' for type '%c')",
                  linenum, status_value, this_type );
      }

      free(this_value);

      ok = false;
      break;
    }

    linenum++;
  }

  free(statusline);

  fclose(fp);

  free(line);

 cleanup:

  if ( !ok ) {
    lcfgcomponent_destroy(comp);
    comp = NULL;
  }

  *result = comp;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

LCFGStatus lcfgcomponent_to_env( const LCFGComponent * comp,
				 const char * val_pfx, const char * type_pfx,
				 LCFGOption options,
                                 char ** msg ) {
  assert( comp != NULL );

  if ( lcfgcomponent_is_empty(comp) ) return LCFG_STATUS_OK;
  if ( !lcfgcomponent_has_name(comp) ) return LCFG_STATUS_ERROR;

  bool all_priorities = (options&LCFG_OPT_ALL_PRIORITIES);
  bool all_values     = (options&LCFG_OPT_ALL_VALUES);

  LCFGStatus status = LCFG_STATUS_OK;

  const char * comp_name = lcfgcomponent_get_name(comp);
  size_t name_len = strlen(comp_name);

  if ( val_pfx == NULL )
    val_pfx = default_val_pfx;

  if ( type_pfx == NULL )
    type_pfx = default_type_pfx;

  /* For security avoid using the user-specified prefix as a format string. */

  char * val_pfx2 = NULL;
  if ( strstr( val_pfx, env_placeholder ) != NULL ) {
    val_pfx2 = lcfgutils_string_replace( val_pfx,
					 env_placeholder, comp_name );
    val_pfx = val_pfx2;
  }

  char * type_pfx2 = NULL;

  /* No point doing this if the type data isn't required */
  if ( options&LCFG_OPT_USE_META ) {
    if ( strstr( type_pfx, env_placeholder ) != NULL ) {
      type_pfx2 = lcfgutils_string_replace( type_pfx,
					    env_placeholder, comp_name );
      type_pfx = type_pfx2;
    }
  }

  LCFGTagList * export_res = lcfgtaglist_new();

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
	cur_node != NULL && status != LCFG_STATUS_ERROR;
	cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res = lcfgcomponent_resource(cur_node);

    if ( ( all_values     || lcfgresource_has_value(res) ) &&
         ( all_priorities || lcfgresource_is_active(res) ) ) {

      status = lcfgresource_to_env( res, val_pfx, type_pfx, options );

      if ( status == LCFG_STATUS_ERROR ) {
        *msg = lcfgresource_build_message( res, comp_name,
                           "Failed to set environment variable for resource" );
      } else {
        const char * res_name = lcfgresource_get_name(res);

        char * add_msg = NULL;
        LCFGChange add_rc = lcfgtaglist_mutate_add( export_res, res_name,
                                                    &add_msg );
        if ( add_rc == LCFG_CHANGE_ERROR )
          status = LCFG_STATUS_ERROR;

        free(add_msg);
      }
    }

  }

  if ( status != LCFG_STATUS_ERROR ) {

    /* Also create an environment variable which holds list of
       resource names for this component. */

    char * reslist_key = NULL;
    int rc = asprintf( &reslist_key, "%s%s", val_pfx, reslist_keyname );
    if ( rc < 0 ) {
      perror("Failed to build component environment variable name");
      exit(EXIT_FAILURE);
    }

    lcfgtaglist_sort(export_res);

    char * reslist_value = NULL;
    char * bufsize = 0;
    ssize_t len = lcfgtaglist_to_string( export_res, LCFG_OPT_NONE,
                                         &reslist_value, &bufsize );
    if ( len < 0 ) {
      status = LCFG_STATUS_ERROR;
    } else {
      if ( setenv( reslist_key, reslist_value, 1 ) != 0 )
        status = LCFG_STATUS_ERROR;
    }

    free(reslist_key);
    free(reslist_value);

  }

  lcfgtaglist_relinquish(export_res);
  free(val_pfx2);
  free(type_pfx2);

  return status;
}

LCFGStatus lcfgcomponent_to_statusfile( const LCFGComponent * comp,
                                        const char * filename,
                                        char ** msg ) {
  assert( comp != NULL );

  const char * compname = lcfgcomponent_get_name(comp);

  if ( filename == NULL && compname == NULL ) {
    lcfgutils_build_message( msg, "Either the target file name or component name is required" );
    return LCFG_STATUS_ERROR;
  }

  const char * statusfile = filename != NULL ? filename : compname;

  char * tmpfile = lcfgutils_safe_tmpfile(statusfile);

  bool ok = true;

  int fd = mkstemp(tmpfile);
  FILE * out = fdopen( fd, "w" );
  if ( out == NULL ) {
    lcfgutils_build_message( msg, "Failed to open temporary status file '%s'",
              tmpfile );
    ok = false;
    goto cleanup;
  }

  /* For efficiency a buffer is pre-allocated. The initial size was
     chosen by looking at typical resource usage for Informatics. The
     buffer will be automatically grown when necessary, the aim is to
     minimise the number of reallocations required.  */

  size_t buf_size = 384;
  char * buffer = calloc( buf_size, sizeof(char) );
  if ( buffer == NULL ) {
    perror( "Failed to allocate memory for LCFG resource buffer" );
    exit(EXIT_FAILURE);
  }

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
	cur_node != NULL;
	cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res = lcfgcomponent_resource(cur_node);

    /* Not interested in resources for inactive contexts */

    if ( !lcfgresource_is_active(res) ) continue;

    ssize_t rc = lcfgresource_to_status( res, compname, LCFG_OPT_NONE,
					 &buffer, &buf_size );

    if ( rc > 0 ) {

      if ( fputs( buffer, out ) < 0 )
	ok = false;

    } else {
      ok = false;
    }

    if (!ok) {
      lcfgutils_build_message( msg, "Failed to write to status file" );
      break;
    }

  }

  free(buffer);

  if ( fclose(out) != 0 ) {
    lcfgutils_build_message( msg, "Failed to close status file" );
    ok = false;
  }

  if (ok) {
    if ( rename( tmpfile, statusfile ) != 0 ) {
      lcfgutils_build_message( msg, "Failed to rename temporary status file to '%s'",
                statusfile );
      ok = false;
    }
  }

 cleanup:

  if ( tmpfile != NULL ) {
    unlink(tmpfile);
    free(tmpfile);
  }

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

LCFGResourceNode * lcfgcomponent_find_node( const LCFGComponent * comp,
                                            const char * name ) {
  assert( comp != NULL );

  if ( lcfgcomponent_is_empty(comp) ) return NULL;

  LCFGResourceNode * result = NULL;

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
	cur_node != NULL && result == NULL;
	cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res = lcfgcomponent_resource(cur_node); 

    if ( !lcfgresource_is_active(res) ) continue;

    const char * res_name = lcfgresource_get_name(res);

    if ( res_name != NULL && strcmp( res_name, name ) == 0 )
      result = (LCFGResourceNode *) cur_node;

  }

  return result;
}

LCFGResource * lcfgcomponent_find_resource( const LCFGComponent * comp,
                                            const char * name ) {
  assert( comp != NULL );

  LCFGResource * res = NULL;

  const LCFGResourceNode * res_node = lcfgcomponent_find_node( comp, name );
  if ( res_node != NULL )
    res = lcfgcomponent_resource(res_node);

  return res;
}

bool lcfgcomponent_has_resource( const LCFGComponent * comp,
                                 const char * name ) {
  assert( comp != NULL );

  return ( lcfgcomponent_find_node( comp, name ) != NULL );
}

LCFGResource * lcfgcomponent_find_or_create_resource( LCFGComponent * comp,
                                                      const char * name ) {
  assert( comp != NULL );

  /* Only searches 'active' resources */

  LCFGResource * result = lcfgcomponent_find_resource( comp, name );

  if ( result != NULL ) return result;

  /* If not found then create new resource and add it to the component */
  result = lcfgresource_new();

  /* Setting name can fail if it is invalid */

  char * new_name = strdup(name);
  bool ok = lcfgresource_set_name( result, new_name );

  if ( !ok ) {
    free(new_name);
  } else {

    if ( lcfgcomponent_append( comp, result ) == LCFG_CHANGE_ERROR )
      ok = false;

  }

  lcfgresource_relinquish(result);

  if (!ok)
    result = NULL;

  return result;
}

LCFGChange lcfgcomponent_insert_or_merge_resource(
                                            LCFGComponent * comp,
                                            LCFGResource * new_res,
                                            char ** msg ) {
  assert( comp != NULL );

  LCFGChange result = LCFG_CHANGE_ERROR;

  /* Name for resource is required */
  if ( !lcfgresource_has_name(new_res) ) return LCFG_CHANGE_ERROR;

  LCFGResourceNode * cur_node =
    lcfgcomponent_find_node( comp, lcfgresource_get_name(new_res) );

  if ( cur_node == NULL ) {
    result = lcfgcomponent_append( comp, new_res );
  } else {
    LCFGResource * cur_res = lcfgcomponent_resource(cur_node);

    int priority  = lcfgresource_get_priority(new_res);
    int opriority = lcfgresource_get_priority(cur_res);

    /* If the priority of the older version of this resource is
       greater than the proposed replacement then no change is
       required. */

    if ( opriority > priority ) {  /* nothing more to do */
      result = LCFG_CHANGE_NONE;
    } else if ( priority > opriority ||
                lcfgresource_same_value( cur_res, new_res ) ) {

      if ( !lcfgresource_same_type( cur_res, new_res ) ) {
        /* warn about different types */
      }

      /* replace current version of resource with new one */

      lcfgresource_acquire(new_res);
      cur_node->resource = new_res;

      lcfgresource_relinquish(cur_res);

      result = LCFG_CHANGE_REPLACED;

    } else {
      lcfgutils_build_message( msg, "Resource conflict" );
    }

  }

  return result;
}

LCFGChange lcfgcomponent_insert_or_replace_resource(
                                              LCFGComponent * comp,
                                              LCFGResource * new_res,
                                              char ** msg ) {
  assert( comp != NULL );

  LCFGChange result = LCFG_CHANGE_ERROR;

  /* Name for resource is required */
  if ( !lcfgresource_has_name(new_res) ) return LCFG_CHANGE_ERROR;

  LCFGResourceNode * cur_node =
    lcfgcomponent_find_node( comp, lcfgresource_get_name(new_res) );

  if ( cur_node == NULL ) {
    result = lcfgcomponent_append( comp, new_res );
  } else {
    LCFGResource * cur_res = lcfgcomponent_resource(cur_node);

    /* replace current version of resource with new one */

    lcfgresource_acquire(new_res);
    cur_node->resource = new_res;

    lcfgresource_relinquish(cur_res);

    result = LCFG_CHANGE_REPLACED;
  }

  return result;
}

LCFGStatus lcfgcomponent_apply_overrides( LCFGComponent * comp,
					  const LCFGComponent * overrides,
					  char ** msg ) {
  assert( comp != NULL );

  if ( lcfgcomponent_is_empty(overrides) ) return LCFG_STATUS_OK;

  LCFGStatus status = LCFG_STATUS_OK;

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(overrides);
	cur_node != NULL && status != LCFG_STATUS_ERROR;
	cur_node = lcfgcomponent_next(cur_node) ) {

    LCFGResource * override_res = lcfgcomponent_resource(cur_node);

    LCFGChange rc =
      lcfgcomponent_insert_or_replace_resource( comp, override_res, msg );

    if ( rc == LCFG_CHANGE_ERROR )
      status = LCFG_STATUS_ERROR;

  }

  return status;
}

char * lcfgcomponent_get_resources_as_string(const LCFGComponent * comp) {
  assert( comp != NULL );

  if ( lcfgcomponent_is_empty(comp) ) return strdup("");

  LCFGTagList * reslist = lcfgtaglist_new();

  bool ok = true;

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
        cur_node != NULL && ok;
        cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res = lcfgcomponent_resource(cur_node);

    if ( !lcfgresource_is_active(res) || !lcfgresource_has_name(res) )
      continue;

    const char * res_name = lcfgresource_get_name(res);

    char * msg = NULL;
    LCFGChange change = lcfgtaglist_mutate_add( reslist, res_name, &msg );
    if ( change == LCFG_CHANGE_ERROR )
      ok = false;

    free(msg); /* Just ignoring any message */
  }

  size_t buf_len = 0;
  char * res_as_str = NULL;

  if (ok) {
    lcfgtaglist_sort(reslist);

    if ( lcfgtaglist_to_string( reslist, 0,
                                &res_as_str, &buf_len ) < 0 ) {
      ok = false;
    }
  }

  lcfgtaglist_relinquish(reslist);

  if ( !ok ) {
    free(res_as_str);
    res_as_str = NULL;
  }

  return res_as_str;
}

/* eof */
