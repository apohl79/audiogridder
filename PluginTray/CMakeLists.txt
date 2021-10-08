cmake_minimum_required(VERSION 3.15)

project(AUDIOGRIDDER_PLUGIN_TRAY VERSION 1.0.0)

set(AG_ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png")
set(AG_ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(AG_ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon64.png")
  set(AG_ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon16.png")
endif()

juce_add_gui_app(AudioGridderPluginTray
  VERSION "${AG_VERSION}"
  PRODUCT_NAME "AudioGridderPluginTray"
  COMPANY_NAME "e47"
  COMPANY_COPYRIGHT "2020 Andreas Pohl"
  COMPANY_WEBSITE "https://www.audiogridder.com"
  PLUGINHOST_AU TRUE
  ICON_BIG "${AG_ICON_BIG}"
  ICON_SMALL "${AG_ICON_SMALL}")

juce_generate_juce_header(AudioGridderPluginTray)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/Source AG_SOURCES_PLUGIN_TRAY)

target_sources(AudioGridderPluginTray PRIVATE ${AG_SOURCES_PLUGIN_TRAY} ${AG_SOURCES_COMMON})

target_compile_definitions(AudioGridderPluginTray PRIVATE
  AG_PLUGIN_TRAY
  AG_SENTRY_ENABLED=${AG_SENTRY_ENABLED}
  AG_SENTRY_DSN="${AG_SENTRY_DSN}"
  AG_SENTRY_CRASHPAD_PATH="${AG_SENTRY_CRASHPAD_PATH}"
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_MODAL_LOOPS_PERMITTED=1
  JUCE_DISABLE_AUDIOPROCESSOR_BEGIN_END_GESTURE_CHECKING
  JUCE_DISABLE_ASSERTIONS
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:AudioGridderPluginTray,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:AudioGridderPluginTray,JUCE_VERSION>")

target_compile_features(AudioGridderPluginTray PRIVATE cxx_std_14)

juce_add_binary_data(AudioGridderPluginTrayData SOURCES
  Resources/icon.png
  Resources/icon16.png
  Resources/icon64.png)

target_link_libraries(AudioGridderPluginTray PRIVATE
  AudioGridderPluginTrayData
  juce::juce_graphics
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
  ${SENTRY_LIBRARIES})

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  ag_bundle_add_crashpad(AudioGridderPluginTray)
  ag_bundle_sign(AudioGridderPluginTray)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND AG_ENABLE_DEBUG_COPY_STEP)
    ag_bundle_copy(AudioGridderPluginTray /Applications/Debug)
  endif()
endif()
