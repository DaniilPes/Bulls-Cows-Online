cmake_minimum_required(VERSION 3.18.4)
project(untitled C)

set(CMAKE_C_STANDARD 11)

add_executable(untitled main.c
        main.c)
	
find_package(Threads REQUIRED)
target_link_libraries(untitled PRIVATE Threads::Threads)
