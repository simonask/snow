%{
#include "snow/scanner.h"
#include "snow/parser.tab.h"
#include "snow/ast.h"

#define yyterminate() return TOK_EOF
#define YY_NO_UNISTD_H
%}

%option prefix="snow_"
%option batch
%option yywrap nounput 
%option stack
%x STRING_SINGLE
%x STRING_DOUBLE
%x STRING_INTERPOLATION
%x COMMENT
uintx comment_nesting = 0;

%{
//#define YY_USER_ACTION  yylloc->columns(yyleng);
%}

%% 

%{
//	yylloc->step();
%}

\"                                     { BEGIN(STRING_DOUBLE); string_buffer.str(""); } /* " */
<STRING_DOUBLE>\"                      { BEGIN(INITIAL); yylval->value = snow_create_string(""); return TOK_STRING; } /* " */
<STRING_DOUBLE>\\n                     { string_buffer << '\n'; }
<STRING_DOUBLE>\\t                     { string_buffer << '\t'; }
<STRING_DOUBLE>\\r                     { string_buffer << '\r'; }
<STRING_DOUBLE>\\b                     { string_buffer << '\b'; }
<STRING_DOUBLE>\\f                     { string_buffer << '\f'; }
<STRING_DOUBLE>\\e                     { string_buffer << '\e'; }
<STRING_DOUBLE>\\0[0-7]+               { string_buffer << (char)strtoll(&yytext[1], NULL, 8); }
<STRING_DOUBLE>\\(.|\n)                { string_buffer << yytext[1]; }
<STRING_DOUBLE>\$\{                    { BEGIN(STRING_INTERPOLATION); yylval->value = snow_create_string(string_buffer.str()); string_buffer.str(""); return TOK_STRING; }
<STRING_DOUBLE>[^\$\(\\\n\"]+          { string_buffer << yytext; } /* " */
<STRING_DOUBLE>.                       { string_buffer << yytext; }

<STRING_INTERPOLATION>\}               { BEGIN(STRING_DOUBLE); yylval->node = snow_ast_call(snow_ast_member(Driver::parse(STRING_INTERPOLATION_buffer.str())->sequence, snow_symbol("to_string"), NULL); STRING_INTERPOLATION_buffer.str(""); string_buffer.str(""); return TOK_INTERPOLATION; }
<STRING_INTERPOLATION>[^\}]+           { STRING_INTERPOLATION_buffer << yytext; }

\'                                     { BEGIN(STRING_SINGLE); string_buffer.str(""); } /* ' */
<STRING_SINGLE>\'                      { BEGIN(INITIAL); yylval->value = snow_create_string(yytext); return STRING; } /* ' */
<STRING_SINGLE>\\n                     { string_buffer << "\n"; }
<STRING_SINGLE>\\t                     { string_buffer << "\t"; }
<STRING_SINGLE>\\r                     { string_buffer << "\r"; }
<STRING_SINGLE>\\b                     { string_buffer << "\b"; }
<STRING_SINGLE>\\f                     { string_buffer << "\f"; }
<STRING_SINGLE>\\(.|\n)                { string_buffer << yytext[1]; }
<STRING_SINGLE>[^\\\n\']+              { string_buffer << yytext; } /* ' */

("//"|"#!").+                          { /* Do absolutely nothing. */ }
                                       
"/*"                                   { BEGIN(snow_comment_long); }
<COMMENT>"*/"                          { BEGIN(INITIAL); }
<COMMENT>[^\n\/\*]+                    { /* Do absolutely nothing. */ }
<COMMENT>\n                            { yylloc->lines(yyleng); yylloc->step(); }
<COMMENT>.                             { /* Do absolutely nothing. */ }

                                       
[0-9]+                                 { yylval->value = int_to_value(strtoll(yytext, NULL, 10)); return TOK_INTEGER; }
0b[01]+                                { yylval->value = int_to_value(strtoll(&yytext[2], NULL, 2)); return TOK_INTEGER; }
0x[0-9a-fA-F]+                         { yylval->value = int_to_value(strtoll(&yytext[2], NULL, 16)); return TOK_INTEGER; }
[0-9]+\.[0-9]+                         { yylval->value = float_to_value(strtof(yytext, NULL)); return TOK_FLOAT; }

self                                   { yylval->node = new ast::Self; return TOK_SELF; }
if                                     { return TOK_IF; }
unless                                 { return TOK_UNLESS; }
else                                   { return TOK_ELSE; }
"else if"                              { return TOK_ELSEIF; }
do                                     { return TOK_DO; }
while                                  { return TOK_WHILE; }
end                                    { return TOK_END; }
break                                  { yylval->node = new ast::Break(); return TOK_BREAK; }
continue                               { yylval->node = new ast::Continue(); return TOK_CONTINUE; }
return                                 { return TOK_RETURN; }
true                                   { yylval->value = kTrue; return TOK_TRUE; }
false                                  { yylval->value = kFalse; return TOK_FALSE; }
nil                                    { yylval->value = kNil; return TOK_NIL; }
and                                    { return TOK_LOG_AND; }
or                                     { return TOK_LOG_OR; }
xor                                    { return TOK_LOG_XOR; }
not                                    { return TOK_LOG_NOT; }
[_$@a-zA-Z][_$@a-zA-Z0-9]*\??          { yylval->symbol = new snow_symbol(yytext); return TOK_IDENTIFIER; }
;                                      { return TOK_EOL; }
\n                                     { yylloc->lines(yyleng); yylloc->step(); return TOK_EOL; }
[ \t\r]                                { yylloc->step(); /* Eat whitespaces */ }
[.,\[\]{}():#]                         { return static_cast<token_type>(*yytext); }
\|\||&&                                { yylval->symbol = snow_symbol(yytext); return TOK_OPERATOR_FOURTH; }
=|~=|>|<|>=|<=|==                      { yylval->symbol = snow_symbol(yytext); return TOK_OPERATOR_THIRD; }
%|\/|\*|\*\*                           { yylval->symbol = snow_symbol(yytext); return TOK_OPERATOR_FIRST; }
[^ \t\r\n.,\[\]{}():#a-zA-Z0-9\"]+     { yylval->symbol = snow_symbol(yytext); return TOK_OPERATOR_SECOND; }

%%
