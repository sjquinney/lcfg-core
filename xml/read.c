/**
 * @file xml/read.c
 * @brief Functions for reading LCFG XML profiles
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date: 2017-04-27 11:58:12 +0100 (Thu, 27 Apr 2017) $
 * $Revision: 32561 $
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <libxml/xmlreader.h>

#include "utils.h"
#include "xml.h"

/**
 * @brief Collect profile meta-data from the XML
 *
 * This collects values for the following profile meta-data:
 *
 *   - published_by
 *   - published_at
 *   - server_version
 *   - last_modified
 *   - last_modified_file
 *
 * To prevent the reader overrunning to the end of the XML, a name
 * should be given for the "stop node" which will cause the function
 * to return when a node with that name is encountered.
 *
 * @param[in] reader Pointer to XML reader
 * @param[in] stop_nodename
 * @param[in] profile Pointer to @c LCFGProfile
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgxml_collect_metadata( xmlTextReaderPtr reader,
                                     const char * stop_nodename,
                                     LCFGProfile * profile,
                                     char ** msg ) {
  assert( reader != NULL );
  assert( stop_nodename != NULL );
  assert( profile != NULL );

  xmlChar * metaname = NULL;

  bool done = false;
  LCFGStatus status = LCFG_STATUS_OK;

  int read_status = 1;
  while ( !done && read_status == 1 ) {

    xmlChar * nodename  = xmlTextReaderName(reader);
    int nodetype        = xmlTextReaderNodeType(reader);
    int nodedepth       = xmlTextReaderDepth(reader);
    int linenum         = xmlTextReaderGetParserLineNumber(reader);

    if (xmlStrcmp(nodename, BAD_CAST stop_nodename) == 0 ) {
      xmlFree(nodename);
      nodename = NULL;

      done = true;
    } else {
      if ( nodedepth == LCFGXML_ATTR_DEPTH ) {

        if ( nodetype == XML_READER_TYPE_ELEMENT ) {

          if ( metaname != NULL )
            xmlFree(metaname);

          metaname = nodename;

        } else {

          if ( ! ( nodetype == XML_READER_TYPE_END_ELEMENT &&
                   xmlStrcmp( nodename, metaname ) == 0 )  &&
               nodetype != XML_READER_TYPE_SIGNIFICANT_WHITESPACE ) {
            status = lcfgxml_error( msg, "Unexpected element '%s' of type %d at line %d whilst gathering metadata", nodename, nodetype, linenum );
          }

          xmlFree(nodename);
          nodename = NULL;
        }

      } else {

        if ( nodedepth == LCFGXML_ATTRVALUE_DEPTH &&
             xmlTextReaderHasValue(reader) ) {

          xmlChar * nodevalue = xmlTextReaderValue(reader);

          if (        xmlStrcmp( metaname, BAD_CAST "published_by"       ) == 0 ) {
            profile->published_by       = (char *) nodevalue;
          } else if ( xmlStrcmp( metaname, BAD_CAST "published_at"       ) == 0 ) {
            profile->published_at       = (char *) nodevalue;
          } else if ( xmlStrcmp( metaname, BAD_CAST "server_version"     ) == 0 ) {
            profile->server_version     = (char *) nodevalue;
          } else if ( xmlStrcmp( metaname, BAD_CAST "last_modified"      ) == 0 ) {
            profile->last_modified      = (char *) nodevalue;
          } else if ( xmlStrcmp( metaname, BAD_CAST "last_modified_file" ) == 0 ) {
            profile->last_modified_file = (char *) nodevalue;
          }

        } else {
          status = lcfgxml_error( msg, "Unexpected element '%s' of type %d at line %d whilst gathering metadata", nodename, nodetype, linenum );
        }

        xmlFree(nodename);
        nodename = NULL;
      }

    }

    /* Quit if the processing status is no longer OK */
    if ( status == LCFG_STATUS_ERROR )
      done = true;

    if ( !done )
      read_status = xmlTextReaderRead(reader);

  }

  if ( metaname != NULL ) {
    xmlFree(metaname);
    metaname = NULL;
  }

  return status;
}

/**
 * @brief Initialise the XML reader
 *
 * This will create a new XML reader for the specified file and
 * position the reader at the top @c "lcfg" node in the LCFG profile.
 *
 * If an error occurs the @c NULL value will be returned.
 *
 * @param[in] filename The name of the XML filename to be processed
 * @param[out] result Reference to pointer for new reader
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgxml_init_reader( const char * filename,
                                xmlTextReaderPtr * result,
                                char ** msg ) {
  assert( filename != NULL );

  /* 1. Firstly need to check that the file actually exists and is
        readable. Doing an fopen is better than just calling stat. */

  FILE *file;
  if ((file = fopen(filename, "r")) == NULL) {

    if (errno == ENOENT) {
      return lcfgxml_error( msg, "File '%s' does not exist.\n", filename );
    } else {
      return lcfgxml_error( msg, "File '%s' is not readable.\n", filename );
    }

  } else {
    fclose(file);
  }

  /* 2. Initialise the XML reader */

  xmlTextReaderPtr reader = xmlReaderForFile( filename, "UTF-8",
                                           XML_PARSE_DTDATTR | XML_PARSE_NOENT);
  if ( reader == NULL )
    return lcfgxml_error( msg, "Failed to initialise the LCFG XML reader." );

  /* 3. Walk to the start of the profile */

  bool move_ok = lcfgxml_moveto_node( reader, LCFGXML_TOP_NODE );
  if ( move_ok )
    move_ok = lcfgxml_moveto_next_tag( reader );

  if ( !move_ok ) {
    /* not a valid lcfg profile */
    xmlTextReaderClose(reader);
    return lcfgxml_error( msg, "Invalid LCFG XML profile." );
  }

  *result = reader;

  return LCFG_STATUS_OK;
}

/**
 * @brief Free resources associated with reader
 *
 * This will close the XML reader and free any associated resources.
 *
 * @param[in] reader Pointer to XML reader
 *
 */

void lcfgxml_end_reader(xmlTextReaderPtr reader) {

  if ( reader != NULL ) {
    xmlTextReaderClose(reader);
    xmlFreeTextReader(reader);
  }

  xmlCleanupParser();

}

/**
 * @brief Process XML for LCFG profile
 *
 * This is the top-level function for processing LCFG XML profiles. It
 * will process the data for components/resources and packages and
 * load them into a new @c LCFGProfile.
 *
 * @param[in] filename The filename for the XML profile.
 * @param[out] result Reference to pointer to new @c LCFGProfile
 * @param[in] base_context A context which will be applied to all resources
 * @param[in] base_derivation A derivation which will be applied to all resources and packages
 * @param[in] ctxlist An @c LCFGContextList which is used to evaluate priority
 * @param[in] comps_wanted An @c LCFGTagList of names for the desired components
 * @param[in] require_packages Boolean which indicates if packages are required
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgprofile_from_xml( const char * filename,
				 LCFGProfile ** result,
				 const char * base_context,
				 const char * base_derivation,
				 const LCFGContextList * ctxlist,
				 const LCFGTagList * comps_wanted,
				 bool require_packages,
				 char ** msg ) {
  assert( filename != NULL );

  /* Declare variables here that are required in cleanup stage */

  LCFGProfile * profile = NULL;

  LCFGStatus status = LCFG_STATUS_OK;

  /* Initialise the XML reader for the file */

  xmlTextReaderPtr reader = NULL;
  status = lcfgxml_init_reader( filename, &reader, msg );

  if ( status == LCFG_STATUS_ERROR ) goto cleanup;

  profile = lcfgprofile_new();

  struct stat sb;
  if ( stat( filename, &sb ) == 0 )
    profile->mtime = sb.st_mtime;

  /* First metadata group */

  status = lcfgxml_collect_metadata( reader, LCFGXML_COMPS_PARENT_NODE,
                                     profile, msg );

  if ( status == LCFG_STATUS_ERROR ) goto cleanup;

  /* components */

  /* Step from metadata over any whitespace to the <components> tag */
  if ( !lcfgxml_correct_location(reader, LCFGXML_COMPS_PARENT_NODE) )
    lcfgxml_moveto_next_tag(reader);

  if ( lcfgxml_correct_location(reader, LCFGXML_COMPS_PARENT_NODE) ) {

    status = lcfgxml_process_components( reader, &profile->components, 
                                         base_context, base_derivation,
                                         ctxlist, comps_wanted, 
                                         msg );

  } else {
    status = lcfgxml_error( msg, "Failed to find components section in LCFG XML profile." );
  }

  if ( status != LCFG_STATUS_OK ) goto cleanup;

  /* packages */

  /* Step from </components> over any whitespace to the <packages> tag */
  if ( !lcfgxml_correct_location(reader, LCFGXML_PACKAGES_PARENT_NODE) )
    lcfgxml_moveto_next_tag(reader);

  if ( lcfgxml_correct_location(reader, LCFGXML_PACKAGES_PARENT_NODE) ) {

    status = lcfgxml_process_packages( reader,
                                       &profile->active_packages,
                                       &profile->inactive_packages, 
                                       base_context, base_derivation,
                                       ctxlist, msg );

    lcfgxml_moveto_next_tag(reader); /* step to next tag after </packages> */

  } else if (require_packages) {
    status = lcfgxml_error( msg, "Failed to find packages section in LCFG XML profile." );
  }

  if ( status != LCFG_STATUS_OK ) goto cleanup;

  /* final metadata section */

  status = lcfgxml_collect_metadata( reader, LCFGXML_TOP_NODE,
                                     profile, msg );

 cleanup:

  lcfgxml_end_reader(reader);

  if ( status == LCFG_STATUS_ERROR ) {
    lcfgprofile_destroy(profile);
    profile = NULL;
  }

  *result = profile;

  return status;
}

/**
 * @brief Apply override profiles to current profile
 *
 * This can be used to import local overrides for components. The
 * specified directory is searched for override files which should be
 * named like @c component.xml. Files with names which are not valid
 * component names will be ignored, similarly if the processing of the
 * XML file fails it will be ignored. The @c LCFGComponent loaded from
 * the override file will @b completely replace any existing instance
 * of a component with the same name in the main @c LCFGProfile. This
 * is primarily intended as a useful feature for development
 * environments rather than those which are live.
 *
 * @param[in] main_profile Pointer to @c LCFGProfile
 * @param[in] override_dir The path to the directory of XML override profiles
 * @param[in] ctxlist An @c LCFGContextList of current contexts
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgprofile_overrides_xmldir( LCFGProfile * main_profile,
                                         const char * override_dir,
                                         const LCFGContextList * ctxlist,
                                         char ** msg ) {
  assert( main_profile != NULL );

  /* If the overrides directory does not exist then just return success */

  struct stat sb;
  if ( override_dir == NULL ||
       ( stat( override_dir, &sb ) != 0 && errno == ENOENT ) ) {
    return LCFG_STATUS_OK;
  }

  DIR * dh = opendir(override_dir);
  if ( dh == NULL ) {
    return lcfgxml_error( msg, "XML override directory '%s' is not accessible",
                          override_dir );
  }

  LCFGStatus status = LCFG_STATUS_OK;

  struct dirent * entry;

  while( ( entry = readdir(dh) ) != NULL ) {

    if ( *( entry->d_name ) == '.' ) continue; /* ignore any dot-files */

    /* Looking for any file with a .xml suffix but must also have a
       basename which is used as the name of the component to
       override */

    if ( lcfgutils_endswith( entry->d_name, ".xml" ) ) {

      char * comp_name = lcfgutils_basename( entry->d_name, ".xml" );
      if ( !lcfgcomponent_valid_name(comp_name) ) {
        /* ignoring files with invalid names */

        free(comp_name);
        comp_name = NULL;

        continue;
      }

      char * fullpath = lcfgutils_catfile( override_dir, entry->d_name );

      if ( stat( fullpath, &sb ) == 0 && S_ISREG(sb.st_mode) ) {

	/* only store this component */
	LCFGTagList * comps_wanted = lcfgtaglist_new();
        char * tagmsg = NULL;
	if ( lcfgtaglist_mutate_add( comps_wanted, comp_name, &tagmsg ) 
             == LCFG_CHANGE_ERROR ) {
          status = lcfgxml_error( msg, 
                               "Failed to create list of required components: ",
                                  tagmsg );
        }
        free(tagmsg);

        LCFGProfile * override_profile = NULL;
        if ( status != LCFG_STATUS_ERROR ) {
          status = lcfgprofile_from_xml( fullpath,
                                         &override_profile,/* result */
                                         NULL,            /* base context */
                                         fullpath,        /* base derivation */
                                         ctxlist,         /* current contexts */
                                         comps_wanted,
                                         false,           /* no packages */
                                         msg );
        }

        if ( status != LCFG_STATUS_ERROR ) {
          LCFGChange transplant_rc
	    = lcfgprofile_transplant_components( main_profile,
						 override_profile,
						 msg );
	  if ( transplant_rc == LCFG_CHANGE_ERROR )
            status = LCFG_STATUS_ERROR;
        }

	lcfgprofile_destroy(override_profile);

        /* warn but do not fail entire processing */
        if ( status == LCFG_STATUS_ERROR )
          fprintf( stderr, "Failed to process '%s': %s\n", fullpath, *msg );

	free(*msg);
	*msg = NULL;

	lcfgtaglist_relinquish(comps_wanted);
      }

      free(comp_name);
      free(fullpath);

    }

  }

  closedir(dh);

  return status;
}

/**
 * @brief Apply context-specific overrides to current profile
 *
 * The LCFG client supports the use of context-specific XML override
 * profiles. A context-specific profile can be used to add or modify
 * components and resources when a particular context is enabled.
 *
 * The path to the context profile is based on a combination of the
 * values for the @e name and @e value attributes for the context with
 * a @c .xml suffix (see @c lcfgcontext_profile_path() for details).
 *
 * This function will attempt to import override XML profiles for all
 * current contexts. If they do not exist or an error occurs during
 * processing the file will be ignored. Note that this can only be
 * used to modify existing components, it is not possible to add
 * entirely new components.
 *
 * @param[in] main_profile Pointer to @c LCFGProfile
 * @param[in] override_dir The path to the directory of XML context overrides
 * @param[in] ctxlist An @c LCFGContextList of current contexts
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgprofile_overrides_context( LCFGProfile * main_profile,
					  const char * override_dir,
                                          LCFGContextList * ctxlist,
                                          char ** msg ) {
  assert( main_profile != NULL );
  assert(override_dir != NULL );

  if ( lcfgctxlist_is_empty(ctxlist) ) return LCFG_STATUS_OK;

  LCFGStatus status = LCFG_STATUS_OK;

  /* Always apply any context overrides in order of priority */

  lcfgctxlist_sort_by_priority(ctxlist);

  LCFGContextNode * cur_node = NULL;
  for ( cur_node = lcfgctxlist_head(ctxlist);
        cur_node != NULL && status != LCFG_STATUS_ERROR;
        cur_node = lcfgctxlist_next(cur_node) ) {

    const LCFGContext * ctx = lcfgctxlist_context(cur_node);
    char * ctxvarfile = lcfgcontext_profile_path( ctx, override_dir, ".xml" );

    /* Not all contexts can be associated with valid filenames */
    if ( ctxvarfile == NULL ) continue;

    /* Ignore any files which do not have a .xml suffix */
    if ( !lcfgutils_endswith( ctxvarfile, ".xml" ) ) continue;

    struct stat sb;
    if ( stat( ctxvarfile, &sb ) != 0 || !S_ISREG(sb.st_mode) ) continue;

    size_t buf_size   = 0;
    char * ctx_as_str = NULL;
    ssize_t ctx_len = lcfgcontext_to_string( ctx, LCFG_OPT_NONE,
                                             &ctx_as_str, &buf_size );
    if ( ctx_len < 0 ) {
      lcfgutils_build_message( msg, "Failed to convert context to string" );
      status = LCFG_STATUS_ERROR;
    }

    if ( status != LCFG_STATUS_ERROR ) {
      char * import_msg = NULL;

      LCFGProfile * ctx_profile = NULL;
      status = lcfgprofile_from_xml( ctxvarfile,
                                     &ctx_profile,
                                     ctx_as_str, /* base context */
                                     ctxvarfile, /* base derivation */
                                     ctxlist,    /* current contexts */
                                     NULL,       /* store ALL components */
                                     false,      /* packages not required */
                                     &import_msg );

      if ( status != LCFG_STATUS_ERROR ) {
        bool take_new_comps = false;
        LCFGChange merge_rc = lcfgprofile_merge( main_profile, ctx_profile,
                                                 take_new_comps, &import_msg );

	if ( merge_rc == LCFG_CHANGE_ERROR )
          status = LCFG_STATUS_ERROR;
      }

      if ( status == LCFG_STATUS_ERROR ) {
        /* warn but do not fail entire processing */
        fprintf( stderr, "Failed to process '%s': %s\n", ctxvarfile,
                 import_msg );
      }

      free(import_msg);

      lcfgprofile_destroy(ctx_profile);

      free(ctx_as_str);
    }

    free(ctxvarfile);

  }

  return status;
}


/* eof */
