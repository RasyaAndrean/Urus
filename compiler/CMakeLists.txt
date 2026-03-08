cmake_minimum_required(VERSION 3.10)

project(UrusCompiler VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11) # -std=c11
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -g)
endif()

# source
set(SOURCES
    src/main.c
    src/lexer.c
    src/ast.c
    src/parser.c
    src/util.c
    src/sema.c
    src/codegen.c
)

add_executable(urusc ${SOURCES})
target_include_directories(urusc PRIVATE include) # -Iinclude

# -lm for non-windows
if(UNIX AND NOT APPLE)
    target_link_libraries(urusc PRIVATE m)
endif()

install(TARGETS urusc DESTINATION bin)
