#define _GNU_SOURCE   /* asprintf */

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

  FILE * fh1 = fopen( cur_file, "r" );
  if ( fh1 == NULL )
    return true;

  fseek(fh1, 0, SEEK_END);
  long size1 = ftell(fh1);
  rewind(fh1);

  FILE * fh2 = fopen( new_file, "r" );
  if ( fh2 == NULL )
    return true;

  fseek(fh2, 0, SEEK_END);
  long size2 = ftell(fh2);
  rewind(fh2);

  if (size1 != size2)
    return true;

  /* Only if sizes are same do we bother comparing bytes */

  bool needs_update = false;

  char tmp1, tmp2;
  long i;
  for ( i=0; i < size1; i++ ) {
    fread( &tmp1, sizeof(char), 1, fh1 );
    fread( &tmp2, sizeof(char), 1, fh2 );
    if (tmp1 != tmp2) {
      needs_update = true;
      break;
    }
  }

  return needs_update;
}

LCFGStatus lcfgpackage_from_rpm_filename( const char * input,
                                          LCFGPackage ** result,
                                          char ** msg ) {

  *result = NULL;

  if ( isempty(input) )
    return invalid_rpm( msg, "bad filename" );

  size_t input_len = strlen(input);
  if ( input_len <= rpm_file_suffix_len )
    return invalid_rpm( msg, "bad filename" );

  /* Check the suffix */

  if ( !lcfgutils_endswith( input, rpm_file_suffix ) )
    return invalid_rpm( msg, "lacks '%s' suffix", rpm_file_suffix );

  /* Results - note that the file name has to be split apart backwards */

  bool ok = true;
  *result = lcfgpackage_new();

  size_t offset = input_len - suffix_len - 1;

  /* Architecture - search backwards for '.' */

  char * pkg_arch = NULL;

  unsigned int i;
  for ( i=offset; i--; ) {
    if ( input[i] == '.' ) {
      pkg_arch = strndup( input + i + 1, offset - i );
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
      if ( input[i] == '-' ) {
        pkg_release = strndup( input + i + 1, offset - i );
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
      invalid_rpm( errmsg, "bad release '%s'", pkg_release );
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
      if ( input[i] == '-' ) {
        pkg_version = strndup( input + i + 1, offset - i );
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

    pkg_name = strndup( input, offset + 1 );
  }

  if ( pkg_name == NULL ) {
    invalid_rpm( msg, "failed to find name" );
    ok = false;
    goto failure;
  } else {
    ok = lcfgpackage_set_name( *result, pkg_name );
    if ( !ok ) {
      invalid_rpm( errmsg, "bad name '%s'", pkg_name );
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

    if ( *result != NULL ) {
      lcfgpackage_destroy(*result);
      *result = NULL;
    }

  }

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

ssize_t lcfgpackage_to_rpm_filename( const LCFGPackage * pkg,
                                     const char * defarch,
                                     unsigned int options,
                                     char ** result, size_t * size ) {
  assert( pkg != NULL );

  /* Name, version, release and architecture are required */

  if ( !lcfgpackage_has_name(pkg) ||
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
  } else {
    pkgarch = defarch;
  }
  size_t pkgarchlen = strlen(pkgarch);

  /* +3 for the two '-' separators and one '.' */
  size_t new_len = ( pkgnamlen +
                     pkgverlen +
                     pkgrellen +
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

LCFGStatus lcfgpkglist_to_rpmlist( const LCFGPackageList * pkglist,
                                   const char * defarch,
                                   const char * filename,
                                   time_t mtime,
                                   char ** msg ) {
  assert( pkglist  != NULL );
  assert( filename != NULL );

  *msg = NULL;

  char * tmpfile = lcfgutils_safe_tmpfile(filename);

  bool ok = true;

  FILE * out = NULL;
  int tmpfd = mkstemp(tmpfile);
  if ( tmpfd >= 0 )
    out = fdopen( tmpfd, "w" );

  if ( out == NULL ) {
    asprintf( msg, "Failed to open temporary rpmlist file");
    ok = false;
    goto cleanup;
  }

  char * buffer = NULL;
  size_t buf_size = 0;

  LCFGPackageNode * cur_node = NULL;
  for ( cur_node = lcfgpkglist_head(pkglist);
        cur_node != NULL;
        cur_node = lcfgpkglist_next(cur_node) ) {

    const LCFGPackage * pkg = lcfgpkglist_package(cur_node);

    if ( !lcfgpackage_is_active(pkg) ) continue;

    ssize_t rc = lcfgpackage_to_rpm_filename( pkg,
                                              defarch,
                                              LCFG_OPT_NEWLINE,
                                              &buffer, &buf_size );

    if ( rc > 0 ) {

      if ( fputs( buffer, out ) < 0 )
        ok = false;

    } else {
      ok = false;
    }

    if (!ok) {
      asprintf( msg, "Failed to write to rpmlist file" );
      break;
    }

  }

  free(buffer);

  if ( fclose(out) != 0 ) {
    asprintf( msg, "Failed to close rpmlist file" );
    ok = false;
  }

  /* rename tmpfile to target if different */

  if ( ok && file_needs_update( filename, tmpfile ) ) {

    if ( rename( tmpfile, filename ) != 0 )
      ok = false;

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

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

LCFGStatus lcfgpkglist_from_rpm_dir( const char * rpmdir,
                                     LCFGPackageList ** result,
                                     char ** msg ) {

  *result = NULL;
  *msg = NULL;

  if ( rpmdir == NULL || *rpmdir == '\0' ) {
    asprintf( msg, "Invalid RPM directory" );
    return false;
  }

  DIR * dir;
  if ( ( dir = opendir(rpmdir) ) == NULL ) {

    if (errno == ENOENT) {
      asprintf( msg, "Directory does not exist" );
    } else {
      asprintf( msg, "Directory is not readable" );
    }

    return false;
  }

  /* Results */

  bool ok = true;
  *result = lcfgpkglist_new();
  lcfgpkglist_set_merge_rules( *result, LCFG_PKGS_OPT_KEEP_ALL );

  /* Scan the directory for any non-hidden files with .rpm suffix */

  struct dirent * dp;
  struct stat sb;

  while ( ok && ( dp = readdir(dir) ) != NULL ) {

    char * filename = dp->d_name;
    if ( *filename == '.' ) continue;

    /* Just ignore anything which does not have a .rpm suffix */
    if ( !lcfgutils_endswith( filename, rpm_file_suffix ) ) continue;

    /* Only interested in files */
    char * fullpath  = lcfgutils_catfile( rpmdir, filename );
    if ( stat( fullpath, &sb ) == 0 && S_ISREG(sb.st_mode) ) {

      LCFGPackage * pkg = NULL;
      char * parse_msg = NULL;
      LCFGStatus parse_rc = lcfgpackage_from_rpm_filename( filename, &pkg,
                                                           &parse_msg );

      if ( parse_rc == LCFG_STATUS_ERROR ) {

        asprintf( msg, "Failed to parse '%s': %s", filename,
                  ( parse_msg != NULL ? parse_msg : "unknown error" ) );

      } else {

        if ( lcfgpkglist_append( *result, pkg )
             != LCFG_CHANGE_ADDED ) {
          ok = false;
        }

      }

      free(parse_msg);

      if ( !ok )
        lcfgpackage_destroy(pkg);

    }

    free(fullpath);

  }

  closedir(dir);

  /* Finishing off */

  if ( !ok ) {

    if ( *msg == NULL )
      asprintf( msg, "Failed to read RPM directory" );

    if ( *result != NULL ) {
      lcfgpkglist_destroy(*result);
      *result = NULL;
    }
  }

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

LCFGStatus lcfgpkglist_from_rpmlist( const char * filename,
                                     LCFGPackageList ** result,
                                     char ** errmsg ) {

  *result = NULL;
  *errmsg = NULL;

  if ( filename == NULL || *filename == '\0' ) {
    asprintf( errmsg, "Invalid filename" );
    return false;
  }

  FILE * fp;
  if ( (fp = fopen(filename, "r")) == NULL ) {

    if (errno == ENOENT) {
      asprintf( errmsg, "File does not exist" );
    } else {
      asprintf( errmsg, "File is not readable" );
    }

    return false;
  }

  /* Setup the getline buffer */

  size_t line_len = 128;
  char * line = malloc( line_len * sizeof(char) );
  if ( line == NULL ) {
    perror( "Failed to allocate memory whilst processing RPM list file" );
    exit(EXIT_FAILURE);
  }

  /* Results */

  *result = lcfgpkglist_new();
  lcfgpkglist_set_merge_rules( *result,
                    LCFG_PKGS_OPT_SQUASH_IDENTICAL | LCFG_PKGS_OPT_KEEP_ALL );

  bool ok = true;

  unsigned int linenum = 0;
  while( ok && getline( &line, &line_len, fp ) != -1 ) {
    linenum++;

    char * trimmed = strdup(line);
    lcfgutils_trim_whitespace(trimmed);

    /* Ignore empty lines */
    if ( *trimmed == '\0' ) {
      free(trimmed);
      continue;
    }

    LCFGPackage * pkg = NULL;
    char * parse_errmsg = NULL;
    LCFGStatus parse_rc = lcfgpackage_from_rpm_filename( trimmed, &pkg,
                                                         &parse_errmsg );

    if ( parse_rc == LCFG_STATUS_ERROR ) {

      if ( pkg != NULL ) {
        lcfgpackage_destroy(pkg);
        pkg = NULL;
      }

      if ( parse_errmsg == NULL ) {
        asprintf( errmsg, "Error at line %u", linenum );
      } else {
        asprintf( errmsg, "Error at line %u: %s", linenum, parse_errmsg );
        free(parse_errmsg);
      }

    } else {
      char * derivation;
      int rc = asprintf( &derivation, "%s:%u", filename, linenum );
      if ( rc <= 0 ) {
        perror( "Failed to build LCFG derivation string" );
        exit(EXIT_FAILURE);
      }

      lcfgpackage_set_derivation( pkg, derivation );

      char * merge_msg = NULL;
      if ( lcfgpkglist_merge_package( *result, pkg, &merge_msg )
           != LCFG_CHANGE_ADDED ) {
        ok = false;
	asprintf( errmsg,
		  "Error at line %u: Failed to merge package into list: %s",
		  linenum, merge_msg );

        lcfgpackage_destroy(pkg);
        pkg = NULL;
      }
      free(merge_msg);

    }

    free(trimmed);
  }
  fclose(fp);
  free(line);

  /* Finishing off */

  if ( !ok ) {

    if ( *errmsg == NULL )
      asprintf( errmsg, "Failed to parse RPM list file" );

    if ( *result != NULL ) {
      lcfgpkglist_destroy(*result);
      *result = NULL;
    }
  }

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

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
    ok = false;
    asprintf( msg, "Failed to open temporary rpmcfg file");
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

  if ( !lcfgpkglist_is_empty(active) ) {
    lcfgpkglist_sort(active);

    LCFGPackageNode * cur_node = lcfgpkglist_head(active);
    while ( cur_node != NULL ) {

      const LCFGPackage * pkg = lcfgpkglist_package(cur_node);

      if ( !lcfgpackage_is_active(pkg) ) continue;

      ssize_t rc = lcfgpackage_to_cpp( pkg,
                                       defarch,
                                       0,
                                       &buffer, &buf_size );

      if ( rc > 0 ) {

        if ( fputs( buffer, out ) < 0 )
          ok = false;

      } else {
        ok = false;
      }

      if (!ok) {
        asprintf( msg, "Failed to write to rpmcfg file" );
        break;
      }

      cur_node = lcfgpkglist_next(cur_node);
    }

  }

  /* List the RPMs that would be present in other contexts. This is
    used by the rpm cache stuff because we need to cache all the RPMs,
    regardless of context. */

  if ( fprintf( out, "#ifdef ALL_CONTEXTS\n" ) < 0 )
    ok = false;

  if ( ok && !lcfgpkglist_is_empty(inactive) ) {
    lcfgpkglist_sort(inactive);

    LCFGPackageNode * cur_node = lcfgpkglist_head(inactive);
    while ( cur_node != NULL ) {

      const LCFGPackage * pkg = lcfgpkglist_package(cur_node);

      ssize_t rc = lcfgpackage_to_cpp( pkg,
                                       defarch,
                                       0,
                                       &buffer, &buf_size );

      if ( rc > 0 ) {

        if ( fputs( buffer, out ) < 0 )
          ok = false;

      } else {
        ok = false;
      }

      if (!ok) {
        asprintf( msg, "Failed to write to rpmcfg file" );
        break;
      }

      cur_node = lcfgpkglist_next(cur_node);
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
    asprintf( msg, "Failed to close rpmcfg file" );
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

/* eof */
