/**
 * @file xml/common.c
 * @brief Functions for processing LCFG XML profiles
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date: 2017-04-27 11:58:12 +0100 (Thu, 27 Apr 2017) $
 * $Revision: 32561 $
 */

#define _GNU_SOURCE /* for vasprintf */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include <libxml/xmlreader.h>

#include "xml.h"

/**
 * @brief Generate an XML error message
 *
 * This can be used to generate a standard error message in the XML
 * processor. As well as creating the message it returns the @c
 * LCFG_STATUS_ERROR integer value so that error handling is
 * simplified.
 *
 * @param[out] msg Reference to pointer to new message string
 * @param[in] fmt Format string for the message
 *
 * @return Integer error status value
 *
 */

LCFGStatus lcfgxml_error( char ** msg, const char *fmt, ...) {
  free(*msg);
  *msg = NULL;

  va_list ap;

  va_start(ap, fmt);
  /* The BSD version apparently sets ptr to NULL on fail.  GNU loses. */
  if (vasprintf( msg, fmt, ap) < 0)
    *msg = NULL;
  va_end(ap);

  return LCFG_STATUS_ERROR;
}

/**
 * @brief Move the XML reader to the next tag
 *
 * This can be used to move the reader to the next node which has a
 * type of @c XML_READER_TYPE_ELEMENT or @c
 * XML_READER_TYPE_END_ELEMENT. If no node of the required type is
 * found the function will return a false value.
 *
 * @param[in] reader Pointer to XML reader
 *
 * @return Boolean indicating success
 *
 */

bool lcfgxml_moveto_next_tag( xmlTextReaderPtr reader ) {
  assert( reader != NULL );

  bool done = false;
  int read_status = xmlTextReaderRead(reader);
  while ( !done && read_status == 1 ) {
    
    int nodetype = xmlTextReaderNodeType(reader);

    if ( nodetype == XML_READER_TYPE_ELEMENT ||
         nodetype == XML_READER_TYPE_END_ELEMENT ) {
      done = true;
    }

    if ( !done )
      read_status = xmlTextReaderRead(reader);

  }

  return done;
}

/**
 * @brief Move the XML reader to the next node with the required name
 *
 * This can be used to move the XML reader onto the next node with the
 * required name. If no node is found with the required name this
 * function will return false.
 *
 * @param[in] reader Pointer to XML reader
 * @param[in] target_nodename The name of the required node
 *
 * @return Boolean indicating success
 *
 */

bool lcfgxml_moveto_node( xmlTextReaderPtr reader,
                         const char * target_nodename ) {
  assert( reader != NULL );
  assert( target_nodename != NULL );

  xmlChar * nodename = NULL;

  bool done = false;
  int read_status = 1;
  while ( !done && read_status == 1 ) {
    read_status = xmlTextReaderRead(reader);
    if ( read_status == 1 ) {
      nodename = xmlTextReaderName(reader);

      if (xmlStrcmp(nodename, BAD_CAST target_nodename) == 0 )
        done = true;

      xmlFree(nodename);
      nodename = NULL;
    }
  }

  return done;
}

/**
 * @brief Check if the current node has the required name
 *
 * This can be used to test if the name of the current XML node
 * matches with that specified.
 *
 * @param[in] reader Pointer to XML reader
 * @param[in] expected_nodename The name of the required node
 *
 * @return Boolean indicating success
 *
 */

bool lcfgxml_correct_location( xmlTextReaderPtr reader,
                               const char * expected_nodename ) {
  assert( reader != NULL );
  assert( expected_nodename != NULL );

  xmlChar * nodename = xmlTextReaderName(reader);

  bool correct = ( xmlStrcmp(nodename,BAD_CAST expected_nodename) == 0 );

  xmlFree(nodename);
  nodename = NULL;

  return correct;
}

/* eof */
