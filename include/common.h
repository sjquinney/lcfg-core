/* Common definitions */

#ifndef LCFG_COMMON_H
#define LCFG_COMMON_H

/**
 * @brief Test if a string (i.e. a char *) is 'empty'
 */

#define isempty(STR) ( STR == NULL || *(STR) == '\0' )

/**
 * @brief Test for word character
 *
 * Extends the standard alpha-numeric test to include the '_'
 * (underscore) character. This is similar to the '\w' in Perl
 * regexps, this gives the set of characters [A-Za-z0-9_]
 *
 */

#define isword(CHR) ( isalnum(CHR) || CHR == '_' )

typedef enum {
  LCFG_STATUS_ERROR,
  LCFG_STATUS_WARN,
  LCFG_STATUS_OK
} LCFGStatus;

typedef enum {
  LCFG_CHANGE_ERROR,
  LCFG_CHANGE_NONE,
  LCFG_CHANGE_MODIFIED,
  LCFG_CHANGE_ADDED,
  LCFG_CHANGE_REPLACED,
  LCFG_CHANGE_REMOVED
} LCFGChange;

/* Various options for functions which read in and write out resources */

#define LCFG_OPT_NOCONTEXT      1
#define LCFG_OPT_NOPREFIX       2
#define LCFG_OPT_NEWLINE        4
#define LCFG_OPT_NOVALUE        8
#define LCFG_OPT_NOTEMPLATES   16
#define LCFG_OPT_ALLOW_NOEXIST 32
#define LCFG_OPT_ENCODE        64
#define LCFG_OPT_ALL_CONTEXTS 128
#define LCFG_OPT_USE_META     256

typedef enum {
  LCFG_TEST_ISTRUE,
  LCFG_TEST_ISFALSE,
  LCFG_TEST_ISEQ,
  LCFG_TEST_ISNE
} LCFGTest;

#endif /* LCFG_COMMON_H */

/* eof */

