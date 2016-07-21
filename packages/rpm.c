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

static bool file_needs_update( const char * cur_file,
                               const char * new_file ) {

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

bool lcfgpackage_from_rpm_filename( const char * input,
                                    LCFGPackage ** result,
                                    char ** errmsg ) {

  *result = NULL;
  *errmsg = NULL;

  if ( input == NULL || *input == '\0' ) {
    asprintf( errmsg, "Invalid RPM filename" );
    return false;
  }

  size_t input_len  = strlen(input);
  size_t suffix_len = strlen(rpm_file_suffix);

  if ( input_len <= suffix_len ) {
    asprintf( errmsg, "Invalid RPM filename '%s'", input );
    return false;
  }

  /* Check the suffix */

  if ( !lcfgutils_endswith( input, rpm_file_suffix ) ) {
    asprintf( errmsg, "Invalid RPM filename '%s', does not have '%s' suffix",
              input, rpm_file_suffix );
    return false;
  }

  /* Results - note that the file name has to be split apart backwards */

  bool ok = true;
  *result = lcfgpackage_new();

  size_t offset = input_len - suffix_len - 1;

  /* Architecture */

  char * pkg_arch = NULL;

  unsigned int i;
  for ( i=offset; i--; ) {
    if ( input[i] == '.' ) {
      pkg_arch = strndup( input + i + 1, offset - i );
      break;
    }
  }

  if ( pkg_arch == NULL ) {
    asprintf( errmsg, "Invalid RPM filename '%s', failed to find package architecture.", input );
    ok = false;
    goto failure;
  } else {
    ok = lcfgpackage_set_arch( *result, pkg_arch );
    if ( !ok ) {
      asprintf( errmsg, "Invalid RPM filename '%s', bad package architecture '%s'", input, pkg_arch );
      free(pkg_arch);
      goto failure;
    }
  }

  /* Release field */

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
    asprintf( errmsg, "Invalid RPM filename '%s', failed to find package release.", input );
    ok = false;
    goto failure;
  } else {
    ok = lcfgpackage_set_release( *result, pkg_release );
    if ( !ok ) {
      asprintf( errmsg, "Invalid RPM filename '%s', bad package release '%s'", input, pkg_release );
      free(pkg_release);
      goto failure;
    }
  }

  /* Version field */

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
    asprintf( errmsg, "Invalid RPM filename '%s', failed to find package version.", input );
    ok = false;
    goto failure;
  } else {
    ok = lcfgpackage_set_version( *result, pkg_version );
    if ( !ok ) {
      asprintf( errmsg, "Invalid RPM filename '%s', bad package version '%s'", input, pkg_version );
      free(pkg_version);
      goto failure;
    }
  }

  /* Name field */

  char * pkg_name = NULL;
  if ( i > 0 ) {
    offset = i - 1;

    pkg_name = strndup( input, offset + 1 );
  }

  if ( pkg_name == NULL ) {
    asprintf( errmsg, "Invalid RPM filename '%s', failed to find package name.", input );
    ok = false;
    goto failure;
  } else {
    ok = lcfgpackage_set_name( *result, pkg_name );
    if ( !ok ) {
      asprintf( errmsg, "Invalid RPM filename '%s', bad package name '%s'", input, pkg_name );
      free(pkg_name);
      goto failure;
    }
  }

  /* Finishing off */

  if ( !ok ) {

  failure:
    ok = false; /* in case we've jumped in from elsewhere */

    if ( *errmsg == NULL )
      asprintf( errmsg, "Invalid RPM filename '%s'", input );

    if ( *result != NULL ) {
      lcfgpackage_destroy(*result);
      *result = NULL;
    }

  }

  return ok;
}

ssize_t lcfgpackage_to_rpm_filename( const LCFGPackage * pkgspec,
                                     const char * defarch,
                                     unsigned int options,
                                     char ** result, size_t * size ) {

  const char * pkgnam  = lcfgpackage_get_name(pkgspec);
  const char * pkgver  = lcfgpackage_get_version(pkgspec);
  const char * pkgrel  = lcfgpackage_get_release(pkgspec);

  const char * pkgarch;
  if ( lcfgpackage_has_arch(pkgspec) ) {
    pkgarch = lcfgpackage_get_arch(pkgspec);
  } else {
    pkgarch = defarch;
  }

  /* Name, version, release and architecture are required */

  if ( pkgnam  == NULL || *pkgnam  == '\0' ||
       pkgver  == NULL || *pkgver  == '\0' ||
       pkgrel  == NULL || *pkgrel  == '\0' ||
       pkgarch == NULL || *pkgarch == '\0' ) {
    return -1;
  }

  /* +3 for the two '-' separators and one '.' */
  size_t new_len = ( strlen(pkgnam) +
		     strlen(pkgver) + 
		     strlen(pkgrel) +
		     strlen(pkgarch) +
		     strlen(rpm_file_suffix) + 3 );

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
  to = stpcpy( to, pkgnam );

  *to = '-';
  to++;

  /* version */
  to = stpcpy( to, pkgver );

  *to = '-';
  to++;

  /* release */
  to = stpcpy( to, pkgrel );

  *to = '.';
  to++;

  /* arch */
  to = stpcpy( to, pkgarch );

  /* suffix */
  to = stpcpy( to, rpm_file_suffix );

  if ( options&LCFG_OPT_NEWLINE )
    to = stpcpy( to, "\n" );

  *to = '\0';

  assert( ( *result + new_len ) == to );

  return new_len;
}

LCFGStatus lcfgpkglist_to_rpmlist( const LCFGPackageList * pkglist,
                                   const char * defarch,
                                   const char * filename,
                                   time_t mtime,
                                   char ** msg ) {

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

  LCFGPackageNode * cur_node = lcfgpkglist_head(pkglist);
  while ( cur_node != NULL ) {

    const LCFGPackage * pkgspec = lcfgpkglist_pkgspec(cur_node);

    ssize_t rc = lcfgpackage_to_rpm_filename( pkgspec,
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

    cur_node = lcfgpkglist_next(cur_node);
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

bool lcfgpkglist_from_rpm_dir( const char * rpmdir,
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

      LCFGPackage * pkgspec = NULL;
      char * parse_msg = NULL;
      ok = lcfgpackage_from_rpm_filename( filename, &pkgspec,
                                          &parse_msg );

      if ( !ok ) {

        asprintf( msg, "Failed to parse '%s': %s", filename,
                  ( parse_msg != NULL ? parse_msg : "unknown error" ) );

      } else {

        if ( lcfgpkglist_append( *result, pkgspec )
             != LCFG_CHANGE_ADDED ) {
          ok = false;
        }

      }

      free(parse_msg);

      if ( !ok )
        lcfgpackage_destroy(pkgspec);

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

  return ok;
}

bool lcfgpkglist_from_rpmlist( const char * filename,
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

    LCFGPackage * pkgspec = NULL;
    char * parse_errmsg = NULL;
    ok = lcfgpackage_from_rpm_filename( trimmed, &pkgspec, &parse_errmsg );

    if ( !ok ) {

      if ( pkgspec != NULL ) {
        lcfgpackage_destroy(pkgspec);
        pkgspec = NULL;
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

      lcfgpackage_set_derivation( pkgspec, derivation );

      if ( lcfgpkglist_append( *result, pkgspec )
           != LCFG_CHANGE_ADDED ) {
        ok = false;
        lcfgpackage_destroy(pkgspec);
        pkgspec = NULL;
      }

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

  return ok;
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

  if ( active != NULL && !lcfgpkglist_is_empty(active) ) {
    lcfgpkglist_sort(active);

    LCFGPackageNode * cur_node = lcfgpkglist_head(active);
    while ( cur_node != NULL ) {

      const LCFGPackage * pkgspec = lcfgpkglist_pkgspec(cur_node);

      ssize_t rc = lcfgpackage_to_cpp( pkgspec,
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

  if ( ok && inactive != NULL && !lcfgpkglist_is_empty(inactive) ) {
    lcfgpkglist_sort(inactive);

    LCFGPackageNode * cur_node = lcfgpkglist_head(inactive);
    while ( cur_node != NULL ) {

      const LCFGPackage * pkgspec = lcfgpkglist_pkgspec(cur_node);

      ssize_t rc = lcfgpackage_to_cpp( pkgspec,
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
