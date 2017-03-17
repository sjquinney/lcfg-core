#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <unistd.h>

#include "context.h"
#include "utils.h"

/* Query the contents of the pending file */

static bool setctx_eval( char * contextdir, const char * expr ) {

  LCFGContextList * pending = NULL;
  time_t modtime = 0;
  char * load_msg = NULL;
  LCFGStatus rc = lcfgcontext_load_pending( contextdir,
					    &pending, &modtime, &load_msg );

  bool ok = ( rc == LCFG_STATUS_OK );
  if (ok) {
    int result;

    ok = lcfgctxlist_eval_expression( pending, expr, &result );
    if (ok) {
      printf("Ctx: '%s', Result: %d\n", expr, result );
    } else {
      fprintf( stderr, "Failed to evaluate context expression\n" );
    }

  } else {
    fprintf( stderr, "Failed to read context file: %s\n", load_msg );
  }

  free(load_msg);
  lcfgctxlist_destroy(pending);

  return ok;
}

static bool setctx_show(const char * contextdir) {

  LCFGContextList * pending = NULL;
  time_t modtime = 0;
  char * load_msg = NULL;
  LCFGStatus rc = lcfgcontext_load_pending( contextdir,
					    &pending, &modtime, &load_msg );

  bool ok = ( rc == LCFG_STATUS_OK );
  if (ok) {
    ok = lcfgctxlist_print( pending, stdout );
  } else {
    fprintf( stderr, "Failed to read context file: %s\n", load_msg );
  }

  free(load_msg);
  lcfgctxlist_destroy(pending);

  return ok;
}

/* Update the contents of the pending file */

static bool setctx_update( const char * contextdir,
			   int count, char * contexts[] ) {

  char * msg = NULL;
  LCFGChange changed = lcfgcontext_update_pending( contextdir,
                                                   count, contexts,
                                                   &msg );

  switch (changed)
    {
    case LCFG_CHANGE_ERROR:
      fprintf( stderr, "Failed to update contexts: %s\n", msg );
      break;
    case LCFG_CHANGE_NONE:
      printf("No changes to contexts\n");
      break;
    default:
      printf("Contexts changed\n");
      break;
  }

  free(msg);

  return ( changed != LCFG_CHANGE_ERROR );
}

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
