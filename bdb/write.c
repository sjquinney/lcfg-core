/**
 * @file bdb/write.c
 * @brief Functions for writing a profile to Berkeley DB
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date: 2017-05-02 15:30:13 +0100 (Tue, 02 May 2017) $
 * $Revision: 32603 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#include <assert.h>
#include <errno.h>

#include <db.h>

#include "utils.h"
#include "bdb.h"

/**
 * @brief Open a Berkeley DB file for writing
 *
 * Opens a Berkeley DB file for writing using the @c DB_HASH access
 * method and returns the database handle.
 *
 * If the file cannot be opened a @c NULL value will be returned.
 *
 * @param[in] filename The path to the Berkeley DB file.
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Database handle
 *
 */

DB * lcfgbdb_init_writer( const char * filename,
                          char ** msg ) {
  assert( filename != NULL );

  return lcfgbdb_open_db( filename, DB_CREATE|DB_TRUNCATE, msg );
}

/**
 * @brief Store resources for a component into a Berkeley DB
 *
 * Stores the active resources for the component into the DB. In
 * addition to storing the values for the resources various metadata
 * will be stored. Information on the resource type, derivation,
 * context and priority will be stored where available.
 *
 * If the component pointer is @c NULL or the resource list is empty
 * then the DB will not be altered. The component @b MUST have a name.
 *
 * The keys are generated by combining the namespace, component name
 * and resource name using a C<.> (period) separator. The key for each
 * meta-data entry has a single-character prefix:
 *
 * - derivation '#' (octothorpe)
 * - type       '%' (percent)
 * - context    '=' (equals)
 * - priority   '^' (caret)
 *
 *
 * @param[in] component Pointer to an @c LCFGComponent struct
 * @param[in] namespace Namespace for the DB keys (usually the nodename)
 * @param[in] dbh Database handle.
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcomponent_to_bdb( const LCFGComponent * component,
                                 const char * namespace,
                                 DB * dbh,
                                 char ** msg ) {
  assert( dbh != NULL );

  if ( lcfgcomponent_is_empty(component) ) return LCFG_STATUS_OK;

  /* A sorted list of resources is stored for each component keyed on
     the component name. It does not appear to be used but we still do
     it to ensure backwards compatibility. */

  if ( !lcfgcomponent_has_name(component) ) {
    lcfgutils_build_message( msg, "Component does not have a name" );
    return LCFG_STATUS_ERROR;
  }

  const char * compname = lcfgcomponent_get_name(component);

  /* Declarations ahead of any jumps to cleanup */

  LCFGStatus status = LCFG_STATUS_OK;

  /* Using a taglist to track the list of names of stored resources */
  LCFGTagList * stored_res = lcfgtaglist_new();

  /* This is used (and reused) as a buffer for all the resource keys
     to avoid allocating and freeing lots of memory. */

  ssize_t key_len = 0;
  size_t key_size = 64;
  char * key_buf = calloc( key_size, sizeof(char) );
  if ( key_buf == NULL ) {
    perror("Failed to allocate memory for data buffer");
    exit(EXIT_FAILURE);
  }

  ssize_t val_len = 0;
  size_t val_size = 16384;
  char * val_buf = calloc( val_size, sizeof(char) );
  if ( val_buf == NULL ) {
    perror("Failed to allocate memory for data buffer");
    exit(EXIT_FAILURE);
  }

  int ret;
  DBT key, data;

  LCFGComponentIterator * compiter =
    lcfgcompiter_new( (LCFGComponent *) component, false );

  const LCFGResource * resource = NULL;
  while ( ( resource = lcfgcompiter_next(compiter) ) != NULL ) {

    /* Derivation */

    if ( lcfgresource_has_derivation(resource) ) {

      memset( &key,  0, sizeof(DBT) );
      memset( &data, 0, sizeof(DBT) );

      key_len = lcfgresource_build_key( resource->name,
                                        compname,
                                        namespace,
                                        LCFG_RESOURCE_SYMBOL_DERIVATION,
                                        &key_buf, &key_size );

      if ( key_len < 0 ) {
        status = LCFG_STATUS_ERROR;
        *msg = lcfgresource_build_message( resource, compname,
                                           "Failed to build derivation key" );
        break;
      }

      key.data = key_buf;
      key.size = (u_int32_t) key_len;

      val_len = lcfgresource_get_derivation_as_string( resource, LCFG_OPT_NONE,
                                                       &val_buf, &val_size );

      if ( val_len <= 0 ) {
	status = LCFG_STATUS_ERROR;
	*msg = lcfgresource_build_message( resource, compname,
					   "Failed to build derivation value");
	break;
      }

      data.data = val_buf;
      data.size = (u_int32_t) val_len;

      ret = dbh->put( dbh, NULL, &key, &data, DB_OVERWRITE_DUP );

      if ( ret != 0 ) {
        status = LCFG_STATUS_ERROR;
        lcfgutils_build_message( msg, "Failed to store resource derivation data: %s\n",
                  db_strerror(ret) );
        break;
      }

    }

    /* Type */

    if ( lcfgresource_get_type(resource) != LCFG_RESOURCE_TYPE_STRING ||
         lcfgresource_has_comment(resource) ) {

      memset( &key,  0, sizeof(DBT) );
      memset( &data, 0, sizeof(DBT) );

      key_len = lcfgresource_build_key( resource->name,
                                       compname,
                                       namespace,
                                       LCFG_RESOURCE_SYMBOL_TYPE,
                                       &key_buf, &key_size );

      if ( key_len < 0 ) {
        status = LCFG_STATUS_ERROR;
        *msg = lcfgresource_build_message( resource, compname,
                                           "Failed to build type key" );
        break;
      }

      key.data = key_buf;
      key.size = (u_int32_t) key_len;

      val_len = lcfgresource_get_type_as_string( resource, LCFG_OPT_NONE,
                                                 &val_buf, &val_size );

      if ( val_len <= 0 ) {
	status = LCFG_STATUS_ERROR;
	*msg = lcfgresource_build_message( resource, compname,
					   "Failed to build type value");
	break;
      }

      data.data = val_buf;
      data.size = (u_int32_t) val_len;

      ret = dbh->put( dbh, NULL, &key, &data, DB_OVERWRITE_DUP );

      if ( ret != 0 ) {
        status = LCFG_STATUS_ERROR;
        lcfgutils_build_message( msg,
				 "Failed to store resource type data: %s\n",
				 db_strerror(ret) );
        break;
      }

    }

    /* Context */

    if ( lcfgresource_has_context(resource) ) {

      memset( &key,  0, sizeof(DBT) );
      memset( &data, 0, sizeof(DBT) );

      key_len = lcfgresource_build_key( resource->name,
                                       compname,
                                       namespace,
                                       LCFG_RESOURCE_SYMBOL_CONTEXT,
                                       &key_buf, &key_size );

      if ( key_len < 0 ) {
        status = LCFG_STATUS_ERROR;
        *msg = lcfgresource_build_message( resource, compname,
                                           "Failed to build context key" );
        break;
      }

      key.data = key_buf;
      key.size = (u_int32_t) key_len;

      const char * context = lcfgresource_get_context(resource);
      data.data = (char *) context;
      data.size = (u_int32_t) strlen(context);

      ret = dbh->put( dbh, NULL, &key, &data, DB_OVERWRITE_DUP );
      if ( ret != 0 ) {
        status = LCFG_STATUS_ERROR;
        lcfgutils_build_message( msg,
				 "Failed to store resource context data: %s\n",
				 db_strerror(ret) );
        break;
      }

    }

    /* Priority */

    /* Only store the value for the priority when it is greater than
       the default (zero). */

    if ( lcfgresource_get_priority(resource) > 0 ) {

      memset( &key,  0, sizeof(DBT) );
      memset( &data, 0, sizeof(DBT) );

      key_len = lcfgresource_build_key( resource->name,
                                       compname,
                                       namespace,
                                       LCFG_RESOURCE_SYMBOL_PRIORITY,
                                       &key_buf, &key_size );

      if ( key_len < 0 ) {
        status = LCFG_STATUS_ERROR;
        *msg = lcfgresource_build_message( resource, compname,
                                           "Failed to build priority key" );
        break;
      }

      key.data = key_buf;
      key.size = (u_int32_t) key_len;

      val_len = lcfgresource_get_priority_as_string( resource, LCFG_OPT_NONE,
                                                     &val_buf, &val_size );

      if ( val_len <= 0 ) {
        status = LCFG_STATUS_ERROR;
        lcfgutils_build_message( msg, "Failed to build priority value" );
        break;
      }

      data.data = val_buf;
      data.size = (u_int32_t) val_len;

      ret = dbh->put( dbh, NULL, &key, &data, DB_OVERWRITE_DUP );

      if ( ret != 0 ) {
        status = LCFG_STATUS_ERROR;
        lcfgutils_build_message( msg,
				 "Failed to store resource priority data: %s\n",
				 db_strerror(ret) );
        break;
      }

    }

    /* Value */

    memset( &key,  0, sizeof(DBT) );
    memset( &data, 0, sizeof(DBT) );

    key_len = lcfgresource_build_key( resource->name,
                                     compname,
                                     namespace,
                                     LCFG_RESOURCE_SYMBOL_VALUE,
                                     &key_buf, &key_size );

    if ( key_len < 0 ) {
      status = LCFG_STATUS_ERROR;
      *msg = lcfgresource_build_message( resource, compname,
					 "Failed to build value key" );
      break;
    }

    key.data = key_buf;
    key.size = (u_int32_t) key_len;

    const char * value;
    size_t value_len;
    if ( lcfgresource_has_value(resource) ) {
      value     = lcfgresource_get_value(resource);
      value_len = strlen(value);
    } else {
      value     = "";
      value_len = 0;
    }

    data.data = (char *) value;
    data.size = (u_int32_t) value_len;

    ret = dbh->put( dbh, NULL, &key, &data, DB_OVERWRITE_DUP );

    if ( ret != 0 ) {
      status = LCFG_STATUS_ERROR;
      lcfgutils_build_message( msg,
			       "Failed to store resource value data: %s\n",
			       db_strerror(ret) );
      break;
    }

    /* Stash the resource name */

    const char * res_name = lcfgresource_get_name(resource);

    char * add_msg = NULL;
    LCFGChange add_rc = lcfgtaglist_mutate_add( stored_res, res_name,
						&add_msg );
    if ( add_rc == LCFG_CHANGE_ERROR )
      status = LCFG_STATUS_ERROR;

    free(add_msg);

  }

  /* Store the list of resource names */

  if ( status != LCFG_STATUS_ERROR ) {

    lcfgtaglist_sort(stored_res);

    ssize_t len = lcfgtaglist_to_string( stored_res, LCFG_OPT_NONE,
                                         &val_buf, &val_size );

    if ( len > 0 ) {

      memset( &key,  0, sizeof(DBT) );
      memset( &data, 0, sizeof(DBT) );

      key.data = (char *) compname;
      key.size = (u_int32_t) strlen(compname);

      data.data = val_buf;
      data.size = (u_int32_t) len;

      int ret = dbh->put( dbh, NULL, &key, &data, DB_OVERWRITE_DUP );

      if ( ret != 0 ) {
	status = LCFG_STATUS_ERROR;
	lcfgutils_build_message( msg,
	   "Failed to store list of resources for component: %s\n",
				 db_strerror(ret) );
	goto cleanup;
      }
    }

  }

 cleanup:

  lcfgcompiter_destroy(compiter);
  lcfgtaglist_relinquish(stored_res);
  free(key_buf);
  free(val_buf);

  return status;
}

/**
 * @brief Store resources for a list of components into a Berkeley DB
 *
 * Stores the active resources for each component in the list into the
 * DB. This function just calls lcfgcomponent_to_bdb() on each
 * component in the list, see the documentation for that function for
 * full details.
 *
 * @param[in] compset Set of LCFG components.
 * @param[in] namespace Namespace for the DB keys (usually the nodename).
 * @param[in] dbh Database handle.
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 * 
 */

LCFGStatus lcfgcompset_to_bdb( const LCFGComponentSet * compset,
                               const char * namespace,
                               DB * dbh,
                               char ** msg ) {
  assert( dbh != NULL );

  if ( lcfgcompset_is_empty(compset) ) return LCFG_STATUS_OK;

  LCFGStatus status = LCFG_STATUS_OK;

  LCFGComponent ** components = compset->components;

  unsigned int i;
  for ( i=0; status != LCFG_STATUS_ERROR && i < compset->buckets; i++ ) {
    const LCFGComponent * component = components[i];

    if (component) {
      status = lcfgcomponent_to_bdb( component,
                                     namespace,
                                     dbh,
                                     msg );
    }
  }

  return status;
}

/**
 * @brief Store resources for the components in a profile into a Berkeley DB
 *
 * Stores the active resources for each component in the profile into
 * the DB. This function calls lcfgcompset_to_bdb() on the list of
 * components in the profile, see the documentation for that function
 * for full details.
 *
 * If the namespace is not specifed and there is a value for the
 * @c profile.node resource in the profile then that will be used.
 *
 * The DB will initially be written into a temporary file in the same
 * directory as the target file name. If the temporary DB is
 * successfully written it will then be renamed to the target file
 * name. It is important to note that this function does NOT update
 * the contents of any current DB stored in the target file.
 *
 * If there is a value for the modification time of the profile
 * (e.g. it was read in from an XML profile) that will be set as the
 * mtime and atime of the new file.
 *
 * @param[in] profile The LCFG profile.
 * @param[in] namespace Namespace for the DB keys (usually the nodename).
 * @param[in] dbfile Name of the file in which the DB should be stored.
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 * 
 */

LCFGStatus lcfgprofile_to_bdb( const LCFGProfile * profile,
                               const char * namespace,
                               const char * dbfile,
                               char ** msg ) {
  assert( profile != NULL );
  assert( dbfile != NULL );

  /* Early declarations so available if jumping to cleanup */
  LCFGStatus status = LCFG_STATUS_OK;
  char * tmpfile = NULL;
  FILE * tmpfh = NULL;

  /* Only use the value for profile.node when the namespace has not
     been specified */

  const char * node = NULL;
  if ( namespace == NULL ) {
    if ( !lcfgprofile_get_meta( profile, "node", &node ) ) {
      /* Just ignore problems with fetching profile.node value */
      node = NULL;
    }
  }

  tmpfh = lcfgutils_safe_tmpfile( dbfile, &tmpfile );

  if ( tmpfh == NULL || tmpfile == NULL ) {
    status = LCFG_STATUS_ERROR;
    lcfgutils_build_message( msg, "Failed to generate safe temporary file name");
    goto cleanup;
  }

  char * init_errmsg = NULL;
  DB * dbh = lcfgbdb_init_writer( tmpfile, &init_errmsg );
  if ( dbh == NULL ) {
    status = LCFG_STATUS_ERROR;
    lcfgutils_build_message( msg, "Failed to initialise new DB: %s", init_errmsg );
  }

  free(init_errmsg);

  if ( status == LCFG_STATUS_ERROR ) goto cleanup;

  if ( lcfgprofile_has_components(profile) ) {
    status = lcfgcompset_to_bdb( profile->components,
                                 ( namespace != NULL ? namespace : node ),
                                 dbh,
                                 msg );
  }

  /* even if the store fails we need to close the DB handle at this point */
  lcfgbdb_close_db(dbh);

  if ( status == LCFG_STATUS_OK ) {

    if ( rename( tmpfile, dbfile ) == 0 ) {

      time_t mtime = lcfgprofile_get_mtime(profile);

      if ( mtime != 0 ) {
        struct utimbuf times;
        times.actime  = mtime;
        times.modtime = mtime;
        utime( dbfile, &times );
      }

    } else {
      char * errmsg = strerror(errno);
      status = LCFG_STATUS_ERROR;
      lcfgutils_build_message( msg, "Failed to rename DB file to '%s': %s",
                               dbfile, errmsg );
    }

  }

 cleanup:

  if ( tmpfh != NULL )
    fclose(tmpfh);

  if ( tmpfile != NULL ) {
    unlink(tmpfile);
    free(tmpfile);
  }

  return status;
}

/* eof */
