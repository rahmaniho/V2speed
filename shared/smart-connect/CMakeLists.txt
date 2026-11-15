cmake_minimum_required(VERSION 3.16)
project(v2speed_smart_connect VERSION 0.1.0 LANGUAGES C)

include(CTest)

add_library(v2speed_smart_connect STATIC
    src/smart_connect.c
    src/connection_state.c
    src/fallback_planner.c
    src/smart_connect_session.c
    src/smart_connect_measurements.c
    src/smart_connect_probe_plan.c)
target_include_directories(v2speed_smart_connect PUBLIC include)
target_compile_features(v2speed_smart_connect PUBLIC c_std_11)

if(UNIX)
    # Measurement stability uses sqrt; propagate libm for static-library consumers.
    target_link_libraries(v2speed_smart_connect PUBLIC m)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(v2speed_smart_connect PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

add_executable(v2speed_csv_rank tools/v2speed_csv_rank.c)
target_link_libraries(v2speed_csv_rank PRIVATE v2speed_smart_connect)

if(BUILD_TESTING)
    add_executable(test_smart_connect tests/test_smart_connect.c)
    target_link_libraries(test_smart_connect PRIVATE v2speed_smart_connect)
    add_test(NAME smart_connect_domain COMMAND test_smart_connect)

    add_executable(test_connection_state tests/test_connection_state.c)
    target_link_libraries(test_connection_state PRIVATE v2speed_smart_connect)
    add_test(NAME connection_state_domain COMMAND test_connection_state)

    add_executable(test_fallback_planner tests/test_fallback_planner.c)
    target_link_libraries(test_fallback_planner PRIVATE v2speed_smart_connect)
    add_test(NAME fallback_planner_domain COMMAND test_fallback_planner)

    add_executable(test_smart_connect_session tests/test_smart_connect_session.c)
    target_link_libraries(test_smart_connect_session PRIVATE v2speed_smart_connect)
    add_test(NAME smart_connect_session_domain COMMAND test_smart_connect_session)

    add_executable(test_smart_connect_measurements tests/test_smart_connect_measurements.c)
    target_link_libraries(test_smart_connect_measurements PRIVATE v2speed_smart_connect)
    add_test(NAME smart_connect_measurements_domain COMMAND test_smart_connect_measurements)

    add_executable(test_smart_connect_probe_plan tests/test_smart_connect_probe_plan.c)
    target_link_libraries(test_smart_connect_probe_plan PRIVATE v2speed_smart_connect)
    add_test(NAME smart_connect_probe_plan_domain COMMAND test_smart_connect_probe_plan)

    add_test(NAME smart_connect_csv_rank_demo
        COMMAND v2speed_csv_rank
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/demo_observations.csv
            60000 30000)
endif()
