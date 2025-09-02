cmake_minimum_required(VERSION 3.21)  # needed for $<TARGET_RUNTIME_DLLS:...>
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
option(LILIA_NATIVE       "Enable native CPU optimizations (-march=native or /arch:AVX2)" ON)
option(LILIA_FAST_MATH    "Enable fast-math in Release" ON)
option(LILIA_LTO          "Enable Link-Time Optimization/IPO in Release" ON)
option(LILIA_PGO_GENERATE "Build with PGO generate (instrumentation)" OFF)
option(LILIA_PGO_USE      "Build with PGO use (optimized by profiles)" OFF)

# ---------------------------
# Runtime assets path (shipped next to the exe)
# ---------------------------
set(LILIA_ASSETS_DIR "${PROJECT_SOURCE_DIR}/assets"
    CACHE PATH "Directory with runtime assets (textures, sounds, fonts, etc.)")

# ---------------------------
# Sources
# ---------------------------
file(GLOB_RECURSE MODEL_FILES      ${PROJECT_SOURCE_DIR}/src/lilia/model/*.cpp)
file(GLOB_RECURSE ENGINE_FILES     ${PROJECT_SOURCE_DIR}/src/lilia/engine/*.cpp)
file(GLOB_RECURSE UCI_FILES        ${PROJECT_SOURCE_DIR}/src/lilia/uci/*.cpp)
file(GLOB_RECURSE CONTROLLER_FILES ${PROJECT_SOURCE_DIR}/src/lilia/controller/*.cpp)
file(GLOB_RECURSE UI_FILES         ${PROJECT_SOURCE_DIR}/src/lilia/view/*.cpp)
file(GLOB_RECURSE APP_FILES        ${PROJECT_SOURCE_DIR}/src/lilia/app/*.cpp)
file(GLOB_RECURSE BOT_FILES        ${PROJECT_SOURCE_DIR}/src/lilia/bot/*.cpp)

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
  ${BOT_FILES}
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
    target_compile_options(${tgt} PRIVATE
      $<$<CONFIG:Release>:/O2 /Ot /Oi /DNDEBUG /Gy /Gw>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_FAST_MATH}>>:/fp:fast>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_NATIVE}>>:/arch:AVX2>
    )
    if (LILIA_LTO)
      include(CheckIPOSupported)
      check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_out)
      if (_ipo_ok)
        set_property(TARGET ${tgt} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
      else()
        message(STATUS "IPO/LTO not supported: ${_ipo_out}")
      endif()
    endif()
    if (LILIA_PGO_GENERATE)
      target_link_options(${tgt} PRIVATE $<$<CONFIG:Release>:/GENPROFILE>)
    endif()
    if (LILIA_PGO_USE)
      target_link_options(${tgt} PRIVATE $<$<CONFIG:Release>:/USEPROFILE>)
    endif()
    target_compile_definitions(${tgt} PRIVATE NOMINMAX)
  else()
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
      set(IS_GCC TRUE)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
      set(IS_CLANG TRUE)
    endif()

    target_compile_options(${tgt} PRIVATE
      $<$<CONFIG:Release>:-O3 -DNDEBUG -fomit-frame-pointer>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_NATIVE}>>:-march=native -mtune=native>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${LILIA_FAST_MATH}>>:-ffast-math>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${IS_GCC}>>:-fno-plt>
      $<$<AND:$<CONFIG:Release>,$<BOOL:${IS_CLANG}>>:-fstrict-aliasing>
      $<$<CONFIG:Release>:-fvisibility=hidden -fvisibility-inlines-hidden>
    )
    if (LILIA_LTO)
      target_link_options(${tgt} PRIVATE $<$<CONFIG:Release>:-flto>)
      if (IS_GCC)
        target_link_options(${tgt} PRIVATE -Wl,-O2)
      endif()
    endif()
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
# Copy assets next to the EXE after build (Option 1)
# ---------------------------
function(lilia_copy_assets tgt)
  if (EXISTS "${LILIA_ASSETS_DIR}")
    add_custom_command(TARGET ${tgt} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${tgt}>/assets"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${LILIA_ASSETS_DIR}"
              "$<TARGET_FILE_DIR:${tgt}>/assets"
      COMMENT "Copying runtime assets to $<TARGET_FILE_DIR:${tgt}>/assets"
    )
  else()
    message(WARNING "LILIA_ASSETS_DIR not found: ${LILIA_ASSETS_DIR} — assets won't be copied.")
  endif()
endfunction()

# copy only for the UI app (engine/uci may not need assets)
lilia_copy_assets(lilia_app)

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

# ---------------------------
# Install rules (so cpack/cmake --install also place assets)
# ---------------------------
install(TARGETS lilia_app
  RUNTIME DESTINATION .
  BUNDLE  DESTINATION .  # future macOS bundle support if enabled
)
if (EXISTS "${LILIA_ASSETS_DIR}")
  install(DIRECTORY "${LILIA_ASSETS_DIR}/" DESTINATION assets)
endif()

# Ensure runtime DLLs (SFML, MinGW/MSVC, etc.) are installed into the package
if (WIN32)
  install(FILES $<TARGET_RUNTIME_DLLS:lilia_app> DESTINATION .)
  install(FILES "${sfml_SOURCE_DIR}/extlibs/bin/x64/openal32.dll" DESTINATION . OPTIONAL)
  # (Optional, MSVC only) also bundle MSVC CRT/UCRT if needed:
  include(InstallRequiredSystemLibraries)
endif()

# Minimal packaging convenience (ZIP on Win/Linux, DragNDrop on macOS)
set(CPACK_PACKAGE_NAME "ChessEngine_Lilia")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
if (WIN32)
  set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
  set(CPACK_GENERATOR "DragNDrop")
else()
  set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)

# Optional: export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
