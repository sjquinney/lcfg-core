#define _GNU_SOURCE /* for asprintf */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <db.h>

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

  *msg = NULL;

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

    status = lcfgbdb_process_components( dbh,
                                         &profile->components,
                                         comps_wanted, 
                                         namespace,
                                         msg );
  }

  lcfgbdb_close_db(dbh);

  if ( status != LCFG_STATUS_OK ) {
    lcfgprofile_destroy(profile);
    profile = NULL;
  }

  *result = profile;

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
 * @param[in] compname Name of required component
 * @param[in] namespace Namespace for the DB keys (usually the nodename)
 * @param[in] options Controls the behaviour of the process.
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcomponent_from_bdb( const char * filename,
                                   LCFGComponent ** result,
                                   const char * compname,
                                   const char * namespace,
				   LCFGOption options,
                                   char ** msg ) {

  *msg = NULL;

  if ( !lcfgcomponent_valid_name(compname) ) {
    asprintf( msg, "Invalid component name '%s'", compname );
    return LCFG_STATUS_ERROR;
  }

  /* Any failures after this point should go via the 'cleanup' phase */

  LCFGStatus status = LCFG_STATUS_OK;
  LCFGComponent * component = NULL;

  DB * dbh = lcfgbdb_init_reader( filename, msg );

  if ( dbh == NULL ) {
    if ( errno != ENOENT || !(options&LCFG_OPT_ALLOW_NOEXIST) )
      status = LCFG_STATUS_ERROR;

  } else {

    /* Create a new tag list containing the component name. */
  
    LCFGTagList * collect_comps = lcfgtaglist_new();

    char * tagmsg = NULL;
    if ( lcfgtaglist_mutate_add( collect_comps, compname, &tagmsg ) 
         == LCFG_CHANGE_ERROR ) {
      status = LCFG_STATUS_ERROR;
      asprintf( msg, "Invalid component name '%s': %s", compname, tagmsg );
    }
    free(tagmsg);

    LCFGComponentList * components = NULL;

    if ( status != LCFG_STATUS_ERROR ) {
      status = lcfgbdb_process_components( dbh,
					   &components,
					   collect_comps,
					   namespace,
					   msg );
    }

    lcfgtaglist_relinquish(collect_comps);

    if ( status != LCFG_STATUS_ERROR ) {
      component = lcfgcomplist_find_component( components, compname );

      if ( component != NULL ) {
	/* If a component was found then it must not be destroyed when the
	   complist is destroyed. */

	lcfgcomponent_acquire(component);

      } else {
	status = LCFG_STATUS_ERROR;
	asprintf( msg, "Failed to load resources for component '%s'",
		  compname );
      }
    }

    lcfgcomplist_relinquish(components);

  }
  
  lcfgbdb_close_db(dbh);

  *result = component;

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
 * @param[out] result Reference to the pointer for the @c LCFGComponentList struct.
 * @param[in] comps_wanted Whitelist for component names.
 * @param[in] namespace Namespace for the DB keys (usually the nodename).
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process.
 *
 */

LCFGStatus lcfgbdb_process_components( DB * dbh,
                                       LCFGComponentList ** result,
                                       const LCFGTagList * comps_wanted,
                                       const char * namespace,
                                       char ** msg ) {

  *result = NULL;
  *msg = NULL;

  LCFGStatus status = LCFG_STATUS_OK;

  LCFGComponentList * complist = lcfgcomplist_new();

  DBC * cursor;

  /* Get a cursor */
  dbh->cursor( dbh, NULL, &cursor, 0 ); 

  DBT key, value;
  /* Initialize our DBTs. */
  memset( &key,   0, sizeof(DBT) );
  memset( &value, 0, sizeof(DBT) );

  char * this_namespace = NULL;
  char * this_compname  = NULL;
  char * this_resname   = NULL;
  char this_type        = LCFG_RESOURCE_SYMBOL_VALUE;

  char * reskey = NULL;

  int ret = 0;
  /* Iterate over the database, retrieving each record in turn. */
  while ( ( ret = cursor->get( cursor, &key, &value, DB_NEXT ) ) == 0 ) {

    free(reskey);
    reskey = NULL;

    if ( (size_t) key.size > 0 )
      reskey = strndup( (char *) key.data, (size_t) key.size );

    /* Ignore unnecessary 'resource list' entries for components */
    if ( reskey == NULL || strchr( reskey, '.' ) == NULL )
      continue;

    if ( !lcfgresource_parse_key( reskey, &this_namespace, &this_compname,
                                  &this_resname, &this_type ) ) {
      status = LCFG_STATUS_ERROR;
      asprintf( msg, "Invalid DB entry '%s'", reskey );
      break;
    }

    /* If a namespace is specified then it must match */
    if ( namespace != NULL ) {
      if ( this_namespace == NULL ||
           strcmp( namespace, this_namespace ) != 0 ) {
        status = LCFG_STATUS_ERROR;
        asprintf( msg, "Invalid DB entry '%s' (invalid namespace '%s')",
                  reskey, this_namespace );
        break;
      }
    }

    /* If not in the list of 'wanted' components just move on to next entry */
    if ( comps_wanted != NULL &&
         strcmp( this_compname, "profile" ) != 0 &&
         !lcfgtaglist_contains( comps_wanted, this_compname ) )
      continue;

    /* Validate both the component and resource names here to avoid
       the possibility of creating unnecessary data structures which
       would then have to be thrown away. */

    if ( !lcfgcomponent_valid_name(this_compname) ) {
      status = LCFG_STATUS_ERROR;
      asprintf( msg, "Invalid DB entry '%s' (invalid component name '%s')",
                reskey, this_compname );
      break;
    }

    if ( !lcfgresource_valid_name(this_resname) ) {
      status = LCFG_STATUS_ERROR;
      asprintf( msg, "Invalid DB entry '%s' (invalid resource name '%s')",
                reskey, this_resname );
      break;
    }

    LCFGComponent * comp = NULL;
    LCFGResource * res   = NULL;

    comp = lcfgcomplist_find_or_create_component( complist, this_compname ); 
    if ( comp == NULL ) {
      asprintf( msg, "Failed to load LCFG component '%s'",
                this_compname );
      status = LCFG_STATUS_ERROR;
      break;
    }

    res = lcfgcomponent_find_or_create_resource( comp, this_resname, true );
    if ( res == NULL ) {
      asprintf( msg, "Failed to load LCFG resource '%s.%s'",
                this_compname, this_resname );
      status = LCFG_STATUS_ERROR;
      break;
    }

    char * this_value = strndup( (char *) value.data, (size_t) value.size );
    char * set_msg = NULL;
    if ( !lcfgresource_set_attribute( res, this_type, this_value,
                                      &set_msg ) ) {

      status = LCFG_STATUS_ERROR;
      if ( set_msg != NULL ) {
        *msg = lcfgresource_build_message( res, this_compname,
                                           set_msg );

        free(set_msg);
      } else {
        *msg = lcfgresource_build_message( res, this_compname,
                                  "Failed to set '%s' for attribute type '%c'",
                                           this_value, this_type );
      }

      free(this_value);

      break;
    }

  }

  free(reskey);

  /* Cursors must be closed */
  if ( cursor != NULL ) 
    cursor->close(cursor);

  if ( status != LCFG_STATUS_OK ) {

    if ( *msg == NULL )
      asprintf( msg, "Something bad happened whilst processing DB." );

    lcfgcomplist_relinquish(complist);
    complist = NULL;
  }

  *result = complist;

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

  DB *dbp;               /* DB structure handle */
  int ret;               /* function return value */

  /* Initialize the structure. This
   * database is not opened in an environment, 
   * so the environment pointer is NULL. */
  ret = db_create( &dbp, NULL, 0 );
  if (ret != 0) {
    asprintf( msg, "Failed to initialise DB: %s\n", db_strerror(ret) );
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
    asprintf( msg, "Failed to open DB '%s': %s\n",
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

  FILE * file;
  if ( ( file = fopen(filename, "r") ) == NULL ) {

    if (errno == ENOENT) {
      asprintf( msg, "File '%s' does not exist.\n", filename );
    } else {
      asprintf( msg, "File '%s' is not readable.\n", filename );
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

