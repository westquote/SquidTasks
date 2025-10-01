#pragma once

/// @defgroup Tasks Tasks
/// @brief Coroutine-based task handles for controlling execution and lifetime management
/// @copydoc Task

 /// @defgroup Time Time Streams
 /// @brief Time-stream functionality for time-sensitive awaiters

 /// @defgroup Awaiters Awaiters
 /// @brief Versatile task awaiters that offer utility to most projects

#include <functional>
#include <future>
#include <memory>
#include <string>

//--- User configuration header ---//
#include "TasksConfig.h"

//--- Debug Macros ---//
#if SQUID_ENABLE_TASK_DEBUG
/// @ingroup Tasks
/// @brief Macro that instruments a task with a debug name string. Usually at the top of every task coroutine as @c TASK_NAME(__FUNCTION__)
#define TASK_NAME(...) co_await NAMESPACE_SQUID::SetDebugName(__VA_ARGS__);
#define DEBUG_STR , std::string in_debugStr
#define PASS_DEBUG_STR , in_debugStr
#define MANUAL_DEBUG_STR(debugStr) , debugStr
#define WaitUntilImpl(...) _WaitUntil(__VA_ARGS__, #__VA_ARGS__)
#define WaitWhileImpl(...) _WaitWhile(__VA_ARGS__, #__VA_ARGS__)
#ifndef IN_DOXYGEN
#define WaitUntil(...) WaitUntilImpl(__VA_ARGS__)
#define WaitWhile(...) WaitWhileImpl(__VA_ARGS__)
#endif //IN_DOXYGEN
#else
#define TASK_NAME(...)
#define DEBUG_STR
#define PASS_DEBUG_STR
#define MANUAL_DEBUG_STR(...)
#ifndef IN_DOXYGEN
#define WaitUntil(...) _WaitUntil(__VA_ARGS__)
#define WaitWhile(...) _WaitWhile(__VA_ARGS__)
#endif //IN_DOXYGEN
#endif //SQUID_ENABLE_TASK_DEBUG

NAMESPACE_SQUID_BEGIN

/// @addtogroup Tasks
/// @{

//--- Task Reference Type ---//
enum class eTaskRef /// Whether a handle references a task using a strong or weak reference
{
	Strong, ///< Handle will keep the task alive (so long as there exists a valid Resumable handle)
	Weak, ///< Handle will not the task alive
};

//--- Task Resumable Type ---//
enum class eTaskResumable /// Whether a handle can be resumed (all live tasks have exactly one resumable handle and 0+ non-resumable handles)
{
	Yes, ///< Handle is resumable
	No, ///< Handle is not resumable
};

//--- Task Status ---//
enum class eTaskStatus /// Status of a task (whether it is currently suspended or done)
{
	Suspended, ///< Task is currently suspended
	Done, ///< Task has terminated and coroutine frame has been destroyed
};

//--- tTaskCancelFn ---//
using tTaskCancelFn = std::function<bool()>; ///< CancelIf/StopIf condition function type

// Forward declarations
template <typename tRet, eTaskRef RefType, eTaskResumable Resumable>
class Task; /// Templated handle type (defaults to <void, Strong, Resumable>)
template <typename tRet = void>
using TaskHandle = Task<tRet, eTaskRef::Strong, eTaskResumable::No>; ///< Non-resumable handle that holds a strong reference to a task
using WeakTask = Task<void, eTaskRef::Weak, eTaskResumable::Yes>; ///< Resumable handle that holds a weak reference to a task (always void return type)
using WeakTaskHandle = Task<void, eTaskRef::Weak, eTaskResumable::No>; ///< Non-resumable handle that holds a weak reference to a task (always void return type)

/// @} end of addtogroup Tasks

/// @addtogroup Awaiters
/// @{

//--- Suspend Awaiter ---//
/// Awaiter class that suspends unconditionally
struct Suspend : public std::suspend_always
{
};

//--- Stop Context ---//
/// Context for a task's stop requests (undefined behavior if used after the underlying task is destroyed)
struct StopContext
{
	bool IsStopRequested() const
	{
		return *m_isStoppedPtr;
	}

protected:
	friend class TaskInternalBase;
	StopContext(const bool* in_isStoppedPtr)
		: m_isStoppedPtr(in_isStoppedPtr)
	{
	}

private:
	const bool* m_isStoppedPtr = nullptr;
};

//--- GetStopContext Awaiter ---//
/// Awaiter class that immediately (without suspending) yields a stop context
struct GetStopContext
{
};

/// @} end of addtogroup Awaiters

//--- Internal Implementation Header ---//
#include "Private/TaskPrivate.h" // Internal use only! Do not move or include elsewhere!

/// @addtogroup Tasks
/// @{

//--- Task ---//
/// Task is a high-level task handle used to manage the lifetime and execution of an underlying coroutine
/// @details
/// 
/// Handle Types
/// ------------
/// The Task class is actually a template class that implements 4 user-level handle types:
/// 
/// - \ref Task<tRet>
///		- A resumable task handle that holds a strong reference to the underlying coroutine.
///		- Should be the return type of every coroutine you write.
///		- Can be used to resume tasks, kill tasks, check if they are done, and access a return value.
///		- Is permitted to have void return type (e.g. Task<>), which disables return value access.
/// - \ref WeakTask
///		- A resumable task handle that holds a weak reference to the underlying coroutine.
///		- Can only be used to resume tasks, kill tasks, and check if they are done.
/// - \ref TaskHandle<tRet>
///		- A non-resumable task handle that holds a strong reference to the underlying coroutine.
///		- Can be used to kill tasks, check if they are done, and access a return value.
///		- Is permitted to have void return type (e.g. TaskHandle<>), which disables return value access.
/// - \ref WeakTaskHandle
///		- A non-resumable task handle that holds a weak reference to the underlying coroutine.
///		- Can only be used to kill tasks and check if they are done.
/// 
/// Handle Type         | Return Type | Resumable? | Ref Strength
/// ------------------- | ----------- | ---------- | ------------------
/// \ref Task	        | <any type>  | Yes        | Strong
/// \ref WeakTask       | void        | Yes        | Weak
/// \ref TaskHandle     | <any type>  | No         | Strong
/// \ref WeakTaskHandle | void        | No         | Weak
/// 
/// Conversion Rules
/// ----------------
/// 
/// It is possible to convert between these 4 types, but not all conversions are permitted. The rules for conversion are:
/// - A conversion may remove resumability (e.g. \ref Task<tRet> -> \ref TaskHandle<tRet>), but cannot restore it
/// - A conversion may remove return type (e.g. \ref Task<tRet> -> \ref Task<>), but cannot restore it
/// - A conversion may remove reference strength (e.g. \ref Task<tRet> -> \ref WeakTask), but cannot restore it
/// 
/// In simpler terms, this means: __a handle can always convert to a handle type with fewer capabilities, but not vice-versa__.
/// 
/// Generally-speaking, it would be unsafe to convert in such a way that would add handle properties, hence the motivation for these
/// conversion rules. Care has been taken, however, to provide clear human-readable compile-time error messages if and when an
/// invalid conversion is attempted in code.
/// 
/// Resumability
/// ------------
/// 
/// For a given coroutine instance, it is impossible to have more than a single resumable handle that references it at runtime.
/// We refer to this as the "single-resumer rule". Because both Task and WeakTask are move-only types that cannot be copy-constructed
/// or copy-assigned from other handles, this guarantees at compile-time that there will never be two handles that are able to resume
/// the same underlying coroutine. This compile-time guarantee was implemented after many insidious bugs emerged in gameplay code
/// written using early versions of the Squid::Tasks library.
/// 
/// When a task's single-resumer handle is destroyed, the task is immediately killed. If a coroutine were able to remain suspended
/// without the possibility of ever being resumed again, then any task waiting for it to terminate would deadlock. For this reason,
/// Squid::Tasks enforces that all coroutines must have a valid resumable handle at all times, otherwise they are immediately killed.
/// 
/// (If you are unfamiliar with what it meant by "move-only type", we recommend you research "C++ move semantics" to familiarize
/// yourself.)
/// 
/// Lifetime Management
/// -------------------
/// 
/// The default lifetime of a Task's underlying coroutine is determined by the handles the refer to it:
/// - The underlying coroutine will be killed immediately if it is no longer referenced by a resumable handle ( \ref Task/ \ref WeakTask).
/// - The underlying coroutine will be killed immediately if it ever has zero strong references remaining to it ( \ref Task/ \ref TaskHandle).
/// 
/// This lifetime management model is essentially the same as a strong-pointer/weak-pointer model, with the added constraint that
/// tasks are killed as soon as they can no longer logically be resumed.
/// 
/// @tparam tRet Return type of the underlying coroutine (can be void if the coroutine does not co_return a value)
/// @tparam RefType Whether this handle holds a strong or weak reference to the underlying coroutine
/// @tparam Resumable Whether this handle can be used to resume the underlying coroutine
template <typename tRet = void, eTaskRef RefType = eTaskRef::Strong, eTaskResumable Resumable = eTaskResumable::Yes>
class Task
{
public:
	/// @cond
	using tTaskInternal = TaskInternal<tRet>;
	using promise_type = TaskPromise<tRet>;
	/// @endcond

#define NONVOID_ONLY template <typename U = tRet, typename std::enable_if_t<!std::is_void<U>::value>* = nullptr>

	// Prohibit illegal task types
	static_assert(RefType == eTaskRef::Strong || std::is_void<tRet>::value, "Illegal task type (cannot combine weak reference type with non-void return type");

	Task() /// Default constructor (constructs an invalid handle)
	{
	}
	Task(std::nullptr_t) /// Null-pointer constructor (constructs an invalid handle)
	{
	}
	Task(std::shared_ptr<tTaskInternal> in_taskInternal) /// @private
		: m_taskInternal(in_taskInternal)
	{
		AddRef();
	}
	Task(std::coroutine_handle<promise_type> in_coroHandle) /// @private
		: m_taskInternal(std::make_shared<tTaskInternal>(in_coroHandle))
	{
		AddRef();
	}
	Task(const Task& in_otherTask) /// Copy constructor (TaskHandle/WeakTaskHandle only)
		: m_taskInternal(in_otherTask.GetInternalTask())
	{
		static_assert(IsCopyable(), "Cannot copy-construct Task/WeakTask (only TaskHandle/WeakTaskHandle)");
		AddRef();
	}
	Task(Task&& in_otherTask) noexcept /// Move constructor
		: m_taskInternal(std::move(in_otherTask.m_taskInternal))
	{
		// NOTE: No need to alter logical reference here (this is a move)
	}
	Task& operator=(std::nullptr_t) noexcept /// Null-pointer assignment operator (makes the handle invalid)
	{
		RemoveRef(); // Remove logical reference from old internal task
		m_taskInternal = nullptr;
		return *this;
	}
	Task& operator=(const Task& in_otherTask) /// Copy assignment operator (TaskHandle/WeakTaskHandle only)
	{
		static_assert(IsCopyable(), "Cannot copy-assign Task/WeakTask (only TaskHandle/WeakTaskHandle)");
		RemoveRef(); // Remove logical reference from our current internal task
		m_taskInternal = in_otherTask.m_taskInternal;
		AddRef();
		return *this;
	}
	Task& operator=(Task&& in_otherTask) noexcept /// Move assignment operator
	{
		// If the internal task that we're about to move over can never be resumed again, kill it immediately
		KillIfResumable();
		RemoveRef(); // Remove logical reference from old internal task
		// NOTE: No need to add logical reference here (this is a move)
		m_taskInternal = std::move(in_otherTask.m_taskInternal);
		return *this;
	}
	~Task() /// Destructor
	{
		RemoveRef(); // Remove logical reference task

		// If the internal task can never be resumed again, kill it immediately
		KillIfResumable();
	}
	bool IsValid() const /// Returns whether the underlying coroutine is valid
	{
		return m_taskInternal.get();
	}
	operator bool() const /// Conversion-to-bool that yields whether an underlying coroutine is set for the task
	{
		return IsValid();
	}
	bool IsDone() const /// Returns whether the task has terminated
	{
		return IsValid() ? m_taskInternal->IsDone() : true;
	}
	bool IsStopRequested() const /// Returns whether a stop request has been issued for the task
	{
		return IsValid() ? m_taskInternal->IsStopRequested() : true;
	}
	void RequestStop() /// Issues a request for the task to terminate gracefully as soon as possible
	{
		if(IsValid())
		{
			m_taskInternal->RequestStop(); // Tell sub-tasks to stop, as well
		}
	}
	void Kill() /// Immediately terminates the task
	{
		// NOTE: Killing a task immediately destroys the coroutine and all of the coroutine's local variables
		if(IsValid())
		{
			m_taskInternal->Kill();
		}
	}
	NONVOID_ONLY std::optional<tRet> TakeReturnValue() /// Attempts to take the task's return value (throws error if return value is either orphaned or was already taken)
	{
		SQUID_RUNTIME_CHECK(IsValid(), "Tried to retrieve return value from an invalid handle");
		return GetInternalTask()->TakeReturnValue();
	}
	eTaskStatus Resume() /// Resumes the task (Task/WeakTask only)
	{
		static_assert(IsResumable(), "Cannot call Resume() on a TaskHandle/WeakTaskHandle");
		return IsValid() ? m_taskInternal->Resume() : eTaskStatus::Done;
	}

#if SQUID_ENABLE_TASK_DEBUG
	std::string GetDebugName(std::optional<TaskDebugStackFormatter> in_formatter = {}) const /// Gets this task's debug name (use TASK_NAME to set the debug name)
	{
		const char* defaultRetVal = Resumable == eTaskResumable::Yes ? "[empty task]" : "[empty task handle]";
		auto debugName = IsValid() ? m_taskInternal->GetDebugName() : defaultRetVal;
		return in_formatter ? in_formatter.value().Format(debugName) : debugName;
	}
	std::string GetDebugStack(std::optional<TaskDebugStackFormatter> in_formatter = {}) const /// Gets this task's debug stack (use TASK_NAME to set a task's debug name)
	{
		if(IsValid())
		{
			return in_formatter ? in_formatter.value().Format(m_taskInternal->GetDebugStack()) : m_taskInternal->GetDebugStack();
		}
		return GetDebugName(in_formatter);
	}
#else
	std::string GetDebugName(std::optional<TaskDebugStackFormatter> in_formatter = {}) const /// @private
	{
		return ""; // Returns an empty string when task debug is disabled
	}
	std::string GetDebugStack(std::optional<TaskDebugStackFormatter> in_formatter = {}) const /// @private
	{
		return ""; // Returns an empty string when task debug is disabled
	}
#endif //SQUID_ENABLE_TASK_DEBUG


#if SQUID_USE_EXCEPTIONS
	std::exception_ptr GetUnhandledException() const /// Gets any unhandled exceptions thrown by the task
	{
		SQUID_RUNTIME_CHECK(IsValid(), "Tried to retrieve unhandled exception from an invalid handle");
		return m_taskInternal->GetUnhandledException();
	}
	void RethrowUnhandledException() const /// Rethrows any unhandled exceptions thrown by the task
	{
		if(auto e = m_taskInternal->GetUnhandledException())
		{
			std::rethrow_exception(e);
		}
	}
#else
	void RethrowUnhandledException() const /// @private
	{
	}
#endif //SQUID_USE_EXCEPTIONS

	// Task conversion methods
	template <typename tOtherRet>
	operator Task<tOtherRet>() const & /// @private
	{
		constexpr bool isLegalReturnTypeConversion = std::is_void<tOtherRet>::value || std::is_same<tRet, tOtherRet>::value;
		constexpr bool isLegalTypeConversion = IsStrong() && IsResumable();
		static_assert(isLegalTypeConversion, "Cannot promote WeakTask/TaskHandle/WeakTaskHandle to Task");
		static_assert(!isLegalTypeConversion || isLegalReturnTypeConversion, "Mismatched return type (invalid return type conversion)");
		static_assert(!isLegalTypeConversion || !isLegalReturnTypeConversion, "Cannot copy Task -> Task because it is non-copyable (try std::move(task))");
		return {};
	}
	template <typename tOtherRet>
	operator Task<tOtherRet>() && /// @private
	{
		constexpr bool isLegalReturnTypeConversion = std::is_void<tOtherRet>::value || std::is_same<tRet, tOtherRet>::value;
		constexpr bool isLegalTypeConversion = IsStrong() && IsResumable();
		static_assert(isLegalTypeConversion, "Cannot promote WeakTask/TaskHandle/WeakTaskHandle to Task");
		static_assert(!isLegalTypeConversion || isLegalReturnTypeConversion, "Cannot convert tasks to non-void return type (invalid return type conversion)");
		
		// Move-to-void conversion (applies to all types)
		return MoveToTask<tOtherRet, RefType, Resumable>();
	}
	operator WeakTask() const & /// @private Copy-convert to WeakTask (always illegal)
	{
		static_assert(IsResumable(), "Cannot convert TaskHandle -> WeakTask (invalid resumability conversion");
		static_assert(!IsResumable(), "Cannot copy Task -> WeakTask because it is non-copyable (try std::move(task))");
		return {};
	}
	operator WeakTask() && /// @private Move-convert to WeakTask (sometimes legal)
	{
		static_assert(IsResumable(), "Cannot convert TaskHandle -> WeakTask (invalid resumability conversion)");
		return MoveToTask<void, eTaskRef::Weak, eTaskResumable::Yes>();
	}
	operator TaskHandle<tRet>() const /// @private
	{
		static_assert(IsStrong(), "Cannot convert WeakTask/WeakTaskHandle -> TaskHandle (invalid reference-strength conversion)");
		return CopyToTask<tRet, eTaskRef::Strong, eTaskResumable::No>();
	}
	template <typename tOtherRet>
	operator TaskHandle<tOtherRet>() const /// @private
	{
		constexpr bool isLegalReturnTypeConversion = std::is_void<tOtherRet>::value || std::is_same<tRet, tOtherRet>::value;
		static_assert(IsStrong(), "Cannot convert WeakTask/WeakTaskHandle -> TaskHandle (invalid reference-strength conversion)");
		static_assert(!IsStrong() || isLegalReturnTypeConversion, "Mismatched return type (invalid return type conversion)");
		return CopyToTask<tOtherRet, eTaskRef::Strong, eTaskResumable::No>();
	}
	operator WeakTaskHandle() const /// @private
	{
		// Convert anything to a weak task handle
		return CopyToTask<void, eTaskRef::Weak, eTaskResumable::No>();
	}

	// Cancel-If Methods
	/// Returns wrapper task that kills this task when the given function returns true. Returns whether wrapped task was canceled.
	/// Task return value will be bool if wrapped task had void return type, otherwise std::optional<tRet>.
	auto CancelIf(tTaskCancelFn in_cancelFn) &&
	{
		return CancelTaskIf(std::move(*this), in_cancelFn);
	}
	/// Returns wrapper task that kills this task when a stop request is issued on it. Returns whether wrapped task was canceled.
	/// Task return value will be bool if wrapped task had void return type, otherwise std::optional<tRet>.
	auto CancelIfStopRequested() && /// 
	{
		return std::move(*this).CancelIf([this] { return IsStopRequested(); });
	}
	auto CancelIf(tTaskCancelFn in_cancelFn) & /// @private Illegal lvalue implementation
	{
		static_assert(static_false<tRet>::value, "Cannot call CancelIf() on an lvalue (try std::move(task).CancelIf())");
		return CancelTaskIf(std::move(*this), in_cancelFn);
	}
	auto CancelIfStopRequested() & /// @private Illegal lvalue implementation
	{
		static_assert(static_false<tRet>::value, "Cannot call CancelIfStopRequested() on an lvalue (try std::move(task).CancelIfStopRequested())");
		return std::move(*this).CancelIf([this] { return IsStopRequested(); });
	}

	// Stop-If Methods
	/// @brief Returns wrapper task that requests a stop on this task when the given function returns true, then waits for the task to terminate (without timeout).
	/// @details Task returns whether wrapped task was canceled. Task return value will be bool if wrapped task had void return type, otherwise std::optional<tRet>.
	auto StopIf(tTaskCancelFn in_cancelFn) && /// Returns wrapper task that requests a stop on this task when the given function returns true
	{
		return StopTaskIf(std::move(*this), in_cancelFn);
	}
	auto StopIf(tTaskCancelFn in_cancelFn) & /// @private Illegal lvalue implementation
	{
		static_assert(static_false<tRet>::value, "Cannot call StopIf() on an lvalue (try std::move(task).StopIf())");
		return StopTaskIf(std::move(*this), in_cancelFn);
	}
#if SQUID_ENABLE_GLOBAL_TIME
	/// @brief Returns wrapper task that requests a stop on this task when the given function returns true, then waits for the task to terminate (with timeout in the global time-stream).
	/// @details Task returns whether wrapped task was canceled. Task return value will be bool if wrapped task had void return type, otherwise std::optional<tRet>.
	auto StopIf(tTaskCancelFn in_cancelFn, tTaskTime in_timeout) &&
	{
		// Cannot be called unless SQUID_ENABLE_GLOBAL_TIME has been set in TasksConfig.h.
		return StopTaskIf(std::move(*this), in_cancelFn, in_timeout);
	}
	auto StopIf(tTaskCancelFn in_cancelFn, tTaskTime in_timeout) & /// @private Illegal lvalue implementation
	{
		static_assert(static_false<tRet>::value, "Cannot call StopIf() on an lvalue (try std::move(task).StopIf())");
		return StopTaskIf(std::move(*this), in_cancelFn, in_timeout);
	}
#else
	auto StopIf(tTaskCancelFn in_cancelFn, tTaskTime in_timeout) && /// @private Illegal global-time implementation
	{
		static_assert(static_false<tRet>::value, "Global task time not enabled (see SQUID_ENABLE_GLOBAL_TIME in TasksConfig.h)");
		return StopTaskIf(std::move(*this), in_cancelFn);
	}
	auto StopIf(tTaskCancelFn in_cancelFn, tTaskTime in_timeout) & /// @private Illegal lvalue implementation
	{
		static_assert(static_false<tRet>::value, "Global task time not enabled (see TasksConfig.h)");
		return StopTaskIf(std::move(*this), in_cancelFn);
	}
#endif //SQUID_ENABLE_GLOBAL_TIME
	/// @brief Returns wrapper task that requests a stop on this task when the given function returns true, then waits for the task to terminate (with timeout in a given time-stream).
	/// @details Task returns whether wrapped task was canceled. Task return value will be bool if wrapped task had void return type, otherwise std::optional<tRet>.
	template <typename tTimeFn>
	auto StopIf(tTaskCancelFn in_cancelFn, tTaskTime in_timeout, tTimeFn in_timeFn) &&
	{
		return StopTaskIf(std::move(*this), in_cancelFn, in_timeout, in_timeFn);
	}
	template <typename tTimeFn>
	auto StopIf(tTaskCancelFn in_cancelFn, tTaskTime in_timeout, tTimeFn in_timeFn) & /// @private Illegal lvalue implementation
	{
		static_assert(static_false<tRet>::value, "Cannot call StopIf() on an lvalue (try std::move(task).StopIf())");
		return StopTaskIf(std::move(*this), in_cancelFn, in_timeout, in_timeFn);
	}

private:
	/// @cond
	template <typename, eTaskRef, eTaskResumable, typename> friend struct TaskAwaiterBase;
	template <typename, eTaskRef, eTaskResumable> friend class Task;
	friend class TaskInternalBase;
	/// @endcond

	// Task Internal Storage
	std::shared_ptr<TaskInternalBase> m_taskInternal;

	// Casts the internal task storage pointer to a concrete (non-TaskInternalBase) pointer
	std::shared_ptr<tTaskInternal> GetInternalTask() const
	{
		// We can safely downcast from TaskInternalBase to TaskInternal<void>
		return std::static_pointer_cast<tTaskInternal>(m_taskInternal);
	}

	// Copy/Move Implementations
	template <typename tNewRet, eTaskRef NewRefType, eTaskResumable NewResumable>
	Task<tNewRet, NewRefType, NewResumable> CopyToTask() const
	{
		Task<tNewRet, NewRefType, NewResumable> ret;
		ret.m_taskInternal = m_taskInternal;
		ret.AddRef();
		return ret;
	}

	template <typename tNewRet, eTaskRef NewRefType, eTaskResumable NewResumable>
	Task<tNewRet, NewRefType, NewResumable> MoveToTask()
	{
		Task<tNewRet, NewRefType, NewResumable> ret;
		ret.m_taskInternal = m_taskInternal;
		ret.AddRef();
		RemoveRef();
		m_taskInternal = nullptr;
		return ret;
	}

	// Logical reference management
	void AddRef()
	{
		if(m_taskInternal)
		{
			if(RefType == eTaskRef::Strong)
			{
				m_taskInternal->AddLogicalRef();
			}
		}
	}
	void RemoveRef()
	{
		if(m_taskInternal)
		{
			if(RefType == eTaskRef::Strong)
			{
				m_taskInternal->RemoveLogicalRef();
			}
		}
	}
	void KillIfResumable()
	{
		if (IsResumable() && IsValid())
		{
			Kill();
		}
	}

	// Constexpr Helpers
	static constexpr bool IsResumable()
	{
		return Resumable == eTaskResumable::Yes;
	}
	static constexpr bool IsStrong()
	{
		return RefType == eTaskRef::Strong;
	}
	static constexpr bool IsCopyable()
	{
		return !IsResumable();
	}

	#undef NONVOID_ONLY
};

/// @} end of addtogroup Tasks

/// @addtogroup Time
/// @details
/// 
/// Time-Streams
/// ------------
/// 
/// Every game project has its own method of updating and measuring game time.  Most games feature multiple different "time-streams", such
/// as "game time", "real time", "editor time", "paused time", "audio time", etc... Because of this, the Squid::Tasks library requires each
/// time-sensitive awaiter (e.g. ```WaitSeconds()```, ```Timeout()```, etc) to be presented with a time-stream function that returns the current
/// time in the desired time-stream. By convention, these time-streams are passed as functions into the final argument of time-sensitive
/// awaiters.
/// 
/// Enabling Global Time Support
/// ----------------------------
///	
///	For less-complex projects it can be desirable to default to a "global time-stream" that removes the requirement to explicitly pass a
/// time-stream function into time-sensitive awaiters. To enable this functionality, the user must set ```SQUID_ENABLE_GLOBAL_TIME``` in
/// TasksConfig.h and implement a special function called Squid::GetTime(). Failure to define this function will result in a linker error.
///	
///	The Squid::GetTime() function should return a floating-point value representing the number of seconds since the program started running.
/// Here is an example Squid::GetTime() function implementation from within the ```main.cpp``` file of a sample project:
///	
///	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.cpp
///	NAMESPACE_SQUID_BEGIN
///	tTaskTime GetTime()
///	{
///		return (tTaskTime)TimeSystem::GetTime();
///	}
///	NAMESPACE_SQUID_END
///	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
///	
/// It is recommended to save off the current time value at the start of each game frame, returning that saved value from within ```Squid::GetTime()```.
/// The reason for this is that, within a single frame, you likely want all of the tasks to behave as if they are updating at the same time.
/// By providing the same exact time value to all Tasks that are resumed within a given update, the software is more likely to behave in a stable
/// and predictable manner.
/// @{

/// Helper function to elapsed time in a given time-stream
template <typename tTimeFn>
tTaskTime GetTimeSince(tTaskTime in_t, tTimeFn in_timeFn)
{
	return in_timeFn() - in_t;
}

#if SQUID_ENABLE_GLOBAL_TIME

/// @brief User-defined global time-stream function (must be implemented if SQUID_ENABLE_GLOBAL_TIME is set, otherwise
/// there will be a linker error)
tTaskTime GetGlobalTime();

/// Global time-stream function used internally by Squid::Tasks (requires SQUID_ENABLE_GLOBAL_TIME)
inline auto GlobalTime()
{
	return &GetGlobalTime;
}

/// Helper function to elapsed time in the global time-stream (requires SQUID_ENABLE_GLOBAL_TIME)
inline tTaskTime GetTimeSince(tTaskTime in_t)
{
	return GetTimeSince(in_t, GlobalTime());
}
#else
template <typename T = void>
tTaskTime GetTimeSince(tTaskTime in_t)
{
	static_assert(static_false<T>::value, "Global task time not enabled (see SQUID_ENABLE_GLOBAL_TIME in TasksConfig.h)");
	return tTaskTime(0);
}
#endif //SQUID_ENABLE_GLOBAL_TIME

/// @} end of addtogroup Time

/// @addtogroup Awaiters
/// @{

//--- All/Any/Select Tasks ---//
/// @private
struct TaskWrapper
{
public:
	TaskWrapper(Task<> in_task) : task(std::move(in_task)) {}
	~TaskWrapper() {}
	Task<> task;

	template <typename tRet>
	static std::shared_ptr<TaskWrapper> Wrap(Task<tRet> in_task)
	{
		return std::make_shared<TaskWrapper>(std::move(in_task));
	}
	template <typename tReadyFn>
	static std::shared_ptr<TaskWrapper> Wrap(tReadyFn in_readyFn)
	{
		auto task = [](tReadyFn in_readyFn) -> Task<> { co_await in_readyFn; }(in_readyFn);
		return std::make_shared<TaskWrapper>(std::move(task));
	}
};

/// @private
struct TaskSingleEntry
{
	template <typename tRet>
	TaskSingleEntry(Task<tRet> in_task)
		: taskWrapper(TaskWrapper::Wrap(std::move(in_task)))
	{
	}
	template <typename tReadyFn>
	TaskSingleEntry(tReadyFn in_readyFn)
		: taskWrapper(TaskWrapper::Wrap(in_readyFn))
	{
	}
	auto Resume()
	{
		return taskWrapper->task.Resume();
	}
	std::shared_ptr<TaskWrapper> taskWrapper;
};

/// @private
template <typename tValue>
struct TaskSelectEntry
{
	template <typename tRet>
	TaskSelectEntry(tValue in_value, Task<tRet> in_task)
		: value(in_value)
		, taskWrapper(TaskWrapper::Wrap(std::move(in_task)))
	{
	}
	template <typename tReadyFn>
	TaskSelectEntry(tValue in_value, tReadyFn in_readyFn)
		: value(in_value)
		, taskWrapper(TaskWrapper::Wrap(in_readyFn))
	{
	}
	auto Resume()
	{
		return taskWrapper->task.Resume();
	}
	auto GetValue()
	{
		return value;
	}
	tValue value;
	std::shared_ptr<TaskWrapper> taskWrapper;
};

/// @cond
#define TASK_NAME_ENTRIES(name, entries) \
	TASK_NAME(name, [entries]() { \
		std::string debugStr; \
		for(auto entry : entries) \
		{ \
			debugStr += debugStr.size() ? "\n" : "\n`"; \
			debugStr += entry.taskWrapper->task.GetDebugStack(); \
		} \
		debugStr += "`\n"; \
		return debugStr; \
	});

#define TASK_NAME_ENTRIES_ALL(name, entries) \
	TASK_NAME(name, [entries]() { \
		std::string debugStr; \
		for(auto entry : entries) \
		{ \
			debugStr += debugStr.size() ? "\n" : "\n`"; \
			debugStr += entry.taskWrapper->task.GetDebugStack() + (entry.taskWrapper->task.IsDone() ? " [DONE]" : " [RUNNING]"); \
		} \
		debugStr += "`\n"; \
		return debugStr; \
	});
/// @endcond

/// Awaiter task that manages a set of other awaiters and waits until at least one of them is done
inline Task<> WaitForAny(std::vector<TaskSingleEntry> in_entries)
{
	TASK_NAME_ENTRIES(__FUNCTION__, in_entries);

	for(auto& entry : in_entries)
	{
		co_await AddStopTask(entry.taskWrapper->task); // Setup stop-request propagation
	}

	while(true)
	{
		for(auto& entry : in_entries)
		{
			if(entry.Resume() == eTaskStatus::Done)
			{
				co_return;
			}
		}
		co_await Suspend();
	}
}

/// Awaiter task that manages a set of other awaiters and waits until all of them are done
inline Task<> WaitForAll(std::vector<TaskSingleEntry> in_entries)
{
	TASK_NAME_ENTRIES_ALL(__FUNCTION__, in_entries);

	for(auto& entry : in_entries)
	{
		co_await AddStopTask(entry.taskWrapper->task); // Setup stop-request propagation
	}

	while(true)
	{
		bool allDone = true;
		for(auto& entry : in_entries)
		{
			if(entry.Resume() != eTaskStatus::Done)
			{
				allDone = false;
			}
		}
		if(allDone)
		{
			co_return; // Done!
		}
		co_await Suspend();
	}
}

/// Awaiter task that behaves like WaitForAny(), but returns a value associated with whichever awaiter finishes first
template<class tValue>
Task<tValue> Select(std::vector<TaskSelectEntry<tValue>> in_entries)
{
	TASK_NAME_ENTRIES(__FUNCTION__, in_entries);

	for(auto& entry : in_entries)
	{
		co_await AddStopTask(entry.taskWrapper->task); // Setup stop-request propagation
	}

	while(true)
	{
		for(size_t i = 0; i < in_entries.size(); ++i)
		{
			if(in_entries[i].Resume() == eTaskStatus::Done)
			{
				co_return in_entries[i].GetValue();
			}
		}
		co_await Suspend();
	}
	co_return tValue{};
}

#ifdef IN_DOXYGEN
/// Awaiter function that waits until a given functor returns true
inline Task<> WaitUntil(tTaskReadyFn in_readyFn) {}

/// Awaiter function that waits until a given functor returns false
inline Task<> WaitWhile(tTaskReadyFn in_readyFn) {}
#endif // IN_DOXYGEN

inline Task<> _WaitUntil(tTaskReadyFn in_readyFn DEBUG_STR) /// @private
{
	TASK_NAME("WaitUntil", [debugStr = FormatDebugString(in_debugStr)]{ return debugStr; });

	co_await in_readyFn; // Wait until the ready functor returns true
}
inline Task<> _WaitWhile(tTaskReadyFn in_readyFn DEBUG_STR) /// @private
{
	TASK_NAME("WaitWhile", [debugStr = FormatDebugString(in_debugStr)]{ return debugStr; });

	co_await[&in_readyFn]{ return !in_readyFn(); }; // Wait until the ready function returns false
}

/// Awaiter function that waits forever (only for use in tasks that will be killed externally)
inline Task<> WaitForever()
{
	return _WaitUntil([]() { return false; } MANUAL_DEBUG_STR("WaitForever"));
}

/// Awaiter function that waits N seconds in a given time-stream
template <typename tTimeFn>
Task<tTaskTime> WaitSeconds(tTaskTime in_seconds, tTimeFn in_timeFn)
{
	auto startTime = in_timeFn();
	TASK_NAME(__FUNCTION__, [in_timeFn, startTime, in_seconds] { return std::to_string(GetTimeSince(startTime, in_timeFn)) + "/" + std::to_string(in_seconds); });

	auto IsTimerUp = [in_timeFn, startTime, in_seconds] {
		return GetTimeSince(startTime, in_timeFn) >= in_seconds;
	};
	co_await IsTimerUp; // Wait until the timer is up
	co_return in_timeFn() - startTime - in_seconds;
}

/// Awaiter function that wraps a given task, canceling it after N seconds in a given time-stream. Returns whether it timed-out or not.
template <typename tRet, typename tTimeFn>
auto Timeout(Task<tRet>&& in_task, tTaskTime in_seconds, tTimeFn in_timeFn)
{
	auto IsTimerUp = [in_timeFn, startTime = in_timeFn(), in_seconds]{
		return GetTimeSince(startTime, in_timeFn) >= in_seconds;
	};
	return CancelTaskIf(std::move(in_task), IsTimerUp);
}

/// Awaiter function that calls a given function after N seconds in a given time-stream
template <typename tFn, typename tTimeFn>
Task<> DelayCall(tTaskTime in_delaySeconds, tFn in_fn, tTimeFn in_timeFn)
{
	TASK_NAME(__FUNCTION__);

	// Call function after N seconds
	co_await WaitSeconds(in_delaySeconds, in_timeFn);
	in_fn();
}

#if SQUID_ENABLE_GLOBAL_TIME
/// Awaiter function that waits N seconds in the global time-stream (requires SQUID_ENABLE_GLOBAL_TIME)
inline Task<tTaskTime> WaitSeconds(tTaskTime in_seconds)
{
	return WaitSeconds(in_seconds, GlobalTime());
}

/// Awaiter function that wraps a given task, canceling it after N seconds in the global time-stream. Returns whether it timed-out or not. (requires SQUID_ENABLE_GLOBAL_TIME)
template <typename tRet>
auto Timeout(Task<tRet>&& in_task, tTaskTime in_seconds)
{
	return Timeout(std::move(in_task), in_seconds, GlobalTime());
}

/// Awaiter function that calls a given function after N seconds in the global time-stream (requires SQUID_ENABLE_GLOBAL_TIME)
template <typename tFn>
Task<> DelayCall(tTaskTime in_delaySeconds, tFn in_fn)
{
	return DelayCall(in_delaySeconds, in_fn, GlobalTime());
}
#else
template <typename T = void>
Task<tTaskTime> WaitSeconds(tTaskTime in_seconds) /// @private Illegal global-time implementation
{
	static_assert(static_false<T>::value, "Global task time not enabled (see SQUID_ENABLE_GLOBAL_TIME in TasksConfig.h)");
	return Task<tTaskTime>{};
}
template <typename tRet, typename T = void>
auto Timeout(Task<tRet>&& in_task, tTaskTime in_seconds) /// @private Illegal global-time implementation
{
	static_assert(static_false<T>::value, "Global task time not enabled (see SQUID_ENABLE_GLOBAL_TIME in TasksConfig.h)");
	return Task<>{};
}
template <typename tFn, typename T = void>
static Task<> DelayCall(tTaskTime in_delaySeconds, tFn in_fn) /// @private Illegal global-time implementation
{
	static_assert(static_false<T>::value, "Global task time not enabled (see SQUID_ENABLE_GLOBAL_TIME in TasksConfig.h)");
	return Task<>{};
}
#endif //SQUID_ENABLE_GLOBAL_TIME

//--- Cancel-If Implementation ---//
template <typename tRet>
Task<std::optional<tRet>> CancelIfImpl(Task<tRet> in_task, tTaskCancelFn in_cancelFn) /// @private
{
	TASK_NAME("CancelIf", [taskHandle = TaskHandle<tRet>(in_task)]{ return taskHandle.GetDebugStack(); });

	co_await AddStopTask(in_task); // Setup stop-request propagation

	while(true)
	{
		if(in_cancelFn && in_cancelFn())
		{
			co_return{};
		}
		auto taskStatus = in_task.Resume();
		if(taskStatus == eTaskStatus::Done)
		{
			co_return in_task.TakeReturnValue();
		}
		co_await Suspend();
	}
	co_return{};
}
inline Task<bool> CancelIfImpl(Task<> in_task, tTaskCancelFn in_cancelFn) /// @private
{
	TASK_NAME("CancelIf", [taskHandle = TaskHandle<>(in_task)]{ return taskHandle.GetDebugStack(); });

	co_await AddStopTask(in_task); // Setup stop-request propagation

	while(true)
	{
		if(in_cancelFn && in_cancelFn())
		{
			co_return false;
		}
		auto taskStatus = in_task.Resume();
		if(taskStatus == eTaskStatus::Done)
		{
			co_return true;
		}
		co_await Suspend();
	}
	co_return false;
}
template <typename tRet, eTaskRef RefType, eTaskResumable Resumable>
auto CancelTaskIf(Task<tRet, RefType, Resumable>&& in_task, tTaskCancelFn in_cancelFn) /// @private
{
	static_assert(RefType == eTaskRef::Strong && Resumable == eTaskResumable::Yes, "Cannot call CancelIf() on WeakTask, TaskHandle or WeakTaskHandle");
	return CancelIfImpl(std::move(in_task), in_cancelFn);
}

//--- Stop-If Implementation ---//
template <typename tRet, typename tTimeFn>
Task<std::optional<tRet>> StopIfImpl(Task<tRet> in_task, tTaskCancelFn in_cancelFn, std::optional<tTaskTime> in_timeout, tTimeFn in_timeFn) /// @private
{
	TASK_NAME("StopIf", [taskHandle = TaskHandle<tRet>(in_task), in_timeout]{
		return std::string("timeout = ") + (in_timeout ? std::to_string(in_timeout.value()) : "none") + ", task = " + taskHandle.GetDebugStack();
		});

	co_await AddStopTask(in_task); // Setup stop-request propagation

	while(true)
	{
		if(!in_task.IsStopRequested() && in_cancelFn && in_cancelFn())
		{
			in_task.RequestStop();
			if(in_timeout.has_value())
			{
				co_return co_await Timeout(std::move(in_task), in_timeout.value(), in_timeFn);
			}
		}
		auto taskStatus = in_task.Resume();
		if(taskStatus == eTaskStatus::Done)
		{
			co_return in_task.TakeReturnValue();
		}
		co_await Suspend();
	}
}
template <typename tTimeFn>
Task<bool> StopIfImpl(Task<> in_task, tTaskCancelFn in_cancelFn, std::optional<tTaskTime> in_timeout, tTimeFn in_timeFn) /// @private
{
	TASK_NAME("StopIf", [taskHandle = TaskHandle<>(in_task), in_timeout]{
		return std::string("timeout = ") + (in_timeout ? std::to_string(in_timeout.value()) : "none") + ", task = " + taskHandle.GetDebugStack();
		});

	co_await AddStopTask(in_task); // Setup stop-request propagation

	while(true)
	{
		if(!in_task.IsStopRequested() && in_cancelFn && in_cancelFn())
		{
			in_task.RequestStop();
			if(in_timeout)
			{
				co_return co_await Timeout(std::move(in_task), in_timeout.value(), in_timeFn);
			}
		}
		auto taskStatus = in_task.Resume();
		if(taskStatus == eTaskStatus::Done)
		{
			co_return true;
		}
		co_await Suspend();
	}
	co_return false;
}
template <typename tRet, eTaskRef RefType, eTaskResumable Resumable>
auto StopTaskIf(Task<tRet, RefType, Resumable>&& in_task, tTaskCancelFn in_cancelFn) /// @private
{
	return StopIfImpl(std::move(in_task), in_cancelFn, {}, (float(*)())nullptr);
}

#if SQUID_ENABLE_GLOBAL_TIME
template <typename tRet, eTaskRef RefType, eTaskResumable Resumable>
auto StopTaskIf(Task<tRet, RefType, Resumable>&& in_task, tTaskCancelFn in_cancelFn, tTaskTime in_timeout) /// @private
{
	return StopIfImpl(std::move(in_task), in_cancelFn, in_timeout, GlobalTime()); // Default time function to global-time
}
#else
template <typename tRet, eTaskRef RefType, eTaskResumable Resumable>
auto StopTaskIf(Task<tRet, RefType, Resumable>&& in_task, tTaskCancelFn in_cancelFn, tTaskTime in_timeout) /// @private Illegal global-time implementation
{
	static_assert(static_false<tRet>::value, "Global task time not enabled (see SQUID_ENABLE_GLOBAL_TIME in TasksConfig.h)");
	return StopIfImpl(std::move(in_task), in_cancelFn, in_timeout, nullptr); // Default time function to global-time
}
#endif //SQUID_ENABLE_GLOBAL_TIME

template <typename tRet, eTaskRef RefType, eTaskResumable Resumable, typename tTimeFn>
auto StopTaskIf(Task<tRet, RefType, Resumable>&& in_task, tTaskCancelFn in_cancelFn, tTaskTime in_timeout, tTimeFn in_timeFn) /// @private
{
	// See forward-declaration for default arguments
	static_assert(RefType == eTaskRef::Strong && Resumable == eTaskResumable::Yes, "Cannot call StopIf() on WeakTask, TaskHandle or WeakTaskHandle");
	return StopIfImpl(std::move(in_task), in_cancelFn, in_timeout, in_timeFn);
}

/// @} end of addtogroup Tasks

NAMESPACE_SQUID_END
