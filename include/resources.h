#ifndef LCFG_RESOURCES_H
#define LCFG_RESOURCES_H

#include <stdio.h>
#include <stdbool.h>

#include "common.h"
#include "context.h"
#include "tags.h"
#include "templates.h"

/* Resources */

#define LCFG_RESOURCE_NOVALUE ""

/* These are marker symbols for the keys which are used when resources
   are serialised into storage such as Berkeley DB or status files. */

#define LCFG_RESOURCE_SYMBOL_DERIVATION '#'
#define LCFG_RESOURCE_SYMBOL_TYPE       '%'
#define LCFG_RESOURCE_SYMBOL_CONTEXT    '='
#define LCFG_RESOURCE_SYMBOL_PRIORITY   '^'
#define LCFG_RESOURCE_SYMBOL_VALUE      '\0'

typedef enum { /* The various standard LCFG resource types */
  LCFG_RESOURCE_TYPE_STRING,
  LCFG_RESOURCE_TYPE_INTEGER,
  LCFG_RESOURCE_TYPE_BOOLEAN,
  LCFG_RESOURCE_TYPE_LIST,
  LCFG_RESOURCE_TYPE_PUBLISH,
  LCFG_RESOURCE_TYPE_SUBSCRIBE
} LCFGResourceType;

struct LCFGResource {
  char * name;
  char * value;
  LCFGTemplate * template;
  char * context;
  char * derivation;
  char * comment;
  LCFGResourceType type;
  int priority;
  unsigned int _refcount;
};

typedef struct LCFGResource LCFGResource;

LCFGResource * lcfgresource_new(void);

LCFGResource * lcfgresource_clone(const LCFGResource * res);

void lcfgresource_destroy(LCFGResource * res);

#define lcfgresource_inc_ref(res) (((res)->_refcount)++)
#define lcfgresource_dec_ref(res) (((res)->_refcount)--)

/* Resources: Names */

bool lcfgresource_valid_name( const char * name );

bool lcfgresource_has_name( const LCFGResource * res )
  __attribute__((nonnull (1)));

char * lcfgresource_get_name( const LCFGResource * res )
  __attribute__((nonnull (1)));

bool lcfgresource_set_name( LCFGResource * res,
                            char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

char * lcfgresource_build_name( const LCFGTemplate * templates,
                                const LCFGTagList  * taglist,
                                const char * field_name,
                                char ** msg );

/* Resources: Types */

LCFGResourceType lcfgresource_get_type( const LCFGResource * res )
  __attribute__((nonnull (1)));

bool lcfgresource_set_type( LCFGResource * res,
                            LCFGResourceType new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_set_type_as_string( LCFGResource * res,
                                      const char * new_value,
                                      char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

char * lcfgresource_get_type_as_string( const LCFGResource * res,
                                        unsigned int options )
  __attribute__((nonnull (1)));

bool lcfgresource_is_string(  const LCFGResource * res )
  __attribute__((nonnull (1)));

bool lcfgresource_is_integer( const LCFGResource * res )
  __attribute__((nonnull (1)));

bool lcfgresource_is_boolean( const LCFGResource * res )
  __attribute__((nonnull (1)));

bool lcfgresource_is_list(    const LCFGResource * res )
  __attribute__((nonnull (1)));

/* Resources: Templates */

bool lcfgresource_has_template( const LCFGResource * res )
  __attribute__((nonnull (1)));

LCFGTemplate * lcfgresource_get_template( const LCFGResource * res )
  __attribute__((nonnull (1)));

char * lcfgresource_get_template_as_string( const LCFGResource * res )
  __attribute__((nonnull (1)));

bool lcfgresource_set_template( LCFGResource * res,
                                LCFGTemplate * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_set_template_as_string( LCFGResource * res,
                                          const char * new_value,
                                          char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Resources: Values */

bool lcfgresource_valid_boolean( const char * value );
bool lcfgresource_valid_integer( const char * value );
bool lcfgresource_valid_list(    const char * value );

bool lcfgresource_valid_value_for_type( LCFGResourceType type,
                                        const char * value );

bool lcfgresource_valid_value( const LCFGResource * res,
                               const char * value )
  __attribute__((nonnull (1)));

char * lcfgresource_canon_boolean( const char * value );

bool lcfgresource_value_is_empty( const char * value );
bool lcfgresource_has_value( const LCFGResource * res )
  __attribute__((nonnull (1)));

char * lcfgresource_get_value( const LCFGResource * res )
  __attribute__((nonnull (1)));

bool lcfgresource_set_value( LCFGResource * res,
                             char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_unset_value( LCFGResource * res )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

char * lcfgresource_enc_value( const LCFGResource * res )
  __attribute__((nonnull (1)));

/* Resources: Value Mutations */

typedef bool (*LCFGResourceTagFunc)( LCFGResource * res, const char * tag );

bool lcfgresource_value_map_tagstring( LCFGResource * res,
                                       const char * tags,
                                       LCFGResourceTagFunc fn )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_value_append( LCFGResource * res,
                                const char * extra_string )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_value_prepend( LCFGResource * res,
                                 const char * extra_string )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_value_replace( LCFGResource * res,
                                 const char * old_string,
                                 const char * new_string )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_value_remove( LCFGResource * res,
                                const char * string )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

char * lcfgresource_value_find_tag( const LCFGResource * res,
                                    const char * tag )
  __attribute__((nonnull (1)));

bool lcfgresource_value_has_tag( const LCFGResource * res,
                                 const char * tag )
  __attribute__((nonnull (1)));

bool lcfgresource_value_append_tag( LCFGResource * res,
                                    const char * extra_tag )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_value_prepend_tag( LCFGResource * res,
                                     const char * extra_tag )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_value_replace_tag( LCFGResource * res,
                                     const char * old_tag,
                                     const char * new_tag )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_value_remove_tag( LCFGResource * res,
                                    const char * tag )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_value_remove_tags( LCFGResource * res,
                                     const char * unwanted_tags )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_value_add_tag( LCFGResource * res,
                                 const char * extra_tag )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_value_add_tags( LCFGResource * res,
                                  const char * extra_tags )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Resources: Derivations */

bool lcfgresource_has_derivation( const LCFGResource * res )
  __attribute__((nonnull (1)));

char * lcfgresource_get_derivation( const LCFGResource * res )
  __attribute__((nonnull (1)));

bool lcfgresource_set_derivation( LCFGResource * res, char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_add_derivation( LCFGResource * resource,
                                  const char * extra_deriv )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Resources: Contexts */

bool lcfgresource_valid_context( const char * expr );

bool lcfgresource_has_context( const LCFGResource * res )
  __attribute__((nonnull (1)));

char * lcfgresource_get_context( const LCFGResource * res )
  __attribute__((nonnull (1)));

bool lcfgresource_set_context( LCFGResource * res, char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_add_context( LCFGResource * res,
                               const char * extra_context )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Resources: Comments */

bool lcfgresource_has_comment( const LCFGResource * res )
    __attribute__((nonnull (1)));

char * lcfgresource_get_comment( const LCFGResource * res )
  __attribute__((nonnull (1)));

bool lcfgresource_set_comment( LCFGResource * res, char * new_value )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Resources: priority */

int lcfgresource_get_priority( const LCFGResource * res )
  __attribute__((nonnull (1)));

char * lcfgresource_get_priority_as_string( const LCFGResource * res )
  __attribute__((nonnull (1)));

bool lcfgresource_set_priority( LCFGResource * res, int priority )
  __attribute__((nonnull (1)));

bool lcfgresource_is_active( const LCFGResource * res )
  __attribute__((nonnull (1)));

bool lcfgresource_eval_priority( LCFGResource * res,
                                 const LCFGContextList * ctxlist,
                                 char ** msg )
  __attribute__((nonnull (1)));

/* Resources: Output */

bool lcfgresource_print( const LCFGResource * res,
                         const char * prefix,
                         const char * style,
                         unsigned int options,
                         FILE * out )
  __attribute__((nonnull (1,5))) __attribute__((warn_unused_result));

bool lcfgresource_to_env( const LCFGResource * res,
                          const char * prefix,
                          unsigned int options )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

ssize_t lcfgresource_to_string( const LCFGResource * res,
                                const char * prefix,
                                unsigned int options,
                                char ** str, size_t * size )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

ssize_t lcfgresource_to_status( const LCFGResource * res,
                                const char * prefix,
                                unsigned int options,
                                char ** str, size_t * size )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

/* Resources: Others */

int lcfgresource_compare_values( const LCFGResource * res1,
                                 const LCFGResource * res2 )
  __attribute__((nonnull (1,2)));

int lcfgresource_compare( const LCFGResource * res1,
                          const LCFGResource * res2 )
  __attribute__((nonnull (1,2)));

bool lcfgresource_same_value( const LCFGResource * res1,
                              const LCFGResource * res2 )
  __attribute__((nonnull (1,2)));

bool lcfgresource_same_type( const LCFGResource * res1,
                             const LCFGResource * res2 )
  __attribute__((nonnull (1,2)));

bool lcfgresource_equals( const LCFGResource * res1,
                          const LCFGResource * res2 )
  __attribute__((nonnull (1,2)));

char * lcfgresource_build_message( const LCFGResource * res,
                                   const char * component,
                                   const char *fmt, ... );

bool lcfgresource_parse_key( char  * key,
                             char ** namespace,
                             char ** component,
                             char ** resource,
                             char  * type )
  __attribute__((warn_unused_result));

ssize_t lcfgresource_compute_key_length( const LCFGResource * res,
                                         const char * component,
                                         const char * namespace,
                                         char type_symbol )
  __attribute__((nonnull (1)));

ssize_t lcfgresource_insert_key( const LCFGResource * res,
                                 const char * component,
                                 const char * namespace,
                                 char type_symbol,
                                 char * result )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

ssize_t lcfgresource_build_key( const LCFGResource * res,
                                const char * component,
                                const char * namespace,
                                char type_symbol,
                                char ** result,
                                size_t * size )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

bool lcfgresource_set_attribute( LCFGResource * res,
                                 char type_symbol,
                                 char * value,
                                 char ** msg )
  __attribute__((nonnull (1))) __attribute__((warn_unused_result));

#endif /* LCFG_RESOURCES_H */

/* eof */
