#define _GNU_SOURCE /* for asprintf */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>

#ifdef HAVE_RPMLIB
#include <rpm/rpmlib.h>
#endif

#include "context.h"
#include "packages.h"
#include "utils.h"

/* Extends the standard alpha-numeric test to include the '_'
   (underscore) character. This is similar to the '\w' in Perl
   regexps, this gives the set of characters [A-Za-z0-9_] */

inline static bool isword( const char chr ) {
  return ( isalnum(chr) || chr == '_' );
}

/* Permit [A-Za-z0-9_-.+] characters in package names */

inline static bool isnamechr( const char chr ) {
  return ( isword(chr) || strchr( "-.+", chr ) != NULL );
}

/* Currently there are five supported prefixes for LCFG package
   specifications:

   +  insert package into list, replaces any existing package of same name/arch
   =  Similar to + but "pins" the version so it cannot be overridden
   -  remove any package from list which matches this name/arch
   ?  replace any existing package in list which matches this name/arch
   ~  Add package to list if name/arch is not already present
*/

static const char * permitted_prefixes = "?+-=~";

/**
 * @brief Create and initialise a new package
 *
 * Creates a new @c LCFGPackage struct and initialises the
 * parameters to the default values.
 *
 * If the memory allocation for the new struct is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * @return Pointer to new @c LCFGPackage struct
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
  pkg->_refcount  = 0;

  return pkg;
}

/**
 * @brief Destroy the package
 *
 * When the specified @c LCFGPackage struct is no longer required
 * this will free all associated memory.
 *
 * *Reference Counting:* There is support for very simple reference
 * counting which allows an @c LCFGPackage struct to appear in
 * multiple lists. Incrementing and decrementing that reference
 * counter is the responsibility of the container code. If the
 * reference count for the specified package is greater than zero
 * this function will have no affect.
 *
 * This will call @c free() on each parameter of the struct (or @c
 * lcfgtemplate_destroy for the template parameter ) and then set each
 * value to be @c NULL.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * package which has already been destroyed (or potentially was never
 * created).
 *
 * @param[in] res Pointer to @c LCFGPackage struct to be destroyed.
 *
 */

void lcfgpackage_destroy(LCFGPackage * pkg) {

  if ( pkg == NULL )
    return;

  if ( pkg->_refcount > 0 )
    return;

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
 * @brief Clone the package
 *
 * Creates a new @c LCFGPackage struct and copies the values of
 * the parameters from the specified package. The values for the
 * parameters are copied (e.g. strings are duplicated using @c
 * strdup() ) so that a later change to a parameter in the source
 * package does not affect the new clone package.
 *
 * If the memory allocation for the new struct is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * @param[in] res Pointer to @c LCFGPackage struct to be cloned.
 *
 * @return Pointer to new @c LCFGPackage struct or NULL if copy fails.
 *
 */

LCFGPackage * lcfgpackage_clone( const LCFGPackage * pkg ) {

  LCFGPackage * clone = lcfgpackage_new();
  if ( clone == NULL )
    return NULL;

  bool ok = true;
  if ( pkg->name != NULL ) {
    char * new_name = strdup(pkg->name);
    ok = lcfgpackage_set_name( clone, new_name );
    if (!ok)
      free(new_name);
  }

  if ( ok && pkg->arch != NULL ) {
    char * new_arch = strdup(pkg->arch);
    ok = lcfgpackage_set_arch( clone, new_arch );
    if (!ok)
      free(new_arch);
  }

  if ( ok && pkg->version != NULL ) {
    char * new_version = strdup(pkg->version);
    ok = lcfgpackage_set_version( clone, new_version );
    if (!ok)
      free(new_version);
  }

  if ( ok && pkg->release != NULL ) {
    char * new_release = strdup(pkg->release);
    ok = lcfgpackage_set_release( clone, new_release );
    if (!ok)
      free(new_release);
  }

  if ( ok && pkg->flags != NULL ) {
    char * new_flags = strdup(pkg->flags);
    ok = lcfgpackage_set_flags( clone, new_flags );
    if (!ok)
      free(new_flags);
  }

  if ( ok && pkg->context != NULL ) {
    char * new_context = strdup(pkg->context);
    ok = lcfgpackage_set_context( clone, new_context );
    if (!ok)
      free(new_context);
  }

  if ( ok && pkg->derivation != NULL ) {
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

  bool valid = ( name  != NULL &&
                 *name != '\0' &&
                 isalnum(*name) );

  char * ptr;
  for ( ptr = (char *) ( name + 1 ); valid && *ptr != '\0'; ptr++ ) {
    if ( !isnamechr(*ptr) )
      valid = false;
  }

  return valid;
}

/**
 * @brief Check if the package has a name
 *
 * Checks if the specified @c LCFGPackage struct currently has a
 * value set for the @e name attribute. Although a name is required
 * for an LCFG package to be valid it is possible for the value of the
 * name to be set to @c NULL when the struct is first created.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return boolean which indicates if a package has a name
 *
 */

bool lcfgpackage_has_name( const LCFGPackage * pkg ) {
  return ( pkg->name != NULL && *( pkg->name ) != '\0' );
}

/**
 * @brief Get the name for the package
 *
 * This returns the value of the @e name parameter for the @c
 * LCFGPackage struct. If the package does not currently have a @e
 * name then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e name for the
 * package.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return The name for the package (possibly NULL).
 */

char * lcfgpackage_get_name( const LCFGPackage * pkg ) {
  return pkg->name;
}

/**
 * @brief Set the name for the package
 *
 * Sets the value of the @e name parameter for the @c LCFGPackage
 * struct to that specified. It is important to note that this does
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
 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] new_name String which is the new name
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_name( LCFGPackage * pkg, char * new_name ) {

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

  bool valid = ( arch  != NULL &&
                 *arch != '\0' );

  /* Permit [a-zA-Z0-9_-] characters */

  char * ptr;
  for ( ptr = (char *) arch; valid && *ptr != '\0'; ptr++ ) {
    if ( !isword(*ptr) && *ptr != '-' )
      valid = false;
  }

  return valid;
}

/**
 * @brief Check if the package has an architecture 
 *
 * Checks if the specified @c LCFGPackage struct currently has a
 * value set for the @e arch attribute. 
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return boolean which indicates if a package has an architecture
 *
 */

bool lcfgpackage_has_arch( const LCFGPackage * pkg ) {
  return ( pkg->arch != NULL && *( pkg->arch ) != '\0' );
}

/**
 * @brief Get the architecture for the package
 *
 * This returns the value of the @e arch parameter for the @c
 * LCFGPackage struct. If the package does not currently have an @e
 * arch then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e arch for the
 * package.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return The architecture for the package (possibly NULL).
 */

char * lcfgpackage_get_arch( const LCFGPackage * pkg ) {
  return pkg->arch;
}

/**
 * @brief Set the architecture for the package
 *
 * Sets the value of the @e arch parameter for the @c LCFGPackage
 * struct to that specified. It is important to note that this does
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
 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] new_arch String which is the new architecture
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_arch( LCFGPackage * pkg, char * new_arch ) {

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

  bool valid = ( version  != NULL &&
                 *version != '\0' );

  /* It is not entirely clear exactly which characters are allowed but
     it is necessary to forbid '-' (hyphen) and any whitespace */

  char * ptr;
  for ( ptr = (char *) version; valid && *ptr != '\0'; ptr++ ) {
    if ( *ptr == '-' || isspace(*ptr) )
      valid = false;
  }

  return valid;
}

/**
 * @brief Check if the package has a version
 *
 * Checks if the specified @c LCFGPackage struct currently has a
 * value set for the @e version attribute. 
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return boolean which indicates if a package has a version
 *
 */

bool lcfgpackage_has_version( const LCFGPackage * pkg ) {
  return ( pkg->version != NULL && *( pkg->version ) != '\0' );
}

/**
 * @brief Get the version for the package
 *
 * This returns the value of the @e version parameter for the @c
 * LCFGPackage struct. If the package does not currently have a @e
 * version then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e version for the
 * package.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return The version for the package (possibly NULL).
 */

char * lcfgpackage_get_version( const LCFGPackage * pkg ) {
  return pkg->version;
}

/**
 * @brief Set the version for the package
 *
 * Sets the value of the @e version parameter for the @c LCFGPackage
 * struct to that specified. It is important to note that this does
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
 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] new_version String which is the new version
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_version( LCFGPackage * pkg, char * new_version ) {

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
 * Checks if the specified @c LCFGPackage struct currently has a
 * value set for the @e release attribute. 
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return boolean which indicates if a package has a release
 *
 */

bool lcfgpackage_has_release( const LCFGPackage * pkg ) {
  return ( pkg->release != NULL && *( pkg->release ) != '\0' );
}

/**
 * @brief Get the release for the package
 *
 * This returns the value of the @e release parameter for the @c
 * LCFGPackage struct. If the package does not currently have a @e
 * release then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e release for the
 * package.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return The release for the package (possibly NULL).
 */

char * lcfgpackage_get_release( const LCFGPackage * pkg ) {
  return pkg->release;
}

/**
 * @brief Set the release for the package
 *
 * Sets the value of the @e release parameter for the @c LCFGPackage
 * struct to that specified. It is important to note that this does
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
 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] new_release String which is the new release
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_release( LCFGPackage * pkg, char * new_release ) {

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
 * Checks if the specified @c LCFGPackage struct currently has a
 * value set for the @e prefix attribute. 
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return boolean which indicates if a package has a prefix
 *
 */

bool lcfgpackage_has_prefix( const LCFGPackage * pkg ) {
  return ( pkg->prefix != '\0' );
}

/**
 * @brief Get the prefix for the package
 *
 * This returns the value of the @e prefix parameter for the @c
 * LCFGPackage struct. If the package does not currently have a @e
 * release then the null character @c '\0' will be returned.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return The prefix character for the package.
 */

char lcfgpackage_get_prefix( const LCFGPackage * pkg ) {
  return pkg->prefix;
}

/**
 * @brief Clear any prefix for the package
 *
 * Removes any prefix which has been previously specified for the
 * package specification. This is done by resetting the @c prefix
 * attribute for the @c LCFGPackage struct to be the null character @c
 * '\0'.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return boolean indicating success
 */

bool lcfgpackage_clear_prefix( LCFGPackage * pkg ) {
    pkg->prefix = '\0';
    return true;
}

/**
 * @brief Set the prefix for the package
 *
 * Sets the value of the @e prefix parameter for the @c LCFGPackage
 * struct to that specified. 
 *
 * Before changing the value of the @e prefix to be the new character
 * it will be validated using the @c lcfgpackage_valid_prefix()
 * function. If the new character is not valid then no change will
 * occur, the @c errno will be set to @c EINVAL and the function will
 * return a @c false value.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] new_prefix Character which is the new prefix
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_prefix( LCFGPackage * pkg, char new_prefix ) {

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

  bool valid = ( flags  != NULL &&
                 *flags != '\0' );

  /* This only permits [a-zA-Z0-9] characters */

  char * ptr;
  for ( ptr = (char *) flags; valid && *ptr != '\0'; ptr++ ) {
    if ( !lcfgpackage_valid_flag_chr(*ptr) )
      valid = false;
  }

  return valid;
}

/**
 * @brief Check if the package has any flags
 *
 * Checks if the specified @c LCFGPackage struct currently has a
 * value set for the @e flags attribute. 
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return boolean which indicates if a package has flags
 *
 */

bool lcfgpackage_has_flags( const LCFGPackage * pkg ) {
  return ( pkg->flags != NULL && *( pkg->flags ) != '\0' );
}

/**
 * @brief Check if the package has a particular flag
 *
 * Checks if the specified @c LCFGPackage struct currently has a
 * particular flag enabled.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] flag Character to check
 *
 * @return boolean which indicates if a package has a flag set
 *
 */

bool lcfgpackage_has_flag( const LCFGPackage * pkg, char flag ) {

  return ( lcfgpackage_has_flags(pkg) &&
           strchr( pkg->flags, flag ) != NULL );
}

/**
 * @brief Get the flags for the package
 *
 * This returns the value of the @e flags parameter for the @c
 * LCFGPackage struct. If the package does not currently have @e
 * flags then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e flags for the
 * package.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return The flags for the package (possibly NULL).
 */

char * lcfgpackage_get_flags( const LCFGPackage * pkg ) {
  return pkg->flags;
}

/**
 * @brief Set the flags for the package
 *
 * Sets the value of the @e flags parameter for the @c LCFGPackage
 * struct to that specified. It is important to note that this does
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
 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] new_flags String which is the new flags
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_flags( LCFGPackage * pkg, char * new_flags ) {

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
 * struct to be NULL
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_clear_flags( LCFGPackage * pkg ) {
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
 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] extra_flags String of extra flags to be added
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_add_flags( LCFGPackage * pkg,
                            const char * extra_flags ) {

  if ( extra_flags == NULL || *extra_flags == '\0' )
    return true;

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
     than LCFG_PKGS_FLAGS_MAXCHAR but that's unlikely to be a huge
     problem. Other invalid characters not in the [a-zA-Z0-9] set will
     be flagged up as a problem when set_flags is called at the end of
     this function.

  */

#define LCFG_PKGS_FLAGS_MAXCHAR 128

  bool char_set[LCFG_PKGS_FLAGS_MAXCHAR] = { false };

  size_t cur_len = 0;
  if ( pkg->flags != NULL ) {
    cur_len = strlen(pkg->flags);

    for ( i=0; i<cur_len; i++ ) {
      int val = (pkg->flags)[i] - '0';
      if ( val < LCFG_PKGS_FLAGS_MAXCHAR ) {
        if ( !char_set[val] ) {
          new_len++;
          char_set[val] = true;
        }
      }

    }

  }

  size_t extra_len = strlen(extra_flags);
  for ( i=0; i<extra_len; i++ ) {
    int val = (extra_flags)[i] - '0';
    if ( val < LCFG_PKGS_FLAGS_MAXCHAR ) {
      if ( !char_set[val] ) {
        new_len++;
        char_set[val] = true;
      }
    }
  }

  /* If the new length is the same as the current then there is
     nothing more to do so just return success */

  if ( new_len == cur_len )
    return true;

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror( "Failed to allocate memory for LCFG package flags" );
    exit(EXIT_FAILURE);
  }

  char * to = result;

  for ( i=0; i<LCFG_PKGS_FLAGS_MAXCHAR; i++ ) {
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
 * @brief Check if a string is a valid LCFG context
 *
 * Checks the contents of a specified string against the specification
 * for an LCFG context. See @c lcfgcontext_valid_expression for details.
 *
 * @param[in] ctx String to be tested
 *
 * @return boolean which indicates if string is a valid context
 *
 */

bool lcfgpackage_valid_context( const char * ctx ) {
  return lcfgcontext_valid_expression(ctx);
}

/**
 * @brief Check if the package has a context
 *
 * Checks if the specified @c LCFGPackage struct currently has a
 * value set for the @e context attribute. 
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return boolean which indicates if a package has a context
 *
 */

bool lcfgpackage_has_context( const LCFGPackage * pkg ) {
  return ( pkg->context != NULL && *( pkg->context ) != '\0' );
}

/**
 * @brief Get the context for the package
 *
 * This returns the value of the @e context parameter for the @c
 * LCFGPackage struct. If the package does not currently have a @e
 * context then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e context for the
 * package.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return The context for the package (possibly NULL).
 */

char * lcfgpackage_get_context( const LCFGPackage * pkg ) {
  return pkg->context;
}

/**
 * @brief Set the context for the package
 *
 * Sets the value of the @e context parameter for the @c
 * LCFGPackage struct to that specified. It is important to note
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
 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] new_ctx String which is the new context
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_context( LCFGPackage * pkg, char * new_ctx ) {

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
 * parameter in the @c LCFGPackage struct if it is not already
 * found in the string.
 *
 * If not already present in the existing information a new context
 * string is built which is the combination of any existing string
 * with the new string appended, the strings are combined using @c
 * lcfgcontext_combine_expressions(). The new string is passed to @c
 * lcfgpackage_set_context(), unlike that function this does NOT
 * assume "ownership" of the input string.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] extra_context String which is the additional context
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_add_context( LCFGPackage * pkg,
                              const char * extra_context ) {

  if ( extra_context == NULL || *extra_context == '\0' )
    return true;

  char * new_context = NULL;
  if ( !lcfgpackage_has_context(pkg) ) {
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
 * Checks if the specified @c LCFGPackage struct currently has a
 * value set for the @e derivation attribute. 
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return boolean which indicates if a package has a derivation
 *
 */

bool lcfgpackage_has_derivation( const LCFGPackage * pkg ) {
  return ( pkg->derivation != NULL && *( pkg->derivation ) != '\0' );
}

/**
 * @brief Get the derivation for the package
 *
 * This returns the value of the @e derivation parameter for the @c
 * LCFGPackage struct. If the package does not currently have a @e
 * derivation then the pointer returned will be @c NULL.
 *
 * It is important to note that this is NOT a copy of the string,
 * changing the returned string will modify the @e derivation for the
 * package.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return The derivation for the package (possibly NULL).
 */

char * lcfgpackage_get_derivation( const LCFGPackage * pkg ) {
  return pkg->derivation;
}

/**
 * @brief Set the derivation for the package
 *
 * Sets the value of the @e derivation parameter for the @c
 * LCFGPackage struct to that specified. It is important to note
 * that this does NOT take a copy of the string. Furthermore, once the
 * value is set the package assumes "ownership", the memory will be
 * freed if the derivation is further modified or the package is destroyed.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] new_deriv String which is the new derivation
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_derivation( LCFGPackage * pkg, char * new_deriv ) {

  /* Currently no validation of the derivation */

  free(pkg->derivation);

  pkg->derivation = new_deriv;
  return true;
}

/**
 * @brief Add extra derivation information for the package
 *
 * Adds the extra derivation information to the value for the @e
 * derivation parameter in the @c LCFGPackage struct if it is not
 * already found in the string.
 *
 * If not already present in the existing information a new derivation
 * string is built which is the combination of any existing string
 * with the new string appended. The new string is passed to @c
 * lcfgpackage_set_derivation(), unlike that function this does NOT
 * assume "ownership" of the input string.

 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] extra_deriv String which is the additional derivation
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_add_derivation( LCFGPackage * pkg,
                                 const char * extra_deriv ) {

  if ( extra_deriv == NULL || *extra_deriv == '\0' )
    return true;

  char * new_deriv = NULL;
  if ( !lcfgpackage_has_derivation(pkg) ) {
    new_deriv = strdup(extra_deriv);
  } else if ( strstr( pkg->derivation, extra_deriv ) == NULL ) {

    /* Only adding the derivation when it is not present in the
       current value. This avoids unnecessary duplication. */

    new_deriv =
      lcfgutils_join_strings( " ", pkg->derivation, extra_deriv );
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
 * @c LCFGPackage struct. The priority is calculated using the
 * context for the package (if any) along with the current active set
 * of contexts for the system.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return The priority for the package.
 */

int lcfgpackage_get_priority( const LCFGPackage * pkg ) {
  return pkg->priority;
}

/**
 * @brief Set the priority for the package
 *
 * Sets the value of the @e priority parameter for the @c LCFGPackage
 * struct to that specified. 
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] new_prio Integer which is the new priority
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_set_priority( LCFGPackage * pkg, int new_prio ) {
  pkg->priority = new_prio;
  return true;
}


/**
 * @brief Evaluate the priority for the package for a list of contexts
 *
 * This will evaluate and update the value of the @c priority
 * attribute for the LCFG package using the value set for the @c
 * context attribute (if any) and the list of LCFG contexts passed in
 * as an argument. The priority is evaluated using @c
 * lcfgctxlist_eval_expression().
 *
 * The default value for the priority is zero, if the package is
 * applicable for the specified list of contexts the priority will be
 * positive otherwise it will be negative.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 * @param[in] ctxlist List of LCFG contexts
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return boolean indicating success
 *
 */

bool lcfgpackage_eval_priority( LCFGPackage * pkg,
                                const LCFGContextList * ctxlist ) {

  bool ok = true;

  int priority = 0;
  if ( lcfgpackage_has_context(pkg) ) {

    /* Calculate the priority using the context expression for this
       package spec. */

    ok = lcfgctxlist_eval_expression( ctxlist,
                                      pkg->context,
                                      &priority );

  }

  if (ok)
    ok = lcfgpackage_set_priority( pkg, priority );

  return ok;
}

/**
 * @brief Check if the package is considered to be active
 *
 * Checks if the current value for the @c priority attribute in the @c
 * LCFGPackage struct is greater than or equal to zero.
 *
 * The priority is calculated using the value for the @c context
 * attribute and the list of currently active contexts, see @c
 * lcfgpackage_eval_priority() for details.
 *
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return boolean indicating if the package is active
 *
 */

bool lcfgpackage_is_active( const LCFGPackage * pkg ) {
  return ( pkg->priority >= 0 );
}

void lcfgpackage_set_defaults(LCFGPackage * pkg) {

  /* This is used to set defaults for anything which is not yet defined */

  const char * empty = LCFG_PACKAGE_NOVALUE;
  const char * wild  = LCFG_PACKAGE_WILDCARD;

  const size_t empty_len = strlen(empty);
  const size_t wild_len  = strlen(wild);

  /* There is no default value for the name field, it is required */

  if ( pkg->arch == NULL )
    pkg->arch       = strndup(empty,empty_len);

  if ( pkg->version == NULL )
    pkg->version    = strndup(wild,wild_len);

  if ( pkg->release == NULL )
    pkg->release    = strndup(wild,wild_len);

  if ( pkg->flags == NULL )
    pkg->flags      = strndup(empty,empty_len);

  if ( pkg->context == NULL )
    pkg->context    = strndup(empty,empty_len);

  if ( pkg->derivation == NULL )
    pkg->derivation = strndup(empty,empty_len);

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
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return Pointer to new string (call @c free() when no longer required)
 *
 */

char * lcfgpackage_full_version( const LCFGPackage * pkg ) {

  const char * v = lcfgpackage_has_version(pkg) ?
                   lcfgpackage_get_version(pkg) : LCFG_PACKAGE_WILDCARD;
  const char * r = lcfgpackage_has_release(pkg) ?
                   lcfgpackage_get_release(pkg) : LCFG_PACKAGE_WILDCARD;

  char * full_version = lcfgutils_join_strings( "-", v, r );

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
 * @param[in] res Pointer to an @c LCFGPackage struct
 *
 * @return Pointer to new string (call @c free() when no longer required)
 *
 */

char * lcfgpackage_id( const LCFGPackage * pkg ) {

  char * id = NULL;
  if ( lcfgpackage_has_arch(pkg) ) {
    id = lcfgutils_join_strings( ".", pkg->name, pkg->arch );

    if ( id == NULL ) {
      perror( "Failed to build LCFG package ID string" );
      exit(EXIT_FAILURE);
    }
  } else if ( lcfgpackage_has_name(pkg) ) {
    id = strdup( lcfgpackage_get_name(pkg) );
  }

  return id;
}

static bool walk_forwards_until( char ** start,
                                 char separator, const char * stop,
                                 char ** field_value ) {

  *field_value = NULL;

  char * begin = *start;
  char * end = NULL;

  /* Ignore leading whitespace */

  while ( *begin != '\0' && isspace(*begin) ) begin++;

 /* Walk forwards looking for the separator */

  char * ptr;
  for ( ptr = (char *) begin; *ptr != '\0'; ptr++ ) {
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

  const char * end = my_len > 0 ? input + my_len - 1 : input;
  char * begin = NULL;

  /* Walk backwards looking for the separator */

  char * ptr;
  for ( ptr = (char *) end; ptr - input >= 0; ptr-- ) {
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

LCFGStatus lcfgpackage_from_string( const char * input,
                                    LCFGPackage ** result,
                                    char ** msg ) {

  *result = NULL;
  *msg = NULL;

  if ( input == NULL ) {
    asprintf( msg, "Invalid LCFG package specification" );
    return LCFG_STATUS_ERROR;
  }

  char * start = (char *) input;
  while ( *start != '\0' && isspace(*start) ) start++;

  if ( *start == '\0' ) {
    asprintf( msg, "Invalid LCFG package specification" );
    return LCFG_STATUS_ERROR;
  }

  bool ok = true;
  LCFGPackage * pkg = lcfgpackage_new();

  /* Prefix - optional */

  /* If first char is not a word character assume it is intended to be
     a prefix */

  char first_char = *start;
  if ( !isword(first_char) ) {

    if ( !lcfgpackage_set_prefix( pkg, first_char ) ) {
      ok = false;
      asprintf( msg, "Invalid LCFG package prefix '%c'.", first_char );
      goto failure;
    }

    start++;
  }

  /* Secondary Architecture - optional */

  char * pkg_arch = NULL;
  walk_forwards_until( &start, '/', "-", &pkg_arch );

  while ( *start != '\0' && isspace(*start) ) start++;

  size_t len = strlen(start);
  while ( len > 0 && isspace( *( start + len - 1 ) ) ) len--;

  /* Context - optional */

  char * ctx_end = len > 0 ? start + len - 1 : start;
  if ( *ctx_end == ']' ) {
    size_t ctx_len = len - 1;

    char * pkg_context = NULL;

    if ( walk_backwards_until( start, &ctx_len, '[', NULL, &pkg_context ) ) {
      len = ctx_len;

      if ( pkg_context != NULL ) {

        ok = lcfgpackage_set_context( pkg, pkg_context );
        if (!ok) {
          asprintf( msg, "Invalid LCFG package context '%s'.", pkg_context );
          free(pkg_context);
          goto failure;
        }
      }

    }

  }

  /* Flags - optional */

  char * pkg_flags = NULL;
  walk_backwards_until( start, &len, ':', "/-", &pkg_flags );

  if ( pkg_flags != NULL ) {

    ok = lcfgpackage_set_flags( pkg, pkg_flags );
    if (!ok) {
      asprintf( msg, "Invalid LCFG package flags '%s'.", pkg_flags );
      free(pkg_flags);
      goto failure;
    }

  }
  
  /* Primary Architecture - optional */

  char * arch2 = NULL;
  walk_backwards_until( start, &len, '/', NULL, &arch2 );

  if ( pkg_arch == NULL ) {
    pkg_arch = arch2;
  } else {
    free(arch2);
  }

  if ( pkg_arch != NULL ) {

    ok = lcfgpackage_set_arch( pkg, pkg_arch );
    if (!ok) {
      asprintf( msg, "Invalid LCFG package architecture '%s'.", pkg_arch );
      free(pkg_arch);
      goto failure;
    }

  }

  /* Release - required */

  char * pkg_release = NULL;
  walk_backwards_until( start, &len, '-', NULL, &pkg_release );

  if ( pkg_release == NULL ) {
    ok = false;
    asprintf( msg, "Failed to extract package release." );
    goto failure;
  } else {

    ok = lcfgpackage_set_release( pkg, pkg_release );

    if (!ok) {
      asprintf( msg, "Invalid LCFG package release '%s'.", pkg_release );
      free(pkg_release);
      goto failure;
    }

  }

  /* Release - required */

  char * pkg_version = NULL;
  walk_backwards_until( start, &len, '-', NULL, &pkg_version );

  if ( pkg_version == NULL ) {
    ok = false;
    asprintf( msg, "Failed to extract package version." );
    goto failure;
  } else {

    ok = lcfgpackage_set_version( pkg, pkg_version );

    if (!ok) {
      asprintf( msg, "Invalid LCFG package version '%s'.", pkg_version );
      free(pkg_version);
      goto failure;
    }

  }

  /* Name - required */

  while ( len > 0 && isspace( *( start + len - 1 ) ) ) len--;

  if ( len == 0 ) {
    ok = false;
    asprintf( msg, "Failed to extract package name." );
    goto failure;
  } else {
    char * pkg_name = strndup( start, len );

    ok = lcfgpackage_set_name( pkg, pkg_name );

    if ( !ok ) {
      asprintf( msg, "Invalid LCFG package name '%s'.", pkg_name );
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

ssize_t lcfgpackage_to_string( const LCFGPackage * pkg,
                               const char * defarch,
                               unsigned int options,
                               char ** result, size_t * size ) {

  if ( !lcfgpackage_has_name(pkg) )
    return -1;

  /* This is rather long-winded but it is done this way to avoid
     multiple calls to realloc. It is actually the cheapest way to
     assemble the package spec string. */

  /* Calculate the required space */

  /* name, version and release always have values */

  const char * pkgnam = pkg->name;
  size_t pkgnamlen    = strlen(pkgnam);

  const char * pkgver = lcfgpackage_has_version(pkg) ?
                         pkg->version : LCFG_PACKAGE_WILDCARD;
  size_t pkgverlen    = strlen(pkgver);

  const char * pkgrel = lcfgpackage_has_release(pkg) ?
                          pkg->release : LCFG_PACKAGE_WILDCARD;
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

  char * pkgarch = NULL;
  if ( lcfgpackage_has_arch(pkg) ) {

    /* Not added to the spec when same as default architecture */
    if ( defarch == NULL ||
         strcmp( pkg->arch, defarch ) != 0 ) {

      pkgarch = pkg->arch;
      new_len += ( strlen(pkgarch) + 1 ); /* +1 for '/' separator */
    }

  }

  char * pkgflgs = NULL;
  if ( lcfgpackage_has_flags(pkg) ) {
    pkgflgs = pkg->flags;

    new_len += ( strlen(pkgflgs) + 1 ); /* +1 for ':' separator */
  }

  char * pkgctx = NULL;
  if ( !(options&LCFG_OPT_NOCONTEXT) &&
       lcfgpackage_has_context(pkg) ) {

    pkgctx = pkg->context;

    new_len += ( strlen(pkgctx) + 2 ); /* +2 for '[' and ']' brackets */
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
  to = stpcpy( to, pkgnam );

  *to = '-';
  to++;

  /* version */
  to = stpcpy( to, pkgver );

  *to = '-';
  to++;

  /* release */
  to = stpcpy( to, pkgrel );

  /* arch */
  if ( pkgarch != NULL ) {
    *to = '/';
    to++;

    to = stpcpy( to, pkgarch );
  }

  /* flags */
  if ( pkgflgs != NULL ) {
    *to = ':';
    to++;

    to = stpcpy( to, pkgflgs );
  }

  /* context */
  if ( pkgctx != NULL ) {
    *to = '[';
    to++;

    to = stpcpy( to, pkgctx );

    *to = ']';
    to++;
  }

  if ( options&LCFG_OPT_NEWLINE )
    to = stpcpy( to, "\n" );

  *to = '\0';

  assert( ( *result + new_len ) == to ) ;

  return new_len;
}

static char const * const meta_start     = "#ifdef INCLUDE_META\n";
static char const * const meta_end       = "#endif\n";
static char const * const pragma_derive  = "#pragma LCFG derive \"";
static char const * const pragma_context = "#pragma LCFG context \"";
static char const * const pragma_end     = "\"\n";

ssize_t lcfgpackage_to_cpp( const LCFGPackage * pkg,
                            const char * defarch,
                            unsigned int options,
                            char ** result, size_t * size ) {

  unsigned int spec_options = ( options            |
                                LCFG_OPT_NOCONTEXT |
                                LCFG_OPT_NOPREFIX  |
                                LCFG_OPT_NEWLINE );

  ssize_t spec_len = lcfgpackage_to_string( pkg, defarch,
                                            spec_options,
                                            result, size );

  if ( spec_len < 0 )
    return spec_len;

  /* Meta-Data */

  size_t meta_start_len     = strlen(meta_start);
  size_t meta_end_len       = strlen(meta_end);
  size_t pragma_derive_len  = strlen(pragma_derive);
  size_t pragma_context_len = strlen(pragma_context);
  size_t pragma_end_len     = strlen(pragma_end);

  size_t meta_len = 0;

  char * derivation = NULL;
  if ( lcfgpackage_has_derivation(pkg) ) {
    derivation = pkg->derivation;

    meta_len += ( pragma_derive_len + strlen(derivation) + pragma_end_len );
  }

  char * context = NULL;
  if ( lcfgpackage_has_context(pkg) ) {
    context     = pkg->context;

    meta_len += ( pragma_context_len + strlen(context) + pragma_end_len );
  }

  if ( meta_len == 0 ) {
    return spec_len;
  } else {
    meta_len += ( meta_start_len + meta_end_len );
  }

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

  if ( derivation != NULL ) {
    to = stpncpy( to, pragma_derive, pragma_derive_len );

    to = stpcpy( to, derivation );

    to = stpncpy( to, pragma_end, pragma_end_len );
  }

  /* Context */

  if ( context != NULL ) {
    to = stpncpy( to, pragma_context, pragma_context_len );

    to = stpcpy( to, context );

    to = stpncpy( to, pragma_end, pragma_end_len );
  }

  to = stpncpy( to, meta_end, meta_end_len );

  assert( ( to + spec_len ) == ( *result + new_len ) );

  return new_len;
}

ssize_t lcfgpackage_to_xml( const LCFGPackage * pkg,
                            const char * defarch,
                            unsigned int options,
                            char ** result, size_t * size ) {

  if ( !lcfgpackage_has_name(pkg) )
    return -1;

  static const char * indent = "   ";
  size_t indent_len = strlen(indent);

  size_t new_len = indent_len + 20; /* <package></package> + newline */

  /* Name - required */

  const char * name = lcfgpackage_get_name(pkg);
  size_t name_len = strlen(name);

  new_len += ( name_len + 13 ); /* <name></name> */

  /* Version - required */

  const char * version = lcfgpackage_has_version(pkg) ?
                      lcfgpackage_get_version(pkg) : LCFG_PACKAGE_WILDCARD;
  size_t ver_len = strlen(version);

  new_len += ( ver_len + 7 ); /* <v></v> */

  /* Release - required (and possibly architecture) */

  const char * release = lcfgpackage_has_release(pkg) ?
                      lcfgpackage_get_release(pkg) : LCFG_PACKAGE_WILDCARD;
  size_t rel_len = strlen(release);

  new_len += ( rel_len + 7 ); /* <r></r> */

  const char * arch = lcfgpackage_has_arch(pkg) ?
                      lcfgpackage_get_arch(pkg) : defarch;

  size_t arch_len = 0;
  if ( arch != NULL ) {
    arch_len = strlen(arch);

    if ( arch_len > 0 )
      new_len += ( arch_len + 1 ); /* +1 for '/' separator */

  }

  /* Flags - optional */

  const char * flags = lcfgpackage_get_flags(pkg);
  size_t flags_len = 0;
  if ( flags != NULL ) {
    flags_len = strlen(flags);

    if ( flags_len > 0 )
      new_len += ( flags_len + 19 ); /* <options></options> */

  }

  /* Context - optional */

  const char * context = lcfgpackage_get_context(pkg);
  size_t ctx_len = 0;
  if ( context != NULL ) {
    ctx_len = strlen(context);

    if ( ctx_len > 0 )
      new_len += ( ctx_len + 15 ); /* ' cfg:context=""' */
  }

  /* Derivation - optional */

  const char * derivation = lcfgpackage_get_derivation(pkg);
  size_t deriv_len = 0;
  if ( derivation != NULL ) {
    deriv_len = strlen(derivation);

    if ( deriv_len > 0 )
      new_len += ( deriv_len + 18 ); /* ' cfg:derivation=""' */
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

int lcfgpackage_compare_versions( const LCFGPackage * pkg1,
                                  const LCFGPackage * pkg2 ) {

  int result = 0;

  if ( lcfgpackage_has_version(pkg1) &&
       lcfgpackage_has_version(pkg2) ) {

    const char * ver1 = pkg1->version;
    const char * ver2 = pkg2->version;

#ifdef HAVE_RPMLIB
    result = rpmvercmp( ver1, ver2 );
#else
    result = strcmp( ver1, ver2 );
#endif

  }

  if ( result == 0 ) {
    if ( lcfgpackage_has_release(pkg1) &&
         lcfgpackage_has_release(pkg2) ) {

      const char * rel1 = pkg1->release;
      const char * rel2 = pkg2->release;

#ifdef HAVE_RPMLIB
      result = rpmvercmp( rel1, rel2 );
#else
      result = strcmp( rel1, rel2 );
#endif

    }
  }

  return result;
}

int lcfgpackage_compare_names( const LCFGPackage * pkg1,
                               const LCFGPackage * pkg2 ) {

  return strcasecmp( pkg1->name,
                     pkg2->name );
}

int lcfgpackage_compare_archs( const LCFGPackage * pkg1,
                               const LCFGPackage * pkg2 ) {

  const char * arch1 = lcfgpackage_has_arch(pkg1) ? pkg1->arch : LCFG_PACKAGE_NOVALUE;
  const char * arch2 = lcfgpackage_has_arch(pkg2) ? pkg2->arch : LCFG_PACKAGE_NOVALUE;

  return strcmp( arch1, arch2 );
}

int lcfgpackage_compare( const LCFGPackage * pkg1,
                         const LCFGPackage * pkg2 ) {

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

bool lcfgpackage_equals( const LCFGPackage * pkg1,
                         const LCFGPackage * pkg2 ) {

  /* Name */

  bool equals = ( lcfgpackage_compare_names( pkg1, pkg2 ) == 0 );

  /* Architecture */

  if ( !equals ) {
    return false;
  } else {
    equals = ( lcfgpackage_compare_archs( pkg1, pkg2 ) == 0 );
  }

  /* Version and Release */

  if ( !equals ) {
    return false;
  } else {
    equals = ( lcfgpackage_compare_versions( pkg1, pkg2 ) == 0 );
  }

  /* Flags */

  if ( !equals ) {
    return false;
  } else {
    const char * flags1 = lcfgpackage_has_flags(pkg1) ? pkg1->flags : LCFG_PACKAGE_NOVALUE;
    const char * flags2 = lcfgpackage_has_flags(pkg2) ? pkg2->flags : LCFG_PACKAGE_NOVALUE;

    equals = ( strcmp( flags1, flags2 ) == 0 );
  }

  /* Context */

  if ( !equals ) {
    return false;
  } else {
    const char * ctx1 = lcfgpackage_has_context(pkg1) ? pkg1->context : LCFG_PACKAGE_NOVALUE;
    const char * ctx2 = lcfgpackage_has_context(pkg2) ? pkg2->context : LCFG_PACKAGE_NOVALUE;

    equals = ( strcmp( ctx1, ctx2 ) == 0 );
  }

  /* The prefix and derivation are NOT compared */

  return equals;
}

bool lcfgpackage_print( const LCFGPackage * pkg,
                        const char * defarch,
                        const char * style,
                        unsigned int options,
                        FILE * out ) {

  char * lcfgspec = NULL;
  size_t buf_size = 0;

  ssize_t rc = 0;
  if ( style != NULL && strcmp( style, "cpp" ) == 0 ) {
    rc = lcfgpackage_to_cpp( pkg, defarch, options,
                             &lcfgspec, &buf_size );
  } else {
    unsigned int my_options = options | LCFG_OPT_NEWLINE;

    if ( style != NULL && strcmp( style, "rpm" ) == 0 ) {
      rc = lcfgpackage_to_rpm_filename( pkg, defarch, my_options,
                                        &lcfgspec, &buf_size );
    } else {
      rc = lcfgpackage_to_string( pkg, defarch, my_options,
                                  &lcfgspec, &buf_size );
    }

  }

  bool ok = ( rc >= 0 );

  if (ok ) {

    if ( fputs( lcfgspec, out ) < 0 )
      ok = false;

  }

  free(lcfgspec);

  return ok;
}

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
  if ( pkg != NULL && lcfgpackage_has_name(pkg) ) {
    size_t buf_size = 0;
    if ( lcfgpackage_to_string( pkg, NULL, 0,
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

  if ( pkg != NULL && lcfgpackage_has_derivation(pkg) ) {
    rc = asprintf( &result, "%s %s at %s",
                   msg_base, msg_mid,
                   lcfgpackage_get_derivation(pkg) );
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

/* eof */
