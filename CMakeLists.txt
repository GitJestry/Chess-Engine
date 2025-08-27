cmake_minimum_required(VERSION 3.16)
project(ChessEngine_Lilia VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------
# Optionen für schnelle Builds (abschaltbar für Portabilität)
# -------------------------------------------------------------
option(LILIA_NATIVE   "Enable native CPU optimizations (-march=native or /arch:AVX2)" ON)
option(LILIA_FAST_MATH "Enable fast-math in Release" ON)
option(LILIA_LTO      "Enable Link-Time Optimization/IPO in Release" ON)

# Default-Buildtype für Single-Config-Generatoren (Ninja/Makefiles)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# -------------------------------------------------------------
# Quellen
# -------------------------------------------------------------
file(GLOB_RECURSE MODEL_FILES      ${PROJECT_SOURCE_DIR}/src/lilia/model/*.cpp)
file(GLOB_RECURSE ENGINE_FILES     ${PROJECT_SOURCE_DIR}/src/lilia/engine/*.cpp)
file(GLOB_RECURSE UCI_FILES        ${PROJECT_SOURCE_DIR}/src/lilia/uci/*.cpp)
file(GLOB_RECURSE CONTROLLER_FILES ${PROJECT_SOURCE_DIR}/src/lilia/controller/*.cpp)

set(CORE_FILES
    ${MODEL_FILES}
    ${ENGINE_FILES}
    ${UCI_FILES}
)

file(GLOB_RECURSE UI_FILES   ${PROJECT_SOURCE_DIR}/src/lilia/view/*.cpp)
file(GLOB_RECURSE APP_FILES  ${PROJECT_SOURCE_DIR}/src/lilia/app/*.cpp)

# -------------------------------------------------------------
# Executables
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

# -------------------------------------------------------------
# SFML nur für UI
# -------------------------------------------------------------
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
# Threads (für std::thread / atomics auf POSIX)
# -------------------------------------------------------------
find_package(Threads REQUIRED)
target_link_libraries(lilia_engine PRIVATE Threads::Threads)
target_link_libraries(lilia_app    PRIVATE Threads::Threads)

# -------------------------------------------------------------
# Performance-Flags (pro Target, nur für Release)
# -------------------------------------------------------------
foreach(tgt IN ITEMS lilia_engine lilia_app)
  if (MSVC)
    # Compiler: /O2 + Intrinsics + favor speed; optional AVX2 & fast-math
    target_compile_options(${tgt} PRIVATE
      $<$<CONFIG:Release>:/O2 /Ot /Oi /DNDEBUG>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_FAST_MATH}>>:/fp:fast>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_NATIVE}>>:/arch:AVX2>
    )
    # Linker: LTCG (Whole Program Optimization)
    target_link_options(${tgt} PRIVATE
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_LTO}>>:/LTCG>
    )
    # Optional: less Windows macro pain
    target_compile_definitions(${tgt} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>)
  elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${tgt} PRIVATE
      $<$<CONFIG:Release>:-O3 -fomit-frame-pointer -DNDEBUG>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_NATIVE}>>:-march=native -mtune=native>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_FAST_MATH}>>:-ffast-math>
      $<$<CONFIG:Release>:-fno-plt> # kleine Call-Overhead-Reduktion (GCC)
    )
    target_link_options(${tgt} PRIVATE
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_LTO}>>:-flto>
      $<$<CXX_COMPILER_ID:GNU>:-Wl,-O2>
    )
  endif()
endforeach()

# Interprocedural Optimization (IPO) portabel aktivieren
if (LILIA_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_ok OUTPUT ipo_out)
  if (ipo_ok)
    set_property(TARGET lilia_engine PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(TARGET lilia_app    PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  else()
    message(STATUS "IPO/LTO not supported: ${ipo_out}")
  endif()
endif()

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
# Build-Outputs ordnen
# -------------------------------------------------------------
foreach(target lilia_engine lilia_app)
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
endforeach()
