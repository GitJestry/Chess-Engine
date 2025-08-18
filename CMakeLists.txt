cmake_minimum_required(VERSION 3.16) # Needed for $<TARGET_RUNTIME_DLLS>
project(ChessEngine_Lilia VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------
# Fetch SFML
# -------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 2.6.0
)
FetchContent_MakeAvailable(SFML)

# -------------------------------------------------------------
# Sources
# -------------------------------------------------------------
file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)

set(EXECUTABLE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
add_executable(${EXECUTABLE_NAME}
    examples/main.cpp
    ${SRC_FILES}
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# -------------------------------------------------------------
# Link SFML
# -------------------------------------------------------------
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
    sfml-audio
)

if (WIN32)
    # Kopiere automatisch alle benötigten SFML Runtime DLLs
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${EXECUTABLE_NAME}>
            $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>
        COMMAND_EXPAND_LISTS
    )

    # Kopiere explizit OpenAL32.dll, falls vorhanden
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying OpenAL32.dll (if exists)..."
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${sfml_SOURCE_DIR}/extlibs/bin/x64/openal32.dll"
            $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>
        VERBATIM
    )
endif()


# -------------------------------------------------------------
# Organize build output
# -------------------------------------------------------------
set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
