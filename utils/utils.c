#include <ctype.h>
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

#include "utils.h"

/**
 * @brief Generate a safe temporary file name
 *
 * Given a target file name this will generate a safe temporary file
 * path in the same directory which is suitable for use with
 * mkstemp. The particular advantage of being in the same directory as
 * the target file is that it can always be renamed atomically.
 *
 * If the target file path is @c NULL then this will return @c NULL.
 * If the memory allocation for the new string is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * @param[in] path Pointer to target file name
 *
 * @return Pointer to a new string (call @c free(3) when no longer required)
 * 
 */

char * lcfgutils_safe_tmpfile( const char * path ) {

  if ( path == NULL ) {
    perror("Cannot generate a temporary file name from a NULL");
    exit(EXIT_FAILURE);
  }

  size_t base_len = 0;
  char * match = strrchr( path, '/' );
  if ( match != NULL ) {
    base_len = match - path + 1;
  }

  static const char * tmplname = ".lcfg.XXXXXX";
  size_t tmpl_len = strlen(tmplname);

  size_t new_len = base_len + tmpl_len;

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate memory for temporary file name");
    exit(EXIT_FAILURE);
  }

  char * to = result;

  if ( base_len > 0 )
    to = stpncpy( to, path, base_len );

  to = stpncpy( to, tmplname, tmpl_len );

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

char * lcfgutils_join_strings( const char * sep,
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

void lcfgutils_chomp( char * str ) {

  if ( str == NULL || *str == '\0' )
    return;

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

void lcfgutils_trim_whitespace( char * str ) {

  if ( str == NULL )
    return;

  size_t len = strlen(str);
  if ( len == 0 )
    return;

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

  if ( file == NULL )
    return NULL;

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

bool lcfgutils_endswith( const char * str, const char * suffix ) {

  if ( str == NULL || suffix == NULL )
    return false;

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

  if ( path == NULL )
    return NULL;

  char * start = (char *) path;
  char * end   = start + strlen(path) - 1;

  /* This removes all trailing '/' characters */
  while ( end != start && *end == '/' ) end--;

  /* Find the start of the base which is after the final '/' (if any) */
  if ( start != end ) {

    char * ptr = end;
    while ( ptr != start && *ptr != '/') ptr--;

    start = *ptr == '/' ? ptr + 1 : ptr;
  }

  size_t len = end - start + 1;

  if ( suffix != NULL && lcfgutils_endswith( start, suffix ) ) {
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

  if ( path == NULL )
    return NULL;

  char * start = (char *) path;
  char * end   = start + strlen(path) - 1;

  /* This removes all trailing '/' characters */
  while ( end != start && *end == '/' ) end--;

  /* Shift back until next / is found */
  while ( end != start && *end != '/') end--;

  /* end is character before / (if there is one) */
  if ( end != start )
    end--;

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

/* eof */
