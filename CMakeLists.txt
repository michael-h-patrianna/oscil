cmake_minimum_required(VERSION 3.20)

# Project metadata
project(oscil
  VERSION 0.1.0
  DESCRIPTION "Multi-channel oscilloscope effect (VST3) with modern responsive UI"
  LANGUAGES CXX C)

# Export compile commands for tooling (clangd, iwyu)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_TESTING "Enable tests" ON)
option(ENABLE_LTO "Enable link-time optimization" ON)
option(OSCIL_ENABLE_CLANG_TIDY "Enable clang-tidy if available" ON)
option(OSCIL_ENABLE_SANITIZERS "Enable address/ub sanitizers for tests in Debug" OFF)
option(OSCIL_PLUGIN_SANITIZERS "Also apply sanitizers to plugin target (use only for debugging in isolated host)" OFF)
option(OSCIL_INSTALL_SYSTEM_VST3_ON_BUILD "Attempt to copy VST3 to /Library/Audio/Plug-Ins/VST3 after each build (macOS)" OFF)
option(OSCIL_WARNINGS_AS_ERRORS "Treat warnings as errors for project sources" ON)
option(OSCIL_ENABLE_IWYU "Enable include-what-you-use if available" OFF)
option(OSCIL_ENABLE_COVERAGE "Enable coverage flags and gcovr target (Linux/Debug)" OFF)
option(OSCIL_BUILD_UNIVERSAL "On macOS build a universal (arm64+x86_64) binary" ON)
option(OSCIL_ENABLE_OPENGL "Enable optional OpenGL acceleration via JUCE OpenGL context (default OFF for safety)" OFF)
option(OSCIL_DEBUG_HOOKS "Enable debug GPU render hooks for testing (requires OSCIL_ENABLE_OPENGL)" OFF)

# Allow user to override JUCE tag; default to master to track latest until pinned
set(JUCE_GIT_REPOSITORY "https://github.com/juce-framework/JUCE.git" CACHE STRING "JUCE git repository")
set(JUCE_GIT_TAG "master" CACHE STRING "JUCE git tag or branch to checkout (e.g., 7.0.12 or master)")

# External dependencies via FetchContent (built-in CMake)
include(FetchContent)

# Speed up builds using ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
  set(CMAKE_C_COMPILER_LAUNCHER   ${CCACHE_PROGRAM})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Fetch JUCE
FetchContent_Declare(
  juce
  GIT_REPOSITORY ${JUCE_GIT_REPOSITORY}
  GIT_TAG        ${JUCE_GIT_TAG}
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

# JUCE options
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "")
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "")

# Try to let JUCE download VST3 SDK automatically when configuring
set(JUCE_VST3_HEADERS_ONLY TRUE CACHE BOOL "Build VST3 wrapper with headers-only SDK when possible")
set(JUCE_VST3_SDK_DOWNLOAD ON CACHE BOOL "Let JUCE download VST3 SDK automatically")

# Global settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
  # Required for hardened runtime in future; not enforced now
  add_compile_definitions(JUCE_MAC=1)
  if(OSCIL_BUILD_UNIVERSAL)
    if(NOT CMAKE_OSX_ARCHITECTURES)
      set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
    endif()
  endif()
elseif(WIN32)
  add_compile_definitions(JUCE_WINDOWS=1)
else()
  add_compile_definitions(JUCE_LINUX=1)
endif()

# Clang-Tidy
if(OSCIL_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-warnings-as-errors=*;")
  endif()
endif()

# Include-What-You-Use (optional)
if(OSCIL_ENABLE_IWYU)
  find_program(IWYU_EXE NAMES iwyu include-what-you-use)
  if(IWYU_EXE)
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_EXE})
  endif()
endif()

# (Sanitizers now applied per-target below to avoid breaking DAW plugin hosts during scan)

# LTO
if(ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
  if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
  endif()
endif()

# Sources
set(OSCIL_SOURCES
  src/plugin/PluginProcessor.cpp
  src/plugin/PluginEditor.cpp
  src/render/OscilloscopeComponent.cpp
  src/render/OpenGLManager.cpp
  src/render/GpuRenderHook.cpp
  src/render/DecimationProcessor.cpp
  src/render/AdvancedDecimationProcessor.cpp
  src/audio/WaveformDataBridge.cpp
  src/audio/MultiTrackEngine.cpp
  src/audio/SignalProcessor.cpp
  src/audio/MeasurementData.cpp
  src/state/TrackState.cpp
  src/theme/ColorTheme.cpp
  src/theme/ThemeManager.cpp
  src/util/PerformanceMonitor.cpp
  src/ui/components/TrackSelectorComponent.cpp
  src/ui/layout/LayoutManager.cpp
  src/ui/measurements/CorrelationMeterComponent.cpp
  src/ui/measurements/MeasurementOverlayComponent.cpp
  src/timing/TimingEngine.cpp
)
set(OSCIL_HEADERS
  src/plugin/PluginProcessor.h
  src/plugin/PluginEditor.h
  src/render/OscilloscopeComponent.h
  src/render/OpenGLManager.h
  src/render/GpuRenderHook.h
  src/render/DecimationProcessor.h
  src/render/AdvancedDecimationProcessor.h
  src/dsp/RingBuffer.h
  src/audio/WaveformDataBridge.h
  src/audio/MultiTrackEngine.h
  src/audio/SignalProcessor.h
  src/audio/ProcessingModes.h
  src/audio/MeasurementData.h
  src/state/TrackState.h
  src/theme/ColorTheme.h
  src/theme/ThemeManager.h
  src/util/PerformanceMonitor.h
  src/ui/components/TrackSelectorComponent.h
  src/ui/layout/LayoutManager.h
  src/ui/measurements/CorrelationMeterComponent.h
  src/ui/measurements/MeasurementOverlayComponent.h
  src/timing/TimingEngine.h
)

# Define the plugin target using JUCE's CMake functions
set(PLUGIN_FORMATS Standalone)
if(APPLE)
  list(APPEND PLUGIN_FORMATS AU)
elseif(UNIX AND NOT APPLE)
  list(APPEND PLUGIN_FORMATS LV2)
endif()
# Always ensure VST3 is present (primary distribution format)
if(NOT "VST3" IN_LIST PLUGIN_FORMATS)
  list(INSERT PLUGIN_FORMATS 0 VST3)
endif()

juce_add_plugin(oscil_plugin
    COMPANY_NAME                "Oscil Audio"
    BUNDLE_ID                   com.oscil.audio
    PLUGIN_NAME                 "Oscil"
    PRODUCT_NAME                "Oscil"
  PLUGIN_MANUFACTURER_CODE    OscA
  PLUGIN_CODE                 Osc1
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    VST3_CATEGORIES            "Fx|Analyzer"
    VST3_SUBCATEGORIES         "Analyzer"
  COPY_PLUGIN_AFTER_BUILD     TRUE
  FORMATS                     ${PLUGIN_FORMATS}
)

# Post-build auto system install (macOS optional)
if(APPLE AND OSCIL_INSTALL_SYSTEM_VST3_ON_BUILD)
  set(VST3_BUNDLE_PATH "${CMAKE_BINARY_DIR}/oscil_plugin_artefacts/$<CONFIG>/VST3/Oscil.vst3")
  add_custom_command(TARGET oscil_plugin_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "[oscil] Auto-installing VST3 to /Library/Audio/Plug-Ins/VST3 (may fail without permissions)"
    COMMAND /bin/sh -c "if [ -d ${VST3_BUNDLE_PATH} ]; then if rm -rf /Library/Audio/Plug-Ins/VST3/Oscil.vst3 2>/dev/null; then :; fi; cp -R ${VST3_BUNDLE_PATH} /Library/Audio/Plug-Ins/VST3/ 2>/dev/null || echo '[oscil] System copy skipped (need sudo). Run: sudo cmake --build build --config $<CONFIG> --target install-vst3-system'; else echo '[oscil] Built VST3 bundle not found: ${VST3_BUNDLE_PATH}'; fi"
    VERBATIM
  )
endif()

# Add sources to target
target_sources(oscil_plugin PRIVATE ${OSCIL_SOURCES} ${OSCIL_HEADERS})

# JUCE modules used
target_link_libraries(oscil_plugin
  PRIVATE
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
  juce::juce_recommended_lto_flags
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# Conditionally link OpenGL module if enabled
if(OSCIL_ENABLE_OPENGL)
  target_link_libraries(oscil_plugin PRIVATE juce::juce_opengl)
  target_compile_definitions(oscil_plugin PRIVATE OSCIL_ENABLE_OPENGL=1)
  message(STATUS "OpenGL acceleration enabled via JUCE OpenGL module")

  # Debug hooks for testing
  if(OSCIL_DEBUG_HOOKS)
    target_compile_definitions(oscil_plugin PRIVATE OSCIL_DEBUG_HOOKS=1)
    message(STATUS "Debug GPU render hooks enabled")
  endif()
else()
  target_compile_definitions(oscil_plugin PRIVATE OSCIL_ENABLE_OPENGL=0)
  message(STATUS "OpenGL acceleration disabled (CPU-only rendering)")
endif()

# Apply sanitizers selectively (avoid by default on plugin to prevent host scan crashes)
if(OSCIL_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE MATCHES Debug)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT WIN32)
    if(OSCIL_PLUGIN_SANITIZERS)
      target_compile_options(oscil_plugin PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(oscil_plugin PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endif()

# Plugin compile definitions
target_compile_definitions(oscil_plugin
  PRIVATE
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_OGGVORBIS=0
    JUCE_DISABLE_AUDIOPROCESSOR_BEGIN_END_GESTURE_CHECKING=1
  JUCE_VST3_CAN_REPLACE_VST2=0
)

# Strong warnings for our sources (keep JUCE warnings as provided by juce:: targets)
function(oscil_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(OSCIL_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wformat=2)
    if(OSCIL_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

oscil_apply_warnings(oscil_plugin)

# Include directories for our headers
target_include_directories(oscil_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Resources (icons, etc.) - left empty for now
# juce_add_binary_data(oscil_assets SOURCES assets/icon.png)
# target_link_libraries(oscil_plugin PRIVATE oscil_assets)

# Tests
if(BUILD_TESTING)
  include(CTest)
  set(CATCH2_GIT_REPOSITORY "https://github.com/catchorg/Catch2.git" CACHE STRING "Catch2 repo")
  set(CATCH2_GIT_TAG "v3.6.0" CACHE STRING "Catch2 tag")
  FetchContent_Declare(
    catch2
    GIT_REPOSITORY ${CATCH2_GIT_REPOSITORY}
    GIT_TAG        ${CATCH2_GIT_TAG}
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(catch2)

  add_executable(oscil_tests
    tests/test_ringbuffer.cpp
    tests/test_downsampler.cpp
    tests/test_waveform_bridge.cpp
    tests/test_trackstate.cpp
    tests/test_theme_system.cpp
    tests/test_performance_monitor.cpp
    tests/test_performance_optimizations.cpp
    tests/test_64_track_performance.cpp
    tests/test_multitrack_engine.cpp
    tests/test_signal_processor_simple.cpp
    tests/test_timing_engine.cpp
    src/audio/WaveformDataBridge.cpp
    src/audio/MultiTrackEngine.cpp
    src/audio/SignalProcessor.cpp
    src/state/TrackState.cpp
    src/theme/ColorTheme.cpp
    src/theme/ThemeManager.cpp
    src/util/PerformanceMonitor.cpp
    src/render/DecimationProcessor.cpp
    src/render/AdvancedDecimationProcessor.cpp
    src/timing/TimingEngine.cpp
  )
  target_link_libraries(oscil_tests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_core
    juce::juce_data_structures
    juce::juce_graphics
  )
  target_include_directories(oscil_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_features(oscil_tests PRIVATE cxx_std_20)

  if(OSCIL_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE MATCHES Debug)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT WIN32)
      target_compile_options(oscil_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(oscil_tests PRIVATE -fsanitize=address,undefined)
    endif()
  endif()

  oscil_apply_warnings(oscil_tests)

  include(Catch)
  catch_discover_tests(oscil_tests)
endif()

# clang-format targets (format and format-check)
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
  file(GLOB_RECURSE OSCIL_FORMAT_FILES
       ${CMAKE_CURRENT_SOURCE_DIR}/src/*.[ch]pp
       ${CMAKE_CURRENT_SOURCE_DIR}/src/*.[ch]
       ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.[ch]pp
       ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.[ch])
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${OSCIL_FORMAT_FILES}
    COMMENT "Applying clang-format to project sources")
  add_custom_target(format-check
    COMMAND ${CLANG_FORMAT_EXE} --dry-run -Werror ${OSCIL_FORMAT_FILES}
    COMMENT "Checking formatting (clang-format)")
endif()

# Coverage (Linux/Debug) using gcovr
if(OSCIL_ENABLE_COVERAGE AND CMAKE_BUILD_TYPE MATCHES Debug AND UNIX AND NOT APPLE)
  add_compile_options(--coverage)
  add_link_options(--coverage)
  find_program(GCOVR_EXE NAMES gcovr)
  if(GCOVR_EXE)
    add_custom_target(coverage
      COMMAND ${GCOVR_EXE} -r ${CMAKE_CURRENT_SOURCE_DIR} -e ".*_deps/.*" --xml build/coverage.xml --html-details build/coverage.html --print-summary
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      DEPENDS oscil_tests
      COMMENT "Generating coverage report with gcovr")
  endif()
endif()

# macOS: helper target to install VST3 bundle into system plugin folder
if(APPLE)
  set(OSCIL_VST3_SYSTEM_DIR "/Library/Audio/Plug-Ins/VST3" CACHE PATH "System VST3 folder")
  set(OSCIL_VST3_NAME "Oscil.vst3")
  set(OSCIL_VST3_BUILD_DIR "${CMAKE_BINARY_DIR}/oscil_plugin_artefacts/$<CONFIG>/VST3")

  add_custom_target(install-vst3-system
    COMMAND ${CMAKE_COMMAND} -E echo "Installing ${OSCIL_VST3_NAME} from ${OSCIL_VST3_BUILD_DIR} to ${OSCIL_VST3_SYSTEM_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OSCIL_VST3_SYSTEM_DIR}"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${OSCIL_VST3_SYSTEM_DIR}/${OSCIL_VST3_NAME}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${OSCIL_VST3_BUILD_DIR}/${OSCIL_VST3_NAME}"
            "${OSCIL_VST3_SYSTEM_DIR}/${OSCIL_VST3_NAME}"
    DEPENDS oscil_plugin_VST3
    COMMENT "Copying VST3 bundle to system folder (may require sudo if invoked via build)"
  )

  # Ad-hoc signing targets (helps some hosts load unsigned debug builds)
  add_custom_target(sign-vst3
    COMMAND codesign --force --sign - --timestamp=none "${OSCIL_VST3_BUILD_DIR}/${OSCIL_VST3_NAME}"
    DEPENDS oscil_plugin_VST3
    COMMENT "Ad-hoc signing VST3 bundle")

  add_custom_target(sign-au
    COMMAND codesign --force --sign - --timestamp=none "${CMAKE_BINARY_DIR}/oscil_plugin_artefacts/$<CONFIG>/AU/Oscil.component"
    DEPENDS oscil_plugin_AU
    COMMENT "Ad-hoc signing AU component")
endif()
