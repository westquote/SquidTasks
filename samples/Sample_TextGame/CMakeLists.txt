add_executable(Sample_TextGame)

target_link_libraries(Sample_TextGame SquidTasks)

target_include_directories(Sample_TextGame PRIVATE ../Common)

target_sources(Sample_TextGame PRIVATE
  ../Common/TimeSystem.h
  TextGame.h
  TextInput.h
  Main.cpp
)