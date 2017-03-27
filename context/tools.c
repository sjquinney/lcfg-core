
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <utime.h>

#include "context.h"
#include "utils.h"

/**
 * @brief Check the context directory is accessible
 *
 * This can be used to verify that the specified location is a
 * directory and is accessible. If it does not exist then a simple
 * attempt will be made to create the directory. Note that this does
 * not test whether it is possible to write a file into the
 * directory. It is also important to note that the directory might
 * disappear between this check succeeding and any attempt to actually
 * access the files.
 *
 * @param[in] contextdir Location of contexts directory
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return boolean indicating success
 *
 */

bool lcfgcontext_check_cfgdir( const char * contextdir, char ** msg ) {

  *msg = NULL;
  bool ok = false;

  struct stat buf;

  mode_t mode = strtol("0755",NULL,8);

  if ( stat( contextdir, &buf ) == 0 ) {
    if ( S_ISDIR(buf.st_mode) ) {
      ok = true;
    } else {
     lcfgutils_build_message( msg, "'%s' exists but is not a directory",
                              contextdir );
    }
  } else if ( errno == ENOENT ) {
    if ( mkdir( contextdir, mode ) == 0 ) {
      ok = true;
    } else {
      lcfgutils_build_message( msg,
                               "'%s' does not exist and could not be created",
                               contextdir );
    }
  }

  return ok;
}

static char * lcfgcontext_pendingfile(const char * contextdir) {
  return lcfgutils_catfile( contextdir, ".context" );
}

static char * lcfgcontext_activefile(const char * contextdir) {
  return lcfgutils_catfile( contextdir, ".active" );
}

static char * lcfgcontext_lockfile(const char * contextdir) {
  return lcfgutils_catfile( contextdir, ".lockfile" );
}

/**
 * @brief Open a secure temporary context file
 *
 * This can be used to generate a secure temporary contexts file in
 * the specified directory and open the file ready for writing.
 *
 * A failure will result in the @c exit() function being called with a
 * non-zero value.
 *
 * @param[in] contextdir Location of contexts directory
 * @param[out] tmpfile Path to temporary file
 *
 * @return File stream opened for writing
 *
 */

FILE * lcfgcontext_tmpfile(const char * contextdir, char ** tmpfile ) {

  char * file = lcfgutils_catfile( contextdir, ".context.XXXXXX" );
  int fd = mkstemp( file );
  if ( fd < 0 ) {
    free(file);

    perror("Failed to open temporary context file");
    exit(EXIT_FAILURE);
  }

  FILE * tmpfh = fdopen( fd, "w" );
  if ( tmpfh == NULL ) {
    close(fd);
    free(file);

    perror("Failed to open temporary context file");
    exit(EXIT_FAILURE);
  }

  *tmpfile = file;

  return tmpfh;
}

/**
 * @brief Lock the context directory
 *
 * This can be used to lock the specified context directory to block
 * any other updates. If the directory is already locked then this
 * function will wait for the specified number of seconds before
 * attempting to break the lock.
 *
 * @param[in] contextdir Location of contexts directory
 * @param[in] file Context file which should be locked
 * @param[in] timeout Integer timeout in seconds
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return boolean indicating success
 *
 */

bool lcfgcontext_lock( const char * contextdir,
		       const char * file, int timeout, char ** msg ) {

  char * lockfile = lcfgcontext_lockfile(contextdir);

  bool giveup = false;
  bool locked = ( symlink( file, lockfile ) == 0 );
  while ( !locked && !giveup ) {
    if ( errno != EEXIST ) {
      lcfgutils_build_message( msg, "Cannot link '%s' => '%s'",
                               file, lockfile );
      giveup = true;
    } else {

      if ( timeout > 0 ) {
        sleep(1);
        timeout--;
      } else {
        if ( !lcfgcontext_unlock( contextdir, msg ) )
          giveup = true;
      }
    }

    if ( !giveup ) {
      locked = ( link( file, lockfile ) == 0 );
    }

  }

  free(lockfile);

  return locked;
}

/**
 * @brief Unlock the context directory
 *
 * This can be used to unlock the specified context directory.
 *
 * @param[in] contextdir Location of contexts directory
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return boolean indicating success
 *
 */

bool lcfgcontext_unlock( const char * contextdir, char ** msg ) {

  char * lockfile = lcfgcontext_lockfile(contextdir);

  bool unlocked = ( unlink(lockfile) == 0 );

  if (!unlocked) {
    if ( errno == ENOENT ) {  /* deleted in some other way */
      unlocked = true;
    } else {
      lcfgutils_build_message( msg, "Failed to remove lockfile" );
    }
  }

  free(lockfile);

  return unlocked;
}

/**
 * @brief Update the pending contexts file
 *
 * This takes a list of context updates and applies them to the
 * current pending contexts list. Each item in the list is parsed
 * using the @c lcfgcontext_from_string() function and thus the
 * expected format is "NAME = VALUE". To remove a context an empty
 * value should be specified.
 *
 * Whilst the pending contexts file is being updated the directory
 * will be locked so that other processes attempting to do the same
 * thing are blocked and have to wait.
 *
 * If the application of the context updates does not result in any
 * functional differences then the pending file will not be altered.
 *
 * @param[in] contextdir Location of contexts directory
 * @param[in] change_count Number of context updates
 * @param[in] contexts Array of context updates
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return boolean indicating success
 *
 */

LCFGChange lcfgcontext_update_pending( const char * contextdir,
                                       int change_count, char * contexts[],
                                       char ** msg ) {

  if ( change_count <= 0 )
    return LCFG_CHANGE_NONE;

  if ( !lcfgcontext_check_cfgdir(contextdir, msg ) )
    return LCFG_CHANGE_ERROR;

  /* Declare here at the top things which will be referred to in the
     final 'cleanup' section so it is possible to safely jump there. */

  bool ok = true;
  bool changed = false;
  LCFGContextList * newlist = NULL;
  LCFGContextList * pending = NULL;

  /* This is the temporary file that will be written into if there are
     any changes. It is used as part of the locking process so needs
     to be created even when there might not be any changes. */

  char * tmpfile = NULL;
  FILE * tmpfh = lcfgcontext_tmpfile( contextdir, &tmpfile );

  /* Take an exclusive lock */

  char * lock_msg = NULL;
  bool locked = lcfgcontext_lock( contextdir, tmpfile, 5, &lock_msg );
  if ( !locked ) {
    lcfgutils_build_message( msg, "Failed to lock context directory: %s",
                             lock_msg );
    ok = false;
  }
  free(lock_msg);
  if (!ok) goto cleanup;

  /* Load the current pending list */

  time_t modtime = 0;
  char * load_msg = NULL;
  LCFGStatus rc = lcfgcontext_load_pending( contextdir,
					    &pending, &modtime, &load_msg );

  ok = ( rc != LCFG_STATUS_ERROR );
  if (!ok) {
    lcfgutils_build_message( msg, "Failed to load pending contexts: %s",
                             load_msg );
  }
  free(load_msg);
  if (!ok) goto cleanup;

  /* Clone the current list and apply updates */

  newlist = lcfgctxlist_clone(pending);
  if ( newlist == NULL ) {
    lcfgutils_build_message( msg, "Failed to clone pending context list" );
    ok = false;
    goto cleanup;
  }

  int priority = lcfgctxlist_max_priority(pending);

  int i;
  for ( i=0; ok && i<change_count; i++ ){

    char * parse_msg = NULL;

    LCFGContext * ctx = NULL;
    LCFGStatus parse_rc =
      lcfgcontext_from_string( contexts[i], ++priority,
                               &ctx, &parse_msg );

    if ( parse_rc != LCFG_STATUS_OK || ctx == NULL ) {
      lcfgutils_build_message( msg, "Failed to parse context '%s': %s",
                               contexts[i], parse_msg );
      ok = false;
    } else {

      LCFGChange rc = lcfgctxlist_update( newlist, ctx );
      if ( rc == LCFG_CHANGE_ERROR ) {
        lcfgutils_build_message( msg, "Failed to merge context '%s'",
                                 contexts[i] );
        ok = false;
      }
    }

    lcfgcontext_release(ctx);
    free(parse_msg);

    if (!ok) break;
  }

  if (!ok) goto cleanup;

  /* Compare the current and new lists for differences */

  changed = lcfgctxlist_diff( pending, newlist, NULL, modtime );

  if (changed) {
    /* Write the new list to the temporary file in priority order */

    lcfgctxlist_sort_by_priority(newlist);

    ok = lcfgctxlist_print( newlist, tmpfh );

    if (ok) {
      if ( fclose(tmpfh) == 0 ) {
        tmpfh = NULL; /* Avoids a further attempt to close in cleanup */
      } else {
        lcfgutils_build_message( msg, "Failed to close file '%s'", tmpfile );
        ok = false;
      }
    }

    /* Rename to the real pending file */

    if (ok) {
      char * pfile = lcfgcontext_pendingfile(contextdir);
      int rc = rename( tmpfile, pfile );
      ok = ( rc == 0 );
      free(pfile);
    }

  }

 cleanup:
  ; /* yes, this is essential */

  /* Release the exclusive lock */

  char * unlock_msg = NULL;
  if ( !lcfgcontext_unlock( contextdir, &unlock_msg ) ) {
    ok = false;
    lcfgutils_build_message( msg, "Failed to unlock: %s", unlock_msg );
  }
  free(unlock_msg);

  /* This might have already gone but call unlink to ensure
     tidiness. Do not care about the result */

  if ( tmpfile != NULL ) {
    unlink(tmpfile);
    free(tmpfile);
  }

  if ( tmpfh != NULL )
    fclose(tmpfh);

  lcfgctxlist_destroy(pending);
  lcfgctxlist_destroy(newlist);

  return ( ok ? ( changed ? LCFG_CHANGE_MODIFIED : LCFG_CHANGE_NONE )
              : LCFG_CHANGE_ERROR );
}

/**
 * @brief Load pending contexts
 *
 * Loads the contexts of the pending file in the specified directory
 * into a new @c LCFGContextList. If the file does not exist
 * an empty list will be returned.
 *
 * @param[in] contextdir Location of contexts directory
 * @param[out] newactive Reference to pointer to pending @c LCFGContextList
 * @param[out] modtime Modification time of pending contexts file
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return boolean indicating success
 *
 */

LCFGStatus lcfgcontext_load_pending( const char * contextdir,
				     LCFGContextList ** ctxlist,
                                     time_t * modtime,
                                     char ** msg ) {

  char * pfile = lcfgcontext_pendingfile(contextdir);

  LCFGStatus rc = lcfgctxlist_from_file( pfile, ctxlist, modtime,
					 LCFG_OPT_ALLOW_NOEXIST, msg );

  free(pfile);

  return rc;
}

/**
 * @brief Load active contexts
 *
 * Loads the contexts of the active context file in the specified
 * directory into a new @c LCFGContextList. If the file does not exist
 * an empty list will be returned.
 *
 * @param[in] contextdir Location of contexts directory
 * @param[out] newactive Reference to pointer to active @c LCFGContextList
 * @param[out] modtime Modification time of active contexts file
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return boolean indicating success
 *
 */

LCFGStatus lcfgcontext_load_active( const char * contextdir,
				    LCFGContextList ** ctxlist,
				    time_t * modtime,
                                    char ** msg ) {

  char * afile = lcfgcontext_activefile(contextdir);

  LCFGStatus rc = lcfgctxlist_from_file( afile, ctxlist, modtime,
					 LCFG_OPT_ALLOW_NOEXIST, msg );

  free(afile);

  return rc;
}

/**
 * @brief Activate pending contexts
 *
 * This activates the list of pending contexts. If there are no
 * functional differences between the pending and active context lists
 * then the file contents will not be altered. The file modification
 * time will always be updated even when no changes occur. If the list
 * of pending contexts is empty then an empty active file will be
 * created. An @c LCFGContextList holding the newly loaded active
 * contexts is returned.
 *
 * Optionally a base directory for context-specific XML profiles can
 * be specified in which case they will also be examined for relevant
 * changes. When not required this parameter should be set to @c NULL.
 *
 * @param[in] contextdir Location of contexts directory
 * @param[in] ctx_profile_dir Location of contexts profile directory
 * @param[out] newactive Reference to pointer to active @c LCFGContextList
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return boolean indicating success
 *
 */

LCFGChange lcfgcontext_pending_to_active( const char * contextdir,
                                          const char * ctx_profile_dir,
                                          LCFGContextList ** newactive,
                                          char ** msg ) {

  *newactive = NULL;

  LCFGContextList * active  = NULL;
  time_t active_mtime = 0;

  LCFGContextList * pending = NULL;
  time_t pending_mtime = 0;

  bool ok = true;

  if ( !lcfgcontext_check_cfgdir( contextdir, msg ) ) {
    ok = false;
    goto cleanup;
  }

  /* Load the new (pending) contexts */

  char * load_msg = NULL;
  if ( lcfgcontext_load_pending( contextdir, &pending, &pending_mtime,
				 &load_msg ) == LCFG_STATUS_ERROR ) {
    ok = false;
    lcfgutils_build_message( msg, "Failed to load pending contexts: %s",
                             load_msg );
  }

  free(load_msg);
  if (!ok) goto cleanup;

  /* Load the current (active) contexts */

  if ( lcfgcontext_load_active( contextdir, &active, &active_mtime,
				&load_msg ) == LCFG_STATUS_ERROR ) {
    ok = false;
    lcfgutils_build_message( msg, "Failed to load active contexts: %s",
                             load_msg );
  }

  free(load_msg);
  if (!ok) goto cleanup;

  /* Check for changes */

  bool changed = lcfgctxlist_diff( active, pending,
				   ctx_profile_dir,
				   active_mtime );

  char * afile = lcfgcontext_activefile(contextdir);

  if (changed) {

    /* Write out the pending state to the active state */

    char * tmpfile = NULL;
    FILE * tmpfh = lcfgcontext_tmpfile( contextdir, &tmpfile );

    ok = lcfgctxlist_print( pending, tmpfh );

    if (ok) {
      if ( fclose(tmpfh) != 0 ) {
        lcfgutils_build_message( msg, "Failed to close file '%s'", tmpfile );
        ok = false;
      }
    } else {
      int rc = fclose(tmpfh); /* Ignoring any errors */
    }

    /* Rename to the active file */

    if (ok) {
      int rc = rename( tmpfile, afile );
      ok = ( rc == 0 );
    }

    if ( tmpfile != NULL ) {
      unlink(tmpfile);
      free(tmpfile);
    }

  }

  /* Set the mtime on the active file to the same as the pending
     file. Do this even when the contents of the files are
     identical. */

  if ( ok && pending_mtime != 0 ) {
    struct utimbuf times;
    times.actime  = pending_mtime;
    times.modtime = pending_mtime;
    utime( afile, &times );
  }

  free(afile);

 cleanup:

  lcfgctxlist_destroy(active);
  if (!ok) {
    lcfgctxlist_destroy(pending);
    pending = NULL;
  }

  *newactive = pending;

  return ( ok ? ( changed ? LCFG_CHANGE_MODIFIED : LCFG_CHANGE_NONE )
  	      : LCFG_CHANGE_ERROR );
}

/**
 * @brief Query the contents of the pending contexts file
 *
 * This is a high-level function which can be used to evaluate a
 * context query expression given the current list of pending contexts
 * stored in the specified directory. This is really just a wrapper
 * around the @c lcfgctxlist_eval_expression() function. The result is
 * printed on stdout. If an error occurs a suitable message is printed
 * on stderr.
 *
 * @param[in] contextdir Location of contexts directory
 * @param[in] expr Context query string
 *
 * @return boolean indicating success
 *
 */

bool setctx_eval( const char * contextdir, const char * expr ) {

  LCFGContextList * pending = NULL;
  time_t modtime = 0;
  char * load_msg = NULL;
  LCFGStatus rc = lcfgcontext_load_pending( contextdir,
					    &pending, &modtime, &load_msg );

  bool ok = ( rc == LCFG_STATUS_OK );
  if (ok) {
    int result;

    ok = lcfgctxlist_eval_expression( pending, expr, &result );
    if (ok) {
      printf("Ctx: '%s', Result: %d\n", expr, result );
    } else {
      fprintf( stderr, "Failed to evaluate context expression\n" );
    }

  } else {
    fprintf( stderr, "Failed to read context file: %s\n", load_msg );
  }

  free(load_msg);
  lcfgctxlist_destroy(pending);

  return ok;
}

/**
 * @brief Show the contents of the pending contexts file
 *
 * This is a high-level function which can be used to display the
 * current list of pending contexts stored in the specified
 * directory. The result is printed on stdout using the
 * @c lcfgctxlist_print() function. If an error occurs a suitable message
 * is printed on stderr.
 *
 * @param[in] contextdir Location of contexts directory
 *
 * @return boolean indicating success
 *
 */

bool setctx_show(const char * contextdir) {

  LCFGContextList * pending = NULL;
  time_t modtime = 0;
  char * load_msg = NULL;
  LCFGStatus rc = lcfgcontext_load_pending( contextdir,
					    &pending, &modtime, &load_msg );

  bool ok = ( rc == LCFG_STATUS_OK );
  if (ok) {
    ok = lcfgctxlist_print( pending, stdout );
  } else {
    fprintf( stderr, "Failed to read context file: %s\n", load_msg );
  }

  free(load_msg);
  lcfgctxlist_destroy(pending);

  return ok;
}

/**
 * @brief Update the contents of the pending contexts file
 *
 * This is a high-level function which can be used to update the
 * current list of pending contexts stored in the specified
 * directory. This is really just a wrapper around the
 * @c lcfgcontext_update_pending() function. The result is printed on
 * stdout. If an error occurs a suitable message is printed on stderr.
 *
 * @param[in] contextdir Location of contexts directory
 * @param[in] count Number of context updates
 * @param[in] contexts Array of context updates
 *
 * @return boolean indicating success
 *
 */

bool setctx_update( const char * contextdir,
                    int count, char * contexts[] ) {

  char * msg = NULL;
  LCFGChange changed = lcfgcontext_update_pending( contextdir,
                                                   count, contexts,
                                                   &msg );

  switch (changed)
    {
    case LCFG_CHANGE_ERROR:
      fprintf( stderr, "Failed to update contexts: %s\n", msg );
      break;
    case LCFG_CHANGE_NONE:
      printf("No changes to contexts\n");
      break;
    default:
      printf("Contexts changed\n");
      break;
  }

  free(msg);

  return ( changed != LCFG_CHANGE_ERROR );
}

/* eof */
