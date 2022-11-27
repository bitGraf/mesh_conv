cmake_minimum_required( VERSION 3.20 )

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

set (CMAKE_CXX_STANDARD 17)

project(mesh_conv)

if( MSVC )
    SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:mainCRTStartup" )
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "enable X functionality" FORCE)
add_subdirectory("deps/assimp")
add_subdirectory("deps/math_lib")

add_executable(mesh_conv)
target_sources(mesh_conv PRIVATE
    # source files
    src/main.cpp
    src/mesh_converter.cpp
    src/utils.cpp
    src/animation.cpp
    src/mesh.cpp
    src/skeleton.cpp
    src/animimporter.cpp

    # header files
    src/mesh.h
    src/mesh_converter.h
    src/utils.h
    src/animation.h
    src/skeleton.h
    src/animimporter.h
)
target_link_libraries(mesh_conv assimp laml)
target_include_directories(mesh_conv PRIVATE "include/")
set_target_properties(mesh_conv PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_LIST_DIR}/bin/"
                      RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_LIST_DIR}/bin/"
)
set_property(TARGET mesh_conv PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set_property(TARGET mesh_conv PROPERTY VS_STARTUP_PROJECT bar)

if(MSVC)
  #target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()