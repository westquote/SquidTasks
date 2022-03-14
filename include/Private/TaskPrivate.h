// WARNING: This is an internal implementation header, which must be included from a specific location/namespace
//          That is the reason that this header does not contain a #pragma once, nor namespace guards

enum class eTaskRef;
template <typename tRet> class TaskPromise;
class TaskInternalBase;
template <typename tRet> class TaskInternal;

//--- tTaskReadyFn ---//
using tTaskReadyFn = std::function<bool()>;

template <typename tRet, eTaskRef RefType, eTaskResumable Resumable>
auto CancelTaskIf(Task<tRet, RefType, Resumable>&& in_task, tTaskCancelFn in_cancelFn);

template <typename tRet, eTaskRef RefType, eTaskResumable Resumable>
auto StopTaskIf(Task<tRet, RefType, Resumable>&& in_task, tTaskCancelFn in_cancelFn);
template <typename tRet, eTaskRef RefType, eTaskResumable Resumable, typename tTimeFn>
auto StopTaskIf(Task<tRet, RefType, Resumable>&& in_task, tTaskCancelFn in_cancelFn, tTaskTime in_timeout, tTimeFn in_timeFn);
template <typename tRet, eTaskRef RefType, eTaskResumable Resumable, typename T>
auto StopTaskIf(Task<tRet, RefType, Resumable>&& in_task, tTaskCancelFn in_cancelFn, tTaskTime in_timeout);

//--- Suspend-If Awaiter ---//
struct SuspendIf
{
	SuspendIf(bool in_suspend)
		: m_suspend(in_suspend)
	{
	}
	bool await_ready() noexcept { return !m_suspend; }
	void await_suspend(std::coroutine_handle<>) noexcept {}
	void await_resume() noexcept {}

private:
	bool m_suspend;
};

//--- Task Debug Stack Formatter ---//
struct TaskDebugStackFormatter
{
	// Format function (formats a debug output string) [virtual]
	virtual std::string Format(const std::string& in_str)
	{
		std::string result = Indent(0);
		int32_t indent = 0;
		size_t start = 0;
		size_t found = 0;
		while((found = in_str.find('\n', start)) != std::string::npos)
		{
			size_t end = found + 1;
			if((found < in_str.size() - 1) && (in_str[found + 1] == '`')) // indent
			{
				++indent;
				++end;
			}
			else if((found >= 1) && (in_str[found - 1] == '`')) // dedent
			{
				--indent;
				--found;
			}
			result += in_str.substr(start, found - start) + '\n' + Indent(indent);
			start = end;
		}
		result += in_str.substr(start);
		return result;
	}
	virtual std::string Indent(int32_t in_indent)
	{
		return std::string((long long)in_indent * 2, ' ');
	}
};
static std::string FormatDebugString(std::string in_str)
{
	std::replace(in_str.begin(), in_str.end(), '\n', ' ');
	return in_str.substr(0, 32);
}

//--- SetDebugName Awaiter ---//
#if SQUID_ENABLE_TASK_DEBUG
struct SetDebugName
{
	// Sets a Task's debug name field
	SetDebugName(const char* in_name)
		: m_name(in_name)
	{
	}
	SetDebugName(const char* in_name, std::function<std::string()> in_dataFn)
		: m_name(in_name)
		, m_dataFn(in_dataFn)
	{
	}

private:
	template <typename tRet> friend class TaskPromiseBase;
	const char* m_name = nullptr;
	std::function<std::string()> m_dataFn;
};
#endif //SQUID_ENABLE_TASK_DEBUG

//--- AddStopTask Awaiter ---//
template <typename tRet, eTaskRef RefType, eTaskResumable Resumable>
struct AddStopTaskAwaiter
{
	AddStopTaskAwaiter(Task<tRet, RefType, Resumable>& in_taskToStop)
		: m_taskToStop(&in_taskToStop)
	{
	}

private:
	template <typename tOtherRet> friend class TaskPromiseBase;
	Task<tRet, RefType, Resumable>* m_taskToStop = nullptr;
};

template <typename tRet, eTaskRef RefType, eTaskResumable Resumable>
auto AddStopTask(Task<tRet, RefType, Resumable>& in_taskToStop)
{
	return AddStopTaskAwaiter<tRet, RefType, Resumable>(in_taskToStop);
};

//--- RemoveStopTask Awaiter ---//
template <typename tRet, eTaskRef RefType, eTaskResumable Resumable>
struct RemoveStopTaskAwaiter
{
	RemoveStopTaskAwaiter(Task<tRet, RefType, Resumable>& in_taskToStop)
		: m_taskToStop(&in_taskToStop)
	{
	}

private:
	template <typename tOtherRet> friend class TaskPromiseBase;
	Task<tRet, RefType, Resumable>* m_taskToStop = nullptr;
};

template <typename tRet, eTaskRef RefType, eTaskResumable Resumable>
auto RemoveStopTask(Task<tRet, RefType, Resumable>& in_taskToStop)
{
	return RemoveStopTaskAwaiter<tRet, RefType, Resumable>(in_taskToStop);
};

//--- Task Awaiter ---//
template <typename tRet, eTaskRef RefType, eTaskResumable Resumable, typename promise_type>
struct TaskAwaiterBase
{
	TaskAwaiterBase(const Task<tRet, RefType, Resumable>& in_task)
	{
		// This constructor exists to minimize downstream compile-error spam when co_awaiting a non-copyable Task by copy
	}
	TaskAwaiterBase(Task<tRet, RefType, Resumable>&& in_task)
		: m_task(std::move(in_task))
	{
		SQUID_RUNTIME_CHECK(m_task.IsValid(), "Tried to await an invalid task");
	}
	TaskAwaiterBase(TaskAwaiterBase&& in_taskAwaiter) noexcept
	{
		m_task = std::move(in_taskAwaiter.m_task);
	}
	bool await_ready() noexcept
	{
		if(m_task.IsDone())
		{
			return true;
		}
		return false;
	}
	template <eTaskResumable UResumable = Resumable, typename std::enable_if_t<UResumable == eTaskResumable::Yes>* = nullptr>
	bool await_suspend(std::coroutine_handle<promise_type> in_coroHandle) noexcept
	{
		// Set the sub-task on the suspending task
		auto& promise = in_coroHandle.promise();
		auto taskInternal = promise.GetInternalTask();
		auto subTaskInternal = m_task.GetInternalTask();
		if(taskInternal->IsStopRequested())
		{
			subTaskInternal->RequestStop(); // Propagate any stop request to new sub-tasks
		}
		taskInternal->SetSubTask(std::static_pointer_cast<TaskInternalBase>(subTaskInternal));

		// Resume the task
		if(m_task.Resume() == eTaskStatus::Done)
		{
			taskInternal->SetSubTask(nullptr);
			return false; // Do not suspend, because the task is done
		}
		return true; // Suspend, because the task is not done
	}
	template <eTaskResumable UResumable = Resumable, typename std::enable_if_t<UResumable == eTaskResumable::No>* = nullptr>
	bool await_suspend(std::coroutine_handle<promise_type> in_coroHandle) noexcept
	{
		auto& promise = in_coroHandle.promise();
		if(!m_task.IsDone())
		{
			promise.SetReadyFunction([this] { return m_task.IsDone(); });
			return true; // Suspend, because the task is not done
		}
		return false; // Do not suspend, because the task is done
	}

protected:
	auto GetInternalTask() const
	{
		return m_task.GetInternalTask();
	}
	Task<tRet, RefType, Resumable> m_task;
};

template <typename tRet, eTaskRef RefType, eTaskResumable Resumable, typename promise_type>
struct TaskAwaiter : public TaskAwaiterBase<tRet, RefType, Resumable, promise_type>
{
	using TaskAwaiterBase<tRet, RefType, Resumable, promise_type>::TaskAwaiterBase;

	template <typename U = tRet, typename std::enable_if_t<!std::is_void<U>::value>* = nullptr>
	auto await_resume()
	{
		this->m_task.RethrowUnhandledException(); // Re-throw any exceptions
		auto retVal = this->m_task.TakeReturnValue();
		SQUID_RUNTIME_CHECK(retVal.has_value(), "Awaited task return value is unset");
		return std::move(retVal.value());
	}

	template <typename U = tRet, typename std::enable_if_t<std::is_void<U>::value>* = nullptr>
	void await_resume()
	{
		this->m_task.RethrowUnhandledException(); // Re-throw any exceptions
	}
};

//--- Future Awaiter ---//
template <typename tRet, typename promise_type>
struct FutureAwaiter
{
	FutureAwaiter(std::future<tRet>&& in_future)
		: m_future(std::move(in_future))
	{
	}
	~FutureAwaiter()
	{
	}
	FutureAwaiter(FutureAwaiter&& in_futureAwaiter) noexcept
	{
		m_future = std::move(in_futureAwaiter.m_future);
	}
	bool await_ready() noexcept
	{
		bool isReady = m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		return isReady;
	}
	bool await_suspend(std::coroutine_handle<promise_type> in_coroHandle) noexcept
	{
		// Set the ready function
		auto& promise = in_coroHandle.promise();
		auto IsFutureReady = [this] {
			return m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		};

		// Suspend if future is not ready
		bool isReady = m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		if(!isReady)
		{
			promise.SetReadyFunction(IsFutureReady);
		}
		return !isReady;
	}

	template <typename U = tRet, typename std::enable_if_t<!std::is_void<U>::value>* = nullptr>
	auto await_resume()
	{
		return m_future.get(); // Re-throws any exceptions
	}

	template <typename U = tRet, typename std::enable_if_t<std::is_void<U>::value>* = nullptr>
	void await_resume()
	{
		m_future.get(); // Re-throws any exceptions
	}

private:
	std::future<tRet> m_future;
};

//--- Shared Future Awaiter ---//
template <typename tRet, typename promise_type>
struct SharedFutureAwaiter
{
	SharedFutureAwaiter(const std::shared_future<tRet>& in_sharedFuture)
		: m_sharedFuture(in_sharedFuture)
	{
	}
	bool await_ready() noexcept
	{
		bool isReady = m_sharedFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		return isReady;
	}
	bool await_suspend(std::coroutine_handle<promise_type> in_coroHandle) noexcept
	{
		// Set the ready function
		auto& promise = in_coroHandle.promise();
		auto IsFutureReady = [this] {
			return m_sharedFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		};

		// Suspend if future is not ready
		bool isReady = m_sharedFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		if(!isReady)
		{
			promise.SetReadyFunction(IsFutureReady);
		}
		return !isReady;
	}

	template <typename U = tRet, typename std::enable_if_t<!std::is_void<U>::value>* = nullptr>
	auto await_resume()
	{
		return m_sharedFuture.get(); // Re-throws any exceptions
	}

	template <typename U = tRet, typename std::enable_if_t<std::is_void<U>::value>* = nullptr>
	void await_resume()
	{
		m_sharedFuture.get(); // Re-throws any exceptions
	}

private:
	std::shared_future<tRet> m_sharedFuture;
};

//--- TaskPromiseBase ---//
template <typename tRet>
class TaskPromiseBase
{
public:
	// Type aliases
	using promise_type = TaskPromise<tRet>;
	using tTaskInternal = TaskInternal<tRet>;

	// Destructor
	~TaskPromiseBase()
	{
		// NOTE: Destructor is non-virtual, because it is always handled + destroyed as its concrete type
		m_taskInternal->OnTaskPromiseDestroyed();
	}

	// Coroutine interface functions
	auto initial_suspend() noexcept
	{
		return std::suspend_always();
	}
	auto final_suspend() noexcept
	{
		return std::suspend_always();
	}
	auto get_return_object()
	{
		return std::coroutine_handle<promise_type>::from_promise(*static_cast<promise_type*>(this));
	}
	static std::shared_ptr<tTaskInternal> get_return_object_on_allocation_failure()
	{
		SQUID_THROW(std::bad_alloc(), "Failed to allocate memory for Task");
		return {};
	}
	void unhandled_exception() noexcept
	{
#if SQUID_USE_EXCEPTIONS
		// Propagate exceptions for handling
		m_taskInternal->SetUnhandledException(std::current_exception());
#endif //SQUID_USE_EXCEPTIONS
	}

	// Internal Task
	void SetInternalTask(tTaskInternal* in_taskInternal)
	{
		m_taskInternal = in_taskInternal;
	}
	tTaskInternal* GetInternalTask()
	{
		return m_taskInternal;
	}
	const tTaskInternal* GetInternalTask() const
	{
		return m_taskInternal;
	}

	// Ready Function
	void SetReadyFunction(const tTaskReadyFn& in_taskReadyFn)
	{
		m_taskInternal->SetReadyFunction(in_taskReadyFn);
	}

	// Await-Transforms
	auto await_transform(Suspend in_awaiter)
	{
		return in_awaiter;
	}
	auto await_transform(std::suspend_never in_awaiter)
	{
		return in_awaiter;
	}

#if SQUID_ENABLE_TASK_DEBUG
	auto await_transform(SetDebugName in_awaiter)
	{
		m_taskInternal->SetDebugName(in_awaiter.m_name);
		m_taskInternal->SetDebugDataFn(in_awaiter.m_dataFn);
		return std::suspend_never();
	}
#endif //SQUID_ENABLE_TASK_DEBUG

	template <typename tInnerRet, eTaskRef RefType, eTaskResumable Resumable>
	auto await_transform(AddStopTaskAwaiter<tInnerRet, RefType, Resumable> in_awaiter)
	{
		m_taskInternal->AddStopTask(*in_awaiter.m_taskToStop);
		return std::suspend_never();
	}

	template <typename tInnerRet, eTaskRef RefType, eTaskResumable Resumable>
	auto await_transform(RemoveStopTaskAwaiter<tInnerRet, RefType, Resumable> in_awaiter)
	{
		m_taskInternal->RemoveStopTask(*in_awaiter.m_taskToStop);
		return std::suspend_never();
	}

	auto await_transform(GetStopContext in_awaiter)
	{
		struct GetStopContextAwaiter : public std::suspend_never
		{
			GetStopContextAwaiter(StopContext in_stopCtx)
				: stopCtx(in_stopCtx)
			{
			}
			auto await_resume() noexcept
			{
				return stopCtx;
			}
			StopContext stopCtx;
		};
		GetStopContextAwaiter stopCtxAwaiter{ m_taskInternal->GetStopContext() };
		return stopCtxAwaiter;
	}
	auto await_transform(const tTaskReadyFn& in_taskReadyFn)
	{
		// Check if we are already ready, and suspend if we are not
		bool isReady = in_taskReadyFn();
		if(!isReady)
		{
			m_taskInternal->SetReadyFunction(in_taskReadyFn);
		}
		return SuspendIf(!isReady); // Suspend if the function isn't already ready
	}

	template <typename tFutureRet>
	auto await_transform(std::future<tFutureRet>&& in_future)
	{
		return FutureAwaiter<tFutureRet, promise_type>(std::move(in_future));
	}

	template <typename tFutureRet>
	auto await_transform(const std::shared_future<tFutureRet>& in_sharedFuture)
	{
		return SharedFutureAwaiter<tFutureRet, promise_type>(in_sharedFuture);
	}

	// Task Await-Transforms
	template <typename tTaskRet, eTaskRef RefType, eTaskResumable Resumable,
		typename std::enable_if_t<Resumable == eTaskResumable::Yes>* = nullptr>
		auto await_transform(Task<tTaskRet, RefType, Resumable>&& in_task) // Move version
	{
		return TaskAwaiter<tTaskRet, RefType, Resumable, promise_type>(std::move(in_task));
	}

	template <typename tTaskRet, eTaskRef RefType, eTaskResumable Resumable,
		typename std::enable_if_t<Resumable == eTaskResumable::No>* = nullptr>
		auto await_transform(Task<tTaskRet, RefType, Resumable> in_task) // Copy version (Non-Resumable)
	{
		return TaskAwaiter<tTaskRet, RefType, Resumable, promise_type>(std::move(in_task));
	}

	template <typename tTaskRet, eTaskRef RefType, eTaskResumable Resumable,
		typename std::enable_if_t<Resumable == eTaskResumable::Yes>* = nullptr>
		auto await_transform(const Task<tTaskRet, RefType, Resumable>& in_task) // Invalid copy version (Resumable)
	{
		static_assert(static_false<tTaskRet>::value, "Cannot await a non-copyable (resumable) Task by copy (try co_await std::move(task), co_await WeakTaskHandle(task), or co_await task.WaitUntilDone()");
		return TaskAwaiter<tTaskRet, RefType, Resumable, promise_type>(std::move(in_task));
	}

protected:
	tTaskInternal* m_taskInternal = nullptr;
};

//--- TaskPromise ---//
template <typename tRet>
class TaskPromise : public TaskPromiseBase<tRet>
{
public:
	// Return value access
	void return_value(const tRet& in_retVal) // Copy return value
	{
		this->m_taskInternal->SetReturnValue(in_retVal);
	}
	void return_value(tRet&& in_retVal) // Move return value
	{
		this->m_taskInternal->SetReturnValue(std::move(in_retVal));
	}
};

template <>
class TaskPromise<void> : public TaskPromiseBase<void>
{
public:
	void return_void()
	{
	}
};

//--- TaskInternalBase ---//
class TaskInternalBase
{
public:
	TaskInternalBase(std::coroutine_handle<> in_coroHandle)
		: m_coroHandle(in_coroHandle)
	{
		SQUID_RUNTIME_CHECK(m_coroHandle, "Invalid coroutine handle passed into Task");
	}
	~TaskInternalBase() // NOTE: Destructor is intentionally non-virtual (shared_ptr preserves concrete type via deleter)
	{
		Kill(); // Used for killing subtasks
	}
	StopContext GetStopContext() const
	{
		return { &m_isStopRequested };
	}
	bool IsStopRequested() const
	{
		return m_isStopRequested;
	}
	void RequestStop() // Propagates a request for the task to come to a 'graceful' stop
	{
		m_isStopRequested = true;
		for(auto& stopTask : m_stopTasks)
		{
			if(auto locked = stopTask.lock())
			{
				locked->RequestStop();
			}
		}
		m_stopTasks.clear();
	}
	template <typename tRet, eTaskRef RefType, eTaskResumable Resumable>
	void AddStopTask(Task<tRet, RefType, Resumable>& in_taskToStop) // Adds a task to the list of tasks to which we propagate stop requests
	{
		if(m_isStopRequested)
		{
			in_taskToStop.RequestStop();
		}
		else if(in_taskToStop.IsValid())
		{
			m_stopTasks.push_back(in_taskToStop.GetInternalTask());
		}
	}
	template <typename tRet, eTaskRef RefType, eTaskResumable Resumable>
	void RemoveStopTask(Task<tRet, RefType, Resumable>& in_taskToStop) // Removes a task to the list of tasks to which we propagate stop requests
	{
		if(in_taskToStop.IsValid())
		{
			for(size_t i = 0; i < m_stopTasks.size(); ++i)
			{
				if(m_stopTasks[i].lock() == in_taskToStop.GetInternalTask())
				{
					m_stopTasks[i] = m_stopTasks.back();
					m_stopTasks.pop_back();
					return;
				}
			}
		}
	}
	eTaskStatus Resume() // Returns whether the task is still running
	{
		// Make sure this task is not already mid-resume
		SQUID_RUNTIME_CHECK(m_internalState != eInternalState::Resuming, "Attempted to resume Task while already resumed");

		// Task is destroyed, therefore task is done
		if(m_internalState == eInternalState::Destroyed)
		{
			return eTaskStatus::Done;
		}

		// Mark task as resuming
		m_internalState = eInternalState::Resuming;

		// Resume any active sub-task
		if(m_subTaskInternal)
		{
			// Propagate any stop requests to sub-task prior to resuming
			if(m_isStopRequested)
			{
				m_subTaskInternal->m_isStopRequested = true;
			}

			// Resume the sub-task
			if(m_subTaskInternal->Resume() != eTaskStatus::Done)
			{
				m_internalState = eInternalState::Idle;
				return eTaskStatus::Suspended; // Sub-task not done, therefore task is not done
			}

			// Clear the sub-task
			m_subTaskInternal = nullptr;
		}

		// Resume task, if necessary
		if(CanResume())
		{
			m_taskReadyFn = nullptr; // Clear any ready function we were waiting on
			m_coroHandle.resume(); // Resume the underlying std::coroutine_handle
		}

		// Return to idle state and return current task status
		auto taskStatus = m_coroHandle.done() ? eTaskStatus::Done : eTaskStatus::Suspended;
		if(taskStatus == eTaskStatus::Done)
		{
			m_isDone = true; // Mark task done
		}
		m_internalState = eInternalState::Idle;
		return taskStatus;
	}

	// Sub-task
	void SetSubTask(std::shared_ptr<TaskInternalBase> in_subTaskInternal)
	{
		m_subTaskInternal = in_subTaskInternal;
	}

#if SQUID_ENABLE_TASK_DEBUG
	// Debug task name + stack
	std::string GetDebugName() const
	{
		return (!IsDone() && m_debugDataFn) ? (std::string(m_debugName) + " [" + m_debugDataFn() + "]") : m_debugName;
	}
	std::string GetDebugStack() const
	{
		std::string result = m_subTaskInternal ? (GetDebugName() + " -> " + m_subTaskInternal->GetDebugStack()) : GetDebugName();
		return result;
	}
	void SetDebugName(const char* in_debugName)
	{
		if(in_debugName)
		{
			m_debugName = in_debugName;
		}
	}
	void SetDebugDataFn(std::function<std::string()> in_debugDataFn)
	{
		m_debugDataFn = in_debugDataFn;
	}
#endif //SQUID_ENABLE_TASK_DEBUG

	// Exceptions
#if SQUID_USE_EXCEPTIONS
	std::exception_ptr GetUnhandledException() const
	{
		if(m_isExceptionSet)
		{
			return m_exception;
		}
		return nullptr;
	}
#endif //SQUID_USE_EXCEPTIONS

protected:
#if SQUID_USE_EXCEPTIONS
	// Internal implementation of exception-setting (called by TaskInternal<> child classes)
	void InternalSetUnhandledException(std::exception_ptr in_exception)
	{
		// NOTE: This must never be called more than once in the lifetime of an internal task
		SQUID_RUNTIME_CHECK(!m_isExceptionSet, "Exception was set for a task after it had already been set");
		if(!m_isExceptionSet)
		{
			m_exception = in_exception;
			m_isExceptionSet = true;
		}
	}
#endif //SQUID_USE_EXCEPTIONS

private:
	template <typename tRet> friend class TaskPromiseBase;
	template <typename tRet, eTaskRef RefType, eTaskResumable Resumable, typename promise_type> friend struct TaskAwaiterBase;
	template <typename tRet, eTaskRef RefType, eTaskResumable Resumable> friend class Task;

	// Kill this task
	void Kill() // Kill() can safely be called multiple times
	{
		SQUID_RUNTIME_CHECK(m_internalState != eInternalState::Resuming, "Attempted to kill Task while resumed");
		if(m_internalState == eInternalState::Idle)
		{
			// Mark task done
			m_isDone = true;

			// First destroy any sub-tasks
			if(m_subTaskInternal)
			{
				m_subTaskInternal->Kill();
			}

			// Destroy the underlying std::coroutine_handle
			m_coroHandle.destroy(); // This should only ever be called directly from this one place
			m_coroHandle = nullptr;
			m_taskReadyFn = nullptr; // Clear out the ready function
			m_internalState = eInternalState::Destroyed;
		}
	}

	// Done + can-resume status 
	void SetReadyFunction(const tTaskReadyFn& in_taskReadyFn)
	{
		m_taskReadyFn = in_taskReadyFn;
	}
	bool CanResume() const
	{
		if(IsDone())
		{
			return false;
		}
		if(m_subTaskInternal)
		{
			bool canResume = m_subTaskInternal->CanResume();
			return canResume;
		}
		bool isReady = !m_taskReadyFn || m_taskReadyFn();
		return isReady;
	}
	bool IsDone() const
	{
		return m_isDone;
	}
	bool m_isDone = false;

	// Internal state
	enum class eInternalState
	{
		Idle,
		Resuming,
		Destroyed,
	};
	eInternalState m_internalState = eInternalState::Idle;

	// Task ready condition (when awaiting a std::function<bool>)
	tTaskReadyFn m_taskReadyFn;

#if SQUID_USE_EXCEPTIONS
	// Exceptions
	std::exception_ptr m_exception = nullptr;
	bool m_isExceptionSet = false;
#endif //SQUID_USE_EXCEPTIONS

	// Sub-task
	std::shared_ptr<TaskInternalBase> m_subTaskInternal;

	// Reference-counting (determines underlying std::coroutine_handle lifetime, not lifetime of this internal task)
	void AddLogicalRef()
	{
		++m_refCount;
	}
	void RemoveLogicalRef()
	{
		--m_refCount;
		if(m_refCount == 0)
		{
			Kill();
		}
	}
	int32_t m_refCount = 0; // Number of (strong) non-weak tasks referencing the internal task

	// C++ std::coroutine_handle
	std::coroutine_handle<> m_coroHandle;

	// Stop request
	bool m_isStopRequested = false;
	std::vector<std::weak_ptr<TaskInternalBase>> m_stopTasks;

#if SQUID_ENABLE_TASK_DEBUG
	// Debug Data
	const char* m_debugName = "[unnamed task]";
	std::function<std::string()> m_debugDataFn;
#endif //SQUID_ENABLE_TASK_DEBUG
};

//--- TaskInternal ---//
template <typename tRet>
class TaskInternal : public TaskInternalBase
{
public:
	using promise_type = TaskPromise<tRet>;

	TaskInternal(std::coroutine_handle<promise_type> in_handle)
		: TaskInternalBase(in_handle)
	{
		auto& promisePtr = in_handle.promise();
		promisePtr.SetInternalTask(this);
	}
#if SQUID_USE_EXCEPTIONS
	void SetUnhandledException(std::exception_ptr in_exception)
	{
		m_retValState = eTaskRetValState::Orphaned; // Return value can never be set if there was an unhandled exception
		InternalSetUnhandledException(in_exception);
	}
#endif //SQUID_USE_EXCEPTIONS
	void SetReturnValue(const tRet& in_retVal)
	{
		tRet retVal = in_retVal;
		SetReturnValue(std::move(retVal));
	}
	void SetReturnValue(tRet&& in_retVal)
	{
		if(m_retValState == eTaskRetValState::Unset)
		{
			m_retVal = std::move(in_retVal);
			m_retValState = eTaskRetValState::Set;
			return;
		}

		// These conditions should (logically) never be met, but they are included in case future changes violate that constraint
		SQUID_RUNTIME_CHECK(m_retValState != eTaskRetValState::Set, "Attempted to set a task's return value when it was already set");
		SQUID_RUNTIME_CHECK(m_retValState != eTaskRetValState::Taken, "Attempted to set a task's return value after it was already taken");
		SQUID_RUNTIME_CHECK(m_retValState != eTaskRetValState::Orphaned, "Attempted to set a task's return value after it was orphaned");
	}
	std::optional<tRet> TakeReturnValue()
	{
		// If the value has been set, mark it as taken and move-return the value
		if(m_retValState == eTaskRetValState::Set)
		{
			m_retValState = eTaskRetValState::Taken;
			return std::move(m_retVal);
		}

		// If the value was not set, return an unset optional (checking that it was neither taken nor orphaned)
		SQUID_RUNTIME_CHECK(m_retValState != eTaskRetValState::Taken, "Attempted to take a task's return value after it was already successfully taken");
		SQUID_RUNTIME_CHECK(m_retValState != eTaskRetValState::Orphaned, "Attempted to take a task's return value that will never be set (task ended prematurely)");
		return {};
	}
	void OnTaskPromiseDestroyed()
	{
		// Mark the return value as orphaned if it was never set
		if (m_retValState == eTaskRetValState::Unset)
		{
			m_retValState = eTaskRetValState::Orphaned;
		}
	}

private:
	// Internal state
	enum class eTaskRetValState
	{
		Unset, // Has not yet been set
		Set, // Has been set and can be taken
		Taken, // Has been taken and can no longer be taken
		Orphaned, // Will never be set because the TaskPromise has been destroyed
	};

	eTaskRetValState m_retValState = eTaskRetValState::Unset; // Initially unset
	std::optional<tRet> m_retVal;
};

template <>
class TaskInternal<void> : public TaskInternalBase
{
public:
	using promise_type = TaskPromise<void>;

	TaskInternal(std::coroutine_handle<promise_type> in_handle)
		: TaskInternalBase(in_handle)
	{
		auto& promisePtr = in_handle.promise();
		promisePtr.SetInternalTask(this);
	}
#if SQUID_USE_EXCEPTIONS
	void SetUnhandledException(std::exception_ptr in_exception)
	{
		InternalSetUnhandledException(in_exception);
	}
#endif //SQUID_USE_EXCEPTIONS
	void TakeReturnValue() // This function is an intentional no-op, to simplify certain templated function implementations
	{
	}
	void OnTaskPromiseDestroyed()
	{
	}
};
