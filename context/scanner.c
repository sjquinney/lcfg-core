#include <stdbool.h>
#include <stdio.h>

#include "context.h"

typedef void * lcfgctx_yyscan_t;

lcfgctx_yyscan_t lcfgctx_scanner_init(void);

void lcfgctx_scanner_destroy(lcfgctx_yyscan_t ctxscanner);

int lcfgctx_yyparse (lcfgctx_yyscan_t ctxscanner,
                     const LCFGContextList * ctxlist,
                     int * priority );

void * lcfgctx_yy_scan_string (const char * str, lcfgctx_yyscan_t ctxscanner );


bool lcfgctxlist_eval_expression( const LCFGContextList * ctxlist,
                                  const char * expr,
                                  int * result ) {

  *result = 0;

  lcfgctx_yyscan_t ctxscanner = lcfgctx_scanner_init();

  lcfgctx_yy_scan_string( expr, ctxscanner );

  int rc = lcfgctx_yyparse ( ctxscanner, ctxlist, result );

  lcfgctx_scanner_destroy(ctxscanner);

  return ( rc == 0 ? true : false );
}
