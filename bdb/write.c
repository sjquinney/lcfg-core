#define _GNU_SOURCE /* for asprintf */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#include <assert.h>

#include <db.h>

#include "utils.h"
#include "bdb.h"

DB * lcfgbdb_init_writer( const char * filename,
                          char ** errmsg ) {
  return lcfgbdb_open_db( filename, DB_CREATE | DB_EXCL, errmsg );
}

LCFGStatus lcfgcomponent_to_bdb( const LCFGComponent * component,
                                 const char * namespace,
                                 DB * dbh,
                                 char ** errmsg ) {

  if ( component == NULL || lcfgcomponent_is_empty(component) )
    return LCFG_STATUS_OK;

  LCFGStatus status = LCFG_STATUS_OK;

  /* A sorted list of resources is stored for each component keyed on
     the component name. It does not appear to be used but we still do
     it to ensure backwards compatibility. */

  const char * compname = lcfgcomponent_get_name(component);

  char * res_as_str = lcfgcomponent_get_resources_as_string(component);

  DBT key, data;

  /* Initialize our DBTs. */
  memset( &key,  0, sizeof(DBT) );
  memset( &data, 0, sizeof(DBT) );

  key.data = (char *) compname;
  key.size = strlen(compname);

  data.data = res_as_str;
  data.size = strlen(res_as_str);

  int ret = dbh->put( dbh, NULL, &key, &data, DB_OVERWRITE_DUP );

  free(res_as_str);
  res_as_str = NULL;

  if ( ret != 0 ) {
    status = LCFG_STATUS_ERROR;
    asprintf( errmsg, "Failed to store list of resources for component: %s\n",
              db_strerror(ret) );
    goto cleanup;
  }

  /* This is used (and reused) as a buffer for all the resource keys
     to avoid allocating and freeing lots of memory. */

  char * reskey = NULL;
  size_t buf_size = 0;
  ssize_t keylen = 0;

  LCFGResourceNode * cur_node = lcfgcomponent_head(component);
  while ( cur_node != NULL ) {
    LCFGResource * resource = lcfgcomponent_resource(cur_node);

    /* Only want to store active resources (priority >= 0) */
    if ( !lcfgresource_is_active(resource) )
      continue;

    /* Derivation */

    if ( lcfgresource_has_derivation(resource) ) {

      keylen = lcfgresource_build_key( resource,
                                       compname,
                                       namespace,
                                       LCFG_RESOURCE_SYMBOL_DERIVATION,
                                       &reskey, &buf_size );

      key.data = reskey;
      key.size = keylen;

      char * deriv = lcfgresource_get_derivation(resource);
      data.data = deriv;
      data.size = strlen(deriv);

      ret = dbh->put( dbh, NULL, &key, &data, DB_OVERWRITE_DUP );
      if ( ret != 0 ) {
        status = LCFG_STATUS_ERROR;
        asprintf( errmsg, "Failed to store resource derivation data: %s\n",
                  db_strerror(ret) );
        break;
      }

    }

    /* Type */

    if ( lcfgresource_get_type(resource) != LCFG_RESOURCE_TYPE_STRING ||
         lcfgresource_has_comment(resource) ) {

      keylen = lcfgresource_build_key( resource,
                                       compname,
                                       namespace,
                                       LCFG_RESOURCE_SYMBOL_TYPE,
                                       &reskey, &buf_size );

      key.data = reskey;
      key.size = keylen;

      char * type_as_str = lcfgresource_get_type_as_string( resource, 0 );
      data.data = type_as_str;
      data.size = strlen(type_as_str);

      ret = dbh->put( dbh, NULL, &key, &data, DB_OVERWRITE_DUP );

      free(type_as_str);

      if ( ret != 0 ) {
        status = LCFG_STATUS_ERROR;
        asprintf( errmsg, "Failed to store resource type data: %s\n",
                  db_strerror(ret) );
        break;
      }

    }

    /* Context */

    if ( lcfgresource_has_context(resource) ) {

      keylen = lcfgresource_build_key( resource,
                                       compname,
                                       namespace,
                                       LCFG_RESOURCE_SYMBOL_CONTEXT,
                                       &reskey, &buf_size );

      key.data = reskey;
      key.size = keylen;

      char * context = lcfgresource_get_context(resource);
      data.data = context;
      data.size = strlen(context);

      ret = dbh->put( dbh, NULL, &key, &data, DB_OVERWRITE_DUP );
      if ( ret != 0 ) {
        status = LCFG_STATUS_ERROR;
        asprintf( errmsg, "Failed to store resource context data: %s\n",
                  db_strerror(ret) );
        break;
      }

    }

    /* Priority */

    /* Only store the value for the priority when it is greater than
       the default (zero). */

    if ( lcfgresource_get_priority(resource) > 0 ) {

      keylen = lcfgresource_build_key( resource,
                                       compname,
                                       namespace,
                                       LCFG_RESOURCE_SYMBOL_PRIORITY,
                                       &reskey, &buf_size );

      key.data = reskey;
      key.size = keylen;

      char * prio_as_str = lcfgresource_get_priority_as_string(resource);
      data.data = prio_as_str;
      data.size = strlen(prio_as_str);

      ret = dbh->put( dbh, NULL, &key, &data, DB_OVERWRITE_DUP );

      free(prio_as_str);

      if ( ret != 0 ) {
        status = LCFG_STATUS_ERROR;
        asprintf( errmsg, "Failed to store resource priority data: %s\n",
                  db_strerror(ret) );
        break;
      }

    }

    /* Value */

    keylen = lcfgresource_build_key( resource,
                                     compname,
                                     namespace,
                                     LCFG_RESOURCE_SYMBOL_VALUE,
                                     &reskey, &buf_size );

    key.data = reskey;
    key.size = keylen;

    char * value = lcfgresource_has_value(resource) ?
                   lcfgresource_get_value(resource) : "";

    data.data = value;
    data.size = strlen(value);

    ret = dbh->put( dbh, NULL, &key, &data, DB_OVERWRITE_DUP );

    if ( ret != 0 ) {
      status = LCFG_STATUS_ERROR;
      asprintf( errmsg, "Failed to store resource value data: %s\n",
                db_strerror(ret) );
      break;
    }

    cur_node = lcfgcomponent_next(cur_node);
  }

 cleanup:

  free(reskey);

  return status;
}

LCFGStatus lcfgcomplist_to_bdb( const LCFGComponentList * components,
                                const char * namespace,
                                DB * dbh,
                                char ** errmsg ) {

  if ( components == NULL || lcfgcomplist_is_empty(components) )
    return LCFG_STATUS_OK;

  LCFGStatus status = LCFG_STATUS_OK;

  LCFGComponentNode * cur_node = lcfgcomplist_head(components);
  while ( cur_node != NULL ) {
    const LCFGComponent * component = lcfgcomplist_component(cur_node);
    
    status = lcfgcomponent_to_bdb( component,
                                   namespace,
                                   dbh,
                                   errmsg );

    if ( status != LCFG_STATUS_OK )
      break;

    cur_node = lcfgcomplist_next(cur_node);
  }

  return status;
}

LCFGStatus lcfgprofile_to_bdb( const LCFGProfile * profile,
                               const char * namespace,
                               const char * dbfile,
                               char ** errmsg ) {

  *errmsg = NULL;

  /* Only use the value for profile.node when the namespace has not
     been specified */

  char * node = NULL;
  if ( namespace == NULL ) {
    if ( !lcfgprofile_get_meta( profile, "node", &node ) ) {
      /* Just ignore problems with fetching profile.node value */
      node = NULL;
    }
  }

  LCFGStatus status = LCFG_STATUS_OK;

  /* This generates a 'safe' temporary file name in the same directory
     as the target DB file so that a rename will work. Note that this
     does not actually open the temporary file so there is a tiny
     chance something else will do so before it is opened by BDB. To
     avoid that being a security issue the DB is opened with the
     DB_EXCL flag. */

  char * dirname = lcfgutils_dirname(dbfile);
  char * tmpfile = tempnam( dirname, ".lcfg" );
  free(dirname);

  if ( tmpfile == NULL ) {
    status = LCFG_STATUS_ERROR;
    asprintf( errmsg, "Failed to generate safe temporary file name");
    goto cleanup;
  }

  char * init_errmsg = NULL;
  DB * dbh = lcfgbdb_init_writer( tmpfile, &init_errmsg );
  if ( dbh == NULL ) {
    status = LCFG_STATUS_ERROR;
    asprintf( errmsg, "Failed to initialise new DB: %s", init_errmsg );
  }

  free(init_errmsg);

  if ( status == LCFG_STATUS_ERROR ) goto cleanup;

  if ( lcfgprofile_has_components(profile) ) {
    status = lcfgcomplist_to_bdb( profile->components,
                                  ( namespace != NULL ? namespace : node ),
                                  dbh,
                                  errmsg );
  }

  /* even if the store fails we need to close the DB handle at this point */
  lcfgbdb_end_reader(dbh);

  if ( status == LCFG_STATUS_OK ) {

    if ( rename( tmpfile, dbfile ) == 0 ) {

      time_t mtime = profile->mtime;

      if ( mtime != 0 ) {
        struct utimbuf times;
        times.actime  = mtime;
        times.modtime = mtime;
        utime( dbfile, &times );
      }

    } else {
      status = LCFG_STATUS_ERROR;
      asprintf( errmsg, "Failed to rename DB file to '%s'", dbfile );
    }

  }

 cleanup:

  if ( tmpfile != NULL ) {
    unlink(tmpfile);
    free(tmpfile);
  }

  return status;
}

/* eof */
