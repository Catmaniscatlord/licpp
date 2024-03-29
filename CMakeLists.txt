#
# filename: CMakeLists.txt
#
# description: Build file
#
# authors: Chamberlain, David
#

cmake_minimum_required(VERSION 3.28)
include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# to fix clang errors
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(LispInterpeter)

# ##############################################################################
# FETCH PACKAGES #
# ##############################################################################

set(BOOST_INCLUDE_LIBRARIES program_options unordered)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.gz
      USES_TERMINAL_DOWNLOAD
      TRUE
  DOWNLOAD_NO_EXTRACT FALSE)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
      USES_TERMINAL_DOWNLOAD TRUE
  DOWNLOAD_NO_EXTRACT FALSE)

FetchContent_MakeAvailable(Boost googletest)

# ##############################################################################
# Project Wide Options #
# ##############################################################################
# This is AFTER the fetch contents otherwise we will compile the packages we
# include with our strick compiler settings.

# Enforce a strict compiler over the whole project
add_compile_options(-Werror -Wall -Wextra -Wconversion -Wpedantic)
enable_testing()
add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(tests)
