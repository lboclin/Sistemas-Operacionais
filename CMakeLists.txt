cmake_minimum_required(VERSION 3.16)
project(os_llm_project VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(os_core STATIC
    src/core/request.c
    src/gateway/gateway.c
    src/metrics/events.c
    src/scheduling/scheduler.c
    src/runtime/virtual.c
    src/metrics/summary.c
    src/memory/pager.c
)
target_include_directories(os_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gateway
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scheduling
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/src/metrics
    ${CMAKE_CURRENT_SOURCE_DIR}/src/memory
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(os_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror)
endif()

add_executable(os_llm_cli apps/cli/main.c)
target_link_libraries(os_llm_cli PRIVATE os_core)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(os_llm_cli PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(test_core tests/unit/test_core.c)
    target_link_libraries(test_core PRIVATE os_core)
    add_test(NAME core COMMAND test_core)

    add_executable(test_pager tests/unit/test_pager.c)
    target_link_libraries(test_pager PRIVATE os_core)
    add_test(NAME pager COMMAND test_pager)

    add_executable(test_gateway_events tests/integration/test_gateway_events.c)
    target_link_libraries(test_gateway_events PRIVATE os_core)
    add_test(NAME gateway_events COMMAND test_gateway_events)

    add_executable(test_virtual tests/integration/test_virtual.c)
    target_link_libraries(test_virtual PRIVATE os_core)
    add_test(NAME virtual COMMAND test_virtual)

    # Keep assertion-based tests active even when the library is optimized.
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        foreach(test_target IN ITEMS test_core test_pager test_virtual test_gateway_events)
            target_compile_options(${test_target} PRIVATE -UNDEBUG)
        endforeach()
    endif()

    add_test(NAME cli_help COMMAND os_llm_cli --help)
    add_test(NAME cli_round_robin COMMAND os_llm_cli --policy rr --quantum 2)
    add_test(NAME cli_scenario COMMAND os_llm_cli --scenario ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/sample.csv)
endif()
