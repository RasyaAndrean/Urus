cmake_minimum_required(VERSION 3.10)
project(UrusCompiler VERSION 0.2.3 LANGUAGES C)

set(PROJECT_VERSION_TAGS "(A)-Added")
set(CMAKE_C_STANDARD 11) # -std=c11
set(CMAKE_C_STANDARD_REQUIRED ON)

configure_file(config.h.in config.h)

if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
else()
    add_compile_options(-Wall -Wextra -g)
endif()

set(URUS_RUNTIME_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/urus_runtime.h")
set(URUS_RUNTIME_EMBEDDED "${CMAKE_CURRENT_BINARY_DIR}/urus_runtime.c")

add_custom_command(
    OUTPUT "${URUS_RUNTIME_EMBEDDED}"
    COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE=${URUS_RUNTIME_HEADER}
            -DOUTPUT_FILE=${URUS_RUNTIME_EMBEDDED}
            -DVARIABLE_NAME=urus_runtime_header_data
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed-string.cmake"
    DEPENDS "${URUS_RUNTIME_H}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed-string.cmake"
)

# source
set(SOURCES
    src/main.c
    src/lexer.c
    src/ast.c
    src/parser.c
    src/util.c
    src/error.c
    src/Sema/sema.c
    src/Sema/scope.c
    src/Sema/builtins.c
    src/codegen.c
    src/preprocess.c
)

add_executable(urusc ${SOURCES} ${URUS_RUNTIME_EMBEDDED})

target_include_directories(urusc PRIVATE "${CMAKE_CURRENT_BINARY_DIR}") # -I<BUILD DIRECTORIES> (example for config.h by config.h.in)
target_include_directories(urusc PRIVATE include) # -Iinclude

# -lm for non-windows
if(UNIX AND NOT APPLE)
    target_link_libraries(urusc PRIVATE m)
endif()

install(TARGETS urusc DESTINATION bin)
