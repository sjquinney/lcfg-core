#ifndef LCFG_COMPONENT_H
#define LCFG_COMPONENT_H

#include <stdbool.h>
#include <stdio.h>

#include "resources.h"

/* Components */

struct LCFGResourceNode {
  LCFGResource * resource;
  struct LCFGResourceNode * next;
};

typedef struct LCFGResourceNode LCFGResourceNode;

LCFGResourceNode * lcfgresourcenode_new(LCFGResource * res);

void lcfgresourcenode_destroy(LCFGResourceNode * resnode);

struct LCFGComponent {
  char * name;
  LCFGResourceNode * head;
  LCFGResourceNode * tail;
  unsigned int size;
  unsigned int _refcount;
};

typedef struct LCFGComponent LCFGComponent;

LCFGComponent * lcfgcomponent_new(void);

void lcfgcomponent_destroy(LCFGComponent * comp);
void lcfgcomponent_acquire(LCFGComponent * comp);
void lcfgcomponent_relinquish(LCFGComponent * comp);

bool lcfgcomponent_has_name(const LCFGComponent * comp);
bool lcfgcomponent_valid_name(const char * name );
char * lcfgcomponent_get_name(const LCFGComponent * comp);
bool lcfgcomponent_set_name( LCFGComponent * comp, char * new_name )
  __attribute__((warn_unused_result));

bool lcfgcomponent_print( const LCFGComponent * comp,
                          LCFGResourceStyle style,
                          LCFGOption options,
                          FILE * out )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomponent_from_statusfile( const char * filename,
                                          LCFGComponent ** result,
                                          const char * compname,
					  LCFGOption options,
                                          char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomponent_to_statusfile( const LCFGComponent * comp,
                                        const char * filename,
                                        char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomponent_to_env( const LCFGComponent * comp,
				 const char * val_pfx, const char * type_pfx,
				 LCFGOption options,
                                 char ** msg )
  __attribute__((warn_unused_result));

char * lcfgcomponent_get_resources_as_string(const LCFGComponent * comp);

LCFGChange lcfgcomponent_insert_next( LCFGComponent    * comp,
                                      LCFGResourceNode * resnode,
                                      LCFGResource     * res )
  __attribute__((warn_unused_result));

LCFGChange lcfgcomponent_remove_next( LCFGComponent    * comp,
                                      LCFGResourceNode * resnode,
                                      LCFGResource    ** res )
  __attribute__((warn_unused_result));

#define lcfgcomponent_head(comp) ((comp)->head)
#define lcfgcomponent_tail(comp) ((comp)->tail)
#define lcfgcomponent_size(comp) ((comp)->size)

#define lcfgcomponent_is_empty(comp) ( comp == NULL || (comp)->size == 0)

#define lcfgcomponent_next(resnode)     ((resnode)->next)
#define lcfgcomponent_resource(resnode) ((resnode)->resource)

#define lcfgcomponent_append(comp, res) ( lcfgcomponent_insert_next( comp, lcfgcomponent_tail(comp), res ) )

LCFGResourceNode * lcfgcomponent_find_node( const LCFGComponent * comp,
                                            const char * name );

LCFGResource * lcfgcomponent_find_resource( const LCFGComponent * comp,
                                            const char * name );

bool lcfgcomponent_has_resource(  const LCFGComponent * comp,
                                  const char * name );

LCFGResource * lcfgcomponent_find_or_create_resource( LCFGComponent * comp,
                                                      const char * name );

LCFGChange lcfgcomponent_insert_or_merge_resource( LCFGComponent * comp,
                                                   LCFGResource  * res,
                                                   char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgcomponent_insert_or_replace_resource( LCFGComponent * comp,
                                                     LCFGResource  * res,
                                                     char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomponent_apply_overrides( LCFGComponent * comp,
                                          const LCFGComponent * overrides,
                                          char ** msg )
  __attribute__((warn_unused_result));

void lcfgcomponent_sort( LCFGComponent * comp );

/* Component list */

struct LCFGComponentNode {
  LCFGComponent * component;
  struct LCFGComponentNode * next;
};

typedef struct LCFGComponentNode LCFGComponentNode;

LCFGComponentNode * lcfgcomponentnode_new(LCFGComponent * comp);

void lcfgcomponentnode_destroy(LCFGComponentNode * compnode);

struct LCFGComponentList {
  LCFGComponentNode * head;
  LCFGComponentNode * tail;
  unsigned int size;
};

typedef struct LCFGComponentList LCFGComponentList;

LCFGComponentList * lcfgcomplist_new(void);

void lcfgcomplist_destroy(LCFGComponentList * complist);

LCFGChange lcfgcomplist_insert_next( LCFGComponentList * complist,
                                     LCFGComponentNode * compnode,
                                     LCFGComponent     * comp )
  __attribute__((warn_unused_result));

LCFGChange lcfgcomplist_remove_next( LCFGComponentList * complist,
                                     LCFGComponentNode * compnode,
                                     LCFGComponent    ** comp )
  __attribute__((warn_unused_result));

LCFGComponentNode * lcfgcomplist_find_node( const LCFGComponentList * complist,
                                            const char * name )
  __attribute__((nonnull (2)));

LCFGComponent * lcfgcomplist_find_component( const LCFGComponentList * complist,
                                             const char * name )
  __attribute__((nonnull (2)));

bool lcfgcomplist_has_component( const LCFGComponentList * complist,
                                 const char * name )
  __attribute__((nonnull (2)));

LCFGComponent * lcfgcomplist_find_or_create_component( 
                                             LCFGComponentList * complist,
                                             const char * name )
  __attribute__((nonnull (1)));

bool lcfgcomplist_print( const LCFGComponentList * complist,
                         LCFGResourceStyle style,
                         LCFGOption options,
                         FILE * out )
  __attribute__((nonnull (1,4))) __attribute__((warn_unused_result));

#define lcfgcomplist_head(complist) ((complist)->head)
#define lcfgcomplist_tail(complist) ((complist)->tail)
#define lcfgcomplist_size(complist) ((complist)->size)

#define lcfgcomplist_is_empty(complist) ( complist == NULL || (complist)->size == 0)

#define lcfgcomplist_next(compnode)     ((compnode)->next)
#define lcfgcomplist_component(compnode) ((compnode)->component)

#define lcfgcomplist_append(complist, comp) ( lcfgcomplist_insert_next( complist, lcfgcomplist_tail(complist), comp ) )

LCFGChange lcfgcomplist_insert_or_replace_component(
                                              LCFGComponentList * complist,
                                              LCFGComponent * new_comp,
                                              char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomplist_transplant_components( LCFGComponentList * list1,
                                               const LCFGComponentList * list2,
                                               char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomplist_apply_overrides( LCFGComponentList * list1,
                                         const LCFGComponentList * list2,
                                         char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomplist_from_status_dir( const char * status_dir,
                                         LCFGComponentList ** complist,
                                         const LCFGTagList * comps_wanted,
                                         char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomplist_to_status_dir( const LCFGComponentList * complist,
                                       const char * status_dir,
                                       char ** msg )
  __attribute__((warn_unused_result));

/* Resource List iterator */

struct LCFGResourceIterator {
  LCFGComponent * component;
  LCFGResourceNode * current;
  bool done;
};
typedef struct LCFGResourceIterator LCFGResourceIterator;

LCFGResourceIterator * lcfgresiter_new( LCFGComponent * component );

void lcfgresiter_destroy( LCFGResourceIterator * iterator );
void lcfgresiter_reset( LCFGResourceIterator * iterator );
bool lcfgresiter_has_next( LCFGResourceIterator * iterator );
LCFGResource * lcfgresiter_next(LCFGResourceIterator * iterator);


#endif /* LCFG_COMPONENT_H */

/* eof */
