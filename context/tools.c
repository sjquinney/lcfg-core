
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
      } else if ( rc == LCFG_CHANGE_NONE ) {

        /* no update, throw away spare struct */
	lcfgcontext_release(ctx);
      }

    }

    free(parse_msg);

    if ( !ok )
      lcfgcontext_release(ctx);
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

LCFGChange lcfgcontext_pending_to_active( const char * contextdir,
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
				   contextdir,
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
      fclose(tmpfh); /* Ignoring any errors */
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

/* eof */
