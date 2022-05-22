cmake_minimum_required(VERSION 3.15)

project(AUDIOGRIDDER VERSION 1.0.0)

option(AG_WITH_PLUGIN "Enable Plugin build." on)
option(AG_WITH_SERVER "Enable Server build." on)
option(AG_WITH_TRACEREADER "Enable tracereader build." off)
option(AG_WITH_TESTS "Enable unit tests." off)
option(AG_ENABLE_DYNAMIC_LINKING "Enable dynamic linking of ffmpeg/webp." off)
option(AG_ENABLE_CODE_SIGNING "Enable code signing." on)
option(AG_ENABLE_DEBUG_COPY_STEP "Enable copying binaries after building in Debug mode (on macOS)." on)
option(AG_ENABLE_SENTRY "Enable crash reporting via sentry." off)
option(AG_ENABLE_ASAN "Enable AddressSanizizer." off)
option(AG_VST2_PLUGIN_ENABLED "Enable the VST2 plugins." off)
option(AG_AAX_PLUGIN_ENABLED "Enable the AAX plugins." off)

file(STRINGS ${CMAKE_SOURCE_DIR}/package/VERSION.num AG_VERSION)

set(CMAKE_CXX_EXTENSIONS OFF)

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
  set(Boost_USE_STATIC_RUNTIME ON)
  set(Boost_USE_STATIC_LIBS ON)
  set(Boost_NO_SYSTEM_PATHS ON)
endif()

set(AG_VST2_SDK_DEFAULT "${AG_SDKS_ROOT}/vstsdk2.4")
set(AG_VST2_SDK ${AG_VST2_SDK_DEFAULT} CACHE STRING "VST2 SDK Folder")
set(AG_AAX_SDK_DEFAULT "${AG_SDKS_ROOT}/aax-sdk")
set(AG_AAX_SDK ${AG_AAX_SDK_DEFAULT} CACHE STRING "AAX SDK Folder")

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(AG_MACOS_TARGET 10.8 CACHE STRING "macOS target, default is 10.8")
  set(CMAKE_OSX_DEPLOYMENT_TARGET ${AG_MACOS_TARGET})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(AG_WITH_TRACEREADER on)
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_definitions(_WIN32_WINNT=0x0601)
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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(AG_ENABLE_CODE_SIGNING OFF)
endif()

find_package(FFmpeg REQUIRED)
find_package(WebP REQUIRED)
if(AG_WITH_TRACEREADER)
  find_package(Boost 1.73.0 REQUIRED COMPONENTS program_options)
else()
  find_package(Boost 1.73.0 REQUIRED)
endif()

add_subdirectory(JUCE)

if(AG_SDKS_ROOT)
  juce_set_vst2_sdk_path(${AG_VST2_SDK})
endif()

if(AG_AAX_PLUGIN_ENABLED)
  juce_set_aax_sdk_path(${AG_AAX_SDK})
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Common/Source)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Server/Source)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Source)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Tests/Source)
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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(AG_WITH_SERVER off)
endif()

macro(ag_strip target_name target_dir)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_custom_target(${target_name}_DBGSYM ALL DEPENDS ${target_name}
      COMMAND
        ${CMAKE_COMMAND} -E echo "Creating DWARF file from $<TARGET_FILE:${target_name}>"
      COMMAND
        ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debug/${target_dir}
      COMMAND
        ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target_name}> ${CMAKE_BINARY_DIR}/debug/${target_dir}
      COMMAND
        /usr/bin/dsymutil -o ${CMAKE_BINARY_DIR}/debug/${target_dir}/$<TARGET_FILE_NAME:${target_name}>.dSYM $<TARGET_FILE:${target_name}>
      )
    add_custom_target(${target_name}_STRIP ALL DEPENDS ${target_name}_DBGSYM
      COMMAND
        ${CMAKE_COMMAND} -E echo "Stripping file $<TARGET_FILE:${target_name}>"
      COMMAND
        /usr/bin/strip -S $<TARGET_FILE:${target_name}>
      )
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_custom_target(${target_name}_DBGSYM ALL DEPENDS ${target_name}
      COMMAND
        ${CMAKE_COMMAND} -E echo "Copying debug symbols for $<TARGET_FILE:${target_name}>"
      COMMAND
        ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debug/${target_dir}
      COMMAND
        ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target_name}> ${CMAKE_BINARY_DIR}/debug/${target_dir}
      )
    add_custom_target(${target_name}_STRIP ALL DEPENDS ${target_name}_DBGSYM
      COMMAND
        ${CMAKE_COMMAND} -E echo "Stripping file $<TARGET_FILE:${target_name}>"
      COMMAND
        /usr/bin/strip $<TARGET_FILE:${target_name}>
      )
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_custom_target(${target_name}_STRIP ALL DEPENDS ${target_name}
      COMMAND
        ${CMAKE_COMMAND} -E echo "Copying debug symbols for $<TARGET_FILE:${target_name}>"
      COMMAND
        ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debug/${target_dir}
      COMMAND
        ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target_name}> ${CMAKE_BINARY_DIR}/debug/${target_dir}
      COMMAND
        ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${target_name}> ${CMAKE_BINARY_DIR}/debug/${target_dir}
      )
  endif()
endmacro()

macro(ag_bundle_add_crashpad target_name)
  if(AG_ENABLE_SENTRY)
    add_custom_target(${target_name}_CRASHPAD ALL DEPENDS ${target_name}_STRIP
      COMMAND
        ${CMAKE_COMMAND} -E echo "Adding crashpad to bundle $<TARGET_BUNDLE_DIR:${target_name}>"
      COMMAND
        ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/bin/crashpad_handler $<TARGET_BUNDLE_DIR:${target_name}>/Contents/MacOS
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
        ${CMAKE_COMMAND} -E remove_directory $<TARGET_BUNDLE_DIR:${target_name}>/Contents/_CodeSignature
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
      add_custom_target(${target_name}_SIGN ALL DEPENDS ${target_name}_STRIP
        COMMAND
          ${CMAKE_COMMAND} -E echo "Signing AAX bundle $<TARGET_BUNDLE_DIR:${target_name}>"
        COMMAND
          aax_sign.sh $<TARGET_BUNDLE_DIR:${target_name}> || echo "AAX signing not available"
      )
    else()
      add_custom_target(${target_name}_SIGN ALL DEPENDS ${target_name}_STRIP
        COMMAND
          ${CMAKE_COMMAND} -E echo "Code signing disabled. Not signing AAX bundle $<TARGET_BUNDLE_DIR:${target_name}>"
      )
    endif()
  endmacro()
else()
  macro(ag_bundle_sign_aax target_name)
    if(AG_ENABLE_CODE_SIGNING)
      add_custom_target(${target_name}_SIGN ALL DEPENDS ${target_name}_STRIP
        COMMAND
          ${CMAKE_COMMAND} -E echo "Creating AAX plugin bundle $<TARGET_FILE_NAME:${target_name}>"
        COMMAND
          ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lib/AAX/signed/$<TARGET_FILE_NAME:${target_name}>/Contents/x64
        COMMAND
          ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target_name}> ${CMAKE_BINARY_DIR}/lib/AAX/signed/$<TARGET_FILE_NAME:${target_name}>/Contents/x64
        COMMAND
          ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/package/desktop.ini ${CMAKE_BINARY_DIR}/lib/AAX/signed/$<TARGET_FILE_NAME:${target_name}>
        COMMAND
          attrib +h +r +s ${CMAKE_BINARY_DIR}/lib/AAX/signed/$<TARGET_FILE_NAME:${target_name}>/desktop.ini
        COMMAND
          ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/package/PlugIn.ico ${CMAKE_BINARY_DIR}/lib/AAX/signed/$<TARGET_FILE_NAME:${target_name}>
        COMMAND
          attrib +h +r +s ${CMAKE_BINARY_DIR}/lib/AAX/signed/$<TARGET_FILE_NAME:${target_name}>/PlugIn.ico
        COMMAND
          ${CMAKE_COMMAND} -E echo "Signing AAX plugin $<TARGET_FILE_NAME:${target_name}>"
        COMMAND
          aax_sign.bat ${CMAKE_BINARY_DIR}/lib/AAX/signed/$<TARGET_FILE_NAME:${target_name}>
      )
    else()
      add_custom_target(${target_name}_SIGN ALL DEPENDS ${target_name}_STRIP
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

# Remove some warnings from the JUCE list as the JUCE code triggers those itself...
if((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
  target_compile_options(juce_recommended_warning_flags INTERFACE -Wno-unused-variable -Wno-unused-function -Wno-deprecated)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(juce_recommended_warning_flags INTERFACE -Wno-unused-variable -Wno-unused-but-set-variable)
endif()

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
message(STATUS "VST2 plugins: ${AG_VST2_PLUGIN_ENABLED}")
message(STATUS "AAX plugins: ${AG_AAX_PLUGIN_ENABLED}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND AG_ENABLE_DEBUG_COPY_STEP)
  message(STATUS "Copy step of binaries after building is enabled.")
endif()


if(AG_WITH_TRACEREADER)
  message(STATUS "Tracereader enabled.")
  add_executable(tracereader "${CMAKE_CURRENT_SOURCE_DIR}/Common/Source/TraceReader.cpp")
  target_compile_features(tracereader PRIVATE cxx_std_14)
  target_compile_options(tracereader PRIVATE "-fvisibility=hidden")
  target_link_libraries(tracereader ${Boost_LIBRARIES})
  if(AG_ENABLE_DEBUG_COPY_STEP)
    ag_file_copy(tracereader $ENV{HOME}/bin)
  endif()
endif()

if(AG_WITH_TESTS)
  message(STATUS "Unit tests enabled.")
  add_subdirectory(Tests)
else()
  message(STATUS "Unit tests disabled.")
endif()
