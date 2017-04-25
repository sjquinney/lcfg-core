/**
 * @file templates.h
 * @brief LCFG resource template handling library
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date$
 * $Revision$
 */

#ifndef LCFG_CORE_TEMPLATES_H
#define LCFG_CORE_TEMPLATES_H

#include <stdbool.h>
#include <string.h>

#include "tags.h"

/**
 * @brief The placeholder character used in LCFG resource templates
 */

#define LCFG_TEMPLATE_PLACEHOLDER '$'

/**
 * @brief A structure to represent an LCFG resource template
 */

struct LCFGTemplate {
  char * tmpl;                     /**< The template string */
  int places[LCFG_TAGS_MAX_DEPTH]; /**< Array of locations in the string where placeholders are found */
  struct LCFGTemplate * next;      /**< The next template in the single-linked list of templates (or @c NULL if last) */
  size_t name_len;                 /**< The size of the base name part of the template */
  size_t tmpl_len;                 /**< The size of the whole template */
  unsigned int pcount;             /**< The number of placeholders found */
};

typedef struct LCFGTemplate LCFGTemplate;

LCFGTemplate * lcfgtemplate_new(void);

void lcfgtemplate_destroy(LCFGTemplate * head_template);

bool lcfgtemplate_is_valid ( const LCFGTemplate * template );

bool lcfgresource_valid_template( const char * tmpl );

char * lcfgtemplate_get_tmpl( const LCFGTemplate * template );
bool lcfgtemplate_set_tmpl( LCFGTemplate * template, char * new_tmpl )
  __attribute__((warn_unused_result));

LCFGStatus lcfgtemplate_from_string( const char * input,
                                     LCFGTemplate ** template,
                                     char ** msg )
  __attribute__((warn_unused_result));

ssize_t lcfgtemplate_to_string( const LCFGTemplate * head_template,
                                const char * prefix,
                                LCFGOption options,
                                char ** str, size_t * len )
  __attribute__((warn_unused_result));

LCFGTemplate * lcfgtemplate_find( const LCFGTemplate * head_template,
                                  const char * field_name );

char * lcfgresource_build_name( const LCFGTemplate * templates,
                                const LCFGTagList  * taglist,
                                const char * field_name,
                                char ** msg );

#endif /* LCFG_CORE_TEMPLATES_H */

/* eof */
