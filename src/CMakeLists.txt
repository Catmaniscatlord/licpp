#
# filename: CMakeLists.txt
#
# description: Build file
#
# authors: Chamberlain, David

cmake_minimum_required(VERSION 3.28)

# ##############################################################################
# MAIN EXECUTEABLE #
# ##############################################################################
add_executable(Main main.cpp)

target_link_libraries(Main LispInterpeterLib Boost::program_options)

install(TARGETS Main RUNTIME DESTINATION bin)
