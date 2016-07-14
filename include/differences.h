
#ifndef LCFG_DIFFERENCES_H
#define LCFG_DIFFERENCES_H

#include <stdio.h>
#include <stdbool.h>

#include "common.h"
#include "resources.h"
#include "components.h"
#include "profile.h"
#include "utils.h"

/* Differences */

struct LCFGDiffResource {
  LCFGResource * old;
  LCFGResource * new;
  struct LCFGDiffResource * next;
  unsigned int _refcount;
};

typedef struct LCFGDiffResource LCFGDiffResource;

LCFGDiffResource * lcfgdiffresource_new(void);

void lcfgdiffresource_destroy(LCFGDiffResource * resdiff);

#define lcfgdiffresource_inc_ref(resdiff) (((resdiff)->_refcount)++)
#define lcfgdiffresource_dec_ref(resdiff) (((resdiff)->_refcount)--)

bool lcfgdiffresource_has_old( const LCFGDiffResource * resdiff )
  __attribute__((nonnull (1)));

LCFGResource * lcfgdiffresource_get_old( const LCFGDiffResource * resdiff )
  __attribute__((nonnull (1)));

bool lcfgdiffresource_set_old( LCFGDiffResource * resdiff,
                               LCFGResource * res )
  __attribute__((nonnull (1)));

bool lcfgdiffresource_has_new( const LCFGDiffResource * resdiff )
  __attribute__((nonnull (1)));

LCFGResource * lcfgdiffresource_get_new( const LCFGDiffResource * resdiff )
  __attribute__((nonnull (1)));

bool lcfgdiffresource_set_new( LCFGDiffResource * resdiff,
                               LCFGResource * res )
  __attribute__((nonnull (1)));

char * lcfgdiffresource_get_name( const LCFGDiffResource * resdiff )
  __attribute__((nonnull (1)));

LCFGChange lcfgdiffresource_get_type( const LCFGDiffResource * resdiff )
  __attribute__((nonnull (1)));

bool lcfgdiffresource_is_changed( const LCFGDiffResource * resdiff )
  __attribute__((nonnull (1)));

bool lcfgdiffresource_is_nochange( const LCFGDiffResource * resdiff )
  __attribute__((nonnull (1)));

bool lcfgdiffresource_is_modified( const LCFGDiffResource * resdiff )
  __attribute__((nonnull (1)));

bool lcfgdiffresource_is_added( const LCFGDiffResource * resdiff )
  __attribute__((nonnull (1)));

bool lcfgdiffresource_is_removed( const LCFGDiffResource * resdiff )
  __attribute__((nonnull (1)));

char * lcfgdiffresource_to_string( const LCFGDiffResource * resdiff,
                                   const char * prefix,
                                   const char * comments,
                                   bool pending )
  __attribute__((nonnull (1)));

ssize_t lcfgdiffresource_to_hold( const LCFGDiffResource * resdiff,
                                  const char * prefix,
                                  char ** result, size_t * size )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

struct LCFGDiffComponent {
  char * name;
  LCFGDiffResource * head;
  LCFGDiffResource * tail;
  struct LCFGDiffComponent * next;
  unsigned int size;
  LCFGChange change_type;
  unsigned int _refcount;
};

typedef struct LCFGDiffComponent LCFGDiffComponent;

LCFGDiffComponent * lcfgdiffcomponent_new(void);

void lcfgdiffcomponent_destroy(LCFGDiffComponent * compdiff );

#define lcfgdiffcomponent_inc_ref(compdiff) (((compdiff)->_refcount)++)
#define lcfgdiffcomponent_dec_ref(compdiff) (((compdiff)->_refcount)--)

bool lcfgdiffcomponent_valid_name(const char * name);

bool lcfgdiffcomponent_has_name(const LCFGDiffComponent * compdiff)
  __attribute__((nonnull (1)));

char * lcfgdiffcomponent_get_name(const LCFGDiffComponent * compdiff)
  __attribute__((nonnull (1)));

bool lcfgdiffcomponent_set_name( LCFGDiffComponent * compdiff,
                                 char * new_name )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

void lcfgdiffcomponent_set_type( LCFGDiffComponent * compdiff,
                                 LCFGChange change_type )
  __attribute__((nonnull (1)));

LCFGChange lcfgdiffcomponent_get_type( const LCFGDiffComponent * compdiff )
  __attribute__((nonnull (1)));

bool lcfgdiffcomponent_is_nochange( const LCFGDiffComponent * compdiff )
  __attribute__((nonnull (1)));

bool lcfgdiffcomponent_is_changed( const LCFGDiffComponent * compdiff )
  __attribute__((nonnull (1)));

bool lcfgdiffcomponent_is_added( const LCFGDiffComponent * compdiff )
  __attribute__((nonnull (1)));

bool lcfgdiffcomponent_is_modified( const LCFGDiffComponent * compdiff )
  __attribute__((nonnull (1)));

bool lcfgdiffcomponent_is_removed( const LCFGDiffComponent * compdiff )
  __attribute__((nonnull (1)));

LCFGChange lcfgdiffcomponent_insert_next( LCFGDiffComponent * compdiff,
                                          LCFGDiffResource  * current,
                                          LCFGDiffResource  * new )
 __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGChange lcfgdiffcomponent_remove_next( LCFGDiffComponent * compdiff,
                                          LCFGDiffResource  * current,
                                          LCFGDiffResource ** old )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

#define lcfgdiffcomponent_head(compdiff) ((compdiff)->head)
#define lcfgdiffcomponent_tail(compdiff) ((compdiff)->tail)
#define lcfgdiffcomponent_size(compdiff) ((compdiff)->size)

#define lcfgdiffcomponent_is_empty(compdiff) ((compdiff)->size == 0)

#define lcfgdiffcomponent_next(resdiff)     ((resdiff)->next)

#define lcfgdiffcomponent_append(compdiff, resdiff) ( lcfgdiffcomponent_insert_next( compdiff, lcfgdiffcomponent_tail(compdiff), resdiff ) )

LCFGDiffResource * lcfgdiffcomponent_find_resource(
					  const LCFGDiffComponent * compdiff,
					  const char * want_name )
  __attribute__((nonnull (1,2)));

bool lcfgdiffcomponent_has_resource( const LCFGDiffComponent * compdiff,
				     const char * want_name )
  __attribute__((nonnull (1,2)));

void lcfgdiffcomponent_sort( LCFGDiffComponent * compdiff )
  __attribute__((nonnull (1)));

LCFGStatus lcfgdiffcomponent_to_holdfile( const LCFGDiffComponent * compdiff,
                                          FILE * holdfile,
                                          md5_state_t * md5state )
  __attribute__((nonnull (1,2))) __attribute__((warn_unused_result));

LCFGStatus lcfgresource_diff( LCFGResource * old_res,
                              LCFGResource * new_res,
                              LCFGDiffResource ** resdiff )
  __attribute__((warn_unused_result));

LCFGChange lcfgcomponent_quickdiff( const LCFGComponent * comp1,
                                    const LCFGComponent * comp2 )
  __attribute__((warn_unused_result));

LCFGChange lcfgcomplist_quickdiff( const LCFGComponentList * list1,
                                   const LCFGComponentList * list2,
                                   LCFGTagList ** modified,
                                   LCFGTagList ** added,
                                   LCFGTagList ** removed )
  __attribute__((warn_unused_result));

LCFGChange lcfgprofile_quickdiff( const LCFGProfile * profile1,
                                  const LCFGProfile * profile2,
                                  LCFGTagList ** modified,
                                  LCFGTagList ** added,
                                  LCFGTagList ** removed )
  __attribute__((warn_unused_result));

struct LCFGDiffProfile {
  LCFGDiffComponent * head;
  LCFGDiffComponent * tail;
  int size;
};

typedef struct LCFGDiffProfile LCFGDiffProfile;

LCFGDiffProfile * lcfgdiffprofile_new(void);

void lcfgdiffprofile_destroy(LCFGDiffProfile * profdiff );

LCFGChange lcfgdiffprofile_insert_next( LCFGDiffProfile    * profdiff,
                                        LCFGDiffComponent  * current,
                                        LCFGDiffComponent  * new )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGChange lcfgdiffprofile_remove_next( LCFGDiffProfile    * profdiff,
                                        LCFGDiffComponent  * current,
                                        LCFGDiffComponent ** old )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

#define lcfgdiffprofile_head(profdiff) ((profdiff)->head)
#define lcfgdiffprofile_tail(profdiff) ((profdiff)->tail)
#define lcfgdiffprofile_size(profdiff) ((profdiff)->size)

#define lcfgdiffprofile_is_empty(profdiff) ((profdiff)->size == 0)

#define lcfgdiffprofile_next(compdiff)     ((compdiff)->next)

#define lcfgdiffprofile_append(profdiff, compdiff) ( lcfgdiffprofile_insert_next( profdiff, lcfgdiffprofile_tail(profdiff), compdiff ) )

LCFGDiffComponent * lcfgdiffprofile_find_component(
					    const LCFGDiffProfile * profdiff,
					    const char * want_name )
  __attribute__((nonnull (1,2)));

bool lcfgdiffprofile_has_component( const LCFGDiffProfile * profdiff,
				    const char * comp_name )
  __attribute__((nonnull (1,2)));

LCFGStatus lcfgdiffprofile_to_holdfile( const LCFGDiffProfile * profdiff,
                                        const char * holdfile,
                                        char ** signature,
                                        char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGStatus lcfgcomponent_diff( const LCFGComponent * comp1,
                               const LCFGComponent * comp2,
                               LCFGDiffComponent ** compdiff )
  __attribute__((warn_unused_result));

LCFGStatus lcfgprofile_diff( const LCFGProfile * profile1,
                             const LCFGProfile * profile2,
                             LCFGDiffProfile ** result,
                             char ** msg )
  __attribute__((warn_unused_result));

#endif /* LCFG_DIFFERENCES_H */

/* eof */
