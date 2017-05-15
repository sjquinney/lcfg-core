/**
 * @file packages.h
 * @brief LCFG package handling library
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date$
 * $Revision$
 */

#ifndef LCFG_CORE_PACKAGES_H
#define LCFG_CORE_PACKAGES_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "common.h"
#include "context.h"

const char * default_architecture(void);

#define LCFG_PACKAGE_NOVALUE ""
#define LCFG_PACKAGE_WILDCARD "*"

/**
 * @brief A structure to represent an LCFG Package
 *
 */

struct LCFGPackage {
  /*@{*/
  char * name;       /**< Name (required) */
  char * arch;       /**< Architecture (e.g. x86_64 or i686) */
  char * version;    /**< Version */
  char * release;    /**< Release (not used on all platforms) */
  char * flags;      /**< Flags - controls behaviour of package tool (e.g. updaterpms) */
  char * context;    /**< Context expression - when this package is applicable */
  char * derivation; /**< Derivation - where this package was specified */
  char prefix;       /**< Prefix - primary merge conflict resolution for multiple specifications (single alpha-numeric character) */
  int priority;      /**< Priority - result of evaluating context expression, secondary merge conflict resolution */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGPackage LCFGPackage;

LCFGPackage * lcfgpackage_new(void);
void lcfgpackage_destroy(LCFGPackage * pkg);
void lcfgpackage_acquire( LCFGPackage * pkg );
void lcfgpackage_relinquish( LCFGPackage * pkg );

LCFGPackage * lcfgpackage_clone( const LCFGPackage * pkg );

bool lcfgpackage_is_valid( const LCFGPackage * pkg );

/* Name */

bool lcfgpackage_valid_name( const char * name );
bool lcfgpackage_has_name( const LCFGPackage * pkg );

const char * lcfgpackage_get_name( const LCFGPackage * pkg );

bool lcfgpackage_set_name( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

/* Architecture */

bool lcfgpackage_valid_arch( const char * arch );
bool lcfgpackage_has_arch( const LCFGPackage * pkg );

const char * lcfgpackage_get_arch( const LCFGPackage * pkg );

bool lcfgpackage_set_arch( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

/* Version */

bool lcfgpackage_valid_version( const char * version );
bool lcfgpackage_has_version( const LCFGPackage * pkg );

const char * lcfgpackage_get_version( const LCFGPackage * pkg );

bool lcfgpackage_set_version( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

/* Release */

bool lcfgpackage_valid_release( const char * release );
bool lcfgpackage_has_release( const LCFGPackage * pkg );

const char * lcfgpackage_get_release( const LCFGPackage * pkg );

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

const char * lcfgpackage_get_flags( const LCFGPackage * pkg );

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

const char * lcfgpackage_get_context( const LCFGPackage * pkg );

bool lcfgpackage_set_context( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

bool lcfgpackage_add_context( LCFGPackage * pkg,
                              const char * extra_context )
  __attribute__((warn_unused_result));

/* Derivation */

bool lcfgpackage_has_derivation( const LCFGPackage * pkg );

const char * lcfgpackage_get_derivation( const LCFGPackage * pkg );

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

LCFGStatus lcfgpackage_from_spec( const char * input,
                                    LCFGPackage ** result,
                                    char ** msg)
  __attribute__((warn_unused_result));

/**
 * @brief Package format styles
 *
 * This can be used to control the format 'style' which is used when a
 * package is formatted as a string.
 *
 */

typedef enum {
  LCFG_PKG_STYLE_SPEC, /**< Standard LCFG package specification */
  LCFG_PKG_STYLE_RPM,  /**< RPM filename */
  LCFG_PKG_STYLE_CPP,  /**< LCFG CPP block (as used by updaterpms) */
  LCFG_PKG_STYLE_XML,  /**< LCFG XML block (as used by client/server) */
  LCFG_PKG_STYLE_SUMMARY, /**< qxpack style summary */
  LCFG_PKG_STYLE_EVAL  /**< Shell variables */
} LCFGPkgStyle;

ssize_t lcfgpackage_to_string( const LCFGPackage * pkg,
                               const char * defarch,
                               LCFGPkgStyle style,
                               LCFGOption options,
                               char ** result, size_t * size )
  __attribute__((warn_unused_result));

#define LCFG_PKG_TOSTR_ARGS \
    const LCFGPackage * pkg,\
    const char * defarch,\
    LCFGOption options,\
    char ** result, size_t * size

typedef ssize_t (*LCFGPkgStrFunc) ( LCFG_PKG_TOSTR_ARGS );

ssize_t lcfgpackage_to_spec( LCFG_PKG_TOSTR_ARGS )
  __attribute__((warn_unused_result));

ssize_t lcfgpackage_to_cpp( LCFG_PKG_TOSTR_ARGS )
  __attribute__((warn_unused_result));

ssize_t lcfgpackage_to_summary( LCFG_PKG_TOSTR_ARGS )
  __attribute__((warn_unused_result));

ssize_t lcfgpackage_to_xml( LCFG_PKG_TOSTR_ARGS )
  __attribute__((warn_unused_result));

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
 * @brief A structure to wrap an LCFG package as a single-linked list item
 */

struct LCFGPackageNode {
  LCFGPackage * pkg; /**< Pointer to the package structure */
  struct LCFGPackageNode * next;
};

typedef struct LCFGPackageNode LCFGPackageNode;

LCFGPackageNode * lcfgpkgnode_new(LCFGPackage * pkg);

void lcfgpkgnode_destroy(LCFGPackageNode * pkgnode);

/**
 * @brief A structure for storing LCFG packages as a single-linked list
 */

struct LCFGPackageList {
  /*@{*/
  LCFGPackageNode * head;  /**< The first package node in the list */
  LCFGPackageNode * tail;  /**< The last package node in the list */
  LCFGMergeRule merge_rules; /**< Rules which control how packages are merged */
  unsigned int size;       /**< The length of the list */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGPackageList LCFGPackageList;

LCFGPackageList * lcfgpkglist_new(void);
void lcfgpkglist_destroy(LCFGPackageList * pkglist);

void lcfgpkglist_acquire( LCFGPackageList * pkglist );
void lcfgpkglist_relinquish( LCFGPackageList * pkglist );

LCFGMergeRule lcfgpkglist_get_merge_rules( const LCFGPackageList * pkglist );

bool lcfgpkglist_set_merge_rules( LCFGPackageList * pkglist,
				  LCFGMergeRule new_rules )
  __attribute__((warn_unused_result));

LCFGChange lcfgpkglist_insert_next( LCFGPackageList * pkglist,
                                    LCFGPackageNode * pkgnode,
                                    LCFGPackage     * pkg )
  __attribute__((warn_unused_result));

LCFGChange lcfgpkglist_remove_next( LCFGPackageList * pkglist,
                                    LCFGPackageNode * pkgnode,
                                    LCFGPackage    ** pkg )
  __attribute__((warn_unused_result));

/**
 * @brief Retrieve the first package node in the list
 *
 * This is a simple macro which can be used to get the first package
 * node structure in the list. Note that if the list is empty this
 * will be the @c NULL value. To retrieve the package from this node
 * use @c lcfgpkglist_package()
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 *
 * @return Pointer to first @c LCFGPackageNode structure in list
 *
 */

#define lcfgpkglist_head(pkglist) ((pkglist)->head)

/**
 * @brief Retrieve the last package node in the list
 *
 * This is a simple macro which can be used to get the last package
 * node structure in the list. Note that if the list is empty this
 * will be the @c NULL value. To retrieve the package from this node
 * use @c lcfgpkglist_package()
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 *
 * @return Pointer to last @c LCFGPackageNode structure in list
 *
 */

#define lcfgpkglist_tail(pkglist) ((pkglist)->tail)

/**
 * @brief Get the number of nodes in the package list
 *
 * This is a simple macro which can be used to get the length of the
 * single-linked package list.
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 *
 * @return Integer length of the package list
 *
 */

#define lcfgpkglist_size(pkglist) ((pkglist)->size)

/**
 * @brief Test if the package list is empty
 *
 * This is a simple macro which can be used to test if the
 * single-linked package list contains any items.
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 *
 * @return Boolean which indicates whether the list contains any items
 *
 */

#define lcfgpkglist_is_empty(pkglist) (pkglist == NULL || (pkglist)->size == 0)

/**
 * @brief Retrieve the next package node in the list
 *
 * This is a simple macro which can be used to fetch the next node in
 * the single-linked package list for a given node. If the node
 * specified is the final item in the list this will return a @c NULL
 * value.
 *
 * @param[in] pkgnode Pointer to current @c LCFGPackageNode
 *
 * @return Pointer to next @c LCFGPackageNode
 */

#define lcfgpkglist_next(pkgnode)     ((pkgnode)->next)

/**
 * @brief Retrieve the package for a list node
 *
 * This is a simple macro which can be used to get the package
 * structure from the specified node. 
 *
 * Note that this does @b NOT increment the reference count for the
 * returned package structure. To retain the package call the
 * @c lcfgpackage_acquire() function.
 *
 * @param[in] pkgnode Pointer to @c LCFGPackageNode
 *
 * @return Pointer to @c LCFGPackage structure
 *
 */

#define lcfgpkglist_package(pkgnode)  ((pkgnode)->pkg)

/**
 * @brief Append a package to a list
 *
 * This is a simple macro wrapper around the
 * @c lcfgpkglist_insert_next() function which can be used to append
 * a package structure on to the end of the specified package list.
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 * @param[in] pkg Pointer to @c LCFGPackage
 * 
 * @return Integer value indicating type of change
 *
 */

#define lcfgpkglist_append(pkglist, pkg) ( lcfgpkglist_insert_next( pkglist, lcfgpkglist_tail(pkglist), pkg ) )

LCFGPackageNode * lcfgpkglist_find_node( const LCFGPackageList * pkglist,
                                         const char * name,
                                         const char * arch );

LCFGPackage * lcfgpkglist_find_package( const LCFGPackageList * pkglist,
                                        const char * name,
                                        const char * arch );

bool lcfgpkglist_contains( const LCFGPackageList * pkglist,
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

ssize_t lcfgpackage_to_rpm_filename( LCFG_PKG_TOSTR_ARGS )
  __attribute__((warn_unused_result));

LCFGStatus lcfgpkglist_from_rpm_dir( const char * rpmdir,
                                     LCFGPackageList ** result,
                                     char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgpkglist_from_rpmlist( const char * filename,
                                     LCFGPackageList ** result,
                                     LCFGOption options,
                                     char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgpkglist_to_rpmlist( LCFGPackageList * pkglist,
                                   const char * defarch,
                                   const char * filename,
                                   time_t mtime,
                                   char ** msg )
  __attribute__((warn_unused_result));

LCFGPackageList * lcfgpkglist_match( const LCFGPackageList * pkglist,
                                     const char * name,
                                     const char * arch,
                                     const char * ver,
                                     const char * rel );

/**
 * @brief Simple iterator for package lists
 */

struct LCFGPackageIterator {
  LCFGPackageList * list; /**< The package list */
  LCFGPackageNode * current; /**< Current location in the list */
};
typedef struct LCFGPackageIterator LCFGPackageIterator;

LCFGPackageIterator * lcfgpkgiter_new( LCFGPackageList * list );

void lcfgpkgiter_destroy( LCFGPackageIterator * iterator );

void lcfgpkgiter_reset( LCFGPackageIterator * iterator );

bool lcfgpkgiter_has_next( LCFGPackageIterator * iterator );

LCFGPackage * lcfgpkgiter_next(LCFGPackageIterator * iterator);

#endif /* LCFG_CORE_PACKAGES_H */

/* eof */
