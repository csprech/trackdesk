cmake_minimum_required(VERSION 3.22)
project(Trackdesk VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
  message(FATAL_ERROR "JUCE not found. Run scripts/bootstrap.sh first.")
endif()
add_subdirectory(external/JUCE)

# The source lists (the console without its window, the engine and panel, the processor for hosts) live in
# trackdesk-module.cmake, so the Tabletop studio compiles exactly what the app does.
include(trackdesk-module.cmake)

set(TRACKDESK_JUCE_DEFS
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1
)

option(TRACKDESK_BUILD_APP "Build the GUI app" ON)

if(TRACKDESK_BUILD_APP)
juce_add_gui_app(Trackdesk
  PRODUCT_NAME "Trackdesk"
  COMPANY_NAME "Trackdesk Research"
  BUNDLE_ID "org.trackdesk.app"
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Trackdesk's input jacks are your audio interface's inputs: it mixes and records them.")

target_sources(Trackdesk PRIVATE
  ${TRACKDESK_MODULE_SOURCES}
  src/ui/MainComponent.cpp
  src/Main.cpp)
target_include_directories(Trackdesk PRIVATE src)
target_compile_definitions(Trackdesk PRIVATE ${TRACKDESK_JUCE_DEFS} JUCE_APPLICATION_NAME_STRING="Trackdesk"
  JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(Trackdesk PRIVATE -Wno-float-equal)
target_link_libraries(Trackdesk PRIVATE
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_audio_devices
  juce::juce_audio_formats
  juce::juce_gui_extra
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------- the GPUI front end's bridge
# A static library with the console (engine, and the JUCE panel used off-screen as controller and renderer) behind
# the C interface in bridge/PanelBridge.h. gpui/build.rs builds and links it.
option(TRACKDESK_BUILD_BRIDGE "Build the bridge for the GPUI front end" OFF)
if(TRACKDESK_BUILD_BRIDGE)
  add_library(trackdesk_bridge STATIC ${TRACKDESK_CORE_SOURCES} ${TRACKDESK_UI_SOURCES} src/ui/MainComponent.cpp bridge/PanelBridge.cpp bridge/TrackdeskAdapter.cpp)
  target_include_directories(trackdesk_bridge PRIVATE src)
  target_compile_definitions(trackdesk_bridge PRIVATE ${TRACKDESK_JUCE_DEFS} JUCE_STANDALONE_APPLICATION=0
    JUCE_APPLICATION_NAME_STRING="Trackdesk" JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
  target_link_libraries(trackdesk_bridge PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags)
endif()

# ---------------------------------------------------------------- tests
juce_add_console_app(TrackdeskTests PRODUCT_NAME "TrackdeskTests")
target_sources(TrackdeskTests PRIVATE
  ${TRACKDESK_CORE_SOURCES}
  tests/TestMain.cpp
  tests/DspTests.cpp
  tests/MixerTests.cpp
  tests/RecorderTests.cpp
  tests/PanelTests.cpp
  tests/CardTests.cpp
  src/engine/CardFolder.cpp)
target_include_directories(TrackdeskTests PRIVATE src)
target_compile_definitions(TrackdeskTests PRIVATE ${TRACKDESK_JUCE_DEFS})
target_compile_options(TrackdeskTests PRIVATE -Wno-float-equal) # (the model's defaulted == compares floats exactly, on purpose)
target_link_libraries(TrackdeskTests PRIVATE
  juce::juce_core
  juce::juce_audio_basics
  juce::juce_audio_formats
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

enable_testing()
add_test(NAME TrackdeskTests COMMAND TrackdeskTests)
