cmake_minimum_required(VERSION 3.16)
project(ChessEngine_Lilia VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------
# Option: baue mit oder ohne UI
# -------------------------------------------------------------
option(LILIA_UI "Build with SFML UI" ON)

# -------------------------------------------------------------
# Sources (Core: immer)
# -------------------------------------------------------------
file(GLOB_RECURSE MODEL_FILES     ${PROJECT_SOURCE_DIR}/src/lilia/model/*.cpp)
file(GLOB_RECURSE ENGINE_FILES    ${PROJECT_SOURCE_DIR}/src/lilia/engine/*.cpp)
file(GLOB_RECURSE UCI_FILES       ${PROJECT_SOURCE_DIR}/src/lilia/uci/*.cpp)

set(CORE_FILES
    ${MODEL_FILES}
    ${ENGINE_FILES}
    ${UCI_FILES}
)

# -------------------------------------------------------------
# Sources (UI: nur wenn gewünscht)
# -------------------------------------------------------------
if(LILIA_UI)
    file(GLOB_RECURSE CONTROLLER_FILES ${PROJECT_SOURCE_DIR}/src/lilia/controller/*.cpp)
    file(GLOB_RECURSE UI_FILES         ${PROJECT_SOURCE_DIR}/src/lilia/view/*.cpp)
    file(GLOB_RECURSE APP_FILES        ${PROJECT_SOURCE_DIR}/src/lilia/app/*.cpp)
endif()

# -------------------------------------------------------------
# Executable
# -------------------------------------------------------------
set(EXECUTABLE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")

if(LILIA_UI)
    add_executable(${EXECUTABLE_NAME}
        examples/main.cpp
        ${CORE_FILES}
        ${CONTROLLER_FILES}
        ${UI_FILES}
        ${APP_FILES}
    )
else()
    add_executable(${EXECUTABLE_NAME}
        examples/main.cpp
        ${CORE_FILES}
    )
endif()

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# -------------------------------------------------------------
# Falls mit UI: SFML laden und linken
# -------------------------------------------------------------
if(LILIA_UI)
    message(STATUS "Building with UI (SFML)")

    include(FetchContent)
    FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 2.6.0
    )
    FetchContent_MakeAvailable(SFML)

    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        sfml-graphics
        sfml-window
        sfml-system
        sfml-audio
    )

    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE LILIA_UI)
else()
    message(STATUS "Building engine-only (UCI mode)")
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE LILIA_ENGINE)
endif()

# -------------------------------------------------------------
# Windows: SFML-DLLs kopieren
# -------------------------------------------------------------
if (WIN32 AND LILIA_UI)
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${EXECUTABLE_NAME}>
            $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>
        COMMAND_EXPAND_LISTS
    )

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
