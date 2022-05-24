cmake_minimum_required(VERSION 3.16.3)
project(sik_zadanie2A)
#set(BOOST_ROOT "/usr/include/boost/")
set(Boost_USE_STATIC_LIBS OFF)

find_package(Boost COMPONENTS system REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (NOT Boost_FOUND)
    message(FATAL_ERROR "Could not find boost!")
endif ()


set(CMAKE_CXX_STANDARD 14)

#add_executable(boost_smieci_testowe
        #boost_smieci_testowe.cpp
        #boost_smieci_testowe.hpp)

add_executable(client
        client.cpp
        )

target_link_libraries(client ${Boost_LIBRARIES} Threads::Threads)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
#target_link_libraries(client PRIVATE Threads::Threads)

find_package(Boost 1.40 COMPONENTS program_options REQUIRED)
target_link_libraries(client Boost::program_options Threads::Threads)
