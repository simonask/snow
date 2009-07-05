#ifndef SCANNER_H_X1PFB2DS
#define SCANNER_H_X1PFB2DS


#ifndef __FLEX_LEXER_H
#define yyFlexLexer SnowFlexLexer
//#include "FlexLexer.h"
#undef yyFlexLexer
#endif

/*
#include "Parser.h"

namespace snow {

	class Scanner : public SnowFlexLexer
	{
	public:
		Scanner(std::istream* arg_yyin = 0, std::ostream* arg_yyout = 0);
		virtual ~Scanner();
		virtual Parser::token_type lex(Parser::semantic_type* yylval, Parser::location_type* yylloc);

	protected:
		snow::Parser::token_type token_for_operator(char* op);
	};

}*/

#endif /* end of include guard: SCANNER_H_X1PFB2DS */
