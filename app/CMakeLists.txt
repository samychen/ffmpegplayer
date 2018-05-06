cmake_minimum_required(VERSION 3.4.1)

set(distribution_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)

add_library( # Sets the name of the library.
             native-lib

             SHARED

             #src/main/cpp/native-lib.cpp
             #src/main/cpp/transcode.cpp
             src/main/cpp/transcode2.cpp
             src/main/cpp/VideoUtil.cpp
             src/main/cpp/BackAndForth.cpp
             #src/main/cpp/videoreverse.cpp
             #src/main/cpp/savefile.cpp
             src/main/cpp/native-lib.cpp
          src/main/cpp/memory.cpp
          src/main/cpp/test.c
          src/main/cpp/main.cpp
          src/main/cpp/processpcm.cpp
             )


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libbzffmpeg.so ${CMAKE_CURRENT_SOURCE_DIR}/.externalNativeBuild/cmake/debug/${ANDROID_ABI}/libbzffmpeg.so COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libbzffmpegcmd.so ${CMAKE_CURRENT_SOURCE_DIR}/.externalNativeBuild/cmake/debug/${ANDROID_ABI}/libbzffmpegcmd.so COPYONLY)

#release
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libbzffmpeg.so ${CMAKE_CURRENT_SOURCE_DIR}/.externalNativeBuild/cmake/release/${ANDROID_ABI}/libbzffmpeg.so COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libbzffmpegcmd.so ${CMAKE_CURRENT_SOURCE_DIR}/.externalNativeBuild/cmake/release/${ANDROID_ABI}/libbzffmpegcmd.so COPYONLY)

add_library(
            bzffmpeg
            SHARED
            IMPORTED)
set_target_properties(
            bzffmpeg
            PROPERTIES IMPORTED_LOCATION
            libbzffmpeg.so)
add_library(
          bzffmpegcmd
          SHARED
          IMPORTED)
set_target_properties(
          bzffmpegcmd
          PROPERTIES IMPORTED_LOCATION
          libbzffmpegcmd.so)
 add_library(
             fmemopen
             SHARED
             IMPORTED)
 set_target_properties(
             fmemopen
             PROPERTIES IMPORTED_LOCATION
             libfmemopen.so)
add_library(
            sox
            SHARED
            IMPORTED)
set_target_properties(
            sox
            PROPERTIES IMPORTED_LOCATION
            libsox.so)
include_directories(
                    src/main/cpp/
                          )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libsox.so ${CMAKE_CURRENT_SOURCE_DIR}/.externalNativeBuild/cmake/debug/${ANDROID_ABI}/libsox.so COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libfmemopen.so ${CMAKE_CURRENT_SOURCE_DIR}/.externalNativeBuild/cmake/debug/${ANDROID_ABI}/libfmemopen.so COPYONLY)

find_library( # Sets the name of the path variable.
              log-lib
              log )


target_link_libraries( # Specifies the target library.
                       native-lib  bzffmpeg   bzffmpegcmd  sox  fmemopen
                       ${log-lib} )