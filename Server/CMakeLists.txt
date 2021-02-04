cmake_minimum_required(VERSION 3.15)

project(AUDIOGRIDDER_SERVER VERSION 1.0.0)

set(AG_ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png")
set(AG_ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(AG_ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon64.png")
  set(AG_ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon16.png")
endif()

juce_add_gui_app(AudioGridderServer
  VERSION "${AG_VERSION}"
  PRODUCT_NAME "AudioGridderServer"
  COMPANY_NAME "e47"
  COMPANY_COPYRIGHT "2020 Andreas Pohl"
  COMPANY_WEBSITE "https://www.audiogridder.com"
  PLUGINHOST_AU TRUE
  ICON_BIG "${AG_ICON_BIG}"
  ICON_SMALL "${AG_ICON_SMALL}")

juce_generate_juce_header(AudioGridderServer)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/Source AG_SOURCES_SERVER)

target_sources(AudioGridderServer PRIVATE ${AG_SOURCES_SERVER} ${AG_SOURCES_COMMON})

target_compile_definitions(AudioGridderServer PRIVATE
  AG_SERVER
  JUCE_PLUGINHOST_VST3=1
  JUCE_PLUGINHOST_VST=1
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_DISABLE_AUDIOPROCESSOR_BEGIN_END_GESTURE_CHECKING
  JUCE_DISABLE_ASSERTIONS
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:AudioGridderServer,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:AudioGridderServer,JUCE_VERSION>")

target_compile_features(AudioGridderServer PRIVATE cxx_std_14)

juce_add_binary_data(AudioGridderServerData SOURCES
  Resources/icon.png
  Resources/icon16.png
  Resources/icon64.png)

target_link_libraries(AudioGridderServer PRIVATE
  AudioGridderServerData
  juce::juce_audio_basics
  juce::juce_audio_processors
  juce::juce_audio_formats
  juce::juce_graphics
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
  ${FFMPEG_LIBRARIES}
  ${WEBP_LIBRARIES})

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_link_libraries(AudioGridderServer PRIVATE "-framework AVFoundation -framework CoreMedia")
  if(AG_MACOS_TARGET STRGREATER_EQUAL 10.8)
    target_link_libraries(AudioGridderServer PRIVATE "-framework VideoToolbox")
  endif()
  ag_bundle_sign(AudioGridderServer)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    ag_bundle_copy(AudioGridderServer /Applications/Debug)
  endif()
endif()
