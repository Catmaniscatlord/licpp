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

target_link_libraries(Main PRIVATE notcurses++ Boost::program_options
)# target_include_directories(Main ${NCPP_INCLUDE_DIRS})
include_directories(${notcurses_SOURCE_DIR}/include)
install(TARGETS Main RUNTIME DESTINATION bin)
