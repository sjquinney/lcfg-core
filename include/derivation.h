/**
 * @file derivation.h
 * @brief Functions for working with LCFG derivation information
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2018 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#ifndef LCFG_CORE_DERIVATION_H
#define LCFG_CORE_DERIVATION_H

#include "common.h"
#include "utils.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/**
 * @brief Structure to represent LCFG derivation information for a single file
 */

struct LCFGDerivation {
  /*@{*/
  char * file;              /**< The file name (required) */
  unsigned int * lines;     /**< Array of line numbers (optional) */
  unsigned int lines_size;  /**< Size of the line number array */
  unsigned int lines_count; /**< Number of lines */
  ssize_t length;           /**< Cached length of serialised form */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGDerivation LCFGDerivation;

LCFGDerivation * lcfgderivation_new(void);
LCFGDerivation * lcfgderivation_clone( const LCFGDerivation * drv );
void lcfgderivation_destroy( LCFGDerivation * drv );
void lcfgderivation_acquire( LCFGDerivation * drv );
void lcfgderivation_relinquish( LCFGDerivation * drv );
bool lcfgderivation_is_shared( const LCFGDerivation * drv );

bool lcfgderivation_is_valid( const LCFGDerivation * drv );

bool lcfgderivation_has_file( const LCFGDerivation * drv );
bool lcfgderivation_set_file( LCFGDerivation * drv,
                              char * new_value )
    __attribute__((warn_unused_result));
const char * lcfgderivation_get_file( const LCFGDerivation * drv );

bool lcfgderivation_has_lines( const LCFGDerivation * drv );
bool lcfgderivation_has_line( const LCFGDerivation * drv,
                              unsigned int line );
LCFGChange lcfgderivation_merge_line( LCFGDerivation * drv, unsigned int line )
  __attribute__((warn_unused_result));
LCFGChange lcfgderivation_merge_lines( LCFGDerivation * drv1,
                                       const LCFGDerivation * drv2 )
  __attribute__((warn_unused_result));
void lcfgderivation_sort_lines( LCFGDerivation * drv );

ssize_t lcfgderivation_get_length( const LCFGDerivation * drv );

ssize_t lcfgderivation_to_string( const LCFGDerivation * drv,
                                  LCFGOption options,
                                  char ** result, size_t * size )
  __attribute__((warn_unused_result));

LCFGStatus lcfgderivation_from_string( const char * input,
                                       LCFGDerivation ** result,
                                       char ** msg )
  __attribute__((warn_unused_result));

bool lcfgderivation_print( const LCFGDerivation * drv,
                           LCFGOption options,
                           FILE * out )
  __attribute__((warn_unused_result));

int lcfgderivation_compare_files( const LCFGDerivation * drv1,
                                  const LCFGDerivation * drv2 );

bool lcfgderivation_same_file( const LCFGDerivation * drv1,
                               const LCFGDerivation * drv2 );

int lcfgderivation_compare( const LCFGDerivation * drv1,
                            const LCFGDerivation * drv2 );

bool lcfgderivation_match( const LCFGDerivation * drv,
                           const char * file );


/* Lists */

/**
 * @brief Structure to represent LCFG derivation information for multiple files
 */

struct LCFGDerivationList {
  /*@{*/
  LCFGSListNode * head;   /**< The first derivation in the list */
  LCFGSListNode * tail;   /**< The last derivation in the list */
  unsigned int size;      /**< The length of the list */
  uint64_t id;            /**< Hash of stringified version of list */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGDerivationList LCFGDerivationList;

/**
 * @brief Test if the list of derivations is empty
 *
 * This is a simple macro which can be used to test if the
 * single-linked derivation list contains any items.
 *
 * @param[in] list Pointer to @c LCFGDerivationList
 *
 * @return Boolean which indicates whether the list contains any items
 *
 */

#define lcfgderivlist_is_empty(list) (list == NULL || (list)->size == 0)

LCFGDerivationList * lcfgderivlist_new(void);
void lcfgderivlist_destroy(LCFGDerivationList * drvlist);
void lcfgderivlist_acquire( LCFGDerivationList * drvlist );
void lcfgderivlist_relinquish( LCFGDerivationList * drvlist );
bool lcfgderivlist_is_shared( const LCFGDerivationList * drvlist );

LCFGDerivationList * lcfgderivlist_clone( const LCFGDerivationList * drvlist );

size_t lcfgderivlist_get_string_length( const LCFGDerivationList * drvlist );

LCFGSListNode * lcfgderivlist_find_node( const LCFGDerivationList * drvlist,
                                         const char * want_file );
LCFGDerivation * lcfgderivlist_find_derivation( const LCFGDerivationList * drvlist,
                                                const char * want_file );
bool lcfgderivlist_contains( const LCFGDerivationList * drvlist,
                             const char * want_file );

LCFGChange lcfgderivlist_merge_derivation( LCFGDerivationList * drvlist,
                                           LCFGDerivation     * new_drv )
  __attribute__((warn_unused_result));

LCFGChange lcfgderivlist_merge_list( LCFGDerivationList * drvlist1,
                                     const LCFGDerivationList * drvlist2 )
   __attribute__((warn_unused_result));

LCFGChange lcfgderivlist_merge_file_line( LCFGDerivationList * drvlist,
                                          const char * filename,
                                          int line )
  __attribute__((warn_unused_result));

LCFGChange lcfgderivlist_merge_string_list( LCFGDerivationList * drvlist,
                                            const char * input,
                                            char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgderivlist_from_string( const char * input,
                                      LCFGDerivationList ** result,
                                      char ** msg )
   __attribute__((warn_unused_result));

ssize_t lcfgderivlist_to_string( const LCFGDerivationList * drvlist,
                                 LCFGOption options,
                                 char ** result, size_t * size )
  __attribute__((warn_unused_result));

bool lcfgderivlist_print( const LCFGDerivationList * drvlist,
                          FILE * out )
  __attribute__((warn_unused_result));

/* Maps */

#define LCFG_DRVMAP_DEFAULT_SIZE 1999
#define LCFG_DRVMAP_LOAD_INIT 0.5
#define LCFG_DRVMAP_LOAD_MAX  0.7

/**
 * @brief Structure to for fast lookup of LCFG derivation lists
 */

struct LCFGDerivationMap {
  /*@{*/
  LCFGDerivationList ** derivations; /**< Array of derivation lists */
  unsigned long buckets;             /**< Number of buckets in map */
  unsigned long entries;             /**< Number of full buckets in map */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGDerivationMap LCFGDerivationMap;

LCFGDerivationMap * lcfgderivmap_new(void);
void lcfgderivmap_acquire( LCFGDerivationMap * drvmap );
void lcfgderivmap_relinquish( LCFGDerivationMap * drvmap );
bool lcfgderivmap_is_shared( const LCFGDerivationMap * drvmap );
LCFGChange lcfgdrvmap_insert_list( LCFGDerivationMap * drvmap,
                                   LCFGDerivationList * drvlist,
                                   char ** msg )
  __attribute__((warn_unused_result));
LCFGDerivationList * lcfgderivmap_find_or_insert_string(
                                           LCFGDerivationMap * drvmap,
                                           const char * deriv_as_str,
                                           char ** msg )
  __attribute__((warn_unused_result));

#endif /* LCFG_CORE_DERIVATION_H */

/* eof */
