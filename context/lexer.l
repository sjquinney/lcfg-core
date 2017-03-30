%{
#define _GNU_SOURCE /* for vasprintf */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "context.h"
#include "parser.h"

/* Used to handle the contents of quoted strings */

struct lcfgctx_yylex_extra {
  char * strbuf;
  char * errmsg;
  size_t strbuflen;
  size_t strbufalloc;
};
typedef struct lcfgctx_yylex_extra lcfgctx_yylex_extra_type;

#define strbuf_get() ( yyextra->strbuf )
static void strbuf_init( yyscan_t yyscanner );
static void strbuf_append( yyscan_t yyscanner, const char *ytext, int yleng );
void lcfgctx_yyerror( yyscan_t ctxscanner,
                      const LCFGContextList * ctxlist, int * priority,
                      const char * s, ... );
%}

%option header-file="lexer.h"
%option prefix="lcfgctx_yy"
%option warn
%option nodefault
%option noinput
%option nounput
%option noyywrap
%option stack
%option reentrant
%option extra-type="lcfgctx_yylex_extra_type *"
%option bison-bridge

/* Double quote state */
%x xd
/* Single quote state */
%x xs

/* Context name - must be at least 1 char and start with letter */

context_name            [a-zA-Z][a-zA-Z0-9_]*

/* Simple value - may be left unquoted if they do not clash with
   reserved words */

simple_value            [a-zA-Z0-9_]+

/* Comparison operators */

cmp_eq                  =
cmp_ne                  !=

/* Boolean logic operators */

op_not                  \!
op_or                   \|
op_and                  &
op_xor                  \^

/* Boolean values */

bool_true               yes|true|on
bool_false              no|false|off
bool                    {bool_true}|{bool_false}

/* Quoted string support */

dquote                  \"
xdstart			{dquote}
xdstop			{dquote}
xddouble		{dquote}{dquote}
xdinside		[^"\n\r]+
                   /* " hack around emacs syntax highlighter */

squote                  \'
xsstart			{squote}
xsstop			{squote}
xsdouble		{squote}{squote}
xsinside		[^'\n\r]+
                   /* ' hack around emacs syntax highlighter */

/* Other stuff */

paren_left              \(
paren_right             \)
paren                   [\(\)]
whitespace              [ \t\n\r\f]
newline                 [\r\n]

%%

(?i:{bool_true})          {
                            yylval->boolValue = 1;
                            return VALUE_BOOL;
                          }

(?i:{bool_false})         {
                            yylval->boolValue = 0;
                            return VALUE_BOOL;
                          }

{cmp_ne}                  return CMP_NE;

{cmp_eq}                  return CMP_EQ;

{op_or}                   return OP_OR;

{op_xor}                  return OP_XOR;

{op_not}                  return OP_NOT;

{op_and}                  return OP_AND;

{xdstart}              {
                         yy_push_state( xd, yyscanner ); 
                         strbuf_init(yyscanner);
                       }
<xd>{
     {xdinside}        { strbuf_append( yyscanner, yytext, yyleng ); }
     {xddouble}        { strbuf_append( yyscanner, "\"", 1 ); }
     {xdstop}          {
                         yy_pop_state( yyscanner );
			 yylval->stringValue = strdup(strbuf_get());
			 return VALUE_STRING;
                       }
     <<EOF>>           {
                         lcfgctx_yyerror( yyscanner, NULL, NULL,
                                          "unterminated double-quoted string");
                         return END;
                       }
     {newline}+        {
                         lcfgctx_yyerror( yyscanner, NULL, NULL,
                                          "unterminated double-quoted string");
                                          return END;
                       }
}

{xsstart}              {
                         yy_push_state( xs, yyscanner ); 
                         strbuf_init(yyscanner);
                       }
<xs>{
     {xsinside}        { strbuf_append( yyscanner, yytext, yyleng ); }
     {xsdouble}        { strbuf_append( yyscanner, "'", 1 ); }
     {xsstop}          {
                         yy_pop_state( yyscanner );
			 yylval->stringValue = strdup(strbuf_get());
			 return VALUE_STRING;

                       }
     <<EOF>>           {
                         lcfgctx_yyerror( yyscanner, NULL, NULL,
                                          "unterminated single-quoted string");
                         return END;
                       }
     {newline}+        {
                         lcfgctx_yyerror( yyscanner, NULL, NULL,
                                          "unterminated single-quoted string");
                         return END;
                       }
}

{paren}	                 return yytext[0];
			 
{context_name}         {
                         yylval->stringValue = strndup( yytext, yyleng );
                         return CTX_NAME;
                       }

{simple_value}         {
                         yylval->stringValue = strndup( yytext, yyleng );
			 return VALUE_STRING;
                       }

{whitespace}+          /* eat whitespace */

.                      {
                         lcfgctx_yyerror( yyscanner, NULL, NULL,
                                          "Invalid character '%s'", yytext );
                         return yytext[0];
                       }
%%

void lcfgctx_yyerror( yyscan_t ctxscanner,
                      const LCFGContextList * ctxlist, int * priority,
                      const char * fmt, ... ) {

  va_list ap;
  va_start( ap, fmt );

  lcfgctx_yylex_extra_type * extra = lcfgctx_yyget_extra(ctxscanner);

  /* Avoid being called twice by both flex and bison */
  if ( extra->errmsg == NULL ) {
    int rc = vasprintf( &(extra->errmsg), fmt, ap );
    if ( rc < 0 ) {
      perror("Failed to allocate memory for error string");
      exit(EXIT_FAILURE);
    }
  }

}

static void strbuf_init( yyscan_t ctxscanner ) {

  lcfgctx_yylex_extra_type * extra = lcfgctx_yyget_extra(ctxscanner);

  /* Initial size for buffer is chosen to be suitable for most cases.
     Usually it's just a 'boolean' such as yes/no/true/false/off/on */

  if ( extra->strbuf == NULL || extra->strbufalloc == 0 ) {
    extra->strbufalloc = 8;
    extra->strbuf = calloc( extra->strbufalloc, sizeof(char *) );
    if ( extra->strbuf == NULL ) {
      perror("Failed to allocate memory for LCFG context string");
      exit(EXIT_FAILURE);
    }
  }

  *( extra->strbuf ) = '\0';
  extra->strbuflen = 0;

}

static void strbuf_append( yyscan_t ctxscanner,
                           const char * text, int len ) {

  lcfgctx_yylex_extra_type * extra = lcfgctx_yyget_extra(ctxscanner);

  size_t size_required = extra->strbuflen + len + 1;

  /* enlarge buffer if needed - doubling to avoid frequent reallocs */

  size_t new_size = extra->strbufalloc;
  while ( size_required > new_size ) new_size *= 2;

  if ( new_size > extra->strbufalloc ) {

    extra->strbuf = realloc( extra->strbuf, new_size * sizeof(char *) );
    if ( extra->strbuf == NULL ) {
      perror("Failed to allocate memory for LCFG context string");
      exit(EXIT_FAILURE);
    }
    extra->strbufalloc = new_size;
  }

  memcpy( extra->strbuf + extra->strbuflen, text, len );
  extra->strbuflen += len;
  *(extra->strbuf + extra->strbuflen) = '\0';

}

yyscan_t lcfgctx_scanner_init(void) {

  yyscan_t ctxscanner;
  lcfgctx_yylex_init( &ctxscanner );

  /* extra data */

  lcfgctx_yylex_extra_type * yyext =
    malloc( sizeof(lcfgctx_yylex_extra_type) );
  if ( yyext == NULL ) {
    perror( "Failed to allocate memory for LCFG context" );
    exit(EXIT_FAILURE);
  }

  yyext->errmsg      = NULL;
  yyext->strbuf      = NULL;
  yyext->strbuflen   = 0;
  yyext->strbufalloc = 0;

  lcfgctx_yyset_extra( yyext, ctxscanner);

  return ctxscanner;
}

void lcfgctx_scanner_destroy(yyscan_t ctxscanner) {

  lcfgctx_yylex_extra_type * extra = lcfgctx_yyget_extra(ctxscanner);

  free(extra->errmsg);
  extra->errmsg = NULL;
  free(extra->strbuf);
  extra->strbuf = NULL;
  free(extra);

  lcfgctx_yylex_destroy(ctxscanner);
  ctxscanner = NULL;

}

char * lcfgctx_scanner_errmsg(yyscan_t ctxscanner) {

  lcfgctx_yylex_extra_type * extra = lcfgctx_yyget_extra(ctxscanner);

  char * errmsg = NULL;
  if ( extra->errmsg != NULL )
    errmsg = strdup(extra->errmsg);

  return errmsg;
}

/* eof */
