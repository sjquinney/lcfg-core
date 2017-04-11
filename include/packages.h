
#ifndef LCFG_PACKAGES_H
#define LCFG_PACKAGES_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "common.h"
#include "context.h"

const char * default_architecture(void);

#define LCFG_PACKAGE_NOVALUE ""
#define LCFG_PACKAGE_WILDCARD "*"

struct LCFGPackage {
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

typedef struct LCFGPackage LCFGPackage;

LCFGPackage * lcfgpackage_new(void);
void lcfgpackage_destroy(LCFGPackage * pkg);
void lcfgpackage_acquire( LCFGPackage * pkg );
void lcfgpackage_relinquish( LCFGPackage * pkg );

LCFGPackage * lcfgpackage_clone( const LCFGPackage * pkg );

/* Name */

bool lcfgpackage_valid_name( const char * name );
bool lcfgpackage_has_name( const LCFGPackage * pkg );

char * lcfgpackage_get_name( const LCFGPackage * pkg );

bool lcfgpackage_set_name( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

/* Architecture */

bool lcfgpackage_valid_arch( const char * arch );
bool lcfgpackage_has_arch( const LCFGPackage * pkg );

char * lcfgpackage_get_arch( const LCFGPackage * pkg );

bool lcfgpackage_set_arch( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

/* Version */

bool lcfgpackage_valid_version( const char * version );
bool lcfgpackage_has_version( const LCFGPackage * pkg );

char * lcfgpackage_get_version( const LCFGPackage * pkg );

bool lcfgpackage_set_version( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

/* Release */

bool lcfgpackage_valid_release( const char * release );
bool lcfgpackage_has_release( const LCFGPackage * pkg );

char * lcfgpackage_get_release( const LCFGPackage * pkg );

bool lcfgpackage_set_release( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

/* Prefix */

bool lcfgpackage_valid_prefix( char prefix );

bool lcfgpackage_has_prefix( const LCFGPackage * pkg );

char lcfgpackage_get_prefix( const LCFGPackage * pkg );

bool lcfgpackage_set_prefix( LCFGPackage * pkg, char new_prefix )
  __attribute__((warn_unused_result));

bool lcfgpackage_clear_prefix( LCFGPackage * pkg )
  __attribute__((warn_unused_result));

/* Flags */

bool lcfgpackage_valid_flag_chr( const char flag );

bool lcfgpackage_valid_flags( const char * flag );

bool lcfgpackage_has_flag( const LCFGPackage * pkg, char flag );

bool lcfgpackage_has_flags( const LCFGPackage * pkg );

char * lcfgpackage_get_flags( const LCFGPackage * pkg );

bool lcfgpackage_clear_flags( LCFGPackage * pkg )
  __attribute__((warn_unused_result));

bool lcfgpackage_set_flags( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

bool lcfgpackage_add_flags( LCFGPackage * pkg,
                            const char * new_value )
  __attribute__((warn_unused_result));

/* Context Expression */

bool lcfgpackage_valid_context( const char * expr );

bool lcfgpackage_has_context( const LCFGPackage * pkg );

char * lcfgpackage_get_context( const LCFGPackage * pkg );

bool lcfgpackage_set_context( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

bool lcfgpackage_add_context( LCFGPackage * pkg,
                              const char * extra_context )
  __attribute__((warn_unused_result));

/* Derivation */

bool lcfgpackage_has_derivation( const LCFGPackage * pkg );

char * lcfgpackage_get_derivation( const LCFGPackage * pkg );

bool lcfgpackage_set_derivation( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

bool lcfgpackage_add_derivation( LCFGPackage * pkg,
                                 const char * extra_deriv )
  __attribute__((warn_unused_result));

/* Priority */

int lcfgpackage_get_priority( const LCFGPackage * pkg );

bool lcfgpackage_set_priority( LCFGPackage * pkg, int priority )
  __attribute__((warn_unused_result));

bool lcfgpackage_is_active( const LCFGPackage * pkg );

bool lcfgpackage_eval_priority( LCFGPackage * pkg,
                                const LCFGContextList * ctxlist,
				char ** msg )
  __attribute__((warn_unused_result));

void lcfgpackage_set_defaults(LCFGPackage *pkg);

char * lcfgpackage_full_version( const LCFGPackage * pkg );

char * lcfgpackage_id( const LCFGPackage * pkg );

int lcfgpackage_compare_versions( const LCFGPackage * pkg1,
                                  const LCFGPackage * pkg2 );

int lcfgpackage_compare_names( const LCFGPackage * pkg1,
                               const LCFGPackage * pkg2 );

int lcfgpackage_compare_archs( const LCFGPackage * pkg1,
                               const LCFGPackage * pkg2 );

int lcfgpackage_compare( const LCFGPackage * pkg1,
                         const LCFGPackage * pkg2 );

bool lcfgpackage_equals( const LCFGPackage * pkg1,
                         const LCFGPackage * pkg2 );

LCFGStatus lcfgpackage_from_string( const char * input,
                                    LCFGPackage ** result,
                                    char ** msg)
  __attribute__((warn_unused_result));

ssize_t lcfgpackage_to_string(const LCFGPackage * pkg,
			      const char * defarch,
			      LCFGPkgStyle style,
			      LCFGOption options,
			      char ** result, size_t * size )
  __attribute__((warn_unused_result));

typedef ssize_t (*LCFGPkgStrFunc) ( const LCFGPackage * pkg,
				    const char * defarch,
				    LCFGOption options,
				    char ** result, size_t * size );

ssize_t lcfgpackage_to_spec( const LCFGPackage * pkg,
			     const char * defarch,
			     LCFGOption options,
			     char ** result, size_t * size )
  __attribute__((warn_unused_result));

ssize_t lcfgpackage_to_cpp( const LCFGPackage * pkg,
                            const char * defarch,
                            LCFGOption options,
                            char ** result, size_t * size )
  __attribute__((warn_unused_result));

ssize_t lcfgpackage_to_xml( const LCFGPackage * pkg,
                            const char * defarch,
                            LCFGOption options,
                            char ** result, size_t * size )
  __attribute__((warn_unused_result));

typedef enum {
  LCFG_PKG_STYLE_SPEC,
  LCFG_PKG_STYLE_RPM,
  LCFG_PKG_STYLE_CPP,
  LCFG_PKG_STYLE_XML,
  LCFG_PKG_STYLE_EVAL
} LCFGPkgStyle;

bool lcfgpackage_print( const LCFGPackage * pkg,
                        const char * defarch,
                        LCFGPkgStyle style,
                        LCFGOption options,
                        FILE * out )
  __attribute__((warn_unused_result));

char * lcfgpackage_build_message( const LCFGPackage * pkg,
                                  const char *fmt, ... );

/* Package Lists */

/**
 * @brief Rules for merging a package into a list
 */

typedef enum {
  LCFG_PKG_OPT_NONE             = 0, /**< Null Option */
  LCFG_PKG_OPT_KEEP_ALL         = 1, /**< Keep all packages */
  LCFG_PKG_OPT_SQUASH_IDENTICAL = 2, /**< Ignore extra identical package */
  LCFG_PKG_OPT_USE_PRIORITY     = 4, /**< Merge using context priority */
  LCFG_PKG_OPT_USE_PREFIX       = 8  /**< Merge using package prefix */
} LCFGPkgOption;

struct LCFGPackageNode {
  LCFGPackage * pkg;
  struct LCFGPackageNode * next;
};

typedef struct LCFGPackageNode LCFGPackageNode;

LCFGPackageNode * lcfgpkgnode_new(LCFGPackage * pkg);

void lcfgpkgnode_destroy(LCFGPackageNode * pkgnode);

struct LCFGPackageList {
  LCFGPackageNode * head;
  LCFGPackageNode * tail;
  LCFGPkgOption merge_rules;
  unsigned int size;
  unsigned int _refcount;
};

typedef struct LCFGPackageList LCFGPackageList;

LCFGPackageList * lcfgpkglist_new(void);
void lcfgpkglist_destroy(LCFGPackageList * pkglist);

void lcfgpkglist_acquire( LCFGPackageList * pkglist );
void lcfgpkglist_relinquish( LCFGPackageList * pkglist );

LCFGPkgOption lcfgpkglist_get_merge_rules( const LCFGPackageList * pkglist );

bool lcfgpkglist_set_merge_rules( LCFGPackageList * pkglist,
				  LCFGPkgOption new_rules )
  __attribute__((warn_unused_result));

LCFGChange lcfgpkglist_insert_next( LCFGPackageList * pkglist,
                                    LCFGPackageNode * pkgnode,
                                    LCFGPackage     * pkg )
  __attribute__((warn_unused_result));

LCFGChange lcfgpkglist_remove_next( LCFGPackageList * pkglist,
                                    LCFGPackageNode * pkgnode,
                                    LCFGPackage    ** pkg )
  __attribute__((warn_unused_result));

#define lcfgpkglist_head(pkglist) ((pkglist)->head)
#define lcfgpkglist_tail(pkglist) ((pkglist)->tail)
#define lcfgpkglist_size(pkglist) ((pkglist)->size)

#define lcfgpkglist_is_empty(pkglist) (pkglist == NULL || (pkglist)->size == 0)

#define lcfgpkglist_next(pkgnode)     ((pkgnode)->next)
#define lcfgpkglist_package(pkgnode)  ((pkgnode)->pkg)

#define lcfgpkglist_append(pkglist, pkg) ( lcfgpkglist_insert_next( pkglist, lcfgpkglist_tail(pkglist), pkg ) )

LCFGPackageNode * lcfgpkglist_find_node( const LCFGPackageList * pkglist,
                                         const char * name,
                                         const char * arch );

LCFGPackage * lcfgpkglist_find_package( const LCFGPackageList * pkglist,
                                        const char * name,
                                        const char * arch );

LCFGChange lcfgpkglist_merge_package( LCFGPackageList * pkglist,
                                      LCFGPackage * pkg,
                                      char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgpkglist_merge_list( LCFGPackageList * pkglist1,
                                   const LCFGPackageList * pkglist2,
                                   char ** msg )
  __attribute__((warn_unused_result));

void lcfgpkglist_sort( LCFGPackageList * pkglist );

bool lcfgpkglist_print( const LCFGPackageList * pkglist,
                        const char * defarch,
                        LCFGPkgStyle style,
                        LCFGOption options,
                        FILE * out )
  __attribute__((warn_unused_result));

LCFGStatus lcfgpkglist_from_cpp( const char * filename,
				 LCFGPackageList ** result,
				 const char * defarch,
                                 LCFGOption options,
				 char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgpkglist_to_rpmcfg( LCFGPackageList * active,
                                  LCFGPackageList * inactive,
                                  const char * defarch,
                                  const char * filename,
                                  const char * rpminc,
                                  time_t mtime,
                                  char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgpackage_from_rpm_filename( const char * input,
                                          LCFGPackage ** result,
                                          char ** msg)
  __attribute__((warn_unused_result));

ssize_t lcfgpackage_to_rpm_filename( const LCFGPackage * pkg,
                                     const char * defarch,
                                     LCFGOption options,
                                     char ** result, size_t * size )
  __attribute__((warn_unused_result));

LCFGStatus lcfgpkglist_from_rpm_dir( const char * rpmdir,
                                     LCFGPackageList ** result,
                                     char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgpkglist_from_rpmlist( const char * filename,
                                     LCFGPackageList ** result,
                                     char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgpkglist_to_rpmlist( LCFGPackageList * pkglist,
                                   const char * defarch,
                                   const char * filename,
                                   time_t mtime,
                                   char ** msg )
  __attribute__((warn_unused_result));

LCFGPackageList * lcfgpkglist_search( const LCFGPackageList * pkglist,
                                      const char * pkgname,
                                      const char * pkgarch,
                                      const char * pkgver,
                                      const char * pkgrel );

/* Alternative approach to going through the package node list */

struct LCFGPackageIterator {
  LCFGPackageList * pkglist;
  LCFGPackageNode * current;
  bool done;
};
typedef struct LCFGPackageIterator LCFGPackageIterator;

LCFGPackageIterator * lcfgpkgiter_new( LCFGPackageList * pkglist );

void lcfgpkgiter_destroy( LCFGPackageIterator * iterator );

void lcfgpkgiter_reset( LCFGPackageIterator * iterator );

bool lcfgpkgiter_has_next( LCFGPackageIterator * iterator );

LCFGPackage * lcfgpkgiter_next(LCFGPackageIterator * iterator);

#endif /* LCFG_PACKAGES_H */

/* eof */
