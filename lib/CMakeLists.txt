#
# filename: CMakeLists.txt
#
# description: Build file
#
# authors: Chamberlain, David

cmake_minimum_required(VERSION 3.28)

# ##############################################################################
# LIBRARY CREATION #
# ##############################################################################
add_library(LispInterpreterLib STATIC interpreter.cpp interpreter.hpp)
target_include_directories(LispInterpreterLib PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(LispInterpreterLib Boost::unordered)
