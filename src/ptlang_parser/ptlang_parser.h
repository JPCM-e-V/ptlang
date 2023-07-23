#ifndef PTLANG_PARSER_H
#define PTLANG_PARSER_H

#include "../ptlang_ast/ptlang_ast.h"
#include "ptlang_error.h"

#include <stdio.h>

ptlang_error *ptlang_parser_parse(FILE *file, ptlang_ast_module *out);

#endif
