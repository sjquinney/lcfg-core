/* Doubly linked-list structure for ordered lists of LCFG "tags". Also
   intended to be reasonably efficient for set-like operations. */

#ifndef LCFG_CORE_TAGS_H
#define LCFG_CORE_TAGS_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"

/* The maximum supported tag depth */
#define LCFG_TAGS_MAX_DEPTH 5

/* Tag */

struct LCFGTag {
  char * name;
  size_t name_len;
  unsigned int _refcount;
};

typedef struct LCFGTag LCFGTag;

LCFGTag * lcfgtag_new();
void lcfgtag_destroy( LCFGTag * tag );
void lcfgtag_acquire( LCFGTag * tag );
void lcfgtag_relinquish( LCFGTag * tag );

bool lcfgtag_is_valid( const LCFGTag * tag );
bool lcfgresource_valid_tag( const char * value );

bool lcfgtag_set_name( LCFGTag * tag, char * new_name )
  __attribute__((warn_unused_result));

char * lcfgtag_get_name( const LCFGTag * tag );

size_t lcfgtag_get_length( const LCFGTag * tag );

LCFGStatus lcfgtag_from_string( const char * input,
                                LCFGTag ** result,
                                char ** msg )
  __attribute__((warn_unused_result));

int lcfgtag_compare( const LCFGTag * tag, const LCFGTag * other );
bool lcfgtag_matches( const LCFGTag * tag, const char * name );

/* List of Tags */

#define lcfgtaglist_head(taglist) ((taglist)->head)
#define lcfgtaglist_tail(taglist) ((taglist)->tail)
#define lcfgtaglist_size(taglist) ((taglist)->size)

#define lcfgtaglist_is_empty(taglist) ( taglist == NULL || (taglist)->size == 0)

#define lcfgtaglist_next(tagnode) ((tagnode)->next)
#define lcfgtaglist_prev(tagnode) ((tagnode)->prev)
#define lcfgtaglist_tag(tagnode)  ((tagnode)->tag)

#define lcfgtaglist_append_tag(taglist, tag) ( lcfgtaglist_insert_next( taglist, lcfgtaglist_tail(taglist), tag ) )

struct LCFGTagNode {
  LCFGTag * tag;
  struct LCFGTagNode * prev;
  struct LCFGTagNode * next;
};

typedef struct LCFGTagNode LCFGTagNode;

LCFGTagNode * lcfgtagnode_new(LCFGTag * tag);
void lcfgtagnode_destroy(LCFGTagNode * tagnode);

struct LCFGTagList {
  LCFGTagNode * head;
  LCFGTagNode * tail;
  unsigned int size;
  unsigned int _refcount;
};
typedef struct LCFGTagList LCFGTagList;

LCFGTagList * lcfgtaglist_new(void);
void lcfgtaglist_destroy(LCFGTagList * taglist);
void lcfgtaglist_acquire( LCFGTagList * taglist );
void lcfgtaglist_relinquish( LCFGTagList * taglist );

LCFGTagList * lcfgtaglist_clone(const LCFGTagList * taglist);

LCFGChange lcfgtaglist_insert_next( LCFGTagList * taglist,
                                    LCFGTagNode * tagnode,
                                    LCFGTag * tag )
  __attribute__((warn_unused_result));

LCFGChange lcfgtaglist_remove_tag( LCFGTagList * taglist,
                                   LCFGTagNode * tagnode,
                                   LCFGTag ** tag )
  __attribute__((warn_unused_result));

LCFGChange lcfgtaglist_append_string( LCFGTagList * taglist,
                                      const char * tagname,
                                      char ** msg )
  __attribute__((warn_unused_result));

LCFGTagNode * lcfgtaglist_find_node( const LCFGTagList * taglist,
                                     const char * name );

LCFGTag * lcfgtaglist_find_tag( const LCFGTagList * taglist,
                                const char * name );

bool lcfgtaglist_contains( const LCFGTagList * taglist,
                           const char * name );

LCFGStatus lcfgtaglist_from_string( const char * input,
                                    LCFGTagList ** result,
                                    char ** msg )
  __attribute__((warn_unused_result));

ssize_t lcfgtaglist_to_string( const LCFGTagList * taglist,
                               unsigned int options,
                               char ** result, size_t * size )
  __attribute__((warn_unused_result));

bool lcfgtaglist_print( const LCFGTagList * taglist, FILE * out )
  __attribute__((warn_unused_result));

void lcfgtaglist_sort( LCFGTagList * taglist );

/* Mutators */

LCFGChange lcfgtaglist_mutate_add( LCFGTagList * taglist, const char * name,
                                   char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgtaglist_mutate_extra( LCFGTagList * taglist, const char * name,
                                     char ** msg )
  __attribute__((warn_unused_result));

#endif /* LCFG_CORE_TAGS_H */

/* eof */
