cmake_minimum_required(VERSION 3.15)
project(MtFifo)

set(CMAKE_CXX_STANDARD 17)

add_executable(SingleThreadCacheModel main.cpp Fifo.cpp Fifo.hpp)