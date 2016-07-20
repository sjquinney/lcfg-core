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

/* Currently there are only three supported prefixes for LCFG package
   specifications:

   +  insert package into list, replaces any existing package of same name/arch
   -  remove any package from list which matches this name/arch
   ?  replace any existing package in list which matches this name/arch
*/

static const char * permitted_prefixes = "?+-=~";

LCFGPackageSpec * lcfgpkgspec_new(void) {

  LCFGPackageSpec * pkgspec = malloc( sizeof(LCFGPackageSpec) );
  if ( pkgspec == NULL ) {
    perror( "Failed to allocate memory for LCFG package spec" );
    exit(EXIT_FAILURE);
  }

  pkgspec->name       = NULL;
  pkgspec->arch       = NULL;
  pkgspec->version    = NULL;
  pkgspec->release    = NULL;
  pkgspec->flags      = NULL;
  pkgspec->context    = NULL;
  pkgspec->derivation = NULL;
  pkgspec->prefix     = '\0';
  pkgspec->priority   = 0;
  pkgspec->_refcount  = 0;

  return pkgspec;
}

void lcfgpkgspec_destroy(LCFGPackageSpec * pkgspec) {

  if ( pkgspec == NULL )
    return;

  if ( pkgspec->_refcount > 0 )
    return;

  free(pkgspec->name);
  pkgspec->name = NULL;

  free(pkgspec->arch);
  pkgspec->arch = NULL;

  free(pkgspec->version);
  pkgspec->version = NULL;

  free(pkgspec->release);
  pkgspec->release = NULL;

  free(pkgspec->flags);
  pkgspec->flags = NULL;

  free(pkgspec->context);
  pkgspec->context = NULL;

  free(pkgspec->derivation);
  pkgspec->derivation = NULL;

  free(pkgspec);
  pkgspec = NULL;

}

LCFGPackageSpec * lcfgpkgspec_clone( const LCFGPackageSpec * pkgspec ) {

  LCFGPackageSpec * clone = lcfgpkgspec_new();
  if ( clone == NULL )
    return NULL;

  bool ok = true;
  if ( pkgspec->name != NULL ) {
    char * new_name = strdup(pkgspec->name);
    ok = lcfgpkgspec_set_name( clone, new_name );
    if (!ok)
      free(new_name);
  }

  if ( ok && pkgspec->arch != NULL ) {
    char * new_arch = strdup(pkgspec->arch);
    ok = lcfgpkgspec_set_arch( clone, new_arch );
    if (!ok)
      free(new_arch);
  }

  if ( ok && pkgspec->version != NULL ) {
    char * new_version = strdup(pkgspec->version);
    ok = lcfgpkgspec_set_version( clone, new_version );
    if (!ok)
      free(new_version);
  }

  if ( ok && pkgspec->release != NULL ) {
    char * new_release = strdup(pkgspec->release);
    ok = lcfgpkgspec_set_release( clone, new_release );
    if (!ok)
      free(new_release);
  }

  if ( ok && pkgspec->flags != NULL ) {
    char * new_flags = strdup(pkgspec->flags);
    ok = lcfgpkgspec_set_flags( clone, new_flags );
    if (!ok)
      free(new_flags);
  }

  if ( ok && pkgspec->context != NULL ) {
    char * new_context = strdup(pkgspec->context);
    ok = lcfgpkgspec_set_context( clone, new_context );
    if (!ok)
      free(new_context);
  }

  if ( ok && pkgspec->derivation != NULL ) {
    char * new_deriv = strdup(pkgspec->derivation);
    ok = lcfgpkgspec_set_derivation( clone, new_deriv );
    if (!ok)
      free(new_deriv);
  }

  clone->prefix   = pkgspec->prefix;
  clone->priority = pkgspec->priority;

  /* Cloning should never fail */
  if ( !ok ) {
    lcfgpkgspec_destroy(clone);
    clone = NULL;
  }

  return clone;
}

/* Name */

bool lcfgpkgspec_valid_name( const char * name ) {

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

bool lcfgpkgspec_has_name( const LCFGPackageSpec * pkgspec ) {
  return ( pkgspec->name != NULL && *( pkgspec->name ) != '\0' );
}

char * lcfgpkgspec_get_name( const LCFGPackageSpec * pkgspec ) {
  return pkgspec->name;
}

bool lcfgpkgspec_set_name( LCFGPackageSpec * pkgspec, char * new_name ) {

  bool ok = false;
  if ( lcfgpkgspec_valid_name(new_name) ) {
    free(pkgspec->name);

    pkgspec->name = new_name;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/* Architecture */

bool lcfgpkgspec_valid_arch( const char * arch ) {

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

bool lcfgpkgspec_has_arch( const LCFGPackageSpec * pkgspec ) {
  return ( pkgspec->arch != NULL && *( pkgspec->arch ) != '\0' );
}

char * lcfgpkgspec_get_arch( const LCFGPackageSpec * pkgspec ) {
  return pkgspec->arch;
}

bool lcfgpkgspec_set_arch( LCFGPackageSpec * pkgspec, char * new_arch ) {

  bool ok = false;
  if ( lcfgpkgspec_valid_arch(new_arch) ) {
    free(pkgspec->arch);

    pkgspec->arch = new_arch;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/* Version */

bool lcfgpkgspec_valid_version( const char * version ) {

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

bool lcfgpkgspec_has_version( const LCFGPackageSpec * pkgspec ) {
  return ( pkgspec->version != NULL && *( pkgspec->version ) != '\0' );
}

char * lcfgpkgspec_get_version( const LCFGPackageSpec * pkgspec ) {
  return pkgspec->version;
}

bool lcfgpkgspec_set_version( LCFGPackageSpec * pkgspec, char * new_version ) {

  bool ok = false;
  if ( lcfgpkgspec_valid_version(new_version) ) {
    free(pkgspec->version);

    pkgspec->version = new_version;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/* Release */

bool lcfgpkgspec_valid_release( const char * release ) {
  /* Currently the same rules as for version strings */
  return lcfgpkgspec_valid_version(release);
}

bool lcfgpkgspec_has_release( const LCFGPackageSpec * pkgspec ) {
  return ( pkgspec->release != NULL && *( pkgspec->release ) != '\0' );
}

char * lcfgpkgspec_get_release( const LCFGPackageSpec * pkgspec ) {
  return pkgspec->release;
}

bool lcfgpkgspec_set_release( LCFGPackageSpec * pkgspec, char * new_release ) {

  bool ok = false;
  if ( lcfgpkgspec_valid_release(new_release) ) {
    free(pkgspec->release);

    pkgspec->release = new_release;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/* Prefix */

bool lcfgpkgspec_valid_prefix( char prefix ) {
  return ( strchr( permitted_prefixes, prefix ) != NULL );
}

bool lcfgpkgspec_has_prefix( const LCFGPackageSpec * pkgspec ) {
  return ( pkgspec->prefix != '\0' );
}

char lcfgpkgspec_get_prefix( const LCFGPackageSpec * pkgspec ) {
  return pkgspec->prefix;
}

bool lcfgpkgspec_remove_prefix( LCFGPackageSpec * pkgspec ) {
    pkgspec->prefix = '\0';
    return true;
}

bool lcfgpkgspec_set_prefix( LCFGPackageSpec * pkgspec, char new_prefix ) {

  bool ok = false;
  if ( lcfgpkgspec_valid_prefix(new_prefix) ) {
    pkgspec->prefix = new_prefix;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/* Flags */

bool lcfgpkgspec_valid_flag_chr( const char flag ) {
  return ( isalnum(flag) );
}

bool lcfgpkgspec_valid_flags( const char * flags ) {

  /* MUST be at least one character long. */

  bool valid = ( flags  != NULL &&
                 *flags != '\0' );

  /* This only permits [a-zA-Z0-9] characters */

  char * ptr;
  for ( ptr = (char *) flags; valid && *ptr != '\0'; ptr++ ) {
    if ( !lcfgpkgspec_valid_flag_chr(*ptr) )
      valid = false;
  }

  return valid;
}

bool lcfgpkgspec_has_flags( const LCFGPackageSpec * pkgspec ) {
  return ( pkgspec->flags != NULL && *( pkgspec->flags ) != '\0' );
}

bool lcfgpkgspec_has_flag( const LCFGPackageSpec * pkgspec, char flag ) {

  return ( lcfgpkgspec_has_flags(pkgspec) &&
           strchr( pkgspec->flags, flag ) != NULL );
}

char * lcfgpkgspec_get_flags( const LCFGPackageSpec * pkgspec ) {
  return pkgspec->flags;
}

bool lcfgpkgspec_set_flags( LCFGPackageSpec * pkgspec, char * new_flags ) {

  bool ok = false;
  if ( lcfgpkgspec_valid_flags(new_flags) ) {
    free(pkgspec->flags);

    pkgspec->flags = new_flags;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

bool lcfgpkgspec_add_flags( LCFGPackageSpec * pkgspec,
                            const char * extra_flags ) {

  if ( extra_flags == NULL || *extra_flags == '\0' )
    return true;

  unsigned int i;

  size_t new_len = 0;

  /* This uses a simple trick to find the unique set of flags required
     given the 'current' and 'extra' flags

     An array of booleans is used, each element represents the ASCII
     code for a character. The array begins as all false and then when
     characters are seen for the first time in either string the
     value for that element is switched to true.

     This silently ignores any character with an ASCII code greater
     than MAXCHAR but that's unlikely to be a huge problem. Other
     invalid characters not in the [a-zA-Z0-9] set will be flagged up
     as a problem when set_flags is called at the end of this
     function.

  */

#define MAXCHAR 128

  bool char_set[MAXCHAR] = { false };

  size_t cur_len = 0;
  if ( pkgspec->flags != NULL ) {
    cur_len = strlen(pkgspec->flags);

    for ( i=0; i<cur_len; i++ ) {
      int val = (pkgspec->flags)[i] - '0';
      if ( val < MAXCHAR ) {
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
    if ( val < MAXCHAR ) {
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

  for ( i=0; i<MAXCHAR; i++ ) {
    if ( char_set[i] ) {
      *to = i + '0';
      to++;
    }
  }

  *to = '\0';

  assert( ( result + new_len ) == to );

  bool ok = lcfgpkgspec_set_flags( pkgspec, result );
  if ( !ok )
    free(result);

  return ok;
}

/* Context Expression */

bool lcfgpkgspec_valid_context( const char * expr ) {
  return lcfgcontext_valid_expression(expr);
}

bool lcfgpkgspec_has_context( const LCFGPackageSpec * pkgspec ) {
  return ( pkgspec->context != NULL && *( pkgspec->context ) != '\0' );
}

char * lcfgpkgspec_get_context( const LCFGPackageSpec * pkgspec ) {
  return pkgspec->context;
}

bool lcfgpkgspec_set_context( LCFGPackageSpec * pkgspec, char * new_value ) {

  bool ok = false;
  if ( lcfgpkgspec_valid_context(new_value) ) {
    free(pkgspec->context);

    pkgspec->context = new_value;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

bool lcfgpkgspec_add_context( LCFGPackageSpec * pkgspec,
                              const char * extra_context ) {

  if ( extra_context == NULL || *extra_context == '\0' )
    return true;

  char * new_context = NULL;
  if ( !lcfgpkgspec_has_context(pkgspec) ) {
    new_context = strdup(extra_context);
  } else {
    new_context =
      lcfgcontext_combine_expressions( pkgspec->context, extra_context );
  }

  bool ok = lcfgpkgspec_set_context( pkgspec, new_context );
  if ( !ok )
    free(new_context);

  return ok;
}

/* Derivation */

bool lcfgpkgspec_has_derivation( const LCFGPackageSpec * pkgspec ) {
  return ( pkgspec->derivation != NULL && *( pkgspec->derivation ) != '\0' );
}

char * lcfgpkgspec_get_derivation( const LCFGPackageSpec * pkgspec ) {
  return pkgspec->derivation;
}

bool lcfgpkgspec_set_derivation( LCFGPackageSpec * pkgspec, char * new_value ) {

  /* Currently no validation of the derivation */

  free(pkgspec->derivation);

  pkgspec->derivation = new_value;
  return true;
}

bool lcfgpkgspec_add_derivation( LCFGPackageSpec * pkgspec,
                                 const char * extra_deriv ) {

  if ( extra_deriv == NULL || *extra_deriv == '\0' )
    return true;

  char * new_deriv = NULL;
  if ( !lcfgpkgspec_has_derivation(pkgspec) ) {
    new_deriv = strdup(extra_deriv);
  } else if ( strstr( pkgspec->derivation, extra_deriv ) == NULL ) {

    /* Only adding the derivation when it is not present in the
       current value. This avoids unnecessary duplication. */

    new_deriv =
      lcfgutils_join_strings( pkgspec->derivation, extra_deriv, " " );
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
    ok = lcfgpkgspec_set_derivation( pkgspec, new_deriv );

    if ( !ok )
      free(new_deriv);
  }

  return ok;
}

/* Priority */

int lcfgpkgspec_get_priority( const LCFGPackageSpec * pkgspec ) {
  return pkgspec->priority;
}

bool lcfgpkgspec_set_priority( LCFGPackageSpec * pkgspec, int priority ) {
  pkgspec->priority = priority;
  return true;
}

bool lcfgpkgspec_eval_priority( LCFGPackageSpec * pkgspec,
                                const LCFGContextList * ctxlist,
                                char ** msg ) {

  *msg = NULL;
  bool ok = true;

  int priority = 0;
  if ( lcfgpkgspec_has_context(pkgspec) ) {

    /* Calculate the priority using the context expression for this
       package spec. */

    ok = lcfgctxlist_eval_expression( ctxlist,
                                      pkgspec->context,
                                      &priority, msg );

  }

  if (ok)
    ok = lcfgpkgspec_set_priority( pkgspec, priority );

  return ok;
}

bool lcfgpkgspec_is_active( const LCFGPackageSpec * pkgspec ) {
  return ( pkgspec->priority >= 0 );
}

void lcfgpkgspec_set_defaults(LCFGPackageSpec * pkgspec) {

  /* This is used to set defaults for anything which is not yet defined */

  const char * empty = LCFG_PACKAGE_NOVALUE;
  const char * wild  = LCFG_PACKAGE_WILDCARD;

  const size_t empty_len = strlen(empty);
  const size_t wild_len  = strlen(wild);

  /* There is no default value for the name field, it is required */

  if ( pkgspec->arch == NULL )
    pkgspec->arch       = strndup(empty,empty_len);

  if ( pkgspec->version == NULL )
    pkgspec->version    = strndup(wild,wild_len);

  if ( pkgspec->release == NULL )
    pkgspec->release    = strndup(wild,wild_len);

  if ( pkgspec->flags == NULL )
    pkgspec->flags      = strndup(empty,empty_len);

  if ( pkgspec->context == NULL )
    pkgspec->context    = strndup(empty,empty_len);

  if ( pkgspec->derivation == NULL )
    pkgspec->derivation = strndup(empty,empty_len);

}

/* Higher-level functions */

char * lcfgpkgspec_full_version( const LCFGPackageSpec * pkgspec ) {

  char * result =
    lcfgutils_join_strings( pkgspec->version, pkgspec->release, "-" );

  if ( result == NULL ) {
    perror( "Failed to build LCFG package full-version string" );
    exit(EXIT_FAILURE);
  }

  return result;
}

char * lcfgpkgspec_id( const LCFGPackageSpec * pkgspec ) {

  char * result;
  if ( lcfgpkgspec_has_arch(pkgspec) ) {
    result = lcfgutils_join_strings( pkgspec->name, pkgspec->arch, "." );

    if ( result == NULL ) {
      perror( "Failed to build LCFG package ID string" );
      exit(EXIT_FAILURE);
    }
  } else {
    result = strdup( pkgspec->name );
  }

  return result;
}

bool lcfgpkgspec_from_string( const char * spec,
                              LCFGPackageSpec ** result,
                              char ** errmsg ) {
  *result = NULL;
  *errmsg = NULL;

  if ( spec == NULL || *spec == '\0' ) {
    asprintf( errmsg, "Invalid LCFG package specification" );
    return false;
  }

  bool ok = true;

  int i;

  char * input = (char *) spec;
  int end = strlen(input) - 1;

  /* Results */

  char * pkg_name    = NULL;
  char * pkg_arch    = NULL;
  char * pkg_version = NULL;
  char * pkg_release = NULL;
  char * pkg_flags   = NULL;
  char * pkg_context = NULL;

  *result = lcfgpkgspec_new();

  /* Prefix - optional

     Check the first character of the string to see if it is one of
     the supported prefix characters. If found then store it and
     advance the pointer.

   */

  if ( !isword(input[0]) ) {
    if ( !lcfgpkgspec_set_prefix( *result, input[0] ) ) {
      asprintf( errmsg, "Invalid LCFG package prefix '%c'.", input[0] );
      goto failure;
    }

    input += 1;
    end   -= 1;
  }

  /* Secondary Architecture - optional

     Scan forwards from the start of the string until a '/'
     (forward-slash) separator is found. The characters of the
     architecture are always alpha-numeric or underscore so if any
     other character is found or the end of the string is reached then
     just stop and do not store anything. If found then store it and
     advance the pointer.

  */

  i = 0;
  while ( i <= end ) {

    if ( input[i] == '/' ) {
      if ( i > 0 ) {
        pkg_arch = strndup( input, i );
      }
      input += ( i + 1 );
      end   -= ( i + 1 );
      break;
    } else if ( !isword(input[i]) ) {
      break;
    }

    i++;
  }

  /* Context - optional

     If the final character of the string is a ']' (right
     square-bracket) then scan backwards from the end until a '['
     (left square-bracket) is found for the start of the context
     string. If the start of the string is found then fail.

  */

  if ( input[end] == ']' ) {

    i = end - 1;
    while ( i >= 0 ) {

      if ( input[i] == '[' ) {
        /* -1 to avoid the closing ']' character */
        int len = end - 1 - i;
        if ( len > 0 ) {
          pkg_context = strndup( input + i + 1, len );
        }
        end = i - 1;
        break;
      }

      i--;
    }

    if ( i < 0 ) {
      asprintf( errmsg, "Failed to extract package context." );
      goto failure;
    }
  }

  if ( pkg_context != NULL ) {
    if ( !lcfgpkgspec_set_context( *result, pkg_context ) ) {
      asprintf( errmsg, "Invalid LCFG package context '%s'.", pkg_context );
      free(pkg_context);
      pkg_context = NULL;
      goto failure;
    }
  }

  /* Flags - optional

     Scan backwards from the end of the string until a ':' (colon)
     separator is found. If a '/' architecture separator or a '-'
     (hyphen) version-release separator is found first or the start of
     the string is reached then just stop and do not store
     anything. Note that this could break if a package has a colon in
     the 'release' field but that has not been seen in the wild.

  */

  i = end;
  while ( i >= 0 ) {

    if ( input[i] == ':' ) {
      int len = end - i;
      if ( len > 0 ) {
        pkg_flags = strndup( input + i + 1, len );
      }
      end = i - 1;
      break;
    } else if ( input[i] == '/' || input[i] == '-' ) {
      break;
    }

    i--;
  }

  if ( pkg_flags != NULL ) {
    if ( !lcfgpkgspec_set_flags( *result, pkg_flags ) ) {
      asprintf( errmsg, "Invalid LCFG package flags '%s'.", pkg_flags );
      free(pkg_flags);
      pkg_flags = NULL;
      goto failure;
    }
  }

  /* Primary Architecture - optional

     Scan backwards from the end of the string until a '/'
     (forward-slash) separator is found. . Note that if a secondary
     architecture has already been found then that takes precedence
     and the primary architecture will NOT be stored.

   */

  i = end;
  while ( i > 0 ) {

    if ( input[i] == '/' ) {
      int len = end - i;
      if ( len > 0 && pkg_arch == NULL ) {
        pkg_arch = strndup( input + i + 1, len );
      }
      end = i - 1;
      break;
    }

    i--;
  }

  if ( pkg_arch != NULL ) {
    if ( !lcfgpkgspec_set_arch( *result, pkg_arch ) ) {
      asprintf( errmsg, "Invalid LCFG package architecture '%s'.", pkg_arch );
      free(pkg_arch);
      pkg_arch = NULL;
      goto failure;
    }
  }

  /* Release - required

   Scan backwards until the first '-' (hyphen) character is
   found. Fails if the start of the string is reached or the field is
   empty. 

  */

  i = end;
  while ( i > 0 ) {

    if ( input[i] == '-' ) {
      int len = end - i;
      if ( len > 0 ) {
        pkg_release = strndup( input + i + 1, len );
      }
      end = i - 1;
      break;
    }

    i--;
  }
  if ( i < 0 || pkg_release == NULL ) {
    asprintf( errmsg, "Failed to extract package release." );
    goto failure;
  } else {
    if ( !lcfgpkgspec_set_release( *result, pkg_release ) ) {
      asprintf( errmsg, "Invalid LCFG package release '%s'.", pkg_release );
      free(pkg_release);
      pkg_release = NULL;
      goto failure;
    }
  }

  /* Version - required

   Scan backwards until the first '-' (hyphen) character is
   found. Fails if the start of the string is reached or the field is
   empty. 

  */

  i = end;
  while ( i > 0 ) {

    if ( input[i] == '-' ) {
      int len = end - i;
      if ( len > 0 ) {
        pkg_version = strndup( input + i + 1, len );
      }
      end = i - 1;
      break;
    }

    i--;
  }
  if ( i < 0 || pkg_version == NULL ) {
    asprintf( errmsg, "Failed to extract package version." );
    goto failure;
  } else {
    if ( !lcfgpkgspec_set_version( *result, pkg_version ) ) {
      asprintf( errmsg, "Invalid LCFG package version '%s'.", pkg_version );
      free(pkg_version);
      pkg_version = NULL;
      goto failure;
    }
  }


  /* Name - required

   This is everything left after finding all the other fields. 

  */

  int len = end + 1;
  if ( len > 0 ) {
    pkg_name = strndup( input, end + 1 );
  }
  if ( pkg_name == NULL ) {
    asprintf( errmsg, "Failed to extract package name." );
    goto failure;
  } else {
    if ( !lcfgpkgspec_set_name( *result, pkg_name ) ) {
      asprintf( errmsg, "Invalid LCFG package name '%s'.", pkg_name );
      free(pkg_name);
      pkg_name = NULL;
      goto failure;
    }
  }

  /* Finishing off */

  if (!ok) {

  failure:
    ok = false; /* in case we've jumped in from elsewhere */

    if ( *result != NULL ) {
      lcfgpkgspec_destroy(*result);
      *result = NULL;
    }

  }

  return ok;
}

ssize_t lcfgpkgspec_to_string( const LCFGPackageSpec * pkgspec,
                               const char * defarch,
                               unsigned int options,
                               char ** result, size_t * size ) {

  if ( !lcfgpkgspec_has_name(pkgspec) )
    return -1;

  /* This is rather long-winded but it is done this way to avoid
     multiple calls to realloc. It is actually the cheapest way to
     assemble the package spec string. */

  /* Calculate the required space */

  /* name, version and release always have values */

  const char * pkgnam = pkgspec->name;
  size_t pkgnamlen    = strlen(pkgnam);

  const char * pkgver = lcfgpkgspec_has_version(pkgspec) ?
                         pkgspec->version : LCFG_PACKAGE_WILDCARD;
  size_t pkgverlen    = strlen(pkgver);

  const char * pkgrel = lcfgpkgspec_has_release(pkgspec) ?
                          pkgspec->release : LCFG_PACKAGE_WILDCARD;
  size_t pkgrellen    = strlen(pkgrel);

  /* +2 for the two '-' separators */
  size_t new_len = pkgnamlen + pkgverlen + pkgrellen + 2;

  /* prefix can be disabled */

  char pkgpfx = '\0';
  if ( !(options&LCFG_OPT_NOPREFIX) &&
       lcfgpkgspec_has_prefix(pkgspec) ) {

    pkgpfx = pkgspec->prefix;
    new_len++;
  }

  char * pkgarch = NULL;
  if ( lcfgpkgspec_has_arch(pkgspec) ) {

    /* Not added to the spec when same as default architecture */
    if ( defarch == NULL ||
         strcmp( pkgspec->arch, defarch ) != 0 ) {

      pkgarch = pkgspec->arch;
      new_len += ( strlen(pkgarch) + 1 ); /* +1 for '/' separator */
    }

  }

  char * pkgflgs = NULL;
  if ( lcfgpkgspec_has_flags(pkgspec) ) {
    pkgflgs = pkgspec->flags;

    new_len += ( strlen(pkgflgs) + 1 ); /* +1 for ':' separator */
  }

  char * pkgctx = NULL;
  if ( !(options&LCFG_OPT_NOCONTEXT) &&
       lcfgpkgspec_has_context(pkgspec) ) {

    pkgctx = pkgspec->context;

    new_len += ( strlen(pkgctx) + 2 ); /* +2 for '[' and ']' brackets */
  }

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

ssize_t lcfgpkgspec_to_cpp( const LCFGPackageSpec * pkgspec,
                            const char * defarch,
                            unsigned int options,
                            char ** result, size_t * size ) {

  unsigned int spec_options = ( options            |
                                LCFG_OPT_NOCONTEXT |
                                LCFG_OPT_NOPREFIX  |
                                LCFG_OPT_NEWLINE );

  ssize_t spec_len = lcfgpkgspec_to_string( pkgspec, defarch,
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
  if ( lcfgpkgspec_has_derivation(pkgspec) ) {
    derivation = pkgspec->derivation;

    meta_len += ( pragma_derive_len + strlen(derivation) + pragma_end_len );
  }

  char * context = NULL;
  if ( lcfgpkgspec_has_context(pkgspec) ) {
    context     = pkgspec->context;

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
      perror("Failed to allocate memory for LCFG resource string");
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

ssize_t lcfgpkgspec_to_xml( const LCFGPackageSpec * pkgspec,
                            const char * defarch,
                            unsigned int options,
                            char ** result, size_t * size ) {

  if ( !lcfgpkgspec_has_name(pkgspec) )
    return -1;

  static const char * indent = "   ";
  size_t indent_len = strlen(indent);

  size_t new_len = indent_len + 20; /* <package></package> + newline */

  /* Name - required */

  const char * name = lcfgpkgspec_get_name(pkgspec);
  size_t name_len = strlen(name);

  new_len += ( name_len + 13 ); /* <name></name> */

  /* Version - required */

  const char * version = lcfgpkgspec_has_version(pkgspec) ?
                      lcfgpkgspec_get_version(pkgspec) : LCFG_PACKAGE_WILDCARD;
  size_t ver_len = strlen(version);

  new_len += ( ver_len + 7 ); /* <v></v> */

  /* Release - required (and possibly architecture) */

  const char * release = lcfgpkgspec_has_release(pkgspec) ?
                      lcfgpkgspec_get_release(pkgspec) : LCFG_PACKAGE_WILDCARD;
  size_t rel_len = strlen(release);

  new_len += ( rel_len + 7 ); /* <r></r> */

  const char * arch = lcfgpkgspec_has_arch(pkgspec) ?
                      lcfgpkgspec_get_arch(pkgspec) : defarch;

  size_t arch_len = 0;
  if ( arch != NULL ) {
    arch_len = strlen(arch);

    if ( arch_len > 0 )
      new_len += ( arch_len + 1 ); /* +1 for '/' separator */

  }

  /* Flags - optional */

  const char * flags = lcfgpkgspec_get_flags(pkgspec);
  size_t flags_len = 0;
  if ( flags != NULL ) {
    flags_len = strlen(flags);

    if ( flags_len > 0 )
      new_len += ( flags_len + 19 ); /* <options></options> */

  }

  /* Context - optional */

  const char * context = lcfgpkgspec_get_context(pkgspec);
  size_t ctx_len = 0;
  if ( context != NULL ) {
    ctx_len = strlen(context);

    if ( ctx_len > 0 )
      new_len += ( ctx_len + 15 ); /* ' cfg:context=""' */
  }

  /* Derivation - optional */

  const char * derivation = lcfgpkgspec_get_derivation(pkgspec);
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
      perror("Failed to allocate memory for LCFG resource string");
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

int lcfgpkgspec_compare_versions( const LCFGPackageSpec * pkgspec1,
                                  const LCFGPackageSpec * pkgspec2 ) {

  int result = 0;

  if ( lcfgpkgspec_has_version(pkgspec1) &&
       lcfgpkgspec_has_version(pkgspec2) ) {

    const char * ver1 = pkgspec1->version;
    const char * ver2 = pkgspec2->version;

#ifdef HAVE_RPMLIB
    result = rpmvercmp( ver1, ver2 );
#else
    result = strcmp( ver1, ver2 );
#endif

  }

  if ( result == 0 ) {
    if ( lcfgpkgspec_has_release(pkgspec1) &&
         lcfgpkgspec_has_release(pkgspec2) ) {

      const char * rel1 = pkgspec1->release;
      const char * rel2 = pkgspec2->release;

#ifdef HAVE_RPMLIB
      result = rpmvercmp( rel1, rel2 );
#else
      result = strcmp( rel1, rel2 );
#endif

    }
  }

  return result;
}

int lcfgpkgspec_compare_names( const LCFGPackageSpec * pkgspec1,
                               const LCFGPackageSpec * pkgspec2 ) {

  return strcasecmp( pkgspec1->name,
                     pkgspec2->name );
}

int lcfgpkgspec_compare_archs( const LCFGPackageSpec * pkgspec1,
                               const LCFGPackageSpec * pkgspec2 ) {

  const char * arch1 = lcfgpkgspec_has_arch(pkgspec1) ? pkgspec1->arch : LCFG_PACKAGE_NOVALUE;
  const char * arch2 = lcfgpkgspec_has_arch(pkgspec2) ? pkgspec2->arch : LCFG_PACKAGE_NOVALUE;

  return strcmp( arch1, arch2 );
}

int lcfgpkgspec_compare( const LCFGPackageSpec * pkgspec1,
                         const LCFGPackageSpec * pkgspec2 ) {

  /* Name */

  int result = lcfgpkgspec_compare_names( pkgspec1, pkgspec2 );

  /* Architecture */

  if ( result == 0 )
    result = lcfgpkgspec_compare_archs( pkgspec1, pkgspec2 );

  /* Version and Release */

  if ( result == 0 )
    result = lcfgpkgspec_compare_versions( pkgspec1, pkgspec2 );

  return result;
}

bool lcfgpkgspec_equals( const LCFGPackageSpec * pkgspec1,
                         const LCFGPackageSpec * pkgspec2 ) {

  /* Name */

  bool equals = ( lcfgpkgspec_compare_names( pkgspec1, pkgspec2 ) == 0 );

  /* Architecture */

  if ( !equals ) {
    return false;
  } else {
    equals = ( lcfgpkgspec_compare_archs( pkgspec1, pkgspec2 ) == 0 );
  }

  /* Version and Release */

  if ( !equals ) {
    return false;
  } else {
    equals = ( lcfgpkgspec_compare_versions( pkgspec1, pkgspec2 ) == 0 );
  }

  /* Flags */

  if ( !equals ) {
    return false;
  } else {
    const char * flags1 = lcfgpkgspec_has_flags(pkgspec1) ? pkgspec1->flags : LCFG_PACKAGE_NOVALUE;
    const char * flags2 = lcfgpkgspec_has_flags(pkgspec2) ? pkgspec2->flags : LCFG_PACKAGE_NOVALUE;

    equals = ( strcmp( flags1, flags2 ) == 0 );
  }

  /* Context */

  if ( !equals ) {
    return false;
  } else {
    const char * ctx1 = lcfgpkgspec_has_context(pkgspec1) ? pkgspec1->context : LCFG_PACKAGE_NOVALUE;
    const char * ctx2 = lcfgpkgspec_has_context(pkgspec2) ? pkgspec2->context : LCFG_PACKAGE_NOVALUE;

    equals = ( strcmp( ctx1, ctx2 ) == 0 );
  }

  /* The prefix and derivation are NOT compared */

  return equals;
}

bool lcfgpkgspec_print( const LCFGPackageSpec * pkgspec,
                        const char * defarch,
                        const char * style,
                        unsigned int options,
                        FILE * out ) {

  char * lcfgspec = NULL;
  size_t buf_size = 0;

  ssize_t rc = 0;
  if ( style != NULL && strcmp( style, "cpp" ) == 0 ) {
    rc = lcfgpkgspec_to_cpp( pkgspec, defarch, options,
                             &lcfgspec, &buf_size );
  } else {
    unsigned int my_options = options | LCFG_OPT_NEWLINE;

    if ( style != NULL && strcmp( style, "rpm" ) == 0 ) {
      rc = lcfgpkgspec_to_rpm_filename( pkgspec, defarch, my_options,
                                        &lcfgspec, &buf_size );
    } else {
      rc = lcfgpkgspec_to_string( pkgspec, defarch, my_options,
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

char * lcfgpkgspec_build_message( const LCFGPackageSpec * pkgspec,
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
  if ( pkgspec != NULL && lcfgpkgspec_has_name(pkgspec) ) {
    size_t buf_size = 0;
    if ( lcfgpkgspec_to_string( pkgspec, NULL, 0,
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

  if ( pkgspec != NULL && lcfgpkgspec_has_derivation(pkgspec) ) {
    rc = asprintf( &result, "%s %s at %s",
                   msg_base, msg_mid,
                   lcfgpkgspec_get_derivation(pkgspec) );
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
