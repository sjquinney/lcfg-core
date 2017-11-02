/**
 * @file bdb/read.c
 * @brief Functions for reading a profile from Berkeley DB
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date: 2017-05-02 15:30:13 +0100 (Tue, 02 May 2017) $
 * $Revision: 32603 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <db.h>

#include "utils.h"
#include "bdb.h"

/**
 * @brief Load LCFG profile from Berkeley DB
 *
 * This will load the components and resources for an LCFG profile
 * from a Berkeley DB file. If the @c comps_wanted parameter is @c
 * NULL then all components are loaded, otherwise the set of
 * components can be restricted by passing in an @c LCFGTagList of the
 * component names.
 *
 * Typically the keys in the DB will be stored with a @e namespace
 * prefix which is the short nodename for a profile (e.g. foo for
 * foo.lcfg.org).
 *
 * By default the function will return an error status if the file
 * does not exist. This behaviour can be altered to returning success
 * and an empty @c LCFGProfile struct by specifying the @c
 * LCFG_OPT_ALLOW_NOEXIST option. This is useful when loading an 'old'
 * profile for comparison with a 'new' one when the old file might
 * legitimately not exist.
 *
 * @param[in] filename Path to the Berkeley DB file.
 * @param[out] result Reference to the pointer for the @c LCFGProfile struct.
 * @param[in] comps_wanted Whitelist for component names.
 * @param[in] namespace Namespace for the DB keys (usually the nodename)
 * @param[in] options Controls the behaviour of the process.
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgprofile_from_bdb( const char * filename,
                                 LCFGProfile ** result,
                                 const LCFGTagList * comps_wanted,
                                 const char * namespace,
				 LCFGOption options,
                                 char ** msg ) {
  assert( filename != NULL );

  LCFGStatus status = LCFG_STATUS_OK;

  LCFGProfile * profile = lcfgprofile_new();

  struct stat sb;
  if ( stat( filename, &sb ) == 0 )
    profile->mtime = sb.st_mtime;

  DB * dbh = lcfgbdb_init_reader( filename, msg );

  if ( dbh == NULL ) {
    if ( errno != ENOENT || !(options&LCFG_OPT_ALLOW_NOEXIST) )
      status = LCFG_STATUS_ERROR;

  } else {

    /* Ensure the 'profile' component is always loaded. Empty list
       means 'load everything' */
 
    LCFGTagList * comps_wanted2 = NULL;
    if ( !lcfgtaglist_is_empty(comps_wanted) && 
         !lcfgtaglist_contains( comps_wanted, "profile" ) ) {

      comps_wanted2 = lcfgtaglist_clone(comps_wanted);

      char * add_msg = NULL;
      if ( !lcfgtaglist_mutate_add( comps_wanted2, "profile", &add_msg ) ) {
        lcfgutils_build_message( msg,
                                 "Problems with list of components: %s",
                                 add_msg );
        status = LCFG_STATUS_ERROR;
      }
      free(add_msg);

      comps_wanted = comps_wanted2;
    }

    if ( status != LCFG_STATUS_ERROR ) {

      status = lcfgbdb_process_components( dbh,
                                           &profile->components,
                                           comps_wanted, 
                                           namespace,
                                           msg );
    }

    lcfgtaglist_relinquish(comps_wanted2);
  }

  lcfgbdb_close_db(dbh);

  if ( status != LCFG_STATUS_OK ) {
    lcfgprofile_destroy(profile);
    profile = NULL;
  }

  *result = profile;

  return status;
}

static LCFGChange lcfgbdb_get_resource_item( DB * dbh,
                                             LCFGResource * res,
                                             const char * comp_name,
                                             const char * namespace,
                                             char type_symbol,
                                             char ** keybuf,
                                             size_t * keybufsize,
                                             char ** msg ) {
  assert( res != NULL );

  ssize_t keylen = lcfgresource_build_key( res->name,
                                           comp_name, namespace,
                                           type_symbol,
                                           keybuf, keybufsize );

  if ( keylen < 0 ) {
    *msg = lcfgresource_build_message( res, comp_name,
                                       "Failed to retrieve data from DB" );
    return LCFG_CHANGE_ERROR;
  }

  LCFGChange change = LCFG_CHANGE_NONE;

  DBT key, data;

  memset( &key,  0, sizeof(DBT) );
  memset( &data, 0, sizeof(DBT) );

  key.data = *keybuf;
  key.size = (u_int32_t) keylen;

  int ret = dbh->get( dbh, NULL, &key, &data, 0 );

  if ( ret == 0 ) {

    char * set_msg = NULL;
    if ( lcfgresource_set_attribute( res, type_symbol,
                                      (char *) data.data, (size_t) data.size,
				      &set_msg ) ) {
      change = LCFG_CHANGE_MODIFIED;
    } else {
      change = LCFG_CHANGE_ERROR;
      *msg = lcfgresource_build_message( res, comp_name,
                                         "Failed to set attribute: %s",
                                         set_msg );

    }
    free(set_msg);
  }

  return change;
}

static LCFGStatus lcfgbdb_process_component( DB * dbh,
                                             const char * comp_name,
                                             const char * namespace,
                                             LCFGComponent ** result,
                                             char ** msg ) {
  assert( comp_name != NULL );

  LCFGStatus status = LCFG_STATUS_OK;
  LCFGComponent * component = NULL;
  char * reslist = NULL;
  char * keybuf  = NULL;

  int ret;
  DBT key, data;

  memset( &key,  0, sizeof(DBT) );
  memset( &data, 0, sizeof(DBT) );

  /* Fetch the list of resource names from the DB */

  key.data = (void *) comp_name;
  key.size = (u_int32_t) strlen(comp_name);

  ret = dbh->get( dbh, NULL, &key, &data, 0 );

  if ( ret != 0 ) {
    status = LCFG_STATUS_ERROR;
    lcfgutils_build_message( msg,
                             "Failed to find resources for component '%s': %s",
                             comp_name, db_strerror(ret) );
  } else if ( data.size > 0 ) {

    reslist = strndup( (char *) data.data, data.size );

  }

  if ( reslist == NULL ) {
    status = LCFG_STATUS_ERROR;
    lcfgutils_build_message( msg,
                             "Failed to find resources for component '%s'",
                             comp_name );
  }

  if ( status == LCFG_STATUS_ERROR ) goto cleanup;

  component = lcfgcomponent_new();
  char * comp_name2 = strdup(comp_name);
  if ( !lcfgcomponent_set_name( component, comp_name2 ) ) {
    free(comp_name2);

    status = LCFG_STATUS_ERROR;
    lcfgutils_build_message( msg,
                             "Failed to set '%s' as name for component",
                             comp_name );
    goto cleanup;
  }

  size_t keybufsize = 512;
  keybuf = calloc( keybufsize, sizeof(char) );
  if ( keybuf == NULL ) {
    perror("Failed to allocate memory for DB key buffer");
    exit(EXIT_FAILURE);
  }

  char * saveptr = NULL;
  char * resname = strtok_r( reslist, " ", &saveptr );
  while (resname) {

    if ( !lcfgresource_valid_name(resname) ) {
      lcfgutils_build_message( msg, "Invalid resource name '%s.%s'",
                               comp_name, resname );
      break;
    }

    LCFGResource * res = lcfgresource_new();

    char * resname_copy = strdup(resname);
    if ( !lcfgresource_set_name( res, resname_copy ) ) {
      free(resname_copy);

      status = LCFG_STATUS_ERROR;
      lcfgutils_build_message( msg, "Failed to set resource name '%s.%s'",
                               comp_name, resname );
      goto local_cleanup;
    }

    LCFGChange res_change = LCFG_CHANGE_NONE;

    /* Derivation */

    LCFGChange deriv_change =
      lcfgbdb_get_resource_item( dbh, res, 
				 comp_name, namespace,
				 LCFG_RESOURCE_SYMBOL_DERIVATION,
				 &keybuf, &keybufsize, msg );

    if ( deriv_change == LCFG_CHANGE_ERROR ) {
      res_change = LCFG_CHANGE_ERROR;
      goto local_cleanup;
    } else if ( deriv_change == LCFG_CHANGE_MODIFIED ) {
      res_change = LCFG_CHANGE_MODIFIED;
    }

    /* Type */

    LCFGChange type_change =
      lcfgbdb_get_resource_item( dbh, res, 
				 comp_name, namespace,
				 LCFG_RESOURCE_SYMBOL_TYPE,
				 &keybuf, &keybufsize, msg );

    if ( type_change == LCFG_CHANGE_ERROR ) {
      res_change = LCFG_CHANGE_ERROR;
      goto local_cleanup;
    } else if ( type_change == LCFG_CHANGE_MODIFIED ) {
      res_change = LCFG_CHANGE_MODIFIED;
    }

    /* Context */

    LCFGChange context_change =
      lcfgbdb_get_resource_item( dbh, res, 
				 comp_name, namespace,
				 LCFG_RESOURCE_SYMBOL_CONTEXT,
				 &keybuf, &keybufsize, msg );

    if ( context_change == LCFG_CHANGE_ERROR ) {
      res_change = LCFG_CHANGE_ERROR;
      goto local_cleanup;
    } else if ( context_change == LCFG_CHANGE_MODIFIED ) {
      res_change = LCFG_CHANGE_MODIFIED;
    }

    /* Priority */

    LCFGChange prio_change =
      lcfgbdb_get_resource_item( dbh, res, 
				 comp_name, namespace,
				 LCFG_RESOURCE_SYMBOL_PRIORITY,
				 &keybuf, &keybufsize, msg );

    if ( prio_change == LCFG_CHANGE_ERROR ) {
      res_change = LCFG_CHANGE_ERROR;
      goto local_cleanup;
    } else if ( prio_change == LCFG_CHANGE_MODIFIED ) {
      res_change = LCFG_CHANGE_MODIFIED;
    }

    /* Value */

    LCFGChange value_change =
      lcfgbdb_get_resource_item( dbh, res, 
				 comp_name, namespace,
				 LCFG_RESOURCE_SYMBOL_VALUE,
				 &keybuf, &keybufsize, msg );

    if ( value_change == LCFG_CHANGE_ERROR ) {
      res_change = LCFG_CHANGE_ERROR;
      goto local_cleanup;
    } else if ( value_change == LCFG_CHANGE_MODIFIED ) {
      res_change = LCFG_CHANGE_MODIFIED;
    }

    /* Store the resource into the component.  Maybe we only want to
       store it if something has been modified?? That would avoid
       adding resources which don't really exist in the DB. That would
       be different from behaviour in old client code. */

    char * merge_msg = NULL;
    LCFGChange merge_rc = 
      lcfgcomponent_merge_resource( component, res, &merge_msg );

    if ( merge_rc == LCFG_CHANGE_ERROR ) {
      status = LCFG_STATUS_ERROR;
      lcfgutils_build_message( msg,
                               "Failed to merge resource into component: %s",
                               merge_msg );
    }
    free(merge_msg);

  local_cleanup:

    if ( res_change == LCFG_CHANGE_ERROR )
      status = LCFG_STATUS_ERROR;

    lcfgresource_relinquish(res);

    if ( status != LCFG_STATUS_ERROR ) {
      resname = strtok_r( NULL, " ", &saveptr );
    } else {
      break;
    }

  }

 cleanup:

  free(reslist);
  free(keybuf);

  if ( status == LCFG_STATUS_ERROR ) {
    lcfgcomponent_relinquish(component);
    component = NULL;
  }

  *result = component;

  return status;
}

/**
 * @brief Load LCFG component from Berkeley DB
 *
 * This will load the resources for the specified LCFG component from
 * a Berkeley DB file.
 *
 * Typically the keys in the DB will be stored with a @e namespace
 * prefix which is the short nodename for a profile (e.g. foo for
 * foo.lcfg.org).
 *
 * By default the function will return an error status if the file
 * does not exist. This behaviour can be altered to returning success
 * by specifying the @c LCFG_OPT_ALLOW_NOEXIST option. This is useful
 * when loading an 'old' profile for comparison with a 'new' one when
 * the old file might legitimately not exist.
 *
 * @param[in] filename Path to the Berkeley DB file.
 * @param[out] result Reference to the pointer for the @c LCFGComponent struct.
 * @param[in] comp_name Name of required component
 * @param[in] namespace Namespace for the DB keys (usually the nodename)
 * @param[in] options Controls the behaviour of the process.
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcomponent_from_bdb( const char * filename,
                                   LCFGComponent ** result,
                                   const char * comp_name,
                                   const char * namespace,
				   LCFGOption options,
                                   char ** msg ) {
  assert( filename != NULL );
  assert( comp_name != NULL );

  *result = NULL;

  if ( !lcfgcomponent_valid_name(comp_name) ) {
    lcfgutils_build_message( msg, "Invalid component name '%s'", comp_name );
    return LCFG_STATUS_ERROR;
  }

  LCFGStatus status = LCFG_STATUS_OK;

  DB * dbh = lcfgbdb_init_reader( filename, msg );

  if ( dbh != NULL ) {
    status = lcfgbdb_process_component( dbh, comp_name, namespace,
                                        result, msg );

    lcfgbdb_close_db(dbh);
  } else {

    if ( errno != ENOENT || !(options&LCFG_OPT_ALLOW_NOEXIST) ) {
      status = LCFG_STATUS_ERROR;
    } else {

      /* Create an empty component with the required name */

      LCFGComponent * comp = lcfgcomponent_new();
      char * comp_name2 = strdup(comp_name);
      if ( lcfgcomponent_set_name( comp, comp_name2 ) ) {
        *result = comp;
      } else {
        free(comp_name2);

        status = LCFG_STATUS_ERROR;
        lcfgcomponent_relinquish(comp);
      }

    }

  }

  return status;
}

/**
 * @brief Process DB file to load LCFG components
 *
 * Iterates through all records in the database and loads the LCFG
 * component and resource structures. If the @c comps_wanted parameter
 * is @c NULL then all components are loaded, otherwise the set of
 * components can be restricted by passing in an @c LCFGTagList of the
 * component names.
 *
 * Typically the keys in the DB will be stored with a @e namespace
 * prefix which is the short nodename for a profile (e.g. foo for
 * foo.lcfg.org).
 *
 * @param[in] dbh Handle for database.
 * @param[out] result Reference to the pointer for the @c LCFGComponentSet struct.
 * @param[in] comps_wanted Whitelist for component names.
 * @param[in] namespace Namespace for the DB keys (usually the nodename).
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process.
 *
 */

LCFGStatus lcfgbdb_process_components( DB * dbh,
                                       LCFGComponentSet ** result,
                                       const LCFGTagList * comps_wanted,
                                       const char * namespace,
                                       char ** msg ) {
  assert( dbh != NULL );

  *result = NULL;

  LCFGStatus status = LCFG_STATUS_OK;

  LCFGComponentSet * compset = lcfgcompset_new();

  /* If the list of required components is empty then just load
     everything. In which case it is necessary to make a list of all
     available components. */

  LCFGTagList * all_comps = NULL;

  if ( lcfgtaglist_is_empty(comps_wanted) ) {

    all_comps = lcfgtaglist_new();
    comps_wanted = all_comps;

    /* Get a cursor */

    DBC * cursor;
    dbh->cursor( dbh, NULL, &cursor, 0 ); 

    DBT key, value;
    /* Initialize our DBTs. */
    memset( &key,   0, sizeof(DBT) );
    memset( &value, 0, sizeof(DBT) );

    int ret = 0;
    /* Iterate over the database, retrieving each record in turn. */
    while ( status != LCFG_STATUS_ERROR &&
            ( ret = cursor->get( cursor, &key, &value, DB_NEXT ) ) == 0 ) {

      if ( (size_t) key.size == 0 ) continue;

      /* Scan for a period character '.', if not found in the key then
         this is probably a component name which is used as the key
         for the list of resources for that component. */

      if ( memchr( key.data, '.', (size_t) key.size ) != NULL ) continue;

      /* Get a null-terminated string for the key */
      char * keyname = strndup( (char *) key.data, key.size );

      /* 'resource list' entry for a component */
      if ( lcfgcomponent_valid_name(keyname) ) {

        char * add_msg = NULL;
        if ( lcfgtaglist_mutate_add( all_comps, keyname, &add_msg )
             == LCFG_CHANGE_ERROR ) {
          lcfgutils_build_message( msg, 
                  "Failed to add '%s' to list of available components: %s",
                                   keyname, add_msg );
          status = LCFG_STATUS_ERROR;
        }
        free(add_msg);
      }

      free(keyname);
    }

    /* Cursors must be closed */
    if ( cursor != NULL ) 
      cursor->close(cursor);

  }

  /* Load the components */

  LCFGTagIterator * tagiter = lcfgtagiter_new(comps_wanted);
  const LCFGTag * tag = NULL;

  while ( status != LCFG_STATUS_ERROR &&
          ( tag = lcfgtagiter_next(tagiter) ) != NULL ) {

    const char * comp_name = lcfgtag_get_name(tag);
    if ( isempty(comp_name) ) continue;

    LCFGComponent * comp = NULL;

    status = lcfgbdb_process_component( dbh, comp_name, namespace,
                                        &comp, msg );

    if ( status != LCFG_STATUS_ERROR ) {

      LCFGChange rc = lcfgcompset_insert_component( compset, comp );
      if ( rc == LCFG_CHANGE_ERROR ) {
        lcfgutils_build_message( msg,
                                 "Failed to load resources for '%s' component",
                                 comp_name );
        status = LCFG_STATUS_ERROR;
      }

    }

    lcfgcomponent_relinquish(comp);

  }

  lcfgtagiter_destroy(tagiter);

  lcfgtaglist_relinquish(all_comps);

  if ( status != LCFG_STATUS_OK ) {

    if ( *msg == NULL )
      lcfgutils_build_message( msg, "Something bad happened whilst processing DB." );

    lcfgcompset_relinquish(compset);
    compset = NULL;
  }

  *result = compset;

  return status;
}

/**
 * @brief Open a Berkeley DB file
 *
 * Low-level function to open a DB file using the @c DB_HASH access
 * method and return the database handle. It is normally preferable to
 * use either lcfgbdb_init_writer() or lcfgbdb_init_reader().
 * 
 * The flags are passed through to the @c open function call. See the
 * Berkeley DB documentation for full details (e.g. @c DB_CREATE,
 * @c DB_EXCL and @c DB_RDONLY).
 *
 * If the file cannot be opened a @c NULL value will be returned.
 *
 * @param[in] filename The path to the Berkeley DB file.
 * @param[in] flags Flags which control how the file is opened.
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Database handle
 *
 */

DB * lcfgbdb_open_db( const char * filename,
                      u_int32_t flags,
                      char ** msg ) {
  assert( filename != NULL );

  DB *dbp;               /* DB structure handle */
  int ret;               /* function return value */

  /* Initialize the structure. This
   * database is not opened in an environment, 
   * so the environment pointer is NULL. */
  ret = db_create( &dbp, NULL, 0 );
  if (ret != 0) {
    lcfgutils_build_message( msg, "Failed to initialise DB: %s\n",
			     db_strerror(ret) );
    return NULL;
  }

  /* open the database */
  ret = dbp->open(dbp,        /* DB structure pointer */
                  NULL,       /* Transaction pointer */
                  filename,   /* On-disk file that holds the database. */
                  NULL,       /* Optional logical database name */
                  DB_HASH,    /* Database access method */
                  flags,      /* Open flags */
                  0);         /* File mode (using defaults) */

  if (ret != 0) {
    lcfgutils_build_message( msg, "Failed to open DB '%s': %s\n",
              filename, db_strerror(ret));
    return NULL;
  }

  return dbp;
}

/**
 * @brief Open a Berkeley DB file for reading
 *
 * Opens a Berkeley DB file for reading using the @c DB_HASH access
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

DB * lcfgbdb_init_reader( const char * filename,
                          char ** msg ) {
  assert( filename != NULL );

  FILE * file;
  if ( ( file = fopen(filename, "r") ) == NULL ) {

    if (errno == ENOENT) {
      lcfgutils_build_message( msg, "File '%s' does not exist.\n", filename );
    } else {
      lcfgutils_build_message( msg, "File '%s' is not readable.\n", filename );
    }
    return NULL;

  } else {
    fclose(file);
  }

  return lcfgbdb_open_db( filename, DB_RDONLY, msg );
}

/**
 * @brief Close a database handle
 *
 * If the specified database handle is not @c NULL then it will be
 * closed.
 *
 * @param[in] dbh Database handle
 *
 */

void lcfgbdb_close_db( DB * dbh ) {

  if ( dbh != NULL )
    dbh->close( dbh, 0 );

}

/* eof */

