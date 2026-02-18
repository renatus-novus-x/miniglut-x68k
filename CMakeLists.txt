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
