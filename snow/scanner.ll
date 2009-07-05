%{
#include "snow/scanner.h"
#include "snow/parser.tab.h"
#include "snow/ast.h"
#include "snow/linkbuffer.h"

#define yyterminate() return TOK_EOF
#define YY_NO_UNISTD_H
%}

%option prefix="snow_flex_"
%option batch
%option yywrap nounput 
%option stack
%x STRING_SINGLE
%x STRING_DOUBLE
%x STRING_INTERPOLATION
%x COMMENT

%{
//#define YY_USER_ACTION  yylloc->columns(yyleng);

SnLinkBuffer* string_buffer = NULL;
#define STR_CHAR(c) snow_linkbuffer_push(string_buffer, c)
#define STR_CHARS(str) snow_linkbuffer_push_string(string_buffer, str);
#define STR_CLEAR() snow_linkbuffer_clear(string_buffer)
%}

%% 

%{
//	yylloc->step();
	if (!string_buffer)
		string_buffer = snow_create_linkbuffer(1024);
%}

\"                                     { BEGIN(STRING_DOUBLE); STR_CLEAR(); } /* " */
<STRING_DOUBLE>\"                      { BEGIN(INITIAL); yylval.value = snow_create_string(""); return TOK_STRING; } /* " */
<STRING_DOUBLE>\\n                     { STR_CHAR('\n'); }
<STRING_DOUBLE>\\t                     { STR_CHAR('\t'); }
<STRING_DOUBLE>\\r                     { STR_CHAR('\r'); }
<STRING_DOUBLE>\\b                     { STR_CHAR('\b'); }
<STRING_DOUBLE>\\f                     { STR_CHAR('\f'); }
<STRING_DOUBLE>\\e                     { STR_CHAR('\e'); }
<STRING_DOUBLE>\\0[0-7]+               { STR_CHAR((char)strtoll(&yytext[1], NULL, 8)); }
<STRING_DOUBLE>\\(.|\n)                { STR_CHAR(yytext[1]); }
<STRING_DOUBLE>\$\{                    { BEGIN(STRING_INTERPOLATION); yylval.value = snow_create_string_from_linkbuffer(string_buffer); STR_CLEAR(); return TOK_STRING; }
<STRING_DOUBLE>[^\$\(\\\n\"]+          { STR_CHARS(yytext); } /* " */
<STRING_DOUBLE>.                       { STR_CHARS(yytext); }

                                       /* TODO: String interpolation */

\'                                     { BEGIN(STRING_SINGLE); STR_CLEAR(); } /* ' */
<STRING_SINGLE>\'                      { BEGIN(INITIAL); yylval.value = snow_create_string(yytext); return TOK_STRING; } /* ' */
<STRING_SINGLE>\\n                     { STR_CHAR('\n'); }
<STRING_SINGLE>\\t                     { STR_CHAR('\t'); }
<STRING_SINGLE>\\r                     { STR_CHAR('\r'); }
<STRING_SINGLE>\\b                     { STR_CHAR('\b'); }
<STRING_SINGLE>\\f                     { STR_CHAR('\f'); }
<STRING_SINGLE>\\(.|\n)                { STR_CHAR(yytext[1]); }
<STRING_SINGLE>[^\\\n\']+              { STR_CHARS(yytext); } /* ' */

("//"|"#!").+                          { /* Do absolutely nothing. */ }
                                       
"/*"                                   { BEGIN(COMMENT); }
<COMMENT>"*/"                          { BEGIN(INITIAL); }
<COMMENT>[^\n\/\*]+                    { /* Do absolutely nothing. */ }
<COMMENT>\n                            { /*yylloc.lines = yyleng; yylloc.lines++;*/ }
<COMMENT>.                             { /* Do absolutely nothing. */ }

                                       
[0-9]+                                 { yylval.value = int_to_value(strtoll(yytext, NULL, 10)); return TOK_INTEGER; }
0b[01]+                                { yylval.value = int_to_value(strtoll(&yytext[2], NULL, 2)); return TOK_INTEGER; }
0x[0-9a-fA-F]+                         { yylval.value = int_to_value(strtoll(&yytext[2], NULL, 16)); return TOK_INTEGER; }
[0-9]+\.[0-9]+                         { yylval.value = float_to_value(strtof(yytext, NULL)); return TOK_FLOAT; }

self                                   { yylval.node = snow_ast_self(); return TOK_SELF; }
if                                     { return TOK_IF; }
unless                                 { return TOK_UNLESS; }
else                                   { return TOK_ELSE; }
"else if"                              { return TOK_ELSEIF; }
do                                     { return TOK_DO; }
while                                  { return TOK_WHILE; }
end                                    { return TOK_END; }
break                                  { yylval.node = snow_ast_break(); return TOK_BREAK; }
continue                               { yylval.node = snow_ast_continue(); return TOK_CONTINUE; }
return                                 { return TOK_RETURN; }
true                                   { yylval.value = SN_TRUE; return TOK_TRUE; }
false                                  { yylval.value = SN_FALSE; return TOK_FALSE; }
nil                                    { yylval.value = SN_NIL; return TOK_NIL; }
and                                    { return TOK_LOG_AND; }
or                                     { return TOK_LOG_OR; }
xor                                    { return TOK_LOG_XOR; }
not                                    { return TOK_LOG_NOT; }
[_$@a-zA-Z][_$@a-zA-Z0-9]*\??          { yylval.symbol = snow_symbol(yytext); return TOK_IDENTIFIER; }
;                                      { return TOK_EOL; }
\n                                     { /*yylloc->lines(yyleng); yylloc->step();*/ return TOK_EOL; }
[ \t\r]                                { /*yylloc->step();  Eat whitespaces */ }
[.,\[\]{}():#]                         { return (int)(*yytext); }
\|\||&&                                { yylval.symbol = snow_symbol(yytext); return TOK_OPERATOR_FOURTH; }
=|~=|>|<|>=|<=|==                      { yylval.symbol = snow_symbol(yytext); return TOK_OPERATOR_THIRD; }
%|\/|\*|\*\*                           { yylval.symbol = snow_symbol(yytext); return TOK_OPERATOR_FIRST; }
[^ \t\r\n.,\[\]{}():#a-zA-Z0-9\"]+     { yylval.symbol = snow_symbol(yytext); return TOK_OPERATOR_SECOND; }

%%
