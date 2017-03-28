#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <unistd.h>

#include "context.h"
#include "utils.h"

#define DEFAULT_CONTEXTDIR "/var/lcfg/conf/profile/context"

int main(int argc, char* argv[]) {

  int c;

  char * rootdir = NULL;
  char * ctxdir  = NULL;
  char * expr    = NULL;

  while (1) {
    static struct option long_options[] =
      {
        {"ctxdir", required_argument, 0, 'c'},
        {"root", required_argument, 0, 'r'},
        {"eval", required_argument, 0, 'e'},
        {0, 0, 0, 0}
      };

    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long (argc, argv, "c:r:",
                     long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c) {
    case 'c':
      ctxdir = strdup(optarg);
      break;
    case 'r':
      rootdir = strdup(optarg);
      break;
    case 'e':
      expr = strdup(optarg);
      break;
    default:
      abort ();
    }
  }

  if ( ctxdir == NULL )
    ctxdir = strdup(DEFAULT_CONTEXTDIR);

  if ( rootdir != NULL ) {
    char * new_ctxdir = lcfgutils_catfile( rootdir, ctxdir );
    free(ctxdir);
    ctxdir = new_ctxdir;
  }

  bool ok = true;

  if ( expr != NULL ) {
    ok = setctx_eval( ctxdir, expr );
    free(expr);
  } else if (optind >= argc) {
    ok = setctx_show(ctxdir);
  } else {
    int count = argc - optind;
    char * contexts[count];
    int i;
    for (i=0;i<count;i++) {
      contexts[i] = argv[argc - count + i];
    }
    ok = setctx_update( ctxdir, count, contexts );
  }

  free(ctxdir);
  free(rootdir);

  return ( ok ? EXIT_SUCCESS : EXIT_FAILURE );
}

/* eof */
