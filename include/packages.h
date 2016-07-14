
#ifndef LCFG_PACKAGES_H
#define LCFG_PACKAGES_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "common.h"
#include "context.h"

#define LCFG_PACKAGE_NOVALUE ""
#define LCFG_PACKAGE_WILDCARD "*"

struct LCFGPackageSpec {
  char * name;
  char * arch;
  char * version;
  char * release;
  char * flags;
  char * context;
  char * derivation;
  char prefix;
  int priority;
  unsigned int _refcount;
};

typedef struct LCFGPackageSpec LCFGPackageSpec;

LCFGPackageSpec * lcfgpkgspec_new(void);
void lcfgpkgspec_destroy(LCFGPackageSpec * pkgspec);

LCFGPackageSpec * lcfgpkgspec_clone( const LCFGPackageSpec * pkgspec );

#define lcfgpkgspec_inc_ref(pkgspec) (((pkgspec)->_refcount)++)
#define lcfgpkgspec_dec_ref(pkgspec) (((pkgspec)->_refcount)--)

/* Name */

bool lcfgpkgspec_valid_name( const char * name );
bool lcfgpkgspec_has_name( const LCFGPackageSpec * pkgspec )
__attribute__((nonnull (1)));

char * lcfgpkgspec_get_name( const LCFGPackageSpec * pkgspec )
__attribute__((nonnull (1)));

bool lcfgpkgspec_set_name( LCFGPackageSpec * pkgspec, char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Architecture */

bool lcfgpkgspec_valid_arch( const char * arch );
bool lcfgpkgspec_has_arch( const LCFGPackageSpec * pkgspec )
__attribute__((nonnull (1)));

char * lcfgpkgspec_get_arch( const LCFGPackageSpec * pkgspec )
  __attribute__((nonnull (1)));

bool lcfgpkgspec_set_arch( LCFGPackageSpec * pkgspec, char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Version */

bool lcfgpkgspec_valid_version( const char * version );
bool lcfgpkgspec_has_version( const LCFGPackageSpec * pkgspec )
  __attribute__((nonnull (1)));

char * lcfgpkgspec_get_version( const LCFGPackageSpec * pkgspec )
__attribute__((nonnull (1)));

bool lcfgpkgspec_set_version( LCFGPackageSpec * pkgspec, char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Release */

bool lcfgpkgspec_valid_release( const char * release );
bool lcfgpkgspec_has_release( const LCFGPackageSpec * pkgspec )
__attribute__((nonnull (1)));

char * lcfgpkgspec_get_release( const LCFGPackageSpec * pkgspec )
__attribute__((nonnull (1)));

bool lcfgpkgspec_set_release( LCFGPackageSpec * pkgspec, char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Prefix */

bool lcfgpkgspec_valid_prefix( char prefix );

bool lcfgpkgspec_has_prefix( const LCFGPackageSpec * pkgspec )
__attribute__((nonnull (1)));

char lcfgpkgspec_get_prefix( const LCFGPackageSpec * pkgspec )
__attribute__((nonnull (1)));

bool lcfgpkgspec_set_prefix( LCFGPackageSpec * pkgspec, char new_prefix )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgpkgspec_remove_prefix( LCFGPackageSpec * pkgspec )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Flags */

bool lcfgpkgspec_valid_flag_chr( const char flag );

bool lcfgpkgspec_valid_flags( const char * flag );

bool lcfgpkgspec_has_flag( const LCFGPackageSpec * pkgspec, char flag )
  __attribute__((nonnull (1)));

bool lcfgpkgspec_has_flags( const LCFGPackageSpec * pkgspec )
__attribute__((nonnull (1)));

char * lcfgpkgspec_get_flags( const LCFGPackageSpec * pkgspec )
__attribute__((nonnull (1)));

bool lcfgpkgspec_set_flags( LCFGPackageSpec * pkgspec, char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgpkgspec_add_flags( LCFGPackageSpec * pkgspec,
                            const char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Context Expression */

bool lcfgpkgspec_valid_context( const char * expr );

bool lcfgpkgspec_has_context( const LCFGPackageSpec * pkgspec )
  __attribute__((nonnull (1)));

char * lcfgpkgspec_get_context( const LCFGPackageSpec * pkgspec )
  __attribute__((nonnull (1)));

bool lcfgpkgspec_set_context( LCFGPackageSpec * pkgspec, char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgpkgspec_add_context( LCFGPackageSpec * pkgspec,
                              const char * extra_context )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Derivation */

bool lcfgpkgspec_has_derivation( const LCFGPackageSpec * pkgspec )
  __attribute__((nonnull (1)));

char * lcfgpkgspec_get_derivation( const LCFGPackageSpec * pkgspec )
  __attribute__((nonnull (1)));

bool lcfgpkgspec_set_derivation( LCFGPackageSpec * pkgspec, char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgpkgspec_add_derivation( LCFGPackageSpec * pkgspec,
                                 const char * extra_deriv )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Priority */

int lcfgpkgspec_get_priority( const LCFGPackageSpec * pkgspec )
  __attribute__((nonnull (1)));

bool lcfgpkgspec_set_priority( LCFGPackageSpec * pkgspec, int priority )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgpkgspec_is_active( const LCFGPackageSpec * pkgspec )
  __attribute__((nonnull (1)));

bool lcfgpkgspec_eval_priority( LCFGPackageSpec * pkgspec,
                                const LCFGContextList * ctxlist,
                                char ** msg )
    __attribute__((nonnull (1))) __attribute__((warn_unused_result));

void lcfgpkgspec_set_defaults(LCFGPackageSpec *pkgspec)
  __attribute__((nonnull (1)));

char * lcfgpkgspec_full_version( const LCFGPackageSpec * pkgspec )
  __attribute__((nonnull (1)));

char * lcfgpkgspec_id( const LCFGPackageSpec * pkgspec )
  __attribute__((nonnull (1)));

int lcfgpkgspec_compare_versions( const LCFGPackageSpec * pkgspec1,
                                  const LCFGPackageSpec * pkgspec2 )
  __attribute__((nonnull (1,2)));

int lcfgpkgspec_compare_names( const LCFGPackageSpec * pkgspec1,
                               const LCFGPackageSpec * pkgspec2 )
  __attribute__((nonnull (1,2)));

int lcfgpkgspec_compare_archs( const LCFGPackageSpec * pkgspec1,
                               const LCFGPackageSpec * pkgspec2 )
  __attribute__((nonnull (1,2)));

int lcfgpkgspec_compare( const LCFGPackageSpec * pkgspec1,
                         const LCFGPackageSpec * pkgspec2 )
  __attribute__((nonnull (1,2)));

bool lcfgpkgspec_equals( const LCFGPackageSpec * pkgspec1,
                         const LCFGPackageSpec * pkgspec2 )
  __attribute__((nonnull (1,2)));

bool lcfgpkgspec_from_string( const char * input,
                              LCFGPackageSpec ** result,
                              char ** msg)
  __attribute__((warn_unused_result));

ssize_t lcfgpkgspec_to_string( const LCFGPackageSpec * pkgspec,
                               const char * defarch,
                               unsigned int options,
                               char ** result, size_t * size )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

ssize_t lcfgpkgspec_to_cpp( const LCFGPackageSpec * pkgspec,
                            const char * defarch,
                            unsigned int options,
                            char ** result, size_t * size )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

ssize_t lcfgpkgspec_to_xml( const LCFGPackageSpec * pkgspec,
                            const char * defarch,
                            unsigned int options,
                            char ** result, size_t * size )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgpkgspec_print( const LCFGPackageSpec * pkgspec,
                        const char * defarch,
                        const char * style,
                        unsigned int options,
                        FILE * out )
  __attribute__((nonnull (1,5))) __attribute__((warn_unused_result));

char * lcfgpkgspec_build_message( const LCFGPackageSpec * pkgspec,
                                  const char *fmt, ... );

/* Package Lists */

struct LCFGPackageNode {
  LCFGPackageSpec * pkgspec;
  struct LCFGPackageNode * next;
};

typedef struct LCFGPackageNode LCFGPackageNode;

LCFGPackageNode * lcfgpkgnode_new(LCFGPackageSpec * pkgspec);

void lcfgpkgnode_destroy(LCFGPackageNode * pkgnode);

struct LCFGPackageList {
  LCFGPackageNode * head;
  LCFGPackageNode * tail;
  unsigned int size;
};

typedef struct LCFGPackageList LCFGPackageList;

LCFGPackageList * lcfgpkglist_new(void);
void lcfgpkglist_destroy(LCFGPackageList * pkglist);

LCFGChange lcfgpkglist_insert_next( LCFGPackageList * pkglist,
                                    LCFGPackageNode * pkgnode,
                                    LCFGPackageSpec * pkgspec )
  __attribute__((nonnull (1,3))) __attribute__((warn_unused_result));

LCFGChange lcfgpkglist_remove_next( LCFGPackageList * pkglist,
                                    LCFGPackageNode * pkgnode,
                                    LCFGPackageSpec ** pkgspec )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

#define lcfgpkglist_head(pkglist) ((pkglist)->head)
#define lcfgpkglist_tail(pkglist) ((pkglist)->tail)
#define lcfgpkglist_size(pkglist) ((pkglist)->size)

#define lcfgpkglist_is_empty(pkglist) ((pkglist)->size == 0)

#define lcfgpkglist_next(pkgnode)     ((pkgnode)->next)
#define lcfgpkglist_pkgspec(pkgnode)  ((pkgnode)->pkgspec)

#define lcfgpkglist_append(pkglist, pkgspec) ( lcfgpkglist_insert_next( pkglist, lcfgpkglist_tail(pkglist), pkgspec ) )

LCFGPackageNode * lcfgpkglist_find_node( const LCFGPackageList * pkglist,
                                         const char * name,
                                         const char * arch )
  __attribute__((nonnull (1,2)));

LCFGPackageSpec * lcfgpkglist_find_pkgspec( const LCFGPackageList * pkglist,
                                            const char * name,
                                            const char * arch )
  __attribute__((nonnull (1,2)));

/* Rules for merging a package into a list */

#define LCFG_PKGS_OPT_KEEP_ALL         1
#define LCFG_PKGS_OPT_SQUASH_IDENTICAL 2
#define LCFG_PKGS_OPT_USE_PRIORITY     4
#define LCFG_PKGS_OPT_USE_PREFIX       8

LCFGChange lcfgpkglist_merge_pkgspec( LCFGPackageList * pkglist,
                                      LCFGPackageSpec * pkgspec,
                                      unsigned int options,
                                      char ** msg )
  __attribute__((nonnull (1,2))) __attribute__((warn_unused_result));

LCFGChange lcfgpkglist_merge_list( LCFGPackageList * pkglist1,
                                   const LCFGPackageList * pkglist2,
                                   unsigned int options,
                                   char ** msg )
  __attribute__((nonnull (1,2))) __attribute__((warn_unused_result));

void lcfgpkglist_sort( LCFGPackageList * pkglist )
  __attribute__((nonnull (1)));

bool lcfgpkglist_print( const LCFGPackageList * pkglist,
                        const char * defarch,
                        const char * style,
                        unsigned int options,
                        FILE * out )
  __attribute__((nonnull (1,5))) __attribute__((warn_unused_result));

LCFGChange lcfgpkglist_to_rpmcfg( LCFGPackageList * active,
                                  LCFGPackageList * inactive,
                                  const char * defarch,
                                  const char * filename,
                                  const char * rpminc,
                                  time_t mtime,
                                  char ** msg )
  __attribute__((warn_unused_result));

bool lcfgpkgspec_from_rpm_filename( const char * input,
                                    LCFGPackageSpec ** result,
                                    char ** msg)
  __attribute__((warn_unused_result));

ssize_t lcfgpkgspec_to_rpm_filename( const LCFGPackageSpec * pkgspec,
                                     const char * defarch,
                                     unsigned int options,
                                     char ** result, size_t * size )
    __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgpkglist_from_rpm_dir( const char * rpmdir,
                               LCFGPackageList ** result,
                               char ** msg )
  __attribute__((warn_unused_result));

bool lcfgpkglist_from_rpmlist( const char * filename,
                               LCFGPackageList ** result,
                               char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgpkglist_to_rpmlist( const LCFGPackageList * pkglist,
                                   const char * defarch,
                                   const char * filename,
                                   time_t mtime,
                                   char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGPackageList * lcfgpkglist_search( const LCFGPackageList * pkglist,
                                      const char * pkgname,
                                      const char * pkgarch,
                                      const char * pkgver,
                                      const char * pkgrel )
  __attribute__((nonnull (1)));

/* Alternative approach to going through the package node list */

struct LCFGPackageIterator {
  LCFGPackageList * pkglist;
  LCFGPackageNode * current;
  bool done;
  bool manage;
};
typedef struct LCFGPackageIterator LCFGPackageIterator;

LCFGPackageIterator * lcfgpkgiter_new( LCFGPackageList * pkglist,
                                       bool manage )
  __attribute__((nonnull (1)));

void lcfgpkgiter_destroy( LCFGPackageIterator * iterator );

void lcfgpkgiter_reset( LCFGPackageIterator * iterator );

bool lcfgpkgiter_has_next( LCFGPackageIterator * iterator );

LCFGPackageSpec * lcfgpkgiter_next(LCFGPackageIterator * iterator);

#endif /* LCFG_PACKAGES_H */

/* eof */
