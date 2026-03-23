cmake_minimum_required(VERSION 3.16)
project(upac VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- pugixml (vendored) ---
add_library(pugixml STATIC third_party/pugixml/pugixml.cpp)
target_include_directories(pugixml PUBLIC third_party/pugixml)

# --- libupac (static library, reusable by uflash later) ---
add_library(upac_lib STATIC
    src/pac.cpp
    src/pac_reader.cpp
    src/xml_config.cpp
)
target_include_directories(upac_lib PUBLIC include)
target_link_libraries(upac_lib PUBLIC pugixml)

# --- upac CLI ---
add_executable(upac-cli src/main.cpp)
target_link_libraries(upac-cli PRIVATE upac_lib)
