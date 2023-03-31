cmake_minimum_required(VERSION 3.25.2)

project(ptlang
        LANGUAGES C)

set(PTLANG_SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(PTLANG_AST_SRC_DIR ${PTLANG_SRC_DIR}/ptlang_ast)
set(PTLANG_PARSER_SRC_DIR ${PTLANG_SRC_DIR}/ptlang_parser)


add_library(ptlang_ast OBJECT
            ${PTLANG_AST_SRC_DIR}/ptlang_ast.c)

set_property(TARGET ptlang_ast PROPERTY C_STANDARD 17)

find_package(BISON 3.8.2 REQUIRED)
find_package(FLEX 2.6.4 REQUIRED)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(DEBUG_FLAG "--debug")
endif()

BISON_TARGET(PTLANG_PARSER ${PTLANG_PARSER_SRC_DIR}/ptlang.y ${PROJECT_BINARY_DIR}/parser.c DEFINES_FILE ${PROJECT_BINARY_DIR}/parser.h COMPILE_FLAGS "${DEBUG_FLAG}")
FLEX_TARGET(PTLANG_LEXER ${PTLANG_PARSER_SRC_DIR}/ptlang.l ${PROJECT_BINARY_DIR}/lexer.c DEFINES_FILE ${PROJECT_BINARY_DIR}/lexer.h COMPILE_FLAGS "${DEBUG_FLAG}")
ADD_FLEX_BISON_DEPENDENCY(PTLANG_LEXER PTLANG_PARSER)

set(LEXER_OUTPUT_SOURCE ${FLEX_PTLANG_LEXER_OUTPUTS})

foreach(LEXER_OUTPUT ${FLEX_PTLANG_LEXER_OUTPUTS})
    if(LEXER_OUTPUT STREQUAL FLEX_PTLANG_LEXER_OUTPUT_HEADER)
        list(REMOVE_ITEM LEXER_OUTPUT_SOURCE ${LEXER_OUTPUT})
    endif()
endforeach()


add_library(ptlang_parser OBJECT
            ${PTLANG_PARSER_SRC_DIR}/ptlang_parser.c
            ${LEXER_OUTPUT_SOURCE}
            ${BISON_PTLANG_PARSER_OUTPUT_SOURCE})

set_property(TARGET ptlang_parser PROPERTY C_STANDARD 17)

target_include_directories(ptlang_parser
                           PRIVATE ${PTLANG_PARSER_SRC_DIR}
                           PRIVATE ${PROJECT_BINARY_DIR})
