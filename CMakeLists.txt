cmake_minimum_required(VERSION 3.25.2)

project(plang
        LANGUAGES C)

set(PTLANG_SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(PTLANG_AST_SRC_DIR ${PTLANG_SRC_DIR}/ptlang_ast)
set(PTLANG_PARSER_SRC_DIR ${PTLANG_SRC_DIR}/ptlang_parser)


add_library(ptlang_ast OBJECT
            ${PTLANG_AST_SRC_DIR}/ptlang_ast.c)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)


BISON_TARGET(PTLANG_PARSER ${PTLANG_PARSER_SRC_DIR}/ptlang.y ${PROJECT_BINARY_DIR}/parser.c DEFINES_FILE ${PROJECT_BINARY_DIR}/parser.h)
# BISON_TARGET(PTLANG_PARSER ${PTLANG_PARSER_SRC_DIR}/ptlang.y ${PROJECT_BINARY_DIR}/parser.c DEFINES_FILE ${PROJECT_BINARY_DIR}/parser.h COMPILE_FLAGS -Dapi.prefix=ptlang_yy)
FLEX_TARGET(PTLANG_LEXER ${PTLANG_PARSER_SRC_DIR}/ptlang.l ${PROJECT_BINARY_DIR}/lexer.c DEFINES_FILE ${PROJECT_BINARY_DIR}/lexer.h)
# FLEX_TARGET(PTLANG_LEXER ${PTLANG_PARSER_SRC_DIR}/ptlang.l ${PROJECT_BINARY_DIR}/lexer.c DEFINES_FILE ${PROJECT_BINARY_DIR}/lexer.h COMPILE_FLAGS --prefix=ptlang_yy)
ADD_FLEX_BISON_DEPENDENCY(PTLANG_LEXER PTLANG_PARSER)

add_library(ptlang_parser OBJECT
            ${FLEX_PTLANG_LEXER_OUTPUTS} ${BISON_MyParser_OUTPUT_SOURCE})

target_include_directories(ptlang_parser
                           PRIVATE ${PTLANG_PARSER_SRC_DIR}
                           PRIVATE ${PROJECT_BINARY_DIR})


# message(:${BISON_MyParser_OUTPUT_SOURCE})
# message(:${BISON_MyParser_OUTPUT_HEADER})
# message(:${BISON_MyParser_OUTPUTS})
# message(:${FLEX_PTLANG_LEXER_OUTPUTS})
