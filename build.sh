#!/bin/bash
set -e

# --- Configuration ---
CXX=clang++
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -Iinclude -Ithird_party/pugixml"
OUT="upac-cli"

echo "Building upac..."

# --- Compilation ---
$CXX $CXXFLAGS \
    third_party/pugixml/pugixml.cpp \
    src/pac.cpp \
    src/pac_reader.cpp \
    src/xml_config.cpp \
    src/main.cpp \
    -o$OUT

echo "Build successful! Created: ./$OUT"
./$OUT help
