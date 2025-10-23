cmake_minimum_required(VERSION 3.22.0)
project(iminspect LANGUAGES CXX)

if(POLICY CMP0128)
    cmake_policy(SET CMP0128 NEW)
endif() 
 
set(INCS "")
file(GLOB_RECURSE INCS "include/*.hpp")
 
set(SRCS "") 
file(GLOB_RECURSE SRCS "src/*.cpp")

add_library(iminspect ${INCS} ${SRCS})
add_library(iminspect::iminspect ALIAS iminspect)

find_package(imgui CONFIG REQUIRED)
find_package(lahzam CONFIG REQUIRED)
find_package(enchantum CONFIG REQUIRED)
find_package(imsweet CONFIG REQUIRED)
 

target_link_libraries(iminspect PUBLIC enchantum::enchantum imgui::imgui lahzam::lahzam imsweet::imsweet)

target_include_directories(iminspect PUBLIC
    include
)

option(IMINSPECT_BUILD_EXAMPLES "examples" OFF)

if(IMINSPECT_BUILD_EXAMPLES)
  add_subdirectory(thirdparty/glad)
  add_subdirectory(examples)
endif()
