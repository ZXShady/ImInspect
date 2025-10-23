cmake_minimum_required(VERSION 3.22.0)

find_package(OpenGL REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
file(GLOB_RECURSE SRCS "*.cpp")
add_executable(examples ${SRCS})
target_link_libraries(examples PRIVATE EnTT::EnTT OpenGL::GL glfw glad::glad iminspect::iminspect)

