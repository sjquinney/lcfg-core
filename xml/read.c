#define _GNU_SOURCE /* for asprintf */

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <libxml/xmlreader.h>

#include "xml.h"
#include "utils.h"

LCFGStatus lcfgxml_collect_metadata( xmlTextReaderPtr reader,
                                     const char * stop_nodename,
                                     LCFGProfile * profile,
                                     char ** errmsg ) {

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
            lcfgxml_set_error_message( errmsg, "Unexpected element '%s' of type %d at line %d whilst gathering metadata", nodename, nodetype, linenum );

            status = LCFG_STATUS_ERROR;
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
          lcfgxml_set_error_message( errmsg,"Unexpected element '%s' of type %d at line %d whilst gathering metadata", nodename, nodetype, linenum );

          status = LCFG_STATUS_ERROR;
        }

        xmlFree(nodename);
        nodename = NULL;
      }

    }

    /* Quit if the processing status is no longer OK */
    if ( status != LCFG_STATUS_OK )
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

xmlTextReaderPtr lcfgxml_init_reader(const char *filename, char ** errmsg ) {

  /* 1. Firstly need to check that the file actually exists and is
        readable. Doing an fopen is better than just calling stat. */

  FILE *file;
  if ((file = fopen(filename, "r")) == NULL) {

    if (errno == ENOENT) {
      lcfgxml_set_error_message( errmsg, "File '%s' does not exist.\n", filename );
    } else {
      lcfgxml_set_error_message( errmsg, "File '%s' is not readable.\n", filename );
    }
    return NULL;

  } else {
    fclose(file);
  }


  /* 2. Initialise the XML reader */

  xmlTextReaderPtr reader = xmlReaderForFile( filename, "UTF-8", XML_PARSE_DTDATTR | XML_PARSE_NOENT);
  if ( reader == NULL ) {
    lcfgxml_set_error_message( errmsg, "Failed to initialise the LCFG XML reader." );
    return NULL;
  }

  /* 3. Walk to the start of the profile */

  bool move_ok = lcfgxml_moveto_node( reader, LCFGXML_TOP_NODE );
  if ( move_ok ) {
    move_ok = lcfgxml_moveto_next_tag( reader );
  }

  if ( !move_ok ) {
    /* not a valid lcfg profile */
    xmlTextReaderClose(reader);
    lcfgxml_set_error_message( errmsg, "Invalid LCFG XML profile." );
    return NULL;
  }

  return reader;
}

static void lcfgxml_end_reader(xmlTextReaderPtr reader) {

  xmlTextReaderClose(reader);
  xmlFreeTextReader(reader);
  xmlCleanupParser();

}

LCFGStatus lcfgprofile_from_xml( const char * filename,
				 LCFGProfile ** result,
				 const char * base_context,
				 const char * base_derivation,
				 const LCFGContextList * ctxlist,
				 const LCFGTagList * comps_wanted,
				 bool require_packages,
				 char ** errmsg ) {

  *errmsg = NULL;
  *result = NULL;

  LCFGProfile * profile = lcfgprofile_new();

  struct stat sb;
  if ( stat( filename, &sb ) == 0 )
    profile->mtime = sb.st_mtime;

  LCFGStatus status = LCFG_STATUS_OK;

  xmlTextReaderPtr reader = lcfgxml_init_reader( filename, errmsg );
  if ( reader == NULL ) {
    status = LCFG_STATUS_ERROR;
    goto cleanup;
  }

  /* First metadata group */

  status = lcfgxml_collect_metadata( reader, LCFGXML_COMPS_PARENT_NODE,
                                     profile, errmsg );

  if ( status != LCFG_STATUS_OK )
    goto cleanup;

  /* components */

  /* Step from metadata over any whitespace to the <components> tag */
  if ( !lcfgxml_correct_location(reader, LCFGXML_COMPS_PARENT_NODE) ) {
    lcfgxml_moveto_next_tag(reader);
  }

  if ( lcfgxml_correct_location(reader, LCFGXML_COMPS_PARENT_NODE) ) {

    status = lcfgxml_process_components( reader, &profile->components, 
                                         base_context, base_derivation,
                                         ctxlist, comps_wanted, 
                                         errmsg );

  } else {
    lcfgxml_set_error_message( errmsg, "Failed to find components section in LCFG XML profile." );
    status = LCFG_STATUS_ERROR;
  }

  if ( status != LCFG_STATUS_OK )
    goto cleanup;

  /* packages */

  /* Step from </components> over any whitespace to the <packages> tag */
  if ( !lcfgxml_correct_location(reader, LCFGXML_PACKAGES_PARENT_NODE) ) {
    lcfgxml_moveto_next_tag(reader);
  }

  if ( lcfgxml_correct_location(reader, LCFGXML_PACKAGES_PARENT_NODE) ) {

    status = lcfgxml_process_packages( reader,
                                       &profile->active_packages,
                                       &profile->inactive_packages, 
                                       base_context, base_derivation,
                                       ctxlist, errmsg );

    lcfgxml_moveto_next_tag(reader); /* step to next tag after </packages> */

  } else if (require_packages) {
    lcfgxml_set_error_message( errmsg, "Failed to find packages section in LCFG XML profile." );
    status = LCFG_STATUS_ERROR;
  }

  if ( status != LCFG_STATUS_OK )
    goto cleanup;

  /* final metadata section */

  status = lcfgxml_collect_metadata( reader, LCFGXML_TOP_NODE,
                                     profile, errmsg );

 cleanup:

  lcfgxml_end_reader(reader);

  if ( status == LCFG_STATUS_OK ) {
    *result = profile;
  } else {
    lcfgprofile_destroy(profile);
    *result = NULL;
  }

  return status;
}

LCFGStatus lcfgprofile_overrides_xmldir( LCFGProfile * main_profile,
                                         const char * override_dir,
                                         const LCFGContextList * ctxlist,
                                         char ** msg ) {

  /* If the overrides directory does not exist then just return success */

  struct stat sb;
  if ( override_dir == NULL ||
       ( stat( override_dir, &sb ) != 0 && errno == ENOENT ) ) {
    return LCFG_STATUS_OK;
  }

  DIR * dh = opendir(override_dir);
  if ( dh == NULL ) {
    lcfgxml_set_error_message( msg,
			       "XML override directory '%s' is not accessible",
			       override_dir );
    return LCFG_STATUS_ERROR;
  }

  struct dirent * entry;

  while( ( entry = readdir(dh) ) != NULL ) {

    if ( *( entry->d_name ) == '.' ) continue; /* ignore any dot-files */

    /* Looking for any file with a .xml suffix but must also have a
       basename which is used as the name of the component to
       override */

    if ( lcfgutils_endswith( entry->d_name, ".xml" ) ) {

      char * comp_name = lcfgutils_basename( entry->d_name, ".xml" );
      if ( !lcfgcomponent_valid_name(comp_name) ) {
        lcfgxml_set_error_message( msg, 
             "Ignoring override profile '%s' for invalid component name '%s'.",
                                   entry->d_name, comp_name );
        free(comp_name);
        comp_name = NULL;

        continue;
      }

      char * fullpath = lcfgutils_catfile( override_dir, entry->d_name );

      if ( stat( fullpath, &sb ) == 0 && S_ISREG(sb.st_mode) ) {

        printf("Processing profile '%s' for component '%s'\n",
               fullpath, comp_name );

        bool ok = true;

	/* only store this comp */
	LCFGTagList * comps_wanted = lcfgtaglist_new();
        char * tagmsg = NULL;
	if ( lcfgtaglist_mutate_add( comps_wanted, comp_name, &tagmsg ) 
             == LCFG_CHANGE_ERROR ) {
          ok = false;
          lcfgxml_set_error_message( msg, 
                               "Failed to create list of required components: ",
                                     tagmsg );
        }
        free(tagmsg);

        LCFGProfile * override_profile = NULL;
        if (ok) {
          LCFGStatus rc =
            lcfgprofile_from_xml( fullpath, &override_profile,
                                  NULL,      /* base context */
                                  fullpath,  /* base derivation */
                                  ctxlist,   /* current contexts */
                                  comps_wanted,
                                  false,     /* no packages */
                                  msg );

          if ( rc == LCFG_STATUS_ERROR )
            ok = false;

        }

        if (ok) {
          LCFGChange transplant_rc
	    = lcfgprofile_transplant_components( main_profile,
						 override_profile,
						 msg );
	  if ( transplant_rc == LCFG_CHANGE_ERROR )
	    ok = false;
        }

	lcfgprofile_destroy(override_profile);

        if (!ok) {
          /* warn but do not fail entire processing */
          fprintf( stderr, "Failed to process '%s': %s\n", fullpath, *msg );
        }

	free(*msg);
	*msg = NULL;

	lcfgtaglist_relinquish(comps_wanted);
      }

      free(comp_name);
      free(fullpath);

    }

  }

  closedir(dh);

  return LCFG_STATUS_OK;
}

LCFGStatus lcfgprofile_overrides_context( LCFGProfile * main_profile,
					  const char * override_dir,
                                          LCFGContextList * ctxlist,
                                          char ** msg ) {

  *msg = NULL;

  if ( ctxlist == NULL || lcfgctxlist_is_empty(ctxlist) )
    return LCFG_STATUS_OK;

  bool ok = true;

  /* Always apply any context overrides in order of priority */

  lcfgctxlist_sort_by_priority(ctxlist);

  LCFGContextNode * cur_node = lcfgctxlist_head(ctxlist);

  while ( ok && cur_node != NULL ) {
    const LCFGContext * ctx = lcfgctxlist_context(cur_node);
    char * ctxvarfile = lcfgcontext_profile_path( ctx, override_dir, ".xml" );

    /* Not all contexts can be associated with valid filenames */
    if ( ctxvarfile == NULL ) continue;

    /* Ignore any files which do not have a .xml suffix */
    if ( !lcfgutils_endswith( ctxvarfile, ".xml" ) ) continue;

    fprintf( stderr, "Checking for context file '%s'\n", ctxvarfile );

    struct stat sb;
    if ( stat( ctxvarfile, &sb ) == 0 && S_ISREG(sb.st_mode) ) {

      size_t buf_size   = 0;
      char * ctx_as_str = NULL;
      ssize_t ctx_len = lcfgcontext_to_string( ctx, LCFG_OPT_NONE,
					       &ctx_as_str, &buf_size );
      if ( ctx_len < 0 ) {
	asprintf( msg, "Failed to convert context to string" );
	ok = false;
      }

      if (ok) {
	char * parse_msg = NULL;

	LCFGProfile * ctx_profile;
	LCFGStatus rc =
	  lcfgprofile_from_xml( ctxvarfile, &ctx_profile,
				ctx_as_str, /* base context */
				ctxvarfile, /* base derivation */
				ctxlist,    /* current contexts */
				NULL,       /* store ALL components */
				false,      /* packages not required */
				&parse_msg );

	if ( rc == LCFG_STATUS_OK ) {
          bool take_new_comps = false;
	  rc = lcfgprofile_merge( main_profile, ctx_profile,
                                  take_new_comps, &parse_msg );
	}

	if ( rc != LCFG_STATUS_OK ) {
	  /* warn but do not fail entire processing */
	  fprintf( stderr, "Failed to process '%s': %s\n", ctxvarfile,
		   parse_msg );
	}

	free(parse_msg);

	lcfgprofile_destroy(ctx_profile);
      }

      free(ctx_as_str);
    }

    free(ctxvarfile);

    if (!ok) break;

    cur_node = lcfgctxlist_next(cur_node);
  }

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/* eof */
