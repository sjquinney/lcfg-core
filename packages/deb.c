/**
 * @file packages/deb.c
 * @brief Functions for working with Debian packages
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "packages.h"

static const char * deb_file_suffix = ".deb";
static size_t deb_file_suffix_len = 4;

/**
 * @brief Format the package as an Debian filename.
 *
 * Generates a new Debian filename based on the @c LCFGPackage in the
 * standard @c name_version-release_arch.deb format.
 *
 * The following options are supported:
 *   - @c LCFG_OPT_NEWLINE - Add a newline at the end of the string.
 *
 * Note that the filename must contain a value for each field. If the
 * package is missing a name or version an error will occur. If the
 * package does not have an architecture specified and the default
 * architecture is set to @c NULL then the value as returned by the @c
 * default_architecture() function will be used.
 *
 * For compatibility with Redhat packages, on Debian a default
 * architecture of C<x86_64> is translated into C<amd64> and C<noarch>
 * is translated into C<all>.
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
ssize_t lcfgpackage_to_deb_filename( LCFG_PKG_TOSTR_ARGS ) {
  assert( pkg != NULL );

  /* Name, version and architecture are required */

  if ( !lcfgpackage_is_valid(pkg) || !lcfgpackage_has_version(pkg) )
    return -1;

  const char * pkgnam = lcfgpackage_get_name(pkg);
  size_t pkgnamlen    = strlen(pkgnam);

  const char * pkgver = lcfgpackage_get_version(pkg);

  /* Step beyond any epoch */
  const char * epoch_colon = strchr( pkgver, ':' );
  if ( epoch_colon != NULL )
    pkgver = epoch_colon + 1;

  size_t pkgverlen    = strlen(pkgver);

  const char * pkgarch;
  if ( lcfgpackage_has_arch(pkg) ) {
    pkgarch = lcfgpackage_get_arch(pkg);
  } else if ( !isempty(defarch) ) {
    pkgarch = defarch;
  } else {
    pkgarch = default_architecture();
  }

  /* x86_64 packages are really amd64 on Debian */
  if ( strcmp(pkgarch, "x86_64" ) == 0 )
    pkgarch = "amd64";
  else if ( strcmp(pkgarch, "noarch" ) == 0 )
    pkgarch = "all";

  size_t pkgarchlen = strlen(pkgarch);

  /* +2 for the two '_' (underscore) separators - 'name_version_arch.deb' */
  size_t new_len = ( pkgnamlen  +
                     pkgverlen  +
                     pkgarchlen +
		     deb_file_suffix_len + 2 );

  /* For Debian packages the release (i.e. the debian_revision) is optional */

  const char * pkgrel = NULL;
  size_t pkgrellen    = 0;

  if ( lcfgpackage_has_release(pkg) ) {
    pkgrel    = lcfgpackage_get_release(pkg);
    pkgrellen = strlen(pkgrel);
    new_len += ( pkgrellen + 1 ); /* +1 for the '-' (hyphen) version-release */
  }

  if ( options&LCFG_OPT_NEWLINE )
    new_len += 1;

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    size_t new_size = new_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG package string");
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

  /* name */
  to = stpncpy( to, pkgnam, pkgnamlen );

  *to = '_';
  to++;

  /* version */
  to = stpncpy( to, pkgver, pkgverlen );

  /* optional release */
  if ( pkgrel != NULL ) {
    *to = '-';
    to++;

    to = stpncpy( to, pkgrel, pkgrellen );
  }

  *to = '_';
  to++;

  /* arch */
  to = stpncpy( to, pkgarch, pkgarchlen );

  /* suffix */
  to = stpncpy( to, deb_file_suffix, deb_file_suffix_len );

  if ( options&LCFG_OPT_NEWLINE )
    to = stpncpy( to, "\n", 1 );

  *to = '\0';

  assert( ( *result + new_len ) == to );

  return new_len;
}
