/**
 * @file packages/container.c
 * @brief Generic functions for working with LCFG package lists and sets
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2018 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#define _GNU_SOURCE /* for asprintf */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "packages.h"
#include "container.h"

/**
 * @brief Process a CPP packages file
 *
 * This processes any LCFG packages file, that includes the @e rpmcfg
 * files used as input for the updaterpms package manager.
 *
 * Each LCFG package specification found will be parsed and merged
 * into the container using the relevant merge function (e.g. @c
 * lcfgpkgset_merge_package or @c lcfgpkglist_merge_package).
 *
 * Optionally the path to a file of macros can be specified which will
 * be passed to the cpp command using the @c -imacros option. If the
 * path does not exist or is not a file it will be ignored. That file
 * can be generated using the @c lcfgpackage_store_options function.
 *
 * Optionally a list of directories may also be specified, these will
 * be passed to the cpp command using the @c -I option. Any paths
 * which do not exist or are not directories will be ignored.
 *
 * The file is pre-processed using the C Pre-Processor so the cpp tool
 * must be available.
 *
 * An error is returned if the input file does not exist or is not
 * readable.
 *
 * The following options are supported:
 *   - @c LCFG_OPT_USE_META - include any metadata (contexts and derivations)
 *   - @c LCFG_OPT_ALL_CONTEXTS - include packages for all contexts
 *
 * @param[in] filename The path to the input CPP file
 * @param[in] ctr Reference to a @c LCFGPackageSet or @c LCFGPackageList
 * @param[in] ctr_type Type of package container being passed
 * @param[in] defarch Default architecture string (may be @c NULL)
 * @param[in] macros_file Optional file of CPP macros (may be @c NULL)
 * @param[in] incpath Optional list of include directories for CPP (may be @c NULL)
 * @param[in]  options Controls the behaviour of the process.
 * @param[out] deps Reference to list of file dependencies
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgpackages_from_cpp( const char * filename,
                                  LCFGPkgContainer * ctr,
				  LCFGPkgContainerType ctr_type,
                                  const char * defarch,
                                  const char * macros_file,
				  char ** incpath,
                                  LCFGOption options,
				  char *** deps,
                                  char ** msg ) {

  /* Ensure we have a filename and do a simple readability test */

  if ( isempty(filename) ) {
    lcfgutils_build_message( msg, "Invalid CPP filename" );
    return LCFG_CHANGE_ERROR;
  } else if ( !lcfgutils_file_readable(filename) ) {
    lcfgutils_build_message( msg, "File '%s' does not exist or is not readable",
                             filename );
    return LCFG_CHANGE_ERROR;
  }

  bool include_meta = options & LCFG_OPT_USE_META;
  bool all_contexts = options & LCFG_OPT_ALL_CONTEXTS;
  LCFGChange change = LCFG_CHANGE_NONE;

  /* Variables which need to be declared ahead of any jumps to 'cleanup' */

  /* Any temporary files created must be secure (i.e. 0600) */
  mode_t mode_mask = umask(S_IXUSR | S_IRWXG | S_IRWXO);

  char * tmpfile = NULL;
  FILE * fp = NULL;
  char * line = NULL;

  /* Temporary file for cpp output */

  tmpfile = lcfgutils_safe_tmpname(NULL);

  int tmpfd = mkstemp(tmpfile);
  if ( tmpfd == -1 ) {
    change = LCFG_CHANGE_ERROR;
    lcfgutils_build_message( msg, "Failed to create temporary file '%s'",
                             tmpfile );
    goto cleanup;
  }

  pid_t pid = fork();
  if ( pid == -1 ) {
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  } else if ( pid == 0 ) {

    unsigned int cmd_size = 18;
    if ( incpath != NULL ) {
      char ** ptr;
      for ( ptr=incpath; *ptr!=NULL; ptr++ ) cmd_size += 2;
    }

    char ** cpp_cmd = calloc( cmd_size, sizeof(char *) );
    if ( cpp_cmd == NULL ) {
      perror("Failed to allocate memory for cpp command");
      exit(EXIT_FAILURE);
    }

    unsigned int i = 0;
    cpp_cmd[i++] = "cpp";
    cpp_cmd[i++] = "-traditional";
    cpp_cmd[i++] = "-x";
    cpp_cmd[i++] = "c";
    cpp_cmd[i++] = "-undef";
    cpp_cmd[i++] = "-nostdinc";
    cpp_cmd[i++] = "-Wall";
    cpp_cmd[i++] = "-Wundef";
    cpp_cmd[i++] = "-Ui386";
    cpp_cmd[i++] = "-DLCFGNG";

    if ( all_contexts )
      cpp_cmd[i++] = "-DALL_CONTEXTS";

    if ( include_meta )
      cpp_cmd[i++] = "-DINCLUDE_META";

    /* We test files and directories before including them in the list
       of arguments for the cpp command. This does NOT guarantee they
       will still exist or be the same thing in the filesystem when we
       actually run the command. What it does achieve is ensure that
       the caller cannot abuse the facility to insert other options
       into the command. */

    struct stat sb;

    /* Macros file can be generated using lcfgpackages_store_options */

    if ( !isempty(macros_file) ) {
      if ( stat( macros_file, &sb ) == 0 && S_ISREG(sb.st_mode) ) {
        cpp_cmd[i++] = "-imacros";
        cpp_cmd[i++] = (char *) macros_file;
      } else {
        fprintf( stderr, "Ignoring '%s': not a valid macros file\n",
                 macros_file );
      }
    }

    /* List of directories to be included in the cpp search path */

    if ( incpath != NULL ) {
      char ** ptr;
      for ( ptr=incpath; *ptr!=NULL; ptr++ ) {
        char * path = *ptr;

        if ( isempty(path) ) continue;

        /* Ignore any leading '-I', we will add our own */
        if ( strncmp( path, "-I", 2 ) == 0 ) path += 2;

        /* Only interested in paths which are directories */
        
        if ( stat( path, &sb ) == 0 && S_ISDIR(sb.st_mode) ) {
          cpp_cmd[i++] = "-I";
          cpp_cmd[i++] = path;
        } else {
          fprintf( stderr, "Ignoring '%s': not a valid directory\n", *ptr );
        }
      }
    }

    cpp_cmd[i++] = (char *) filename; /* input */
    cpp_cmd[i++] = "-o";
    cpp_cmd[i++] = tmpfile;           /* output */

    execvp( cpp_cmd[0], cpp_cmd ); 

    _exit(errno); /* Not normally reached */
  }

  int status = 0;
  waitpid( pid, &status, 0 );
  if ( WIFEXITED(status) && WEXITSTATUS(status) != 0 ) {
    change = LCFG_CHANGE_ERROR;
    lcfgutils_build_message( msg, "Failed to process '%s' using cpp",
                             filename );
    goto cleanup;
  }

  fp = fdopen( tmpfd, "r" );
  if ( fp == NULL ) {
    change = LCFG_CHANGE_ERROR;
    lcfgutils_build_message( msg, "Failed to open cpp output file '%s'",
                             tmpfile );
    goto cleanup;
  }

  /* Setup the getline buffer */

  size_t line_len = 128;
  line = calloc( line_len, sizeof(char) );
  if ( line == NULL ) {
    perror( "Failed to allocate memory whilst processing package list file" );
    exit(EXIT_FAILURE);
  }

  /* Process results

     Select package list merge function - hack to support sets and
     lists - this generates warnings about pointer types but its
     perfectly valid code.
  */

  LCFGChange (*merge_fn)(void *, LCFGPackage *, char **);

  void * pkgs;
  if ( ctr_type == LCFG_PKG_CONTAINER_SET ) {
    pkgs = ctr->set;
    merge_fn = &lcfgpkgset_merge_package;
  } else {
    pkgs = ctr->list;
    merge_fn = &lcfgpkglist_merge_package;
  }

  char * meta_deriv    = NULL;
  char * meta_context  = NULL;
  char * meta_category = NULL;

  /* Keep a list of all files included */
  size_t deps_size = 64; /* sufficient for all but the most extreme cases */
  size_t deps_item = 0;
  *deps = calloc( deps_size, sizeof(char *) );
  if ( *deps == NULL ) {
    perror("Failed to allocate dependency list");
    exit(EXIT_FAILURE);
  }

  /* For efficiency the derivations are stashed in a map once
     processed. Many packages have the same derivations and they can
     be quite large so we want to only process them once - saves time
     and memory */

  LCFGDerivationMap * drvmap = include_meta ? lcfgderivmap_new() : NULL;

  char * cur_file = NULL;
  unsigned int cur_line = 0;
  while( LCFGChangeOK(change) && getline( &line, &line_len, fp ) != -1 ) {
    cur_line++;

    lcfgutils_string_trim(line);

    if ( *line == '\0' ) continue;

    if ( *line == '#' ) {

      unsigned int cpp_flags = 0;
      bool cpp_derive = lcfgutils_parse_cpp_derivation( line,
                                                        &cur_file, &cur_line,
                                                        &cpp_flags );
      if ( cpp_derive ) {
        cur_line--; /* next time around it will be incremented */

	if ( cpp_flags & LCFG_CPP_FLAG_ENTRY ) {  /* Dependency tracking */

	  bool found = false;
	  char ** ptr;
	  for ( ptr=*deps; !found && *ptr!=NULL; ptr++ )
	    if ( strcmp( *ptr, cur_file ) == 0 ) found = true;

	  if ( !found )
	    (*deps)[deps_item++] = strdup(cur_file);

	  if ( deps_item == deps_size ) {
	    size_t new_size = deps_size * 2;
	    char ** deps_new = realloc( deps, new_size * sizeof(char *) );
	    if ( deps_new == NULL ) {
	      perror("Failed to resize dependency list");
	      exit(EXIT_FAILURE);
	    } else {
	      memset( deps_new + deps_size, 0, deps_size );

	      *deps     = deps_new;
	      deps_size = new_size;
	    }
	  }

	}

      } else if ( include_meta ) {
        LCFGPkgPragma meta_key;
        char * meta_value = NULL;

        bool meta_ok = lcfgpackage_parse_pragma( line, &meta_key, &meta_value );
        if ( meta_ok && !isempty(meta_value) ) {

          switch(meta_key)
            {
            case LCFG_PKG_PRAGMA_DERIVE:
              free(meta_deriv);
              meta_deriv = meta_value;
              meta_value = NULL;
              break;
            case LCFG_PKG_PRAGMA_CONTEXT:
              free(meta_context);
              meta_context = meta_value;
              meta_value = NULL;
              break;
            case LCFG_PKG_PRAGMA_CATEGORY:
              free(meta_category);
              meta_category = meta_value;
              meta_value = NULL;
              break;
            default:
              break; /* no op */
            }
        }

        free(meta_value);
      }

      continue;
    }

    char * error_msg = NULL;

    LCFGPackage * pkg = NULL;
    LCFGStatus parse_status = lcfgpackage_from_spec( line, &pkg, &error_msg );

    if ( parse_status == LCFG_STATUS_ERROR )
      change = LCFG_CHANGE_ERROR;

    /* Architecture */

    if ( LCFGChangeOK(change) ) {
      free(error_msg);
      error_msg = NULL;

      if ( !lcfgpackage_has_arch(pkg) && !isempty(defarch) ) {

        char * pkg_arch = strdup(defarch);
        if ( !lcfgpackage_set_arch( pkg, pkg_arch ) ) {
          free(pkg_arch);
          change = LCFG_CHANGE_ERROR;
          lcfgutils_build_message( &error_msg,
                                   "Failed to set package architecture to '%s'",
                                   defarch );
        }
      }
    }

    /* Derivation */

    if ( LCFGChangeOK(change) ) {
      free(error_msg);
      error_msg = NULL;
        
      if ( include_meta && !isempty(meta_deriv) ) {

        char * drvmsg = NULL;
        LCFGDerivationList * drvlist =
          lcfgderivmap_find_or_insert_string( drvmap, meta_deriv, &drvmsg );

        bool ok = false;

        if ( drvlist != NULL )
          ok = lcfgpackage_set_derivation( pkg, drvlist );

        if (!ok) {
          change = LCFG_CHANGE_ERROR;
          lcfgutils_build_message( &error_msg, "Invalid derivation '%s': %s",
                                   meta_deriv, drvmsg );
        }

        free(drvmsg);
      } else {
        /* Ignore any problem with setting the derivation */
        (void) lcfgpackage_add_derivation_file_line( pkg, cur_file, cur_line );
      }
    }

    /* All other metadata */

    if ( include_meta ) {

      /* Context */

      if ( LCFGChangeOK(change) && !isempty(meta_context) ) {
        if ( lcfgpackage_set_context( pkg, meta_context ) ) {
          meta_context = NULL; /* Ensure memory is NOT immediately freed */
        } else {
          change = LCFG_CHANGE_ERROR;
          lcfgutils_build_message( &error_msg, "Invalid context '%s'",
                                     meta_context );
        }
      }

      /* Category */

      if ( LCFGChangeOK(change) && !isempty(meta_category) ) {

        /* Once a category is set it applies to all subsequent
           packages until a new category is specified */

        char * category_copy = strdup(meta_category);
        if ( !lcfgpackage_set_category( pkg, category_copy ) ) {
          free(category_copy);
          change = LCFG_CHANGE_ERROR;
          lcfgutils_build_message( &error_msg, "Invalid category '%s'",
                                   meta_category );
        }
      }

    }

    /* Merge package into container (set or list) */

    if ( LCFGChangeOK(change) ) {
      free(error_msg);
      error_msg = NULL;

      LCFGChange merge_status = (*merge_fn)( pkgs, pkg, &error_msg );
      if ( LCFGChangeError(merge_status) )
        change = LCFG_CHANGE_ERROR;
      else if ( merge_status != LCFG_CHANGE_NONE )
        change = LCFG_CHANGE_MODIFIED;
    }

    /* Issue a useful error message */
    if ( LCFGChangeError(change) ) {

      if ( error_msg == NULL ) {
        lcfgutils_build_message( msg, "Error in '%s' at line %u",
                                 cur_file, cur_line );
      } else {
        lcfgutils_build_message( msg, "Error in '%s' at line %u: %s",
				 cur_file, cur_line, error_msg );
      }
    }

    lcfgpackage_relinquish(pkg);

    free(error_msg);
  }

  free(cur_file);

  free(meta_deriv);
  free(meta_context);
  free(meta_category);
  lcfgderivmap_relinquish(drvmap);

 cleanup:
  free(line);

  if ( fp != NULL )
    (void) fclose(fp);

  if ( tmpfile != NULL ) {
    (void) unlink(tmpfile);
    free(tmpfile);
  }

  if ( LCFGChangeError(change) ) {

    if ( *msg == NULL )
      lcfgutils_build_message( msg, "Failed to process package list file" );

  }

  umask(mode_mask);

  return change;
}

LCFGChange lcfgpackages_from_debian_index( const char * filename,
                                           LCFGPkgContainer * ctr,
                                           LCFGPkgContainerType ctr_type,
                                           LCFGOption options,
                                           char ** msg ) {

  /* Ensure we have a filename and do a simple readability test */

  if ( isempty(filename) ) {
    lcfgutils_build_message( msg, "Invalid filename" );
    return LCFG_CHANGE_ERROR;
  }

  FILE * fp;
  if ( (fp = fopen(filename, "r")) == NULL ) {

    if (errno == ENOENT) {
      if ( options&LCFG_OPT_ALLOW_NOEXIST ) {
        return LCFG_CHANGE_NONE;
      } else {
        lcfgutils_build_message( msg, "File does not exist" );
        return LCFG_CHANGE_ERROR;
      }
    } else {
      lcfgutils_build_message( msg, "File is not readable" );
      return LCFG_CHANGE_ERROR;
    }

  }

  /* Setup the getline buffer */

  size_t line_len = 128;
  char * line = malloc( line_len * sizeof(char) );
  if ( line == NULL ) {
    perror( "Failed to allocate memory whilst processing debian index file" );
    exit(EXIT_FAILURE);
  }

  /* Process index file

     Select package list merge function - hack to support sets and
     lists - this generates warnings about pointer types but its
     perfectly valid code.
  */

  LCFGChange (*merge_fn)(void *, LCFGPackage *, char **);

  void * pkgs;
  if ( ctr_type == LCFG_PKG_CONTAINER_SET ) {
    pkgs = ctr->set;
    merge_fn = &lcfgpkgset_merge_package;
  } else {
    pkgs = ctr->list;
    merge_fn = &lcfgpkglist_merge_package;
  }

  LCFGChange change = LCFG_CHANGE_NONE;
  LCFGPackage * pkg = NULL;
  char * error_msg = NULL;

  unsigned int cur_line = 0;
  while( LCFGChangeOK(change) && getline( &line, &line_len, fp ) != -1 ) {
    cur_line++;

    lcfgutils_string_trim(line);

    if ( isempty(line) ) {

      if ( pkg == NULL )
        continue;

      /* Merge package into container (set or list) */

      LCFGChange merge_status = (*merge_fn)( pkgs, pkg, &error_msg );
      if ( LCFGChangeError(merge_status) )
        change = LCFG_CHANGE_ERROR;
      else if ( merge_status != LCFG_CHANGE_NONE )
        change = LCFG_CHANGE_MODIFIED;

      lcfgpackage_relinquish(pkg);
      pkg = NULL;

    } else {

      if ( pkg == NULL )
        pkg = lcfgpackage_new();

      /* Only interested in Package/Version/Architecture */

      bool ok = true;
      char * value = NULL;
      if ( strncmp( line, "Package: ", 9 ) == 0 ) {
        value = strdup( line + 9 );
        ok = lcfgpackage_set_name( pkg, value );

        if (ok) {
          value = NULL;
        } else {
          error_msg =
            lcfgpackage_build_message(pkg, "Invalid name '%s'", value );
        }

      } else if ( strncmp( line, "Version: ", 9 ) == 0 ) {
        value = strdup( line + 9 );
        ok = lcfgpackage_set_version( pkg, value );

        if (ok) {
          value = NULL;
        } else {
          error_msg =
            lcfgpackage_build_message(pkg, "Invalid version '%s'", value );
        }

      } else if ( strncmp( line, "Architecture: ", 14 ) == 0 ) {
        value = strdup( line + 14 );
        ok = lcfgpackage_set_arch( pkg, value );

        if (ok) {
          value = NULL;
        } else {
          error_msg =
            lcfgpackage_build_message(pkg, "Invalid architecture '%s'", value );
        }

      }

      if (!ok) {
        change = LCFG_CHANGE_ERROR;

        free(value);
        lcfgpackage_relinquish(pkg);
      }
    }
    
  }

  free(line);

  if ( pkg != NULL ) {

    /* Merge package into container (set or list) */

    LCFGChange merge_status = (*merge_fn)( pkgs, pkg, &error_msg );
    if ( LCFGChangeError(merge_status) )
      change = LCFG_CHANGE_ERROR;
    else if ( merge_status != LCFG_CHANGE_NONE )
      change = LCFG_CHANGE_MODIFIED;

    lcfgpackage_relinquish(pkg);
    pkg = NULL;
  }

  fclose(fp);

  /* Issue a useful error message */
  if ( LCFGChangeError(change) ) {

    if ( error_msg == NULL ) {
      lcfgutils_build_message( msg, "Error in '%s' at line %u",
                               filename, cur_line );
    } else {
      lcfgutils_build_message( msg, "Error in '%s' at line %u: %s",
                               filename, cur_line, error_msg );
    }
  }

  free(error_msg);

  return change;
}

/* eof */
