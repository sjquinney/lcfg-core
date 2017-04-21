/**
 * @file resources.h
 * @brief LCFG resource and component handling library
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date$
 * $Revision$
 */

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

typedef enum {
  LCFG_RESOURCE_STYLE_SPEC,
  LCFG_RESOURCE_STYLE_STATUS,
  LCFG_RESOURCE_STYLE_SUMMARY,
  LCFG_RESOURCE_STYLE_EXPORT
} LCFGResourceStyle;

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
void lcfgresource_acquire( LCFGResource * res );
void lcfgresource_relinquish( LCFGResource * res );
void lcfgresource_destroy(LCFGResource * res);

/* Resources: Names */

bool lcfgresource_valid_name( const char * name );
bool lcfgresource_has_name( const LCFGResource * res );
char * lcfgresource_get_name( const LCFGResource * res );
bool lcfgresource_set_name( LCFGResource * res,
                            char * new_value )
  __attribute__((warn_unused_result));

/* Resources: Types */

LCFGResourceType lcfgresource_get_type( const LCFGResource * res );
bool lcfgresource_set_type( LCFGResource * res,
                            LCFGResourceType new_value )
  __attribute__((warn_unused_result));

bool lcfgresource_set_type_as_string( LCFGResource * res,
                                      const char * new_value,
                                      char ** msg )
  __attribute__((warn_unused_result));

char * lcfgresource_get_type_as_string( const LCFGResource * res,
                                        LCFGOption options );

bool lcfgresource_is_string(  const LCFGResource * res );
bool lcfgresource_is_integer( const LCFGResource * res );
bool lcfgresource_is_boolean( const LCFGResource * res );
bool lcfgresource_is_list(    const LCFGResource * res );
bool lcfgresource_is_true( const LCFGResource * res );

/* Resources: Templates */

bool lcfgresource_has_template( const LCFGResource * res );
LCFGTemplate * lcfgresource_get_template( const LCFGResource * res );
char * lcfgresource_get_template_as_string( const LCFGResource * res );
bool lcfgresource_set_template( LCFGResource * res,
                                LCFGTemplate * new_value )
  __attribute__((warn_unused_result));

bool lcfgresource_set_template_as_string( LCFGResource * res,
                                          const char * new_value,
                                          char ** msg )
  __attribute__((warn_unused_result));

/* Resources: Values */

bool lcfgresource_valid_boolean( const char * value );
bool lcfgresource_valid_integer( const char * value );
bool lcfgresource_valid_list(    const char * value );

bool lcfgresource_valid_value_for_type( LCFGResourceType type,
                                        const char * value );

bool lcfgresource_valid_value( const LCFGResource * res,
                               const char * value );

char * lcfgresource_canon_boolean( const char * value );

bool lcfgresource_has_value( const LCFGResource * res );

char * lcfgresource_get_value( const LCFGResource * res );

bool lcfgresource_set_value( LCFGResource * res,
                             char * new_value )
  __attribute__((warn_unused_result));

bool lcfgresource_unset_value( LCFGResource * res )
  __attribute__((warn_unused_result));

char * lcfgresource_enc_value( const LCFGResource * res );

/* Resources: Value Mutations */

typedef bool (*LCFGResourceTagFunc)( LCFGResource * res, const char * tag );

bool lcfgresource_value_map_tagstring( LCFGResource * res,
                                       const char * tags,
                                       LCFGResourceTagFunc fn )
  __attribute__((warn_unused_result));

bool lcfgresource_value_append( LCFGResource * res,
                                const char * extra_string )
  __attribute__((warn_unused_result));

bool lcfgresource_value_prepend( LCFGResource * res,
                                 const char * extra_string )
  __attribute__((warn_unused_result));

bool lcfgresource_value_replace( LCFGResource * res,
                                 const char * old_string,
                                 const char * new_string )
  __attribute__((warn_unused_result));

bool lcfgresource_value_remove( LCFGResource * res,
                                const char * string )
  __attribute__((warn_unused_result));

char * lcfgresource_value_find_tag( const LCFGResource * res,
                                    const char * tag );

bool lcfgresource_value_has_tag( const LCFGResource * res,
                                 const char * tag );

bool lcfgresource_value_append_tag( LCFGResource * res,
                                    const char * extra_tag )
  __attribute__((warn_unused_result));

bool lcfgresource_value_prepend_tag( LCFGResource * res,
                                     const char * extra_tag )
  __attribute__((warn_unused_result));

bool lcfgresource_value_replace_tag( LCFGResource * res,
                                     const char * old_tag,
                                     const char * new_tag )
  __attribute__((warn_unused_result));

bool lcfgresource_value_remove_tag( LCFGResource * res,
                                    const char * tag )
  __attribute__((warn_unused_result));

bool lcfgresource_value_remove_tags( LCFGResource * res,
                                     const char * unwanted_tags )
  __attribute__((warn_unused_result));

bool lcfgresource_value_add_tag( LCFGResource * res,
                                 const char * extra_tag )
  __attribute__((warn_unused_result));

bool lcfgresource_value_add_tags( LCFGResource * res,
                                  const char * extra_tags )
  __attribute__((warn_unused_result));

/* Resources: Derivations */

bool lcfgresource_has_derivation( const LCFGResource * res );
char * lcfgresource_get_derivation( const LCFGResource * res );
bool lcfgresource_set_derivation( LCFGResource * res, char * new_value )
  __attribute__((warn_unused_result));
bool lcfgresource_add_derivation( LCFGResource * resource,
                                  const char * extra_deriv )
  __attribute__((warn_unused_result));

/* Resources: Contexts */

bool lcfgresource_valid_context( const char * expr );
bool lcfgresource_has_context( const LCFGResource * res );
char * lcfgresource_get_context( const LCFGResource * res );
bool lcfgresource_set_context( LCFGResource * res, char * new_value );
bool lcfgresource_add_context( LCFGResource * res,
                               const char * extra_context )
  __attribute__((warn_unused_result));

/* Resources: Comments */

bool lcfgresource_has_comment( const LCFGResource * res );
char * lcfgresource_get_comment( const LCFGResource * res );
bool lcfgresource_set_comment( LCFGResource * res, char * new_value );

/* Resources: priority */

int lcfgresource_get_priority( const LCFGResource * res );
char * lcfgresource_get_priority_as_string( const LCFGResource * res );
bool lcfgresource_set_priority( LCFGResource * res, int priority );
bool lcfgresource_is_active( const LCFGResource * res );

bool lcfgresource_eval_priority( LCFGResource * res,
                                 const LCFGContextList * ctxlist,
				 char ** msg )
  __attribute__((warn_unused_result));

/* Resources: Output */

bool lcfgresource_print( const LCFGResource * res,
                         const char * prefix,
                         const char * style,
                         LCFGOption options,
                         FILE * out )
  __attribute__((warn_unused_result));

LCFGStatus lcfgresource_from_env( const char * resname,
				  const char * val_pfx, const char * type_pfx,
				  LCFGResource ** result, char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgresource_to_env( const LCFGResource * res,
				const char * val_pfx, const char * type_pfx,
				LCFGOption options )
  __attribute__((warn_unused_result));

ssize_t lcfgresource_to_export( const LCFGResource * res,
                                const char * val_pfx, const char * type_pfx,
                                LCFGOption options,
                                char ** result, size_t * size )
  __attribute__((warn_unused_result));

ssize_t lcfgresource_to_spec( const LCFGResource * res,
                              const char * prefix,
                              LCFGOption options,
                              char ** str, size_t * size )
  __attribute__((warn_unused_result));

ssize_t lcfgresource_to_status( const LCFGResource * res,
                                const char * prefix,
                                LCFGOption options,
                                char ** str, size_t * size )
  __attribute__((warn_unused_result));

ssize_t lcfgresource_to_summary( const LCFGResource * res,
                                 const char * prefix,
                                 LCFGOption options,
                                 char ** result, size_t * size )
  __attribute__((warn_unused_result));

/* Resources: Others */

int lcfgresource_compare_values( const LCFGResource * res1,
                                 const LCFGResource * res2 );

int lcfgresource_compare( const LCFGResource * res1,
                          const LCFGResource * res2 );

bool lcfgresource_same_value( const LCFGResource * res1,
                              const LCFGResource * res2 );

bool lcfgresource_same_type( const LCFGResource * res1,
                             const LCFGResource * res2 );

bool lcfgresource_equals( const LCFGResource * res1,
                          const LCFGResource * res2 );

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
                                         char type_symbol );

ssize_t lcfgresource_insert_key( const LCFGResource * res,
                                 const char * component,
                                 const char * namespace,
                                 char type_symbol,
                                 char * result )
  __attribute__((warn_unused_result));

ssize_t lcfgresource_build_key( const LCFGResource * res,
                                const char * component,
                                const char * namespace,
                                char type_symbol,
                                char ** result,
                                size_t * size )
  __attribute__((warn_unused_result));

bool lcfgresource_set_attribute( LCFGResource * res,
                                 char type_symbol,
                                 char * value,
                                 char ** msg )
  __attribute__((warn_unused_result));

#endif /* LCFG_RESOURCES_H */

/* eof */
