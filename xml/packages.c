#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/xmlreader.h>

#include "xml.h"

LCFGStatus lcfgxml_process_package( xmlTextReaderPtr reader,
                                              LCFGPackage ** result,
                                              const char * base_context,
                                              const char * base_derivation,
                                              char ** errmsg ) {

  *errmsg = NULL;
  *result = NULL;

  if ( !lcfgxml_correct_location(reader, LCFGXML_PACKAGES_CHILD_NODE ) ) {
    lcfgxml_set_error_message( errmsg, "Not an LCFG package node." );
    return LCFG_STATUS_ERROR;
  }

  if (xmlTextReaderIsEmptyElement(reader))
    return LCFG_STATUS_OK; /* Nothing to do */

  int topdepth = xmlTextReaderDepth(reader);
  LCFGStatus status = LCFG_STATUS_OK;

  LCFGPackage * pkg = lcfgpackage_new();

  /* The linenum and cur_element variables are declared here as the
     jump to 'cleanup' could otherwise leave them undefined. */

  int linenum = 0;
  xmlChar * cur_element = NULL;

  /* add base context and derivation rather than set so that we take a copy */

  if ( base_context != NULL ) {
    if ( !lcfgpackage_add_context( pkg, base_context ) ) {
      status = LCFG_STATUS_ERROR;
      *errmsg = lcfgpackage_build_message( pkg,
                "Failed to set base context '%s'", base_context );
      goto cleanup;
    }
  }

  if ( base_derivation != NULL ) {
    if ( !lcfgpackage_add_derivation( pkg, base_derivation ) ) {
      status = LCFG_STATUS_ERROR;
      *errmsg = lcfgpackage_build_message( pkg,
                "Failed to set base derivation '%s'", base_derivation );
      goto cleanup;
    }
  }

  /* Handle the context and derivation which are stored as attributes */

  if ( xmlTextReaderHasAttributes(reader) ) {

    /* Optional derivation string for package */

    xmlChar * derivation =
                 xmlTextReaderGetAttribute( reader, BAD_CAST "cfg:derivation");

    if ( derivation != NULL ) {
      if ( lcfgpackage_has_derivation(pkg) ) {

        if ( !lcfgpackage_add_derivation( pkg, (char *) derivation ) ) {
          status = LCFG_STATUS_ERROR;
          *errmsg = lcfgpackage_build_message( pkg,
                    "Invalid derivation '%s'", (char *) derivation );
        }

        /* always free as add_derivation makes a copy of the value */
        xmlFree(derivation);

      } else {

        if ( !lcfgpackage_set_derivation( pkg, (char *) derivation ) ) {
          status = LCFG_STATUS_ERROR;
          *errmsg = lcfgpackage_build_message( pkg,
                    "Invalid derivation '%s'", (char *) derivation );
          xmlFree(derivation);
        }

      }
    }

    if ( status != LCFG_STATUS_OK )
      goto cleanup;

    /* Optional context expression for the package */

    xmlChar * context =
                 xmlTextReaderGetAttribute( reader, BAD_CAST "cfg:context" );

    if ( context != NULL ) {
      if ( lcfgpackage_has_context(pkg) ) {

        if ( !lcfgpackage_add_context( pkg, (char *) context ) ) {
          status = LCFG_STATUS_ERROR;
          *errmsg = lcfgpackage_build_message( pkg,
                    "Invalid context '%s'", (char *) context );
        }

        /* always free as add_context makes a copy of the value */
        xmlFree(context);

      } else {

        if ( !lcfgpackage_set_context( pkg, (char *) context ) ) {
          status = LCFG_STATUS_ERROR;
          *errmsg = lcfgpackage_build_message( pkg,
                    "Invalid context '%s'", (char *) context );
          xmlFree(context);
        }

      }
    }

    if ( status != LCFG_STATUS_OK )
      goto cleanup;

  }

  bool done  = false;

  int read_status = xmlTextReaderRead(reader);

  while ( !done && read_status == 1 ) {

    xmlChar * nodename  = xmlTextReaderName(reader);
    int nodetype        = xmlTextReaderNodeType(reader);
    int nodedepth       = xmlTextReaderDepth(reader);
    linenum             = xmlTextReaderGetParserLineNumber(reader);

    if ( nodetype == XML_READER_TYPE_ELEMENT ) {

      xmlFree(cur_element);

      cur_element = nodename;

      /* examine the next node to see if it is a text node */
      read_status = xmlTextReaderRead(reader);

      if ( read_status != 1 ) {
        status = LCFG_STATUS_ERROR;

        lcfgxml_set_error_message( errmsg,
                                   "Malformed LCFG package node at line %d",
                                   linenum);

      } else if ( !xmlTextReaderIsEmptyElement(reader) &&
                  xmlTextReaderHasValue(reader) ) {

        xmlChar * nodevalue = xmlTextReaderValue(reader);

        /* Name (and optional architecture) */

        if ( xmlStrcmp(nodename, BAD_CAST "name" ) == 0 ) {
          char * archname = (char *) nodevalue;

          /* Check for the '/' (forward-slash) separator */
          char * slash = strrchr(archname,'/');
          if ( slash == NULL ) {

            if ( !lcfgpackage_set_name( pkg, archname ) ) {
              status = LCFG_STATUS_ERROR;
              *errmsg = lcfgpackage_build_message( pkg,
                        "Invalid name '%s'", archname );

              xmlFree(nodevalue);
              nodevalue = NULL;
            }

          } else {
            /* Handle secondary architecture */

            *slash = '\0';
            slash += 1; /* move on past the separator */
            size_t name_len = strlen(slash);
            if ( name_len > 0 ) {
              char * name = strndup(slash,name_len);

              if ( !lcfgpackage_set_name( pkg, name ) ) {
                status = LCFG_STATUS_ERROR;
                *errmsg = lcfgpackage_build_message( pkg,
                          "Invalid name '%s'", name );
                free(name);
              }

            } else {
              status = LCFG_STATUS_ERROR;
              *errmsg = lcfgpackage_build_message( pkg,
                        "Missing name" );
            }

            if ( status == LCFG_STATUS_OK ) {

              char * arch = strdup(archname); /* everything before slash */
              if ( !lcfgpackage_set_arch( pkg, arch ) ) {
                status = LCFG_STATUS_ERROR;
                *errmsg = lcfgpackage_build_message( pkg,
                          "Invalid architecture '%s'", arch );
                free(arch);
              }

            }

            xmlFree(nodevalue);
            nodevalue = NULL;
          }

        /* Version */

        } else if ( xmlStrcmp(nodename, BAD_CAST "v" ) == 0 ) {

          if ( !lcfgpackage_set_version( pkg, (char *) nodevalue ) ) {
            status = LCFG_STATUS_ERROR;
            *errmsg = lcfgpackage_build_message( pkg,
                     "Invalid version '%s'", (char *) nodevalue );
            xmlFree(nodevalue);
            nodevalue = NULL;
          }

        /* Release (and optional architecture) */

        } else if ( xmlStrcmp(nodename, BAD_CAST "r" ) == 0 ) {

          /* Sometimes the arch is encoded in the release field,
             if it is included there will be a '/' (forward slash)
             separator. We cannot rely on the release field to
             never use this character for its own purposes so we
             look for the final occurrence and split the string at
             that point. */

          char * relarch = (char *) nodevalue;
          char * slash = strrchr(relarch,'/');
          if ( slash != NULL ) {
            *slash = '\0';
            slash += 1; /* move on past the separator */
            size_t arch_len = strlen(slash);
            if ( arch_len > 0 ) {
              char * arch = strndup(slash,arch_len);

              if ( !lcfgpackage_set_arch( pkg, arch ) ) {
                status = LCFG_STATUS_ERROR;
                *errmsg = lcfgpackage_build_message( pkg,
                        "Invalid architecture '%s'", arch );
                free(arch);
              }

            }
          }

          if ( status == LCFG_STATUS_OK ) {

            if ( !lcfgpackage_set_release( pkg, relarch ) ) {
              status = LCFG_STATUS_ERROR;
              *errmsg = lcfgpackage_build_message( pkg,
                        "Invalid release '%s'", relarch );
            }

          }

          if ( status != LCFG_STATUS_OK ) {
            xmlFree(nodevalue);
            nodevalue = NULL;
          }

        /* Flags */

        } else if ( xmlStrcmp(nodename, BAD_CAST "options" ) == 0 ) {

          if ( !lcfgpackage_set_flags( pkg, (char *) nodevalue ) ) {
            status = LCFG_STATUS_ERROR;
            *errmsg = lcfgpackage_build_message( pkg,
                      "Invalid flags '%s'", (char *) nodevalue );
            xmlFree(nodevalue);
            nodevalue = NULL;
          }

        /* Anything else is an error */

        } else {
          status = LCFG_STATUS_ERROR;

          lcfgxml_set_error_message( errmsg, "Unexpected node '%s' at line %d whilst processing package.", nodename, linenum );

          xmlFree(nodevalue);
          nodevalue = NULL;
        }

      }

    } else {

      if ( nodetype == XML_READER_TYPE_END_ELEMENT ) {

        if ( nodedepth == topdepth &&
             xmlStrcmp(nodename, BAD_CAST LCFGXML_PACKAGES_CHILD_NODE ) == 0 ) {
          done = true;
        } else if ( cur_element == NULL ||
                    xmlStrcmp(nodename, cur_element) != 0 ) {
          status = LCFG_STATUS_ERROR;

          lcfgxml_set_error_message( errmsg, "Unexpected end element '%s' at line %d whilst processing package.", nodename, linenum );
        }

      } else if ( nodetype != XML_READER_TYPE_WHITESPACE &&
                  nodetype != XML_READER_TYPE_SIGNIFICANT_WHITESPACE ) {
        status = LCFG_STATUS_ERROR;

        lcfgxml_set_error_message( errmsg, "Unexpected element '%s' of type %d at line %d whilst processing package.", nodename, nodetype, linenum);

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

  xmlFree(cur_element);

  if ( status == LCFG_STATUS_OK ) {
    *result = pkg;
  } else {

    if ( *errmsg != NULL )
      lcfgxml_set_error_message( errmsg, "Something bad happened whilst processing package at line %d.", linenum );

    lcfgpackage_destroy(pkg);
    *result = NULL;
  }

  return status;
}

LCFGStatus lcfgxml_process_packages( xmlTextReaderPtr reader,
                                               LCFGPackageList ** active,
                                               LCFGPackageList ** inactive,
                                               const char * base_context,
                                               const char * base_derivation,
                                               const LCFGContextList * ctxlist,
                                               char ** errmsg ) {
  *errmsg   = NULL;
  *active   = NULL;
  *inactive = NULL;

  if ( !lcfgxml_correct_location(reader, LCFGXML_PACKAGES_PARENT_NODE) ) {
    if ( !lcfgxml_moveto_node( reader, LCFGXML_PACKAGES_PARENT_NODE ) ) {
      lcfgxml_set_error_message( errmsg, "Failed to find top-level packages element." );
      return LCFG_STATUS_ERROR;
    }
  }

  if (xmlTextReaderIsEmptyElement(reader)) {
    /* nothing to do */
    return LCFG_STATUS_OK;
  }

  *active   = lcfgpkglist_new();
  *inactive = lcfgpkglist_new();

  unsigned int active_merge_rules =
    LCFG_PKGS_OPT_SQUASH_IDENTICAL | LCFG_PKGS_OPT_USE_PRIORITY;

  lcfgpkglist_set_merge_rules( *active, active_merge_rules );

  unsigned int inactive_merge_rules =
    LCFG_PKGS_OPT_SQUASH_IDENTICAL | LCFG_PKGS_OPT_KEEP_ALL;

  lcfgpkglist_set_merge_rules( *inactive, inactive_merge_rules );

  /* Need to store the depth of the packages element. */

  int topdepth = xmlTextReaderDepth(reader);

  bool done  = false;
  LCFGStatus status = LCFG_STATUS_OK;

  int read_status = xmlTextReaderRead(reader);
  int linenum = 0;

  while ( !done && read_status == 1 ) {

    xmlChar * nodename  = xmlTextReaderName(reader);
    int nodetype        = xmlTextReaderNodeType(reader);
    int nodedepth       = xmlTextReaderDepth(reader);
    linenum             = xmlTextReaderGetParserLineNumber(reader);

    if ( nodetype == XML_READER_TYPE_ELEMENT &&
         xmlStrcmp(nodename, BAD_CAST LCFGXML_PACKAGES_CHILD_NODE ) == 0 ) {

      LCFGPackage * pkg = NULL;

      status = lcfgxml_process_package( reader, &pkg, 
                                        base_context, base_derivation,
                                        errmsg );

      if ( status == LCFG_STATUS_OK ) {

        /* Ignore empty package nodes */
        if ( pkg == NULL ) continue;

	char * eval_msg = NULL;
        if ( !lcfgpackage_has_context(pkg) ||
             lcfgpackage_eval_priority( pkg, ctxlist, &eval_msg ) ) {

          LCFGChange rc;
          char * merge_errmsg = NULL;
          if ( lcfgpackage_is_active(pkg) ) {
            rc = lcfgpkglist_merge_package( *active, pkg,
                                            &merge_errmsg );
          } else {
            rc = lcfgpkglist_merge_package( *inactive, pkg,
					    &merge_errmsg );
          }

          /* Either did not need to store or failed in some way */
          if ( rc != LCFG_CHANGE_ADDED ) {
            lcfgpackage_destroy(pkg);
            pkg = NULL;
          }

          if ( rc == LCFG_CHANGE_ERROR ) {
            status = LCFG_STATUS_ERROR;

            lcfgxml_set_error_message( errmsg,
                                       "Failed to store package '%s': %s",
                                       lcfgpackage_get_name(pkg),
                                       merge_errmsg );
          }

	  free(merge_errmsg);

        } else { /* context eval failure */
          status = LCFG_STATUS_ERROR;

          lcfgpackage_build_message( pkg,
				     "Failed to evaluate context: ",
				     eval_msg );
        }
	free(eval_msg);
      }

      if ( status != LCFG_STATUS_OK ) {
        lcfgpackage_destroy(pkg);
        pkg = NULL;
      }

    } else if ( nodetype == XML_READER_TYPE_END_ELEMENT ) {

      if ( nodedepth == topdepth &&
           xmlStrcmp( nodename, BAD_CAST LCFGXML_PACKAGES_PARENT_NODE ) == 0 ) {
        done = true;
      } else {
        status = LCFG_STATUS_ERROR;

        lcfgxml_set_error_message( errmsg, "Unexpected end element '%s' at line %d whilst processing packages.", nodename, linenum );

      }

    } else if ( nodetype != XML_READER_TYPE_WHITESPACE &&
                nodetype != XML_READER_TYPE_SIGNIFICANT_WHITESPACE ) {
      status = LCFG_STATUS_ERROR;

      lcfgxml_set_error_message( errmsg, "Unexpected element '%s' of type %d at line %d whilst processing packages.", nodename, nodetype, linenum );

    }

    xmlFree(nodename);
    nodename = NULL;

    /* Quit if the processing status is no longer OK */
    if ( status != LCFG_STATUS_OK )
      done = true;

    if ( !done )
      read_status = xmlTextReaderRead(reader);

  }

  if ( status != LCFG_STATUS_OK ) {

    if ( *errmsg == NULL )
      lcfgxml_set_error_message( errmsg, "Something bad happened whilst processing packages at line %d.", linenum );

    lcfgpkglist_destroy(*active);
    *active = NULL;

    lcfgpkglist_destroy(*inactive);
    *inactive = NULL;

  }

  return status;
}

/* eof */
