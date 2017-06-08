/**
 * @file xml/components.c
 * @brief Functions for processing component data in LCFG XML profiles
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date: 2017-04-27 11:58:12 +0100 (Thu, 27 Apr 2017) $
 * $Revision: 32561 $
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <libxml/xmlreader.h>

#include "xml.h"

/**
 * @brief Process XML for single component
 *
 * @param[in] reader Pointer to XML reader
 * @param[in] compname Name of component
 * @param[out] result Reference to pointer to new @c LCFGComponent
 * @param[in] base_context A context which will be applied to all resources
 * @param[in] base_derivation A derivation which will be applied to all resources
 * @param[in] ctxlist An @c LCFGContextList which is used to evaluate priority
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgxml_process_component( xmlTextReaderPtr reader,
				      const char * compname,
				      LCFGComponent ** result,
				      const char * base_context,
				      const char * base_derivation,
				      const LCFGContextList * ctxlist,
				      char ** msg ) {
  assert( reader != NULL );
  assert( compname != NULL );

  *result = NULL; /* guarantee this is NULL if anything fails */

  if (xmlTextReaderIsEmptyElement(reader)) return LCFG_STATUS_OK; /* Nothing to do */

  LCFGStatus status = LCFG_STATUS_OK;

  LCFGComponent * lcfgcomp = lcfgcomponent_new();
  char * new_name = strdup(compname);
  if ( !lcfgcomponent_set_name( lcfgcomp, new_name ) ) {
    free(new_name);

    status = lcfgxml_error( msg, "Invalid LCFG component name '%s'", compname );
    goto cleanup;
  }

  if ( !lcfgcomponent_set_merge_rules( lcfgcomp, 
           LCFG_MERGE_RULE_SQUASH_IDENTICAL|LCFG_MERGE_RULE_USE_PRIORITY ) ) {
    status  = lcfgxml_error( msg,
                             "Failed to set merge rules for component '%s'",
                             compname );

    goto cleanup;
  }

  int topdepth = xmlTextReaderDepth(reader);

  bool done  = false;

  int read_status = xmlTextReaderRead(reader);
  while ( !done && read_status == 1 ) {

    int nodetype  = xmlTextReaderNodeType(reader);
    int nodedepth = xmlTextReaderDepth(reader);
    int linenum   = xmlTextReaderGetParserLineNumber(reader);

    if ( nodetype == XML_READER_TYPE_ELEMENT ) {

      if ( nodedepth == topdepth + 1 ) {

        /* start of new resource */

        char * tagname = NULL; /* should never be set */

        status = lcfgxml_process_resource( reader, lcfgcomp, NULL,
                                           &tagname, NULL,
                                           base_context, base_derivation,
                                           ctxlist, msg );

	free(tagname);
	tagname = NULL;

      } else {
        xmlChar * nodename  = xmlTextReaderName(reader);

        status = lcfgxml_error( msg, "Unexpected element '%s' of type %d at line %d whilst processing component.", nodename, nodetype, linenum );

        xmlFree(nodename);
        nodename = NULL;
      }

    } else {
      xmlChar * nodename  = xmlTextReaderName(reader);

      if ( nodetype == XML_READER_TYPE_END_ELEMENT ) {

        if ( nodedepth == topdepth &&
             xmlStrcmp(nodename, BAD_CAST lcfgcomp->name ) == 0 ) {
          done = true; /* Successfully finished this block */
        } else {
          status = lcfgxml_error( msg, "Unexpected end element '%s' at line %d whilst processing component.", nodename, linenum );
        }

      } else if ( nodetype != XML_READER_TYPE_WHITESPACE &&
                  nodetype != XML_READER_TYPE_SIGNIFICANT_WHITESPACE ) {

        status = lcfgxml_error( msg, "Unexpected element '%s' of type %d at line %d whilst processing component.", nodename, nodetype, linenum );

      }

      xmlFree(nodename);
      nodename = NULL;
    }

    /* Quit if the processing status is no longer OK */
    if ( status == LCFG_STATUS_ERROR )
      done = true;

    if ( !done )
      read_status = xmlTextReaderRead(reader);

  }

 cleanup:

  if ( status == LCFG_STATUS_ERROR ) {

    if ( *msg == NULL )
      lcfgxml_error( msg, "Something bad happened whilst processing component '%s'.", compname );

    lcfgcomponent_relinquish(lcfgcomp);
    lcfgcomp = NULL;
  }

  *result = lcfgcomp;

  return status;
}

/**
 * @brief Process XML for all components
 *
 * @param[in] reader Pointer to XML reader
 * @param[out] result Reference to pointer to new @c LCFGComponentSet
 * @param[in] base_context A context which will be applied to all resources
 * @param[in] base_derivation A derivation which will be applied to all resources
 * @param[in] ctxlist An @c LCFGContextList which is used to evaluate priority
 * @param[in] comps_wanted An @c LCFGTagList of names for the desired components
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgxml_process_components( xmlTextReaderPtr reader,
				       LCFGComponentSet ** result,
				       const char * base_context,
				       const char * base_derivation,
				       const LCFGContextList * ctxlist,
				       const LCFGTagList * comps_wanted,
				       char ** msg ) {
  assert( reader != NULL );

  *result = NULL; /* guarantee this is NULL if anything fails */

  if ( !lcfgxml_correct_location( reader, LCFGXML_COMPS_PARENT_NODE ) ) {
    if ( !lcfgxml_moveto_node( reader, LCFGXML_COMPS_PARENT_NODE ) ) {
      return lcfgxml_error( msg, "Failed to find top-level components element." );
    }
  }

  if (xmlTextReaderIsEmptyElement(reader)) return LCFG_STATUS_OK; /* nothing to do */

  LCFGComponentSet * compset = lcfgcompset_new();

  /* Need to store the depth of the components element. */

  int topdepth = xmlTextReaderDepth(reader);

  bool done = false;
  LCFGStatus status = LCFG_STATUS_OK;

  int read_status = xmlTextReaderRead(reader);

  char * compname = NULL;

  while ( !done && read_status == 1 ) {

    xmlChar * nodename  = xmlTextReaderName(reader);
    int nodetype        = xmlTextReaderNodeType(reader);
    int nodedepth       = xmlTextReaderDepth(reader);
    int linenum         = xmlTextReaderGetParserLineNumber(reader);

    if ( nodetype  == XML_READER_TYPE_ELEMENT ) {

      if ( nodedepth == topdepth + 1 ) {

        /* start of new component */

        /* name of node is name of the component */
	free(compname);
        compname = NULL;

        LCFGComponent * cur_comp = NULL;
        if ( lcfgcomponent_valid_name( (char *) nodename ) ) {

          /* A copy of the most recently seen component name is stashed
             so that it can be compared each time to see if the end of
             the component block has been reached. */

          compname = (char *) xmlStrdup(nodename);

          status = lcfgxml_process_component( reader, compname, &cur_comp,
                                              base_context, base_derivation,
                                              ctxlist, msg );

        } else {
          status = lcfgxml_error( msg, "Invalid component name '%s' found at line %d whilst processing components.",
                                  (char *) nodename, linenum );
        }

        if ( status != LCFG_STATUS_ERROR ) {

          /* If the component node was empty then NULL will be returned */

          /* If no list of components is defined stash everything.
             Otherwise only stash components if name is in the list.
             Always keep the 'profile' component as it contains useful
             meta-data. */

	  if ( cur_comp != NULL &&
	       ( comps_wanted == NULL ||
		 strcmp( compname, "profile" ) == 0 ||
		 lcfgtaglist_contains( comps_wanted, compname ) ) ) {

	    if ( lcfgcompset_insert_component( compset, cur_comp )
		 == LCFG_CHANGE_ERROR ) {

	      status = lcfgxml_error( msg, "Failed to add component '%s' to the set of components", lcfgcomponent_get_name(cur_comp) );

	    }

	  }

        }

	lcfgcomponent_relinquish(cur_comp);

      } else {

        status = lcfgxml_error( msg, "Unexpected element '%s' of type %d at line %d whilst processing components.", nodename, nodetype, linenum );

      }

    } else if ( nodetype == XML_READER_TYPE_END_ELEMENT ) {

      if ( nodedepth == topdepth &&
           xmlStrcmp( nodename, BAD_CAST LCFGXML_COMPS_PARENT_NODE ) == 0 ) {
        done = true; /* Successfully finished this block */
      } else if ( compname  == NULL         ||
                  nodedepth != topdepth + 1 ||
                  xmlStrcmp(nodename, BAD_CAST compname) != 0 ) {

        status = lcfgxml_error( msg, "Unexpected end element '%s' at line %d whilst processing components.", nodename, linenum );

      }

    }

    xmlFree(nodename);
    nodename = NULL;

    /* Quit if the processing status is no longer OK */
    if ( status == LCFG_STATUS_ERROR )
      done = true;

    if ( !done )
      read_status = xmlTextReaderRead(reader);

  }

  free(compname);
  compname = NULL;

  if ( status == LCFG_STATUS_ERROR ) {

    if ( *msg == NULL )
      lcfgxml_error( msg, "Something bad happened whilst processing components." );

    lcfgcompset_relinquish(compset);
    compset = NULL;
  }

  *result = compset;

  return status;
}

/* eof */
