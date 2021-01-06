cmake_minimum_required(VERSION 3.16)
project(Skiplist_CPP)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(./stress-test)

add_executable(main
        main.cpp
        skiplist.hpp)

add_executable(stress-test
        ./stress-test/stress_test.cpp
        skiplist.hpp)