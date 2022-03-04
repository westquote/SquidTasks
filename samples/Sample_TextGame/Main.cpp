#include "Task.h"
#include "TimeSystem.h"
#include "TextGame.h"
#include "TaskFSM.h"

// User-defined GetGlobalTime() is required to link Task.h
NAMESPACE_SQUID_BEGIN
tTaskTime GetGlobalTime()
{
	return (tTaskTime)TimeSystem::GetTime();
}
NAMESPACE_SQUID_END

// Simple main function
int main(int argc, char** argv)
{
	TimeSystem::Create();

	auto game = std::make_shared<TextGame>();
	while(!game->IsGameOver())
	{
		TimeSystem::UpdateTime(); // Update time before each frame (logically, all tasks should resume "at the same time")
		game->Update();
	}

	return 0;
}
