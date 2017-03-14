%{
  #include <stdio.h>
  int lcfgctx_yylex (void *, void * ctxscanner );
  void lcfgctx_yyerror (void * ctxscanner, int * priority, char const * msg);
%}

%define api.pure
%lex-param {void * ctxscanner}
%parse-param {void * ctxscanner} {int *priority}
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

%token END 0 "end of data"

%type <intValue> expression term

%%

query:
  %empty            { *priority = 0; }
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
  | CTX_NAME                      { printf( "NAME: %s\n", $<stringValue>1 );
				    $<intValue>$ = 1; }
  | OP_NOT CTX_NAME               { printf( "NOT NAME: %s\n", $<stringValue>2 );
                                    $<intValue>$ = -1; }
  | CTX_NAME CMP_EQ VALUE_STRING  { printf( "%s eq %s\n", $<stringValue>1, $<stringValue>3 );
				    $<intValue>$ = 0; }
  | CTX_NAME CMP_NE VALUE_STRING  { printf( "%s ne %s\n", $<stringValue>1, $<stringValue>3 );
				    $<intValue>$ = 0; }
  | CTX_NAME CMP_EQ CTX_NAME      { printf( "%s eq %s\n", $<stringValue>1, $<stringValue>3 );
				    $<intValue>$ = 0; }
  | CTX_NAME CMP_NE CTX_NAME      { printf( "%s ne %s\n", $<stringValue>1, $<stringValue>3 );
				    $<intValue>$ = 0; }
  | CTX_NAME CMP_EQ VALUE_BOOL    { printf( "%s eq %u\n", $<stringValue>1, $<boolValue>3 );
                                    $<intValue>$ = $<boolValue>3 == 0 ? 0 : 1; }
  | CTX_NAME CMP_NE VALUE_BOOL    { printf( "%s ne %u\n", $<stringValue>1, $<boolValue>3 );
				    $<intValue>$ = $<boolValue>3 == 1 ? 0 : 1; }
;

%%


