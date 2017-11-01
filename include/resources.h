/**
 * @file resources.h
 * @brief Functions for working with LCFG resources
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#ifndef LCFG_CORE_RESOURCES_H
#define LCFG_CORE_RESOURCES_H

#include <stdio.h>
#include <stdbool.h>

#include "common.h"
#include "context.h"
#include "tags.h"
#include "templates.h"

/* Resources */

#define LCFG_RESOURCE_NOVALUE ""

/* Used when creating environment variables from resources */

#define LCFG_RESOURCE_ENV_VAL_PFX  "LCFG_%s_"
#define LCFG_RESOURCE_ENV_TYPE_PFX "LCFGTYPE_%s_"
#define LCFG_RESOURCE_ENV_PHOLDER  "%s"
#define LCFG_RESOURCE_ENV_LISTKEY  "_RESOURCES"

/* These are marker symbols for the keys which are used when resources
   are serialised into storage such as Berkeley DB or status files. */

#define LCFG_RESOURCE_SYMBOL_DERIVATION '#'
#define LCFG_RESOURCE_SYMBOL_TYPE       '%'
#define LCFG_RESOURCE_SYMBOL_CONTEXT    '='
#define LCFG_RESOURCE_SYMBOL_PRIORITY   '^'
#define LCFG_RESOURCE_SYMBOL_VALUE      '\0'

/*
 * @brief The standard LCFG resource types
 */

typedef enum {
  LCFG_RESOURCE_TYPE_STRING,      /**< String, can hold any value */
  LCFG_RESOURCE_TYPE_INTEGER,     /**< Integer */
  LCFG_RESOURCE_TYPE_BOOLEAN,     /**< Boolean */
  LCFG_RESOURCE_TYPE_LIST,        /**< List of tag names */
  LCFG_RESOURCE_TYPE_PUBLISH,     /**< Published to spanning map, like String */
  LCFG_RESOURCE_TYPE_SUBSCRIBE    /**< Subscribed from spanning map, like String*/
} LCFGResourceType;

#define LCFG_RESOURCE_DEFAULT_TYPE     LCFG_RESOURCE_TYPE_STRING
#define LCFG_RESOURCE_DEFAULT_PRIORITY 0

/**
 * @brief Resource format styles
 *
 * This can be used to specify the format 'style' which is used when a
 * resource is serialised as a string.
 *
 */

typedef enum {
  LCFG_RESOURCE_STYLE_SPEC,       /**< Standard LCFG resource specification */
  LCFG_RESOURCE_STYLE_STATUS,     /**< LCFG status block (as used by components) */
  LCFG_RESOURCE_STYLE_SUMMARY,    /**< qxprof style summary */
  LCFG_RESOURCE_STYLE_EXPORT      /**< Environment variables for shell evaluation */
} LCFGResourceStyle;

/**
 * @brief A structure to represent an LCFG resource
 *
 * This structure supports reference counting so a resource may appear in
 * multiple components. See @c lcfgresource_acquire() and 
 * @c lcfgresource_relinquish() for details.
 *
 */

struct LCFGResource {
  /*@{*/
  char * name;                    /**< Name (required) */
  char * value;                   /**< Value - validated according to type */
  LCFGTemplate * template;        /**< Templates - used when list type */
  char * context;                 /**< Context expression - when the resource is applicable */
  char * derivation;              /**< Derivation - where the resource was specified */
  char * comment;                 /**< Any comments associated with the type information */
  LCFGResourceType type;          /**< Type - see LCFGResourceType for list of supported types */
  int priority;                   /**< Priority - result of evaluating context expression, used for merge conflict resolution */
  /*@}*/
  unsigned int _refcount;
};

typedef struct LCFGResource LCFGResource;

LCFGResource * lcfgresource_new(void);

LCFGResource * lcfgresource_clone(const LCFGResource * res);
void lcfgresource_acquire( LCFGResource * res );
void lcfgresource_relinquish( LCFGResource * res );
void lcfgresource_destroy(LCFGResource * res);

bool lcfgresource_is_valid( const LCFGResource * res );

/* Resources: Names */

bool lcfgresource_valid_name( const char * name )
    __attribute__((pure));

bool lcfgresource_has_name( const LCFGResource * res );
const char * lcfgresource_get_name( const LCFGResource * res );
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

bool lcfgresource_valid_boolean( const char * value )
    __attribute__((pure));

bool lcfgresource_valid_integer( const char * value )
    __attribute__((pure));

bool lcfgresource_valid_list(    const char * value )
  __attribute__((pure));

bool lcfgresource_valid_value_for_type( LCFGResourceType type,
                                        const char * value )
  __attribute__((pure));

bool lcfgresource_valid_value( const LCFGResource * res,
                               const char * value );

char * lcfgresource_canon_boolean( const char * value )
  __attribute__((pure));

bool lcfgresource_has_value( const LCFGResource * res );

const char * lcfgresource_get_value( const LCFGResource * res );

bool lcfgresource_set_value( LCFGResource * res,
                             char * new_value )
  __attribute__((warn_unused_result));

bool lcfgresource_unset_value( LCFGResource * res )
  __attribute__((warn_unused_result));

bool lcfgresource_value_needs_encode( const LCFGResource * res );

char * lcfgresource_enc_value( const LCFGResource * res );

/* Resources: Value Mutations */

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
const char * lcfgresource_get_derivation( const LCFGResource * res );
bool lcfgresource_set_derivation( LCFGResource * res, char * new_value )
  __attribute__((warn_unused_result));
bool lcfgresource_add_derivation( LCFGResource * resource,
                                  const char * extra_deriv )
  __attribute__((warn_unused_result));

/* Resources: Contexts */

bool lcfgresource_valid_context( const char * expr )
    __attribute__((pure));
bool lcfgresource_has_context( const LCFGResource * res );
const char * lcfgresource_get_context( const LCFGResource * res );
bool lcfgresource_set_context( LCFGResource * res, char * new_value );
bool lcfgresource_add_context( LCFGResource * res,
                               const char * extra_context )
  __attribute__((warn_unused_result));

/* Resources: Comments */

bool lcfgresource_has_comment( const LCFGResource * res );
const char * lcfgresource_get_comment( const LCFGResource * res );
bool lcfgresource_set_comment( LCFGResource * res, char * new_value );

/* Resources: priority */

int lcfgresource_get_priority( const LCFGResource * res );
char * lcfgresource_get_priority_as_string( const LCFGResource * res );
bool lcfgresource_set_priority( LCFGResource * res, int priority )
  __attribute__((warn_unused_result));
bool lcfgresource_set_priority_default( LCFGResource * res )
  __attribute__((warn_unused_result));

bool lcfgresource_is_active( const LCFGResource * res );

bool lcfgresource_eval_priority( LCFGResource * res,
                                 const LCFGContextList * ctxlist,
				 char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgresource_from_spec( const char * spec, LCFGResource ** result,
				   char ** hostname, char ** compname,
				   char ** msg )
  __attribute__((warn_unused_result));

/* Resources: Output */

bool lcfgresource_print( const LCFGResource * res,
                         const char * prefix,
                         LCFGResourceStyle style,
                         LCFGOption options,
                         FILE * out )
  __attribute__((warn_unused_result));

bool lcfgresource_build_env_prefix( const char * prefix,
                                    const char * compname,
                                    char ** result );

LCFGStatus lcfgresource_from_env( const char * resname,
                                  const char * compname,
				  const char * val_pfx, const char * type_pfx,
				  LCFGResource ** result,
                                  LCFGOption options, char ** msg )
  __attribute__((warn_unused_result));

LCFGStatus lcfgresource_to_env( const LCFGResource * res,
                                const char * compname,
				const char * val_pfx, const char * type_pfx,
				LCFGOption options )
  __attribute__((warn_unused_result));

#define LCFG_RES_TOSTR_ARGS \
    const LCFGResource * res,\
    const char * prefix,\
    LCFGOption options,\
    char ** result, size_t * size 

typedef ssize_t (*LCFGResStrFunc) ( LCFG_RES_TOSTR_ARGS );

LCFGStatus lcfgresource_to_string( const LCFGResource * res,
                                   const char * prefix,
                                   LCFGResourceStyle style,
                                   LCFGOption options,
                                   char ** result, size_t * size )
 __attribute__((warn_unused_result));

ssize_t lcfgresource_to_spec( LCFG_RES_TOSTR_ARGS )
  __attribute__((warn_unused_result));

ssize_t lcfgresource_to_status( LCFG_RES_TOSTR_ARGS )
  __attribute__((warn_unused_result));

ssize_t lcfgresource_to_summary( LCFG_RES_TOSTR_ARGS )
  __attribute__((warn_unused_result));

ssize_t lcfgresource_to_export( const LCFGResource * res,
                                const char * compname,
                                const char * val_pfx, const char * type_pfx,
                                LCFGOption options,
                                char ** result, size_t * size )
  __attribute__((warn_unused_result));

/* Resources: Others */

bool lcfgresource_match( const LCFGResource * res, const char * name );

int lcfgresource_compare_names( const LCFGResource * res1, 
                                const LCFGResource * res2 );

int lcfgresource_compare_values( const LCFGResource * res1,
                                 const LCFGResource * res2 );

int lcfgresource_compare( const LCFGResource * res1,
                          const LCFGResource * res2 );

bool lcfgresource_same_name( const LCFGResource * res1, 
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
                             const char ** namespace,
                             const char ** component,
                             const char ** resource,
                             char  * type )
  __attribute__((warn_unused_result));

LCFGStatus lcfgresource_parse_spec( char * spec,
                                    const char ** hostname,
                                    const char ** compname,
                                    const char ** resname,
                                    const char ** value,
                                    char  * type,
                                    char ** msg )
  __attribute__((warn_unused_result));

ssize_t lcfgresource_compute_key_length( const char * resource,
                                         const char * component,
                                         const char * namespace,
                                         char type_symbol );

ssize_t lcfgresource_insert_key( const char * resource,
                                 const char * component,
                                 const char * namespace,
                                 char type_symbol,
                                 char * result )
  __attribute__((warn_unused_result));

ssize_t lcfgresource_build_key( const char * resource,
                                const char * component,
                                const char * namespace,
                                char type_symbol,
                                char ** result,
                                size_t * size )
  __attribute__((warn_unused_result));

bool lcfgresource_set_attribute( LCFGResource * res,
                                 char type_symbol,
                                 const char * value,
				 size_t value_len,
                                 char ** msg )
  __attribute__((warn_unused_result));

unsigned long lcfgresource_hash( const LCFGResource * res );

#endif /* LCFG_CORE_RESOURCES_H */

/* eof */
