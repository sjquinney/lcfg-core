/**
 * @file xml/resource.c
 * @brief Functions for processing resource data in LCFG XML profiles
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date: 2017-04-27 11:58:12 +0100 (Thu, 27 Apr 2017) $
 * $Revision: 32561 $
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <libxml/xmlreader.h>

#include "xml.h"
#include "utils.h"

/**
 * Get the resource name for a node
 *
 * This gets the name for the resource from the @c cfg:name attribute
 * for the current node. If the name begins with an @c '_'
 * (underscore) followed by a digit those characters will be removed
 * from the name.
 *
 * @param[in] reader Pointer to XML reader
 *
 * @return Resource name
 *
 */

static char * get_lcfgtagname( xmlTextReaderPtr reader ) {
  assert( reader != NULL );

  char * tagname = NULL;

  if ( xmlTextReaderHasAttributes(reader) ) {

    xmlChar * attrvalue = xmlTextReaderGetAttribute(reader,BAD_CAST "cfg:name");
    if ( attrvalue != NULL ) {

      tagname = (char *) attrvalue;
      size_t length = strlen(tagname);

      /* Due to a misunderstanding of the XML spec the LCFG server
         prepends an underscore to the value of the name attribute
         when the first character is one of [0-9_]. For compatibility
         this code does the unescaping */

      if ( length >= 2 && tagname[0] == '_' ) {

        /* one character shorter but add one for null byte */
        tagname = memmove( tagname, tagname + 1, length );
      }

    }

  }

  return tagname;
}

/**
 * @brief Collect resource information from attributes
 *
 * Collects the following resource information from the attributes for
 * the current node:
 *
 *   - derivation (@c cfg:derivation)
 *   - context (@c cfg:context)
 *   - type (@c cfg:type)
 *   - template (@c cfg:template)
 *
 * @param[in] reader Pointer to XML reader
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] compname Name of component
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

static LCFGStatus lcfgxml_gather_resource_attributes( xmlTextReaderPtr reader,
                                                      LCFGResource * res,
                                                      const char * compname,
                                                      char ** msg ) {
  assert( reader != NULL );
  assert( res != NULL );

  if ( !xmlTextReaderHasAttributes(reader) ) return LCFG_STATUS_OK;

  LCFGStatus status = LCFG_STATUS_OK;

  /* Do the derivation first so that any errors after this point get
     the derivation information attached. */

  xmlChar * derivation =
    xmlTextReaderGetAttribute( reader, BAD_CAST "cfg:derivation" );
  if ( derivation != NULL && xmlStrlen(derivation) > 0 ) {

    if ( !lcfgresource_add_derivation_string( res, (char *) derivation ) ) {
      status = LCFG_STATUS_ERROR;
      *msg = lcfgresource_build_message( res, compname,
                                         "Invalid derivation '%s'",
                                         (char *) derivation );
    }

  }

  xmlFree(derivation);
  derivation = NULL;

  if ( status == LCFG_STATUS_ERROR ) return status;

  /* Context Expression */

  xmlChar * context =
    xmlTextReaderGetAttribute( reader, BAD_CAST "cfg:context" );
  if ( context != NULL && xmlStrlen(context) > 0 ) {

    if ( lcfgresource_has_context(res) ) {

      if ( !lcfgresource_add_context( res, (char *) context ) )
        status = LCFG_STATUS_ERROR;

    } else {

      if ( lcfgresource_set_context( res, (char *) context ) )
        context = NULL; /* Resource now "owns" context string */
      else
        status = LCFG_STATUS_ERROR;

    }

    if ( status == LCFG_STATUS_ERROR ) {
      *msg = lcfgresource_build_message( res, compname,
                                         "Invalid context '%s'", 
                                         (char *) context );
    }

  }

  xmlFree(context);
  context = NULL;

  if ( status == LCFG_STATUS_ERROR ) return status;

  /* Type */

  xmlChar * type =
    xmlTextReaderGetAttribute( reader, BAD_CAST "cfg:type" );
  if ( type != NULL && xmlStrlen(type) > 0 ) {

    char * type_msg = NULL;
    if ( !lcfgresource_set_type_as_string( res, (char *) type,
                                           &type_msg ) ) {
      *msg = lcfgresource_build_message( res, compname,
                                         "Invalid type '%s': %s",
                                         (char *) type,
                                         type_msg );
      status = LCFG_STATUS_ERROR;

      free(type_msg);

    }

  }

  xmlFree(type);
  type = NULL;

  if ( status == LCFG_STATUS_ERROR ) return status;

  /* Template */

  xmlChar * template =
    xmlTextReaderGetAttribute( reader, BAD_CAST "cfg:template" );
  if ( template != NULL ) {

    if ( !lcfgresource_is_list(res) ) {

      if ( !lcfgresource_set_type( res, LCFG_RESOURCE_TYPE_LIST ) ) {
        *msg = lcfgresource_build_message( res, compname,
                                           "Failed to set type to 'list'" );
        status = LCFG_STATUS_ERROR;
      }

    }

    if ( xmlStrlen(template) > 0 ) {

      char * tmpl_msg = NULL;
      if ( !lcfgresource_set_template_as_string( res, (char *) template,
                                                 &tmpl_msg ) ) {
        status = LCFG_STATUS_ERROR;

        *msg = lcfgresource_build_message( res, compname,
                                           "Invalid template '%s': %s",
                                           (char *) template,
                                           tmpl_msg );
      }

      free(tmpl_msg);

    }

    xmlFree(template);
    template = NULL;

  }

  return status;
}

/**
 * Process XML for a single resource record
 *
 * Child resources for a list resource may be serialised as
 * "records". This function can be used to process a record and build
 * a resource.
 *
 * @param[in] reader Pointer to XML reader
 * @param[in] lcfgcomp Pointer to current @c LCFGComponent
 * @param[in] templates Pointer to any @c LCFGTemplate (may be @c NULL)
 * @param[out] thistag Tag name for the resource (passed back to parent)
 * @param[in] ancestor_tags An @c LCFGTagList (used to build resource name)
 * @param[in] base_context A context which will be applied to the resource
 * @param[in] base_derivation A derivation which will be applied to the resource
 * @param[in] ctxlist An @c LCFGContextList which is used to evaluate priority
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

static LCFGStatus lcfgxml_process_record( xmlTextReaderPtr reader,
                                          LCFGComponent * lcfgcomp,
                                          const LCFGTemplate * templates,
                                          char ** thistag,
                                          const LCFGTagList * ancestor_tags,
                                          const char * base_context,
                                          const char * base_derivation,
                                          const LCFGContextList * ctxlist,
                                          char ** msg ) {
  assert( reader != NULL );
  assert( lcfgcomp != NULL );

  *thistag = NULL;

  int topdepth          = xmlTextReaderDepth(reader);
  xmlChar * record_name = xmlTextReaderName(reader);

  LCFGStatus status = LCFG_STATUS_OK;

  /* Process tags */

  /* The current_tags list is passed as the ancestor_tags parameter
     when calling process_resource. */

  LCFGTagList * current_tags = NULL; /* Created when required */
  if ( ancestor_tags != NULL )
    current_tags = lcfgtaglist_clone(ancestor_tags);

  /* A record ALWAYS has a cfg:name attribute */

  char * tagname = get_lcfgtagname(reader);
  if ( tagname != NULL ) {
    *thistag = tagname; /* Will be freed by the caller */

    if ( current_tags == NULL )
      current_tags = lcfgtaglist_new();

    char * tagmsg = NULL;
    if ( lcfgtaglist_mutate_append( current_tags, tagname, &tagmsg )
         == LCFG_CHANGE_ERROR ) {
      status = lcfgxml_error( msg, "Failed to append to list of tags: %s", tagmsg );
    }
    free(tagmsg);

    if ( status == LCFG_STATUS_ERROR ) goto cleanup;

  } else {

    status = lcfgxml_error( msg, "Missing cfg:name attribute for '%s' record node", record_name );
    goto cleanup;

  }

  bool done  = false;

  int read_status = xmlTextReaderRead(reader);
  while ( !done && read_status == 1 ) {

    int nodetype  = xmlTextReaderNodeType(reader);
    int nodedepth = xmlTextReaderDepth(reader);
    int linenum   = xmlTextReaderGetParserLineNumber(reader);

    if ( nodedepth == topdepth + 1 ) {
      if ( nodetype == XML_READER_TYPE_ELEMENT ) {

        char * child_tagname = NULL;
        status = lcfgxml_process_resource( reader, lcfgcomp,
                                           templates,
                                           &child_tagname, current_tags,
                                           base_context, base_derivation,
                                           ctxlist, msg );

	free(child_tagname);
	child_tagname = NULL;

      } else if ( nodetype != XML_READER_TYPE_WHITESPACE &&
                  nodetype != XML_READER_TYPE_SIGNIFICANT_WHITESPACE ) {

        xmlChar * nodename  = xmlTextReaderName(reader);
        status = lcfgxml_error( msg, "Unexpected element '%s' of type %d at line '%d' whilst processing record.", nodename, nodetype, linenum );
        xmlFree(nodename);
        nodename = NULL;

      }

    } else {
      xmlChar * nodename  = xmlTextReaderName(reader);

      if ( nodedepth == topdepth && nodetype == XML_READER_TYPE_END_ELEMENT ) {

        if ( xmlStrcmp( nodename, record_name ) == 0 ) {
          done = true; /* Successfully finished this block */
        } else {
          status = lcfgxml_error( msg, "Unexpected end element '%s' at line '%d' whilst processing package.", nodename, linenum );
        }

      } else {
        status = lcfgxml_error( msg, "Unexpected element '%s' of type %d at line '%d' whilst processing package.", nodename, nodetype, linenum);
      }

      xmlFree(nodename);
      nodename = NULL;
    }

    /* Quit if the processing if an error occurred */
    if ( status == LCFG_STATUS_ERROR )
      done = true;

    if ( !done )
      read_status = xmlTextReaderRead(reader);

  }

 cleanup:

  xmlFree(record_name);
  record_name = NULL;

  lcfgtaglist_relinquish(current_tags);

  if ( status == LCFG_STATUS_ERROR && *msg == NULL )
    lcfgxml_error( msg, "Something bad happened whilst processing record." );

  return status;
}


/**
 * @brief Process XML for a single resource
 *
 * @param[in] reader Pointer to XML reader
 * @param[in] lcfgcomp Pointer to current @c LCFGComponent
 * @param[in] templates Pointer to any @c LCFGTemplate (may be @c NULL)
 * @param[out] thistag Tag name for the resource (passed back to parent)
 * @param[in] ancestor_tags An @c LCFGTagList (used to build resource name)
 * @param[in] base_context A context which will be applied to the resource
 * @param[in] base_derivation A derivation which will be applied to the resource
 * @param[in] ctxlist An @c LCFGContextList which is used to evaluate priority
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgxml_process_resource( xmlTextReaderPtr reader,
                                     LCFGComponent * lcfgcomp,
                                     const LCFGTemplate * templates,
                                     char ** thistag,
                                     const LCFGTagList * ancestor_tags,
                                     const char * base_context,
                                     const char * base_derivation,
                                     const LCFGContextList * ctxlist,
                                     char ** msg ) {
  assert( reader != NULL );
  assert( lcfgcomp != NULL );

  *thistag = NULL;

  if (xmlTextReaderIsEmptyElement(reader)) return LCFG_STATUS_OK;

  if ( !lcfgcomponent_is_valid(lcfgcomp) )
    return lcfgxml_error( msg, "Invalid component" );

  const char * compname = lcfgcomponent_get_name(lcfgcomp);

  int topdepth          = xmlTextReaderDepth(reader);
  xmlChar * resnodename = xmlTextReaderName(reader);

  /* The resource and child_tags variables are declared here as the
     jump to 'cleanup' could otherwise leave them undefined. */

  LCFGResource * resource = NULL;
  LCFGTagList * child_tags = NULL; /* Created when required */

  LCFGStatus status = LCFG_STATUS_OK;

  /* Process tags */

  /* The current_tags list is passed as the ancestor_tags parameter
     when calling process_resource and process_record. */

  LCFGTagList * current_tags = NULL; /* Created when required */
  if ( ancestor_tags != NULL )
    current_tags = lcfgtaglist_clone(ancestor_tags);

  char * tagname = get_lcfgtagname(reader);
  if ( tagname != NULL ) {

    if ( current_tags == NULL )
      current_tags = lcfgtaglist_new();

    char * tagmsg = NULL;
    if ( lcfgtaglist_mutate_append( current_tags, tagname, &tagmsg )
         == LCFG_CHANGE_ERROR ) {
      status = lcfgxml_error( msg, "Failed to append to list of tags: %s", tagmsg );
    }
    free(tagmsg);

    if ( status == LCFG_STATUS_ERROR ) goto cleanup;
  }

  resource = lcfgresource_new();

  /* add base context and derivation rather than set so that we take a copy */

  if ( base_context != NULL ) {
    if ( !lcfgresource_add_context( resource, base_context ) ) {
      status = LCFG_STATUS_ERROR;
      *msg = lcfgresource_build_message( resource, compname,
                "Failed to set base context '%s'", base_context );
      goto cleanup;
    }
  }

  if ( base_derivation != NULL ) {
    if ( !lcfgresource_add_derivation_string( resource, base_derivation ) ) {
      status = LCFG_STATUS_ERROR;
      *msg = lcfgresource_build_message( resource, compname,
                "Failed to set base derivation '%s'", base_derivation );
      goto cleanup;
    }
  }

  char * resname = NULL;
  char * name_msg = NULL;
  bool bad_name = false;

  /* Any failure to set the name is deferred until AFTER the
     attributes have been gathered, this means there is a better
     chance of giving a useful diagnostic message (which hopefully
     includes derivation information). */

  if ( templates != NULL ) {
    resname = lcfgresource_build_name( templates, current_tags,
                                       (char *) resnodename,
                                       &name_msg );

    if ( resname == NULL )
      bad_name = true;

  } else {
    resname = (char *) xmlStrdup(resnodename);
  }

  if ( !bad_name ) {
    if ( !lcfgresource_set_name( resource, resname ) )
      bad_name = true;
  }

  /* Gather attributes before handling any bad name so that info such
     as the derivation is available for the error message. */

  LCFGStatus gather_rc =
    lcfgxml_gather_resource_attributes( reader, resource, compname, msg );

  if ( gather_rc == LCFG_STATUS_ERROR )
    status = LCFG_STATUS_ERROR;

  if (bad_name) {
    if ( name_msg != NULL ) {
      *msg = lcfgresource_build_message( resource, compname,
                                            "Invalid name '%s': %s",
                                            resname, name_msg );
    } else {
      *msg = lcfgresource_build_message( resource, compname,
                                            "Invalid name '%s'",
                                            resname );
    }

    free(resname);

    status = LCFG_STATUS_ERROR;
  }

  free(name_msg);

  if ( status == LCFG_STATUS_ERROR ) goto cleanup;

  bool done  = false;

  int read_status = xmlTextReaderRead(reader);
  while ( !done && read_status == 1 ) {

    int nodetype  = xmlTextReaderNodeType(reader);
    int nodedepth = xmlTextReaderDepth(reader);
    int linenum   = xmlTextReaderGetParserLineNumber(reader);

    if ( nodedepth == topdepth + 1 ) {

      if ( nodetype == XML_READER_TYPE_TEXT ||
           nodetype == XML_READER_TYPE_CDATA ||
           nodetype == XML_READER_TYPE_SIGNIFICANT_WHITESPACE ) {

        xmlChar * nodevalue = xmlTextReaderValue(reader);

        /* This is a little complicated as boolean values come in a
           variety of supported flavours so might need to be
           canonicalised before setting as the resource value */

        if ( lcfgresource_is_boolean(resource) &&
             !lcfgresource_valid_boolean( (char *) nodevalue ) ) {

          char * canon_value =
            lcfgresource_canon_boolean( (char *) nodevalue );

          if ( canon_value == NULL ||
               !lcfgresource_set_value( resource, canon_value ) ) {
            status = LCFG_STATUS_ERROR;

            *msg = lcfgresource_build_message( resource, compname,
                          "Invalid value '%s'", (char *) nodevalue );

            free(canon_value);
          }

        } else if ( !lcfgresource_is_list(resource) ) {

          /* Only setting value for resources which are not lists. Tag
             lists get the values modified when a record is processed
             and a tag name is returned. */

          if ( lcfgresource_set_value( resource, (char *) nodevalue ) ) {
            nodevalue = NULL; /* Resource now "owns" value string */
          } else {
            status = LCFG_STATUS_ERROR;

            *msg = lcfgresource_build_message( resource, compname,
                       "Invalid value '%s'", (char *) nodevalue );
          }

        }

        xmlFree(nodevalue);
        nodevalue = NULL;

      } else if ( nodetype == XML_READER_TYPE_ELEMENT ) {

        LCFGStatus process_rc = LCFG_STATUS_OK;

        char * child_tagname = NULL;

        xmlChar * nodename  = xmlTextReaderName(reader);
        if ( lcfgutils_string_endswith( (char *) nodename, "_RECORD" ) ) {

          process_rc = lcfgxml_process_record( reader, lcfgcomp,
                                               resource->template,
                                               &child_tagname, current_tags,
                                               resource->context,
                                               base_derivation, ctxlist,
                                               msg );

        } else {
          process_rc = lcfgxml_process_resource( reader, lcfgcomp,
                                                 resource->template,
                                                 &child_tagname, current_tags,
                                                 resource->context,
                                                 base_derivation, ctxlist, 
                                                 msg );
        }

        if ( process_rc == LCFG_STATUS_ERROR ) {
          status = LCFG_STATUS_ERROR;
        } else {

          if ( child_tagname != NULL ) {
            if ( child_tags == NULL )
              child_tags = lcfgtaglist_new();

            char * tagmsg = NULL;
            if ( lcfgtaglist_mutate_append( child_tags, child_tagname, &tagmsg )
                 == LCFG_CHANGE_ERROR ) {

              status = lcfgxml_error( msg, "Failed to append to list of tags: %s", tagmsg );

            }
            free(tagmsg);

            free(child_tagname);
            child_tagname = NULL;
          }

        }
        xmlFree(nodename);
        nodename = NULL;

      } else {

        xmlChar * nodename  = xmlTextReaderName(reader);
        status = lcfgxml_error( msg, "Unexpected element '%s' of type %d at line '%d' whilst processing record.", nodename, nodetype, linenum);
        xmlFree(nodename);
        nodename = NULL;

      }

    }  else {
      xmlChar * nodename  = xmlTextReaderName(reader);

      if ( nodedepth == topdepth && nodetype == XML_READER_TYPE_END_ELEMENT ) {

        if ( xmlStrcmp( nodename, resnodename ) == 0 ) {
          done = true; /* Successfully finished this block */
        } else {
          status = lcfgxml_error( msg, "Unexpected end element '%s' at line '%d' whilst processing package.", nodename, linenum );
        }

      } else {
        status = lcfgxml_error( msg, "Unexpected element '%s' of type %d at line '%d' whilst processing package.", nodename, nodetype, linenum );

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

  xmlFree(resnodename);
  resnodename = NULL;

  /* If processing was successful then assemble the list of child tags
     into a single string and set it as the parent resource value. */

  if ( status != LCFG_STATUS_ERROR ) {

    if ( !lcfgtaglist_is_empty(child_tags) ) {
      size_t buf_len = 0;
      char * taglist = NULL;
      ssize_t rc = lcfgtaglist_to_string( child_tags, 0,
                                          &taglist, &buf_len );

      if ( rc < 0 || !lcfgresource_set_value( resource, taglist ) ) {
        *msg = lcfgresource_build_message( resource, compname,
                  "Failed to set taglist '%s'", taglist );

        status = LCFG_STATUS_ERROR;

	free(taglist);
      }

    }

    /* Evaluate the priority */

    if ( status != LCFG_STATUS_ERROR ) {
      char * eval_msg = NULL;
      if ( !lcfgresource_eval_priority( resource, ctxlist, &eval_msg ) ) {
        status = LCFG_STATUS_ERROR;

        *msg = lcfgresource_build_message( resource, compname,
                                           "Failed to evaluate context: ",
                                           eval_msg );
      }
      free(eval_msg);
    }

    if ( status != LCFG_STATUS_ERROR && lcfgresource_is_active(resource) ) {

      /* Stash the resource into the component if it is active (priority
         is zero or greater) */

      char * merge_msg = NULL;
      LCFGChange rc = lcfgcomponent_merge_resource( lcfgcomp, resource,
                                                    &merge_msg );

      if ( rc == LCFG_CHANGE_ERROR ) {
        status = LCFG_STATUS_ERROR;

        *msg = lcfgresource_build_message( resource, compname,
                                           "Failed to merge resource: %s",
                                           merge_msg);

      } else if ( rc != LCFG_CHANGE_NONE ) {
        /* new resource will not be stashed if previously seen
           something with higher priority */

        *thistag = tagname;
        tagname = NULL; /* Will be freed by the caller */
      }

      free(merge_msg);
    }
  }

 cleanup:

  free(tagname);

  lcfgtaglist_relinquish(current_tags);
  lcfgtaglist_relinquish(child_tags);

  if ( status == LCFG_STATUS_ERROR && *msg == NULL ) {
    *msg = lcfgresource_build_message( resource, compname,
			"Something bad happened whilst processing resource");
  }

  lcfgresource_relinquish(resource);

  return status;
}

/* eof */
