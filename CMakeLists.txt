cmake_minimum_required(VERSION 3.22)
project(Iro_x VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  # Universal (Apple silicon + Intel) is set by the CI workflow; macOS 11+.
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

# Milestone 0: an empty effect. The Iroi firmware source is in third_party/iroi-firmware (unmodified) and is not
# compiled yet; Milestone 1 adds the engine.

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
      juce::juce_audio_utils
      juce::juce_recommended_config_flags)
  endif()
endif()
