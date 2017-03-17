#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <libxml/xmlreader.h>

#include "xml.h"
#include "utils.h"

static char * get_lcfgtagname( xmlTextReaderPtr reader ) {

  char * tagname = NULL;

  if ( xmlTextReaderHasAttributes(reader) ) {

    xmlChar * attrvalue = xmlTextReaderGetAttribute(reader,BAD_CAST "cfg:name");
    if ( attrvalue != NULL ) {

      tagname = (char *) attrvalue;
      size_t length = strlen(tagname);

      /* A tag name which really starts with a digit will have had an
         _ (underscore) prefix applied. In that case it must be removed. */

      if ( length >= 2 &&
           tagname[0] == '_' && isdigit(tagname[1]) ) {

        /* one character shorter but add one for null byte */
        tagname = memmove( tagname, tagname + 1, length );
      }

    }

  }

  return tagname;
}

LCFGStatus lcfgxml_gather_attributes( xmlTextReaderPtr reader,
                                                LCFGResource * res,
                                                const char * compname,
                                                char ** errmsg ) {

  LCFGStatus status = LCFG_STATUS_OK;

  if ( xmlTextReaderHasAttributes(reader) ) {

    /* Do the derivation first so that any errors after this point get
       the derivation information attached. */

    xmlChar * derivation =
      xmlTextReaderGetAttribute( reader, BAD_CAST "cfg:derivation" );
    if ( derivation != NULL ) {
      bool needs_free = true;

      if ( xmlStrlen(derivation) > 0 ) {

        if ( lcfgresource_has_derivation(res) ) {

          if ( !lcfgresource_add_derivation( res, (char *) derivation ) ) {
            *errmsg = lcfgresource_build_message( res, compname,
                      "Invalid derivation '%s'", (char *) derivation );
            status = LCFG_STATUS_ERROR;
          }

        } else {
          if ( lcfgresource_set_derivation( res, (char *) derivation ) ) {
            needs_free = false;
          } else {
            *errmsg = lcfgresource_build_message( res, compname,
                      "Invalid derivation to '%s'", (char *) derivation );
            status = LCFG_STATUS_ERROR;
          }

        }

      }

      if (needs_free) {
        xmlFree(derivation);
        derivation = NULL;
      }

    }

    if ( status != LCFG_STATUS_OK )
      return status;

    /* Context Expression */

    xmlChar * context =
      xmlTextReaderGetAttribute( reader, BAD_CAST "cfg:context" );
    if ( context != NULL ) {
      bool needs_free = true;

      if ( xmlStrlen(context) > 0 ) {

        if ( lcfgresource_has_context(res) ) {

          if ( !lcfgresource_add_context( res, (char *) context ) ) {
            *errmsg = lcfgresource_build_message( res, compname,
                          "Invalid context '%s'", (char *) context );
            status = LCFG_STATUS_ERROR;
          }

        } else {
          if ( lcfgresource_set_context( res, (char *) context ) ) {
            needs_free = false;
          } else {
            *errmsg = lcfgresource_build_message( res, compname,
                          "Invalid context '%s'", (char *) context );
            status = LCFG_STATUS_ERROR;
          }
        }

      }

      if (needs_free) {
        xmlFree(context);
        context = NULL;
      }
    }

    if ( status != LCFG_STATUS_OK )
      return status;

    /* Type */

    xmlChar * type =
      xmlTextReaderGetAttribute( reader, BAD_CAST "cfg:type" );
    if ( type != NULL ) {

      if ( xmlStrlen(type) > 0 ) {

        char * type_errmsg = NULL;
        if ( !lcfgresource_set_type_as_string( res, (char *) type,
                                               &type_errmsg ) ) {
          *errmsg = lcfgresource_build_message( res, compname,
                                                "Invalid type '%s': %s",
                                                (char *) type,
                                                type_errmsg );
          status = LCFG_STATUS_ERROR;
        }

	free(type_errmsg);

      }

      xmlFree(type);
      type = NULL;

    }

    if ( status != LCFG_STATUS_OK )
      return status;

    /* Template */

    xmlChar * template =
      xmlTextReaderGetAttribute( reader, BAD_CAST "cfg:template" );
    if ( template != NULL ) {

      if ( !lcfgresource_is_list(res) ) {

        if ( !lcfgresource_set_type( res, LCFG_RESOURCE_TYPE_LIST ) ) {
          *errmsg = lcfgresource_build_message( res, compname,
                                                "Failed to set type to 'list'" );
          status = LCFG_STATUS_ERROR;
        }

      }

      if ( xmlStrlen(template) > 0 ) {

        char * tmpl_errmsg = NULL;
        if ( !lcfgresource_set_template_as_string( res, (char *) template,
                                                   &tmpl_errmsg ) ) {
          status = LCFG_STATUS_ERROR;

          *errmsg = lcfgresource_build_message( res, compname,
                                                "Invalid template '%s': %s",
                                                (char *) template,
                                                tmpl_errmsg );
        }

	free(tmpl_errmsg);

      }

      xmlFree(template);
      template = NULL;

    }

  }

  return status;
}

LCFGStatus lcfgxml_process_record( xmlTextReaderPtr reader,
                                             LCFGComponent * lcfgcomp,
                                             const LCFGTemplate * templates,
                                             char ** thistag,
                                             const LCFGTagList * ancestor_tags,
                                             const char * base_context,
                                             const char * base_derivation,
                                             const LCFGContextList * ctxlist,
                                             char ** errmsg ) {

  *errmsg  = NULL;
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

    if ( current_tags == NULL )
      current_tags = lcfgtaglist_new();

    if ( lcfgtaglist_append( current_tags, tagname )
         != LCFG_CHANGE_ADDED ) {
      status = LCFG_STATUS_ERROR;
      lcfgxml_set_error_message( errmsg, "Failed to append tag '%s' to list of current tags", tagname );
      goto cleanup;
    }

    *thistag = strdup(tagname);
  } else {

    status = LCFG_STATUS_ERROR;
    lcfgxml_set_error_message( errmsg, "Missing cfg:name attribute for '%s' record node", record_name );
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
                                           ctxlist, errmsg );

	free(child_tagname);
	child_tagname = NULL;

      } else if ( nodetype != XML_READER_TYPE_WHITESPACE &&
                  nodetype != XML_READER_TYPE_SIGNIFICANT_WHITESPACE ) {

        xmlChar * nodename  = xmlTextReaderName(reader);
        lcfgxml_set_error_message( errmsg, "Unexpected element '%s' of type %d at line '%d' whilst processing record.", nodename, nodetype, linenum );
        xmlFree(nodename);
        nodename = NULL;

        status = LCFG_STATUS_ERROR;
      }

    } else {
      xmlChar * nodename  = xmlTextReaderName(reader);

      if ( nodedepth == topdepth && nodetype == XML_READER_TYPE_END_ELEMENT ) {

        if ( xmlStrcmp( nodename, record_name ) == 0 ) {
          done = true; /* Successfully finished this block */
        } else {
          lcfgxml_set_error_message( errmsg, "Unexpected end element '%s' at line '%d' whilst processing package.", nodename, linenum );
          status = LCFG_STATUS_ERROR;
        }

      } else {
        lcfgxml_set_error_message( errmsg, "Unexpected element '%s' of type %d at line '%d' whilst processing package.", nodename, nodetype, linenum);
        status = LCFG_STATUS_ERROR;
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

  xmlFree(record_name);
  record_name = NULL;

  lcfgtaglist_destroy(current_tags);

  if ( status != LCFG_STATUS_OK ) {
    if ( *errmsg == NULL )
      lcfgxml_set_error_message( errmsg, "Something bad happened whilst processing record." );
  }

  return status;
}

LCFGStatus lcfgxml_process_resource( xmlTextReaderPtr reader,
                                               LCFGComponent * lcfgcomp,
                                               const LCFGTemplate * templates,
                                               char ** thistag,
                                               const LCFGTagList * ancestor_tags,
                                               const char * base_context,
                                               const char * base_derivation,
                                               const LCFGContextList * ctxlist,
                                               char ** errmsg ) {

  *thistag = NULL;
  *errmsg = NULL;

  if (xmlTextReaderIsEmptyElement(reader))
    return LCFG_STATUS_OK;

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

    if ( lcfgtaglist_append( current_tags, tagname )
         != LCFG_CHANGE_ADDED ) {
      status = LCFG_STATUS_ERROR;
      lcfgxml_set_error_message( errmsg, "Failed to append tag '%s' to list of current tags", tagname );
      goto cleanup;
    }

  }

  resource = lcfgresource_new();

  /* add base context and derivation rather than set so that we take a copy */

  if ( base_context != NULL ) {
    if ( !lcfgresource_add_context( resource, base_context ) ) {
      status = LCFG_STATUS_ERROR;
      *errmsg = lcfgresource_build_message( resource, compname,
                "Failed to set base context '%s'", base_context );
      goto cleanup;
    }
  }

  if ( base_derivation != NULL ) {
    if ( !lcfgresource_add_derivation( resource, base_derivation ) ) {
      status = LCFG_STATUS_ERROR;
      *errmsg = lcfgresource_build_message( resource, compname,
                "Failed to set base derivation '%s'", base_derivation );
      goto cleanup;
    }
  }

  char * resname;
  char * name_errmsg = NULL;
  bool bad_name = false;

  /* Any failure to set the name is deferred until AFTER the
     attributes have been gathered, this means there is a better
     chance of giving a useful diagnostic message (which hopefully
     includes derivation information). */

  if ( templates != NULL ) {
    resname = lcfgresource_build_name( templates, current_tags,
                                       (char *) resnodename,
                                       &name_errmsg );

    if ( resname == NULL )
      bad_name = true;

  } else {
    resname = (char *) xmlStrdup(resnodename);
  }

  if ( !bad_name ) {
    if ( !lcfgresource_set_name( resource, resname ) )
      bad_name = true;
  }

  if ( lcfgxml_gather_attributes( reader, resource, compname, errmsg )
       != LCFG_STATUS_OK ) {
    status = LCFG_STATUS_ERROR;
  }

  if (bad_name) {
    if ( name_errmsg != NULL ) {
      *errmsg = lcfgresource_build_message( resource, compname,
                                            "Invalid name '%s': %s",
                                            resname, name_errmsg );
    } else {
      *errmsg = lcfgresource_build_message( resource, compname,
                                            "Invalid name '%s'",
                                            resname );
    }

    free(resname);

    status = LCFG_STATUS_ERROR;
  }

  free(name_errmsg);

  if ( status != LCFG_STATUS_OK )
    goto cleanup;

  bool done  = false;

  int read_status = xmlTextReaderRead(reader);
  while ( !done && read_status == 1 ) {

    int nodetype  = xmlTextReaderNodeType(reader);
    int nodedepth = xmlTextReaderDepth(reader);
    int linenum   = xmlTextReaderGetParserLineNumber(reader);

    if ( nodedepth == topdepth + 1 ) {

      if ( nodetype == XML_READER_TYPE_TEXT ||
           nodetype == XML_READER_TYPE_CDATA ) {

        xmlChar * nodevalue = xmlTextReaderValue(reader);
        bool needs_free = true;

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

            *errmsg = lcfgresource_build_message( resource, compname,
                          "Invalid value '%s'", (char *) nodevalue );

            free(canon_value);
          }

        } else {

          if ( lcfgresource_set_value( resource, (char *) nodevalue ) ) {
            needs_free = false;
          } else {
            status = LCFG_STATUS_ERROR;

            *errmsg = lcfgresource_build_message( resource, compname,
                       "Invalid value '%s'", (char *) nodevalue );
          }

        }

        if (needs_free) {
          xmlFree(nodevalue);
          nodevalue = NULL;
        }

      } else if ( nodetype == XML_READER_TYPE_ELEMENT ) {

        int process_ok = LCFG_STATUS_OK;

        char * child_tagname = NULL;

        xmlChar * nodename  = xmlTextReaderName(reader);
        if ( lcfgutils_endswith( (char *) nodename, "_RECORD" ) ) {

          process_ok = lcfgxml_process_record( reader, lcfgcomp,
                                               resource->template,
                                               &child_tagname, current_tags,
                                               resource->context,
                                               base_derivation, ctxlist,
                                               errmsg );

        } else {
          process_ok = lcfgxml_process_resource( reader, lcfgcomp,
                                                 resource->template,
                                                 &child_tagname, current_tags,
                                                 resource->context,
                                                 base_derivation, ctxlist, 
                                                 errmsg );
        }

        if ( process_ok != LCFG_STATUS_OK ) {
          status = LCFG_STATUS_ERROR;
        } else {

          if ( child_tagname != NULL ) {
            if ( child_tags == NULL )
              child_tags = lcfgtaglist_new();

            if ( lcfgtaglist_append( child_tags, child_tagname )
                 != LCFG_CHANGE_ADDED ) {

              status = LCFG_STATUS_ERROR;
              lcfgxml_set_error_message( errmsg, "Failed to append tag '%s' to list of child tags", child_tagname );

            }

          }

        }
        xmlFree(nodename);
        nodename = NULL;

      } else if ( nodetype != XML_READER_TYPE_WHITESPACE &&
                  nodetype != XML_READER_TYPE_SIGNIFICANT_WHITESPACE ) {

        status = LCFG_STATUS_ERROR;

        xmlChar * nodename  = xmlTextReaderName(reader);
        lcfgxml_set_error_message( errmsg, "Unexpected element '%s' of type %d at line '%d' whilst processing record.", nodename, nodetype, linenum);
        xmlFree(nodename);
        nodename = NULL;

      }

    }  else {
      xmlChar * nodename  = xmlTextReaderName(reader);

      if ( nodedepth == topdepth && nodetype == XML_READER_TYPE_END_ELEMENT ) {

        if ( xmlStrcmp( nodename, resnodename ) == 0 ) {
          done = true; /* Successfully finished this block */
        } else {
          status = LCFG_STATUS_ERROR;

          lcfgxml_set_error_message( errmsg, "Unexpected end element '%s' at line '%d' whilst processing package.", nodename, linenum );

        }

      } else {
        status = LCFG_STATUS_ERROR;

        lcfgxml_set_error_message( errmsg, "Unexpected element '%s' of type %d at line '%d' whilst processing package.", nodename, nodetype, linenum );

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

  xmlFree(resnodename);
  resnodename = NULL;

  /* If processing was successful then assemble the list of child tags
     into a single string and set it as the parent resource value. */

  if ( status == LCFG_STATUS_OK ) {

    if ( child_tags != NULL && lcfgtaglist_size(child_tags) > 0 ) {
      size_t buf_len = 0;
      char * taglist = NULL;
      ssize_t rc = lcfgtaglist_to_string( child_tags, 0,
                                          &taglist, &buf_len );

      if ( rc < 0 || !lcfgresource_set_value( resource, taglist ) ) {
        *errmsg = lcfgresource_build_message( resource, compname,
                  "Failed to set taglist '%s'", taglist );

        status = LCFG_STATUS_ERROR;

	free(taglist);
      }

    }

    /* Evaluate the priority */

    if ( status == LCFG_STATUS_OK ) {
      if ( !lcfgresource_eval_priority( resource, ctxlist ) ) {
        status = LCFG_STATUS_ERROR;

        *errmsg = lcfgresource_build_message( resource, compname,
                                              "Failed to evaluate context" );
      }
    }

    /* Stash the resource into the component if it is active (priority
       is zero or greater) */

    if ( status == LCFG_STATUS_OK ) {
      bool added = false;

      if ( lcfgresource_is_active(resource) ) {
        char * merge_errmsg = NULL;
        LCFGChange rc =
          lcfgcomponent_insert_or_merge_resource( lcfgcomp, resource,
                                                  &merge_errmsg );

        if ( rc == LCFG_CHANGE_ERROR ) {
          status = LCFG_STATUS_ERROR;

          *errmsg = lcfgresource_build_message( resource, compname,
                                                "Failed to merge resource: %s",
                                                merge_errmsg);

        } else {
          /* new resource will not be stashed if previously seen
             something with higher priority */

          added = ( rc != LCFG_CHANGE_NONE );
        }

	free(merge_errmsg);
      }

      if (added) {
        if ( tagname != NULL )
          *thistag = strdup(tagname);
      } else {
        lcfgresource_destroy(resource);
        resource = NULL;
      }

    }
  }

 cleanup:

  lcfgtaglist_destroy(current_tags);
  lcfgtaglist_destroy(child_tags);

  if ( status != LCFG_STATUS_OK ) {

    if ( *errmsg == NULL ) {
      *errmsg = lcfgresource_build_message( resource, compname,
                    "Something bad happened whilst processing resource");
    }

    if ( resource != NULL ) {
      lcfgresource_destroy(resource);
      resource = NULL;
    }

  }

  return status;
}

/* eof */
