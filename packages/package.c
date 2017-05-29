/**
 * @file packages/package.c
 * @brief Functions for working with LCFG packages
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#define _GNU_SOURCE /* for asprintf */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <sys/utsname.h>
#include <errno.h>
#include <assert.h>

#ifdef HAVE_RPMLIB
#include <rpm/rpmlib.h>
#endif

#include "context.h"
#include "packages.h"
#include "utils.h"

/**
 * @brief Check character is valid for package name
 *
 * This permits @c [A-Za-z0-9_-.+] characters in package names
 */

#define isnamechr(CHR) ( isword(CHR) || strchr( "-.+", CHR ) != NULL )

static LCFGStatus invalid_package( char ** msg, const char * base, ... ) {

  const char * fmt = "Invalid package (%s)";

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

/* Currently there are five supported prefixes for LCFG package
   specifications:

   +  Insert package into list, replaces any existing package of same name/arch
   =  Similar to + but "pins" the version so it cannot be overridden
   -  Remove any package from list which matches this name/arch
   ?  Replace any existing package in list which matches this name/arch
   ~  Add package to list if name/arch is not already present
*/

static const char * permitted_prefixes = "?+-=~";

/**
 * @brief Create and initialise a new package
 *
 * Creates a new @c LCFGPackage and initialises the parameters to the
 * default values.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgpackage_relinquish() function should be called.
 *
 * @return Pointer to new @c LCFGPackage
 *
 */

LCFGPackage * lcfgpackage_new(void) {

  LCFGPackage * pkg = malloc( sizeof(LCFGPackage) );
  if ( pkg == NULL ) {
    perror( "Failed to allocate memory for LCFG package spec" );
    exit(EXIT_FAILURE);
  }

  pkg->name       = NULL;
  pkg->arch       = NULL;
  pkg->version    = NULL;
  pkg->release    = NULL;
  pkg->flags      = NULL;
  pkg->context    = NULL;
  pkg->derivation = NULL;
  pkg->prefix     = '\0';
  pkg->priority   = 0;
  pkg->_refcount  = 1;

  return pkg;
}

/**
 * @brief Destroy the package
 *
 * When the specified @c LCFGPackage is no longer required this will
 * free all associated memory.
 *
 * *Reference Counting:* There is support for very simple reference
 * counting which allows an @c LCFGPackage to appear in multiple
 * lists. Incrementing and decrementing that reference counter is the
 * responsibility of the container code. If the reference count for
 * the specified package is greater than zero then calling this
 * function will have no effect.
 *
 * This will call @c free() on each parameter of the structure and
 * then set each value to be @c NULL.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * package which has already been destroyed (or potentially was never
 * created).
 *
 * @param[in] pkg Pointer to @c LCFGPackage to be destroyed.
 *
 */

void lcfgpackage_destroy(LCFGPackage * pkg) {

  if ( pkg == NULL ) return;

  free(pkg->name);
  pkg->name = NULL;

  free(pkg->arch);
  pkg->arch = NULL;

  free(pkg->version);
  pkg->version = NULL;

  free(pkg->release);
  pkg->release = NULL;

  free(pkg->flags);
  pkg->flags = NULL;

  free(pkg->context);
  pkg->context = NULL;

  free(pkg->derivation);
  pkg->derivation = NULL;

  free(pkg);
  pkg = NULL;

}

/**
 * @brief Acquire reference to package
 *
 * This is used to record a reference to the @c LCFGPackage, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgpackage_relinquish() function should be
 * called.
 *
 * @param[in] pkg Pointer to @c LCFGPackage
 *
 */

void lcfgpackage_acquire( LCFGPackage * pkg ) {
  assert( pkg != NULL );

  pkg->_refcount += 1;
}

/**
 * @brief Release reference to package
 *
 * This is used to release a reference to the @c LCFGPackage,
 * it does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgpackage_destroy() function
 * will be called to clean up the memory associated with the structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * package which has already been destroyed (or potentially was never
 * created).
 *
 * @param[in] pkg Pointer to @c LCFGPackage
 *
 */

void lcfgpackage_relinquish( LCFGPackage * pkg ) {

  if ( pkg == NULL ) return;

  if ( pkg->_refcount > 0 )
    pkg->_refcount -= 1;

  if ( pkg->_refcount == 0 )
    lcfgpackage_destroy(pkg);

}

/**
 * @brief Clone the package
 *
 * Creates a new @c LCFGPackage and copies the values of the
 * parameters from the specified package. The values for the
 * parameters are copied (e.g. strings are duplicated using
 * @c strdup(3) ) so that a later change to a parameter in the source
 * package does not affect the new clone package.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * @param[in] pkg Pointer to @c LCFGPackage to be cloned.
 *
 * @return Pointer to new @c LCFGPackage or @c NULL if copy fails.
 *
 */

LCFGPackage * lcfgpackage_clone( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  LCFGPackage * clone = lcfgpackage_new();
  if ( clone == NULL ) return NULL;

  bool ok = true;
  if ( !isempty(pkg->name) ) {
    char * new_name = strdup(pkg->name);
    ok = lcfgpackage_set_name( clone, new_name );
    if (!ok)
      free(new_name);
  }

  if ( ok && !isempty(pkg->arch) ) {
    char * new_arch = strdup(pkg->arch);
    ok = lcfgpackage_set_arch( clone, new_arch );
    if (!ok)
      free(new_arch);
  }

  if ( ok && !isempty(pkg->version) ) {
    char * new_version = strdup(pkg->version);
    ok = lcfgpackage_set_version( clone, new_version );
    if (!ok)
      free(new_version);
  }

  if ( ok && !isempty(pkg->release) ) {
    char * new_release = strdup(pkg->release);
    ok = lcfgpackage_set_release( clone, new_release );
    if (!ok)
      free(new_release);
  }

  if ( ok && !isempty(pkg->flags) ) {
    char * new_flags = strdup(pkg->flags);
    ok = lcfgpackage_set_flags( clone, new_flags );
    if (!ok)
      free(new_flags);
  }

  if ( ok && !isempty(pkg->context) ) {
    char * new_context = strdup(pkg->context);
    ok = lcfgpackage_set_context( clone, new_context );
    if (!ok)
      free(new_context);
  }

  if ( ok && !isempty(pkg->derivation) ) {
    char * new_deriv = strdup(pkg->derivation);
    ok = lcfgpackage_set_derivation( clone, new_deriv );
    if (!ok)
      free(new_deriv);
  }

  clone->prefix   = pkg->prefix;
  clone->priority = pkg->priority;

  /* Cloning should never fail */
  if ( !ok ) {
    lcfgpackage_destroy(clone);
    clone = NULL;
  }

  return clone;
}

/**
 * @brief Check validity of package
 *
 * Checks the specified package to ensure that it is valid. For a
 * package to be considered valid the pointer must not be @c NULL and
 * the package must have a name.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return Boolean which indicates if package is valid
 *
 */

bool lcfgpackage_is_valid( const LCFGPackage * pkg ) {
  return ( pkg != NULL && !isempty(pkg->name) );
}

/* Name */

/**
 * @brief Check if a string is a valid LCFG package name
 *
 * Checks the contents of a specified string against the specification
 * for an LCFG package name.
 *
 * An LCFG package name MUST be at least one character in length. The
 * first character MUST be in the class @c [A-Za-z] and all other
 * characters MUST be in the class @c [A-Za-z0-9_-.+]. 
 *
 * @param[in] name String to be tested
 *
 * @return boolean which indicates if string is a valid package name
 *
 */

bool lcfgpackage_valid_name( const char * name ) {

  /* MUST be at least one character long and first character MUST be
     an alpha-numeric. */

  bool valid = ( !isempty(name) && isalnum(*name) );

  const char * ptr;
  for ( ptr = name + 1; valid && *ptr != '\0'; ptr++ )
    if ( !isnamechr(*ptr) ) valid = false;

  return valid;
}

/**
 * @brief Check if the package has a name
 *
 * Checks if the specified @c LCFGPackage currently has a
 * value set for the @e name attribute. Although a name is required
 * for an LCFG package to be valid it is possible for the value of the
 * name to be set to @c NULL when the structure is first created.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return boolean which indicates if a package has a name
 *
 */

bool lcfgpackage_has_name( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return !isempty(pkg->name);
}

/**
 * @brief Get the name for the package
 *
 * This returns the value of the @e name parameter for the
 * @c LCFGPackage. If the package does not currently have a
 * @e name then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e name for the
 * package.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return The name for the package (possibly NULL).
 */

const char * lcfgpackage_get_name( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return pkg->name;
}

/**
 * @brief Set the name for the package
 *
 * Sets the value of the @e name parameter for the @c LCFGPackage
 * to that specified. It is important to note that this does
 * NOT take a copy of the string. Furthermore, once the value is set
 * the package assumes "ownership", the memory will be freed if the
 * name is further modified or the package is destroyed.
 *
 * Before changing the value of the @e name to be the new string it
 * will be validated using the @c lcfgpackage_valid_name()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] new_name String which is the new name
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_name( LCFGPackage * pkg, char * new_name ) {
  assert( pkg != NULL );

  bool ok = false;
  if ( lcfgpackage_valid_name(new_name) ) {
    free(pkg->name);

    pkg->name = new_name;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/* Architecture */

/**
 * @brief Check if a string is a valid LCFG package architecture
 *
 * Checks the contents of a specified string against the specification
 * for an LCFG package architecture.
 *
 * An LCFG package architecture MUST be at least one character in
 * length. All the characters MUST be in the class @c [A-Za-z0-9_-].
 *
 * @param[in] arch String to be tested
 *
 * @return boolean which indicates if string is a valid package architecture
 *
 */

bool lcfgpackage_valid_arch( const char * arch ) {

  /* MUST be at least one character long */

  bool valid = !isempty(arch);

  /* Permit [a-zA-Z0-9_-] characters */

  const char * ptr;
  for ( ptr = arch; valid && *ptr != '\0'; ptr++ )
    if ( !isword(*ptr) && *ptr != '-' ) valid = false;

  return valid;
}

/**
 * @brief Check if the package has an architecture 
 *
 * Checks if the specified @c LCFGPackage currently has a
 * value set for the @e arch attribute. 
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return boolean which indicates if a package has an architecture
 *
 */

bool lcfgpackage_has_arch( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return !isempty(pkg->arch);
}

/**
 * @brief Get the architecture for the package
 *
 * This returns the value of the @e arch parameter for the
 * @c LCFGPackage. If the package does not currently have an
 * @e arch then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e arch for the
 * package.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return The architecture for the package (possibly @c NULL).
 */

const char * lcfgpackage_get_arch( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return pkg->arch;
}

/**
 * @brief Set the architecture for the package
 *
 * Sets the value of the @e arch parameter for the @c LCFGPackage
 * to that specified. It is important to note that this does
 * NOT take a copy of the string. Furthermore, once the value is set
 * the package assumes "ownership", the memory will be freed if the
 * architecture is further modified or the package is destroyed.
 *
 * Before changing the value of the @e arch to be the new string it
 * will be validated using the @c lcfgpackage_valid_arch()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] new_arch String which is the new architecture
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_arch( LCFGPackage * pkg, char * new_arch ) {
  assert( pkg != NULL );

  bool ok = false;
  if ( lcfgpackage_valid_arch(new_arch) ) {
    free(pkg->arch);

    pkg->arch = new_arch;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/* Version */

/**
 * @brief Check if a string is a valid LCFG package version
 *
 * Checks the contents of a specified string against the specification
 * for an LCFG package version.
 *
 * An LCFG package version MUST be at least one character in length. The
 * version string must NOT contain a '-' (hyphen) or white space.
 *
 * @param[in] version String to be tested
 *
 * @return boolean which indicates if string is a valid package version
 *
 */

bool lcfgpackage_valid_version( const char * version ) {

  /* Must be at least one character long. */

  bool valid = !isempty(version);

  /* It is not entirely clear exactly which characters are allowed but
     it is necessary to forbid '-' (hyphen) and any whitespace */

  const char * ptr;
  for ( ptr = version; valid && *ptr != '\0'; ptr++ )
    if ( *ptr == '-' || isspace(*ptr) ) valid = false;

  return valid;
}

/**
 * @brief Check if the package has a version
 *
 * Checks if the specified @c LCFGPackage currently has a
 * value set for the @e version attribute. 
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return boolean which indicates if a package has a version
 *
 */

bool lcfgpackage_has_version( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return !isempty(pkg->version);
}

/**
 * @brief Get the version for the package
 *
 * This returns the value of the @e version parameter for the
 * @c LCFGPackage. If the package does not currently have a
 * @e version then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e version for the
 * package.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return The version for the package (possibly @c NULL).
 */

const char * lcfgpackage_get_version( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return pkg->version;
}

/**
 * @brief Set the version for the package
 *
 * Sets the value of the @e version parameter for the @c LCFGPackage
 * to that specified. It is important to note that this does
 * NOT take a copy of the string. Furthermore, once the value is set
 * the package assumes "ownership", the memory will be freed if the
 * version is further modified or the package is destroyed.
 *
 * Before changing the value of the @e version to be the new string it
 * will be validated using the @c lcfgpackage_valid_version()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] new_version String which is the new version
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_version( LCFGPackage * pkg, char * new_version ) {
  assert( pkg != NULL );

  bool ok = false;
  if ( lcfgpackage_valid_version(new_version) ) {
    free(pkg->version);

    pkg->version = new_version;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/* Release */

/**
 * @brief Check if a string is a valid LCFG package release
 *
 * Checks the contents of a specified string against the specification
 * for an LCFG package release.
 *
 * An LCFG package release MUST be at least one character in
 * length. The version string must NOT contain a '-' (hyphen) or white
 * space.
 *
 * @param[in] release String to be tested
 *
 * @return boolean which indicates if string is a valid package release
 *
 */

bool lcfgpackage_valid_release( const char * release ) {
  /* Currently the same rules as for version strings */
  return lcfgpackage_valid_version(release);
}

/**
 * @brief Check if the package has a release
 *
 * Checks if the specified @c LCFGPackage currently has a
 * value set for the @e release attribute. 
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return boolean which indicates if a package has a release
 *
 */

bool lcfgpackage_has_release( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return !isempty(pkg->release);
}

/**
 * @brief Get the release for the package
 *
 * This returns the value of the @e release parameter for the
 * @c LCFGPackage. If the package does not currently have a
 * @e release then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e release for the
 * package.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return The release for the package (possibly NULL).
 */

const char * lcfgpackage_get_release( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return pkg->release;
}

/**
 * @brief Set the release for the package
 *
 * Sets the value of the @e release parameter for the @c LCFGPackage
 * to that specified. It is important to note that this does
 * NOT take a copy of the string. Furthermore, once the value is set
 * the package assumes "ownership", the memory will be freed if the
 * release is further modified or the package is destroyed.
 *
 * Before changing the value of the @e release to be the new string it
 * will be validated using the @c lcfgpackage_valid_release()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] new_release String which is the new release
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_release( LCFGPackage * pkg, char * new_release ) {
  assert( pkg != NULL );

  bool ok = false;
  if ( lcfgpackage_valid_release(new_release) ) {
    free(pkg->release);

    pkg->release = new_release;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/* Prefix */

/**
 * @brief Check if a character is a valid LCFG package prefix
 *
 * Checks the specified character against the specification for an
 * LCFG package prefix.
 *
 * An LCFG package prefix is a single character. The currently
 * supported prefixes are:
 * 
 *   - @c +  Add package to list, replace any existing package of same name/arch
 *   - @c =  Similar to @c + but "pins" the version so it cannot be overridden
 *   - @c -  Remove any package from list which matches this name/arch
 *   - @c ?  Replace any existing package in list which matches this name/arch
 *   - @c ~  Add package to list if name/arch is not already present
 *
 * @param[in] prefix Character to be tested
 *
 * @return boolean which indicates if string is a valid package prefix
 *
 */

bool lcfgpackage_valid_prefix( char prefix ) {
  return ( strchr( permitted_prefixes, prefix ) != NULL );
}

/**
 * @brief Check if the package has a prefix
 *
 * Checks if the specified @c LCFGPackage currently has a
 * value set for the @e prefix attribute. 
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return boolean which indicates if a package has a prefix
 *
 */

bool lcfgpackage_has_prefix( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return ( pkg->prefix != '\0' );
}

/**
 * @brief Get the prefix for the package
 *
 * This returns the value of the @e prefix parameter for the
 * @c LCFGPackage. If the package does not currently have a
 * @e release then the null character @c '\0' will be returned.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return The prefix character for the package.
 */

char lcfgpackage_get_prefix( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return pkg->prefix;
}

/**
 * @brief Clear any prefix for the package
 *
 * Removes any prefix which has been previously specified for the
 * package specification. This is done by resetting the @c prefix
 * attribute for the @c LCFGPackage to be the null character @c '\0'.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return boolean indicating success
 */

bool lcfgpackage_clear_prefix( LCFGPackage * pkg ) {
  assert( pkg != NULL );

  pkg->prefix = '\0';
  return true;
}

/**
 * @brief Set the prefix for the package
 *
 * Sets the value of the @e prefix parameter for the @c LCFGPackage
 * to that specified. 
 *
 * Before changing the value of the @e prefix to be the new character
 * it will be validated using the @c lcfgpackage_valid_prefix()
 * function. If the new character is not valid then no change will
 * occur, the @c errno will be set to @c EINVAL and the function will
 * return a @c false value.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] new_prefix Character which is the new prefix
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_prefix( LCFGPackage * pkg, char new_prefix ) {
  assert( pkg != NULL );

  bool ok = false;
  if ( lcfgpackage_valid_prefix(new_prefix) ) {
    pkg->prefix = new_prefix;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/* Flags */

/**
 * @brief Check if character is a valid LCFG package flag
 *
 * Checks the specified character against the specification
 * for an LCFG package flag.
 *
 * An LCFG package flag is a single character which MUST be in the set
 * @c [a-zA-Z0-9]
 *
 * @param[in] flag Character to be tested
 *
 * @return boolean which indicates if character is a valid package flag
 *
 */

bool lcfgpackage_valid_flag_chr( const char flag ) {
  return ( isalnum(flag) );
}

/**
 * @brief Check if a string is a valid set of LCFG package flags
 *
 * Checks the contents of a specified string are all valid LCFG
 * package flags. See @c lcfgpackage_valid_flag() for details.
 *
 * @param[in] flags String to be tested
 *
 * @return boolean which indicates if string is a valid set of package flags
 *
 */

bool lcfgpackage_valid_flags( const char * flags ) {

  /* MUST be at least one character long. */

  bool valid = !isempty(flags);

  /* This only permits [a-zA-Z0-9] characters */

  const char * ptr;
  for ( ptr = flags; valid && *ptr != '\0'; ptr++ )
    if ( !lcfgpackage_valid_flag_chr(*ptr) ) valid = false;

  return valid;
}

/**
 * @brief Check if the package has any flags
 *
 * Checks if the specified @c LCFGPackage currently has a
 * value set for the @e flags attribute. 
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return boolean which indicates if a package has flags
 *
 */

bool lcfgpackage_has_flags( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return !isempty(pkg->flags);
}

/**
 * @brief Check if the package has a particular flag
 *
 * Checks if the specified @c LCFGPackage currently has a
 * particular flag enabled.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] flag Character to check
 *
 * @return boolean which indicates if a package has a flag set
 *
 */

bool lcfgpackage_has_flag( const LCFGPackage * pkg, char flag ) {
  assert( pkg != NULL );

  return ( !isempty(pkg->flags) && strchr( pkg->flags, flag ) != NULL );
}

/**
 * @brief Get the flags for the package
 *
 * This returns the value of the @e flags parameter for the @c
 * LCFGPackage. If the package does not currently have @e
 * flags then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e flags for the
 * package.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return The flags for the package (possibly NULL).
 */

const char * lcfgpackage_get_flags( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return pkg->flags;
}

/**
 * @brief Set the flags for the package
 *
 * Sets the value of the @e flags parameter for the @c LCFGPackage
 * to that specified. It is important to note that this does
 * NOT take a copy of the string. Furthermore, once the value is set
 * the package assumes "ownership", the memory will be freed if the
 * flags are further modified or the package is destroyed.
 *
 * Before changing the value of the @e flags to be the new string it
 * will be validated using the @c lcfgpackage_valid_flags()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] new_flags String which is the new flags
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_flags( LCFGPackage * pkg, char * new_flags ) {
  assert( pkg != NULL );

  bool ok = false;
  if ( lcfgpackage_valid_flags(new_flags) ) {
    free(pkg->flags);

    pkg->flags = new_flags;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/**
 * @brief Clear the flags for the package
 *
 * Sets the value of the @e flags parameter for the @c LCFGPackage
 * to be NULL
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_clear_flags( LCFGPackage * pkg ) {
  assert( pkg != NULL );

  free(pkg->flags);
  pkg->flags = NULL;

  return true;
}

/**
 * @brief Add flags to the package
 *
 * Combines the extra flags with any already present so that each flag
 * character only appears once. Due to the way the selection process
 * works this has the side-effect of sorting the list of flags. The
 * extra flags must be valid according to @c lcfgpackage_valid_flags.
 *
 * The new string is passed to @c lcfgpackage_set_flags(), unlike that
 * function this does NOT assume "ownership" of the input string.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] extra_flags String of extra flags to be added
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_add_flags( LCFGPackage * pkg,
                            const char * extra_flags ) {
  assert( pkg != NULL );

  if ( isempty(extra_flags) ) return true;

  /* No point doing any work if there are invalid flags */

  if ( !lcfgpackage_valid_flags(extra_flags) ) {
    errno = EINVAL;
    return false;
  }

  unsigned int i;

  size_t new_len = 0;

  /* This uses a simple trick to find the unique set of flags required
     given the 'current' and 'extra' flags

     An array of booleans is used, each element represents the ASCII
     code for a character. The array begins as all false and then when
     characters are seen for the first time in either string the
     value for that element is switched to true.

     This silently ignores any character with an ASCII code greater
     than LCFG_PKG_FLAGS_MAXCHAR but that's unlikely to be a huge
     problem. Other invalid characters not in the [a-zA-Z0-9] set will
     be flagged up as a problem when set_flags is called at the end of
     this function.

  */

#define LCFG_PKG_FLAGS_MAXCHAR 128

  bool char_set[LCFG_PKG_FLAGS_MAXCHAR] = { false };

  size_t cur_len = 0;
  if ( !isempty(pkg->flags) ) {
    cur_len = strlen(pkg->flags);

    for ( i=0; i<cur_len; i++ ) {
      int val = (pkg->flags)[i] - '0';
      if ( val < LCFG_PKG_FLAGS_MAXCHAR ) {
        if ( !char_set[val] ) {
          new_len++;
          char_set[val] = true;
        }
      }

    }

  }

  const char * ptr;
  for ( ptr = extra_flags; *ptr != '\0'; ptr++ ) {
    int val = *ptr - '0';
    if ( val < LCFG_PKG_FLAGS_MAXCHAR ) {
      if ( !char_set[val] ) {
        new_len++;
        char_set[val] = true;
      }
    }
  }

  /* If the new length is the same as the current then there is
     nothing more to do so just return success */

  if ( new_len == cur_len ) return true;

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror( "Failed to allocate memory for LCFG package flags" );
    exit(EXIT_FAILURE);
  }

  char * to = result;

  for ( i=0; i<LCFG_PKG_FLAGS_MAXCHAR; i++ ) {
    if ( char_set[i] ) {
      *to = i + '0';
      to++;
    }
  }

  *to = '\0';

  assert( ( result + new_len ) == to );

  bool ok = lcfgpackage_set_flags( pkg, result );
  if ( !ok )
    free(result);

  return ok;
}

/* Context Expression */

/**
 * @brief Check if a string is a valid LCFG context expression
 *
 * Checks the contents of a specified string against the specification
 * for an LCFG context expression. See @c lcfgcontext_valid_expression
 * for details.
 *
 * @param[in] ctx String to be tested
 *
 * @return boolean which indicates if string is a valid context
 *
 */

bool lcfgpackage_valid_context( const char * ctx ) {
  char * msg = NULL;
  bool valid = lcfgcontext_valid_expression( ctx, &msg );
  free(msg); /* Just ignore any error message */
  return valid;
}

/**
 * @brief Check if the package has a context
 *
 * Checks if the specified @c LCFGPackage currently has a
 * value set for the @e context attribute. 
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return boolean which indicates if a package has a context
 *
 */

bool lcfgpackage_has_context( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return !isempty(pkg->context);
}

/**
 * @brief Get the context for the package
 *
 * This returns the value of the @e context parameter for the @c
 * LCFGPackage. If the package does not currently have a @e
 * context then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e context for the
 * package.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return The context for the package (possibly @c NULL).
 */

const char * lcfgpackage_get_context( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return pkg->context;
}

/**
 * @brief Set the context for the package
 *
 * Sets the value of the @e context parameter for the @c
 * LCFGPackage to that specified. It is important to note
 * that this does NOT take a copy of the string. Furthermore, once the
 * value is set the package assumes "ownership", the memory will be
 * freed if the context is further modified or the package is destroyed.
 *
 * Before changing the value of the @e context to be the new string it
 * will be validated using the @c lcfgpackage_valid_context()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] new_ctx String which is the new context
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_context( LCFGPackage * pkg, char * new_ctx ) {
  assert( pkg != NULL );

  bool ok = false;
  if ( lcfgpackage_valid_context(new_ctx) ) {
    free(pkg->context);

    pkg->context = new_ctx;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/**
 * @brief Add extra context information for the package
 *
 * Adds the extra context information to the value for the @e context
 * parameter in the @c LCFGPackage if it is not already
 * found in the string.
 *
 * If not already present in the existing information a new context
 * string is built which is the combination of any existing string
 * with the new string appended, the strings are combined using @c
 * lcfgcontext_combine_expressions(). The new string is passed to @c
 * lcfgpackage_set_context(), unlike that function this does NOT
 * assume "ownership" of the input string.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] extra_context String which is the additional context
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_add_context( LCFGPackage * pkg,
                              const char * extra_context ) {
  assert( pkg != NULL );

  if ( isempty(extra_context) ) return true;

  char * new_context = NULL;
  if ( isempty(pkg->context) ) {
    new_context = strdup(extra_context);
  } else {
    new_context =
      lcfgcontext_combine_expressions( pkg->context, extra_context );
  }

  bool ok = lcfgpackage_set_context( pkg, new_context );
  if ( !ok )
    free(new_context);

  return ok;
}

/* Derivation */

/**
 * @brief Check if the package has derivation information
 *
 * Checks if the specified @c LCFGPackage currently has a
 * value set for the @e derivation attribute. 
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return boolean which indicates if a package has a derivation
 *
 */

bool lcfgpackage_has_derivation( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return !isempty(pkg->derivation);
}

/**
 * @brief Get the derivation for the package
 *
 * This returns the value of the @e derivation parameter for the
 * @c LCFGPackage. If the package does not currently have a
 * @e derivation then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e derivation for the
 * package.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return The derivation for the package (possibly NULL).
 */

const char * lcfgpackage_get_derivation( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return pkg->derivation;
}

/**
 * @brief Set the derivation for the package
 *
 * Sets the value of the @e derivation parameter for the
 * @c LCFGPackage to that specified. It is important to note
 * that this does NOT take a copy of the string. Furthermore, once the
 * value is set the package assumes "ownership", the memory will be
 * freed if the derivation is further modified or the package is destroyed.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] new_deriv String which is the new derivation
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_derivation( LCFGPackage * pkg, char * new_deriv ) {
  assert( pkg != NULL );

  /* Currently no validation of the derivation */

  free(pkg->derivation);

  pkg->derivation = new_deriv;
  return true;
}

/**
 * @brief Add extra derivation information for the package
 *
 * Adds the extra derivation information to the value for the @e
 * derivation parameter in the @c LCFGPackage if it is not
 * already found in the string.
 *
 * If not already present in the existing information a new derivation
 * string is built which is the combination of any existing string
 * with the new string appended. The new string is passed to @c
 * lcfgpackage_set_derivation(), unlike that function this does NOT
 * assume "ownership" of the input string.

 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] extra_deriv String which is the additional derivation
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_add_derivation( LCFGPackage * pkg,
                                 const char * extra_deriv ) {
  assert( pkg != NULL );

  if ( isempty(extra_deriv) ) return true;

  char * new_deriv = NULL;
  if ( isempty(pkg->derivation) ) {
    new_deriv = strdup(extra_deriv);
  } else if ( strstr( pkg->derivation, extra_deriv ) == NULL ) {

    /* Only adding the derivation when it is not present in the
       current value. This avoids unnecessary duplication. */

    new_deriv =
      lcfgutils_string_join( " ", pkg->derivation, extra_deriv );
    if ( new_deriv == NULL ) {
      perror( "Failed to build LCFG derivation" );
      exit(EXIT_FAILURE);
    }

  }

  /* If the extra derivation does not need to be added (since it is
     already present) then new_deriv will be NULL which means ok needs
     to be true. */

  bool ok = true;
  if ( new_deriv != NULL ) {
    ok = lcfgpackage_set_derivation( pkg, new_deriv );

    if ( !ok )
      free(new_deriv);
  }

  return ok;
}

/* Priority */

/**
 * @brief Get the priority for the package
 *
 * This returns the value of the integer @e priority parameter for the
 * @c LCFGPackage. The priority is calculated using the context
 * expression for the package (if any) along with the current active
 * set of contexts for the system.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return The priority for the package.
 */

int lcfgpackage_get_priority( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return pkg->priority;
}

/**
 * @brief Set the priority for the package
 *
 * Sets the value of the @e priority parameter for the @c LCFGPackage
 * to that specified. 
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] new_prio Integer which is the new priority
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_priority( LCFGPackage * pkg, int new_prio ) {
  assert( pkg != NULL );

  pkg->priority = new_prio;
  return true;
}


/**
 * @brief Evaluate the priority for the package for a list of contexts
 *
 * This will evaluate and update the value of the @e priority
 * attribute for the LCFG package using the value set for the @e
 * context attribute (if any) and the list of LCFG contexts passed in
 * as an argument. The priority is evaluated using @c
 * lcfgctxlist_eval_expression().
 *
 * The default value for the priority is zero, if the package is
 * applicable for the specified list of contexts the priority will be
 * positive otherwise it will be negative.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 * @param[in] ctxlist List of LCFG contexts
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_eval_priority( LCFGPackage * pkg,
                                const LCFGContextList * ctxlist,
				char ** msg ) {
  assert( pkg != NULL );

  bool ok = true;

  int priority = 0;
  if ( !isempty(pkg->context) ) {

    /* Calculate the priority using the context expression for this
       package spec. */

    ok = lcfgctxlist_eval_expression( ctxlist,
                                      pkg->context,
                                      &priority, msg );

  }

  if (ok)
    ok = lcfgpackage_set_priority( pkg, priority );

  return ok;
}

/**
 * @brief Check if the package is considered to be active
 *
 * Checks if the current value for the @e priority attribute in the
 * @c LCFGPackage is greater than or equal to zero.
 *
 * The priority is calculated using the value for the @e context
 * attribute and the list of currently active contexts, see
 * @c lcfgpackage_eval_priority() for details.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return boolean indicating if the package is active
 *
 */

bool lcfgpackage_is_active( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  return ( pkg->priority >= 0 );
}

/* Higher-level functions */

/**
 * @brief Get the full version for the resource
 *
 * Combines the version and release strings for the package using a
 * '-' (hyphen) separator to create a new "full version" string. If
 * either of the version or release attributes is empty then the
 * wildcard '*' (asterisk) character will be used. When this string is
 * no longer required it must be freed.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return Pointer to new string (call @c free() when no longer required)
 *
 */

char * lcfgpackage_full_version( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  const char * v = or_default( pkg->version, LCFG_PACKAGE_WILDCARD );
  const char * r = or_default( pkg->release, LCFG_PACKAGE_WILDCARD );

  char * full_version = lcfgutils_string_join( "-", v, r );

  if ( full_version == NULL ) {
    perror( "Failed to build LCFG package full-version string" );
    exit(EXIT_FAILURE);
  }

  return full_version;
}

/**
 * @brief Get an identifier for the resource
 *
 * Combines the @c name and @c arch (if any) strings for the package
 * using a '.' (period) separator to create a new "identifier"
 * string. If the architecture attribute is empty then this will just
 * return a copy of the name. If the package does not have a value for
 * the name then this will return a @c NULL value. When this string is
 * no longer required it must be freed.
 *
 * @param[in] pkg Pointer to an @c LCFGPackage
 *
 * @return Pointer to new string (call @c free() when no longer required)
 *
 */

char * lcfgpackage_id( const LCFGPackage * pkg ) {
  assert( pkg != NULL );

  char * id = NULL;

  const char * name = pkg->name;
  if ( !isempty(name) ) {

    const char * arch = pkg->arch;
    if ( isempty(arch) ) {
      id = strdup(name);
    } else {
      id = lcfgutils_string_join( ".", name, arch );

      if ( id == NULL ) {
	perror( "Failed to build LCFG package ID string" );
	exit(EXIT_FAILURE);
      }

    }
  }

  return id;
}

static bool walk_forwards_until( const char ** start,
                                 char separator, const char * stop,
                                 char ** field_value ) {

  *field_value = NULL;

  const char * begin = *start;
  const char * end   = NULL;

  /* Ignore leading whitespace */

  while ( *begin != '\0' && isspace(*begin) ) begin++;

 /* Walk forwards looking for the separator */

  const char * ptr;
  for ( ptr = begin; *ptr != '\0'; ptr++ ) {
    if ( *ptr == separator ) {
      end = ptr;
      break;
    } else if ( stop != NULL && strchr( stop, *ptr ) ) { 
      break;
    }
  }

  bool found = false;
  if ( end != NULL ) {
    found = true;

    *start = end + 1;

    if ( begin != end ) {

      end--; /* step backward from separator */

      size_t field_len = end - begin + 1;

      while ( field_len > 0 && isspace( *( begin + field_len - 1 ) ) ) field_len--;

      if ( field_len > 0 )
        *field_value = strndup( begin, field_len );

    }

  }

  return found;
}


static bool walk_backwards_until( const char * input, size_t * len,
                                  char separator, const char * stop,
                                  char ** field_value ) {

  *field_value = NULL;

  size_t my_len = *len;

  /* Ignore trailing whitespace */

  while ( my_len > 0 && isspace( *( input + my_len - 1 ) ) ) my_len--;

  const char * begin = NULL;
  const char * end   = my_len > 0 ? input + my_len - 1 : input;

  /* Walk backwards looking for the separator */

  const char * ptr;
  for ( ptr = end; ptr - input >= 0; ptr-- ) {
    if ( *ptr == separator ) {
      begin = ptr;
      break;
    } else if ( stop != NULL && strchr( stop, *ptr ) ) {
      break;
    }
  }

  bool found = false;

  if ( begin != NULL ) {
    found = true;
    *len = begin - input;

    if ( begin != end ) {

      begin++; /* step forward from separator */

      /* Ignore any leading whitespace */

      while ( *begin != '\0' && isspace(*begin) ) begin++;

      size_t field_len = end - begin + 1;
      if ( field_len > 0 )
        *field_value = strndup( begin, field_len );

    }

  }

  return found;
}

/**
 * @brief Create a new package from a string
 *
 * This parses an LCFG package specification as a string and creates a
 * new @c LCFGPackage. The specification is expected to be of the
 * following form: @c PrefixArch2/Name-Version-Release/Arch1:Flags[Context]
 *
 *   - @b Name - required - must match @c [A-Za-z][A-Za-z0-9_]*
 *   - @b Architecture - optional - must match @c [a-zA-Z0-9_-]+ - for
 *     historical reasons it can be specified in one of two ways, if
 *     both are specified then @c arch2 is preferred over @c arch1.
 *   - @b Version - required - must not contain a hyphen or whitespace,
 *     may be a '*' wildcard.
 *   - @b Release - required - must not contain a hyphen or whitespace,
 *     may be a '*' wildcard.
 *   - @b Prefix - optional - single character in @c [+-?=~] 
 *   - @b Flags - optional - must match @c [a-zA-Z0-9]
 *   - @b Context - optional - The contexts for which the package is
 *     applicable, must be valid according to the
 *     @c lcfgcontext_valid_expression() function.
 *
 * To avoid memory leaks, when the newly created package structure is
 * no longer required you should call the @c lcfgpackage_relinquish()
 * function.
 *
 * @param[in] input The package specification string.
 * @param[out] result Reference to the pointer for the new @c LCFGPackage.
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgpackage_from_spec( const char * input,
                                    LCFGPackage ** result,
                                    char ** msg ) {

  *result = NULL;
  *msg = NULL;

  if ( input == NULL ) return invalid_package( msg, "empty spec string" );

  /* Move past any leading whitespace */

  const char * start = input;
  while ( *start != '\0' && isspace(*start) ) start++;

  if ( *start == '\0' ) return invalid_package( msg, "empty spec string" );

  bool ok = true;
  LCFGPackage * pkg = lcfgpackage_new();

  /* Prefix - optional */

  /* If first char is not a word character assume it is intended to be
     a prefix */

  char first_char = *start;
  if ( !isword(first_char) ) {

    if ( !lcfgpackage_set_prefix( pkg, first_char ) ) {
      ok = false;
      invalid_package( msg,  "bad prefix '%c'", first_char );
      goto failure;
    }

    start++;
  }

  /* Secondary Architecture - optional

   If used this is at the front of the string and is separated from
   the name using a '/' (forward slash). If we find a '-' (hyphen) we
   give up as that is likely to be the separator between name and
   version.

   This vallue will get handled when the primary architecture is found.

  */

  char * pkg_arch = NULL;
  walk_forwards_until( &start, '/', "-", &pkg_arch );

  /* Find the end of the string, ignoring any trailing whitespace */

  size_t len = strlen(start);
  while ( len > 0 && isspace( *( start + len - 1 ) ) ) len--;

  /* Context - optional 

     If used this is at the end of the string and is surrounded by '['
     and ']' (square brackets).

  */

  const char * ctx_end = len > 0 ? start + len - 1 : start;
  if ( *ctx_end == ']' ) {
    size_t ctx_len = len - 1;

    char * pkg_context = NULL;

    if ( walk_backwards_until( start, &ctx_len, '[', NULL, &pkg_context ) ) {
      len = ctx_len;

      if ( pkg_context != NULL ) {

        ok = lcfgpackage_set_context( pkg, pkg_context );
        if (!ok) {
          invalid_package( msg, "bad context '%s'", pkg_context );
          free(pkg_context);
          goto failure;
        }
      }

    }

  }

  /* Flags - optional 

   If used this is at the end of the string immediately before any
   context and after the architecture and version. It is separated
   from the previous field using a ':' (colon).

   Whilst walking backwards to search for the separator this will give
   up if a '/' (forward slash - the arch separator) or '-' (hyphen -
   version/release separator) character is found.

  */

  char * pkg_flags = NULL;
  walk_backwards_until( start, &len, ':', "/-", &pkg_flags );

  if ( pkg_flags != NULL ) {

    ok = lcfgpackage_set_flags( pkg, pkg_flags );
    if (!ok) {
      invalid_package( msg, "bad flags '%s'", pkg_flags );
      free(pkg_flags);
      pkg_flags = NULL;
      goto failure;
    }

  }
  
  /* Primary Architecture - optional

     If used this will be at the end of the string after the release
     field. There is a '/' (forward slash) separator.

     If both the primary and secondary architecture have been
     specified then the secondary value will be used.

 */

  char * arch2 = NULL;
  walk_backwards_until( start, &len, '/', NULL, &arch2 );

  if ( pkg_arch == NULL ) {
    pkg_arch = arch2;
  } else {
    free(arch2);
    arch2 = NULL;
  }

  if ( pkg_arch != NULL ) {

    ok = lcfgpackage_set_arch( pkg, pkg_arch );
    if (!ok) {
      invalid_package( msg, "bad architecture '%s'", pkg_arch );
      free(pkg_arch);
      pkg_arch = NULL;
      goto failure;
    }

  }

  /* Release - required

     This is separated from the version field using a '-' (hyphen) character.
  */

  char * pkg_release = NULL;
  walk_backwards_until( start, &len, '-', NULL, &pkg_release );

  if ( pkg_release == NULL ) {
    ok = false;
    invalid_package( msg, "failed to extract release" );
    goto failure;
  } else {

    ok = lcfgpackage_set_release( pkg, pkg_release );

    if (!ok) {
      invalid_package( msg, "bad release '%s'", pkg_release );
      free(pkg_release);
      pkg_release = NULL;
      goto failure;
    }

  }

  /* Version - required

     This is separated from the version and name fields using a '-'
     (hyphen) character.
 */

  char * pkg_version = NULL;
  walk_backwards_until( start, &len, '-', NULL, &pkg_version );

  if ( pkg_version == NULL ) {
    ok = false;
    invalid_package( msg, "failed to extract version" );
    goto failure;
  } else {

    ok = lcfgpackage_set_version( pkg, pkg_version );

    if (!ok) {
      invalid_package( msg, "bad version '%s'", pkg_version );
      free(pkg_version);
      pkg_version = NULL;
      goto failure;
    }

  }

  /* Name - required

   This is using anything which is left after the other fields are discovered.

  */

  if ( len == 0 ) {
    ok = false;
    invalid_package( msg, "failed to extract name" );
    goto failure;
  } else {
    char * pkg_name = strndup( start, len );

    ok = lcfgpackage_set_name( pkg, pkg_name );

    if ( !ok ) {
      invalid_package( msg, "bad name '%s'", pkg_name );
      free(pkg_name);
      goto failure;
    }

  }

 failure:

  if ( !ok ) {
    lcfgpackage_destroy(pkg);
    pkg = NULL;
  }

  *result = pkg;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/**
 * @brief Format the package as an LCFG specification
 *
 * Generates a new string representation of the @c LCFGPackage. For
 * details of the format see the documentation for the
 * @c lcfgpackage_from_spec() function. Note that this function will
 * never generate strings which contain a secondary architecture
 * field. The package must have a name, if either of the version and
 * release attributes are not specified then a wildcard '*' character
 * will be used. If there is a value for the architecture it will only
 * be used when it is different from the value (if any) specified for
 * the default architecture.
 *
 * The following options are supported:
 *   - @c LCFG_OPT_NOPREFIX - Do not include any prefix character.
 *   - @c LCFG_OPT_NOCONTEXT - Do not include any context information.
 *   - @c LCFG_OPT_NEWLINE - Add a newline at the end of the string.
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
 * longer required. If an error occurs this function will return -1.
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

ssize_t lcfgpackage_to_spec( LCFG_PKG_TOSTR_ARGS ) {
  assert( pkg != NULL );

  if ( !lcfgpackage_is_valid(pkg) ) return -1;

  /* This is rather long-winded but it is done this way to avoid
     multiple calls to realloc. It is actually the cheapest way to
     assemble the package spec string. */

  /* Calculate the required space */

  /* name, version and release always have values */

  const char * pkgnam = pkg->name;
  size_t pkgnamlen    = strlen(pkgnam);

  const char * pkgver = or_default( pkg->version, LCFG_PACKAGE_WILDCARD );
  size_t pkgverlen    = strlen(pkgver);

  const char * pkgrel = or_default( pkg->release, LCFG_PACKAGE_WILDCARD );
  size_t pkgrellen    = strlen(pkgrel);

  /* +2 for the two '-' separators */
  size_t new_len = pkgnamlen + pkgverlen + pkgrellen + 2;

  /* prefix can be disabled */

  char pkgpfx = '\0';
  if ( !(options&LCFG_OPT_NOPREFIX) &&
       lcfgpackage_has_prefix(pkg) ) {

    pkgpfx = pkg->prefix;
    new_len++;
  }

  const char * pkgarch = NULL;
  size_t pkgarchlen = 0;
  if ( lcfgpackage_has_arch(pkg) ) {

    /* Not added to the spec when same as default architecture */
    if ( isempty(defarch) ||
         strcmp( pkg->arch, defarch ) != 0 ) {

      pkgarch = pkg->arch;
      pkgarchlen = strlen(pkgarch);
      new_len += ( pkgarchlen + 1 ); /* +1 for '/' separator */
    }

  }

  const char * pkgflgs = pkg->flags;
  size_t pkgflgslen = 0;
  if ( !isempty(pkgflgs) ) {
    pkgflgslen = strlen(pkgflgs);
    new_len += ( pkgflgslen + 1 ); /* +1 for ':' separator */
  }

  const char * pkgctx = pkg->context;
  size_t pkgctxlen = 0;
  if ( !(options&LCFG_OPT_NOCONTEXT) && !isempty(pkgctx) ) {
    pkgctxlen = strlen(pkgctx);
    new_len += ( pkgctxlen + 2 ); /* +2 for '[' and ']' brackets */
  }

  if ( options&LCFG_OPT_NEWLINE )
    new_len += 1;

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    *size = new_len + 1;

    *result = realloc( *result, ( *size * sizeof(char) ) );
    if ( *result == NULL ) {
      perror("Failed to allocate memory for LCFG package string");
      exit(EXIT_FAILURE);
    }

  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  /* optional prefix */
  if ( pkgpfx != '\0' ) {
    *to = pkgpfx;
    to++;
  }

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

  /* arch */
  if ( pkgarch != NULL ) {
    *to = '/';
    to++;

    to = stpncpy( to, pkgarch, pkgarchlen );
  }

  /* flags */
  if ( pkgflgslen > 0 ) {
    *to = ':';
    to++;

    to = stpncpy( to, pkgflgs, pkgflgslen );
  }

  /* context */
  if ( pkgctxlen > 0 ) {
    *to = '[';
    to++;

    to = stpncpy( to, pkgctx, pkgctxlen );

    *to = ']';
    to++;
  }

  if ( options&LCFG_OPT_NEWLINE )
    to = stpncpy( to, "\n", 1 );

  *to = '\0';

  assert( ( *result + new_len ) == to ) ;

  return new_len;
}

/**
 * @brief Summarise the package information
 *
 * Summarises the @c LCFGPackage as a string in the verbose key-value
 * style used by the qxpack tool. The output will look something like:
 *
\verbatim
lcfg-client:
 version=3.3.2-1
    arch=noarch
  derive=/var/lcfg/conf/server/releases/develop/core/packages/lcfg/lcfg_el7_lcfg.rpms:13
\endverbatim
 *
 * The following options are supported:
 *   - @c LCFG_OPT_USE_META - Include any derivation and context information.
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
 * longer required. If an error occurs this function will return -1.
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

ssize_t lcfgpackage_to_summary( LCFG_PKG_TOSTR_ARGS ) {
  assert( pkg != NULL );

  if ( !lcfgpackage_is_valid(pkg) ) return -1;

  static const char * format = " %7s=%s\n";
  size_t base_len = 10; /* 1 for indent + 7 for key + 1 for '=' +1 for newline */

  const char * pkgnam = pkg->name;
  size_t pkgnamlen    = strlen(pkgnam);

  size_t new_len = pkgnamlen + 2; /* +1 for ':' +1 for newline */

  /* Full version string (combination of version and release) - needs
     to be freed afterwards */

  char * pkgver    = lcfgpackage_full_version(pkg);
  size_t pkgverlen = strlen(pkgver);

  new_len += ( pkgverlen + base_len );

  const char * pkgarch = or_default( pkg->arch, defarch );
  size_t pkgarchlen = 0;

  if ( !isempty(pkgarch) ) {
    pkgarchlen = strlen(pkgarch);
    new_len += ( pkgarchlen + base_len );
  }

  /* Optional meta-data */

  const char * derivation = NULL;
  size_t deriv_len = 0;

  const char * context = NULL;
  size_t ctx_len = 0;
  
  if ( options&LCFG_OPT_USE_META ) {

    derivation = pkg->derivation;
    if ( !isempty(derivation) ) {
      deriv_len = strlen(derivation);
      new_len += ( deriv_len + base_len );
    }

    context = pkg->context;
    if ( !isempty(context) ) {
      ctx_len = strlen(context);
      new_len += ( ctx_len + base_len );
    }

  }

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    *size = new_len + 1;

    *result = realloc( *result, ( *size * sizeof(char) ) );
    if ( *result == NULL ) {
      perror("Failed to allocate memory for LCFG package string");
      exit(EXIT_FAILURE);
    }

  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  /* Name */

  to = stpncpy( to, pkgnam, pkgnamlen );
  to = stpncpy( to, ":\n", 2 );

  int rc;

  /* Version */

  rc = sprintf( to, format, "version", pkgver );
  to += rc;

  free(pkgver);
  pkgver = NULL;

  /* Architecture */

  if ( pkgarchlen > 0 ) {
    rc = sprintf( to, format, "arch", pkgarch );
    to += rc;
  }

  /* Derivation */

  if ( deriv_len > 0 ) {
    rc = sprintf( to, format, "derive", derivation );
    to += rc;
  }

  /* Context */

  if ( ctx_len > 0 ) {
    rc = sprintf( to, format, "context", context );
    to += rc;
  }

  *to = '\0';

  assert( ( *result + new_len ) == to ) ;

  return new_len;
}

static const char meta_start[]     = "#ifdef INCLUDE_META\n";
static const char meta_end[]       = "#endif\n";
static const char pragma_derive[]  = "#pragma LCFG derive \"";
static const char pragma_context[] = "#pragma LCFG context \"";
static const char pragma_end[]     = "\"\n";

static const size_t meta_start_len     = sizeof(meta_start) - 1;
static const size_t meta_end_len       = sizeof(meta_end) - 1;
static const size_t pragma_derive_len  = sizeof(pragma_derive) - 1;
static const size_t pragma_context_len = sizeof(pragma_context) - 1;
static const size_t pragma_end_len     = sizeof(pragma_end) - 1;

/**
 * @brief Format the package as CPP
 *
 * Generates a new C Preprocessor (CPP) representation of the
 * @c LCFGPackage. This is used by the LCFG client when generating the
 * rpmcfg file which is used as input for the updaterpms tool. The
 * package must have a name, if either of the version and release
 * attributes are not specified then a wildcard '*' character will be
 * used. If there is a value for the architecture it will only
 * be used when it is different from the value (if any) specified for
 * the default architecture.
 *
 * The following options are supported:
 *   - @c LCFG_OPT_USE_META - Include any derivation and context information.
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
 * longer required. If an error occurs this function will return -1.
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

ssize_t lcfgpackage_to_cpp( LCFG_PKG_TOSTR_ARGS ) {
  assert( pkg != NULL );

  unsigned int spec_options = ( options            |
                                LCFG_OPT_NOCONTEXT |
                                LCFG_OPT_NOPREFIX  |
                                LCFG_OPT_NEWLINE );

  ssize_t spec_len = lcfgpackage_to_spec( pkg, defarch,
                                            spec_options,
                                            result, size );

  if ( spec_len < 0 ) return spec_len;

  /* Meta-Data */

  size_t meta_len = 0;

  const char * derivation = NULL;
  size_t deriv_len = 0;

  const char * context = NULL;
  size_t ctx_len = 0;

  if ( options&LCFG_OPT_USE_META ) {

    derivation = pkg->derivation;
    if ( !isempty(derivation) ) {
      deriv_len = strlen(derivation);
      meta_len += ( pragma_derive_len + deriv_len + pragma_end_len );
    }

    context = pkg->context;
    if ( !isempty(context) ) {
      ctx_len = strlen(context);
      meta_len += ( pragma_context_len + ctx_len + pragma_end_len );
    }

  }

  /* If there is no metadata (i.e. length is zero) then there is
     no more work to do. */

  if ( meta_len == 0 ) return spec_len;

  meta_len += ( meta_start_len + meta_end_len );

  size_t new_len = spec_len + meta_len;

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    *size = new_len + 1;

    *result = realloc( *result, ( *size * sizeof(char) ) );
    if ( *result == NULL ) {
      perror("Failed to allocate memory for LCFG package string");
      exit(EXIT_FAILURE);
    }

  }

  /* Always initialise the characters of the space after the spec to nul */

  memset( *result + spec_len, '\0', *size - spec_len );

  /* Shuffle the spec across to create the necessary space for the
     meta-data. */

  memmove( *result + meta_len, *result, spec_len );

  char * to = *result;

  to = stpncpy( to, meta_start, meta_start_len );

  /* Derivation */

  if ( deriv_len > 0 ) {
    to = stpncpy( to, pragma_derive, pragma_derive_len );

    to = stpncpy( to, derivation, deriv_len );

    to = stpncpy( to, pragma_end, pragma_end_len );
  }

  /* Context */

  if ( ctx_len > 0 ) {
    to = stpncpy( to, pragma_context, pragma_context_len );

    to = stpncpy( to, context, ctx_len );

    to = stpncpy( to, pragma_end, pragma_end_len );
  }

  to = stpncpy( to, meta_end, meta_end_len );

  assert( ( to + spec_len ) == ( *result + new_len ) );

  return new_len;
}

/**
 * @brief Format the package as XML
 *
 * Generates a new XML representation of the @c LCFGPackage. This is
 * used by the LCFG server when generating the XML profile file which
 * is used as input for the LCFG client. 
 *
 * The package must have a name, if either of the version and release
 * attributes are not specified then a wildcard '*' character will be
 * used. If there is a value for the architecture it will only
 * be used when it is different from the value (if any) specified for
 * the default architecture.
 *
 * The following options are supported:
 *   - @c LCFG_OPT_USE_META - Include any derivation and context information.
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
 * longer required. If an error occurs this function will return -1.
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

ssize_t lcfgpackage_to_xml( LCFG_PKG_TOSTR_ARGS ) {
  assert( pkg != NULL );

  if ( !lcfgpackage_is_valid(pkg) ) return -1;

  static const char indent[] = "   ";
  static const size_t indent_len = sizeof(indent) - 1;

  size_t new_len = indent_len + 20; /* <package></package> + newline */

  /* Name - required */

  const char * name = pkg->name;
  size_t name_len = strlen(name);

  new_len += ( name_len + 13 ); /* <name></name> */

  /* Version - required */

  const char * version = or_default( pkg->version, LCFG_PACKAGE_WILDCARD );
  size_t ver_len = strlen(version);

  new_len += ( ver_len + 7 ); /* <v></v> */

  /* Release - required (and possibly architecture) */

  const char * release = or_default( pkg->release, LCFG_PACKAGE_WILDCARD );
  size_t rel_len = strlen(release);

  new_len += ( rel_len + 7 ); /* <r></r> */

  const char * arch = NULL;
  size_t arch_len = 0;
  if ( !isempty(pkg->arch) ) {

    /* Not added to the spec when same as default architecture */
    if ( isempty(defarch) ||
         strcmp( pkg->arch, defarch ) != 0 ) {

      arch = pkg->arch;
      arch_len = strlen(arch);
      new_len += ( arch_len + 1 ); /* +1 for '/' separator */
    }

  }

  /* Flags - optional */

  const char * flags = pkg->flags;
  size_t flags_len = 0;
  if ( !isempty(flags) ) {
    flags_len = strlen(flags);
    new_len += ( flags_len + 19 ); /* <options></options> */
  }

  /* Context - optional */

  const char * context = NULL;
  size_t ctx_len = 0;

  /* Derivation - optional */

  const char * derivation = NULL;
  size_t deriv_len = 0;

  if ( options&LCFG_OPT_USE_META ) {

    context = pkg->context;
    if ( !isempty(context) ) {
      ctx_len = strlen(context);
      new_len += ( ctx_len + 15 );   /* ' cfg:context=""' */
    }

    derivation = pkg->derivation;
    if ( !isempty(derivation) ) {
      deriv_len = strlen(derivation);
      new_len += ( deriv_len + 18 ); /* ' cfg:derivation=""' */
    }

  }

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    *size = new_len + 1;

    *result = realloc( *result, ( *size * sizeof(char) ) );
    if ( *result == NULL ) {
      perror("Failed to allocate memory for LCFG package string");
      exit(EXIT_FAILURE);
    }

  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  char * to = *result;

  /* Build the new string */

  /* Initial indentation */

  if ( indent_len > 0 )
    to = stpncpy( to, indent, indent_len );

  /* Opening package tag */

  to = stpncpy( to, "<package", 8 );

  /* Optional context attribute */

  if ( ctx_len > 0 ) {
    to = stpncpy( to, " cfg:context=\"", 14 );

    to = stpncpy( to, context, ctx_len );

    *to = '"';
    to++;
  }

  /* Optional derivation attribute */

  if ( deriv_len > 0 ) {
    to = stpncpy( to, " cfg:derivation=\"", 17 );

    to = stpncpy( to, derivation, deriv_len );

    *to = '"';
    to++;
  }

  /* Close the <package> tag */
  *to = '>';
  to++;

  /* Name */

  to = stpncpy( to, "<name>", 6 );
  to = stpncpy( to, name, name_len );
  to = stpncpy( to, "</name>", 7 );

  /* Version */

  to = stpncpy( to, "<v>", 6 );
  to = stpncpy( to, version, ver_len );
  to = stpncpy( to, "</v>", 4 );

  /* Release */

  to = stpncpy( to, "<r>", 6 );
  to = stpncpy( to, release, rel_len );

  /* Architecture - optional */

  if ( arch_len > 0 ) {
    *to = '/';
    to++;

    to = stpncpy( to, arch, arch_len );
  }

  to = stpncpy( to, "</r>", 4 );

  /* Flags - optional */

  if ( flags_len > 0 ) {
    to = stpncpy( to, "<options>", 9 );
    to = stpncpy( to, flags, flags_len );
    to = stpncpy( to, "</options>", 10 );
  }

  /* Closing package tag */

  to = stpncpy( to, "</package>\n", 11 );

  *to = '\0';

  assert( ( *result + new_len ) == to );

  return new_len;

}

/**
 * @brief Test if package matches name and architecture
 *
 * This compares the @e name and @e arch of the @c LCFGPackage with
 * the specified strings.
 *
 * @param[in] pkg Pointer to @c LCFGPackage
 * @param[in] name The name to check for a match
 * @param[in] arch The architecture to check for a match
 *
 * @return boolean indicating equality of values
 *
 */

bool lcfgpackage_match( const LCFGPackage * pkg,
			const char * name, const char * arch ) {
  assert( pkg != NULL );
  assert( name != NULL );

  if ( arch == NULL )
    arch = "";

  const char * pkg_name = or_default( pkg->name, "" );
  bool match = ( strcmp( pkg_name, name ) == 0 );

  if ( match ) {
    const char * pkg_arch = or_default( pkg->arch, "" );
    match = ( strcmp( pkg_arch, arch ) == 0 );
  }

  return match;
}

int compare_vstrings( const char * v1, const char * v2 ) {

  bool v1_isempty = isempty(v1);
  bool v2_isempty = isempty(v2);

  int result = 0;

  /* empty compares as "less than" any non-empty value */

  if ( v1_isempty || v2_isempty ) {

    if ( v1_isempty ) {
      if ( !v2_isempty)
        result = -1;
    } else {
      if ( v2_isempty )
        result = 1;
    }

  } else {

    bool v1_iswild  = ( strcmp( v1, LCFG_PACKAGE_WILDCARD ) == 0 );
    bool v2_iswild  = ( strcmp( v2, LCFG_PACKAGE_WILDCARD ) == 0 );

    /* wild compares as "less than" any non-wild value */

    if ( v1_iswild || v2_iswild ) {

      if ( v1_iswild ) {
        if ( !v2_iswild)
          result = -1;
      } else {
        if ( v2_iswild )
          result = 1;
      }

    } else { /* neither is empty or wild */

#ifdef HAVE_RPMLIB
      result = rpmvercmp( v1, v2 );
#else
      result = strcmp( v1, v2 );
#endif

    }

  }

  return result;
}

/**
 * @brief Compare the package versions
 *
 * This compares the versions and releases for two packages, this is
 * mostly useful for sorting lists of packages. An integer value is
 * returned which indicates lesser than, equal to or greater than in
 * the same way as @c strcmp(3).
 *
 * Comparison rules are:
 *   - Empty is "less than" non-empty
 *   - Wild is "less than" non-empty non-wild
 *   - When rpmlib is available the @c rpmvercmp() function is used
 *   - Fall back to simple @c strcmp(3)
 *
 * @param[in] pkg1 Pointer to @c LCFGPackage
 * @param[in] pkg2 Pointer to @c LCFGPackage
 * 
 * @return Integer (-1,0,+1) indicating lesser,equal,greater
 *
 */

int lcfgpackage_compare_versions( const LCFGPackage * pkg1,
                                  const LCFGPackage * pkg2 ) {
  assert( pkg1 != NULL );
  assert( pkg2 != NULL );

  int result = compare_vstrings( pkg1->version, pkg2->version );

  if ( result == 0 )
    result = compare_vstrings( pkg1->release, pkg2->release );

  return result;
}

/**
 * @brief Compare the package names
 *
 * This compares the names for two packages, this is mostly useful for
 * sorting lists of packages. The names are compared in a
 * case-independent way using the standard @c strcasecmp(3)
 * function. If either package does not have a name then the empty
 * string will be used.
 *
 * @param[in] pkg1 Pointer to @c LCFGPackage
 * @param[in] pkg2 Pointer to @c LCFGPackage
 * 
 * @return Integer (-1,0,+1) indicating lesser,equal,greater
 *
 */

int lcfgpackage_compare_names( const LCFGPackage * pkg1,
                               const LCFGPackage * pkg2 ) {
  assert( pkg1 != NULL );
  assert( pkg2 != NULL );

  const char * name1 = or_default( pkg1->name, "" );
  const char * name2 = or_default( pkg2->name, "" );

  return strcasecmp( name1, name2 );
}

/**
 * @brief Compare the package architectures
 *
 * This compares the architectures for two packages, this is mostly
 * useful for sorting lists of packages. The architectures are
 * compared using the standard @c strcmp(3) function. If either
 * package does not have an architecture then the empty string will be
 * used.
 *
 * @param[in] pkg1 Pointer to @c LCFGPackage
 * @param[in] pkg2 Pointer to @c LCFGPackage
 * 
 * @return Integer (-1,0,+1) indicating lesser,equal,greater
 *
 */

int lcfgpackage_compare_archs( const LCFGPackage * pkg1,
                               const LCFGPackage * pkg2 ) {
  assert( pkg1 != NULL );
  assert( pkg2 != NULL );

  const char * arch1 = or_default( pkg1->arch, "" );
  const char * arch2 = or_default( pkg2->arch, "" );

  return strcmp( arch1, arch2 );
}

/**
 * @brief Compare the packages
 *
 * This compares two packages using the name, architecture and
 * versions, this is mostly useful for sorting lists of packages. The
 * comparison uses these functions in the following order:
 *
 *   -  @c lcfgpackage_compare_names()
 *   -  @c lcfgpackage_compare_archs()
 *   -  @c lcfgpackage_compare_versions()
 *
 * with each subsequent function only being called if the previous
 * returned a value of zero to indicate equality.
 *
 * @param[in] pkg1 Pointer to @c LCFGPackage
 * @param[in] pkg2 Pointer to @c LCFGPackage
 * 
 * @return Integer (-1,0,+1) indicating lesser,equal,greater
 *
 */

int lcfgpackage_compare( const LCFGPackage * pkg1,
                         const LCFGPackage * pkg2 ) {
  assert( pkg1 != NULL );
  assert( pkg2 != NULL );

  /* Name */

  int result = lcfgpackage_compare_names( pkg1, pkg2 );

  /* Architecture */

  if ( result == 0 )
    result = lcfgpackage_compare_archs( pkg1, pkg2 );

  /* Version and Release */

  if ( result == 0 )
    result = lcfgpackage_compare_versions( pkg1, pkg2 );

  return result;
}

/**
 * @brief Test for package equality
 *
 * This can be used to test if two packages are considered to be 
 * @e equal. The following fields are compared in this order:
 *
 *   - name - uses @c lcfgpackage_compare_names()
 *   - architecture - uses @c lcfgpackage_compare_archs()
 *   - version - uses @c lcfgpackage_compare_versions()
 *   - flags - uses @c strcmp(3)
 *   - context - uses @c strcmp(3)
 *
 * Note that prefix, derivation and priority are @b NOT compared.
 *
 * @param[in] pkg1 Pointer to @c LCFGPackage
 * @param[in] pkg2 Pointer to @c LCFGPackage
 *
 * @return boolean indicating equality of packages
 *
 */

bool lcfgpackage_equals( const LCFGPackage * pkg1,
                         const LCFGPackage * pkg2 ) {
  assert( pkg1 != NULL );
  assert( pkg2 != NULL );

  /* Name */

  bool equals = ( lcfgpackage_compare_names( pkg1, pkg2 ) == 0 );

  /* Architecture */

  if ( !equals )
    return false;
  else
    equals = ( lcfgpackage_compare_archs( pkg1, pkg2 ) == 0 );

  /* Version and Release */

  if ( !equals )
    return false;
  else
    equals = ( lcfgpackage_compare_versions( pkg1, pkg2 ) == 0 );

  /* Flags */

  if ( !equals ) {
    return false;
  } else {
    const char * flags1 = or_default( pkg1->flags, "" );
    const char * flags2 = or_default( pkg2->flags, "" );

    equals = ( strcmp( flags1, flags2 ) == 0 );
  }

  /* Context */

  if ( !equals ) {
    return false;
  } else {
    const char * ctx1 = or_default( pkg1->context, "" );
    const char * ctx2 = or_default( pkg2->context, "" );

    equals = ( strcmp( ctx1, ctx2 ) == 0 );
  }

  /* The prefix and derivation are NOT compared */

  return equals;
}

/**
 * @brief Format the package as a string
 *
 * Generates a new string representation of the @c LCFGPackage in
 * various styles. The following styles are supported:
 * 
 *   - @c LCFG_PKG_STYLE_XML - uses @c lcfgpackage_to_xml()
 *   - @c LCFG_PKG_STYLE_CPP - uses @c lcfgpackage_to_cpp()
 *   - @c LCFG_PKG_STYLE_RPM - uses @c lcfgpackage_to_rpm_filename()
 *   - @c LCFG_PKG_STYLE_SPEC - uses @c lcfgpackage_to_spec()
 *   - @c LCFG_PKG_STYLE_SUMMARY - uses @c lcfgpackage_to_summary()
 *
 * See the documentation for each function to see which options are
 * supported.
 *
 * These functions use a string buffer which may be pre-allocated if
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
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] pkg Pointer to @c LCFGPackage
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] style Integer indicating required style of formatting
 * @param[in] options Integer that controls formatting
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgpackage_to_string( const LCFGPackage * pkg,
			       const char * defarch,
			       LCFGPkgStyle style,
			       LCFGOption options,
			       char ** result, size_t * size ) {

  assert( pkg != NULL );

  /* Select the appropriate string function */

  LCFGPkgStrFunc str_func;

  switch (style)
    {
    case LCFG_PKG_STYLE_XML:
      str_func = &lcfgpackage_to_xml;
      break;
    case LCFG_PKG_STYLE_CPP:
      str_func = &lcfgpackage_to_cpp;
      break;
    case LCFG_PKG_STYLE_SUMMARY:
      str_func = &lcfgpackage_to_summary;
      break;
    case LCFG_PKG_STYLE_RPM:
      str_func = &lcfgpackage_to_rpm_filename;
      break;
    case LCFG_PKG_STYLE_SPEC:
    default:
      str_func = &lcfgpackage_to_spec;
    }

  return (*str_func)( pkg, defarch, options, result, size );
}

/**
 * @brief Write formatted package to file stream
 *
 * This can be used to write out the package in various formats to the
 * specified file stream which must have already been opened for
 * writing. The following styles are supported:
 * 
 *   - @c LCFG_PKG_STYLE_XML - uses @c lcfgpackage_to_xml()
 *   - @c LCFG_PKG_STYLE_CPP - uses @c lcfgpackage_to_cpp()
 *   - @c LCFG_PKG_STYLE_RPM - uses @c lcfgpackage_to_rpm_filename()
 *   - @c LCFG_PKG_STYLE_SPEC - uses @c lcfgpackage_to_spec()
 *
 * See the documentation for each function to see which options are
 * supported.
 *
 * @param[in] pkg Pointer to @c LCFGPackage
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] style Integer indicating required style of formatting
 * @param[in] options Integer for any additional options
 * @param[in] out Stream to which the package string should be written
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_print( const LCFGPackage * pkg,
                        const char * defarch,
                        LCFGPkgStyle style,
                        LCFGOption options,
                        FILE * out ) {
  assert( pkg != NULL );

  char * lcfgspec = NULL;
  size_t buf_size = 0;

  if ( style == LCFG_PKG_STYLE_RPM || style == LCFG_PKG_STYLE_SPEC )
    options |= LCFG_OPT_NEWLINE;

  ssize_t rc = lcfgpackage_to_string( pkg, defarch, style, options,
				      &lcfgspec, &buf_size );

  bool ok = ( rc >= 0 );

  if (ok ) {

    if ( fputs( lcfgspec, out ) < 0 )
      ok = false;

  }

  free(lcfgspec);

  return ok;
}

/**
 * @brief Assemble a package-specific message
 *
 * This can be used to assemble package-specific message, typically
 * this is used for generating diagnostic error messages.
 *
 * @param[in] pkg Pointer to @c LCFGPackage
 * @param[in] fmt Format string for message (and any additional arguments)
 *
 * @return Pointer to message string (call @c free(3) when no longer required)
 *
 */

char * lcfgpackage_build_message( const LCFGPackage * pkg,
                                  const char *fmt, ... ) {

  /* This is rather messy and probably somewhat inefficient. It is
     intended to be used primarily for generating error messages,
     usually just before failing entirely. */

  char * result = NULL;

  /* First assemble the base of the message using the format string
     and any varargs passed in by the caller */

  char * msg_base;
  va_list ap;

  va_start(ap, fmt);
  /* The BSD version apparently sets ptr to NULL on fail.  GNU loses. */
  if (vasprintf(&msg_base, fmt, ap) < 0)
    msg_base = NULL;
  va_end(ap);

  int rc = 0;

  char * pkg_as_str  = NULL;
  if ( lcfgpackage_is_valid(pkg) ) {
    size_t buf_size = 0;
    if ( lcfgpackage_to_spec( pkg, NULL, LCFG_OPT_NONE,
				&pkg_as_str, &buf_size ) < 0 ) {
      free(pkg_as_str);
      perror("Failed to build LCFG package message");
      exit(EXIT_FAILURE);
    }
  }

  /* Build the most useful summary possible using all the available
     information for the package. */

  char * msg_mid = NULL;
  if ( pkg_as_str != NULL ) {
    rc = asprintf( &msg_mid, "for package '%s'",
                   pkg_as_str );
  } else {
    msg_mid = strdup("for package");
  }

  if ( rc < 0 ) {
    perror("Failed to build LCFG package message");
    exit(EXIT_FAILURE);
  }

  /* Final string, possibly with derivation information */

  if ( pkg != NULL && !isempty(pkg->derivation) ) {
    rc = asprintf( &result, "%s %s at %s",
                   msg_base, msg_mid,
                   pkg->derivation );
  } else {
    rc = asprintf( &result, "%s %s",
                   msg_base, msg_mid );
  }

  if ( rc < 0 ) {
    perror("Failed to build LCFG package message");
    exit(EXIT_FAILURE);
  }

  /* Tidy up */

  free(msg_base);
  free(msg_mid);
  free(pkg_as_str);

  return result;
}

/**
 * @brief Maximum length for an architecture name
 */

#define ARCH_MAXLEN 8

/**
 * @brief Get the default processor architecture
 *
 * This returns the processor architecture (e.g. x86_64 or i686 )
 * according to the @c machine field in the @c utsname structure
 * returned by uname(2). The string will be a maximum of 8 characters
 * in length which should be sufficient for any normal architecture
 * identifier.
 *
 * @return Pointer to default architecture string
 *
 */

const char * default_architecture(void) {

  static char defarch[ARCH_MAXLEN] = "";

  if ( defarch[0] == '\0' ) {
    struct utsname name;
    uname(&name);
    strncpy( defarch, name.machine, ARCH_MAXLEN );
    defarch[ARCH_MAXLEN-1] = '\0';
  }

  return defarch;
}

/**
 * @brief Calculate the hash for a package
 *
 * This will calculate the hash for the package using the values for
 * the @e name and @e arch parameters. It does this using the @c
 * lcfgutils_string_djbhash() function.
 *
 * @param[in] pkg Pointer to @c LCFGPackage
 *
 * @return The hash for the package name and architecture
 *
 */

unsigned long lcfgpackage_hash( const LCFGPackage * pkg ) {
  return lcfgutils_string_djbhash( pkg->name, pkg->arch, NULL );
}

/* eof */
