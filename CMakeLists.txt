
cmake_minimum_required(VERSION 3.2)
message(STATUS "Cmake version ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION}")

project(workflow_benchmark_makespan_estimator)

add_definitions("-Wall -Wno-unused-variable -Wno-unused-private-field")

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules/")

# Find WRENCH, SimGrid, and Boost
find_package(WRENCH REQUIRED)
find_package(SimGrid REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)

# include directories
include_directories(include/ /usr/local/include/ /opt/local/include/ ${WRENCH_INCLUDE_DIR} ${SimGrid_INCLUDE_DIR} ${Boost_INCLUDE_DIR})

# source files
set(SOURCE_FILES
        src/Estimator.cpp
        src/UnitParser.cpp
        src/WfCommonsWorkflowParser.cpp
        include/UnitParser.h
        include/WfCommonsWorkflowParser.h
        )

# generating the executable
add_executable(workflow_benchmark_makespan_estimator ${SOURCE_FILES})


target_link_libraries(workflow_benchmark_makespan_estimator
            ${WRENCH_LIBRARY}
            ${SimGrid_LIBRARY}
            ${Boost_LIBRARIES}
            )

install(TARGETS workflow_benchmark_makespan_estimator DESTINATION bin)
