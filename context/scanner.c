#include <stdbool.h>
#include <stdio.h>

#include "context.h"

typedef void * lcfgctx_yyscan_t;

lcfgctx_yyscan_t lcfgctx_scanner_init(void);

void lcfgctx_scanner_destroy(lcfgctx_yyscan_t ctxscanner);

int lcfgctx_yyparse (lcfgctx_yyscan_t ctxscanner, int * priority );

void * lcfgctx_yy_scan_string (const char * str, lcfgctx_yyscan_t ctxscanner );


bool lcfgctxlist_eval_expression( const LCFGContextList * ctxlist,
                                  const char * expr,
                                  int * result,
                                  char ** errmsg ) {

  *errmsg = NULL;
  *result = 0;

  lcfgctx_yyscan_t ctxscanner = lcfgctx_scanner_init();

  lcfgctx_yy_scan_string( expr, ctxscanner );

  int rc = lcfgctx_yyparse ( ctxscanner, result );
  printf( "parse: %s, priority: %d\n",
	  (ok == 0 ? "success" : "fail"), *result );

  lcfgctx_scanner_destroy(ctxscanner);

  return ( rc == 0 ? true : false );
}
