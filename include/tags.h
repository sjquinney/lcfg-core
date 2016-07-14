/* Doubly linked-list structure for ordered lists of LCFG "tags". Also
   intended to be reasonably efficient for set-like operations. */

#ifndef LCFG_TAGS_H
#define LCFG_TAGS_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"

/* The maximum supported tag depth */
#define LCFG_TAGS_MAX_DEPTH 5

struct LCFGTag {
  char * name;
  struct LCFGTag * prev;
  struct LCFGTag * next;
  size_t name_len;
};

typedef struct LCFGTag LCFGTag;

LCFGTag * lcfgtag_new(char * name);

struct LCFGTagList {
  LCFGTag * head;
  LCFGTag * tail;
  unsigned int size;
  bool manage;
};
typedef struct LCFGTagList LCFGTagList;

bool lcfgtag_valid_name( const char * value );

LCFGTagList * lcfgtaglist_new(void);
void lcfgtaglist_destroy(LCFGTagList * taglist);


LCFGChange lcfgtaglist_insert_next( LCFGTagList * taglist,
                                    LCFGTag * tag,
                                    char * name )
  __attribute__((nonnull (1,3))) __attribute__((warn_unused_result));

LCFGChange lcfgtaglist_remove( LCFGTagList * taglist,
                               LCFGTag * tag,
                               char ** name )
  __attribute__((nonnull (1,2))) __attribute__((warn_unused_result));

LCFGTag * lcfgtaglist_find_tag( const LCFGTagList * taglist,
                                const char * name )
  __attribute__((nonnull (1,2)));

bool lcfgtaglist_contains( const LCFGTagList * taglist,
                           const char * name )
  __attribute__((nonnull (1,2)));

#define lcfgtaglist_head(taglist) ((taglist)->head)
#define lcfgtaglist_tail(taglist) ((taglist)->tail)
#define lcfgtaglist_size(taglist) ((taglist)->size)

#define lcfgtaglist_is_empty(taglist) ((taglist)->size == 0)

#define lcfgtaglist_next(tagnode) ((tagnode)->next)
#define lcfgtaglist_prev(tagnode) ((tagnode)->prev)
#define lcfgtaglist_name(tagnode) ((tagnode)->name)
#define lcfgtaglist_name_len(tagnode) ((tagnode)->name_len)

#define lcfgtaglist_append(taglist, name) ( lcfgtaglist_insert_next( taglist, lcfgtaglist_tail(taglist), name ) )

LCFGTagList * lcfgtaglist_clone(const LCFGTagList * taglist)
  __attribute__((nonnull (1)));

LCFGStatus lcfgtaglist_from_string( const char * input,
                                    LCFGTagList ** taglist,
                                    char ** msg )
  __attribute__((warn_unused_result));

ssize_t lcfgtaglist_to_string( const LCFGTagList * taglist,
                               unsigned int options,
                               char ** str, size_t * len )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgtaglist_print( const LCFGTagList * taglist, FILE * out )
  __attribute__((nonnull (1,2))) __attribute__((warn_unused_result));

void lcfgtaglist_sort( LCFGTagList * taglist )
  __attribute__((nonnull (1)));

#endif /* LCFG_TAGS_H */

/* eof */
