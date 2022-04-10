cmake_minimum_required(VERSION 3.15)

project(AUDIOGRIDDER_TESTS VERSION 1.0.0)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/Source AG_SOURCES_TESTS)
aux_source_directory(${CMAKE_SOURCE_DIR}/Common/Source AG_SOURCES_COMMON)
aux_source_directory(${CMAKE_SOURCE_DIR}/Server/Source AG_SOURCES_SERVER)
list(REMOVE_ITEM AG_SOURCES_COMMON "${CMAKE_SOURCE_DIR}/Common/Source/TraceReader.cpp")

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/Tests/Source)
include_directories(${CMAKE_SOURCE_DIR}/Server/Source)
#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Source/Server)

macro(ag_add_testrunner type)
  set(BINNAME "testrunner-${type}")
  message(STATUS "Target ${BINNAME} enabled.")
  juce_add_console_app(${BINNAME} PLUGINHOST_AU TRUE)
  juce_generate_juce_header(${BINNAME})

  target_sources(${BINNAME} PRIVATE
    ${AG_SOURCES_TESTS}
    ${AG_SOURCES_COMMON}
    ${AG_SOURCES_SERVER})

  target_compile_features(${BINNAME} PRIVATE cxx_std_14)

  target_compile_definitions(${BINNAME}
    PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_OGGVORBIS=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    PLUGIN_DIR="${CMAKE_BINARY_DIR}/lib"
    AG_VERSION="${AG_VERSION}"
    AG_UNIT_TESTS
    AG_TESTS_DATA="${CMAKE_SOURCE_DIR}/TestsData"
    BOOST_ALL_NO_LIB)

  if(${type} STREQUAL "server")
    target_compile_definitions(${BINNAME} PRIVATE
      AG_UNIT_TEST_SERVER
      AG_SERVER
      JUCE_PLUGINHOST_VST3=1
      JUCE_PLUGINHOST_VST=1)
  endif()

  set(LINK_LIBRARIES
    ${FFMPEG_LIBRARIES}
    ${WEBP_LIBRARIES}
    juce::juce_core
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_events
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    juce::juce_recommended_lto_flags)

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND LINK_LIBRARIES "-framework AVFoundation -framework CoreMedia")
    if(AG_MACOS_TARGET STRGREATER_EQUAL 10.8)
      list(APPEND LINK_LIBRARIES "-framework VideoToolbox")
    endif()
  endif()

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(INSERT LINK_LIBRARIES 0 "-Wl,--start-group")
    list(APPEND LINK_LIBRARIES "-Wl,--end-group")
  endif()

  target_link_libraries(${BINNAME}
    PRIVATE
    ${LINK_LIBRARIES})

endmacro()

ag_add_testrunner(server)
