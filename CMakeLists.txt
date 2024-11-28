cmake_minimum_required(VERSION 3.10)

# Set the project name
project(ChessEngine_Lilia VERSION 1.0.0)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)

# Define SFML_STATIC for static linking (optional, depends on your use case)
set(SFML_STATIC_LIBRARIES TRUE)
# Optional: Set SFML_DIR if it isn't in a standard location
# Replace the path below with your actual SFML installation directory
set(SFML_DIR ${CMAKE_SOURCE_DIR}/libs/sfml/lib/cmake/SFML)
# Find SFML package with required components
find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)

include_directories(include)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)

# Executable naming
set(EXECUTABLE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
# Add the executable
add_executable(${EXECUTABLE_NAME} examples/main.cpp ${SRC_FILES})

# Link SFML libraries (dependencies are automatically resolved)
target_link_libraries(${EXECUTABLE_NAME} sfml-graphics sfml-window sfml-system)


