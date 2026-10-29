cmake_minimum_required(VERSION 3.22)
project(ElfPro800 VERSION 0.1.0 LANGUAGES C CXX)

# V1 scaffold: JUCE 8 (AGPL-3.0, ADR-002) plugin wrapping the V0 core.
# Build:  cmake -B build-plugin -S plugin -DCMAKE_BUILD_TYPE=Release
#         cmake --build build-plugin
# Produces VST3 + Standalone (AU too on macOS). CLAP comes via
# clap-juce-extensions in a later milestone.

include(FetchContent)
# Tarball of the immutable 8.0.8 tag: one ~40 MB stream instead of a
# full git clone (much faster through this VM's proxy; same content).
FetchContent_Declare(JUCE
  URL https://codeload.github.com/juce-framework/JUCE/tar.gz/refs/tags/8.0.8)
FetchContent_MakeAvailable(JUCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../core ${CMAKE_BINARY_DIR}/core)
# The VST3/AU modules are shared objects: the core's static libs must be
# PIC or ld refuses the link (R_X86_64_PC32 against `settings', found by
# the ubuntu plugin CI job; macOS links it either way).
set_target_properties(p8core p8p600 PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND PLUGIN_FORMATS AU)
endif()

juce_add_plugin(ElfPro800
  VERSION 1.2.0
  COMPANY_NAME            "Garth Elfstrom"
  BUNDLE_ID               com.garthelfstrom.elfpro800
  PLUGIN_MANUFACTURER_CODE Elfc
  PLUGIN_CODE             Ep80
  FORMATS                 ${PLUGIN_FORMATS}
  PRODUCT_NAME            "ElfPro800"
  IS_SYNTH                TRUE
  NEEDS_MIDI_INPUT        TRUE
  NEEDS_MIDI_OUTPUT       FALSE
  IS_MIDI_EFFECT          FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(ElfPro800)

target_sources(ElfPro800 PRIVATE
  source/PluginProcessor.cpp
  source/PluginEditor.cpp)

target_compile_definitions(ElfPro800 PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ElfPro800 PRIVATE
  p8core
  juce::juce_audio_utils
  juce::juce_dsp
  PUBLIC
  juce::juce_recommended_config_flags
  # NOTE: no juce_recommended_lto_flags - the LTO link of the VST3 module
  # was OOM-killed on the 8 GB dev VM (2026-10-28) and slows CI; a
  # scaffold does not need it. Revisit for release builds.
  juce::juce_recommended_warning_flags)

# Headless harness: exercises the processor + full parameter wiring in CI.
juce_add_console_app(ElfPro800Harness PRODUCT_NAME "ElfPro800Harness")
juce_generate_juce_header(ElfPro800Harness)
target_sources(ElfPro800Harness PRIVATE
  source/PluginProcessor.cpp
  source/PluginEditor.cpp
  test/Harness.cpp)
target_compile_definitions(ElfPro800Harness PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
  # This VM (and minimal CI images) have the X11 runtime libs but not all
  # -dev headers; the plugin target auto-detects that, the console app
  # does not, so state it explicitly.
  JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
target_link_libraries(ElfPro800Harness PRIVATE
  p8core juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
enable_testing()
add_test(NAME harness COMMAND ElfPro800Harness)
