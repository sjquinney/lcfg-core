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

#include <libxml/xmlreader.h>

#include "xml.h"

LCFGStatus lcfgxml_error( char ** msg, const char *fmt, ...) {
  free(*msg);

  va_list ap;

  va_start(ap, fmt);
  /* The BSD version apparently sets ptr to NULL on fail.  GNU loses. */
  if (vasprintf( msg, fmt, ap) < 0)
    *msg = NULL;
  va_end(ap);

  return LCFG_STATUS_ERROR;
}

bool lcfgxml_moveto_next_tag( xmlTextReaderPtr reader ) {

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

bool lcfgxml_moveto_node( xmlTextReaderPtr reader,
                         const char * target_nodename ) {

  xmlChar * nodename;

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

bool lcfgxml_correct_location( xmlTextReaderPtr reader,
                               const char * expected_nodename ) {

  xmlChar * nodename = xmlTextReaderName(reader);

  bool correct = ( xmlStrcmp(nodename,BAD_CAST expected_nodename) == 0 );

  xmlFree(nodename);
  nodename = NULL;

  return correct;
}

/* eof */
