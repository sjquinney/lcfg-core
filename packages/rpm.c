/**
 * @file packages/rpm.c
 * @brief Functions for working with RPMs
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#define _GNU_SOURCE   /* asprintf */

#include <stdarg.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <utime.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include "packages.h"
#include "utils.h"

static const char * rpm_file_suffix = ".rpm";
static size_t rpm_file_suffix_len = 4;

static LCFGStatus invalid_rpm( char ** msg, const char * base, ... ) {

  const char * fmt = "Invalid RPM (%s)";

  va_list ap;
  va_start( ap, base );

  char * reason = NULL;
  int rc = vasprintf( &reason, base, ap );
  va_end(ap);

  if ( rc < 0 ) {
    perror("Failed to allocate memory for error string");
    exit(EXIT_FAILURE);
  }

  lcfgutils_build_message( msg, fmt, reason );
  free(reason);

  return LCFG_STATUS_ERROR;
}

static bool file_needs_update( const char * cur_file,
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
    needs_update = true;
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
    fclose(fh1);

  if ( fh2 != NULL )
    fclose(fh2);

  return needs_update;
}

/**
 * @brief Create a new package from an RPM filename
 *
 * This parses an RPM filename into the constituent parts: @e name,
 * @e version, @e release and @e architecture and creates a new
 * @c LCFGPackage. Any leading directories in the string will be ignored.
 *
 * To avoid memory leaks, when the newly created package structure is
 * no longer required you should call the @c lcfgpackage_relinquish()
 * function.
 *
 * @param[in] input The RPM filename string.
 * @param[out] result Reference to the pointer for the @c LCFGPackage.
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgpackage_from_rpm_filename( const char * input,
                                          LCFGPackage ** result,
                                          char ** msg ) {

  *result = NULL;

  if ( isempty(input) )
    return invalid_rpm( msg, "bad filename" );

  /* Ignore any leading directories */

  const char * dirsep = strrchr( input, '/' );
  const char * filename = dirsep != NULL ? dirsep + 1 : input;

  size_t filename_len = strlen(filename);
  if ( filename_len <= rpm_file_suffix_len )
    return invalid_rpm( msg, "bad filename" );

  /* Check the suffix */

  if ( !lcfgutils_string_endswith( filename, rpm_file_suffix ) )
    return invalid_rpm( msg, "lacks '%s' suffix", rpm_file_suffix );

  /* Results - note that the file name has to be split apart backwards */

  bool ok = true;
  *result = lcfgpackage_new();

  size_t offset = filename_len - rpm_file_suffix_len - 1;

  /* Architecture - search backwards for '.' */

  char * pkg_arch = NULL;

  unsigned int i;
  for ( i=offset; i--; ) {
    if ( filename[i] == '.' ) {
      pkg_arch = strndup( filename + i + 1, offset - i );
      break;
    }
  }

  if ( pkg_arch == NULL ) {
    invalid_rpm( msg, "failed to extract architecture" );
    ok = false;
    goto failure;
  } else {
    ok = lcfgpackage_set_arch( *result, pkg_arch );
    if ( !ok ) {
      invalid_rpm( msg, "bad package architecture '%s'", pkg_arch );
      free(pkg_arch);
      pkg_arch = NULL;
      goto failure;
    }
  }

  /* Release field - search backwards for '-' */

  char * pkg_release = NULL;

  if ( i > 0 ) {
    offset = i - 1;

    for ( i=offset; i--; ) {
      if ( filename[i] == '-' ) {
        pkg_release = strndup( filename + i + 1, offset - i );
        break;
      }
    }

  }

  if ( pkg_release == NULL ) {
    invalid_rpm( msg, "failed to extract release" );
    ok = false;
    goto failure;
  } else {
    ok = lcfgpackage_set_release( *result, pkg_release );
    if ( !ok ) {
      invalid_rpm( msg, "bad release '%s'", pkg_release );
      free(pkg_release);
      pkg_release = NULL;
      goto failure;
    }
  }

  /* Version field - search backwards for '-' */

  char * pkg_version = NULL;

  if ( i > 0 ) {
    offset = i - 1;

    for ( i=offset; i--; ) {
      if ( filename[i] == '-' ) {
        pkg_version = strndup( filename + i + 1, offset - i );
        break;
      }
    }

  }

  if ( pkg_version == NULL ) {
    invalid_rpm( msg, "failed to extract version" );
    ok = false;
    goto failure;
  } else {
    ok = lcfgpackage_set_version( *result, pkg_version );
    if ( !ok ) {
      invalid_rpm( msg, "bad version '%s'", pkg_version );
      free(pkg_version);
      pkg_version = NULL;
      goto failure;
    }
  }

  /* Name field - everything else */

  char * pkg_name = NULL;
  if ( i > 0 ) {
    offset = i - 1;

    pkg_name = strndup( filename, offset + 1 );
  }

  if ( pkg_name == NULL ) {
    invalid_rpm( msg, "failed to find name" );
    ok = false;
    goto failure;
  } else {
    ok = lcfgpackage_set_name( *result, pkg_name );
    if ( !ok ) {
      invalid_rpm( msg, "bad name '%s'", pkg_name );
      free(pkg_name);
      pkg_name = NULL;
      goto failure;
    }
  }

  /* Finishing off */

  if ( !ok ) {

  failure:
    ok = false; /* in case we've jumped in from elsewhere */

    if ( *msg == NULL )
      invalid_rpm( msg, "bad filename" );

    lcfgpackage_relinquish(*result);
    *result = NULL;

  }

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/**
 * @brief Format the package as an RPM filename.
 *
 * Generates a new RPM filename based on the @c LCFGPackage in the
 * standard @c name-version-release.arch.rpm format.
 *
 * The following options are supported:
 *   - @c LCFG_OPT_NEWLINE - Add a newline at the end of the string.
 *
 * Note that the filename must contain a value for each field. If the
 * package is missing a name, version or release an error will
 * occur. If the package does not have an architecture specified and
 * the default architecture is set to @c NULL then the value as
 * returned by the @c default_architecture() function will be used.
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many package strings, this can be a
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
 * longer required.
 *
 * @param[in] pkg Pointer to @c LCFGPackage
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgpackage_to_rpm_filename( LCFG_PKG_TOSTR_ARGS ) {
  assert( pkg != NULL );

  /* Name, version, release and architecture are required */

  if ( !lcfgpackage_is_valid(pkg)    ||
       !lcfgpackage_has_version(pkg) ||
       !lcfgpackage_has_release(pkg) )
    return -1;

  const char * pkgnam  = lcfgpackage_get_name(pkg);
  size_t pkgnamlen    = strlen(pkgnam);

  const char * pkgver  = lcfgpackage_get_version(pkg);
  size_t pkgverlen    = strlen(pkgver);

  const char * pkgrel  = lcfgpackage_get_release(pkg);
  size_t pkgrellen    = strlen(pkgrel);

  const char * pkgarch;
  if ( lcfgpackage_has_arch(pkg) ) {
    pkgarch = lcfgpackage_get_arch(pkg);
  } else if ( defarch != NULL ) {
    pkgarch = defarch;
  } else {
    pkgarch = default_architecture();
  }

  size_t pkgarchlen = strlen(pkgarch);

  /* +3 for the two '-' separators and one '.' */
  size_t new_len = ( pkgnamlen  +
                     pkgverlen  +
                     pkgrellen  +
                     pkgarchlen +
		     rpm_file_suffix_len + 3 );

  if ( options&LCFG_OPT_NEWLINE )
    new_len += 1;

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    *size = new_len + 1;

    *result = realloc( *result, ( *size * sizeof(char) ) );
    if ( *result == NULL ) {
      perror("Failed to allocate memory for LCFG resource string");
      exit(EXIT_FAILURE);
    }

  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  /* name */
  to = stpncpy( to, pkgnam, pkgnamlen );

  *to = '-';
  to++;

  /* version */
  to = stpncpy( to, pkgver, pkgverlen );

  *to = '-';
  to++;

  /* release */
  to = stpncpy( to, pkgrel, pkgrellen );

  *to = '.';
  to++;

  /* arch */
  to = stpncpy( to, pkgarch, pkgarchlen );

  /* suffix */
  to = stpncpy( to, rpm_file_suffix, rpm_file_suffix_len );

  if ( options&LCFG_OPT_NEWLINE )
    to = stpncpy( to, "\n", 1 );

  *to = '\0';

  assert( ( *result + new_len ) == to );

  return new_len;
}

/**
 * @brief Write a package list to an RPM file
 *
 * This can be used to create an rpmlist file which is used as input
 * by the updaterpms tool.
 *
 * Each package in the list is formatted as an RPM filename using the
 * @c lcfgpackage_to_rpm_filename() function.
 *
 * If the default architecture is not specified (i.e. it is set to 
 * @c NULL) then the value returned by the @c default_architecture
 * function will be used.
 *
 * The package list will be sorted so that the file is generated
 * consistently.
 *
 * The file is securely created using a temporary file and it is only
 * renamed to the target name if the contents differ. If the
 * modification time is specified (i.e. non-zero) the mtime for the
 * file will always be updated. If the package list is empty then an
 * empty file will be created.
 *
 * @param[in] pkglist Pointer to @c LCFGPackageList
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] base String to be prepended to all package strings
 * @param[in] filename Path file to be created
 * @param[in] mtime Modification time to set on file (or zero)
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgpkglist_to_rpmlist( LCFGPackageList * pkglist,
                                   const char * defarch,
                                   const char * base,
                                   const char * filename,
                                   time_t mtime,
                                   char ** msg ) {
  assert( pkglist  != NULL );
  assert( filename != NULL );

  char * tmpfile = lcfgutils_safe_tmpfile(filename);

  bool ok = true;
  LCFGChange change = LCFG_CHANGE_ERROR;

  FILE * tmpfh = NULL;
  int tmpfd = mkstemp(tmpfile);
  if ( tmpfd >= 0 )
    tmpfh = fdopen( tmpfd, "w" );

  if ( tmpfh == NULL ) {
    if ( tmpfd >= 0 ) close(tmpfd);

    lcfgutils_build_message( msg, "Failed to open temporary rpmlist file" );
    ok = false;
    goto cleanup;
  }

  /* For efficiency, ensure we have a default architecture */
  if ( defarch == NULL )
    defarch = default_architecture();

  /* The sort is not just cosmetic - there needs to be a deterministic
     order so that we can compare the RPM list for changes using cmp */

  lcfgpkglist_sort(pkglist);

  ok = lcfgpkglist_print( pkglist,
                          defarch, base,
                          LCFG_PKG_STYLE_RPM,
                          LCFG_OPT_NEWLINE,
                          tmpfh );

  if (!ok) {
    lcfgutils_build_message( msg, "Failed to write rpmlist file" );
    goto cleanup;
  } else {

    if ( fclose(tmpfh) == 0 ) {
      tmpfh = NULL; /* Avoids a further attempt to close in cleanup */
    } else {
      lcfgutils_build_message( msg, "Failed to close rpmlist file" );
      ok = false;
    }

  }

  /* rename tmpfile to target if different */

  if ( ok ) {

    if ( file_needs_update( filename, tmpfile ) ) {

      if ( rename( tmpfile, filename ) == 0 ) {
        change = LCFG_CHANGE_MODIFIED;
      } else {
        ok = false;
      }

    } else {
      change = LCFG_CHANGE_NONE;
    }

  }

  /* Even when the file is not changed the mtime might need to be updated */

  if ( ok && mtime != 0 ) {
    struct utimbuf times;
    times.actime  = mtime;
    times.modtime = mtime;
    utime( filename, &times );
  }

 cleanup:

  /* This might have already gone but call fclose and unlink to ensure
     tidiness. Do not care about the result */

  if ( tmpfh != NULL )
    fclose(tmpfh);

  if ( tmpfile != NULL ) {
    unlink(tmpfile);
    free(tmpfile);
  }

  return ( ok ? change : LCFG_CHANGE_ERROR );
}

/**
 * @brief Write a package set to an RPM file
 *
 * This can be used to create an rpmlist file which is used as input
 * by the updaterpms tool.
 *
 * Each package in the set is formatted as an RPM filename using the
 * @c lcfgpackage_to_rpm_filename() function.
 *
 * If the default architecture is not specified (i.e. it is set to 
 * @c NULL) then the value returned by the @c default_architecture
 * function will be used.
 *
 * The package set will be sorted so that the file is generated
 * consistently.
 *
 * The file is securely created using a temporary file and it is only
 * renamed to the target name if the contents differ. If the
 * modification time is specified (i.e. non-zero) the mtime for the
 * file will always be updated. If the package list is empty then an
 * empty file will be created.
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] base String to be prepended to all package strings
 * @param[in] filename Path file to be created
 * @param[in] mtime Modification time to set on file (or zero)
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgpkgset_to_rpmlist( LCFGPackageSet * pkgset,
                                  const char * defarch,
                                  const char * base,
                                  const char * filename,
                                  time_t mtime,
                                  char ** msg ) {
  assert( pkgset  != NULL );
  assert( filename != NULL );

  char * tmpfile = lcfgutils_safe_tmpfile(filename);

  bool ok = true;
  LCFGChange change = LCFG_CHANGE_ERROR;

  FILE * tmpfh = NULL;
  int tmpfd = mkstemp(tmpfile);
  if ( tmpfd >= 0 )
    tmpfh = fdopen( tmpfd, "w" );

  if ( tmpfh == NULL ) {
    if ( tmpfd >= 0 ) close(tmpfd);

    lcfgutils_build_message( msg, "Failed to open temporary rpmlist file" );
    ok = false;
    goto cleanup;
  }

  /* For efficiency, ensure we have a default architecture */
  if ( defarch == NULL )
    defarch = default_architecture();

  ok = lcfgpkgset_print( pkgset,
                         defarch,
                         base,
                         LCFG_PKG_STYLE_RPM,
                         LCFG_OPT_NEWLINE,
                         tmpfh );

  if (!ok) {
    lcfgutils_build_message( msg, "Failed to write rpmlist file" );
    goto cleanup;
  } else {

    if ( fclose(tmpfh) == 0 ) {
      tmpfh = NULL; /* Avoids a further attempt to close in cleanup */
    } else {
      lcfgutils_build_message( msg, "Failed to close rpmlist file" );
      ok = false;
    }

  }

  /* rename tmpfile to target if different */

  if ( ok ) {

    if ( file_needs_update( filename, tmpfile ) ) {

      if ( rename( tmpfile, filename ) == 0 ) {
        change = LCFG_CHANGE_MODIFIED;
      } else {
        ok = false;
      }

    } else {
      change = LCFG_CHANGE_NONE;
    }

  }

  /* Even when the file is not changed the mtime might need to be updated */

  if ( ok && mtime != 0 ) {
    struct utimbuf times;
    times.actime  = mtime;
    times.modtime = mtime;
    utime( filename, &times );
  }

 cleanup:

  /* This might have already gone but call fclose and unlink to ensure
     tidiness. Do not care about the result */

  if ( tmpfh != NULL )
    fclose(tmpfh);

  if ( tmpfile != NULL ) {
    unlink(tmpfile);
    free(tmpfile);
  }

  return ( ok ? change : LCFG_CHANGE_ERROR );
}

/**
 * @brief Read package list from RPM directory
 *
 * This reads the contents of a directory and generates a new
 * @c LCFGPackageList using the names of files with the @c .rpm
 * suffix. Note that it does not make any attempt to inspect the
 * contents of the files to ensure that they really are valid
 * RPMs. Dot files (starting with a '.') and any other files without
 * the @c .rpm suffix will be ignored.
 *
 * An error will be returned if the directory does not exist or is
 * inaccessible.
 *
 * To avoid memory leaks, when the package list is no longer required
 * the @c lcfgpkglist_relinquish() function should be called.
 *
 * @param[in] rpmdir Path to RPM directory
 * @param[out] result Reference to pointer for new @c LCFGPackageList
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgpkglist_from_rpm_dir( const char * rpmdir,
                                     LCFGPackageList ** result,
                                     char ** msg ) {

  *result = NULL;

  if ( isempty(rpmdir) ) {
    lcfgutils_build_message( msg, "Invalid RPM directory" );
    return LCFG_STATUS_ERROR;
  }

  DIR * dir;
  if ( ( dir = opendir(rpmdir) ) == NULL ) {

    if (errno == ENOENT) {
      lcfgutils_build_message( msg, "Directory does not exist" );
    } else {
      lcfgutils_build_message( msg, "Directory is not readable" );
    }

    return LCFG_STATUS_ERROR;
  }

  /* Results */

  bool ok = true;
  *result = lcfgpkglist_new();
  ok = lcfgpkglist_set_merge_rules( *result, LCFG_MERGE_RULE_KEEP_ALL );

  /* Scan the directory for any non-hidden files with .rpm suffix */

  struct dirent * dp;
  struct stat sb;

  while ( ok && ( dp = readdir(dir) ) != NULL ) {

    char * filename = dp->d_name;
    if ( *filename == '.' ) continue;

    /* Just ignore anything which does not have a .rpm suffix */
    if ( !lcfgutils_string_endswith( filename, rpm_file_suffix ) ) continue;

    /* Only interested in files */
    char * fullpath  = lcfgutils_catfile( rpmdir, filename );
    if ( stat( fullpath, &sb ) == 0 && S_ISREG(sb.st_mode) ) {

      LCFGPackage * pkg = NULL;
      char * parse_msg = NULL;
      LCFGStatus parse_rc = lcfgpackage_from_rpm_filename( filename, &pkg,
                                                           &parse_msg );

      if ( parse_rc == LCFG_STATUS_ERROR ) {

        lcfgutils_build_message( msg, "Failed to parse '%s': %s", filename,
                  ( parse_msg != NULL ? parse_msg : "unknown error" ) );

      } else {

        char * merge_msg = NULL;
        if ( lcfgpkglist_merge_package( *result, pkg, &merge_msg )
             == LCFG_CHANGE_ERROR ) {
          ok = false;
          *msg = lcfgpackage_build_message( pkg, "Merge failure: %s",
                         ( merge_msg != NULL ? merge_msg : "unknown error" ) );
        }
        free(merge_msg);
      }

      free(parse_msg);

      lcfgpackage_relinquish(pkg);

    }

    free(fullpath);

  }

  closedir(dir);

  /* Finishing off */

  if ( !ok ) {

    if ( *msg == NULL )
      lcfgutils_build_message( msg, "Failed to read RPM directory" );

    if ( *result != NULL ) {
      lcfgpkglist_destroy(*result);
      *result = NULL;
    }
  }

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/**
 * @brief Read package set from RPM directory
 *
 * This reads the contents of a directory and generates a new
 * @c LCFGPackageSet using the names of files with the @c .rpm
 * suffix. Note that it does not make any attempt to inspect the
 * contents of the files to ensure that they really are valid
 * RPMs. Dot files (starting with a '.') and any other files without
 * the @c .rpm suffix will be ignored.
 *
 * An error will be returned if the directory does not exist or is
 * inaccessible.
 *
 * To avoid memory leaks, when the package list is no longer required
 * the @c lcfgpkgset_relinquish() function should be called.
 *
 * @param[in] rpmdir Path to RPM directory
 * @param[out] result Reference to pointer for new @c LCFGPackageSet
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgpkgset_from_rpm_dir( const char * rpmdir,
                                    LCFGPackageSet ** result,
                                    char ** msg ) {

  *result = NULL;

  if ( isempty(rpmdir) ) {
    lcfgutils_build_message( msg, "Invalid RPM directory" );
    return LCFG_STATUS_ERROR;
  }

  DIR * dir;
  if ( ( dir = opendir(rpmdir) ) == NULL ) {

    if (errno == ENOENT) {
      lcfgutils_build_message( msg, "Directory does not exist" );
    } else {
      lcfgutils_build_message( msg, "Directory is not readable" );
    }

    return LCFG_STATUS_ERROR;
  }

  /* Results */

  bool ok = true;
  *result = lcfgpkgset_new();
  ok = lcfgpkgset_set_merge_rules( *result, LCFG_MERGE_RULE_KEEP_ALL );

  /* Scan the directory for any non-hidden files with .rpm suffix */

  struct dirent * dp;
  struct stat sb;

  while ( ok && ( dp = readdir(dir) ) != NULL ) {

    char * filename = dp->d_name;
    if ( *filename == '.' ) continue;

    /* Just ignore anything which does not have a .rpm suffix */
    if ( !lcfgutils_string_endswith( filename, rpm_file_suffix ) ) continue;

    /* Only interested in files */
    char * fullpath  = lcfgutils_catfile( rpmdir, filename );
    if ( stat( fullpath, &sb ) == 0 && S_ISREG(sb.st_mode) ) {

      LCFGPackage * pkg = NULL;
      char * parse_msg = NULL;
      LCFGStatus parse_rc = lcfgpackage_from_rpm_filename( filename, &pkg,
                                                           &parse_msg );

      if ( parse_rc == LCFG_STATUS_ERROR ) {

        lcfgutils_build_message( msg, "Failed to parse '%s': %s", filename,
                  ( parse_msg != NULL ? parse_msg : "unknown error" ) );

      } else {

        char * merge_msg = NULL;
        if ( lcfgpkgset_merge_package( *result, pkg, &merge_msg )
             == LCFG_CHANGE_ERROR ) {
          ok = false;
          *msg = lcfgpackage_build_message( pkg, "Merge failure: %s",
                         ( merge_msg != NULL ? merge_msg : "unknown error" ) );
        }
        free(merge_msg);
      }

      free(parse_msg);

      lcfgpackage_relinquish(pkg);

    }

    free(fullpath);

  }

  closedir(dir);

  /* Finishing off */

  if ( !ok ) {

    if ( *msg == NULL )
      lcfgutils_build_message( msg, "Failed to read RPM directory" );

    if ( *result != NULL ) {
      lcfgpkgset_destroy(*result);
      *result = NULL;
    }
  }

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/**
 * @brief Read package list from file
 *
 * This reads the contents of an rpmlist file and generates a new
 * @c LCFGPackageList. Blank lines and those beginning with a '#' (hash)
 * character will be ignored. All other lines will be parsed as RPM
 * filenames using the @c lcfgpackage_from_rpm_filename()
 * function. The RPM filenames may have leading directories, they will
 * be ignored.
 *
 * An error is returned if the file does not exist unless the
 * @c LCFG_OPT_ALLOW_NOEXIST option is specified. If the file exists
 * but is empty then an empty @c LCFGPackageList is returned.
 *
 * If the @c LCFG_OPT_USE_META option is specified then derivation
 * information will be set for each package which is based on the
 * filename and line number.
 *
 * To avoid memory leaks, when the package list is no longer required
 * the @c lcfgpkglist_relinquish() function should be called.
 *
 * @param[in] filename Path to rpmlist file
 * @param[out] result Reference to pointer for new @c LCFGPackageList
 * @param[in] options Controls the behaviour of the process.
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgpkglist_from_rpmlist( const char * filename,
                                     LCFGPackageList ** result,
                                     LCFGOption options,
                                     char ** msg ) {

  *result = NULL;

  if ( isempty(filename) ) {
    lcfgutils_build_message( msg, "Invalid filename" );
    return LCFG_STATUS_ERROR;
  }

  FILE * fp;
  if ( (fp = fopen(filename, "r")) == NULL ) {
    LCFGStatus status = LCFG_STATUS_ERROR;

    if (errno == ENOENT) {
      if ( options&LCFG_OPT_ALLOW_NOEXIST ) {
        /* No file so just create an empty list */
        *result = lcfgpkglist_new();
      } else {
        lcfgutils_build_message( msg, "File does not exist" );
      }
    } else {
      lcfgutils_build_message( msg, "File is not readable" );
    }

    return status;
  }

  /* Setup the getline buffer */

  size_t line_len = 128;
  char * line = malloc( line_len * sizeof(char) );
  if ( line == NULL ) {
    perror( "Failed to allocate memory whilst processing RPM list file" );
    exit(EXIT_FAILURE);
  }

  /* Results */

  bool include_meta = options & LCFG_OPT_USE_META;

  bool ok = true;

  *result = lcfgpkglist_new();
  ok = lcfgpkglist_set_merge_rules( *result,
                    LCFG_MERGE_RULE_SQUASH_IDENTICAL | LCFG_MERGE_RULE_KEEP_ALL );

  unsigned int linenum = 0;
  while( ok && getline( &line, &line_len, fp ) != -1 ) {
    linenum++;

    char * trimmed = strdup(line);
    lcfgutils_string_trim(trimmed);

    /* Ignore empty lines */
    if ( *trimmed == '\0' || *trimmed == '#' ) {
      free(trimmed);
      continue;
    }

    LCFGPackage * pkg = NULL;
    char * parse_errmsg = NULL;
    LCFGStatus parse_rc = lcfgpackage_from_rpm_filename( trimmed, &pkg,
                                                         &parse_errmsg );

    if ( parse_rc == LCFG_STATUS_ERROR ) {

      if ( parse_errmsg == NULL ) {
        lcfgutils_build_message( msg, "Error at line %u", linenum );
      } else {
        lcfgutils_build_message( msg, "Error at line %u: %s",
				 linenum, parse_errmsg );
        free(parse_errmsg);
      }

    } else {

      if ( include_meta ) {
        /* Simplest to use asprintf here since we have an unsigned int */
        char * derivation = NULL;
        int rc = asprintf( &derivation, "%s:%u", filename, linenum );
        if ( rc < 0 || derivation == NULL ) {
          perror( "Failed to build LCFG derivation string" );
          exit(EXIT_FAILURE);
        }

        /* Ignore any problem with setting the derivation */
        if ( !lcfgpackage_set_derivation( pkg, derivation ) )
          free(derivation);
      }

      char * merge_msg = NULL;
      LCFGChange merge_rc =
        lcfgpkglist_merge_package( *result, pkg, &merge_msg );

      if ( merge_rc == LCFG_CHANGE_ERROR ) {
        ok = false;
	lcfgutils_build_message( msg,
		  "Error at line %u: Merge failure: %s",
		  linenum, merge_msg );

      }
      free(merge_msg);

    }
    lcfgpackage_relinquish(pkg);

    free(trimmed);
  }
  fclose(fp);
  free(line);

  /* Finishing off */

  if ( !ok ) {

    if ( *msg == NULL )
      lcfgutils_build_message( msg, "Failed to parse RPM list file" );

    if ( *result != NULL ) {
      lcfgpkglist_destroy(*result);
      *result = NULL;
    }
  }

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/**
 * @brief Read package set from file
 *
 * This reads the contents of an rpmlist file and generates a new
 * @c LCFGPackageSet. Blank lines and those beginning with a '#' (hash)
 * character will be ignored. All other lines will be parsed as RPM
 * filenames using the @c lcfgpackage_from_rpm_filename()
 * function. The RPM filenames may have leading directories, they will
 * be ignored.
 *
 * An error is returned if the file does not exist unless the
 * @c LCFG_OPT_ALLOW_NOEXIST option is specified. If the file exists
 * but is empty then an empty @c LCFGPackageSet is returned.
 *
 * If the @c LCFG_OPT_USE_META option is specified then derivation
 * information will be set for each package which is based on the
 * filename and line number.
 *
 * To avoid memory leaks, when the package list is no longer required
 * the @c lcfgpkgset_relinquish() function should be called.
 *
 * @param[in] filename Path to rpmlist file
 * @param[out] result Reference to pointer for new @c LCFGPackageSet
 * @param[in] options Controls the behaviour of the process.
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgpkgset_from_rpmlist( const char * filename,
                                     LCFGPackageSet ** result,
                                     LCFGOption options,
                                     char ** msg ) {

  *result = NULL;

  if ( isempty(filename) ) {
    lcfgutils_build_message( msg, "Invalid filename" );
    return LCFG_STATUS_ERROR;
  }

  FILE * fp;
  if ( (fp = fopen(filename, "r")) == NULL ) {
    LCFGStatus status = LCFG_STATUS_ERROR;

    if (errno == ENOENT) {
      if ( options&LCFG_OPT_ALLOW_NOEXIST ) {
        /* No file so just create an empty list */
        *result = lcfgpkgset_new();
      } else {
        lcfgutils_build_message( msg, "File does not exist" );
      }
    } else {
      lcfgutils_build_message( msg, "File is not readable" );
    }

    return status;
  }

  /* Setup the getline buffer */

  size_t line_len = 128;
  char * line = malloc( line_len * sizeof(char) );
  if ( line == NULL ) {
    perror( "Failed to allocate memory whilst processing RPM list file" );
    exit(EXIT_FAILURE);
  }

  /* Results */

  bool include_meta = options & LCFG_OPT_USE_META;

  bool ok = true;

  *result = lcfgpkgset_new();
  ok = lcfgpkgset_set_merge_rules( *result,
                    LCFG_MERGE_RULE_SQUASH_IDENTICAL | LCFG_MERGE_RULE_KEEP_ALL );

  unsigned int linenum = 0;
  while( ok && getline( &line, &line_len, fp ) != -1 ) {
    linenum++;

    char * trimmed = strdup(line);
    lcfgutils_string_trim(trimmed);

    /* Ignore empty lines */
    if ( *trimmed == '\0' || *trimmed == '#' ) {
      free(trimmed);
      continue;
    }

    LCFGPackage * pkg = NULL;
    char * parse_errmsg = NULL;
    LCFGStatus parse_rc = lcfgpackage_from_rpm_filename( trimmed, &pkg,
                                                         &parse_errmsg );

    if ( parse_rc == LCFG_STATUS_ERROR ) {

      if ( parse_errmsg == NULL ) {
        lcfgutils_build_message( msg, "Error at line %u", linenum );
      } else {
        lcfgutils_build_message( msg, "Error at line %u: %s",
				 linenum, parse_errmsg );
        free(parse_errmsg);
      }

    } else {

      if ( include_meta ) {
        /* Simplest to use asprintf here since we have an unsigned int */
        char * derivation = NULL;
        int rc = asprintf( &derivation, "%s:%u", filename, linenum );
        if ( rc < 0 || derivation == NULL ) {
          perror( "Failed to build LCFG derivation string" );
          exit(EXIT_FAILURE);
        }

        /* Ignore any problem with setting the derivation */
        if ( !lcfgpackage_set_derivation( pkg, derivation ) )
          free(derivation);
      }

      char * merge_msg = NULL;
      LCFGChange merge_rc =
        lcfgpkgset_merge_package( *result, pkg, &merge_msg );

      if ( merge_rc == LCFG_CHANGE_ERROR ) {
        ok = false;
	lcfgutils_build_message( msg,
		  "Error at line %u: Merge failure: %s",
		  linenum, merge_msg );

      }
      free(merge_msg);

    }
    lcfgpackage_relinquish(pkg);

    free(trimmed);
  }
  fclose(fp);
  free(line);

  /* Finishing off */

  if ( !ok ) {

    if ( *msg == NULL )
      lcfgutils_build_message( msg, "Failed to parse RPM list file" );

    if ( *result != NULL ) {
      lcfgpkgset_destroy(*result);
      *result = NULL;
    }
  }

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/**
 * @brief Write a package list to an rpmcfg file
 *
 * This can be used to create an rpmcfg file which is used as input by
 * the updaterpms tool. The file is intended to be passed through the
 * C Preprocessor (cpp), the packages are formatted using the
 * @c lcfgpackage_to_cpp() function. The packages which are @e active for
 * the current contexts are listed first. All other @e inactive
 * packages are listed afterwards in an @c ALL_CONTEXTS block. They
 * are used by the rpm cache stuff which needs to be able to cache all
 * the packages, regardless of context.
 *
 * The package list will be sorted so that the file is generated
 * consistently.
 *
 * The file is securely created using a temporary file and it is only
 * renamed to the target name if the contents differ. If the
 * modification time is specified (i.e. non-zero) the mtime for the
 * file will always be updated. If the package list is empty then an
 * empty file will be created.
 *
 * @param[in] active Pointer to active @c LCFGPackageList
 * @param[in] inactive Pointer to inactive @c LCFGPackageList
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] filename Path of file to be created
 * @param[in] rpminc Extra file to be included by cpp
 * @param[in] mtime Modification time to set on file (or zero)
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change for file
 *
 */

LCFGChange lcfgpkglist_to_rpmcfg( LCFGPackageList * active,
                                  LCFGPackageList * inactive,
                                  const char * defarch,
                                  const char * filename,
                                  const char * rpminc,
                                  time_t mtime,
                                  char ** msg ) {

  *msg = NULL;
  LCFGChange change = LCFG_CHANGE_ERROR;
  bool ok = true;

  char * tmpfile = lcfgutils_safe_tmpfile(filename);

  FILE * out = NULL;
  int tmpfd = mkstemp(tmpfile);
  if ( tmpfd >= 0 )
    out = fdopen( tmpfd, "w" );

  if ( out == NULL ) {
    if ( tmpfd >= 0 ) close(tmpfd);

    ok = false;
    lcfgutils_build_message( msg, "Failed to open temporary rpmcfg file");
    goto cleanup;
  }

  /* Many derivations are enormous so a large buffer is required */

  size_t buf_size = 7000;  
  char * buffer = calloc( buf_size, sizeof(char) );
  if ( buffer == NULL ) {
    perror( "Failed to allocate memory for LCFG package list buffer" );
    exit(EXIT_FAILURE);
  }

  /* The sort is not just cosmetic - there needs to be a deterministic
     order so that we can compare the RPM list for changes using cmp */

  if ( !lcfgslist_is_empty(active) ) {
    lcfgpkglist_sort(active);

    const LCFGSListNode * cur_node = NULL;
    for ( cur_node = lcfgslist_head(active);
	  cur_node != NULL && ok;
	  cur_node = lcfgslist_next(cur_node) ) {

      const LCFGPackage * pkg = lcfgslist_data(cur_node);

      if ( !lcfgpackage_is_valid(pkg) ) continue;

      ssize_t rc = lcfgpackage_to_cpp( pkg,
                                       defarch,
                                       LCFG_OPT_USE_META,
                                       &buffer, &buf_size );

      if ( rc > 0 ) {

        if ( fputs( buffer, out ) < 0 )
          ok = false;

      } else {
        ok = false;
      }

      if (!ok) {
        lcfgutils_build_message( msg, "Failed to write to rpmcfg file" );
        break;
      }

    }

  }

  /* List the RPMs that would be present in other contexts. This is
    used by the rpm cache stuff because we need to cache all the RPMs,
    regardless of context. */

  if ( fprintf( out, "#ifdef ALL_CONTEXTS\n" ) < 0 )
    ok = false;

  if ( ok && !lcfgslist_is_empty(inactive) ) {
    lcfgpkglist_sort(inactive);

    const LCFGSListNode * cur_node = NULL;
    for ( cur_node = lcfgslist_head(inactive);
	  cur_node != NULL && ok;
	  cur_node = lcfgslist_next(cur_node) ) {

      const LCFGPackage * pkg = lcfgslist_data(cur_node);

      if ( !lcfgpackage_is_valid(pkg) ) continue;

      ssize_t rc = lcfgpackage_to_cpp( pkg,
                                       defarch,
                                       LCFG_OPT_USE_META,
                                       &buffer, &buf_size );

      if ( rc > 0 ) {

        if ( fputs( buffer, out ) < 0 )
          ok = false;

      } else {
        ok = false;
      }

      if (!ok) {
        lcfgutils_build_message( msg, "Failed to write to rpmcfg file" );
        break;
      }

    }

  }

  free(buffer);

  if (ok) {
    if ( fprintf( out, "#endif\n\n" ) < 0 )
      ok = false;
  }

  if ( ok && rpminc != NULL ) {
    if ( fprintf( out, "#include \"%s\"\n", rpminc ) < 0 )
      ok = false;
  }

  /* Attempt to close the temporary file whatever happens */
  if ( fclose(out) != 0 ) {
    lcfgutils_build_message( msg, "Failed to close rpmcfg file" );
    ok = false;
  }

  /* rename tmpfile to target if different */

  if ( ok ) {

    if ( file_needs_update( filename, tmpfile ) ) {

      if ( rename( tmpfile, filename ) == 0 ) {
        change = LCFG_CHANGE_MODIFIED;
      } else {
        ok = false;
      }

    } else {
      change = LCFG_CHANGE_NONE;
    }

  }

  /* Even when the file is not changed the mtime might need to be updated */

  if ( ok && mtime != 0 ) {
    struct utimbuf times;
    times.actime  = mtime;
    times.modtime = mtime;
    utime( filename, &times );
  }

 cleanup:

  if ( tmpfile != NULL ) {
    unlink(tmpfile);
    free(tmpfile);
  }

  return ( ok ? change : LCFG_CHANGE_ERROR );
}

/**
 * @brief Write a package set to an rpmcfg file
 *
 * This can be used to create an rpmcfg file which is used as input by
 * the updaterpms tool. The file is intended to be passed through the
 * C Preprocessor (cpp), the packages are formatted using the
 * @c lcfgpackage_to_cpp() function. The packages which are @e active for
 * the current contexts are listed first. All other @e inactive
 * packages are listed afterwards in an @c ALL_CONTEXTS block. They
 * are used by the rpm cache stuff which needs to be able to cache all
 * the packages, regardless of context.
 *
 * The package set will be sorted so that the file is generated
 * consistently.
 *
 * The file is securely created using a temporary file and it is only
 * renamed to the target name if the contents differ. If the
 * modification time is specified (i.e. non-zero) the mtime for the
 * file will always be updated. If the package list is empty then an
 * empty file will be created.
 *
 * @param[in] active Pointer to active @c LCFGPackageSet
 * @param[in] inactive Pointer to inactive @c LCFGPackageSet
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] filename Path of file to be created
 * @param[in] rpminc Extra file to be included by cpp
 * @param[in] mtime Modification time to set on file (or zero)
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Integer value indicating type of change for file
 *
 */

LCFGChange lcfgpkgset_to_rpmcfg( LCFGPackageSet * active,
                                 LCFGPackageSet * inactive,
                                 const char * defarch,
                                 const char * filename,
                                 const char * rpminc,
                                 time_t mtime,
                                 char ** msg ) {

  *msg = NULL;
  LCFGChange change = LCFG_CHANGE_ERROR;
  bool ok = true;

  char * tmpfile = lcfgutils_safe_tmpfile(filename);

  FILE * out = NULL;
  int tmpfd = mkstemp(tmpfile);
  if ( tmpfd >= 0 )
    out = fdopen( tmpfd, "w" );

  if ( out == NULL ) {
    if ( tmpfd >= 0 ) close(tmpfd);

    ok = false;
    lcfgutils_build_message( msg, "Failed to open temporary rpmcfg file");
    goto cleanup;
  }

  /* The sort is not just cosmetic - there needs to be a deterministic
     order so that we can compare the RPM list for changes using cmp */

  if ( !lcfgpkgset_is_empty(active) ) {
    ok = lcfgpkgset_print( active, defarch, NULL,
                           LCFG_PKG_STYLE_CPP, LCFG_OPT_USE_META,
                           out );
  }

  /* List the RPMs that would be present in other contexts. This is
    used by the rpm cache stuff because we need to cache all the RPMs,
    regardless of context. */

  if ( fprintf( out, "#ifdef ALL_CONTEXTS\n" ) < 0 )
    ok = false;

  if ( ok && !lcfgpkgset_is_empty(inactive) ) {
    ok = lcfgpkgset_print( inactive, defarch, NULL,
                           LCFG_PKG_STYLE_CPP, LCFG_OPT_USE_META,
                           out );
  }

  if (ok) {
    if ( fprintf( out, "#endif\n\n" ) < 0 )
      ok = false;
  }

  if ( ok && rpminc != NULL ) {
    if ( fprintf( out, "#include \"%s\"\n", rpminc ) < 0 )
      ok = false;
  }

  /* Attempt to close the temporary file whatever happens */
  if ( fclose(out) != 0 ) {
    lcfgutils_build_message( msg, "Failed to close rpmcfg file" );
    ok = false;
  }

  /* rename tmpfile to target if different */

  if ( ok ) {

    if ( file_needs_update( filename, tmpfile ) ) {

      if ( rename( tmpfile, filename ) == 0 ) {
        change = LCFG_CHANGE_MODIFIED;
      } else {
        ok = false;
      }

    } else {
      change = LCFG_CHANGE_NONE;
    }

  }

  /* Even when the file is not changed the mtime might need to be updated */

  if ( ok && mtime != 0 ) {
    struct utimbuf times;
    times.actime  = mtime;
    times.modtime = mtime;
    utime( filename, &times );
  }

 cleanup:

  if ( tmpfile != NULL ) {
    unlink(tmpfile);
    free(tmpfile);
  }

  return ( ok ? change : LCFG_CHANGE_ERROR );
}

#ifdef HAVE_RPM

#include <rpm/rpmlib.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>

/**
 * @brief Read package set from RPM database
 *
 * This reads the contents of an RPM database and generates a new @c
 * LCFGPackageSet. This requires rpmlib and thus does not work on
 * platforms which do not provide that library.
 *
 * To avoid memory leaks, when the package list is no longer required
 * the @c lcfgpkgset_relinquish() function should be called.
 *
 * @param[in] rootdir Alternate root directory
 * @param[out] result Reference to pointer for new @c LCFGPackageSet
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgpkgset_from_rpm_db( const char * rootdir,
                                   LCFGPackageSet ** result,
                                   char ** msg ) {

  if ( rpmReadConfigFiles(NULL, NULL) == -1 ) {
     perror("Failed to read RPM config files");
     exit(EXIT_FAILURE);
  }

  rpmts ts = rpmtsCreate();

  if ( !isempty(rootdir) )
    rpmtsSetRootDir(ts, rootdir);

  rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);

  LCFGPackageSet * pkgs = lcfgpkgset_new();
  bool ok = lcfgpkgset_set_merge_rules( pkgs, LCFG_MERGE_RULE_SQUASH_IDENTICAL|LCFG_MERGE_RULE_KEEP_ALL );

  if ( !ok ) goto cleanup;

  if ( iter == NULL) {
    /* no packages installed */
  } else {

    Header hdr;
    while ( ok && (hdr = rpmdbNextIterator(iter)) != NULL) {

      /* Ignoring keyring - code taken from updaterpms */
      rpmds provides = rpmdsNew(hdr, RPMTAG_PROVIDENAME, 0);
      if (rpmdsNext(provides) != -1) {
        if  (rpmdsFlags(provides) & RPMSENSE_KEYRING) {
          rpmdsFree(provides);
          continue;
        }
      }   
      rpmdsFree(provides);
      
      LCFGPackage * pkg = lcfgpackage_new();

      char * pkg_name = headerGetAsString( hdr, RPMTAG_NAME );
      ok = lcfgpackage_set_name( pkg, pkg_name );
      if (!ok ) {
        lcfgutils_build_message( msg, "Invalid package name '%s'", pkg_name );
        free(pkg_name);
      }

      if (ok) {
        char * pkg_arch = headerGetAsString( hdr, RPMTAG_ARCH );
        ok = lcfgpackage_set_arch( pkg, pkg_arch );
        if (!ok ) {
          lcfgutils_build_message( msg, "Invalid architecture '%s' for package '%s'", pkg_arch, pkg_name );
          free(pkg_arch);
        }
      }

      if (ok) {
        char * pkg_version = headerGetAsString( hdr, RPMTAG_VERSION );
        ok = lcfgpackage_set_version( pkg, pkg_version );
        if (!ok ) {
          lcfgutils_build_message( msg, "Invalid version '%s' for package '%s'", pkg_version, pkg_name );
          free(pkg_version);
        }
      }

      if (ok) {
        char * pkg_release = headerGetAsString( hdr, RPMTAG_RELEASE );
        ok = lcfgpackage_set_release( pkg, pkg_release );
        if (!ok ) {
          lcfgutils_build_message( msg, "Invalid release '%s' for package '%s'", pkg_release, pkg_name );
          free(pkg_release);
        }
      }

      /* Add the package to the set */
      if (ok) {
        LCFGChange change = lcfgpkgset_merge_package( pkgs, pkg, msg );
        if ( change == LCFG_CHANGE_ERROR )
          ok = false;
      }

      lcfgpackage_relinquish(pkg);
    }

  }

 cleanup:
  
  rpmdbFreeIterator(iter);
  ts = rpmtsFree(ts);

  LCFGStatus status = LCFG_STATUS_OK;
  if (!ok) {
    status = LCFG_STATUS_ERROR;

    lcfgpkgset_relinquish(pkgs);
    pkgs = NULL;
  }

  *result = pkgs;
  
  return status;
}

#endif /* HAVE_RPM */

/* eof */
