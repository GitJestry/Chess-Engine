cmake_minimum_required(VERSION 3.16)
project(ChessEngine_Lilia VERSION 1.0 LANGUAGES CXX)

# ---------------------------
# C++ Standard
# ---------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------
# Buildtype Default (Make/Ninja)
# ---------------------------
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------
# Options (perf toggles)
# ---------------------------
option(LILIA_NATIVE      "Enable native CPU optimizations (-march=native or /arch:AVX2)" ON)
option(LILIA_FAST_MATH   "Enable fast-math in Release" ON)
option(LILIA_LTO         "Enable Link-Time Optimization/IPO in Release" ON)
option(LILIA_PGO_GENERATE "Build with PGO generate (instrumentation)" OFF)
option(LILIA_PGO_USE      "Build with PGO use (optimized by profiles)" OFF)

# Hinweis: Exceptions werden in deinem Code genutzt -> KEIN -fno-exceptions!

# ---------------------------
# Sources
# ---------------------------
file(GLOB_RECURSE MODEL_FILES      ${PROJECT_SOURCE_DIR}/src/lilia/model/*.cpp)
file(GLOB_RECURSE ENGINE_FILES     ${PROJECT_SOURCE_DIR}/src/lilia/engine/*.cpp)
file(GLOB_RECURSE UCI_FILES        ${PROJECT_SOURCE_DIR}/src/lilia/uci/*.cpp)
file(GLOB_RECURSE CONTROLLER_FILES ${PROJECT_SOURCE_DIR}/src/lilia/controller/*.cpp)
file(GLOB_RECURSE UI_FILES         ${PROJECT_SOURCE_DIR}/src/lilia/view/*.cpp)
file(GLOB_RECURSE APP_FILES        ${PROJECT_SOURCE_DIR}/src/lilia/app/*.cpp)

set(CORE_FILES
  ${MODEL_FILES}
  ${ENGINE_FILES}
  ${UCI_FILES}
)

# ---------------------------
# Targets
# ---------------------------
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

# ---------------------------
# SFML for UI
# ---------------------------
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

# ---------------------------
# Threads
# ---------------------------
find_package(Threads REQUIRED)
target_link_libraries(lilia_engine PRIVATE Threads::Threads)
target_link_libraries(lilia_app    PRIVATE Threads::Threads)

# ---------------------------
# Helper macro: add perf flags per target
# ---------------------------
function(lilia_set_perf_flags tgt)
  if (MSVC)
    # Compile
    target_compile_options(${tgt} PRIVATE
      $<$<CONFIG:Release>:/O2 /Ot /Oi /DNDEBUG /Gy /Gw>   # O2, intrinsics, function/data sections
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_FAST_MATH}>>:/fp:fast>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_NATIVE}>>:/arch:AVX2>  # oder /arch:AVX512 wenn sicher
    )
    # Link
    if (LILIA_LTO)
      include(CheckIPOSupported)
      check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_out)
      if (_ipo_ok)
        set_property(TARGET ${tgt} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
      else()
        message(STATUS "IPO/LTO not supported: ${_ipo_out}")
      endif()
    endif()

    # PGO (MSVC): nur grob als Hinweis – echte PGO-Pipeline ist VS/MSBuild-spezifisch
    if (LILIA_PGO_GENERATE)
      target_link_options(${tgt} PRIVATE $<$<CONFIG:Release>:/GENPROFILE>)
    endif()
    if (LILIA_PGO_USE)
      target_link_options(${tgt} PRIVATE $<$<CONFIG:Release>:/USEPROFILE>)
    endif()

    # Kleinkram
    target_compile_definitions(${tgt} PRIVATE NOMINMAX)
  else()
    # Detect GCC vs (Apple)Clang
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
      set(IS_GCC TRUE)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
      set(IS_CLANG TRUE)
    endif()

    # Compile
    target_compile_options(${tgt} PRIVATE
      $<$<CONFIG:Release>:-O3 -DNDEBUG -fomit-frame-pointer>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_NATIVE}>>:-march=native -mtune=native>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_FAST_MATH}>>:-ffast-math>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${IS_GCC}>>:-fno-plt>       # nur GCC
      $<$<AND:$<CONFIG:Release>,$<BOOL:${IS_CLANG}>>:-fstrict-aliasing>
      # Sichtbarkeit (kleinere Binärgröße, schnellerer Link)
      $<$<CONFIG:Release>:-fvisibility=hidden -fvisibility-inlines-hidden>
    )
    # Link
    if (LILIA_LTO)
      target_link_options(${tgt} PRIVATE $<$<CONFIG:Release>:-flto>)
      if (IS_GCC)
        target_link_options(${tgt} PRIVATE -Wl,-O2)
      endif()
    endif()

    # PGO (GCC/Clang)
    if (LILIA_PGO_GENERATE)
      target_compile_options(${tgt} PRIVATE $<$<CONFIG:Release>:-fprofile-generate>)
      target_link_options(${tgt}    PRIVATE $<$<CONFIG:Release>:-fprofile-generate>)
    endif()
    if (LILIA_PGO_USE)
      target_compile_options(${tgt} PRIVATE $<$<CONFIG:Release>:-fprofile-use -fprofile-correction>)
      target_link_options(${tgt}    PRIVATE $<$<CONFIG:Release>:-fprofile-use>)
    endif()
  endif()
endfunction()

# Apply flags
lilia_set_perf_flags(lilia_engine)
lilia_set_perf_flags(lilia_app)

# ---------------------------
# IPO global fallback (in case)
# ---------------------------
if (LILIA_LTO AND NOT MSVC)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_ok OUTPUT ipo_out)
  if (ipo_ok)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  else()
    message(STATUS "IPO/LTO not supported: ${ipo_out}")
  endif()
endif()

# ---------------------------
# Windows: copy SFML DLLs
# ---------------------------
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

# ---------------------------
# Output dirs
# ---------------------------
foreach(target lilia_engine lilia_app)
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
endforeach()

# Optional: erleichtert Flag-Überprüfung bei Make/Ninja
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
