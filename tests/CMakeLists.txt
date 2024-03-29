#
# filename: CMakeLists.txt
#
# description: Build file
#
# authors: Chamberlain, David

cmake_minimum_required(VERSION 3.28)

# ##############################################################################
# TESTING #
# ##############################################################################
add_executable(LispInterpeterTest lisp-interpreter_test.cpp)
target_link_libraries(LispInterpeterTest PRIVATE gtest_main LispInterpeterLib)

add_test(NAME LispInterpeterTest COMMAND $<TARGET_FILE:LispInterpeterTest>)
