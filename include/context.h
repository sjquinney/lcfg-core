/**
 * @file context.h
 * @brief LCFG context handling library
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#ifndef LCFG_CORE_CONTEXT_H
#define LCFG_CORE_CONTEXT_H

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#include "common.h"
#include "utils.h"

/**
 * @brief Supported comparison operators for context query expressions
 */

typedef enum {
  LCFG_TEST_ISTRUE,   /**< Value is true */
  LCFG_TEST_ISFALSE,  /**< Value is false */
  LCFG_TEST_ISEQ,     /**< Value is equal to specified string */
  LCFG_TEST_ISNE      /**< Value is not equal to specified string */
} LCFGTest;

/**
 * @brief A structure to represent an LCFG context
 */

struct LCFGContext {
  /*@{*/
  char * name;  /**< The name of the context (required) */
  char * value; /**< The value for the context (optional) */
  int priority; /**< The priority for the context (default is 1) */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGContext LCFGContext;

LCFGContext * lcfgcontext_new(void);

void lcfgcontext_acquire( LCFGContext * ctx );

void lcfgcontext_destroy( LCFGContext * ctx );

void lcfgcontext_relinquish( LCFGContext * ctx );

/* Name */

bool lcfgcontext_has_name( const LCFGContext * ctx );

bool lcfgcontext_valid_name( const char * name );

char * lcfgcontext_get_name( const LCFGContext * ctx );

bool lcfgcontext_set_name(LCFGContext * ctx, char * new_value )
  __attribute__((warn_unused_result));

/* Value */

bool lcfgcontext_has_value( const LCFGContext * ctx );

bool lcfgcontext_valid_value( const char * value );

char * lcfgcontext_get_value( const LCFGContext * ctx );

bool lcfgcontext_set_value(LCFGContext * ctx, char * new_value )
  __attribute__((warn_unused_result));

bool lcfgcontext_unset_value( LCFGContext * ctx )
  __attribute__((warn_unused_result));

bool lcfgcontext_is_false( const LCFGContext * ctx );

bool lcfgcontext_is_true(  const LCFGContext * ctx );

bool lcfgcontext_is_valid( const LCFGContext * ctx );

/* Priority */

int lcfgcontext_get_priority( const LCFGContext * ctx );

bool lcfgcontext_set_priority( LCFGContext * ctx, int priority )
  __attribute__((warn_unused_result));

/* Input/Output */

LCFGStatus lcfgcontext_from_string( const char * string, int priority,
                                    LCFGContext ** ctx,
                                    char ** msg )
  __attribute__((warn_unused_result));

ssize_t lcfgcontext_to_string( const LCFGContext * ctx,
                               LCFGOption options,
                               char ** result, size_t * size )
  __attribute__((warn_unused_result));

bool lcfgcontext_print( const LCFGContext * ctx,
                        FILE * out )
  __attribute__((warn_unused_result));

char * lcfgcontext_profile_path( const LCFGContext * ctx,
				 const char * basedir,
				 const char * suffix );

/* Comparisons */

bool lcfgcontext_same_name( const LCFGContext * ctx1,
                            const LCFGContext * ctx2 );

bool lcfgcontext_same_value( const LCFGContext * ctx1,
                             const LCFGContext * ctx2 );

bool lcfgcontext_equals( const LCFGContext * ctx1,
                         const LCFGContext * ctx2 );

bool lcfgcontext_identical( const LCFGContext * ctx1,
                            const LCFGContext * ctx2 );

bool lcfgcontext_match( const LCFGContext * ctx, const char * want_name );

/* Expressions */

bool lcfgcontext_valid_expression( const char * expr, char ** msg );
char * lcfgcontext_bracketify_expression( const char * expr );
char * lcfgcontext_combine_expressions( const char * expr1,
                                        const char * expr2 );

/* Lists */

/**
 * @brief A structure for storing LCFG contexts as a single-linked list
 */

struct LCFGContextList {
  /*@{*/
  LCFGSListNode * head; /**< The first context in the list */
  LCFGSListNode * tail; /**< The last context in the list */
  unsigned int size;      /**< The length of the list */
  /*@}*/
};

typedef struct LCFGContextList LCFGContextList;

LCFGContextList * lcfgctxlist_new(void);

LCFGContextList * lcfgctxlist_clone(const LCFGContextList * ctxlist);

void lcfgctxlist_destroy(LCFGContextList * ctxlist);

/**
 * @brief Get the number of items in the context list
 *
 * This is a simple macro which can be used to get the length of the
 * single-linked context list.
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList
 *
 * @return Integer length of the context list
 *
 */

#define lcfgctxlist_size(ctxlist) ((ctxlist)->size)

/**
 * @brief Test if the context list is empty
 *
 * This is a simple macro which can be used to test if the
 * single-linked context list contains any items.
 *
 * @param[in] ctxlist Pointer to @c LCFGContextList
 *
 * @return Boolean which indicates whether the list contains any items
 *
 */

#define lcfgctxlist_is_empty(ctxlist) ( ctxlist == NULL || (ctxlist)->size == 0)

LCFGChange lcfgctxlist_update( LCFGContextList * ctxlist,
                               LCFGContext     * new_ctx )
  __attribute__((warn_unused_result));

LCFGSListNode * lcfgctxlist_find_node( const LCFGContextList * ctxlist,
                                       const char * name );

LCFGContext * lcfgctxlist_find_context( const LCFGContextList * ctxlist,
                                        const char * name );

bool lcfgctxlist_contains( const LCFGContextList * ctxlist,
			   const char * name );

bool lcfgctxlist_print( const LCFGContextList * ctxlist,
                        FILE * out )
  __attribute__((warn_unused_result));

LCFGStatus lcfgctxlist_from_file( const char * filename,
                                  LCFGContextList ** result,
				  time_t * modtime,
				  LCFGOption options,
                                  char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgctxlist_to_file( LCFGContextList * ctxlist,
                                const char * filename,
                                time_t mtime,
                                char ** errmsg )
  __attribute__((warn_unused_result));

void lcfgctxlist_sort_by_priority( LCFGContextList * ctxlist );

int lcfgctxlist_max_priority( const LCFGContextList * ctxlist );

bool lcfgctxlist_diff( const LCFGContextList * ctxlist1,
                       const LCFGContextList * ctxlist2,
		       const char * ctx_profile_dir,
                       time_t prevtime );

int lcfgctxlist_simple_query( const LCFGContextList * ctxlist,
                              const char * ctxq_name,
                              const char * ctxq_val,
                              LCFGTest ctxq_cmp );

bool lcfgctxlist_eval_expression( const LCFGContextList * ctxlist,
                                  const char * expr,
                                  int * result, char ** msg );

int lcfgcontext_compare_expressions( const char * ctx1,
                                     const char * ctx2 )
  __attribute__((const));

/* Tools */

bool lcfgcontext_check_cfgdir( const char * contextdir, char ** msg )
  __attribute__((warn_unused_result));

FILE * lcfgcontext_tmpfile(const char * contextdir, char ** tmpfile );

bool lcfgcontext_lock( const char * contextdir,
		       const char * file, int timeout, char ** msg )
  __attribute__((warn_unused_result));

bool lcfgcontext_unlock( const char * contextdir, char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcontext_load_pending( const char * contextdir,
				     LCFGContextList ** ctxlist,
                                     time_t * modtime,
                                     char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgcontext_update_pending( const char * contextdir,
                                       int change_count, char * contexts[],
				       LCFGContextList ** ctxlist,
                                       char ** errmsg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcontext_load_active( const char * contextdir,
				    LCFGContextList ** ctxlist,
				    time_t * modtime,
                                    char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgcontext_pending_to_active( const char * contextdir,
                                          const char * ctx_profile_dir,
                                          LCFGContextList ** newactive,
                                          char ** msg )
  __attribute__((warn_unused_result));

bool setctx_eval( const char * contextdir, const char * expr )
  __attribute__((warn_unused_result));

bool setctx_show(const char * contextdir)
  __attribute__((warn_unused_result));

bool setctx_update( const char * contextdir,
                    int count, char * contexts[] )
  __attribute__((warn_unused_result));

#endif /*  LCFG_CORE_CONTEXT_H */

/* eof */
