cmake_minimum_required(VERSION 3.6)
project(demo_wirecube)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_SOURCE_DIR}/bin)

add_executable(demo_wirecube
  src/demo_wirecube.c
)

if(MSVC)
  add_definitions(-D_UNICODE -DUNICODE -D_USE_MATH_DEFINES -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
#  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
#  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:mainCRTStartup")
  set_target_properties(demo_wirecube PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(ProjectDir)/../bin")

  set(CompilerFlags
    CMAKE_CXX_FLAGS
    CMAKE_CXX_FLAGS_DEBUG
    CMAKE_CXX_FLAGS_RELEASE
    CMAKE_C_FLAGS
    CMAKE_C_FLAGS_DEBUG
    CMAKE_C_FLAGS_RELEASE
  )
  #foreach(CompilerFlag ${CompilerFlags})
  #  string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
  #endforeach()

add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
)
endif(MSVC)

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
#  add_definitions(-std=gnu++17)
#  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
  if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "arm64")
    include_directories(/opt/homebrew/include)
  else()
    include_directories(/usr/local/include)
  endif()
endif()

if(UNIX AND NOT APPLE)
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

include_directories( ${OPENGL_INCLUDE_DIRS} )
target_link_libraries(demo_wirecube ${OPENGL_LIBRARIES} GLUT::GLUT)
