add_executable(Sample_Template)

target_link_libraries(Sample_Template SquidTasks)

target_include_directories(Sample_Template PRIVATE ../Common)

target_sources(Sample_Template PRIVATE
  ../Common/TimeSystem.h
   Main.cpp
)