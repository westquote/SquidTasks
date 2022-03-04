#pragma once

/// @defgroup TaskManager Task Manager
/// @brief Manager that runs and resumes a collection of tasks.
/// @{
/// 
/// A TaskManager is a simple manager class that holds an ordered list of tasks and resumes them whenever it is updated.
/// 
/// Running Tasks
/// -------------
/// There are two primary ways to run tasks on a task manager.
/// 
/// The first method (running an "unmanaged task") is to pass a task into @ref TaskManager::Run(). This will move the task
/// into the task manager and return a @ref TaskHandle that can be used to observe and manage the lifetime of the task (as well
/// as potentially take a return value after the task finishes). With unmanaged tasks, the task manager only holds a weak
/// reference to the task, meaning that the @ref TaskHandle returned by @ref TaskManager::Run() is the only remaining strong
/// reference to the task.  Because of this, the caller is entirely responsible for managing the lifetime of the task.
/// 
/// The second method (running a "managed task") is to pass a task into @ref TaskManager::RunManaged(). Like
/// @ref TaskManager::Run(), this will move the task into the task manager and return a @ref WeakTaskHandle that can be used to
/// observe the lifetime of the task (as well as manually kill it, if desired). Unlike unmanaged tasks, the task manager
/// stores a strong reference to the task.  Because of this, that caller is not responsible for managing the lifetime of
/// the task.  This difference in task ownership means that (unlike an unmanaged task) a managed task can be thought of as
/// a "fire-and-forget" task that will run until either it finishes or until something else explicitly kills it.
/// 
/// Order of Execution
/// ------------------
/// The ordering of task updates within a call to @ref TaskManager::Update() is stable, meaning that the first task that
/// is run on a task manager will remain the first to resume, no matter how many other tasks are run on the task manager
/// (or terminate) in the meantime.
/// 
/// Integration into Actor Classes
/// ------------------------------
/// Consider the following example of a TaskManager that has been integrated into a TaskActor base class:
/// 
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
/// 
/// class TaskActor : public Actor
/// {
///	public:
///		virtual void OnInitialize() override // Automatically called when this enemy enters the scene
///		{
///			Actor::OnInitialize(); // Call the base Actor function
///			m_taskMgr.RunManaged(ManageActor()); // Run main actor task as a fire-and-forget "managed task"
///		}
/// 
///		virtual void Tick(float in_dt) override // Automatically called every frame
///		{
///			Actor::Tick(in_dt); // Call the base Actor function
///			m_taskMgr.Update(); // Resume all active tasks once per tick
///		}
/// 
///		virtual void OnDestroy() override // Automatically called when this enemy leaves the scene
///		{
///			m_taskMgr.KillAllTasks(); // Kill all active tasks when we leave the scene
///			Actor::OnDestroy(); // Call the base Actor function
///		}
/// 
/// protected:
///		virtual Task<> ManageActor() // Overridden (in its entirety) by child classes
///		{
///			co_await WaitForever(); // Waits forever (doing nothing)
///		}
/// 
///		TaskManager m_taskMgr;
/// };
/// 
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/// 
/// In the above example, TaskManager is instantiated once per high-level actor. It is updated once per frame within
/// the Tick() method, and all its tasks are killed when it leaves the scene in OnDestroy(). Lastly, a single entry-point
/// coroutine is run as a managed task when the actor enters the scene. (The above is the conventional method of integration
/// into this style of game engine.)
/// 
/// Note that it is sometimes necessary to have multiple TaskManagers within a single actor. For example, if there are
/// multiple tick functions (such as one for pre-physics updates and one for post-physics updates), then instantiating
/// a second "post-physics" task manager may be desirable.

#include "Task.h"

NAMESPACE_SQUID_BEGIN

//--- TaskManager ---//
/// Manager that runs and resumes a collection of tasks.
class TaskManager
{
public:
	~TaskManager() {} /// Destructor (disables copy/move construction + assignment)

	/// @brief Run an unmanaged task
	/// @details Run() return a @ref TaskHandle<> that holds a strong reference to the task. If there are ever no
	/// strong references remaining to an unmanaged task, it will immediately be killed and removed from the manager.
	template <typename tRet>
	SQUID_NODISCARD TaskHandle<tRet> Run(Task<tRet>&& in_task)
	{
		// Run unmanaged task
		TaskHandle<tRet> taskHandle = in_task;
		WeakTask weakTask = MoveTemp(in_task);
		RunWeakTask(MoveTemp(weakTask));
		return taskHandle;
	}
	template <typename tRet>
	SQUID_NODISCARD TaskHandle<tRet> Run(const Task<tRet>& in_task) /// @private Illegal copy implementation
	{
		static_assert(static_false<tRet>::value, "Cannot run an unmanaged task by copy (try Run(MoveTemp(task)))");
		return {};
	}

	/// @brief Run a managed task
	/// @details RunManaged() return a @ref WeakTaskHandle, meaning it can be used to run a "fire-and-forget" background
	/// task in situations where it is not necessary to observe or control task lifetime.
	template <typename tRet>
	WeakTaskHandle RunManaged(Task<tRet>&& in_task)
	{
		// Run managed task
		WeakTaskHandle weakTaskHandle = in_task;
		m_strongRefs.Add(Run(MoveTemp(in_task)));
		return weakTaskHandle;
	}
	template <typename tRet>
	WeakTaskHandle RunManaged(const Task<tRet>& in_task) /// @private Illegal copy implementation
	{
		static_assert(static_false<tRet>::value, "Cannot run a managed task by copy (try RunManaged(MoveTemp(task)))");
		return {};
	}

	/// @brief Run a weak task
	/// @details RunWeakTask() runs a WeakTask. The caller is assumed to have already created a strong TaskHandle<> that
	/// references the WeakTask, thus keeping it from being killed. When the last strong reference to the WeakTask is
	/// destroyed, the task will immediately be killed and removed from the manager.
	void RunWeakTask(WeakTask&& in_task)
	{
		// Run unmanaged task
		m_tasks.Add(MoveTemp(in_task));
	}

	/// Call Task::Kill() on all tasks (managed + unmanaged)
	void KillAllTasks()
	{
		m_tasks.Reset(); // Destroying all the weak tasks implicitly destroys all internal tasks

		// No need to call Kill() on each TaskHandle in m_strongRefs
		m_strongRefs.Reset(); // Handles in the strong refs array only ever point to tasks in the now-cleared m_tasks array
	}

	/// @brief Issue a stop request using @ref Task::RequestStop() on all active tasks (managed and unmanaged)
	/// @details Returns a new awaiter task that will wait until all those tasks have all terminated.
	Task<> StopAllTasks()
	{
		// Request stop on all tasks
		TArray<WeakTaskHandle> weakHandles;
		for(auto& task : m_tasks)
		{
			task.RequestStop();
			weakHandles.Add(task);
		}

		// Return a fence task that waits until all stopped tasks are complete
		return [](TArray<WeakTaskHandle> in_weakHandles) -> Task<> {
			TASK_NAME("StopAllTasks() Fence Task");
			for(const auto& weakHandle : in_weakHandles)
			{
				co_await weakHandle; // Wait until task is complete
			}
		}(MoveTemp(weakHandles));
	}

	/// Call @ref Task::Resume() on all active tasks exactly once (managed + unmanaged)
	void Update()
	{
		// Resume all tasks
		int32 writeIdx = 0;
		for(int32 readIdx = 0; readIdx < m_tasks.Num(); ++readIdx)
		{
			if(m_tasks[readIdx].Resume() != eTaskStatus::Done)
			{
				if(writeIdx != readIdx)
				{
					m_tasks[writeIdx] = MoveTemp(m_tasks[readIdx]);
				}
				++writeIdx;
			}
		}
		m_tasks.SetNum(writeIdx);

		// Prune strong tasks that are done
		m_strongRefs.RemoveAllSwap([](const auto& in_taskHandle) { return in_taskHandle.IsDone(); });
	}

	/// Get a debug string containing a list of all active tasks
	FString GetDebugString(TOptional<TaskDebugStackFormatter> in_formatter = {}) const
	{
		FString debugStr;
		for(const auto& task : m_tasks)
		{
			if(!task.IsDone())
			{
				if(debugStr.Len())
				{
					debugStr += '\n';
				}
				debugStr += task.GetDebugStack(in_formatter);
			}
		}
		return debugStr;
	}

private:
	TArray<WeakTask> m_tasks;
	TArray<TaskHandle<>> m_strongRefs;
};

NAMESPACE_SQUID_END

///@} end of TaskManager group
