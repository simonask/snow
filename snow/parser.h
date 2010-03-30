#ifndef PARSER_H_R3XQLOW1
#define PARSER_H_R3XQLOW1

#include "snow/basic.h"
#include "snow/ast.h"

typedef struct SnParserInfo {
	uintx num_functions;
} SnParserInfo;

CAPI struct SnAstNode* snow_parse(const char* buffer, struct SnParserInfo* out_info);

#endif /* end of include guard: PARSER_H_R3XQLOW1 */
