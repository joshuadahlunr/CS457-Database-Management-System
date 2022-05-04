#------------------------------------------------------------
# Filename: CMakeLists.txt
# Author: Joshua Dahl
# Email: joshuadahl@nevada.unr.edu
# Created: 2/7/22
# Modified: 3/14/22
# Description: CMAKE build script responsible for building the project and libraries
#------------------------------------------------------------

cmake_minimum_required(VERSION 3.8)
project(Database)

find_package(Boost)
add_subdirectory("thirdparty/lexy")

set(CMAKE_BUILD_TYPE Debug)
set(CXX_STANDARD 17 REQUIRED)

file(GLOB sources "src/*.cpp" "src/*.c" "thirdparty/linenoise/linenoise.c")
set(includes "src/" "thirdparty/linenoise/" "thirdparty/simplebinstream/TestBinStream" ${ext_include_dir} ${Boost_INCLUDE_DIRS})
set(libraries lexy ${Boost_LIBRARIES})

add_executable (pa4 ${sources})
target_include_directories (pa4 PUBLIC ${includes})
target_link_libraries (pa4 LINK_PUBLIC ${libraries})
