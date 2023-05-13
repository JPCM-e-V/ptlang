cmake_minimum_required(VERSION 3.25.1)

project(ptlang
        LANGUAGES C)

set(PTLANG_SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(PTLANG_AST_SRC_DIR ${PTLANG_SRC_DIR}/ptlang_ast)
set(PTLANG_PARSER_SRC_DIR ${PTLANG_SRC_DIR}/ptlang_parser)
set(PTLANG_MAIN_SRC_DIR ${PTLANG_SRC_DIR}/ptlang_main)
set(PTLANG_IR_BUILDER_SRC_DIR ${PTLANG_SRC_DIR}/ptlang_ir_builder)


add_library(ptlang_ast OBJECT
            ${PTLANG_AST_SRC_DIR}/ptlang_ast.c)

set_property(TARGET ptlang_ast PROPERTY C_STANDARD 17)

find_package(BISON 3.8.2 REQUIRED)
find_package(FLEX 2.6.4 REQUIRED)
find_package(LLVM REQUIRED CONFIG)

set(PTLANG_LLVM_CONFIG "${LLVM_TOOLS_BINARY_DIR}/llvm-config")
set(PTLANG_LLVM_COMPONENTS "core;analysis;native;passes")
execute_process(COMMAND ${PTLANG_LLVM_CONFIG} --cflags OUTPUT_VARIABLE PTLANG_LLVM_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(PTLANG_LLVM_CFLAG_LIST NATIVE_COMMAND ${PTLANG_LLVM_CFLAGS})
execute_process(COMMAND ${PTLANG_LLVM_CONFIG} --includedir OUTPUT_VARIABLE PTLANG_LLVM_INCLUDEDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${PTLANG_LLVM_CONFIG} --ldflags OUTPUT_VARIABLE PTLANG_LLVM_LDFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(PTLANG_LLVM_LDFLAG_LIST NATIVE_COMMAND ${PTLANG_LLVM_LDFLAGS})
execute_process(COMMAND ${PTLANG_LLVM_CONFIG} --libdir OUTPUT_VARIABLE PTLANG_LLVM_LIBDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${PTLANG_LLVM_CONFIG} --libfiles ${PTLANG_LLVM_COMPONENTS} OUTPUT_VARIABLE PTLANG_LLVM_LIBFILES OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(PTLANG_LLVM_LIBFILE_LIST NATIVE_COMMAND ${PTLANG_LLVM_LIBFILES})


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

add_library(ptlang_ir_builder OBJECT
            ${PTLANG_IR_BUILDER_SRC_DIR}/ptlang_ir_builder.c)

target_include_directories(ptlang_ir_builder
                           PRIVATE ${PTLANG_AST_SRC_DIR}
                           PUBLIC ${PTLANG_LLVM_INCLUDEDIR}
                           PUBLIC ${PROJECT_SOURCE_DIR}/include)

# llvm_map_components_to_libnames(llvm_libs support core irreader)


target_link_libraries(ptlang_ir_builder
                      ${PTLANG_LLVM_LIBFILE_LIST})
target_compile_options(ptlang_ir_builder
                       PUBLIC ${PTLANG_LLVM_CFLAG_LIST})
target_link_options(ptlang_ir_builder
                    PUBLIC ${PTLANG_LLVM_LDFLAG_LIST})
target_link_directories(ptlang_ir_builder
                        PUBLIC ${PTLANG_LLVM_LIBDIR})

add_executable(ptlang ${PTLANG_MAIN_SRC_DIR}/main.c)

target_include_directories(ptlang
                           PRIVATE ${PTLANG_PARSER_SRC_DIR}
                           PRIVATE ${PTLANG_AST_SRC_DIR}
                           PRIVATE ${PTLANG_IR_BUILDER_SRC_DIR}
                           PRIVATE ${PROJECT_BINARY_DIR})

target_link_libraries(ptlang ptlang_ast ptlang_parser ptlang_ir_builder)


# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#     if (ARGV0)
#         unset(MATCHED)
#         string(REGEX MATCH ${ARGV0} MATCHED ${_variableName})
#         if (NOT MATCHED)
#             continue()
#         endif()
#     endif()
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()
