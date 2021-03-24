cmake_minimum_required(VERSION 3.15)

project(AUDIOGRIDDER VERSION 1.0.0)

file(STRINGS ${CMAKE_SOURCE_DIR}/package/VERSION.num AG_VERSION)

set(CMAKE_CXX_EXTENSIONS OFF)

set(AG_VST2_SDK_DEFAULT "")
set(AG_AAX_PLUGIN_ENABLED FALSE)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(AG_MACOS_TARGET 10.8 CACHE STRING "macOS target, default is 10.8")
  set(AG_VST2_SDK_DEFAULT "$ENV{HOME}/audio/vstsdk2.4")
  set(AG_AAX_SDK_DEFAULT "$ENV{HOME}/audio/AAX_SDK_2p3p2")
  set(AG_AAX_SDK ${AG_AAX_SDK_DEFAULT} CACHE STRING "AAX SDK Folder")
  set(CMAKE_OSX_DEPLOYMENT_TARGET ${AG_MACOS_TARGET})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
  # No Pro Tools/AAX support for arm64 yet
  if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
    set(AG_AAX_PLUGIN_ENABLED TRUE)
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(AG_VST2_SDK_DEFAULT "z:/vstsdk2.4")
  set(AG_AAX_SDK_DEFAULT "z:/AAX_SDK_2p3p2")
  set(AG_AAX_SDK ${AG_AAX_SDK_DEFAULT} CACHE STRING "AAX SDK Folder")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set(AG_AAX_PLUGIN_ENABLED TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(AG_VST2_SDK_DEFAULT "/mnt/audio/vstsdk2.4")
endif()

set(AG_VST2_SDK ${AG_VST2_SDK_DEFAULT} CACHE STRING "VST2 SDK Folder")

add_subdirectory(JUCE)
juce_set_vst2_sdk_path(${AG_VST2_SDK})
if(AG_AAX_PLUGIN_ENABLED)
  juce_set_aax_sdk_path(${AG_AAX_SDK})
endif()

option(AG_WITH_PLUGIN "Enable Plugin build." on)
option(AG_WITH_SERVER "Enable Server build." on)
option(AG_WITH_TRACEREADER "Enable tracereader build." off)
option(AG_ENABLE_DYNAMIC_LINKING "Enable dynamic linking of ffmpeg/webp." off)
option(AG_ENABLE_CODE_SIGNING "Enable code signing." on)
option(AG_ENABLE_DEBUG_COPY_STEP "Enable copying binaries after building in Debug mode (on macOS)." on)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

find_package(FFmpeg REQUIRED)
find_package(WebP REQUIRED)
find_package(Boost 1.70.0 REQUIRED COMPONENTS program_options)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Common/Source)
include_directories(${FFMPEG_INCLUDE_DIRS})
include_directories(${WEBP_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

link_directories(${FFMPEG_LIBRARY_DIRS})
link_directories(${WEBP_LIBRARY_DIRS})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/Common/Source AG_SOURCES_COMMON)
list(REMOVE_ITEM AG_SOURCES_COMMON "${CMAKE_CURRENT_SOURCE_DIR}/Common/Source/TraceReader.cpp")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(AG_WITH_TRACEREADER on)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(AG_WITH_SERVER off)
endif()

macro(ag_bundle_sign target_name)
  if(AG_ENABLE_CODE_SIGNING)
    add_custom_target(${target_name}_SIGN ALL DEPENDS ${target_name}
      COMMAND
        ${CMAKE_COMMAND} -E echo "Signing bundle $<TARGET_BUNDLE_DIR:${target_name}>"
      COMMAND
        /usr/bin/codesign --force --sign AudioGridder --timestamp=none $<TARGET_BUNDLE_DIR:${target_name}>
      )
  else()
    add_custom_target(${target_name}_SIGN ALL DEPENDS ${target_name}
      COMMAND
        ${CMAKE_COMMAND} -E echo "Code signing disabled. Not signing bundle $<TARGET_BUNDLE_DIR:${target_name}>"
      )
  endif()
endmacro()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  macro(ag_bundle_sign_aax target_name)
    if(AG_ENABLE_CODE_SIGNING)
      add_custom_target(${target_name}_SIGN ALL DEPENDS ${target_name}
        COMMAND
          ${CMAKE_COMMAND} -E echo "Signing AAX bundle $<TARGET_BUNDLE_DIR:${target_name}>"
        COMMAND
          aax_sign.sh $<TARGET_BUNDLE_DIR:${target_name}> || echo "AAX signing not available"
      )
    else()
      add_custom_target(${target_name}_SIGN ALL DEPENDS ${target_name}
        COMMAND
          ${CMAKE_COMMAND} -E echo "Code signing disabled. Not signing AAX bundle $<TARGET_BUNDLE_DIR:${target_name}>"
      )
    endif()
  endmacro()
else()
  macro(ag_bundle_sign_aax target_name)
    if(AG_ENABLE_CODE_SIGNING)
      add_custom_target(${target_name}_SIGN ALL DEPENDS ${target_name}
        COMMAND
          ${CMAKE_COMMAND} -E echo "Signing AAX plugin $<TARGET_FILE_NAME:${target_name}>"
        COMMAND
          aax_sign.bat $<TARGET_FILE_DIR:${target_name}>../../..
      )
    else()
      add_custom_target(${target_name}_SIGN ALL DEPENDS ${target_name}
        COMMAND
          ${CMAKE_COMMAND} -E echo "Code signing disabled. Not signing AAX plugin $<TARGET_FILE_NAME:${target_name}>"
      )
    endif()
  endmacro()
endif()

macro(ag_bundle_copy target_name dst_dir)
  add_custom_target(${target_name}_COPY ALL DEPENDS ${target_name}_SIGN
    COMMAND
      ${CMAKE_COMMAND} -E echo "Copying bundle $<TARGET_BUNDLE_DIR:${target_name}> to ${dst_dir}"
    COMMAND
      rm -rf ${dst_dir}/`basename $<TARGET_BUNDLE_DIR:${target_name}>`
    COMMAND
      cp -a $<TARGET_BUNDLE_DIR:${target_name}> ${dst_dir}
  )
endmacro()

macro(ag_file_copy target_name dst_dir)
  add_custom_target(${target_name}_COPY ALL DEPENDS ${target_name}
    COMMAND
      ${CMAKE_COMMAND} -E echo "Copying file $<TARGET_FILE:${target_name}> to ${dst_dir}"
    COMMAND
      ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target_name}> ${dst_dir}
  )
endmacro()

if(AG_WITH_PLUGIN)
  message(STATUS "Plugin enabled. Use -DAG_WITH_PLUGIN=OFF to deactivate building the plugins.")
  add_subdirectory(Plugin)
  add_subdirectory(PluginTray)
else()
  message(STATUS "Plugin disabled.")
endif()

if(AG_WITH_SERVER)
  message(STATUS "Server enabled. Use -DAG_WITH_SERVER=OFF to deactivate building the server.")
  add_subdirectory(Server)
else()
  message(STATUS "Server disabled.")
endif()

message(STATUS "Dynamic linking: ${AG_ENABLE_DYNAMIC_LINKING}")
message(STATUS "Code signing: ${AG_ENABLE_CODE_SIGNING}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND AG_ENABLE_DEBUG_COPY_STEP)
  message(STATUS "Copy step of binaries after building is enabled.")
endif()


if(AG_WITH_TRACEREADER)
  message(STATUS "Tracereader enabled.")
  add_executable(tracereader "${CMAKE_CURRENT_SOURCE_DIR}/Common/Source/TraceReader.cpp")
  target_compile_features(tracereader PRIVATE cxx_std_14)
  target_link_libraries(tracereader ${Boost_LIBRARIES})
  if(AG_ENABLE_DEBUG_COPY_STEP)
    ag_file_copy(tracereader $ENV{HOME}/bin)
  endif()
endif()
