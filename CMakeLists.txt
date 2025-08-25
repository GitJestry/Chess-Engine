cmake_minimum_required(VERSION 3.16)
project(ChessEngine_Lilia VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------
# Sources (Core: immer)
# -------------------------------------------------------------
file(GLOB_RECURSE MODEL_FILES     ${PROJECT_SOURCE_DIR}/src/lilia/model/*.cpp)
file(GLOB_RECURSE ENGINE_FILES    ${PROJECT_SOURCE_DIR}/src/lilia/engine/*.cpp)
file(GLOB_RECURSE UCI_FILES       ${PROJECT_SOURCE_DIR}/src/lilia/uci/*.cpp)
file(GLOB_RECURSE CONTROLLER_FILES ${PROJECT_SOURCE_DIR}/src/lilia/controller/*.cpp)

set(CORE_FILES
    ${MODEL_FILES}
    ${ENGINE_FILES}
    ${UCI_FILES}
)

# -------------------------------------------------------------
# Sources (UI: nur für App)
# -------------------------------------------------------------
file(GLOB_RECURSE UI_FILES   ${PROJECT_SOURCE_DIR}/src/lilia/view/*.cpp)
file(GLOB_RECURSE CONTROLLER_FILES   ${PROJECT_SOURCE_DIR}/src/lilia/controller/*.cpp)
file(GLOB_RECURSE APP_FILES  ${PROJECT_SOURCE_DIR}/src/lilia/app/*.cpp)

# -------------------------------------------------------------
# Engine-only executable
# -------------------------------------------------------------
add_executable(lilia_engine
    examples/main.cpp
    ${CORE_FILES}
)

target_compile_definitions(lilia_engine PRIVATE LILIA_ENGINE)
target_include_directories(lilia_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/lilia
)

# -------------------------------------------------------------
# UI executable
# -------------------------------------------------------------
add_executable(lilia_app
    examples/main.cpp
    ${CORE_FILES}
    ${UI_FILES}
    ${APP_FILES}
    ${CONTROLLER_FILES}
)

target_compile_definitions(lilia_app PRIVATE LILIA_UI)
target_include_directories(lilia_app PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/lilia
)

# SFML nur für UI
include(FetchContent)
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 2.6.0
)
FetchContent_MakeAvailable(SFML)

target_link_libraries(lilia_app PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
    sfml-audio
)

# -------------------------------------------------------------
# Windows: SFML-DLLs kopieren
# -------------------------------------------------------------
if (WIN32)
    add_custom_command(TARGET lilia_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:lilia_app>
            $<TARGET_FILE_DIR:lilia_app>
        COMMAND_EXPAND_LISTS
    )

    add_custom_command(TARGET lilia_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying OpenAL32.dll (if exists)..."
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${sfml_SOURCE_DIR}/extlibs/bin/x64/openal32.dll"
            $<TARGET_FILE_DIR:lilia_app>
        VERBATIM
    )
endif()

# -------------------------------------------------------------
# Organize build output
# -------------------------------------------------------------
foreach(target lilia_engine lilia_app)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
endforeach()
