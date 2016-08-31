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

LCFGTemplate * lcfgtemplate_new( char * name, char * tmpl, char ** msg );

void lcfgtemplate_destroy(LCFGTemplate * head_template);

LCFGStatus lcfgtemplate_from_string( const char * input,
                                     LCFGTemplate ** template,
                                     char ** msg )
  __attribute__((warn_unused_result));

ssize_t lcfgtemplate_to_string( const LCFGTemplate * head_template,
                                const char * prefix,
                                unsigned int options,
                                char ** str, size_t * len )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

LCFGTemplate * lcfgtemplate_find( const LCFGTemplate * head_template,
                                  const char * field_name )
  __attribute__((nonnull (1,2)));

/* eof */
