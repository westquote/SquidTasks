#include "Task.h"
#include "TaskManager.h"
#include "TimeSystem.h"

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

	// Initialize the game
	// ... <your code here> ...

	// Run the game until it's ready to quit
	while(true)
	{
		// Update time before each frame (logically, all tasks should resume "at the same time")
		TimeSystem::UpdateTime();

		// Update the game
		// ... <your code here> ...
	}

	return 0;
}
