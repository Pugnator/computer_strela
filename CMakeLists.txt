cmake_minimum_required(VERSION 3.13)
project(strela LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(STRELA_SOURCES src/machine.c src/word.c src/instructions.c src/timing.c src/media.c src/blocks.c src/strela_compat.c src/system.c src/snapshot.c src/state_json.c)
add_library(strela STATIC ${STRELA_SOURCES})
target_include_directories(strela PUBLIC include)
add_library(strela_shared SHARED ${STRELA_SOURCES})
set_target_properties(strela_shared PROPERTIES OUTPUT_NAME strela)
target_include_directories(strela_shared PUBLIC include)
if(WIN32)
    set_target_properties(strela_shared PROPERTIES PREFIX "")
endif()
if(NOT WIN32)
    target_link_libraries(strela PUBLIC m)
    target_link_libraries(strela_shared PUBLIC m)
endif()

option(STRELA_BUILD_MONITOR "Build optional terminal operator console" OFF)
if(STRELA_BUILD_MONITOR)
    add_executable(strela_mon tools/strela_mon/main.c)
    target_link_libraries(strela_mon PRIVATE strela)
endif()

option(STRELA_BUILD_WEB_BRIDGE "Build JSON state bridge for local web console" OFF)
if(STRELA_BUILD_WEB_BRIDGE)
    add_executable(strela_view tools/strela_view.c)
    target_link_libraries(strela_view PRIVATE strela)
    target_include_directories(strela_view PRIVATE src)
endif()

add_executable(strela_run tools/strela_run.c)
target_link_libraries(strela_run PRIVATE strela)

enable_testing()
add_executable(strela_test tests/test_strela.c)
target_link_libraries(strela_test PRIVATE strela)
add_test(NAME strela_core COMMAND strela_test)

add_executable(strela_word_test tests/test_word.c)
target_include_directories(strela_word_test PRIVATE src)
target_link_libraries(strela_word_test PRIVATE strela)
add_test(NAME strela_word COMMAND strela_word_test)

add_executable(strela_programs_test tests/test_programs.c)
target_link_libraries(strela_programs_test PRIVATE strela)
add_test(NAME strela_programs COMMAND strela_programs_test)

add_executable(strela_fixture_runner tests/fixture_runner.c)
target_link_libraries(strela_fixture_runner PRIVATE strela)
add_executable(strela_media_test tests/test_media.c)
target_include_directories(strela_media_test PRIVATE src)
target_link_libraries(strela_media_test PRIVATE strela)
add_test(NAME strela_media COMMAND strela_media_test)
add_executable(strela_operator_test tests/test_operator.c)
target_link_libraries(strela_operator_test PRIVATE strela)
add_test(NAME strela_operator COMMAND strela_operator_test)
add_executable(strela_snapshot_test tests/test_snapshot.c)
target_link_libraries(strela_snapshot_test PRIVATE strela)
add_test(NAME strela_snapshot COMMAND strela_snapshot_test
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/book/kitov-midbatch-v1.sts)
add_executable(strela_timing_test tests/test_timing.c)
target_include_directories(strela_timing_test PRIVATE src)
target_link_libraries(strela_timing_test PRIVATE strela)
add_test(NAME strela_timing COMMAND strela_timing_test)
add_test(NAME book_bit_mask COMMAND strela_fixture_runner
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/book/bit-mask.strela
    0010 1 0012 0 2 0500 0000 0000 0115 0 00 1000)
add_test(NAME book_control_add COMMAND strela_fixture_runner
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/book/control-add.strela
    0010 1 0012 0 2 0500 7160 2474 7531 0 00 1000)
add_test(NAME book_kitov_linear COMMAND strela_fixture_runner
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/book/kitov-linear.strela
    0020 1 0040 0 434 0052 0400 0000 0000 0 01 229000)

add_executable(strela_opcodes_test tests/test_opcodes.c)
target_link_libraries(strela_opcodes_test PRIVATE strela)
add_test(NAME strela_opcodes COMMAND strela_opcodes_test)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    foreach(target strela strela_shared strela_run strela_test strela_word_test strela_programs_test strela_fixture_runner strela_media_test strela_timing_test strela_snapshot_test strela_operator_test strela_opcodes_test)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endforeach()
    # The test programs use assert for checks and setup; keep it active in Release.
    foreach(target strela_test strela_word_test strela_programs_test strela_media_test strela_timing_test strela_snapshot_test strela_operator_test strela_opcodes_test)
        target_compile_options(${target} PRIVATE -UNDEBUG)
    endforeach()
    if(STRELA_BUILD_MONITOR)
        target_compile_options(strela_mon PRIVATE -Wall -Wextra -Werror)
    endif()
    if(STRELA_BUILD_WEB_BRIDGE)
        target_compile_options(strela_view PRIVATE -Wall -Wextra -Werror)
    endif()
endif()




add_test(NAME multi_branch COMMAND strela_run run
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/programs/two-branches.strela
    --max-us 100000 --dump 0103)


add_test(NAME cli_operator COMMAND strela_run operate
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/book/kitov-midbatch-v1.sts
    --step 1 --save-snapshot ${CMAKE_CURRENT_BINARY_DIR}/cli-step.sts)
add_test(NAME cli_operator_parity COMMAND strela_snapshot_test
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/book/kitov-midbatch-v1.sts
    ${CMAKE_CURRENT_BINARY_DIR}/cli-step.sts)
set_tests_properties(cli_operator_parity PROPERTIES DEPENDS cli_operator)
# Book program with complete book data: the root of x^3/65 + x - 5.001 = 0
# (1959 book, pp. 166-167) is 4.00944774, converted to decimal in 0034.
add_test(NAME book_iteration COMMAND strela_run run
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/book/1959-iteration-cubic.strela
    --max-us 100000000 --dump 0034)
set_tests_properties(book_iteration PROPERTIES
    PASS_REGULAR_EXPRESSION "0034: 1000 2242 1672 0 01")
# A snapshot saved after a stop is paused but must still report that stop.
add_test(NAME cli_stop_snapshot COMMAND strela_run run
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/programs/two-branches.strela
    --max-us 100000 --check-stops 0
    --save-snapshot ${CMAKE_CURRENT_BINARY_DIR}/stopped.sts)
add_test(NAME cli_inspect_reason COMMAND strela_run inspect
    ${CMAKE_CURRENT_BINARY_DIR}/stopped.sts)
set_tests_properties(cli_inspect_reason PROPERTIES DEPENDS cli_stop_snapshot
    PASS_REGULAR_EXPRESSION "^normal stop at 0021")


if(STRELA_BUILD_MONITOR)
    add_test(NAME monitor_step COMMAND strela_mon
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/book/kitov-midbatch-v1.sts
        --step 1 --ram 0052 --save-snapshot ${CMAKE_CURRENT_BINARY_DIR}/monitor-step.sts)
    add_test(NAME monitor_cli_parity COMMAND ${CMAKE_COMMAND} -E compare_files
        ${CMAKE_CURRENT_BINARY_DIR}/cli-step.sts
        ${CMAKE_CURRENT_BINARY_DIR}/monitor-step.sts)
    set_tests_properties(monitor_cli_parity PROPERTIES DEPENDS "cli_operator;monitor_step")
endif()
if(STRELA_BUILD_WEB_BRIDGE)
    find_program(STRELA_NODE_EXECUTABLE node)
    if(STRELA_NODE_EXECUTABLE)
        add_test(NAME web_console COMMAND ${STRELA_NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/web/test.mjs)
        set_tests_properties(web_console PROPERTIES
            ENVIRONMENT "STRELA_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}")
        add_test(NAME web_catalog COMMAND ${STRELA_NODE_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/web/test-catalog.mjs)
        set_tests_properties(web_catalog PROPERTIES
            ENVIRONMENT "STRELA_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}")
    endif()
endif()
