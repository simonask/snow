#ifndef PARSER_INTERN_H_P8KQXQRI
#define PARSER_INTERN_H_P8KQXQRI

#include "snow/parser.h"
#include "snow/gc.h"

typedef struct SnParserState {
	void* yyscanner;
	const char* buf;
	int pos;
	int len;
	SnAstNode* result;
	const char* streamname;
} SnParserState;

typedef union SnTokenValue {
	SnAstNode* node;
	SnAstNode* sequence;
	VALUE value;
	SnSymbol symbol;
} SnTokenValue;

#define YYSTYPE SnTokenValue
#define YY_EXTRA_TYPE SnParserState*
struct YYLTYPE;
#define YYMALLOC snow_malloc
#define YYFREE snow_free

HIDDEN int yylex(YYSTYPE*, struct YYLTYPE*, void*);
HIDDEN int yylex_init(void**);
HIDDEN int yylex_destroy(void*);
HIDDEN void yyset_extra(YY_EXTRA_TYPE, void*);
HIDDEN int yyparse(YY_EXTRA_TYPE, void*);
HIDDEN void yyerror(struct YYLTYPE* yylocp, YY_EXTRA_TYPE state, void* scanner, const char* yymsg);

#endif /* end of include guard: PARSER_INTERN_H_P8KQXQRI */
