# Trackdesk as a module for hosts (the Tabletop studio): the sources to compile into the host's own target,
# which brings JUCE (juce_audio_utils, juce_audio_processors, juce_audio_formats, juce_gui_extra). The app and
# the tests use the same lists.
set(TRACKDESK_DIR ${CMAKE_CURRENT_LIST_DIR})
set(TRACKDESK_CORE_SOURCES
  ${TRACKDESK_DIR}/src/core/Model.cpp
  ${TRACKDESK_DIR}/src/core/Dsp.cpp
  ${TRACKDESK_DIR}/src/core/Recorder.cpp
  ${TRACKDESK_DIR}/src/core/Mixer.cpp
  ${TRACKDESK_DIR}/src/core/Lcd.cpp
  ${TRACKDESK_DIR}/src/core/Desk.cpp
  ${TRACKDESK_DIR}/src/core/DeskMenu.cpp)
set(TRACKDESK_UI_SOURCES
  ${TRACKDESK_DIR}/src/engine/CardFolder.cpp
  ${TRACKDESK_DIR}/src/engine/Engine.cpp
  ${TRACKDESK_DIR}/src/ui/Materials.cpp
  ${TRACKDESK_DIR}/src/ui/Widgets.cpp
  ${TRACKDESK_DIR}/src/ui/Panel.cpp)
set(TRACKDESK_MODULE_SOURCES ${TRACKDESK_CORE_SOURCES} ${TRACKDESK_UI_SOURCES} ${TRACKDESK_DIR}/src/plugin/TrackdeskProcessor.cpp)
# (switches over enums use `default:` on purpose; JUCE's warning set flags every one)
set_source_files_properties(${TRACKDESK_CORE_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wno-switch-enum;-Wno-float-equal")
