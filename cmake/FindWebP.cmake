# Macro to find header and lib directories
# Based on: https://sources.debian.org/src/acoustid-fingerprinter/0.6-6/cmake/modules/FindFFmpeg.cmake/
# example: FFMPEG_FIND(AVFORMAT avformat avformat.h)
macro(WEBP_FIND varname shortname headername)

  find_path(WEBP_${varname}_INCLUDE_DIRS webp/${headername}
    PATHS
    ${FFMPEG_ROOT}/include
    $ENV{FFMPEG_DIR}/include
    /usr/local/include
    /usr/include/
    NO_DEFAULT_PATH
    DOC "Location of FFMPEG Headers")

  find_library(WEBP_${varname}_LIBRARIES
    NAMES lib${shortname}.a lib${shortname}.lib
    PATHS
    ${FFMPEG_ROOT}/lib
    $ENV{FFMPEG_DIR}/lib
    /usr/local/lib
    /usr/lib
    NO_DEFAULT_PATH
    DOC "Location of FFMPEG Libs")

  if(WEBP_${varname}_LIBRARIES AND WEBP_${varname}_INCLUDE_DIRS)
    set(WEBP_${varname}_FOUND 1)
    message(STATUS "Found ${shortname}")
    message(STATUS "  includes : ${WEBP_${varname}_INCLUDE_DIRS}")
    message(STATUS "  libraries: ${WEBP_${varname}_LIBRARIES}")
  else()
    message(STATUS "Could not find ${shortname}")
  endif()

endmacro(WEBP_FIND)

set(WEBP_ROOT "$ENV{WEBP_DIR}" CACHE PATH "Location of WebP")

WEBP_FIND(LIBWEBP       webp        encode.h)
WEBP_FIND(LIBWEBPMUX    webpmux     mux.h)

set(WEBP_FOUND "NO")
if (WEBP_LIBWEBP_FOUND AND WEBP_LIBWEBPMUX_FOUND)
    set(WEBP_FOUND "YES")
    set(WEBP_INCLUDE_DIRS ${WEBP_LIBWEBP_INCLUDE_DIRS})
    set(WEBP_LIBRARY_DIRS ${WEBP_LIBWEBP_LIBRARY_DIRS})
    set(WEBP_LIBRARIES
        ${WEBP_LIBWEBP_LIBRARIES}
        ${WEBP_LIBWEBPMUX_LIBRARIES})
else()
  message(STATUS "Could not find WebP")
endif()
