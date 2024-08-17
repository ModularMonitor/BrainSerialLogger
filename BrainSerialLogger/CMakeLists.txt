# CMakeList.txt : CMake project for CMakeTestWSL, include source and define
# project specific logic here.
#

# Add source to this project's executable.
project(BrainSerialLogger VERSION 1.0 DESCRIPTION "The other way to store data of the project.")

file(GLOB_RECURSE source_list "src/*.cpp")
add_executable(${PROJECT_NAME} ${source_list})

# Release?
# set(CMAKE_BUILD_TYPE Release)

target_include_directories(${PROJECT_NAME} PRIVATE 
    #httplib
    "deps"
    "include"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
