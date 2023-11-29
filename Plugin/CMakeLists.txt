cmake_minimum_required(VERSION 3.15)

project(AUDIOGRIDDER_PLUGIN VERSION 1.0.0)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/Source AG_SOURCES_PLUGIN)

macro(ag_add_plugin type)
  cmake_parse_arguments(AG "" "PLUGIN_CODE;IS_SYNTH;IS_MIDI_EFFECT;VST3_CATEGORY;VST_CATEGORY;AAX_CATEGORY;AU_TYPE" "" ${ARGN})

  set(pluginname "AGridder")
  set(productname "AudioGridder")
  if(NOT ${type} STREQUAL "Fx")
    string(TOLOWER "${type}" type_LC)
    set(pluginname "${pluginname} (${type_LC})")
    set(productname "${productname}${type}")
  endif()

  set(AG_FORMATS AU VST3)

  if(AG_VST2_PLUGIN_ENABLED)
    list(APPEND AG_FORMATS VST)
  endif()

  if(AG_AAX_PLUGIN_ENABLED)
    list(APPEND AG_FORMATS AAX)
  endif()

  juce_add_plugin(AudioGridder${type}
    VERSION "${AG_VERSION}"
    PLUGIN_NAME "${pluginname}"
    PRODUCT_NAME "${productname}"
    COMPANY_NAME "e47"
    COMPANY_COPYRIGHT "2020-2022 Andreas Pohl"
    COMPANY_WEBSITE "https://www.audiogridder.com"
    DESCRIPTION "${pluginname}"
    PLUGIN_MANUFACTURER_CODE XE47
    PLUGIN_CODE ${AG_PLUGIN_CODE}
    FORMATS ${AG_FORMATS}
    IS_SYNTH ${AG_IS_SYNTH}
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT ${AG_IS_MIDI_EFFECT}
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST_NUM_MIDI_INS 16
    VST_NUM_MIDI_OUTS 16
    VST2_CATEGORY ${AG_VST_CATEGORY}
    VST3_CATEGORIES Network ${AG_VST3_CATEGORY}
    AAX_CATEGORY ${AG_AAX_CATEGORY}
    AU_MAIN_TYPE ${AG_AU_TYPE}
    AU_SANDBOX_SAFE TRUE
    SUPPRESS_AU_PLIST_RESOURCE_USAGE TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

  juce_generate_juce_header(AudioGridder${type})

  target_sources(AudioGridder${type} PRIVATE ${AG_SOURCES_PLUGIN} ${AG_SOURCES_COMMON})

  target_compile_definitions(AudioGridder${type} PRIVATE
    AG_PLUGIN
    AG_SENTRY_ENABLED=${AG_SENTRY_ENABLED}
    AG_SENTRY_DSN="${AG_SENTRY_DSN}"
    AG_SENTRY_CRASHPAD_PATH="${AG_SENTRY_CRASHPAD_PATH}"
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISABLE_AUDIOPROCESSOR_BEGIN_END_GESTURE_CHECKING
    JUCE_DISABLE_ASSERTIONS)

  target_compile_features(AudioGridder${type} PRIVATE cxx_std_14)

  target_link_libraries(AudioGridder${type}
    PRIVATE
    juce::juce_audio_plugin_client
    juce::juce_audio_utils
    juce::juce_graphics
    juce::juce_gui_extra
    ${FFMPEG_LIBRARIES}
    ${WEBP_LIBRARIES}
    ${SENTRY_LIBRARIES}
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_link_libraries(AudioGridder${type}
            PRIVATE
            "-framework CoreMedia"
            "-framework VideoToolbox"
    )

    ag_strip(AudioGridder${type}_AU AU)
  endif()

  if(AG_VST2_PLUGIN_ENABLED)
    ag_strip(AudioGridder${type}_VST VST)
  endif()

  ag_strip(AudioGridder${type}_VST3 VST3)

  if(AG_AAX_PLUGIN_ENABLED)
    ag_strip(AudioGridder${type}_AAX AAX)
    ag_bundle_sign_aax(AudioGridder${type}_AAX)
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    ag_bundle_add_crashpad(AudioGridder${type}_AU)
    ag_bundle_add_crashpad(AudioGridder${type}_VST3)
    ag_bundle_sign(AudioGridder${type}_AU)
    ag_bundle_sign(AudioGridder${type}_VST3)

    if(AG_VST2_PLUGIN_ENABLED)
      ag_bundle_add_crashpad(AudioGridder${type}_VST)
      ag_bundle_sign(AudioGridder${type}_VST)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND AG_ENABLE_DEBUG_COPY_STEP)
      get_target_property(AG_AU_DEST AudioGridder${type} JUCE_AU_COPY_DIR)
      get_target_property(AG_VST3_DEST AudioGridder${type} JUCE_VST3_COPY_DIR)
      get_target_property(AG_AAX_DEST AudioGridder${type} JUCE_AAX_COPY_DIR)
      ag_bundle_copy(AudioGridder${type}_AU ${AG_AU_DEST})
      ag_bundle_copy(AudioGridder${type}_VST3 ${AG_VST3_DEST})

      if(AG_VST2_PLUGIN_ENABLED)
        get_target_property(AG_VST_DEST AudioGridder${type} JUCE_VST_COPY_DIR)
        ag_bundle_copy(AudioGridder${type}_VST ${AG_VST_DEST})
      endif()

      if(AG_AAX_PLUGIN_ENABLED)
        ag_bundle_copy(AudioGridder${type}_AAX ${AG_AAX_DEST})
      endif()
    endif()
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(AudioGridder${type}_VST3 PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib/VST3
      LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib/VST3
      ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/lib/VST3
      LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/lib/VST3)

    if(AG_VST2_PLUGIN_ENABLED)
      set_target_properties(AudioGridder${type}_VST PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib/VST
        LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib/VST
        ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/lib/VST
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/lib/VST)
    endif()

    if(AG_AAX_PLUGIN_ENABLED)
      set_target_properties(AudioGridder${type}_AAX PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib/AAX
        LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib/AAX
        ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/lib/AAX
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/lib/AAX)
    endif()
  endif()
endmacro()

ag_add_plugin(Fx
  PLUGIN_CODE E401
  IS_SYNTH FALSE
  IS_MIDI_EFFECT FALSE
  VST3_CATEGORY Fx
  VST_CATEGORY kPlugCategEffect
  AAX_CATEGORY AAX_EPlugInCategory_Effect
  AU_TYPE kAudioUnitType_Effect)

ag_add_plugin(Inst
  PLUGIN_CODE E402
  IS_SYNTH TRUE
  IS_MIDI_EFFECT FALSE
  VST3_CATEGORY Instrument
  VST_CATEGORY kPlugCategSynth
  AAX_CATEGORY AAX_ePlugInCategory_SWGenerators
  AU_TYPE kAudioUnitType_MusicDevice)

ag_add_plugin(Midi
  PLUGIN_CODE E403
  IS_SYNTH FALSE
  IS_MIDI_EFFECT TRUE
  VST3_CATEGORY Fx
  VST_CATEGORY kPlugCategEffect
  AAX_CATEGORY AAX_ePlugInCategory_None
  AU_TYPE kAudioUnitType_MIDIProcessor)
