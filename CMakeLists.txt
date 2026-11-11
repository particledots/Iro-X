cmake_minimum_required(VERSION 3.22)
project(Iro_x VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  # Universal (Apple silicon + Intel) is set by the CI workflow; macOS 11+.
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

# The Iroi effects engine: the firmware copy (third_party/iroi-firmware), the OwlProgram library files it is written
# against (third_party/owlprogram) and Iro-x's own stand-ins for what OpenWare provides on the hardware (src/engine).
# No JUCE in here, so it builds and tests on any machine (tools/engine_test).
add_library(irox_engine STATIC
  src/engine/IroxEngine.cpp
  src/engine/IroxStream.cpp
  src/engine/shim/owl_runtime.cpp
  third_party/owlprogram/FloatArray.cpp
  third_party/owlprogram/ComplexFloatArray.cpp
  third_party/owlprogram/SimpleValue.cpp
  third_party/owlprogram/basicmaths.c
  third_party/owlprogram/fastlog.c
  third_party/owlprogram/fastpow.c)
target_include_directories(irox_engine
  PUBLIC src/engine
  PRIVATE src/engine/shim third_party/owlprogram third_party/iroi-firmware)
set_target_properties(irox_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
# OwlProgram's random number function is global; Iro-x provides one per thread instead (src/engine/shim/owl_runtime.cpp).
set_source_files_properties(third_party/owlprogram/basicmaths.c PROPERTIES COMPILE_DEFINITIONS "randf=owlprogram_unused_randf")
# fast_powf(0, y) turns -infinity into an integer (undefined in C, the same result as on the module's ARM chip in practice).
# The sanitizer builds of the engine test would stop on it, so that one check is off for that one file.
set_source_files_properties(third_party/owlprogram/fastpow.c PROPERTIES COMPILE_OPTIONS "-fno-sanitize=float-cast-overflow")
# Third-party code: no warnings from our flags (the firmware has some harmless ones).
target_compile_options(irox_engine PRIVATE -w -fvisibility=hidden -fvisibility-inlines-hidden)

# Engine test (not part of the download): runs test signals through every effect without a host and writes WAV files.
option(IROX_BUILD_ENGINE_TEST "Build the engine test program" ON)
if(IROX_BUILD_ENGINE_TEST)
  add_executable(irox_engine_test tools/engine_test/engine_test.cpp)
  target_link_libraries(irox_engine_test PRIVATE irox_engine)
  target_compile_options(irox_engine_test PRIVATE -O2)
endif()

option(IROX_BUILD_PLUGIN "Build the AU / VST3 plugin (downloads JUCE)" ON)
if(IROX_BUILD_PLUGIN)
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)

  # Plugin formats: AU (macOS only, the priority) + VST3.
  set(FORMATS VST3)
  if(APPLE)
    list(APPEND FORMATS AU)
  endif()
  # For trying the window without a host (not part of the download): -DIROX_EXTRA_FORMATS=Standalone
  set(IROX_EXTRA_FORMATS "" CACHE STRING "Extra plugin formats for local testing")
  list(APPEND FORMATS ${IROX_EXTRA_FORMATS})

  # AU effect: JUCE sets the Audio Unit type to kAudioUnitType_Effect ("aufx") when IS_SYNTH is FALSE and the
  # plugin does not take MIDI. It then shows up under Audio FX in Logic.
  juce_add_plugin(Iro_x
    COMPANY_NAME "particledots"
    PLUGIN_MANUFACTURER_CODE Ptdt
    PLUGIN_CODE Irx1
    FORMATS ${FORMATS}
    PRODUCT_NAME "Iro-x"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

  target_sources(Iro_x PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

  target_include_directories(Iro_x PRIVATE src)

  target_compile_definitions(Iro_x PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    IROX_VERSION_STRING="${PROJECT_VERSION}")

  target_link_libraries(Iro_x PRIVATE
    irox_engine
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

  # Plugin test (not part of the download): runs the plugin code without a host. Off unless asked for:
  #   cmake -S . -B build -DIROX_BUILD_PLUGIN_TEST=ON && cmake --build build --target Iro_x_PluginTest
  option(IROX_BUILD_PLUGIN_TEST "Build the plugin test program" OFF)
  if(IROX_BUILD_PLUGIN_TEST)
    juce_add_console_app(Iro_x_PluginTest)
    target_sources(Iro_x_PluginTest PRIVATE
      tools/plugin_test/plugin_test.cpp
      src/PluginProcessor.cpp
      src/PluginEditor.cpp)
    target_include_directories(Iro_x_PluginTest PRIVATE src)
    target_compile_definitions(Iro_x_PluginTest PRIVATE
      JUCE_WEB_BROWSER=0
      JUCE_USE_CURL=0
      IROX_VERSION_STRING="${PROJECT_VERSION}")
    target_link_libraries(Iro_x_PluginTest PRIVATE
      irox_engine
      juce::juce_audio_utils
      juce::juce_recommended_config_flags)
  endif()
endif()
