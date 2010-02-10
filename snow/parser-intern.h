#ifndef PARSER_INTERN_H_P8KQXQRI
#define PARSER_INTERN_H_P8KQXQRI

#include "snow/parser.h"
#include "snow/gc.h"
#include "snow/linkbuffer.h"

typedef struct SnParserState {
	void* yyscanner;
	const char* buf;
	int pos;
	int len;
	SnAstNode* result;
	const char* streamname;
	SnParserInfo* info;
	SnLinkBuffer* string_buffer;
} SnParserState;

typedef union SnTokenValue {
	SnAstNode* node;
	SnAstNode* sequence;
	VALUE value;
	SnSymbol symbol;
} SnTokenValue;

#define YYSTYPE SnTokenValue
#define YY_EXTRA_TYPE SnParserState*
#define YYMALLOC snow_malloc
#define YYFREE snow_free
#define YYASSERT ASSERT

typedef struct YYLTYPE {
	int first_line;
	int first_column;
	int last_line;
	int last_column;
	char *filename;
} YYLTYPE;
# define YYLTYPE_IS_DECLARED 1

#define YYLOC_DEFAULT(CURRENT, RHS, N) \
	do { \
		(CURRENT).filename = NULL; \
		if (N) { \
			(CURRENT).first_line = YYRHSLOC(RHS, 1).first_line; \
			(CURRENT).first_column = YYRHSLOC(RHS, 1).first_column; \
			(CURRENT).last_line = YYRHSLOC(RHS, N).last_line; \
			(CURRENT).last_column = YYRHSLOC(RHS, N).last_column; \
			(CURRENT).filename = YYRHSLOC(RHS, 1).filename; \
		} else { \
			(CURRENT).first_line = (CURRENT).last_line = YYRHSLOC(RHS, 0).last_line; \
			(CURRENT).first_column = (CURRENT).last_column = YYRHSLOC(RHS, 0).last_column; \
			(CURRENT).filename = NULL; \
		} \
	} while (0)

HIDDEN int yylex(YYSTYPE*, struct YYLTYPE*, void*);
HIDDEN int yylex_init(void**);
HIDDEN int yylex_destroy(void*);
HIDDEN void yyset_extra(YY_EXTRA_TYPE, void*);
HIDDEN int yyparse(YY_EXTRA_TYPE, void*);
HIDDEN void yyerror(struct YYLTYPE* yylocp, YY_EXTRA_TYPE state, void* scanner, const char* yymsg);

#endif /* end of include guard: PARSER_INTERN_H_P8KQXQRI */
