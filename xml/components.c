#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libxml/xmlreader.h>

#include "xml.h"

LCFGStatus lcfgxml_process_component( xmlTextReaderPtr reader,
				      const char * compname,
				      LCFGComponent ** result,
				      const char * base_context,
				      const char * base_derivation,
				      const LCFGContextList * ctxlist,
				      char ** errmsg ) {

  *errmsg = NULL;
  *result = NULL; /* guarantee this is NULL if anything fails */

  if (xmlTextReaderIsEmptyElement(reader)) return LCFG_STATUS_OK; /* Nothing to do */

  LCFGStatus status = LCFG_STATUS_OK;

  LCFGComponent * lcfgcomp = lcfgcomponent_new();
  char * new_name = strdup(compname);
  if ( !lcfgcomponent_set_name( lcfgcomp, new_name ) ) {
    free(new_name);
    lcfgxml_set_error_message( errmsg,
                               "Invalid LCFG component name '%s'", compname );
    status = LCFG_STATUS_ERROR;
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
                                           ctxlist, errmsg );

	free(tagname);
	tagname = NULL;

      } else {
        status = LCFG_STATUS_ERROR;

        xmlChar * nodename  = xmlTextReaderName(reader);

        lcfgxml_set_error_message( errmsg, "Unexpected element '%s' of type %d at line %d whilst processing component.", nodename, nodetype, linenum );

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
          status = LCFG_STATUS_ERROR;

          lcfgxml_set_error_message( errmsg, "Unexpected end element '%s' at line %d whilst processing component.", nodename, linenum );

        }

      } else if ( nodetype != XML_READER_TYPE_WHITESPACE &&
                  nodetype != XML_READER_TYPE_SIGNIFICANT_WHITESPACE ) {

        status = LCFG_STATUS_ERROR;

        lcfgxml_set_error_message( errmsg, "Unexpected element '%s' of type %d at line %d whilst processing component.", nodename, nodetype, linenum );

      }

      xmlFree(nodename);
      nodename = NULL;
    }

    /* Quit if the processing status is no longer OK */
    if ( status != LCFG_STATUS_OK )
      done = true;

    if ( !done )
      read_status = xmlTextReaderRead(reader);

  }

 cleanup:

  if ( status == LCFG_STATUS_OK ) {
    *result = lcfgcomp;
  } else {

    if ( *errmsg == NULL )
      lcfgxml_set_error_message( errmsg, "Something bad happened whilst processing component '%s'.", compname );

    lcfgcomponent_relinquish(lcfgcomp);

    *result = NULL;
  }

  return status;
}

LCFGStatus lcfgxml_process_components( xmlTextReaderPtr reader,
				       LCFGComponentList ** result,
				       const char * base_context,
				       const char * base_derivation,
				       const LCFGContextList * ctxlist,
				       const LCFGTagList * comps_wanted,
				       char ** errmsg ) {

  *errmsg = NULL;
  *result = NULL; /* guarantee this is NULL if anything fails */

  if ( !lcfgxml_correct_location( reader, LCFGXML_COMPS_PARENT_NODE ) ) {
    if ( !lcfgxml_moveto_node( reader, LCFGXML_COMPS_PARENT_NODE ) ) {
      lcfgxml_set_error_message( errmsg, "Failed to find top-level components element." );
      return LCFG_STATUS_ERROR;
    }
  }

  if (xmlTextReaderIsEmptyElement(reader)) return LCFG_STATUS_OK; /* nothing to do */

  LCFGComponentList * complist = lcfgcomplist_new();

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

        /* A copy of the most recently seen component name is stashed
           so that it can be compared each time to see if the end of
           the component block has been reached. */

        compname = (char *) xmlStrdup(nodename);

        LCFGComponent * cur_comp = NULL;
        status = lcfgxml_process_component( reader, compname, &cur_comp,
                                            base_context, base_derivation,
                                            ctxlist, errmsg );

        if ( status == LCFG_STATUS_OK ) {

          /* If the component node was empty then NULL will be returned */

          /* If no list of components is defined stash everything.
             Otherwise only stash components if name is in the list.
             Always keep the 'profile' component as it contains useful
             meta-data. */

	  if ( cur_comp != NULL &&
	       ( comps_wanted == NULL ||
		 strcmp( compname, "profile" ) == 0 ||
		 lcfgtaglist_contains( comps_wanted, compname ) ) ) {

	    if ( lcfgcomplist_append( complist, cur_comp )
		 == LCFG_CHANGE_ERROR ) {

	      lcfgxml_set_error_message( errmsg, "Failed to append component '%s' to the list of components", lcfgcomponent_get_name(cur_comp) );
	      status = LCFG_STATUS_ERROR;
	    }

	  }

        }

	lcfgcomponent_relinquish(cur_comp);

      } else {

        status = LCFG_STATUS_ERROR;

        lcfgxml_set_error_message( errmsg,"Unexpected element '%s' of type %d at line %d whilst processing components.", nodename, nodetype, linenum );


      }

    } else if ( nodetype == XML_READER_TYPE_END_ELEMENT ) {

      if ( nodedepth == topdepth &&
           xmlStrcmp( nodename, BAD_CAST LCFGXML_COMPS_PARENT_NODE ) == 0 ) {
        done = true; /* Successfully finished this block */
      } else if ( compname  == NULL         ||
                  nodedepth != topdepth + 1 ||
                  xmlStrcmp(nodename, BAD_CAST compname) != 0 ) {

        status = LCFG_STATUS_ERROR;

        lcfgxml_set_error_message( errmsg, "Unexpected end element '%s' at line %d whilst processing components.", nodename, linenum );

      }

    }

    xmlFree(nodename);
    nodename = NULL;

    /* Quit if the processing status is no longer OK */
    if ( status != LCFG_STATUS_OK )
      done = true;

    if ( !done )
      read_status = xmlTextReaderRead(reader);

  }

 cleanup:

  free(compname);
  compname = NULL;

  if ( status == LCFG_STATUS_OK ) {
    *result = complist;
  } else {

    if ( *errmsg == NULL )
      lcfgxml_set_error_message( errmsg, "Something bad happened whilst processing components." );

    lcfgcomplist_destroy(complist);

    *result = NULL;
  }

  return status;
}

/* eof */
