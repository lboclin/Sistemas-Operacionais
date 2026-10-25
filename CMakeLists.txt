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
    src/runtime/session.c
    src/metrics/summary.c
    src/memory/pager.c
    src/memory/frames.c
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

    add_executable(test_frames tests/unit/test_frames.c)
    target_link_libraries(test_frames PRIVATE os_core)
    add_test(NAME frames COMMAND test_frames)

    add_executable(test_memory_runtime tests/integration/test_memory_runtime.c)
    target_link_libraries(test_memory_runtime PRIVATE os_core)
    add_test(NAME memory_runtime COMMAND test_memory_runtime)

    add_executable(test_pager tests/unit/test_pager.c)
    target_link_libraries(test_pager PRIVATE os_core)
    add_test(NAME pager COMMAND test_pager)

    add_executable(test_gateway_events tests/integration/test_gateway_events.c)
    target_link_libraries(test_gateway_events PRIVATE os_core)
    add_test(NAME gateway_events COMMAND test_gateway_events)

    add_executable(test_session tests/integration/test_session.c)
    target_link_libraries(test_session PRIVATE os_core)
    add_test(NAME session COMMAND test_session)

    add_executable(test_virtual tests/integration/test_virtual.c)
    target_link_libraries(test_virtual PRIVATE os_core)
    add_test(NAME virtual COMMAND test_virtual)

    # Keep assertion-based tests active even when the library is optimized.
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        foreach(test_target IN ITEMS test_core test_pager test_virtual test_gateway_events test_frames test_memory_runtime test_session)
            target_compile_options(${test_target} PRIVATE -UNDEBUG)
        endforeach()
    endif()

    add_test(NAME cli_help COMMAND os_llm_cli --help)
    add_test(NAME cli_round_robin COMMAND os_llm_cli --policy rr --quantum 2)
    add_test(NAME cli_scenario COMMAND os_llm_cli --scenario ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/sample.csv)
    add_test(NAME cli_memory_trace COMMAND os_llm_cli --frames 2 --page-tokens 2 --page-policy lru --trace)
    add_test(NAME cli_invalid_online_opt COMMAND os_llm_cli --frames 2 --page-policy opt)
    set_tests_properties(cli_invalid_online_opt PROPERTIES WILL_FAIL TRUE)

    add_executable(test_session_controls tests/integration/test_session_controls.c)
    target_link_libraries(test_session_controls PRIVATE os_core)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_session_controls PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Werror)
    endif()
    add_test(NAME session_controls COMMAND test_session_controls)

    add_test(NAME cli_initial_snapshot COMMAND os_llm_cli --steps 0 --snapshots)
    set_tests_properties(cli_initial_snapshot PROPERTIES PASS_REGULAR_EXPRESSION "snapshot step=0 tick=0 pending=3.*finished=no")
    add_test(NAME cli_step_snapshot COMMAND os_llm_cli --steps 2 --snapshots --policy rr --quantum 1)
    set_tests_properties(cli_step_snapshot PROPERTIES PASS_REGULAR_EXPRESSION "snapshot step=2 tick=2.*finished=no")
    add_test(NAME cli_pause COMMAND os_llm_cli --steps 1)
    set_tests_properties(cli_pause PROPERTIES PASS_REGULAR_EXPRESSION "Execution: paused")
    add_test(NAME cli_snapshots_complete COMMAND os_llm_cli --snapshots --frames 2 --page-tokens 2)
    set_tests_properties(cli_snapshots_complete PROPERTIES PASS_REGULAR_EXPRESSION "Execution: completed")
    add_test(NAME cli_invalid_steps COMMAND os_llm_cli --steps -1)
    set_tests_properties(cli_invalid_steps PROPERTIES WILL_FAIL TRUE)
    add_test(NAME cli_invalid_steps_overflow COMMAND os_llm_cli --steps 18446744073709551616)
    set_tests_properties(cli_invalid_steps_overflow PROPERTIES WILL_FAIL TRUE)
    add_test(NAME cli_missing_steps COMMAND os_llm_cli --steps)
    set_tests_properties(cli_missing_steps PROPERTIES WILL_FAIL TRUE)

    add_executable(test_fresh_pcb tests/unit/test_fresh_pcb.c)
    target_link_libraries(test_fresh_pcb PRIVATE os_core)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_fresh_pcb PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Werror)
    endif()
    add_test(NAME fresh_pcb COMMAND test_fresh_pcb)
endif()
