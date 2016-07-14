#define _GNU_SOURCE /* for vasprintf */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include <libxml/xmlreader.h>

#include "xml.h"

void lcfgxml_set_error_message( char **errmsg, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  /* The BSD version apparently sets ptr to NULL on fail.  GNU loses. */
  if (vasprintf(errmsg, fmt, ap) < 0)
    *errmsg = NULL;
  va_end(ap);
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
