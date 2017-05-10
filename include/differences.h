
#ifndef LCFG_CORE_DIFFERENCES_H
#define LCFG_CORE_DIFFERENCES_H

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

void lcfgdiffresource_acquire( LCFGDiffResource * resdiff );
void lcfgdiffresource_relinquish( LCFGDiffResource * resdiff );

bool lcfgdiffresource_has_old( const LCFGDiffResource * resdiff );

LCFGResource * lcfgdiffresource_get_old( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_set_old( LCFGDiffResource * resdiff,
                               LCFGResource * res );

bool lcfgdiffresource_has_new( const LCFGDiffResource * resdiff );

LCFGResource * lcfgdiffresource_get_new( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_set_new( LCFGDiffResource * resdiff,
                               LCFGResource * res );

char * lcfgdiffresource_get_name( const LCFGDiffResource * resdiff );

LCFGChange lcfgdiffresource_get_type( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_is_changed( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_is_nochange( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_is_modified( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_is_added( const LCFGDiffResource * resdiff );

bool lcfgdiffresource_is_removed( const LCFGDiffResource * resdiff );

ssize_t lcfgdiffresource_to_string( const LCFGDiffResource * resdiff,
				    const char * prefix,
				    const char * comments,
				    bool pending,
				    char ** result, size_t * size );

ssize_t lcfgdiffresource_to_hold( const LCFGDiffResource * resdiff,
                                  const char * prefix,
                                  char ** result, size_t * size )
  __attribute__((warn_unused_result));

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

bool lcfgdiffcomponent_has_name(const LCFGDiffComponent * compdiff);

char * lcfgdiffcomponent_get_name(const LCFGDiffComponent * compdiff);

bool lcfgdiffcomponent_set_name( LCFGDiffComponent * compdiff,
                                 char * new_name )
  __attribute__((warn_unused_result));

void lcfgdiffcomponent_set_type( LCFGDiffComponent * compdiff,
                                 LCFGChange change_type );

LCFGChange lcfgdiffcomponent_get_type( const LCFGDiffComponent * compdiff );

bool lcfgdiffcomponent_is_nochange( const LCFGDiffComponent * compdiff );

bool lcfgdiffcomponent_is_changed( const LCFGDiffComponent * compdiff );

bool lcfgdiffcomponent_is_added( const LCFGDiffComponent * compdiff );

bool lcfgdiffcomponent_is_modified( const LCFGDiffComponent * compdiff );

bool lcfgdiffcomponent_is_removed( const LCFGDiffComponent * compdiff );

LCFGChange lcfgdiffcomponent_insert_next( LCFGDiffComponent * compdiff,
                                          LCFGDiffResource  * current,
                                          LCFGDiffResource  * new )
  __attribute__((warn_unused_result));

LCFGChange lcfgdiffcomponent_remove_next( LCFGDiffComponent * compdiff,
                                          LCFGDiffResource  * current,
                                          LCFGDiffResource ** old )
  __attribute__((warn_unused_result));

#define lcfgdiffcomponent_head(compdiff) ((compdiff)->head)
#define lcfgdiffcomponent_tail(compdiff) ((compdiff)->tail)
#define lcfgdiffcomponent_size(compdiff) ((compdiff)->size)

#define lcfgdiffcomponent_is_empty(compdiff) ( compdiff != NULL && (compdiff)->size == 0)

#define lcfgdiffcomponent_next(resdiff)     ((resdiff)->next)

#define lcfgdiffcomponent_append(compdiff, resdiff) ( lcfgdiffcomponent_insert_next( compdiff, lcfgdiffcomponent_tail(compdiff), resdiff ) )

LCFGDiffResource * lcfgdiffcomponent_find_resource(
					  const LCFGDiffComponent * compdiff,
					  const char * want_name );

bool lcfgdiffcomponent_has_resource( const LCFGDiffComponent * compdiff,
				     const char * want_name );

void lcfgdiffcomponent_sort( LCFGDiffComponent * compdiff );

LCFGStatus lcfgdiffcomponent_to_holdfile( const LCFGDiffComponent * compdiff,
                                          FILE * holdfile,
                                          md5_state_t * md5state )
  __attribute__((warn_unused_result));

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
  unsigned int size;
};

typedef struct LCFGDiffProfile LCFGDiffProfile;

LCFGDiffProfile * lcfgdiffprofile_new(void);

void lcfgdiffprofile_destroy(LCFGDiffProfile * profdiff );

LCFGChange lcfgdiffprofile_insert_next( LCFGDiffProfile    * profdiff,
                                        LCFGDiffComponent  * current,
                                        LCFGDiffComponent  * new )
  __attribute__((warn_unused_result));

LCFGChange lcfgdiffprofile_remove_next( LCFGDiffProfile    * profdiff,
                                        LCFGDiffComponent  * current,
                                        LCFGDiffComponent ** old )
  __attribute__((warn_unused_result));

#define lcfgdiffprofile_head(profdiff) ((profdiff)->head)
#define lcfgdiffprofile_tail(profdiff) ((profdiff)->tail)
#define lcfgdiffprofile_size(profdiff) ((profdiff)->size)

#define lcfgdiffprofile_is_empty(profdiff) ( profdiff != NULL && (profdiff)->size == 0)

#define lcfgdiffprofile_next(compdiff)     ((compdiff)->next)

#define lcfgdiffprofile_append(profdiff, compdiff) ( lcfgdiffprofile_insert_next( profdiff, lcfgdiffprofile_tail(profdiff), compdiff ) )

LCFGDiffComponent * lcfgdiffprofile_find_component(
					    const LCFGDiffProfile * profdiff,
					    const char * want_name );

bool lcfgdiffprofile_has_component( const LCFGDiffProfile * profdiff,
				    const char * comp_name );

LCFGStatus lcfgdiffprofile_to_holdfile( const LCFGDiffProfile * profdiff,
                                        const char * holdfile,
                                        char ** signature,
                                        char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgcomponent_diff( const LCFGComponent * comp1,
                               const LCFGComponent * comp2,
                               LCFGDiffComponent ** compdiff )
  __attribute__((warn_unused_result));

LCFGStatus lcfgprofile_diff( const LCFGProfile * profile1,
                             const LCFGProfile * profile2,
                             LCFGDiffProfile ** result,
                             char ** msg )
  __attribute__((warn_unused_result));

#endif /* LCFG_CORE_DIFFERENCES_H */

/* eof */
