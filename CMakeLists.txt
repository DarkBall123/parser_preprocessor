cmake_minimum_required(VERSION 3.16)
project(parser_preprocessor C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(PP_BUILD_TESTS "Build tests" ON)
option(PP_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(PP_ENABLE_SANITIZERS)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(
            -fno-omit-frame-pointer
            -fsanitize=address,undefined
        )
        add_link_options(
            -fsanitize=address,undefined
        )
    else()
        message(WARNING "Sanitizers are only configured for Clang/GNU toolchains")
    endif()
endif()

add_library(parser_preprocessor STATIC
    src/preprocessor_api.c
    src/pp_buffer.c
    src/pp_diagnostics.c
    src/pp_core_state.c
    src/pp_macro_table.c
    src/pp_condition_stack.c
    src/pp_expander.c
)

target_include_directories(parser_preprocessor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(parser_preprocessor PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

if(PP_BUILD_TESTS)
    enable_testing()

    add_executable(pp_unit_tests
        tests/unit_tests.c
    )
    target_link_libraries(pp_unit_tests PRIVATE parser_preprocessor)
    add_test(NAME pp_unit_tests COMMAND pp_unit_tests)

    add_executable(pp_smoke_tests
        tests/smoke_tests.c
    )
    target_link_libraries(pp_smoke_tests PRIVATE parser_preprocessor)
    add_test(NAME pp_smoke_tests COMMAND pp_smoke_tests)

    add_executable(pp_property_tests
        tests/property_tests.c
    )
    target_link_libraries(pp_property_tests PRIVATE parser_preprocessor)
    add_test(NAME pp_property_tests COMMAND pp_property_tests)
endif()
