cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test)


# Test executable
add_executable(test_strategies
    test_strategies.cpp
    ../src/jsontreemodel.cpp
    ../src/config.cpp
    ../src/common.cpp
    ../src/loadworker.cpp
    ../src/jsonnode.h
    ../src/strategies/jsonstrategy.cpp
    ../src/strategies/smallfilestrategy.cpp
    ../src/strategies/mediumfilestrategy.cpp
    ../src/strategies/largefilestrategy.cpp
    ../src/strategies/extremefilestrategy.cpp
    ../src/memorymappedfile.cpp
)

target_include_directories(test_strategies PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../simdjson
    ${seersdk_SOURCE_DIR}
)

target_link_libraries(test_strategies PRIVATE
    simdjson_static
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Test
)

# Add test
add_test(NAME StrategyTests COMMAND test_strategies)

# Set working directory to project root so tests can find test files
set_tests_properties(StrategyTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Paging test executable
add_executable(test_paging
    test_paging.cpp
    ../src/jsontreemodel.cpp
    ../src/config.cpp
    ../src/common.cpp
    ../src/loadworker.cpp
    ../src/jsonnode.h
    ../src/strategies/jsonstrategy.cpp
    ../src/strategies/smallfilestrategy.cpp
    ../src/strategies/mediumfilestrategy.cpp
    ../src/strategies/largefilestrategy.cpp
    ../src/strategies/extremefilestrategy.cpp
    ../src/memorymappedfile.cpp
)

target_include_directories(test_paging PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../simdjson
    ${seersdk_SOURCE_DIR}
)

target_link_libraries(test_paging PRIVATE
    simdjson_static
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Test
)

# Add paging test
add_test(NAME PagingTests COMMAND test_paging)

set_tests_properties(PagingTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Copy operations test executable
add_executable(test_copy
    test_copy.cpp
    ../src/jsontreemodel.cpp
    ../src/config.cpp
    ../src/common.cpp
    ../src/loadworker.cpp
    ../src/jsonnode.h
    ../src/strategies/jsonstrategy.cpp
    ../src/strategies/smallfilestrategy.cpp
    ../src/strategies/mediumfilestrategy.cpp
    ../src/strategies/largefilestrategy.cpp
    ../src/strategies/extremefilestrategy.cpp
    ../src/memorymappedfile.cpp
)

target_include_directories(test_copy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../simdjson
    ${seersdk_SOURCE_DIR}
)

target_link_libraries(test_copy PRIVATE
    simdjson_static
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Test
)

# Add copy test
add_test(NAME CopyTests COMMAND test_copy)

set_tests_properties(CopyTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Async loading test executable (Phase 13)
add_executable(test_async_loading
    test_async_loading.cpp
    ../src/jsontreemodel.cpp
    ../src/config.cpp
    ../src/common.cpp
    ../src/loadworker.cpp
    ../src/jsonnode.h
    ../src/strategies/jsonstrategy.cpp
    ../src/strategies/smallfilestrategy.cpp
    ../src/strategies/mediumfilestrategy.cpp
    ../src/strategies/largefilestrategy.cpp
    ../src/strategies/extremefilestrategy.cpp
    ../src/memorymappedfile.cpp
)

target_include_directories(test_async_loading PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../simdjson
    ${seersdk_SOURCE_DIR}
)

target_link_libraries(test_async_loading PRIVATE
    simdjson_static
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Test
)

# Add async loading test
add_test(NAME AsyncLoadingTests COMMAND test_async_loading)

set_tests_properties(AsyncLoadingTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# String parsing test executable
add_executable(test_string_parsing
    test_string_parsing.cpp
    ../src/jsontreemodel.cpp
    ../src/config.cpp
    ../src/common.cpp
    ../src/loadworker.cpp
    ../src/jsonnode.h
    ../src/strategies/jsonstrategy.cpp
    ../src/strategies/smallfilestrategy.cpp
    ../src/strategies/mediumfilestrategy.cpp
    ../src/strategies/largefilestrategy.cpp
    ../src/strategies/extremefilestrategy.cpp
    ../src/memorymappedfile.cpp
)

target_include_directories(test_string_parsing PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../simdjson
    ${seersdk_SOURCE_DIR}
)

target_link_libraries(test_string_parsing PRIVATE
    simdjson_static
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Test
)

# Add string parsing test
add_test(NAME StringParsingTests COMMAND test_string_parsing)

set_tests_properties(StringParsingTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Error reporting test executable
add_executable(test_errors
    test_errors.cpp
    ../src/jsontreemodel.cpp
    ../src/config.cpp
    ../src/common.cpp
    ../src/loadworker.cpp
    ../src/jsonnode.h
    ../src/strategies/jsonstrategy.cpp
    ../src/strategies/smallfilestrategy.cpp
    ../src/strategies/mediumfilestrategy.cpp
    ../src/strategies/largefilestrategy.cpp
    ../src/strategies/extremefilestrategy.cpp
    ../src/memorymappedfile.cpp
)

target_include_directories(test_errors PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../simdjson
    ${seersdk_SOURCE_DIR}
)

target_link_libraries(test_errors PRIVATE
    simdjson_static
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Test
)

# Add error reporting test
add_test(NAME ErrorTests COMMAND test_errors)

set_tests_properties(ErrorTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Search test executable
add_executable(test_search
    test_search.cpp
    ../src/navigation/searchworker.cpp
    ../src/strategies/jsonstrategy.cpp
    ../src/strategies/smallfilestrategy.cpp
    ../src/strategies/mediumfilestrategy.cpp
    ../src/strategies/largefilestrategy.cpp
    ../src/strategies/extremefilestrategy.cpp
    ../src/memorymappedfile.cpp
    ../src/config.cpp
    ../src/common.cpp
)

target_include_directories(test_search PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../simdjson
    ${seersdk_SOURCE_DIR}
)

target_link_libraries(test_search PRIVATE
    simdjson_static
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Test
)

add_test(NAME SearchTests COMMAND test_search)
set_tests_properties(SearchTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Navigation test executable
add_executable(test_navigation
    test_navigation.cpp
    ../src/navigation/pathnavigator.cpp
    ../src/jsontreeview.cpp
    ../src/jsontreedelegate.cpp
    ../src/jsontreemodel.cpp
    ../src/config.cpp
    ../src/common.cpp
    ../src/loadworker.cpp
    ../src/jsonnode.h
    ../src/strategies/jsonstrategy.cpp
    ../src/strategies/smallfilestrategy.cpp
    ../src/strategies/mediumfilestrategy.cpp
    ../src/strategies/largefilestrategy.cpp
    ../src/strategies/extremefilestrategy.cpp
    ../src/memorymappedfile.cpp
)

target_include_directories(test_navigation PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../simdjson
    ${seersdk_SOURCE_DIR}
)

target_link_libraries(test_navigation PRIVATE
    simdjson_static
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Test
)

add_test(NAME NavigationTests COMMAND test_navigation)
set_tests_properties(NavigationTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Group all targets
set(SUB_TEST_TARGETS 
    test_strategies 
    test_paging 
    test_copy 
    test_async_loading 
    test_string_parsing 
    test_errors
    test_search
    test_navigation
)
foreach(target_name ${SUB_TEST_TARGETS})
    set_property(TARGET ${target_name} PROPERTY FOLDER "Tests")
endforeach()
