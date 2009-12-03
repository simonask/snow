%code requires {
#include "snow/ast.h"
#include "snow/parser-intern.h"
}

%start program
%defines
%skeleton "glr.c"

%locations

%pure_parser
%glr-parser
%parse-param {YY_EXTRA_TYPE state}
%parse-param {void* scanner}
%lex-param {yyscan_t* scanner}

%token TOK_EOF 0 "end of file"

%left <node> TOK_END TOK_RETURN TOK_BREAK TOK_CONTINUE TOK_SELF
%left <value> TOK_INTEGER TOK_FLOAT TOK_STRING TOK_TRUE TOK_FALSE TOK_NIL
%left <symbol> TOK_IDENTIFIER
%left <symbol> TOK_OPERATOR_FOURTH
%left <symbol> TOK_OPERATOR_THIRD
%left <node> TOK_LOG_AND TOK_LOG_OR TOK_LOG_XOR TOK_LOG_NOT
%left <symbol> TOK_OPERATOR_SECOND
%left <symbol> TOK_OPERATOR_FIRST
%left TOK_DOT

%token <node> TOK_INTERPOLATION
%token '.' ',' '[' ']' '{' '}' '(' ')' ':' '#'
%token TOK_EOL TOK_DO TOK_UNLESS TOK_ELSE TOK_IF TOK_ELSEIF TOK_WHILE TOK_UNTIL

%type <node> statement conditional conditional_tail function control
             function_call assignment operation variable log_operation member
             naked_closure closure local literal expression atomic_expr
%type <sequence> sequence program parameters parameter_list arguments argument_list index_arguments scope
%type <symbol> identifier
%type <value> literal_value string_literal string_data symbol

%{

#include "snow/intern.h"
#include "snow/scanner.h"
#include "snow/parser-intern.h"
#include "snow/exception.h"
#include <stdio.h>

#ifdef DEBUG
#define YYERROR_VERBOSE
#endif

#define YYASSERT ASSERT

SnAstNode* snow_parse(const char* buf)
{
	SnParserState state;
	state.buf = buf;
	state.pos = 0;
	state.len = strlen(buf);
	state.streamname = "<TODO>";
	state.result = NULL;
	
	yylex_init(&state.yyscanner);
	yyset_extra(&state, state.yyscanner);
	yyparse(&state, state.yyscanner);
	SnAstNode* root = state.result;
	yylex_destroy(state.yyscanner);
	
	ASSERT(root->base.type == SN_AST_TYPE && root->type == SN_AST_FUNCTION);
	return root;
}

void yyerror(struct YYLTYPE* yylocp, YY_EXTRA_TYPE state, void* scanner, const char* yymsg) {
	snow_throw_exception_with_description("Parser error: %s", yymsg);
}

%}

%%

ignore_eol:
          | ignore_eol eol
          ;

eol: TOK_EOL
   ;

program:    sequence TOK_EOF                                { state->result = $$ = snow_ast_function("<noname>", state->streamname, NULL, $1); }
            ;

statement: function
         | conditional
         | TOK_WHILE expression eol sequence TOK_END { $$ = snow_ast_loop($2, $4); }
         | TOK_UNTIL expression eol sequence TOK_END { $$ = snow_ast_loop(snow_ast_not($2), $4); }
         | function TOK_WHILE expression                 { $$ = snow_ast_loop($3, $1); }
         | function TOK_UNTIL expression                 { $$ = snow_ast_loop(snow_ast_not($3), $1); }
         ;

conditional: TOK_IF expression eol sequence eol conditional_tail                { $$ = snow_ast_if_else($2, $4, $6); }
             | TOK_UNLESS expression eol sequence eol conditional_tail          { $$ = snow_ast_if_else(snow_ast_not($2), $4, $6); }
             | function TOK_IF expression                                               { $$ = snow_ast_if_else($3, $1, snow_ast_literal(SN_NIL)); }
             | function TOK_UNLESS expression                                           { $$ = snow_ast_if_else(snow_ast_not($3), $1, snow_ast_literal(SN_NIL)); }
             ;

conditional_tail: TOK_END                                                       { $$ = NULL; }
                  | TOK_ELSE eol sequence eol TOK_END                   { $$ = $3; }
                  | TOK_ELSEIF expression eol sequence eol conditional_tail     { $$ = snow_ast_if_else($2, $4, $6); }
                  ;


sequence: /* Nothing */                                     { $$ = snow_ast_sequence(0); }
        | statement                                         { $$ = snow_ast_sequence(1, $1); }
        | sequence eol statement ignore_eol                 { $$ = $1; snow_ast_sequence_push($1, $3); }
        ;

function: expression | control;

control: TOK_RETURN                                         { $$ = snow_ast_return(NULL); }
       | TOK_RETURN expression                              { $$ = snow_ast_return($2); }
       | TOK_BREAK                                          { /* TODO: create a BREAK ast node */ }
       | TOK_CONTINUE                                       { /* TODO: create a CONTINUE ast node */ }
       ;
            
member: TOK_DOT identifier                                  { $$ = snow_ast_member(snow_ast_self(), $2); }
      | expression TOK_DOT identifier                       { $$ = snow_ast_member($1, $3); }
      ;

identifier: TOK_IDENTIFIER                                  { /* TODO: register symbol */ }
          ;

local: identifier                                           { $$ = snow_ast_local($1); }
     ;
            
variable: local | member | TOK_SELF
        | atomic_expr index_arguments                    { $$ = snow_ast_call(snow_ast_member($1, snow_symbol("[]")), $2); }
        ;

argument_list: expression                                   { $$ = snow_ast_sequence(1, $1); }
             | argument_list ',' expression                 { $$ = $1; snow_ast_sequence_push($$, $3); }
             ;

arguments: '(' ')'                                         { $$ = snow_ast_sequence(0); }
         | '(' argument_list ')'                           { $$ = $2; }
         ;

index_arguments: '[' argument_list ']'                     { $$ = $2; }
               ;

scope: '{' sequence '}'                                    { $$ = $2; }
     ;

parameter_list: identifier                                  { $$ = snow_ast_sequence(1, symbol_to_value($1)); }
              | parameter_list ',' identifier               { snow_ast_sequence_push($1, symbol_to_value($3)); $$ = $1; }
              ;

parameters: '[' ']'                                        { $$ = NULL; }
          | '[' parameter_list ']'                         { $$ = $2; }
          ;

naked_closure: scope                                       { $$ = snow_ast_function("<unnamed>", state->streamname, NULL, $1); }
             ;

closure: naked_closure
       | parameters scope                                  { $$ = snow_ast_function("<unnamed>", state->streamname, $1, $2); }
       ;

symbol:     '#' identifier                                 { $$ = symbol_to_value($2); }
            | '#' TOK_STRING                               { $$ = symbol_to_value(snow_symbol_from_string($2)); }
            ;

literal_value:    TOK_INTEGER | TOK_FLOAT | TOK_TRUE | TOK_FALSE | TOK_NIL | string_literal | symbol;
literal:          literal_value                             { $$ = snow_ast_literal($1); }
       ;

string_data: TOK_STRING;

string_literal: string_data
            | string_literal string_data                    { $$ = snow_ast_call(snow_ast_member($1, snow_symbol("+")), snow_ast_sequence(1, $2)); }
            ;

function_call: atomic_expr arguments                        { $$ = snow_ast_call($1, $2); }
             | atomic_expr arguments naked_closure          { snow_ast_sequence_push($2, $3); $$ = snow_ast_call($1, $2); }
             | atomic_expr naked_closure                    { $$ = snow_ast_call($1, snow_ast_sequence(1, $2)); }
             ;

assignment: identifier ':' expression                       { $$ = snow_ast_local_assign($1, $3); }
          | member ':' expression                           { $$ = snow_ast_member_assign($1, $3); }
          | atomic_expr index_arguments ':' expression      { snow_ast_sequence_push($2, $4); $$ = snow_ast_call(snow_ast_member($1, snow_symbol("[]:")), $2); }
          ;

log_operation: expression TOK_LOG_AND expression                { $$ = snow_ast_and($1, $3); }
            | expression TOK_LOG_OR expression                  { $$ = snow_ast_or($1, $3); }
            | expression TOK_LOG_XOR expression                 { $$ = snow_ast_xor($1, $3); }
            | TOK_LOG_NOT expression                            { $$ = snow_ast_not($2); }
            ;

operation:  TOK_OPERATOR_FIRST expression                       { $$ = snow_ast_call(snow_ast_member($2, $1), NULL); }
            | TOK_OPERATOR_SECOND expression                    { $$ = snow_ast_call(snow_ast_member($2, $1), NULL); }
            | TOK_OPERATOR_THIRD expression                     { $$ = snow_ast_call(snow_ast_member($2, $1), NULL); }
            | TOK_OPERATOR_FOURTH expression                    { $$ = snow_ast_call(snow_ast_member($2, $1), NULL); }
            | expression TOK_OPERATOR_FIRST expression          { $$ = snow_ast_call(snow_ast_member($1, $2), snow_ast_sequence(1, $3)); }
            | expression TOK_OPERATOR_SECOND expression         { $$ = snow_ast_call(snow_ast_member($1, $2), snow_ast_sequence(1, $3)); }
            | expression TOK_OPERATOR_THIRD expression          { $$ = snow_ast_call(snow_ast_member($1, $2), snow_ast_sequence(1, $3)); }
            | expression TOK_OPERATOR_FOURTH expression         { $$ = snow_ast_call(snow_ast_member($1, $2), snow_ast_sequence(1, $3)); }
            ;

atomic_expr: literal
           | variable
           | closure
           | '(' expression ')'                                 { $$ = $2; }
           | function_call
           ;

expression: assignment %dprec 9999
          | log_operation
          | operation %dprec 1
          | atomic_expr %dprec 1
          ;

%%
