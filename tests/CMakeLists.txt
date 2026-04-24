cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test)

# Test executable
add_executable(test_strategies
    test_strategies.cpp
    ../src/jsontreemodel.cpp
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
)

target_link_libraries(test_strategies PRIVATE
    simdjson_static
    Qt6::Core
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
)

target_link_libraries(test_paging PRIVATE
    simdjson_static
    Qt6::Core
    Qt6::Test
)

# Add paging test
add_test(NAME PagingTests COMMAND test_paging)

set_tests_properties(PagingTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
