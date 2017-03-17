%{
  #include <stdio.h>
  #include "context.h"
  int lcfgctx_yylex (void *, void * ctxscanner );
  void lcfgctx_yyerror ( void * ctxscanner,
                         const LCFGContextList * ctxlist, int * priority, 
                         const char * msg, ... );
  static void release_var(char * var);
%}

%code top {
#include "context.h"
}

%define api.pure full
%lex-param {void * ctxscanner}
%parse-param {void * ctxscanner}
%parse-param {const LCFGContextList * ctxlist}
%parse-param {int * priority}
%name-prefix "lcfgctx_yy"
%output  "ctxparser.c"
%defines "ctxparser.h"

%union
{
  int intValue;
  unsigned int boolValue;
  char *stringValue;
}

%token <stringValue> CTX_NAME

%token <stringValue> VALUE_STRING

%token <boolValue> VALUE_BOOL

%token CMP_EQ
%token CMP_NE

%token OP_OR
%token OP_AND
%right OP_NOT

%token END 0

%type <intValue> expression term

%%

query:
  /* empty */       { *priority = 0; }
  | expression      { printf("result: %d\n", $expression);
                      *priority = $expression; }
  ;

expression:
    expression OP_OR term   { printf(" ... or ... \n"); $<intValue>$ = 0; }
  | expression OP_AND term  { printf(" ... and ... \n"); $<intValue>$ = 0; }
  | term { $<intValue>$ = $term; }
  ;

term:
    '(' expression ')'            { $<intValue>$ = $expression; }
  | OP_NOT '(' expression ')'     { $<intValue>$ = -1 * $expression; }
  | CTX_NAME                      {
				    $<intValue>$ = lcfgctxlist_simple_query(
                                                    ctxlist,
                                                    $<stringValue>1,
                                                    NULL,
                                                    LCFG_TEST_ISTRUE );

                                    release_var($<stringValue>1);
                                  }
  | OP_NOT CTX_NAME               {
				    $<intValue>$ = lcfgctxlist_simple_query(
                                                    ctxlist,
                                                    $<stringValue>2,
                                                    NULL,
                                                    LCFG_TEST_ISFALSE );

                                    release_var($<stringValue>2);
                                  }
  | CTX_NAME CMP_EQ VALUE_STRING  {
				    $<intValue>$ = lcfgctxlist_simple_query(
                                                    ctxlist,
                                                    $<stringValue>1,
                                                    $<stringValue>3,
                                                    LCFG_TEST_ISEQ );

                                    release_var($<stringValue>1);
                                    release_var($<stringValue>3);
                                  }
  | CTX_NAME CMP_NE VALUE_STRING  {
				    $<intValue>$ = lcfgctxlist_simple_query(
                                                    ctxlist,
                                                    $<stringValue>1,
                                                    $<stringValue>3,
                                                    LCFG_TEST_ISNE );

                                    release_var($<stringValue>1);
                                    release_var($<stringValue>3);
                                  }
  | CTX_NAME CMP_EQ CTX_NAME      {
				    $<intValue>$ = lcfgctxlist_simple_query(
                                                    ctxlist,
                                                    $<stringValue>1,
                                                    $<stringValue>3,
                                                    LCFG_TEST_ISEQ );

                                    release_var($<stringValue>1);
                                    release_var($<stringValue>3);
                                  }
  | CTX_NAME CMP_NE CTX_NAME      {
				    $<intValue>$ = lcfgctxlist_simple_query(
                                                    ctxlist,
                                                    $<stringValue>1,
                                                    $<stringValue>3,
                                                    LCFG_TEST_ISNE );

                                    release_var($<stringValue>1);
                                    release_var($<stringValue>3);
                                  }
  | CTX_NAME CMP_EQ VALUE_BOOL    {
                                    LCFGTest cmp = ( $<boolValue>3 == 1 ?
                                                    LCFG_TEST_ISTRUE   :
                                                    LCFG_TEST_ISFALSE );

				    $<intValue>$ = lcfgctxlist_simple_query(
                                                    ctxlist,
                                                    $<stringValue>1,
                                                    NULL,
                                                    cmp );

                                    release_var($<stringValue>1);
                                  }
  | CTX_NAME CMP_NE VALUE_BOOL    {
                                    LCFGTest cmp = ( $<boolValue>3 == 1 ?
                                                    LCFG_TEST_ISTRUE   :
                                                    LCFG_TEST_ISFALSE );

				    $<intValue>$ = lcfgctxlist_simple_query(
                                                    ctxlist,
                                                    $<stringValue>1,
                                                    NULL,
                                                    cmp ) * -1;

                                    release_var($<stringValue>1);
                                  }
;

%%

static void release_var(char * var) {
  free(var);
  var = NULL;
}
