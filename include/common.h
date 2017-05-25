/**
 * @file common.h
 * @brief LCFG common constants
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#ifndef LCFG_CORE_COMMON_H
#define LCFG_CORE_COMMON_H

/**
 * @brief Test if a string (i.e. a char *) is 'empty'
 */

#define isempty(STR) ( STR == NULL || *(STR) == '\0' )

/**
 * @brief Return the string if not empty otherwise return the default
 */

#define or_default(STR,DEFAULT) isempty(STR) ? DEFAULT : STR;

/**
 * @brief Test for word character
 *
 * Extends the standard alpha-numeric test to include the '_'
 * (underscore) character. This is similar to the @c '\w' in Perl
 * regexps, this gives the set of characters [A-Za-z0-9_]
 *
 */

#define isword(CHR) ( isalnum(CHR) || CHR == '_' )

/**
 * @brief Status code
 */

typedef enum {
  LCFG_STATUS_ERROR, /**< Unrecoverable error occurred */
  LCFG_STATUS_WARN,  /**< Unexpected behaviour occurred which may require attention */
  LCFG_STATUS_OK     /**< Success */
} LCFGStatus;

/**
 * @brief State change code
 */

typedef enum {
  LCFG_CHANGE_ERROR       =   0, /**< Unrecoverable error occurred */
  LCFG_CHANGE_NONE        =   1, /**< Success - No change */
  LCFG_CHANGE_MODIFIED    =   2, /**< Success - Modification */
  LCFG_CHANGE_ADDED       =   4, /**< Success - Addition */
  LCFG_CHANGE_REPLACED    =   8, /**< Success - Replacement */
  LCFG_CHANGE_REMOVED     =  16  /**< Success - Removal */
} LCFGChange;

/**
 * @brief Various options for functions which read in or write out
 */

typedef enum {
  LCFG_OPT_NONE           =   0, /**< Null option */
  LCFG_OPT_NOCONTEXT      =   1, /**< Ignore context */
  LCFG_OPT_NOPREFIX       =   2, /**< Ignore prefix */
  LCFG_OPT_NEWLINE        =   4, /**< Include newline */
  LCFG_OPT_NOVALUE        =   8, /**< Ignore value */
  LCFG_OPT_NOTEMPLATES    =  16, /**< Ignore templates */
  LCFG_OPT_ALLOW_NOEXIST  =  32, /**< Allow object to not exist */
  LCFG_OPT_ENCODE         =  64, /**< Encode data */
  LCFG_OPT_ALL_CONTEXTS   = 128, /**< Include all contexts */
  LCFG_OPT_ALL_PRIORITIES = 256, /**< Include all priorities */
  LCFG_OPT_USE_META       = 512, /**< Include metadata */
  LCFG_OPT_ALL_VALUES     = 1024 /**< Include all values */
} LCFGOption;

/**
 * @brief Options to control how resources or packages are merged into lists
 */

typedef enum {
  LCFG_MERGE_RULE_NONE             = 0, /**< Null Option */
  LCFG_MERGE_RULE_KEEP_ALL         = 1, /**< Keep all */
  LCFG_MERGE_RULE_SQUASH_IDENTICAL = 2, /**< Ignore extra identical */
  LCFG_MERGE_RULE_USE_PRIORITY     = 4, /**< Merge according to context priority */
  LCFG_MERGE_RULE_USE_PREFIX       = 8  /**< Merge/mutate according to prefix */
} LCFGMergeRule;

#endif /* LCFG_CORE_COMMON_H */

/* eof */

