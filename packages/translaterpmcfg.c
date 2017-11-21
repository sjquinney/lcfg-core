/** -*- c -*-
 *
 * Program to convert prefixed arch style rpmspecs to postfixed arch style
 * (Required for 64 bit platform)       
 *
 */

#define _WITH_GETLINE /* for BSD */

#include <stdlib.h>
#include <ctype.h>

#include "utils.h"
#include "packages.h"

int main(void) {

  FILE * in_fh  = stdin;
  FILE * out_fh = stdout;

  bool ok = true;

  size_t line_len = 128;
  char * line = malloc( line_len * sizeof(char) );
  if ( line == NULL ) {
    perror( "Failed to allocate memory whilst processing package list" );
    exit(EXIT_FAILURE);
  }

  unsigned int linenum = 0;
  while( ok && getline( &line, &line_len, in_fh ) != -1 ) {
    linenum++;

    /* If line is a comment or is only whitespace just write it out */

    bool isempty = true;
    char * p;
    for ( p = line; isempty && *p != '\0'; p++ )
      if ( !isspace(*p) ) isempty = false;

    if ( isempty || *line == '#' ) {
      fputs( line, out_fh );
      continue;
    }

    LCFGPackage * pkg = NULL;
    char * msg = NULL;
    LCFGStatus status = lcfgpackage_from_spec( line, &pkg, &msg );
    if ( status == LCFG_STATUS_ERROR ) {
      fprintf( stderr, "Failed to parse LCFG package specification '%s' at line %u: %s\n",
	       line, linenum, msg );
      ok = false;
    } else {
      ok = lcfgpackage_print( pkg, NULL,
			      LCFG_PKG_STYLE_SPEC, LCFG_OPT_NEWLINE,
			      out_fh );
    }

    free(msg);
    lcfgpackage_relinquish(pkg);
  }

  free(line);

  return ( ok ? EXIT_SUCCESS : EXIT_FAILURE );
}
