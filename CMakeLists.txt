# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.24)
project(rgbdsxp LANGUAGES C CXX)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/rgbds/CMakeLists.txt")
  message(FATAL_ERROR
    "The rgbds submodule is missing. Initialise it first:\n"
    "  git submodule update --init --recursive")
endif()

if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
      CACHE STRING "Static CRT in every configuration (XP-friendly)")
endif()

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

option(RGBDSXP_RELMIN_NO_EH "RelMin: compile C++ without exception handling (opt-in)" OFF)
if(MSVC)
  foreach(lang C CXX)
    if(NOT CMAKE_${lang}_FLAGS MATCHES "/nologo")
      string(APPEND CMAKE_${lang}_FLAGS " /nologo")
    endif()
  endforeach()

  set(_relmin "/Gw /Gy /Oi /GS- /fp:except- /guard:cf-")
  set(_relmin_cxx "${_relmin} /GR-")
  if(RGBDSXP_RELMIN_NO_EH)
    string(APPEND _relmin_cxx " /EHs-c- /D_HAS_EXCEPTIONS=0 /wd4530")
  endif()

  set(CMAKE_C_FLAGS_RELMIN   "${CMAKE_C_FLAGS_RELEASE} ${_relmin}"       CACHE STRING "" FORCE)
  set(CMAKE_CXX_FLAGS_RELMIN "${CMAKE_CXX_FLAGS_RELEASE} ${_relmin_cxx}" CACHE STRING "" FORCE)
  set(CMAKE_EXE_LINKER_FLAGS_RELMIN
      "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /INCREMENTAL:NO /OPT:REF /OPT:ICF"   CACHE STRING "" FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS_RELMIN "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}" CACHE STRING "" FORCE)
  set(CMAKE_STATIC_LINKER_FLAGS_RELMIN "${CMAKE_STATIC_LINKER_FLAGS_RELEASE}" CACHE STRING "" FORCE)
  set(CMAKE_MODULE_LINKER_FLAGS_RELMIN "${CMAKE_MODULE_LINKER_FLAGS_RELEASE}" CACHE STRING "" FORCE)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELMIN ON)


  if(CMAKE_CONFIGURATION_TYPES AND NOT "RelMin" IN_LIST CMAKE_CONFIGURATION_TYPES)
    list(APPEND CMAKE_CONFIGURATION_TYPES "RelMin")
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING
        "Available build configurations" FORCE)
  endif()

  if(RGBDSXP_RELMIN_NO_EH)
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
      string(APPEND CMAKE_CXX_FLAGS_${cfg} " /EHsc")
    endforeach()
  endif()
endif()

find_program(RGBDSXP_GIT NAMES git)
set(RGBDS_VERSION_FROM_GIT "")
if(RGBDSXP_GIT)
  execute_process(
    COMMAND "${RGBDSXP_GIT}" describe --tags --always
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/rgbds"
    OUTPUT_VARIABLE RGBDS_VERSION_FROM_GIT
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()

add_custom_target(rgbdsxp ALL)

include(FetchContent)
foreach(_dep_pair "ZLIB=zlib" "PNG=libpng")
  string(REPLACE "=" ";" _dep_pair "${_dep_pair}")
  list(GET _dep_pair 0 _dep_name)
  list(GET _dep_pair 1 _dep_dir)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_dep_dir}/CMakeLists.txt")
    message(FATAL_ERROR
      "The ${_dep_dir} submodule is missing. Initialise it first:\n"
      "  git submodule update --init --recursive")
  endif()
  FetchContent_Declare(${_dep_name}
                       SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${_dep_dir}"
                       EXCLUDE_FROM_ALL
                       OVERRIDE_FIND_PACKAGE)
endforeach()

set(CMAKE_DISABLE_FIND_PACKAGE_Git ON)
add_subdirectory(rgbds EXCLUDE_FROM_ALL)
unset(CMAKE_DISABLE_FIND_PACKAGE_Git)

if(TARGET common)
  target_compile_definitions(common PRIVATE "BUILD_VERSION_STRING=\"${RGBDS_VERSION_FROM_GIT}\"")
endif()

set(RGBDSXP_BIN "${CMAKE_BINARY_DIR}/bin")


if(MSVC)
  add_library(xpcompat OBJECT xpcompat/crt.c)
  target_compile_definitions(xpcompat PRIVATE _CRT_SECURE_NO_WARNINGS)
  set_target_properties(xpcompat PROPERTIES
                        INTERPROCEDURAL_OPTIMIZATION OFF
                        INTERPROCEDURAL_OPTIMIZATION_RELMIN OFF)

  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    enable_language(ASM_MASM)
    target_sources(xpcompat PRIVATE xpcompat/slot.asm)
    set(xp_subsystem "/SUBSYSTEM:CONSOLE,5.01")
  else()
    set(xp_subsystem "/SUBSYSTEM:CONSOLE,5.02")
  endif()
endif()

foreach(tool IN ITEMS rgbasm rgblink rgbfix rgbgfx)
  if(TARGET ${tool})
    set_target_properties(${tool} PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "$<1:${RGBDSXP_BIN}>")
    if(MSVC)
      target_sources(${tool} PRIVATE $<TARGET_OBJECTS:xpcompat>)
      target_link_options(${tool} PRIVATE "${xp_subsystem}")
    endif()
    add_dependencies(rgbdsxp ${tool})
  endif()
endforeach()
