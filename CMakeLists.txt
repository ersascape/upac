cmake_minimum_required(VERSION 3.16)

project(upac VERSION 0.1.0 LANGUAGES CXX C)

include(GNUInstallDirs)

option(UPAC_BUILD_CLI "Build the upac command-line tool" ON)
option(UPAC_ENABLE_IPO "Enable interprocedural optimization when supported" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(pugixml STATIC third_party/pugixml/pugixml.cpp)
add_library(upac::pugixml ALIAS pugixml)
target_include_directories(pugixml PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/pugixml>
)
set_target_properties(pugixml PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(upac_lib STATIC
    src/pac.cpp
    src/pac_reader.cpp
    src/xml_config.cpp
)
add_library(upac::upac ALIAS upac_lib)
target_compile_features(upac_lib PUBLIC cxx_std_17)
target_include_directories(upac_lib
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(upac_lib PUBLIC upac::pugixml)
set_target_properties(upac_lib PROPERTIES
    OUTPUT_NAME upac
    POSITION_INDEPENDENT_CODE ON
)

if(UPAC_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT upac_ipo_supported OUTPUT upac_ipo_error)
    if(upac_ipo_supported)
        set_property(TARGET pugixml upac_lib PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

if(UPAC_BUILD_CLI)
    add_executable(upac-cli src/main.cpp)
    target_link_libraries(upac-cli PRIVATE upac::upac)
    set_target_properties(upac-cli PROPERTIES OUTPUT_NAME upac)
endif()
