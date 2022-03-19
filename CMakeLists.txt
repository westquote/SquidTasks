cmake_minimum_required(VERSION 3.15)

project(SquidTasks)

add_library(${PROJECT_NAME} INTERFACE)

# There doesn't seem to be a coroutines feature in CMAKE_CXX_KNOWN_FEATURES
target_compile_features(${PROJECT_NAME} INTERFACE "cxx_std_20")
target_compile_options(${PROJECT_NAME} INTERFACE "-fcoroutines")

target_include_directories(${PROJECT_NAME} INTERFACE include)

target_sources(${PROJECT_NAME} INTERFACE
  include/Task.h
  include/TaskManager.h
  include/TasksConfig.h
  include/TokenList.h
  include/TaskFSM.h
  include/FunctionGuard.h
  )
