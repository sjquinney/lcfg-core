/* Templates */

#include <string.h>

#include "tags.h"

#define LCFG_TEMPLATE_PLACEHOLDER '$'

struct LCFGTemplate {
  char * name;
  char * tmpl;
  int places[LCFG_TAGS_MAX_DEPTH];
  struct LCFGTemplate * next;
  size_t name_len;
  size_t tmpl_len;
  unsigned int pcount;
};

typedef struct LCFGTemplate LCFGTemplate;

LCFGTemplate * lcfgtemplate_new(void);

void lcfgtemplate_destroy(LCFGTemplate * head_template);

bool lcfgtemplate_is_valid ( const LCFGTemplate * template );

char * lcfgtemplate_get_name( const LCFGTemplate * template );
bool lcfgtemplate_set_name( LCFGTemplate * template, char * new_name )
  __attribute__((warn_unused_result));

bool lcfgresource_valid_template( const char * tmpl );

char * lcfgtemplate_get_tmpl( LCFGTemplate * template );
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

/* eof */
