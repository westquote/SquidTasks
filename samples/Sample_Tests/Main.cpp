#include "Task.h"
#include "TimeSystem.h"
#include "TaskFSM.h"

// User-defined GetGlobalTime() is required to link Task.h
NAMESPACE_SQUID_BEGIN
tTaskTime GetGlobalTime()
{
	return (tTaskTime)TimeSystem::GetTime();
}
NAMESPACE_SQUID_END

Task<> IdleTask()
{
	TASK_NAME(__FUNCTION__);

	printf("Idle task\n");
	co_await WaitForever();
}

Task<> PeriodicTask(float in_duration)
{
	TASK_NAME(__FUNCTION__);
	printf("Periodic task\n");
	co_await WaitSeconds(in_duration);
}

Task<> TestFsmTask()
{
	TASK_NAME(__FUNCTION__);
	TaskFSM fsm;

	auto LambdaStateTask = [](float in_duration) -> Task<>
	{
		TASK_NAME(__FUNCTION__);
		printf("Lambda state!\n");

		auto stopCtx = co_await GetStopContext();
		co_await WaitSeconds(in_duration).CancelIf([&] { return stopCtx.IsStopRequested(); });
	};

	auto idleState = fsm.State("Idle", IdleTask);
	auto periodicState = fsm.State("Periodic", &PeriodicTask);
	auto lambdaState = fsm.State("Lambda", LambdaStateTask);
	auto endState = fsm.State("End");

	fsm.EntryLinks({
		idleState.Link(),
		});
	fsm.StateLinks(idleState, {
		periodicState.Link([]() -> std::optional<float> { return 1.0f; }),
		endState.OnCompleteLink(),
		});
	fsm.StateLinks(periodicState, {
		lambdaState.Link([]() -> std::optional<float> { return 2.0f; }),
		});
	fsm.StateLinks(lambdaState, {
		idleState.OnCompleteLink(),
		});

	auto fsmTask = fsm.Run();
	fsmTask.RequestStop();
	co_await std::move(fsmTask);
}

void TestTaskFSM()
{
	auto task = TestFsmTask();
	while(task.Resume() != eTaskStatus::Done)
	{
		TimeSystem::UpdateTime(); // Update time before each frame (logically, all tasks should resume "at the same time")
	}
}

// Simple main function
int main(int argc, char** argv)
{
	TimeSystem::Create();

	TestTaskFSM();

	return 0;
}
