#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#include "utils.h"

/* Given a path this will generate a temporary file path in the same
   directory which is suitable for use with mkstemp. The particular
   advantage of being in the same directory as the target file is that
   it can always be renamed atomically. */

char * lcfgutils_safe_tmpfile( const char * path ) {

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

/* Join two strings together with an optional separator */

char * lcfgutils_join_strings( const char * str1,
                               const char * str2,
                               const char * sep ) {

  size_t str1_len = strlen(str1);
  size_t str2_len = str2 != NULL ? strlen(str2) : 0;
  size_t sep_len  = sep  != NULL ? strlen(sep)  : 0;

  size_t new_len = str1_len + sep_len + str2_len;

  char * result = calloc( (new_len + 1), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate memory for new string");
    exit(EXIT_FAILURE);
  }

  char * to = result;

  to = stpncpy( to, str1, str1_len );

  if ( str2 != NULL ) {
    if ( sep_len != 0 )
      to = stpncpy( to, sep, sep_len );

    to = stpncpy( to, str2, str2_len );
  }

  *to = '\0';

  assert( ( result + new_len ) == to );

  return result;
}

/* trim any trailing newline characters, note this works * in place *,
   it does NOT make a new string */

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

/* remove any leading or trailing whitespace in a string, note this
   works * in place *, it does NOT make a new string */

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

bool lcfgutils_endswith( const char * str, const char * suffix ) {

  size_t str_len    = strlen(str);
  size_t suffix_len = strlen(suffix);

  return ( str_len >= suffix_len &&
           strncmp( str + str_len - suffix_len, suffix, suffix_len ) == 0 );
}

char * lcfgutils_basename( const char * path, const char * suffix ) {

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

char * lcfgutils_dirname( const char * path) {

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

bool lcfgutils_file_readable( const char * path ) {

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
    FILE * fh = fopen( path, "r" );
    if ( fh != NULL ) {
      is_readable = true;
      fclose(fh);
    }
  }

  return is_readable;
}

/* eof */
