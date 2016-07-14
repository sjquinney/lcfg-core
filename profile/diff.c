#define _GNU_SOURCE /* for asprintf */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "profile.h"
#include "differences.h"
#include "utils.h"

LCFGChange lcfgprofile_quickdiff( const LCFGProfile * profile1,
                                  const LCFGProfile * profile2,
                                  LCFGTagList ** modified,
                                  LCFGTagList ** added,
                                  LCFGTagList ** removed ) {

  return lcfgcomplist_quickdiff( profile1->components,
                                 profile2->components,
                                 modified,
                                 added,
                                 removed );

}

LCFGStatus lcfgprofile_diff( const LCFGProfile * profile1,
                             const LCFGProfile * profile2,
                             LCFGDiffProfile ** result,
                             char ** msg ) {

  LCFGDiffProfile * profdiff = lcfgdiffprofile_new();

  const LCFGComponentList * list1 = profile1 != NULL ?
                                    profile1->components : NULL;
  const LCFGComponentList * list2 = profile2 != NULL ?
                                    profile2->components : NULL;

  bool ok = true;

  /* Look for components which have been removed or modified */

  LCFGComponentNode * cur_node =
    list1 != NULL ? lcfgcomplist_head(list1) : NULL;

  while ( ok && cur_node != NULL ) {
    const LCFGComponent * comp1 = lcfgcomplist_component(cur_node);
    const char * comp1_name     = lcfgcomponent_get_name(comp1);

    const LCFGComponent * comp2 =
      lcfgcomplist_find_component( list2, comp1_name );

    LCFGDiffComponent * compdiff = NULL;
    LCFGStatus diff_rc = lcfgcomponent_diff( comp1, comp2, &compdiff );

    bool added = false;
    if ( diff_rc != LCFG_STATUS_ERROR ) {

      if ( compdiff != NULL &&
	   !lcfgdiffcomponent_is_empty(compdiff) ) {

	LCFGChange change = lcfgdiffprofile_append( profdiff, compdiff );
	if ( change == LCFG_CHANGE_ADDED ) {
	  added = true;
	} else {
	  diff_rc = LCFG_STATUS_ERROR;
	}
      }

    }

    if (!added)
      lcfgdiffcomponent_destroy(compdiff);

    if ( diff_rc == LCFG_STATUS_ERROR ) {
      asprintf( msg, "Failed to diff '%s' component", comp1_name );
      ok = false;
      break;
    }

    cur_node = lcfgcomplist_next(cur_node);
  }

  /* Look for components which have been added */

  cur_node = list2 != NULL ? lcfgcomplist_head(list2) : NULL;

  while ( ok && cur_node != NULL ) {
    const LCFGComponent * comp2 = lcfgcomplist_component(cur_node);
    const char * comp2_name     = lcfgcomponent_get_name(comp2);

    if ( !lcfgcomplist_has_component( list1, comp2_name ) ) {

      LCFGDiffComponent * compdiff = NULL;
      LCFGStatus diff_rc = lcfgcomponent_diff( NULL, comp2, &compdiff );
      if ( diff_rc == LCFG_STATUS_ERROR )
        ok = false;

      bool added = false;

      if (ok) {
        if ( !lcfgdiffcomponent_is_empty(compdiff) ) {
          LCFGChange append_rc = lcfgdiffprofile_append( profdiff, compdiff );

          if ( append_rc == LCFG_CHANGE_ADDED ) {
            added = true;
          } else {
            ok = false;
          }
        }
      }

      if ( !added )
        lcfgdiffcomponent_destroy(compdiff);

      if ( !ok ) {
        asprintf( msg, "Failed to diff '%s' component", comp2_name );
        break;
      }

    }

    cur_node = lcfgcomplist_next(cur_node);
  }

  if ( !ok ) {
    lcfgdiffprofile_destroy(profdiff);
    profdiff = NULL; 
  }

  *result = profdiff;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

LCFGDiffProfile * lcfgdiffprofile_new() {

  LCFGDiffProfile * profdiff = malloc( sizeof(LCFGDiffProfile) );
  if ( profdiff == NULL ) {
    perror( "Failed to allocate memory for LCFG profile diff" );
    exit(EXIT_FAILURE);
  }

  /* Set default values */

  profdiff->head = NULL;
  profdiff->tail = NULL;
  profdiff->size = 0;

  return profdiff;
}

void lcfgdiffprofile_destroy(LCFGDiffProfile * profdiff ) {

  if ( profdiff == NULL )
    return;

  while ( lcfgdiffprofile_size(profdiff) > 0 ) {
    LCFGDiffComponent * compdiff;
    if ( lcfgdiffprofile_remove_next( profdiff, NULL, &compdiff )
         == LCFG_CHANGE_REMOVED ) {
      lcfgdiffcomponent_destroy(compdiff);
    }
  }

  free(profdiff);
  profdiff = NULL;

}

LCFGChange lcfgdiffprofile_insert_next( LCFGDiffProfile    * profdiff,
                                        LCFGDiffComponent  * current,
                                        LCFGDiffComponent  * new ) {

  if ( new == NULL )
    return LCFG_CHANGE_ERROR;

  if ( current == NULL ) { /* HEAD */

    if ( lcfgdiffprofile_is_empty(profdiff) )
      profdiff->tail = new;

    new->next      = profdiff->head;
    profdiff->head = new;

  } else {

    if ( current->next == NULL )
      profdiff->tail = new;

    new->next     = current->next;
    current->next = new;

  }

  lcfgdiffcomponent_inc_ref(new);

  profdiff->size++;

  return LCFG_CHANGE_ADDED;
}

LCFGChange lcfgdiffprofile_remove_next( LCFGDiffProfile    * profdiff,
                                        LCFGDiffComponent  * current,
                                        LCFGDiffComponent ** old ) {

  if ( lcfgdiffprofile_is_empty(profdiff) )
    return LCFG_CHANGE_ERROR;

  if ( current == NULL ) { /* HEAD */

    *old           = profdiff->head;
    profdiff->head = profdiff->head->next;

    if ( lcfgdiffprofile_size(profdiff) == 1 )
      profdiff->tail = NULL;

  } else {

    if ( current->next == NULL )
      return LCFG_CHANGE_ERROR;

    *old          = current->next;
    current->next = current->next->next;

    if ( current->next == NULL )
      profdiff->tail = current;

  }

  lcfgdiffcomponent_dec_ref(*old);

  profdiff->size--;

  return LCFG_CHANGE_REMOVED;
}

LCFGStatus lcfgdiffprofile_to_holdfile( const LCFGDiffProfile * profdiff,
                                        const char * holdfile,
                                        char ** signature,
                                        char ** msg ) {

  *msg = NULL;

  if ( lcfgdiffprofile_is_empty(profdiff) )
    return true;

  char * tmpfile = lcfgutils_safe_tmpfile(holdfile);

  bool ok = true;

  int fd = mkstemp(tmpfile);
  FILE * out = fdopen( fd, "w" );
  if ( out == NULL ) {
    asprintf( msg, "Failed to open temporary status file '%s'",
              tmpfile );
    ok = false;
    goto cleanup;
  }

  /* Initialise the MD5 support */

  md5_state_t md5state;
  lcfgutils_md5_init(&md5state);

  /* Iterate through the list of components with differences */

  LCFGDiffComponent * cur_compdiff = lcfgdiffprofile_head(profdiff);

  while ( cur_compdiff != NULL ) {

    /* Sort so that the order is the same each time the function is called */

    lcfgdiffcomponent_sort(cur_compdiff);

    LCFGStatus rc = lcfgdiffcomponent_to_holdfile( cur_compdiff, out,
                                                   &md5state );

    if ( rc == LCFG_STATUS_ERROR ) {
      asprintf( msg, "Failed to generate holdfile data for '%s' component",
                lcfgdiffcomponent_get_name(cur_compdiff) );
      ok = false;
      break;
    }

    cur_compdiff = lcfgdiffprofile_next(cur_compdiff);
  }

  if ( ok ) {
    md5_byte_t digest[16];
    lcfgutils_md5_finish( &md5state, digest );

    char * hex_output = NULL;
    ok = lcfgutils_md5_hexdigest( digest, &hex_output );

    if (ok) {
      if ( fprintf( out, "signature: %s\n", hex_output ) < 0 )
        ok = false;
    }

    if (!ok) {
      asprintf( msg, "Failed to store MD5 signature" );
    }

    *signature = hex_output;
  }

  if ( fclose(out) != 0 ) {
    asprintf( msg, "Failed to close hold file" );

    ok = false;
    goto cleanup;
  }

  if ( ok ) {
    if ( rename( tmpfile, holdfile ) != 0 ) {
      asprintf( msg, "Failed to rename temporary hold file to '%s'",
                holdfile );
      ok = false;
    }
  }

 cleanup:

  if ( tmpfile != NULL ) {
    unlink(tmpfile);
    free(tmpfile);
  }

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

LCFGDiffComponent * lcfgdiffprofile_find_component(
					    const LCFGDiffProfile * profdiff,
					    const char * want_name ) {

  if ( lcfgdiffprofile_is_empty(profdiff) )
    return NULL;

  LCFGDiffComponent * result = NULL;

  LCFGDiffComponent * cur_compdiff = lcfgdiffprofile_head(profdiff);
  while ( cur_compdiff != NULL ) {
    const char * cur_name = lcfgdiffcomponent_get_name(cur_compdiff);

    if ( cur_name != NULL && strcmp( cur_name, want_name ) == 0 ) {
      result = cur_compdiff;
      break;
    }

    cur_compdiff = lcfgdiffprofile_next(cur_compdiff);
  }

  return result;
}

bool lcfgdiffprofile_has_component( const LCFGDiffProfile * profdiff,
				    const char * comp_name ) {

  return ( lcfgdiffprofile_find_component( profdiff, comp_name ) != NULL );
}

/* eof */
