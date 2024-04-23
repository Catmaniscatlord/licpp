#
# filename: CMakeLists.txt
#
# description: Build file
#
# authors: Chamberlain, David
#

cmake_minimum_required(VERSION 3.22)
include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# to fix clang errors
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(LispInterpeter)

# ##############################################################################
# FETCH PACKAGES #
# ##############################################################################

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
      USES_TERMINAL_DOWNLOAD TRUE
  DOWNLOAD_NO_EXTRACT FALSE)

FetchContent_Declare(
  notcurses
  URL https://github.com/dankamongmen/notcurses/archive/refs/tags/v3.0.9.tar.gz
      USES_TERMINAL_DOWNLOAD TRUE
  DOWNLOAD_NO_EXTRACT FALSE)

set(USE_DEFLATE OFF)
set(USE_DOCTEST OFF)
set(USE_POC OFF)
set(USE_PANDOC OFF)
set(USE_MULTIMEDIA none)
set(USE_EXECUTABLES OFF)

set(USE_CXX ON)
FetchContent_MakeAvailable(notcurses googletest)

# ##############################################################################
# Project Wide Options #
# ##############################################################################
# This is AFTER the fetch contents otherwise we will compile the packages we
# include with our strick compiler settings.

# Enforce a strict compiler over the whole project
add_compile_options(-Werror -Wall -Wextra -Wpedantic)
enable_testing()
add_subdirectory(lib)
add_subdirectory(src)
# add_subdirectory(tests)
