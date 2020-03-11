/**
 * @file xml/packages.c
 * @brief Functions for processing package data in LCFG XML profiles
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date: 2017-04-27 11:58:12 +0100 (Thu, 27 Apr 2017) $
 * $Revision: 32561 $
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libxml/xmlreader.h>

#include "xml.h"

/**
 * @brief Collect package information from attributes
 *
 * Collects the following resource information from the attributes for
 * the current node:
 *
 *   - derivation (@c cfg:derivation)
 *   - context (@c cfg:context)
 *
 * @param[in] reader Pointer to XML reader
 * @param[in] res Pointer to @c LCFGPackage
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

static LCFGStatus lcfgxml_gather_package_attributes( xmlTextReaderPtr reader,
                                                     LCFGPackage * pkg,
                                                     LCFGDerivationMap * drvmap, 
                                                     char ** msg ) {
  assert( reader != NULL );
  assert( pkg != NULL );

  if ( !xmlTextReaderHasAttributes(reader) ) return LCFG_STATUS_OK;

  LCFGStatus status = LCFG_STATUS_OK;

  /* Do the derivation first so that any errors after this point get
     the derivation information attached. */

  xmlChar * derivation =
    xmlTextReaderGetAttribute( reader, BAD_CAST "cfg:derivation" );
  if ( derivation != NULL && xmlStrlen(derivation) > 0 ) {

    char * drvmsg = NULL;
    LCFGDerivationList * drvlist =
      lcfgderivmap_find_or_insert_string( drvmap, (char *) derivation,
                                          &drvmsg );

    bool ok = false;

    if ( drvlist != NULL )
      ok = lcfgpackage_set_derivation( pkg, drvlist );

    if (!ok) {
      status = LCFG_STATUS_ERROR;
      *msg = lcfgpackage_build_message( pkg, "Invalid derivation '%s': %s",
                                        (char *) derivation, drvmsg );
    }

    free(drvmsg);
  }

  xmlFree(derivation);
  derivation = NULL;

  if ( status == LCFG_STATUS_ERROR ) return status;

  /* Context Expression */

  xmlChar * context =
    xmlTextReaderGetAttribute( reader, BAD_CAST "cfg:context" );
  if ( context != NULL && xmlStrlen(context) > 0 ) {

    if ( lcfgpackage_has_context(pkg) ) {

      if ( !lcfgpackage_add_context( pkg, (char *) context ) )
        status = LCFG_STATUS_ERROR;

    } else {

      if ( lcfgpackage_set_context( pkg, (char *) context ) )
        context = NULL; /* Package now "owns" context string */
      else
        status = LCFG_STATUS_ERROR;

    }

    if ( status == LCFG_STATUS_ERROR ) {
      *msg = lcfgpackage_build_message( pkg, "Invalid context '%s'", 
                                        (char *) context );
    }

  }

  xmlFree(context);
  context = NULL;

  return status;
}

/**
 * @brief Process XML for a single package

 * @param[in] reader Pointer to XML reader
 * @param[out] result Reference to pointer to new @c LCFGPackage
 * @param[in] base_context A context which will be applied to the package
 * @param[in] base_derivation A derivation which will be applied to the package
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

static LCFGStatus lcfgxml_process_package( xmlTextReaderPtr reader,
                                           LCFGPackage ** result,
                                           const char * base_context,
                                           const char * base_derivation,
                                           LCFGDerivationMap * drvmap,
                                           char ** msg ) {
  assert( reader != NULL );

  *result = NULL;

  if ( !lcfgxml_correct_location(reader, LCFGXML_PACKAGES_CHILD_NODE ) )
    return lcfgxml_error( msg, "Not an LCFG package node." );

  if (xmlTextReaderIsEmptyElement(reader)) return LCFG_STATUS_OK; /* Nothing to do */

  int topdepth = xmlTextReaderDepth(reader);
  LCFGStatus status = LCFG_STATUS_OK;

  LCFGPackage * pkg = lcfgpackage_new();

  /* The linenum and cur_element variables are declared here as the
     jump to 'cleanup' could otherwise leave them undefined. */

  int linenum = 0;
  xmlChar * cur_element = NULL;

  /* Gather any derivation and context information */

  status = lcfgxml_gather_package_attributes( reader, pkg, drvmap, msg );
  if ( status == LCFG_STATUS_ERROR ) goto cleanup;

  /* add any base context and derivation */

  if ( !isempty(base_context) ) {
    if ( !lcfgpackage_add_context( pkg, base_context ) ) {
      status = LCFG_STATUS_ERROR;
      *msg = lcfgpackage_build_message( pkg,
                "Failed to set base context '%s'", base_context );
      goto cleanup;
    }
  }

  if ( !isempty(base_derivation) ) {
    if ( !lcfgpackage_add_derivation_string( pkg, base_derivation ) ) {
      status = LCFG_STATUS_ERROR;
      *msg = lcfgpackage_build_message( pkg,
                "Failed to set base derivation '%s'", base_derivation );
      goto cleanup;
    }
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
        status = lcfgxml_error( msg, "Malformed LCFG package node at line %d",
                                linenum);

      } else if ( !xmlTextReaderIsEmptyElement(reader) &&
                  xmlTextReaderHasValue(reader) ) {

        xmlChar * nodevalue = xmlTextReaderValue(reader);

        /* Name (and optional architecture) */

        /* Due to a historical horrid hack, the name field may contain
           a "secondary" architecture prefix. In that case the forward
           slash character is used as a separator */

        if ( xmlStrcmp(nodename, BAD_CAST "name" ) == 0 ) {
          char * archname = (char *) nodevalue;

          /* Check for the '/' (forward-slash) separator */
          char * arch_name_sep = strrchr(archname,'/');
          if ( arch_name_sep == NULL ) {

            if ( lcfgpackage_set_name( pkg, archname ) ) {
              nodevalue = NULL; /* Package now "owns" string as the name */
            } else {
              status = LCFG_STATUS_ERROR;
              *msg = lcfgpackage_build_message( pkg,
                        "Invalid name '%s'", archname );
            }

          } else {
            *arch_name_sep = '\0';

            const char * name_start = arch_name_sep + 1;
            if ( !isempty(name_start) ) {

              char * name = strdup(name_start);

              if ( !lcfgpackage_set_name( pkg, name ) ) {
                status = LCFG_STATUS_ERROR;
                *msg = lcfgpackage_build_message( pkg,
                          "Invalid name '%s'", name );
                free(name);
              }

            } else {
              status = LCFG_STATUS_ERROR;
              *msg = lcfgpackage_build_message( pkg, "Missing name" );
            }

            if ( status == LCFG_STATUS_OK ) {
              /* Handle secondary architecture */

              if ( lcfgpackage_set_arch( pkg, archname ) ) {
                nodevalue = NULL; /* Package now "owns" string as the arch */
              } else {
                status = LCFG_STATUS_ERROR;
                *msg = lcfgpackage_build_message( pkg,
                                                  "Invalid architecture '%s'",
                                                  archname );
              }

            }

            xmlFree(nodevalue);
            nodevalue = NULL;
          }

        /* Version */

        } else if ( xmlStrcmp(nodename, BAD_CAST "v" ) == 0 ) {

          if ( !lcfgpackage_set_version( pkg, (char *) nodevalue ) ) {
            status = LCFG_STATUS_ERROR;
            *msg = lcfgpackage_build_message( pkg,
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

          char * rel_arch = (char *) nodevalue;
          char * rel_arch_sep = strrchr(rel_arch,'/');
          if ( rel_arch_sep != NULL ) {
            *rel_arch_sep = '\0';
            const char * arch_start = rel_arch_sep + 1;

            if ( !isempty(arch_start) ) {
              char * arch = strdup(arch_start);

              if ( !lcfgpackage_set_arch( pkg, arch ) ) {
                status = LCFG_STATUS_ERROR;
                *msg = lcfgpackage_build_message( pkg,
                        "Invalid architecture '%s'", arch );
                free(arch);
              }

            }
          }

          if ( status == LCFG_STATUS_OK ) {

            if ( !lcfgpackage_set_release( pkg, rel_arch ) ) {
              status = LCFG_STATUS_ERROR;
              *msg = lcfgpackage_build_message( pkg,
                        "Invalid release '%s'", rel_arch );
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
            *msg = lcfgpackage_build_message( pkg,
                      "Invalid flags '%s'", (char *) nodevalue );
            xmlFree(nodevalue);
            nodevalue = NULL;
          }

        /* Anything else is an error */

        } else {
          status = lcfgxml_error( msg, "Unexpected node '%s' at line %d whilst processing package.", nodename, linenum );

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
          status = lcfgxml_error( msg, "Unexpected end element '%s' at line %d whilst processing package.", nodename, linenum );
        }

      } else if ( nodetype != XML_READER_TYPE_WHITESPACE &&
                  nodetype != XML_READER_TYPE_SIGNIFICANT_WHITESPACE ) {

        status = lcfgxml_error( msg, "Unexpected element '%s' of type %d at line %d whilst processing package.", nodename, nodetype, linenum);

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

  xmlFree(cur_element);

  if ( status == LCFG_STATUS_ERROR ) {

    if ( *msg == NULL )
      lcfgxml_error( msg, "Something bad happened whilst processing package at line %d.", linenum );

    lcfgpackage_relinquish(pkg);
    pkg = NULL;
  }

  *result = pkg;

  return status;
}

/**
 * @brief Process XML for all packages
 *
 * @param[in] reader Pointer to XML reader
 * @param[out] active Reference to pointer to new @c LCFGPackageSet for active packages
 * @param[out] inactive  Reference to pointer to new @c LCFGPackageSet for inactive packages
 * @param[in] base_context A context which will be applied to each package
 * @param[in] base_derivation A derivation which will be applied to each package
 * @param[in] ctxlist An @c LCFGContextList which is used to evaluate priority
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return
 *
 */

LCFGStatus lcfgxml_process_packages( xmlTextReaderPtr reader,
                                     LCFGPackageSet ** active,
                                     LCFGPackageSet ** inactive,
                                     const char * base_context,
                                     const char * base_derivation,
                                     const LCFGContextList * ctxlist,
                                     char ** msg ) {
  assert( reader != NULL );

  *active   = NULL;
  *inactive = NULL;

  if ( !lcfgxml_correct_location(reader, LCFGXML_PACKAGES_PARENT_NODE) ) {
    if ( !lcfgxml_moveto_node( reader, LCFGXML_PACKAGES_PARENT_NODE ) ) {
      return lcfgxml_error( msg, "Failed to find top-level packages element." );
    }
  }

  if (xmlTextReaderIsEmptyElement(reader)) return LCFG_STATUS_OK; /* nothing to do */

  /* Declare variables here which are used in the 'cleanup' section */

  *active   = lcfgpkgset_new();
  *inactive = lcfgpkgset_new();

  /* Many package derivations are HUGE and they are shared between
     many packages so we use a map so that they are only parsed once. */

  LCFGDerivationMap * drvmap = lcfgderivmap_new();

  LCFGStatus status = LCFG_STATUS_OK;
  int linenum = 0;

  /* Any conflicts for "active" packages are resolved according to priority */

  if ( !lcfgpkgset_set_merge_rules( *active, ACTIVE_PACKAGE_RULES ) ) {
    status = lcfgxml_error( msg,
                   "Failed to set merge rules for active packages list" );
    goto cleanup;
  }

  /* All other "inactive" packages are stored separately */

  if ( !lcfgpkgset_set_merge_rules( *inactive, INACTIVE_PACKAGE_RULES ) ) {
    status = lcfgxml_error( msg,
                   "Failed to set merge rules for inactive packages list" );
    goto cleanup;
  }

  /* Need to store the depth of the packages element. */

  int topdepth = xmlTextReaderDepth(reader);

  bool done  = false;

  int read_status = xmlTextReaderRead(reader);

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
                                        drvmap,
                                        msg );

      if ( status == LCFG_STATUS_OK && pkg != NULL ) {

	char * eval_msg = NULL;
        if ( !lcfgpackage_has_context(pkg) ||
             lcfgpackage_eval_priority( pkg, ctxlist, &eval_msg ) != LCFG_CHANGE_ERROR ) {

          LCFGChange rc;
          char * merge_msg = NULL;
          if ( lcfgpackage_is_active(pkg) ) {
            rc = lcfgpkgset_merge_package( *active, pkg,
                                            &merge_msg );
          } else {
            rc = lcfgpkgset_merge_package( *inactive, pkg,
					    &merge_msg );
          }

          if ( rc == LCFG_CHANGE_ERROR ) {
            status = lcfgxml_error( msg,
                                    "Failed to store package '%s': %s",
                                    lcfgpackage_get_name(pkg),
                                    merge_msg );
          }

	  free(merge_msg);

        } else { /* context eval failure */
          status = LCFG_STATUS_ERROR;

          lcfgpackage_build_message( pkg,
				     "Failed to evaluate context: %s",
				     eval_msg );
        }
	free(eval_msg);
      }

      lcfgpackage_relinquish(pkg);

    } else if ( nodetype == XML_READER_TYPE_END_ELEMENT ) {

      if ( nodedepth == topdepth &&
           xmlStrcmp( nodename, BAD_CAST LCFGXML_PACKAGES_PARENT_NODE ) == 0 ) {
        done = true;
      } else {

        status = lcfgxml_error( msg, "Unexpected end element '%s' at line %d whilst processing packages.", nodename, linenum );

      }

    } else if ( nodetype != XML_READER_TYPE_WHITESPACE &&
                nodetype != XML_READER_TYPE_SIGNIFICANT_WHITESPACE ) {

      status = lcfgxml_error( msg, "Unexpected element '%s' of type %d at line %d whilst processing packages.", nodename, nodetype, linenum );

    }

    xmlFree(nodename);
    nodename = NULL;

    /* Quit if the processing status is no longer OK */
    if ( status == LCFG_STATUS_ERROR )
      done = true;

    if ( !done )
      read_status = xmlTextReaderRead(reader);

  }

 cleanup:

  lcfgderivmap_relinquish(drvmap);

  if ( status == LCFG_STATUS_ERROR ) {

    if ( *msg == NULL )
      lcfgxml_error( msg, "Something bad happened whilst processing packages at line %d.", linenum );

    lcfgpkgset_relinquish(*active);
    *active = NULL;

    lcfgpkgset_relinquish(*inactive);
    *inactive = NULL;

  }

  return status;
}

/* eof */
