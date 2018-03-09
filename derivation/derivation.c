/**
 * @file derivation/derivation.c
 * @brief Functions for working with single file LCFG derivations
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2018 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "common.h"
#include "utils.h"
#include "derivation.h"

/* For speed the length is cached when computed, a negative value
   forces recalculation */

#define ResetLength(DRV) (DRV)->length = -1;

/**
 * @brief Create and initialise a new derivation
 *
 * Creates a new @c LCFGDerivation and initialises the parameters to the
 * default values.
 *
 * This is used to represent the derivation information for an LCFG
 * resource or package from a single file, possibly occurring on
 * multiple lines within that file. Typically an LCFG resource or
 * package will be modified in multiple files which is represented
 * using an @c LCFGDerivationList.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgderivation_relinquish() function should be called.
 *
 * @return Pointer to new @c LCFGDerivation
 *
 */

LCFGDerivation * lcfgderivation_new(void) {

  LCFGDerivation * drv = malloc( sizeof(LCFGDerivation) );
  if ( drv == NULL ) {
    perror( "Failed to allocate memory for LCFG derivation" );
    exit(EXIT_FAILURE);
  }

  /* Default values */

  drv->file        = NULL;
  drv->lines       = NULL;
  drv->lines_count = 0;
  drv->lines_size  = 0;
  drv->_refcount   = 1;

  ResetLength(drv);

  return drv;
}

/**
 * @brief Destroy the derivation
 *
 * When the specified @c LCFGDerivation is no longer required this will
 * free all associated memory. There is support for reference counting
 * so typically the @c lcfgderivation_relinquish() function should be used.
 *
 * This will call @c free() on each parameter of the structure and
 * then set each value to be @c NULL.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * derivation which has already been destroyed (or potentially was never
 * created).
 *
 * @param[in] drv Pointer to @c LCFGDerivation to be destroyed.
 *
 */

void lcfgderivation_destroy( LCFGDerivation * drv ) {

  if ( drv == NULL ) return;

  free(drv->file);
  drv->file = NULL;

  free(drv->lines);
  drv->lines = NULL;

  free(drv);
  drv = NULL;

}

/**
 * @brief Acquire reference to derivation
 *
 * This is used to record a reference to the @c LCFGDerivation, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgderivation_relinquish() function should be
 * called.
 *
 * @param[in] drv Pointer to @c LCFGDerivation
 *
 */

void lcfgderivation_acquire( LCFGDerivation * drv ) {
  assert( drv != NULL );

  drv->_refcount += 1;
}

/**
 * @brief Release reference to derivation
 *
 * This is used to release a reference to the @c LCFGDerivation,
 * it does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgderivation_destroy() function
 * will be called to clean up the memory associated with the structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * derivation which has already been destroyed (or potentially was never
 * created).
 *
 * @param[in] drv Pointer to @c LCFGDerivation
 *
 */

void lcfgderivation_relinquish( LCFGDerivation * drv ) {

  if ( drv == NULL ) return;

  if ( drv->_refcount > 0 )
    drv->_refcount -= 1;

  if ( drv->_refcount == 0 )
    lcfgderivation_destroy(drv);

}

/**
 * @brief Check validity of derivation
 *
 * Checks the specified @c LCFGDerivation to ensure that it is valid. For a
 * derivation to be considered valid the pointer must not be @c NULL and
 * the derivation must have a value for the @e file attribute.
 *
 * @param[in] drv Pointer to an @c LCFGDerivation
 *
 * @return Boolean which indicates if derivation is valid
 *
 */

bool lcfgderivation_is_valid( const LCFGDerivation * drv ) {
  return ( drv != NULL && !isempty(drv->file) );
}

/**
 * @brief Check if the derivation has a file
 *
 * Checks if the specified @c LCFGDerivation currently has a value set
 * for the @e file attribute. Although a file is required for an LCFG
 * derivation to be valid it is possible for the value of the file to
 * be set to @c NULL when the structure is first created.
 *
 * @param[in] drv Pointer to an @c LCFGDerivation
 *
 * @return boolean which indicates if a derivation has a file
 *
 */

bool lcfgderivation_has_file( const LCFGDerivation * drv ) {
  assert( drv != NULL );

  return !isempty(drv->file);
}

/**
 * @brief Set the file for the derivation
 *
 * Sets the value of the @e file parameter for the @c LCFGDerivation
 * to that specified. It is important to note that this does
 * @b NOT take a copy of the string. Furthermore, once the value is set
 * the derivation assumes "ownership", the memory will be freed if the
 * file is further modified or the derivation is destroyed.
 *
 * There is no validation, any string will be considered as a valid
 * derivation file path.
 *
 * @param[in] drv Pointer to an @c LCFGDerivation
 * @param[in] new_value String which is the new file
 *
 * @return boolean indicating success
 *
 */

bool lcfgderivation_set_file( LCFGDerivation * drv,
                              char * new_value ) {
  assert( drv != NULL );

  free(drv->file);

  drv->file   = new_value;
  ResetLength(drv);

  return true;
}

/**
 * @brief Get the file for the derivation
 *
 * This returns the value of the @e file parameter for the
 * @c LCFGDerivation. If the derivation does not currently have a @e
 * file then the pointer returned will be @c NULL.
 *
 * It is important to note that this is @b NOT a copy of the string,
 * changing the returned string will modify the @e file of the
 * derivation.
 *
 * @param[in] drv Pointer to an @c LCFGDerivation
 *
 * @return The @e file for the derivation (possibly @c NULL).
 *
 */

const char * lcfgderivation_get_file( const LCFGDerivation * drv ) {
  assert( drv != NULL );

  return drv->file;
}

/**
 * @brief Check if the derivation information contains line numbers
 *
 * An @c LCFGDerivation optionally contains a list of line numbers for
 * the locations in the file where a resource or package specification
 * is modified. This can be used to check if line number information
 * is available.
 *
 * @param[in] drv Pointer to an @c LCFGDerivation
 *
 * @return Boolean which indicates if derivation has line numbers
 *
 */

bool lcfgderivation_has_lines( const LCFGDerivation * drv ) {
  return ( drv != NULL && drv->lines != NULL && drv->lines_count > 0 );
}

/**
 * @brief Check if derivation has a specific line number
 *
 * An @c LCFGDerivation optionally contains a list of line numbers for
 * the locations in the file where a resource or package specification
 * is modified. This can be used to check if that list contains a
 * particular line number.
 *
 * @param[in] drv Pointer to an @c LCFGDerivation
 * @param[in] line Line number to check
 *
 * @return Boolean which indicates if derivation has specific line number
 *
 */


bool lcfgderivation_has_line( const LCFGDerivation * drv,
                              unsigned int line ) {

  if ( !lcfgderivation_has_lines(drv) ) return false;

  bool found = false;

  unsigned int i;
  for ( i=0; !found && i<drv->lines_count; i++ )
    if ( (drv->lines)[i] == line ) found = true;

  return found;
}

/**
 * @brief Add a line number to the list for the derivation
 *
 * This will add a line number to the list for the @c LCFGDerivation
 * if it is not already present.
 *
 * If the line is already present nothing will change and the return
 * value will be @c LCFG_CHANGE_NONE otherwise, if successful, @c
 * LCFG_CHANGE_ADDED will be returned. If an error occurs @c
 * LCFG_CHANGE_ERROR is returned.
 *
 * @param[in] drv Pointer to an @c LCFGDerivation
 * @param[in] line Line number to add
 *
 * @return Integer value indicating type of change
 */

LCFGChange lcfgderivation_add_line( LCFGDerivation * drv, unsigned int line ) {
  assert( drv != NULL );

  if ( lcfgderivation_has_line( drv, line ) ) return LCFG_CHANGE_NONE;

  /* Avoid reallocating too frequently */

  unsigned int new_size = drv->lines_count + 1;
  if ( drv->lines == NULL || drv->lines_size == 0 ) {
    new_size = 4;
  } else if ( new_size > drv->lines_size ) {
    new_size = 2 * drv->lines_size;
  }

  if ( new_size > drv->lines_size ) {
    unsigned int * new_lines = realloc( drv->lines,
                                        new_size * sizeof(unsigned int) );
    if ( new_lines == NULL ) {
      perror("Failed to create array for derivation line numbers");
      exit(EXIT_FAILURE);
    } else {

      /* For safety, initialise the extension section of the array */
      unsigned int i;
      for ( i=drv->lines_size; i<new_size; i++ )
        new_lines[i] = 0;

      drv->lines_size = new_size;
      drv->lines      = new_lines;
    }
  }

  (drv->lines)[drv->lines_count] = line;
  drv->lines_count++;
  ResetLength(drv); /* reset so it's recalculated when needed */

  return LCFG_CHANGE_ADDED;
}

/**
 * @brief Merge line information for two derivations
 *
 * This merges the list of line numbers in the second @c
 * LCFGDerivation into the list for the first @c LCFGDerivation. Note
 * that no check is done to compare the file names.
 *
 * If all the line numbers in the second @c LCFGDerivation are already
 * present then nothing will change and the return value will be @c
 * LCFG_CHANGE_NONE otherwise, if successful, @c LCFG_CHANGE_MODIFIED
 * will be returned. If an error occurs @c LCFG_CHANGE_ERROR is
 * returned.

 * @param[in] drv1 Pointer to an @c LCFGDerivation into which lines are merged
 * @param[in] drv2 Pointer to an @c LCFGDerivation from which lines are read
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgderivation_merge_lines( LCFGDerivation * drv1,
                                       const LCFGDerivation * drv2 ) {
  assert( drv1 != NULL );

  if ( !lcfgderivation_has_lines(drv2) ) return LCFG_CHANGE_NONE;

  LCFGChange change = LCFG_CHANGE_NONE;

  unsigned int i;
  for ( i=0; LCFGChangeOK(change) && i<drv2->lines_count; i++ ) {
    unsigned int line = (drv2->lines)[i];

    LCFGChange add_change = lcfgderivation_add_line( drv1, line );

    if ( add_change == LCFG_CHANGE_ERROR )
      change = LCFG_CHANGE_ERROR;
    else if ( add_change == LCFG_CHANGE_ADDED )
      change = LCFG_CHANGE_MODIFIED;
  }

  return change;
}

static int uint_compare( const void * a, const void * b ) {
  const unsigned int * a_uint = a;
  const unsigned int * b_uint = b;

  if ( *a_uint < *b_uint )
    return -1;
  else if ( *a_uint > *b_uint )
    return 1;
  else
    return 0;
}

/**
 * @brief Sort the list of line numbers
 *
 * This can be used to do an in-place numerical sort of the list of
 * line numbers. That is mostly useful prior to serialisation for
 * ensuring that the derivation information is always identical.
 *
 * @param[in] drv Pointer to an @c LCFGDerivation
 *
 */

void lcfgderivation_sort_lines( LCFGDerivation * drv ) {
  assert( drv != NULL );

  if ( lcfgderivation_has_lines(drv) )
    qsort( drv->lines, drv->lines_count, sizeof(unsigned int), &uint_compare );

}

/**
 * @brief Format the derivation as a string
 *
 * Generates a new string representation of the @c
 * LCFGDerivation. This will consist of at least the filename. If
 * there are line numbers then that list will be joined together with
 * ',' (comma) and then appened to the filename with a ':' (colon),
 * for example, @c foo.rpms:1,7,56 If the output must be consistent
 * then it is recommended to call @c lcfgderivation_sort first.
 *
 * The following options are supported:
 *   - @c LCFG_OPT_NEWLINE - append a final newline character
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many derivation strings, this can be a
 * huge performance benefit. If the buffer is initially unallocated
 * then it MUST be set to @c NULL. The current size of the buffer must
 * be passed and should be specified as zero if the buffer is
 * initially unallocated. If the generated string would be too long
 * for the current buffer then it will be resized and the size
 * parameter is updated.
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] drv Pointer to an @c LCFGDerivation
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgderivation_to_string( const LCFGDerivation * drv,
                                  LCFGOption options,
                                  char ** result, size_t * size ) {
  assert( drv != NULL );

  if ( !lcfgderivation_is_valid(drv) ) return -1;

  size_t file_len = strlen(drv->file);

  size_t new_len = file_len;

  /* Optional newline at end of string */

  if ( options&LCFG_OPT_NEWLINE )
    new_len += 1;

  char ** lines_as_str = NULL;
  if ( lcfgderivation_has_lines(drv) ) {
    new_len += 1; /* for ':' (colon) between file and lines */
    new_len += drv->lines_count - 1; /* for ',' (comma) between numbers */

    lines_as_str = calloc( drv->lines_count, sizeof(char *) );
    if ( lines_as_str == NULL ) {
      perror("Failed to allocate memory for list of line numbers");
      exit(EXIT_FAILURE);
    }

    unsigned int i;
    for (i=0; i<drv->lines_count; i++ ) {
      char * as_str = NULL;
      int rc = asprintf( &as_str, "%u", (drv->lines)[i] );
      if ( rc > 0 ) {
        new_len += rc;
        lines_as_str[i] = as_str;
      } else {
        perror("Failed to allocate memory for line number as string");
        exit(EXIT_FAILURE);
      }
    }
  }

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    size_t new_size = new_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG derivation string");
      exit(EXIT_FAILURE);
    } else {
      *result = new_buf;
      *size   = new_size;
    }
  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  to = stpncpy( to, drv->file, file_len );

  if ( lcfgderivation_has_lines(drv) ) {

    *to = ':';
    to++;

    unsigned int i;
    for (i=0; i<drv->lines_count; i++ ) {
      to = stpcpy( to, lines_as_str[i] );
      free(lines_as_str[i]);

      if ( i < drv->lines_count - 1 ) {
        *to = ',';
        to++;
      }
    }
  }

  free(lines_as_str);

  if ( options&LCFG_OPT_NEWLINE )
    to = stpncpy( to, "\n", 1 );

  *to = '\0';

  assert( ( *result + new_len ) == to ) ;

  return new_len;
}

/**
 * @brief Write formatted derivation to file stream
 *
 * This uses @c lcfgderivation_to_string() to format the @c
 * LCFGDerivation as a string with a trailing newline character. The
 * generated string is written to the specified file stream which must
 * have already been opened for writing.
 *
 * @param[in] drv Pointer to @c LCFGDerivation
 * @param[in] out Stream to which the derivation string should be written
 *
 * @return boolean indicating success
 *
 */

bool lcfgderivation_print( const LCFGDerivation * drv,
                           LCFGOption options,
                           FILE * out ) {
  assert( drv != NULL );

  options |= LCFG_OPT_NEWLINE;

  char * as_str = NULL;
  size_t size = 0;

  bool ok = true;

  ssize_t len = lcfgderivation_to_string( drv, options, &as_str, &size );
  if ( len <= 0 ) ok = false;

  if (ok) {
    if ( fputs( as_str, out ) < 0 )
      ok = false;
  }

  free(as_str);

  return ok;
}

static bool uint_valid( const char * value ) {

  bool valid = !isempty(value);

  const char * ptr;
  for ( ptr=value; valid && *ptr!='\0'; ptr++ )
    if ( !isdigit(*ptr) ) valid = false;

  return valid;
}

/**
 * Create a new derivation from a string
 *
 * This parses a single LCFG derivation as a string in the form
 * "foo.rpms:1,5,9" or "bar.h:7,21" and creates a new @c
 * LCFGDerivation struct. Any leading whitespace will be ignored. The
 * filename is required, line numbers are optional.
 *
 * To avoid memory leaks, when the newly created @c LCFGDerivation
 * struct is no longer required you should call the @c
 * lcfgderivation_relinquish() function.
 *
 * @param[in] input The context specification string.
 * @param[out] result Reference to the pointer for the @c LCFGDerivation struct.
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgderivation_from_string( const char * input,
                                       LCFGDerivation ** result,
                                       char ** msg ) {
  if ( isempty(input) ) {
    lcfgutils_build_message( msg, "Invalid derivation string" );
    return LCFG_STATUS_ERROR;
  }

  while( *input != '\0' && isspace(*input) ) input++;

  if ( isempty(input) ) {
    lcfgutils_build_message( msg, "Invalid derivation string" );
    return LCFG_STATUS_ERROR;
  }

  LCFGStatus status = LCFG_STATUS_OK;

  LCFGDerivation * drv = lcfgderivation_new();

  const char * sep = strrchr( input, ':' );

  char * file;
  if ( sep != NULL ) {
    file = strndup( input, sep - input );

    /* Line numbers */

    if ( *( sep + 1 ) != '\0' ) {
      char * lines = strdup(sep+1);
      const char * start = lines;
      char * end = NULL;
      while ( status != LCFG_STATUS_ERROR &&
              ( end = strchr(start, ',' ) ) != NULL ) {
        *end = '\0';
        if ( uint_valid(start) ) {
          unsigned int line = strtoul( start, NULL, 0 );
          LCFGChange add_rc = lcfgderivation_add_line( drv, line );
          if ( LCFGChangeError(add_rc) ) {
            lcfgutils_build_message( msg,
                                     "Invalid derivation line number '%s'",
                                     start );
            status = LCFG_STATUS_ERROR;
          }
        }
        start = end + 1;
      }

      /* Any remaining line number */

      if ( status != LCFG_STATUS_ERROR && uint_valid(start) ) {
        unsigned int line = strtoul( start, NULL, 0 );
        LCFGChange add_rc = lcfgderivation_add_line( drv, line );
        if ( LCFGChangeError(add_rc) ) {
          lcfgutils_build_message( msg,
                                   "Invalid derivation line number '%s'",
                                   start );
          status = LCFG_STATUS_ERROR;
        }
      }

      free(lines);
    }

  } else {
    file = strdup(input);
  }

  if ( status != LCFG_STATUS_ERROR ) {
    bool set_ok = lcfgderivation_set_file( drv, file );
    if ( !set_ok ) {
      lcfgutils_build_message( msg,
                               "Invalid derivation file name '%s'",
                               file );
      status = LCFG_STATUS_ERROR;
    }
  }

  if ( status == LCFG_STATUS_ERROR ) {
    lcfgderivation_relinquish(drv);
    drv = NULL;
  }

  *result = drv;

  return status;
}

/**
 * @brief Compare the derivation names
 *
 * This compares the values of the @c file attributes for the two
 * derivations, this is mostly useful for sorting lists of
 * derivations. An integer value is returned which indicates lesser
 * than, equal to or greater than in the same way as @c strcmp(3).
 *
 * @param[in] drv1 Pointer to @c LCFGDerivation
 * @param[in] drv2 Pointer to @c LCFGDerivation
 * 
 * @return Integer (-1,0,+1) indicating lesser,equal,greater
 *
 */

int lcfgderivation_compare_files( const LCFGDerivation * drv1,
                                  const LCFGDerivation * drv2 ) {
  assert( drv1 != NULL );
  assert( drv2 != NULL );

  const char * file1 = or_default( drv1->file, "" );
  const char * file2 = or_default( drv2->file, "" );

  return strcmp( file1, file2 );
}

/**
 * @brief Test if derivations have same file
 *
 * Compares the values for the @e file attribute for the two
 * derivations using the @c lcfgderivation_compare_files() function.
 *
 * @param[in] drv1 Pointer to @c LCFGDerivation
 * @param[in] drv2 Pointer to @c LCFGDerivation
 *
 * @return boolean indicating equality of names
 *
 */

bool lcfgderivation_same_file( const LCFGDerivation * drv1,
                               const LCFGDerivation * drv2 ) {
  assert( drv1 != NULL );
  assert( drv2 != NULL );

  return ( lcfgderivation_compare_files( drv1, drv2 ) == 0 );
}

/**
 * @brief Compare derivations
 *
 * This compares two derivations using the @c
 * lcfgderivation_compare_files function. This is mostly useful for
 * sorting lists of derivations. All comparisons are done simply using
 * strcmp().
 *
 * @param[in] drv1 Pointer to @c LCFGDerivation
 * @param[in] drv2 Pointer to @c LCFGDerivation
 * 
 * @return Integer (-1,0,+1) indicating lesser,equal,greater
 *
 */

int lcfgderivation_compare( const LCFGDerivation * drv1,
                            const LCFGDerivation * drv2 ) {
  assert( drv1 != NULL );
  assert( drv2 != NULL );

  return lcfgderivation_compare_files( drv1, drv2 );
}

/**
 * @brief Test if derivation matches name
 *
 * This compares the value for the @e file attribute for the @c
 * LCFGDerivation with the specified string.
 *
 * @param[in] drv Pointer to @c LCFGDerivation
 * @param[in] file The file to check for a match
 *
 * @return boolean indicating equality of values
 *
 */

bool lcfgderivation_match( const LCFGDerivation * drv,
                           const char * file ) {
  assert( drv != NULL );
  assert( file != NULL );

  const char * drv_file = or_default( drv->file, "" );
  return ( strcmp( drv_file, file ) == 0 );
}

/**
 * @brief Get the length of the serialised form of the derivation
 *
 * It is sometimes necessary to know the length of the serialised form
 * of the derivation. Serialising the @c LCFGDerivation just to
 * calculate this length would be expensive so this just goes through
 * the motions. Even without serialisation this could be expensive so
 * the value is only recalculated when necessary if the file name or
 * list of line numbers has changed.
 *
 * @param[in] drv Pointer to @c LCFGDerivation
 *
 * @return Length of serialised form (or -1 for error)
 *
 */

ssize_t lcfgderivation_get_length( const LCFGDerivation * drv ) {
  assert( drv != NULL );

  if ( !lcfgderivation_is_valid(drv) ) return -1;

  /* If non-negative it has already been calculated and cached */
  if ( drv->length >= 0 ) return drv->length;

  size_t len = strlen(drv->file);

  if ( lcfgderivation_has_lines(drv) ) {
    len += 1;                         /* for ':' separator */
    len += ( drv->lines_count - 1 );  /* for ',' separators */

    unsigned int i;
    for ( i=0; i<drv->lines_count; i++ ) {
      unsigned int line = (drv->lines)[i];

      while ( line > 9 ) { len++; line/=10; }
      len++;

    }
  }

  /* Cache the length to avoid recalculation next time */
  ((LCFGDerivation * ) drv)->length = len;

  return drv->length;
}

/* eof */
