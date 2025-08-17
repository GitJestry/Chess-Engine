cmake_minimum_required(VERSION 3.10)

project(ChessEngine_Lilia VERSION 1.0.1)

set(CMAKE_CXX_STANDARD 17)

set(SFML_STATIC_LIBRARIES TRUE)
set(SFML_DIR ${CMAKE_SOURCE_DIR}/libs/sfml/lib/cmake/SFML)
find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)

set(EXECUTABLE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
add_executable(${EXECUTABLE_NAME} examples/main.cpp ${SRC_FILES})

target_include_directories(${EXECUTABLE_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(${EXECUTABLE_NAME} sfml-graphics sfml-window sfml-system)
