#include <stdbool.h>
#include <stdio.h>

#include "context.h"

typedef void * lcfgctx_yyscan_t;

/**
 * @brief Initialise a new context query scanner
 *
 * This is used to initialise the flex/bison based context scanner. It
 * is not normally necessary to call this function directly, see 
 * @c lcfgctxlist_eval_expression()
 *
 * @return New context scanner
 *
 */

lcfgctx_yyscan_t lcfgctx_scanner_init(void);

/**
 * @brief Destroy a context query scanner
 *
 * This is used to destroy a flex/bison based context scanner and free
 * all associated memory. It is not normally necessary to call this
 * function directly, see @c lcfgctxlist_eval_expression()
 *
 * @param[in] ctxscanner Context query scanner
 *
 */

void lcfgctx_scanner_destroy(lcfgctx_yyscan_t ctxscanner);

/**
 * @brief Parse an LCFG context query expression
 *
 * This is used to parse a context query using flex/bison. It
 * is not normally necessary to call this function directly, see
 * @c lcfgctxlist_eval_expression()
 *
 * @param[in] ctxscanner Context query scanner
 * @param[in] ctxlist Pointer to @c LCFGContextList
 * @param[out] priority Integer result of evaluation
 *
 * @return Integer indicating success (zero) or failure (non-zero)
 *
 */

int lcfgctx_yyparse (lcfgctx_yyscan_t ctxscanner,
                     const LCFGContextList * ctxlist,
                     int * priority );

/**
 * @brief Specify the context query string to be scanned
 *
 * This is used to specify the string which will be lexed/parsed. It
 * is not normally necessary to call this function directly, see
 * @c lcfgctxlist_eval_expression()
 *
 * @param[in] str Context query string
 * @param[in] ctxscanner Context query scanner
 *
 */

void * lcfgctx_yy_scan_string (const char * str, lcfgctx_yyscan_t ctxscanner );

/**
 * @brief Evaluate an LCFG context query expression
 *
 * This can be used to evaluate any arbitrary context query string for
 * the specified the specified @c LCFGContextList.
 *
 * The magnitude of the value returned is based on a combination of
 * the priorities associated with the contexts evaluated. The sign of
 * the result returned indicates the truth of the comparison
 * (i.e. positive for true and negative for false). See @c
 * lcfgctxlist_simple_query() for details of how individual query
 * values are calculated. When simple query conditions are combined
 * using AND, OR or XOR the following rules apply:
 *
 *   - AND : If both conditions are true then the greater of the
 *     priorities, otherwise the lesser is priority returned.
 *   - OR : If either condition is true then the greater of the
 *     priorities, otherwise the lesser is priority returned.
 *   - XOR : If only one condition is true then the greater of the
 *     priorities, otherwise the lesser priority is returned.
 *
 * Using NOT simply switches the sign of a condition (e.g. positive to
 * negative or negative to positive).
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList
 * @param[in] expr Context query string
 * @param[out] result Integer result of evaluation
 *
 * @return Boolean indicating success
 *
 */

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
