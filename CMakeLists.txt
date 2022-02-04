cmake_minimum_required(VERSION 3.8)
project(as1 LANGUAGES CXX)

find_package(Boost)
add_subdirectory("thirdparty/lexy")

set(CMAKE_BUILD_TYPE Debug)
set(CXX_STANDARD 17)

file(GLOB sources "src/*.cpp" "src/*.c")
set(includes "src/" ${ext_include_dir} ${Boost_INCLUDE_DIRS})
set(libraries lexy ${Boost_LIBRARIES})

add_executable (as1 ${sources})
target_include_directories (as1 PUBLIC ${includes})
target_link_libraries (as1 LINK_PUBLIC ${libraries})