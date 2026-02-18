cmake_minimum_required(VERSION 3.6)
project(miniglut_x68k_demos C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_SOURCE_DIR}/bin)

# Host-only build: compile only demo sources and link against system GLUT/OpenGL.
# Important: do NOT add this repo's "include/" to the include path, because it contains X68000-specific headers.

add_executable(demo_wirecube
  src/demo_wirecube.c
)

add_executable(demo_objflat
  src/demo_objflat.c
)

if(MSVC)
  add_definitions(-D_UNICODE -DUNICODE -D_USE_MATH_DEFINES -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
#  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
#  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:mainCRTStartup")
  set_target_properties(demo_wirecube PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(ProjectDir)/../bin")
  set_target_properties(demo_objflat PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(ProjectDir)/../bin")

  set(CompilerFlags
    CMAKE_CXX_FLAGS
    CMAKE_CXX_FLAGS_DEBUG
    CMAKE_CXX_FLAGS_RELEASE
    CMAKE_C_FLAGS
    CMAKE_C_FLAGS_DEBUG
    CMAKE_C_FLAGS_RELEASE
  )
  foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
  endforeach()

  add_custom_command(
    TARGET demo_wirecube
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  )
  add_custom_command(
    TARGET demo_objflat
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  )
endif()

if(APPLE)
  find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
  find_library(GLUT_FRAMEWORK GLUT REQUIRED)

  target_link_libraries(demo_wirecube PRIVATE ${OPENGL_FRAMEWORK} ${GLUT_FRAMEWORK})
  target_link_libraries(demo_objflat  PRIVATE ${OPENGL_FRAMEWORK} ${GLUT_FRAMEWORK})
else()
  find_package(OpenGL REQUIRED)
  find_package(GLUT REQUIRED)

  foreach(tgt IN ITEMS demo_wirecube demo_objflat)
    if(TARGET OpenGL::GL)
      target_link_libraries(${tgt} PRIVATE OpenGL::GL)
    elseif(OPENGL_gl_LIBRARY)
      target_link_libraries(${tgt} PRIVATE ${OPENGL_gl_LIBRARY})
    endif()

    if(TARGET OpenGL::GLU)
      target_link_libraries(${tgt} PRIVATE OpenGL::GLU)
    elseif(OPENGL_glu_LIBRARY)
      target_link_libraries(${tgt} PRIVATE ${OPENGL_glu_LIBRARY})
    endif()

    if(TARGET GLUT::GLUT)
      target_link_libraries(${tgt} PRIVATE GLUT::GLUT)
    else()
      target_include_directories(${tgt} PRIVATE ${GLUT_INCLUDE_DIR})
      target_link_libraries(${tgt} PRIVATE ${GLUT_LIBRARIES})
    endif()

    if(NOT WIN32)
      target_link_libraries(${tgt} PRIVATE m)
    endif()
  endforeach()
endif()
