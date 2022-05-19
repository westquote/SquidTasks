#pragma once

//--- User configuration header ---//
#include "TasksConfig.h"

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

// Forward declarations
template <typename tRet = void, eTaskRef RefType = eTaskRef::Strong, eTaskResumable Resumable = eTaskResumable::Yes>
class Task; /// Templated handle type (defaults to <void, eTaskRef::Strong, eTaskResumable::Yes>)
template <typename tRet = void>
using TaskHandle = Task<tRet, eTaskRef::Strong, eTaskResumable::No>; ///< Non-resumable handle that holds a strong reference to a task
using WeakTask = Task<void, eTaskRef::Weak, eTaskResumable::Yes>; ///< Resumable handle that holds a weak reference to a task (always void return type)
using WeakTaskHandle = Task<void, eTaskRef::Weak, eTaskResumable::No>; ///< Non-resumable handle that holds a weak reference to a task (always void return type)

/// @} end of addtogroup Tasks

NAMESPACE_SQUID_END
