# Macro to find header and lib directories
# Based on: https://sources.debian.org/src/acoustid-fingerprinter/0.6-6/cmake/modules/FindFFmpeg.cmake/
# example: FFMPEG_FIND(AVFORMAT avformat avformat.h)
macro(FFMPEG_FIND varname shortname headername)

  find_path(FFMPEG_${varname}_INCLUDE_DIRS lib${shortname}/${headername}
    PATHS
    ${FFMPEG_ROOT}/include
    $ENV{FFMPEG_DIR}/include
    /usr/local/include
    /usr/include/
    NO_DEFAULT_PATH
    DOC "Location of FFMPEG Headers")

  find_library(FFMPEG_${varname}_LIBRARIES
    NAMES lib${shortname}.a lib${shortname}.lib
    PATHS
    ${FFMPEG_ROOT}/lib
    $ENV{FFMPEG_DIR}/lib
    /usr/local/lib
    /usr/lib
    NO_DEFAULT_PATH
    DOC "Location of FFMPEG Libs")

  if(FFMPEG_${varname}_LIBRARIES AND FFMPEG_${varname}_INCLUDE_DIRS)
    set(FFMPEG_${varname}_FOUND 1)
    message(STATUS "Found ${shortname}")
    message(STATUS "  includes : ${FFMPEG_${varname}_INCLUDE_DIRS}")
    message(STATUS "  libraries: ${FFMPEG_${varname}_LIBRARIES}")
  else()
    message(STATUS "Could not find ${shortname}")
  endif()

endmacro(FFMPEG_FIND)

set(FFMPEG_ROOT "$ENV{FFMPEG_DIR}" CACHE PATH "Location of FFMPEG")

FFMPEG_FIND(LIBAVFORMAT     avformat    avformat.h)
FFMPEG_FIND(LIBAVDEVICE     avdevice    avdevice.h)
FFMPEG_FIND(LIBAVCODEC      avcodec     avcodec.h)
FFMPEG_FIND(LIBAVUTIL       avutil      avutil.h)
FFMPEG_FIND(LIBAVFILTER     avfilter    avfilter.h)
FFMPEG_FIND(LIBSWSCALE      swscale     swscale.h)
FFMPEG_FIND(LIBSWRESAMPLE   swresample  swresample.h)

set(FFMPEG_FOUND "NO")
if (FFMPEG_LIBAVFORMAT_FOUND AND
    FFMPEG_LIBAVDEVICE_FOUND AND
    FFMPEG_LIBAVCODEC_FOUND AND
    FFMPEG_LIBAVUTIL_FOUND AND
    FFMPEG_LIBAVFILTER_FOUND AND
    FFMPEG_LIBSWSCALE_FOUND AND
    FFMPEG_LIBSWRESAMPLE_FOUND)

    set(FFMPEG_FOUND "YES")
    set(FFMPEG_INCLUDE_DIRS ${FFMPEG_LIBAVFORMAT_INCLUDE_DIRS})
    set(FFMPEG_LIBRARY_DIRS ${FFMPEG_LIBAVFORMAT_LIBRARY_DIRS})
    set(FFMPEG_LIBRARIES
        ${FFMPEG_LIBAVFORMAT_LIBRARIES}
        ${FFMPEG_LIBAVDEVICE_LIBRARIES}
        ${FFMPEG_LIBAVCODEC_LIBRARIES}
        ${FFMPEG_LIBAVUTIL_LIBRARIES}
        ${FFMPEG_LIBAVFILTER_LIBRARIES}
        ${FFMPEG_LIBSWSCALE_LIBRARIES}
        ${FFMPEG_LIBSWRESAMPLE_LIBRARIES})
else()
  message(STATUS "Could not find FFMPEG")
endif()
