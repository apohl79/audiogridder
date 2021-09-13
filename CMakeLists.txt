cmake_minimum_required(VERSION 3.15)

project(AUDIOGRIDDER VERSION 1.0.0)

file(STRINGS ${CMAKE_SOURCE_DIR}/package/VERSION.num AG_VERSION)

set(CMAKE_CXX_EXTENSIONS OFF)

set(AG_VST2_SDK_DEFAULT "")
set(AG_AAX_PLUGIN_ENABLED FALSE)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(AG_MACOS_TARGET 10.8 CACHE STRING "macOS target, default is 10.8")
  set(AG_VST2_SDK_DEFAULT "$ENV{HOME}/audio/vstsdk2.4")
  set(AG_AAX_SDK_DEFAULT "$ENV{HOME}/audio/aax-sdk-2-4-0-beta")
  set(AG_AAX_SDK ${AG_AAX_SDK_DEFAULT} CACHE STRING "AAX SDK Folder")
  set(CMAKE_OSX_DEPLOYMENT_TARGET ${AG_MACOS_TARGET})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
  set(AG_AAX_PLUGIN_ENABLED TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(AG_VST2_SDK_DEFAULT "z:/vstsdk2.4")
  set(AG_AAX_SDK_DEFAULT "z:/aax-sdk-2-4-0-beta")
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
option(AG_ENABLE_SENTRY "Enable crash reporting via sentry." off)

if(AG_DEPS_ROOT)
  if(NOT FFMPEG_ROOT)
    set(FFMPEG_ROOT ${AG_DEPS_ROOT} CACHE STRING "FFMPEG directory")
  endif()
  if(NOT SENTRY_ROOT)
    set(SENTRY_ROOT ${AG_DEPS_ROOT} CACHE STRING "Sentry directory")
  endif()
  if(NOT BOOST_ROOT)
    set(BOOST_ROOT ${AG_DEPS_ROOT} CACHE STRING "Boost directory")
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CURL_INCLUDE_DIR ${AG_DEPS_ROOT}/include)
    set(CURL_LIBRARY ${AG_DEPS_ROOT}/lib/libcurl.lib )
  endif()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/lib)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if(NOT AG_ENABLE_DYNAMIC_LINKING)
  set(Boost_USE_STATIC_LIBS ON)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(AG_ENABLE_CODE_SIGNING OFF)
endif()

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

if($ENV{AG_ENABLE_SENTRY})
  set(AG_ENABLE_SENTRY on)
  set(AG_SENTRY_DSN $ENV{AG_SENTRY_DSN})
endif()

if(AG_ENABLE_SENTRY)
  set(SENTRY_INCLUDE_DIR ${SENTRY_ROOT}/include)
  message(STATUS "Using Sentry: ${SENTRY_ROOT}")
  message(STATUS "Using Sentry DSN: ${AG_SENTRY_DSN}")
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SENTRY_LIBRARIES
      ${SENTRY_ROOT}/lib/crashpad_compat.lib
      ${SENTRY_ROOT}/lib/crashpad_minidump.lib
      ${SENTRY_ROOT}/lib/crashpad_snapshot.lib
      ${SENTRY_ROOT}/lib/crashpad_util.lib
      ${SENTRY_ROOT}/lib/mini_chromium.lib
      ${SENTRY_ROOT}/lib/crashpad_client.lib
      ${SENTRY_ROOT}/lib/crashpad_zlib.lib
      ${SENTRY_ROOT}/lib/crashpad_tools.lib
      ${SENTRY_ROOT}/lib/crashpad_handler_lib.lib
      ${SENTRY_ROOT}/lib/sentry.lib
      winhttp.lib)
  else()
    find_package(CURL REQUIRED)
    set(SENTRY_LIBRARIES
      ${SENTRY_ROOT}/lib/libcrashpad_client.a
      ${SENTRY_ROOT}/lib/libcrashpad_handler_lib.a
      ${SENTRY_ROOT}/lib/libcrashpad_minidump.a
      ${SENTRY_ROOT}/lib/libcrashpad_snapshot.a
      ${SENTRY_ROOT}/lib/libcrashpad_tools.a
      ${SENTRY_ROOT}/lib/libcrashpad_util.a
      ${SENTRY_ROOT}/lib/libmini_chromium.a
      ${SENTRY_ROOT}/lib/libsentry.a
      ${CURL_LIBRARIES})
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND SENTRY_LIBRARIES "-framework Security" bsm)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(INSERT SENTRY_LIBRARIES 0 "-Wl,--start-group")
    list(APPEND SENTRY_LIBRARIES "-Wl,--end-group")
  endif()

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(AG_SENTRY_CRASHPAD_PATH "${SENTRY_ROOT}/bin/crashpad_handler")
  else()
    set(AG_SENTRY_CRASHPAD_PATH "")
  endif()

  set(AG_SENTRY_ENABLED 1)
else()
  set(AG_SENTRY_ENABLED 0)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(AG_WITH_TRACEREADER on)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(AG_WITH_SERVER off)
endif()

macro(ag_bundle_add_crashpad target_name)
  if(AG_ENABLE_SENTRY)
    add_custom_target(${target_name}_CRASHPAD ALL DEPENDS ${target_name}
      COMMAND
        ${CMAKE_COMMAND} -E echo "Adding crashpad to bundle $<TARGET_BUNDLE_DIR:${target_name}>"
      COMMAND
        cp ${CMAKE_BINARY_DIR}/bin/crashpad_handler $<TARGET_BUNDLE_DIR:${target_name}>/Contents/MacOS
      )
  else()
    add_custom_target(${target_name}_CRASHPAD ALL DEPENDS ${target_name}
      COMMAND
        ${CMAKE_COMMAND} -E echo "Sentry disabled, not adding crashpad to $<TARGET_BUNDLE_DIR:${target_name}>"
      )
  endif()
endmacro()

macro(ag_bundle_sign target_name)
  if(AG_ENABLE_CODE_SIGNING)
    add_custom_target(${target_name}_SIGN ALL DEPENDS ${target_name}_CRASHPAD
      COMMAND
        ${CMAKE_COMMAND} -E echo "Signing bundle $<TARGET_BUNDLE_DIR:${target_name}>"
      COMMAND
        /usr/bin/codesign --force --sign AudioGridder --timestamp=none $<TARGET_BUNDLE_DIR:${target_name}>
      )
  else()
    add_custom_target(${target_name}_SIGN ALL DEPENDS ${target_name}_CRASHPAD
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
          aax_sign.bat $<TARGET_FILE:${target_name}>
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
