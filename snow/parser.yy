%code requires {
#include "snow/ast.h"
#include "snow/intern.h"
#include "snow/parser-intern.h"
}

%start program
%defines
%skeleton "glr.c"

%locations

%pure_parser
%parse-param {YY_EXTRA_TYPE state}
%parse-param {void* scanner}
%lex-param {yyscan_t* scanner}

%token TOK_EOF 0

%left <node> TOK_END TOK_RETURN TOK_BREAK TOK_CONTINUE TOK_SELF TOK_LOG_AND TOK_LOG_OR TOK_LOG_XOR TOK_LOG_NOT
%left <value> TOK_INTEGER TOK_FLOAT TOK_STRING TOK_TRUE TOK_FALSE TOK_NIL 
%left <symbol> TOK_IDENTIFIER TOK_OPERATOR_FOURTH TOK_OPERATOR_THIRD TOK_OPERATOR_SECOND TOK_OPERATOR_FIRST

%token <node> TOK_INTERPOLATION
%token '.' ',' '[' ']' '{' '}' '(' ')' ':' '#'
%token TOK_EOL TOK_DO TOK_UNLESS TOK_ELSE TOK_IF TOK_ELSEIF TOK_WHILE TOK_UNTIL

%type <node> statement conditional conditional_tail function command return_cmd expression
             function_call assignment operation variable log_operation member
             sequence 
             program parameters arg_list closure scope
             arguments local literal
%type <symbol> identifier
%type <value> literal_value string_literal string_data symbol

%expect 115

%{

#include "snow/intern.h"
#include "snow/scanner.h"
#include "snow/parser-intern.h"
#include <stdio.h>

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

void yyerror() { fprintf(stderr, "PARSER ERROR!!\n"); }

%}

%%

program:    sequence                                        { state->result = $$ = snow_ast_function("<noname>", state->streamname, NULL, $1); }
            ;

statement:  function                                        { $$ = $1; }
            | conditional                                   { $$ = $1; }
            | TOK_WHILE expression TOK_EOL sequence TOK_END { $$ = snow_ast_loop($2, $4); }
            | TOK_UNTIL expression TOK_EOL sequence TOK_END { $$ = snow_ast_loop(snow_ast_not($2), $4); }
            | function TOK_WHILE expression                 { $$ = snow_ast_loop($3, $1); }
            | function TOK_UNTIL expression                 { $$ = snow_ast_loop(snow_ast_not($3), $1); }
            ;

conditional: TOK_IF expression TOK_EOL sequence TOK_EOL conditional_tail                { $$ = snow_ast_if_else($2, $4, $6); }
             | TOK_UNLESS expression TOK_EOL sequence TOK_EOL conditional_tail          { $$ = snow_ast_if_else(snow_ast_not($2), $4, $6); }
             ;

conditional_tail: TOK_END                                                       { $$ = NULL; }
                  | TOK_ELSE TOK_EOL sequence TOK_EOL TOK_END                   { $$ = $3; }
                  | TOK_ELSEIF expression TOK_EOL sequence TOK_EOL conditional_tail     { $$ = snow_ast_if_else($2, $4, $6); }
                  ;


sequence:   /* Nothing */                                   { $$ = snow_ast_sequence(0); }
            | sequence TOK_EOL                                  { $$ = $1; }
            | sequence statement                            { $$ = $1; snow_ast_sequence_push($1, $2); }
            ;

function:   expression                                      { $$ = $1; }
            | command                                       { $$ = $1; }
            ;

command:    return_cmd                                      { $$ = $1; }
            | TOK_BREAK                                         { $$ = $1; }
            | TOK_CONTINUE                                      { $$ = $1; }
            ;

return_cmd: TOK_RETURN                                          { $$ = snow_ast_return(NULL); }
            | TOK_RETURN expression                             { $$ = snow_ast_return($2); }
            ;
            
member:     '.' identifier                                  { $$ = snow_ast_member(snow_ast_self(), $2); }
            | expression '.' identifier                     { $$ = snow_ast_member($1, $3); }
            ;

identifier: TOK_IDENTIFIER;

local: identifier                                          { $$ = snow_ast_local($1); }
       ;
            
variable:   local                                      { $$ = $1; }
            | member                                        { $$ = $1; }
            ;

parameters: identifier                                      { $$ = snow_ast_sequence(1, symbol_to_value($1)); }
            | parameters ',' identifier                     { snow_ast_sequence_push($1, symbol_to_value($3)); $$ = $1; }
            ;

closure:    '[' parameters ']' scope                        { $$ = $4; $$->children[0] = $2; }
            | scope                                         { $$ = $1; }
            ;

scope:      '{' sequence '}'                                { $$ = snow_ast_function("<unnamed>", state->streamname, NULL, $2); }
            ;

symbol:     '#' identifier                                  { $$ = snow_ast_literal(symbol_to_value($2)); }
            | '#' TOK_STRING                                    { $$ = snow_ast_literal(symbol_to_value(snow_symbol($2))); }
            ;

literal_value:    TOK_INTEGER | TOK_FLOAT | TOK_TRUE | TOK_FALSE | TOK_NIL | string_literal | symbol;
literal:          literal_value                             { $$ = snow_ast_literal($1); }
                  ;

string_data: TOK_STRING                                         { $$ = $1; }
            | TOK_INTERPOLATION                                 { $$ = $1; }
            ;

string_literal: string_data                                 { $$ = $1; }
            | string_literal string_data                    { $$ = snow_ast_call(snow_ast_member($1, snow_symbol("+")), snow_ast_sequence(1, $2)); }
            ;

arg_list:   expression                                      { $$ = snow_ast_sequence(1, $1); }
            | arg_list ',' expression                       { $$ = $1; snow_ast_sequence_push($$, $3); }
            ;

arguments:  '(' ')'                                         { $$ = snow_ast_sequence(0); }
            | '(' arg_list ')'                              { $$ = $2; }
            | closure                                       { $$ = snow_ast_sequence(1, $1); }
            | '(' ')' closure                               { $$ = snow_ast_sequence(1, $3); }
            | '(' arg_list ')' closure                      { $$ = $2; snow_ast_sequence_push($$, $4); }
            ;

function_call: member arguments                             { $$ = snow_ast_call($1, $2); }
               | expression arguments                       { $$ = snow_ast_call($1, $2); }
               ;

assignment: identifier ':' expression                       { $$ = snow_ast_local_assign($1, $3); }
            | member ':' expression                         { $$ = snow_ast_member_assign($1, $3); }
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

expression: literal | closure | variable | function_call | assignment | operation | log_operation   
            | '(' expression ')'                            { $$ = $2; }
            ;

%%
