# add library
add_library(libkilo++
  Editor.cpp
  EditorUtils.cpp)

# add include directories
target_include_directories(libkilo++ PUBLIC ${CMAKE_SOURCE_DIR}/include)
