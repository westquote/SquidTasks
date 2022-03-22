cmake_minimum_required(VERSION 3.15)

project(SquidTasks)

add_library(${PROJECT_NAME} INTERFACE)

# There doesn't seem to be a coroutines feature in CMAKE_CXX_KNOWN_FEATURES

# Some errors in Sample_TextGame with std_20
#target_compile_features(${PROJECT_NAME} INTERFACE "cxx_std_20")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(${PROJECT_NAME} INTERFACE "-fcoroutines-ts")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16.10)
    # Seems like we should use /await:strict here, but seems to fail.
    #target_compile_options(${PROJECT_NAME} INTERFACE "/await:strict")
    target_compile_options(${PROJECT_NAME} INTERFACE "/await")
  else()
    target_compile_options(${PROJECT_NAME} INTERFACE "/await")
  endif()
endif()

target_include_directories(${PROJECT_NAME} INTERFACE include)

target_sources(${PROJECT_NAME} INTERFACE
  include/TaskTypes.h
  include/Task.h
  include/TaskManager.h
  include/TasksConfig.h
  include/TokenList.h
  include/TaskFSM.h
  include/FunctionGuard.h
  # Private
  include/Private/TaskPrivate.h
  include/Private/TaskFSMPrivate.h
  include/Private/TasksCommonPrivate.h
  include/Private/tl/optional.hpp
  )

option(SQUIDTASKS_BUILD_SAMPLES "Enable building with samples." OFF)

if(SQUIDTASKS_BUILD_SAMPLES)
  add_subdirectory(samples/Sample_Tests)
  add_subdirectory(samples/Sample_Template)
  add_subdirectory(samples/Sample_TextGame)
endif()