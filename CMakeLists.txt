cmake_minimum_required(VERSION 3.16)
project(jsontreeviewer VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# MSVC: Enable Debug Information in Release builds
# /Zi: Generates a PDB file
# /DEBUG: Linker flag to create the PDB
# /OPT:REF & /OPT:ICF: Optimize the binary despite debug info presence
if(MSVC)
    add_compile_options("$<$<CONFIG:Release>:/Zi>")
    add_link_options(
        "$<$<CONFIG:Release>:/DEBUG>"
        "$<$<CONFIG:Release>:/OPT:REF>"
        "$<$<CONFIG:Release>:/OPT:ICF>"
    )
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

include(FetchContent)
FetchContent_Declare(SeerSdk
    GIT_REPOSITORY https://github.com/ccseer/Seer-sdk.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SeerSdk)

add_library(simdjson_static STATIC
    simdjson/simdjson.cpp
    simdjson/simdjson.h
)

target_include_directories(simdjson_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/simdjson
)

add_library(jsontreeviewer SHARED
    src/jsontreemodel.cpp
    src/jsontreeview.cpp
    src/jsontreeviewer.cpp
    src/memorymappedfile.cpp
    src/loadworker.cpp
    src/jsonnode.h
    src/jsontreemodel.h
    src/jsontreeview.h
    src/jsontreeviewer.h
    src/memorymappedfile.h
    src/loadworker.h
    src/logging.h
    src/strategies/jsonstrategy.h
    src/strategies/jsonstrategy.cpp
    src/strategies/smallfilestrategy.h
    src/strategies/smallfilestrategy.cpp
    src/strategies/mediumfilestrategy.h
    src/strategies/mediumfilestrategy.cpp
    src/strategies/largefilestrategy.h
    src/strategies/largefilestrategy.cpp
    src/strategies/extremefilestrategy.h
    src/strategies/extremefilestrategy.cpp
    ${seersdk_SOURCE_DIR}/seer/viewerbase.h
)

target_link_libraries(jsontreeviewer PRIVATE
    simdjson_static
    SeerSdk::SeerSdk
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(jsontreeviewer PROPERTIES
    OUTPUT_NAME "jsontreeviewer"
    SUFFIX ".dll"
)

target_compile_definitions(jsontreeviewer PRIVATE
    COMPANY_NAME="1218.io"
    PRODUCT_NAME="Seer"
)

add_executable(test_jsontreeviewer WIN32 src/test.cpp)
target_link_libraries(test_jsontreeviewer PRIVATE
    jsontreeviewer
    simdjson_static
    SeerSdk::SeerSdk
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# Add tests subdirectory
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
