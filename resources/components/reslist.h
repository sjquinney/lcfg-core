
#ifndef LCFG_CORE_RESOURCES_LIST_H
#define LCFG_CORE_RESOURCES_LIST_H

#include <stdbool.h>
#include <stdio.h>

#include "common.h"
#include "resources.h"
#include "components.h"

#define lcfgreslist_size(LIST) ((LIST)->size)
#define lcfgreslist_is_empty(LIST) (LIST == NULL || (LIST)->size == 0)

LCFGResourceList * lcfgreslist_new(void);
void lcfgreslist_acquire( LCFGResourceList * list );
void lcfgreslist_relinquish( LCFGResourceList * list );
bool lcfgreslist_is_shared( const LCFGResourceList * list );

bool lcfgreslist_set_merge_rules( LCFGResourceList * list,
                                  LCFGMergeRule new_rules )
  __attribute__((warn_unused_result));

LCFGChange lcfgreslist_merge_resource( LCFGResourceList * list,
                                       LCFGResource * new_res,
                                       char ** msg )
  __attribute__((warn_unused_result));

const LCFGResource * lcfgreslist_first_resource(const LCFGResourceList * list);

void lcfgreslist_sort_by_priority( LCFGResourceList * list );

bool lcfgreslist_print( const LCFGResourceList * list,
                        const char * compname,
                        LCFGResourceStyle style,
                        LCFGOption options,
                        char ** buffer, size_t * buf_size,
                        FILE * out )
  __attribute__((warn_unused_result));

#endif /* LCFG_CORE_RESOURCES_LIST_H */

/* eof */
