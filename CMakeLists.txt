cmake_minimum_required(VERSION 3.17)

project(bernard)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(SRCs Scanner.cc
        Parser.h
        Parser.cc)

add_executable(Scanner_Test Scanner_Test.cc ${SRCs})
target_link_libraries(Scanner_Test gtest)

add_executable(Parser_Test Parser_Test.cc ${SRCs})
target_link_libraries(Parser_Test gtest)