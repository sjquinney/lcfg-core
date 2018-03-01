/**
 * @file packages.h
 * @brief LCFG package handling library
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
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
#include "utils.h"

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

bool lcfgpackage_valid_name( const char * name )
  __attribute__((const));

bool lcfgpackage_has_name( const LCFGPackage * pkg );

const char * lcfgpackage_get_name( const LCFGPackage * pkg );

bool lcfgpackage_set_name( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

/* Architecture */

bool lcfgpackage_valid_arch( const char * arch )
  __attribute__((const));
bool lcfgpackage_has_arch( const LCFGPackage * pkg );

const char * lcfgpackage_get_arch( const LCFGPackage * pkg );

bool lcfgpackage_set_arch( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

/* Version */

bool lcfgpackage_valid_version( const char * version )
  __attribute__((const));
bool lcfgpackage_has_version( const LCFGPackage * pkg );

const char * lcfgpackage_get_version( const LCFGPackage * pkg );

bool lcfgpackage_set_version( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

/* Release */

bool lcfgpackage_valid_release( const char * release )
  __attribute__((const));
bool lcfgpackage_has_release( const LCFGPackage * pkg );

const char * lcfgpackage_get_release( const LCFGPackage * pkg );

bool lcfgpackage_set_release( LCFGPackage * pkg, char * new_value )
  __attribute__((warn_unused_result));

/* Prefix */

bool lcfgpackage_valid_prefix( char prefix )
  __attribute__((const));
bool lcfgpackage_has_prefix( const LCFGPackage * pkg );

char lcfgpackage_get_prefix( const LCFGPackage * pkg );

bool lcfgpackage_set_prefix( LCFGPackage * pkg, char new_prefix )
  __attribute__((warn_unused_result));

bool lcfgpackage_clear_prefix( LCFGPackage * pkg )
  __attribute__((warn_unused_result));

/* Flags */

bool lcfgpackage_valid_flag_chr( const char flag )
  __attribute__((const));
bool lcfgpackage_valid_flags( const char * flag )
  __attribute__((const));

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

bool lcfgpackage_valid_context( const char * expr )
  __attribute__((const));
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

char * lcfgpackage_full_version( const LCFGPackage * pkg );

char * lcfgpackage_id( const LCFGPackage * pkg );

bool lcfgpackage_match( const LCFGPackage * pkg,
			const char * name, const char * arch );

int compare_vstrings( const char * v1, const char * v2 )
  __attribute__((const));

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

unsigned long lcfgpackage_hash( const LCFGPackage * pkg );

/* CPP package list stuff */

typedef enum {
  LCFG_PKG_PRAGMA_CATEGORY,
  LCFG_PKG_PRAGMA_CONTEXT,
  LCFG_PKG_PRAGMA_DERIVE
} LCFGPkgPragma;

bool lcfgpackage_parse_pragma( const char * line,
			       LCFGPkgPragma * key, char ** value )
  __attribute__((warn_unused_result));

size_t lcfgpackage_pragma_length( LCFGPkgPragma key, const char * value,
                                  LCFGOption options );

ssize_t lcfgpackage_build_pragma( LCFGPkgPragma key, const char * value,
				  LCFGOption options,
				  char ** result, size_t * size )
  __attribute__((warn_unused_result));

bool lcfgpackage_store_options( char ** file, ...  )
  __attribute__((warn_unused_result));

/* Package Lists */

typedef enum {
  LCFG_PKGLIST_PK_NAME = 0,
  LCFG_PKGLIST_PK_ARCH = 1,
  LCFG_PKGLIST_PK_CTX  = 2
} LCFGPkgListPK;

typedef enum {
  LCFG_PKG_CONTAINER_LIST,
  LCFG_PKG_CONTAINER_SET
} LCFGPkgContainer;

/**
 * @brief A structure for storing LCFG packages as a single-linked list
 */

struct LCFGPackageList {
  /*@{*/
  LCFGSListNode * head;      /**< The first node in the list */
  LCFGSListNode * tail;      /**< The last node in the list */
  unsigned int size;         /**< The length of the list */
  LCFGPkgListPK primary_key; /**< Controls which package fields are used as primary key */
  LCFGMergeRule merge_rules; /**< Rules which control how packages are merged */
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

/**
 * @brief Get the number of packages in the package list
 *
 * This is a simple macro which can be used to get the number of
 * packages in the single-linked package list.
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 *
 * @return Integer Number of packages in the list
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

LCFGSListNode * lcfgpkglist_find_node( const LCFGPackageList * pkglist,
                                       const char * name,
                                       const char * arch );

LCFGPackage * lcfgpkglist_find_package( const LCFGPackageList * pkglist,
                                        const char * name,
                                        const char * arch );

bool lcfgpkglist_has_package( const LCFGPackageList * pkglist,
                              const char * name,
                              const char * arch );

LCFGPackage * lcfgpkglist_first_package( const LCFGPackageList * pkglist );

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
                        const char * base,
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
                                   const char * base,
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
  LCFGSListNode * current; /**< Current location in the list */
};
typedef struct LCFGPackageIterator LCFGPackageIterator;

LCFGPackageIterator * lcfgpkgiter_new( LCFGPackageList * list );

void lcfgpkgiter_destroy( LCFGPackageIterator * iterator );

void lcfgpkgiter_reset( LCFGPackageIterator * iterator );

bool lcfgpkgiter_has_next( LCFGPackageIterator * iterator );

LCFGPackage * lcfgpkgiter_next(LCFGPackageIterator * iterator);

/* Set */

#define LCFG_PKGSET_DEFAULT_SIZE 113
#define LCFG_PKGSET_LOAD_INIT 0.5
#define LCFG_PKGSET_LOAD_MAX  0.7

struct LCFGPackageSet {
  /*@{*/
  LCFGPackageList ** packages;
  unsigned long buckets;
  unsigned long entries;
  LCFGPkgListPK primary_key; /**< Controls which package fields are used as primary key */
  LCFGMergeRule merge_rules; /**< Rules which control how packages are merged */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGPackageSet LCFGPackageSet;

unsigned int lcfgpkgset_size( const LCFGPackageSet * pkgset );

#define lcfgpkgset_is_empty(pkgset) (pkgset == NULL || lcfgpkgset_size(pkgset) == 0)

LCFGPackageSet * lcfgpkgset_new();
void lcfgpkgset_destroy(LCFGPackageSet * pkgset);

void lcfgpkgset_acquire(LCFGPackageSet * pkgset);
void lcfgpkgset_relinquish( LCFGPackageSet * pkgset );

bool lcfgpkgset_set_merge_rules( LCFGPackageSet * pkgset,
                                 LCFGMergeRule new_rules )
  __attribute__((warn_unused_result));

LCFGMergeRule lcfgpkgset_get_merge_rules( const LCFGPackageSet * pkgset );

LCFGChange lcfgpkgset_merge_package( LCFGPackageSet * pkgset,
                                     LCFGPackage * new_pkg,
                                     char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgpkgset_merge_list( LCFGPackageSet * pkgset,
                                  const LCFGPackageList * pkglist,
                                  char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgpkgset_merge_set( LCFGPackageSet * pkgset1,
                                 const LCFGPackageSet * pkgset2,
                                 char ** msg )
  __attribute__((warn_unused_result));

LCFGPackageList * lcfgpkgset_find_list( const LCFGPackageSet * pkgset,
                                        const char * want_name );

LCFGPackage * lcfgpkgset_find_package( const LCFGPackageSet * pkgset,
                                       const char * want_name,
                                       const char * want_arch );

bool lcfgpkgset_has_package( const LCFGPackageSet * pkgset,
                             const char * want_name,
                             const char * want_arch );

bool lcfgpkgset_print( const LCFGPackageSet * pkgset,
                       const char * defarch,
                       const char * base,
                       LCFGPkgStyle style,
                       LCFGOption options,
                       FILE * out )
  __attribute__((warn_unused_result));

LCFGStatus lcfgpkgset_from_rpmlist( const char * filename,
                                     LCFGPackageSet ** result,
                                     LCFGOption options,
                                     char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgpkgset_from_rpm_dir( const char * rpmdir,
                                    LCFGPackageSet ** result,
                                    char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgpkgset_to_rpmlist( LCFGPackageSet * pkgset,
                                  const char * defarch,
                                  const char * base,
                                  const char * filename,
                                  time_t mtime,
                                  char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgpkgset_from_cpp( const char * filename,
                                LCFGPackageSet ** result,
                                const char * defarch,
                                LCFGOption options,
                                char ** msg )
  __attribute__((warn_unused_result));

LCFGChange lcfgpkgset_to_rpmcfg( LCFGPackageSet * active,
                                 LCFGPackageSet * inactive,
                                 const char * defarch,
                                 const char * filename,
                                 const char * rpminc,
                                 time_t mtime,
                                 char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgpkgset_from_rpm_db( const char * rootdir,
                                   LCFGPackageSet ** result,
                                   char ** msg )
  __attribute__((warn_unused_result));

LCFGPackageSet * lcfgpkgset_match( const LCFGPackageSet * pkgset,
                                   const char * want_name,
                                   const char * want_arch,
                                   const char * want_ver,
                                   const char * want_rel );

struct LCFGPkgSetIterator {
  LCFGPackageSet * set;
  LCFGPackageIterator * listiter;
  long current;
};
typedef struct LCFGPkgSetIterator LCFGPkgSetIterator;

LCFGPkgSetIterator * lcfgpkgsetiter_new( LCFGPackageSet * pkgset );

void lcfgpkgsetiter_destroy( LCFGPkgSetIterator * iterator );

void lcfgpkgsetiter_reset( LCFGPkgSetIterator * iterator );

bool lcfgpkgsetiter_has_next( LCFGPkgSetIterator * iterator );

LCFGPackage * lcfgpkgsetiter_next(LCFGPkgSetIterator * iterator);

#endif /* LCFG_CORE_PACKAGES_H */

/* eof */
