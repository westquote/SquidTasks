#pragma once

// Squid::Tasks version (major.minor.patch)
#define SQUID_TASKS_VERSION_MAJOR 0
#define SQUID_TASKS_VERSION_MINOR 2
#define SQUID_TASKS_VERSION_PATCH 1

/// @defgroup Config Configuration
/// @brief Configuration settings for the Squid::Tasks library
/// @{

/// Enables Task debug names and callstack tracking via Task::GetDebugStack() and Task::GetDebugName()
#ifndef SQUID_ENABLE_TASK_DEBUG
#define SQUID_ENABLE_TASK_DEBUG 1
#endif

/// Switches time type (tTaskTime) from 32-bit single-precision floats to 64-bit double-precision floats
#ifndef SQUID_ENABLE_DOUBLE_PRECISION_TIME
#define SQUID_ENABLE_DOUBLE_PRECISION_TIME 0
#endif

/// Wraps a Squid:: namespace around all classes in the Squid::Tasks library
#ifndef SQUID_ENABLE_NAMESPACE
#define SQUID_ENABLE_NAMESPACE 0
#endif

/// Enables experimental (largely-untested) exception handling, and replaces all asserts with runtime_error exceptions
#ifndef SQUID_USE_EXCEPTIONS
#define SQUID_USE_EXCEPTIONS 0
#endif

/// Enables global time support(alleviating the need to specify a time stream for time - sensitive awaiters) [see @ref GetGlobalTime()]
#ifndef SQUID_ENABLE_GLOBAL_TIME
// ***************
// *** WARNING ***
// ***************
// It is generally inadvisable for game projects to define a global task time, as it assumes there is only a single time-stream.
// Within game projects, there is usually a "game time" and "real time", as well as others (such as "audio time", "unpaused time").
// Furthermore, in engines such as Unreal, a non-static world context object must be provided.

// To enable global task time, user must *also* define a GetGlobalTime() implementation (otherwise there will be a linker error)
#define SQUID_ENABLE_GLOBAL_TIME 1
#endif

/// @} end of addtogroup Config

// Namespace macros (enabled/disabled via SQUID_ENABLE_NAMESPACE)
#if SQUID_ENABLE_NAMESPACE
#define NAMESPACE_SQUID_BEGIN namespace Squid {
#define NAMESPACE_SQUID_END }
#define NAMESPACE_SQUID Squid
#else
#define NAMESPACE_SQUID_BEGIN
#define NAMESPACE_SQUID_END
#define NAMESPACE_SQUID
namespace Squid {} // Convenience to allow 'using namespace Squid' even when namespace is disabled
#endif


