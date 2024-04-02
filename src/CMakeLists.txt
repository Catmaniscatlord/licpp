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

target_link_libraries(Main PRIVATE notcurses++ LispInterpreterLib
                                   Boost::program_options)
include_directories(${notcurses_SOURCE_DIR}/include)
install(TARGETS Main RUNTIME DESTINATION bin)
