/**
 * @file utils/utils.c
 * @brief Commonly useful functions
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#define _GNU_SOURCE /* for asprintf */

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "common.h"
#include "utils.h"

static const char * tmp_dir_names[] = {
  "LCFGTMP", "TMPDIR", "TEMP", "TMP", NULL
};

#ifndef LCFG_DEFAULT_TMP
#define LCFG_DEFAULT_TMP "/tmp"
#endif

static const char tmp_default_dir[] = LCFG_DEFAULT_TMP;

/**
 * Find the directory for temporary files
 *
 * This will search for one of the following environment variables (in
 * this order) @c LCFGTMP, @c TMPDIR, @c TEMP and @c TMP. The first
 * variable to have a non-empty value will be used as the directory
 * for temporary files. If none of those have a value then the default
 * of @c /tmp will be used.
 *
 * @return Path to the directory for temporary files
 *
 */

const char * lcfgutils_tmp_dirname(void) {

  const char * tmpdir = NULL;

  const char ** ptr;
  for ( ptr = tmp_dir_names; *ptr != NULL && tmpdir == NULL; ptr++ ) {
    const char * env_val = getenv(*ptr);
    if ( !isempty(env_val) )
      tmpdir = env_val;
  }

  if ( tmpdir == NULL )
    tmpdir = tmp_default_dir;

  return tmpdir;
}

/**
 * @brief Generate a safe temporary file name
 *
 * Given a target file name this will generate a safe temporary file
 * path in the same directory which is suitable for use with
 * mkstemp. The particular advantage of being in the same directory as
 * the target file is that it can always be renamed atomically.
 *
 * If the target file path is @c NULL then this will use the @c
 * lcfgutils_tmp_dirname() function to select the appropriate
 * directory for the temporary file.
 *
 * If the memory allocation for the new string is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * @param[in] path Pointer to target file name (may be @c NULL)
 *
 * @return Pointer to a new string (call @c free(3) when no longer required)
 * 
 */

static const char tmp_template[] = ".lcfg.XXXXXX";
const size_t tmp_template_len = sizeof(tmp_template) - 1;

char * lcfgutils_safe_tmpfile( const char * path ) {

  size_t base_len = 0;
  if ( path == NULL ) {
    path = lcfgutils_tmp_dirname();
    base_len = strlen(path);
  } else {
    const char * match = strrchr( path, '/' );
    if ( match != NULL )
      base_len = match - path;
  }

  size_t new_len = tmp_template_len;
  if ( base_len > 0 )
    new_len += ( base_len + 1 ); /* +1 for '/' */

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate memory for temporary file name");
    exit(EXIT_FAILURE);
  }

  char * to = result;

  if ( base_len > 0 ) {
    to = stpncpy( to, path, base_len );

    *to = '/';
    to++;
  }

  to = stpncpy( to, tmp_template, tmp_template_len );

  *to = '\0';

  assert( (result + new_len) == to );

  return result;
}

/**
 * @brief Combine two strings with an optional separator.
 *
 * This creates a new string using the separator string and the two
 * strings which should be combined. If the separator is @c NULL then
 * the strings are combined without a separator. If either string is
 * @c NULL then it will be considered to be an empty string. For
 * example, if the separator is @c "-" (hyphen) then this will result
 * in strings like @c "A-", @c "-B" or @c "-" (if both are @c NULL).
 *
 * If the memory allocation for the new string is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * @param[in] sep Pointer to separator string
 * @param[in] str1 Pointer to first string
 * @param[in] str2 Pointer to second string
 *
 * @return Pointer to a new string (call @c free(3) when no longer required)
 *
 */

char * lcfgutils_string_join( const char * sep,
                              const char * str1,
                              const char * str2 ) {

  size_t str1_len = str1 != NULL ? strlen(str1) : 0;
  size_t str2_len = str2 != NULL ? strlen(str2) : 0;
  size_t sep_len  = sep  != NULL ? strlen(sep)  : 0;

  size_t new_len = str1_len + sep_len + str2_len;

  char * result = calloc( (new_len + 1), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate memory for new string");
    exit(EXIT_FAILURE);
  }

  char * to = result;

  if ( str1_len > 0 )
    to = stpncpy( to, str1, str1_len );

  if ( sep_len > 0 )
    to = stpncpy( to, sep, sep_len );

  if ( str2_len > 0 )
    to = stpncpy( to, str2, str2_len );

  *to = '\0';

  assert( ( result + new_len ) == to );

  return result;
}

/**
 * @brief In-place trim trailing newline characters
 *
 * This trims any newline or carriage return characters from the end
 * of the specified string by replacing them with a nul character.
 *
 * @param[in,out] str Pointer to string which should be trimmed
 *
 */

void lcfgutils_string_chomp( char * str ) {

  if ( isempty(str) ) return;

  char * ptr = str + strlen(str) - 1; /* start on final character */
  while ( ptr != str ) {
    if ( *ptr == '\r' || *ptr == '\n' ) {
      *ptr = '\0';
      ptr--;
    } else {
      break;
    }
  }

}

/**
 * @brief In-place trim leading and trailing whitespace
 *
 * This will remove any whitespace characters (according to the
 * @c isspace(3) function) from the start and end of the specified
 * string.
 *
 * @param[in,out] str Pointer to string which should be trimmed
 *
 */

void lcfgutils_string_trim( char * str ) {

  if ( isempty(str) ) return;

  size_t len = strlen(str);

  /* trim from left */

  char * ptr = str;
  while ( *ptr != '\0' && isspace(*ptr) ) {
    ptr++;
    len--;
  }

  if ( ptr != str && len > 0 )
    memmove( str, ptr, len );

  *( str + len ) = '\0';

  if ( len == 0 )
    return;

  /* trim from right */

  for ( ptr = str + len - 1; ptr != str && isspace(*ptr); ptr-- )
    *ptr = '\0';

}

/**
 * @brief Combine directory and file name to create full path.
 *
 * This combines a directory path and a file name to create a full
 * path. A @c "/" (forward slash) separator will be inserted between
 * the two parts if the specified directory does not have one as the
 * final character.
 *
 * If the memory allocation for the new string is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * @param[in] dir Pointer to string containing directory path
 * @param[in] file Pointer to string containing file name
 *
 * @return Pointer to a new string (call @c free(3) when no longer required)
 */

char * lcfgutils_catfile( const char * dir, const char * file ) {

  if ( file == NULL ) return NULL;

  size_t file_len = strlen(file);
  size_t new_len = file_len;

  size_t dir_len = 0;
  bool needs_slash = false;
  if ( dir != NULL ) {
    dir_len = strlen(dir);

    if ( dir_len > 0 ) {

      /* Move backwards past all trailing forward-slashes */
      while( dir_len > 0 && dir[dir_len - 1] == '/' ) dir_len--;

      new_len += dir_len;

      if ( *file != '/' &&
	   ( dir_len == 0 || dir[dir_len - 1] != '/' ) ) {
	needs_slash = true;
	new_len += 1;
      }
    }

  }

  /* Allocate the required space */

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate memory for LCFG file path");
    exit(EXIT_FAILURE);
  }

  /* Build the new string */

  char * to = result;

  if ( dir_len > 0 )
    to = stpncpy( to, dir, dir_len );

  if (needs_slash) {
    *to = '/';
    to++;
  }

  to = stpncpy( to, file, file_len );

  *to = '\0';

  assert( ( result + new_len ) == to );

  return result;
}

/**
 * @brief Check if a string ends with a particular suffix
 *
 * Checks if the specified suffix string is found at the end of the
 * specified string. Note that if either of the string or suffix are
 * @c NULL then this will always return @c false.
 *
 * @param[in] str Pointer to string which should be checked
 * @param[in] suffix Pointer to suffix string
 *
 * @return boolean which indicates if string has specified suffix
 *
 */

bool lcfgutils_string_endswith( const char * str, const char * suffix ) {

  if ( isempty(str) || isempty(suffix) ) return false;

  size_t str_len    = strlen(str);
  size_t suffix_len = strlen(suffix);

  return ( str_len >= suffix_len &&
           strncmp( str + str_len - suffix_len, suffix, suffix_len ) == 0 );
}

/**
 * @brief Strip the directory and suffix parts of a path
 *
 * Given a path this will remove any leading directory parts of the
 * path. If the suffix is not @c NULL then that will also be stripped.
 *
 * If this fails a @c NULL value will be returned.
 *
 * @param[in] path Pointer to string containing full path
 * @param[in] suffix Pointer to string containing suffix be stripped
 *
 * @return Pointer to a new string (call @c free(3) when no longer required)
 */

char * lcfgutils_basename( const char * path, const char * suffix ) {

  if ( isempty(path) ) return NULL;

  const char * start = path;
  const char * end   = start + strlen(path) - 1;

  /* This removes all trailing '/' characters */
  while ( end != start && *end == '/' ) end--;

  /* Find the start of the base which is after the final '/' (if any) */
  if ( start != end ) {

    const char * ptr = end;
    while ( ptr != start && *ptr != '/') ptr--;

    start = *ptr == '/' ? ptr + 1 : ptr;
  }

  size_t len = end - start + 1;

  if ( suffix != NULL && lcfgutils_string_endswith( start, suffix ) ) {
    len -= strlen(suffix);
  }

  char * base = strndup( start, len );

  return base;
}

/**
 * @brief Extract the directory part of a path
 *
 * Given a path this will extract the directory part of the path with
 * any file name stripped and any trailing slashes removed.
 *
 * If this fails a @c NULL value will be returned.
 *
 * @param[in] path Pointer to string containing path
 *
 * @return Pointer to a new string (call @c free(3) when no longer required)
 */

char * lcfgutils_dirname( const char * path ) {

  if ( isempty(path) ) return NULL;

  const char * start = path;
  const char * end   = start + strlen(path) - 1;

  /* This removes all trailing '/' characters */
  while ( end != start && *end == '/' ) end--;

  /* Shift back until next / is found */
  while ( end != start && *end != '/') end--;

  /* end is character before / (if there is one) */
  if ( end != start ) end--;

  char * dir;
  if ( end == start && *start != '/' ) {
    dir = strdup(".");
  } else {
    size_t len = end - start + 1;
    dir = strndup( start, len );
  }

  return dir;
}

/**
 * @brief Check if a path is readable
 *
 * Checks if a path is readable. The path can be either a file or a
 * directory. Firstly the path is checked using @c stat(2). If the
 * path exists it is then checked for genuine readability. For a
 * directory the read access is checked by calling @c opendir(3) and
 * for files the read access is checked using @c open(2).
 *
 * Note that this does NOT guarantee the path will still be readable
 * when any subsequent attempt is made to open the path for reading,
 * it may have been deleted or had the access mode changed.
 *
 * @param[in] path Pointer to string for path to be checked
 *
 * @return boolean which indicates if path is readable
 *
 */

bool lcfgutils_file_readable( const char * path ) {

  assert( path != NULL );

  struct stat sb;
  if ( stat( path, &sb ) != 0 )
    return false;

  bool is_readable = false;
  if ( S_ISDIR(sb.st_mode) ) {
    DIR * dh = opendir(path);
    if ( dh != NULL ) {
      is_readable = true;
      closedir(dh);
    }
  } else {
    int fd = open( path, O_RDONLY );
    if ( fd != -1 ) {
      is_readable = true;
      close(fd);
    }
  }

  return is_readable;
}

/**
 * @brief Compare contents of two files for any differences
 *
 * This can be used to compare the contents of two files in the
 * situation where a 'new' file is available to replace a 'current'
 * file. It will immediately return true if the current file does not
 * exist or if the files have different sizes. Otherwise the contents
 * of the files will be compared one character at a time until a
 * difference is found or the end of the files is reached.
 *
 * @param[in] cur_file Path to the current file
 * @param[in] new_file Path to the new file
 *
 * @return Boolean which indicates if contents of files differ
 *
 */

bool lcfgutils_file_needs_update( const char * cur_file,
                                  const char * new_file ) {

  assert( cur_file != NULL );
  assert( new_file != NULL );

  struct stat sb;
  if ( ( stat( cur_file, &sb ) != 0 ) || !S_ISREG(sb.st_mode) )
    return true;

  /* open files and compare sizes first. If anything fails then just
     request the file is updated in the hope that will fix things. */

  /* Declare here before using goto */
  bool needs_update = false;
  FILE * fh1 = NULL;
  FILE * fh2 = NULL;

  fh1 = fopen( cur_file, "r" );
  if ( fh1 == NULL ) {
    needs_update = true;
    goto cleanup;
  }

  fseek(fh1, 0, SEEK_END);
  long size1 = ftell(fh1);
  rewind(fh1);

  fh2 = fopen( new_file, "r" );
  if ( fh2 == NULL ) {
    needs_update = false;
    goto cleanup;
  }

  fseek(fh2, 0, SEEK_END);
  long size2 = ftell(fh2);
  rewind(fh2);

  if (size1 != size2) {
    needs_update = true;
    goto cleanup;
  }

  /* Only if sizes are same do we bother comparing bytes */

  char tmp1, tmp2;
  long i;
  for ( i=0; i < size1; i++ ) {
    size_t s1 = fread( &tmp1, sizeof(char), 1, fh1 );
    size_t s2 = fread( &tmp2, sizeof(char), 1, fh2 );
    if ( s1 != s2 || tmp1 != tmp2 ) {
      needs_update = true;
      break;
    }
  }

 cleanup:

  if ( fh1 != NULL )
    (void) fclose(fh1);

  if ( fh2 != NULL )
    (void) fclose(fh2);

  return needs_update;
}

void lcfgutils_build_message( char ** strp, const char *fmt, ... ) {
  free( *strp );
  *strp = NULL;

  va_list ap;
  va_start( ap, fmt );

  int rc = vasprintf( strp, fmt, ap );

  va_end(ap);

  if ( rc < 0 ) {
    perror( "Failed to build error string" );
    exit(EXIT_FAILURE);
  }
}

char * lcfgutils_string_replace( const char * input,
				 const char * match,
				 const char * replace ) {
  assert( input != NULL );
  assert( match != NULL );

  size_t input_len   = strlen(input);
  size_t match_len   = strlen(match);
  size_t replace_len = replace != NULL ? strlen(replace) : 0;

  ssize_t len_diff = replace_len - match_len;

  size_t new_len = input_len;

  const char * p = input;
  while ( ( p = strstr( p, match ) ) != NULL ) {
      p += match_len;
      new_len += len_diff;
  }

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate memory for LCFG resource type string");
    exit(EXIT_FAILURE);
  }

  char * to = result;

  const char * start = input;
  bool done = false;
  while ( !done ) {
    p = strstr( start, match );
    if ( p == NULL ) {
      to = stpcpy( to, start );
      done = true;
    } else {
      to = stpncpy( to, start, p - start );
      start = p + match_len;

      if ( replace_len > 0 )
	to = stpncpy( to, replace, replace_len );
    }
  }

  return result;
}

/**
 * @brief Calculate the hash for a set of strings
 *
 * This will calculate the hash for a set of strings using the djb2
 * algorithm created by Daniel Bernstein.
 *
 * This function supports the passing of multiple strings. There must
 * be at least one string and there must be a final @c NULL value to
 * indicate the end of the list of strings.
 *
 * @param[in] str Pointer to string to be hashed
 *
 * @return The hash for the strings
 *
 */

unsigned long lcfgutils_string_djbhash( const char * str, ... ) {

  unsigned long hash = 5381;

  const char * p;
  for ( p = str; *p != '\0'; p++ )
    hash = ( ( hash << 5 ) + hash ) + ( *p );

  va_list ap;
  va_start( ap, str );

  const char * extra_str = NULL;
  while ( ( extra_str = va_arg( ap, const char *) ) != NULL ) {

    for ( p = extra_str; *p != '\0'; p++ )
      hash = ( ( hash << 5 ) + hash ) + ( *p );

  }

  va_end(ap);

  return hash;
}
/* eof */
