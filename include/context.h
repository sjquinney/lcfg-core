#ifndef LCFG_CONTEXT_H
#define LCFG_CONTEXT_H

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#include "common.h"

#define DEFAULT_CONTEXTDIR "/var/lcfg/conf/profile/context"

struct LCFGContext {
  char * name;
  char * value;
  int priority;
  unsigned int _refcount;
};

typedef struct LCFGContext LCFGContext;

#define lcfgcontext_inc_ref(ctx) (((ctx)->_refcount)++)
#define lcfgcontext_dec_ref(ctx) (((ctx)->_refcount)--)

LCFGContext * lcfgcontext_new(void);

void lcfgcontext_destroy( LCFGContext * ctx );

bool lcfgcontext_has_name( const LCFGContext * ctx )
  __attribute__((nonnull (1)));

bool lcfgcontext_valid_name( const char * name );

char * lcfgcontext_get_name( const LCFGContext * ctx )
  __attribute__((nonnull (1)));

bool lcfgcontext_set_name(LCFGContext * ctx, char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgcontext_has_value(  const LCFGContext * ctx )
  __attribute__((nonnull (1)));

bool lcfgcontext_valid_value( const char * value );

char * lcfgcontext_get_value( const LCFGContext * ctx )
  __attribute__((nonnull (1)));

bool lcfgcontext_set_value(LCFGContext * ctx, char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgcontext_unset_value( LCFGContext * ctx )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgcontext_is_false( const LCFGContext * ctx );

bool lcfgcontext_is_true(  const LCFGContext * ctx );

int lcfgcontext_get_priority( const LCFGContext * ctx )
  __attribute__((nonnull (1)));

bool lcfgcontext_set_priority( LCFGContext * ctx, int priority )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGStatus lcfgcontext_from_string( const char * string, int priority,
                                    LCFGContext ** ctx,
                                    char ** msg )
  __attribute__((warn_unused_result));

ssize_t lcfgcontext_to_string( const LCFGContext * ctx,
                               unsigned int options,
                               char ** result, size_t * size )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgcontext_print( const LCFGContext * ctx,
                        FILE * out )
  __attribute__((nonnull (1,2)))  __attribute__((warn_unused_result));

bool lcfgcontext_equals( const LCFGContext * ctx1,
                         const LCFGContext * ctx2 )
  __attribute__((nonnull (1,2)));

bool lcfgcontext_identical( const LCFGContext * ctx1,
                            const LCFGContext * ctx2 )
  __attribute__((nonnull (1,2)));

char * lcfgcontext_profile_path( const LCFGContext * ctx,
				 const char * basedir,
				 const char * suffix )
  __attribute__((nonnull (1)));

bool lcfgcontext_valid_expression( const char * expr );
char * lcfgcontext_bracketify_expression( const char * expr );
char * lcfgcontext_combine_expressions( const char * expr1,
                                        const char * expr2 );

struct LCFGContextNode {
  LCFGContext * context;
  struct LCFGContextNode * next;
};

typedef struct LCFGContextNode LCFGContextNode;

LCFGContextNode * lcfgctxnode_new(LCFGContext * ctx);

void lcfgctxnode_destroy(LCFGContextNode * ctxnode);

struct LCFGContextList {
  LCFGContextNode * head;
  LCFGContextNode * tail;
  unsigned int size;
};

typedef struct LCFGContextList LCFGContextList;

LCFGContextList * lcfgctxlist_new(void);

LCFGContextList * lcfgctxlist_clone(const LCFGContextList * ctxlist)
  __attribute__((nonnull (1)));

void lcfgctxlist_destroy(LCFGContextList * ctxlist);

#define lcfgctxlist_head(ctxlist) ((ctxlist)->head)
#define lcfgctxlist_tail(ctxlist) ((ctxlist)->tail)
#define lcfgctxlist_size(ctxlist) ((ctxlist)->size)

#define lcfgctxlist_is_empty(ctxlist) ((ctxlist)->size == 0)

#define lcfgctxlist_next(ctxnode) ((ctxnode)->next)
#define lcfgctxlist_context(ctxnode) ((ctxnode)->context)

LCFGChange lcfgctxlist_insert_after( LCFGContextList * ctxlist,
                                     LCFGContextNode * ctxnode,
                                     LCFGContext     * ctx )
  __attribute__((nonnull (1,3))) __attribute__((warn_unused_result));

LCFGChange lcfgctxlist_remove_after( LCFGContextList * ctxlist,
                                     LCFGContextNode * ctxnode,
                                     LCFGContext    ** ctx )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

#define lcfgctxlist_append(ctxlist, ctx) ( lcfgctxlist_insert_after( ctxlist, lcfgctxlist_tail(ctxlist), ctx ) )

LCFGChange lcfgctxlist_update( LCFGContextList * ctxlist,
                               LCFGContext     * new_ctx )
  __attribute__((nonnull (1,2))) __attribute__((warn_unused_result));

LCFGContextNode * lcfgctxlist_find_node( const LCFGContextList * ctxlist,
                                         const char * name )
  __attribute__((nonnull (2)));

LCFGContext * lcfgctxlist_find_context( const LCFGContextList * ctxlist,
                                        const char * name )
  __attribute__((nonnull (2)));

bool lcfgctxlist_contains( const LCFGContextList * ctxlist,
			   const char * name )
  __attribute__((nonnull (1,2)));

bool lcfgctxlist_print( const LCFGContextList * ctxlist,
                        FILE * out )
  __attribute__((nonnull (1,2))) __attribute__((warn_unused_result));

LCFGStatus lcfgctxlist_from_file( const char * filename,
                                  LCFGContextList ** result,
				  time_t * modtime,
				  unsigned int options,
                                  char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgctxlist_to_file( LCFGContextList * ctxlist,
                                const char * filename,
                                time_t mtime,
                                char ** errmsg )
  __attribute__((nonnull (1,2))) __attribute__((warn_unused_result));

void lcfgctxlist_sort_by_priority( LCFGContextList * ctxlist )
  __attribute__((nonnull (1)));

int lcfgctxlist_max_priority( const LCFGContextList * ctxlist )
  __attribute__((nonnull (1)));

bool lcfgctxlist_diff( const LCFGContextList * ctxlist1,
                       const LCFGContextList * ctxlist2,
		       const char * ctx_profile_dir,
                       time_t prevtime )
  __attribute__((nonnull (1,2)));

bool lcfgctxlist_eval_expression( const LCFGContextList * ctxlist,
                                  const char * expr,
                                  int * result,
                                  char ** errmsg )
  __attribute__((nonnull (2)));

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
                                       char ** errmsg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcontext_load_active( const char * contextdir,
				    LCFGContextList ** ctxlist,
				    time_t * modtime,
                                    char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgcontext_pending_to_active( const char * contextdir,
                                          LCFGContextList ** newactive,
                                          char ** msg )
  __attribute__((warn_unused_result));

#endif /*  LCFG_CONTEXT_H */

/* eof */
