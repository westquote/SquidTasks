add_executable(Sample_Tests)

target_link_libraries(Sample_Tests SquidTasks)

target_include_directories(Sample_Tests PRIVATE ../Common)

target_sources(Sample_Tests PRIVATE
  ../Common/TimeSystem.h
   Main.cpp
)