cmake_minimum_required(VERSION 3.16)
project(v2speed_smart_connect VERSION 0.1.0 LANGUAGES C)

include(CTest)

add_library(v2speed_smart_connect STATIC src/smart_connect.c src/connection_state.c src/fallback_planner.c src/smart_connect_session.c)
target_include_directories(v2speed_smart_connect PUBLIC include)
target_compile_features(v2speed_smart_connect PUBLIC c_std_11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(v2speed_smart_connect PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

if(BUILD_TESTING)
    add_executable(test_smart_connect tests/test_smart_connect.c)
    target_link_libraries(test_smart_connect PRIVATE v2speed_smart_connect)
    if(UNIX)
        target_link_libraries(test_smart_connect PRIVATE m)
    endif()
    add_test(NAME smart_connect_domain COMMAND test_smart_connect)

    add_executable(test_connection_state tests/test_connection_state.c)
    target_link_libraries(test_connection_state PRIVATE v2speed_smart_connect)
    add_test(NAME connection_state_domain COMMAND test_connection_state)

    add_executable(test_fallback_planner tests/test_fallback_planner.c)
    target_link_libraries(test_fallback_planner PRIVATE v2speed_smart_connect)
    if(UNIX)
        target_link_libraries(test_fallback_planner PRIVATE m)
    endif()
    add_test(NAME fallback_planner_domain COMMAND test_fallback_planner)

    add_executable(test_smart_connect_session tests/test_smart_connect_session.c)
    target_link_libraries(test_smart_connect_session PRIVATE v2speed_smart_connect)
    if(UNIX)
        target_link_libraries(test_smart_connect_session PRIVATE m)
    endif()
    add_test(NAME smart_connect_session_domain COMMAND test_smart_connect_session)
endif()
