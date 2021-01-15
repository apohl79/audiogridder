cmake_minimum_required(VERSION 3.15)

project(AUDIOGRIDDER_PLUGIN VERSION 1.0.0)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/Source AG_SOURCES_PLUGIN)

macro(ag_add_plugin type)
  cmake_parse_arguments(AG "" "PLUGIN_CODE;IS_SYNTH;IS_MIDI_EFFECT;VST3_CATEGORY;VST_CATEGORY;AU_TYPE" "" ${ARGN})

  set(pluginname "AGridder")
  set(productname "AudioGridder")
  if(NOT ${type} STREQUAL "Fx")
    string(TOLOWER "${type}" type_LC)
    set(pluginname "${pluginname} (${type_LC})")
    set(productname "${productname}${type}")
  endif()

  juce_add_plugin(AudioGridder${type}
    VERSION "${AG_VERSION}"
    PLUGIN_NAME "${pluginname}"
    PRODUCT_NAME "${productname}"
    COMPANY_NAME "e47"
    COMPANY_COPYRIGHT "2020 Andreas Pohl"
    COMPANY_WEBSITE "https://www.audiogridder.com"
    DESCRIPTION "DSP Server Plugin"
    PLUGIN_MANUFACTURER_CODE XE47
    PLUGIN_CODE ${AG_PLUGIN_CODE}
    FORMATS AU VST3 VST
    IS_SYNTH ${AG_IS_SYNTH}
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT ${AG_IS_MIDI_EFFECT}
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST_NUM_MIDI_INS 16
    VST_NUM_MIDI_OUTS 16
    VST2_CATEGORY ${AG_VST_CATEGORY}
    VST3_CATEGORIES Network ${AG_VST3_CATEGORY}
    AU_MAIN_TYPE ${AG_AU_TYPE}
    COPY_PLUGIN_AFTER_BUILD FALSE)

  juce_generate_juce_header(AudioGridder${type})

  target_sources(AudioGridder${type} PRIVATE ${AG_SOURCES_PLUGIN} ${AG_SOURCES_COMMON})

  target_compile_definitions(AudioGridder${type} PRIVATE
    AG_PLUGIN
    JUCE_VST3_CAN_REPLACE_VST2=0
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
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    ag_bundle_sign(AudioGridder${type}_AU)
    ag_bundle_sign(AudioGridder${type}_VST)
    ag_bundle_sign(AudioGridder${type}_VST3)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      get_target_property(AG_AU_DEST AudioGridder${type} JUCE_AU_COPY_DIR)
      get_target_property(AG_VST_DEST AudioGridder${type} JUCE_VST_COPY_DIR)
      get_target_property(AG_VST3_DEST AudioGridder${type} JUCE_VST3_COPY_DIR)
      ag_bundle_copy(AudioGridder${type}_AU ${AG_AU_DEST})
      ag_bundle_copy(AudioGridder${type}_VST ${AG_VST_DEST})
      ag_bundle_copy(AudioGridder${type}_VST3 ${AG_VST3_DEST})
    endif()
  endif()
endmacro()

ag_add_plugin(Fx
  PLUGIN_CODE E401
  IS_SYNTH FALSE
  IS_MIDI_EFFECT FALSE
  VST3_CATEGORY Fx
  VST_CATEGORY kPlugCategEffect
  AU_TYPE kAudioUnitType_Effect)

ag_add_plugin(Inst
  PLUGIN_CODE E402
  IS_SYNTH TRUE
  IS_MIDI_EFFECT FALSE
  VST3_CATEGORY Instrument
  VST_CATEGORY kPlugCategSynth
  AU_TYPE kAudioUnitType_MusicDevice)

ag_add_plugin(Midi
  PLUGIN_CODE E403
  IS_SYNTH FALSE
  IS_MIDI_EFFECT TRUE
  VST3_CATEGORY Fx
  VST_CATEGORY kPlugCategEffect
  AU_TYPE kAudioUnitType_MIDIProcessor)
