#pragma once

/// @defgroup FunctionGuard Function Guard
/// @brief Scope guard that calls a function as it leaves scope.
/// @{
/// 
/// A FunctionGuard is an scope guard object that stores a functor that will be called from its destructor. By
/// convention, scope guards are move-only objects that are intended for allocation on the stack, to ensure that certain
/// operations are performed exactly once (when their scope collapses).
/// 
/// Because tasks can be canceled while suspended (and thus do not reach the end of the function), any cleanup code at
/// the end of a task isn't guaranteed to execute. Because FunctionGuard is an RAII object, it gives programmers an
/// opportunity to schedule guaranteed cleanup code, no matter how a task terminates.
/// 
/// Consider the following example of a task that manages a character's "charge attack" in a combat-oriented game:
/// 
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
/// 
/// class Character : public Actor
/// {
/// public:
/// 	Task<> ChargeAttackState()
/// 	{
/// 		bool bIsFullyCharged = false;
/// 		if(Input->IsAttackButtonPressed())
/// 		{
/// 			StartCharging(); // Start playing charge effects
/// 			auto stopChargingGuard = MakeFnGuard([&]{
/// 				StopCharging(); // Stop playing charge effects
/// 			});
/// 
/// 			// Wait for N seconds (canceling if button is no longer held)
/// 			bIsFullyCharged = co_await WaitSeconds(chargeTime).CancelIf([&] {
/// 				return !Input->IsAttackButtonPressed();
/// 			});
/// 		} // <-- This is when StopCharging() will be called
/// 		FireShot(bIsFullyCharged);
/// 	}
/// };
/// 
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/// 
/// In the above example, we can guarantee that StopCharging will logically be called exactly once for every call to
/// StartCharging(), even the ChargeAttackState() task is killed or canceled. Furthermore, we know that StopCharging()
/// will always be called prior to the call to FireShot().
/// 
/// In practice, it is often desirable to create more domain-specific scope guards for specific use cases, but 
/// FunctionGuard provides a simple general-purpose tool for writing robust, water-tight coroutine logic without the
/// overhead of creating bespoke support classes.

//--- User configuration header ---//
#include "TasksConfig.h"

NAMESPACE_SQUID_BEGIN

template <typename tFn = std::function<void()>>
class FunctionGuard
{
public:
	FunctionGuard() = default; /// Default constructor
	FunctionGuard(nullptr_t) /// Null-pointer constructor
	{
	}
	FunctionGuard(tFn in_fn) /// Functor constructor
		: m_fn(std::move(in_fn))
	{
	}
	~FunctionGuard() /// Destructor
	{
		Execute();
	}
	FunctionGuard(FunctionGuard&& in_other) noexcept /// Move constructor
		: m_fn(std::move(in_other.m_fn))
	{
		in_other.Forget();
	}
	FunctionGuard& operator=(FunctionGuard<tFn>&& in_other) noexcept /// Move assignment operator
	{
		m_fn = std::move(in_other.m_fn);
		in_other.Forget();
		return *this;
	}
	FunctionGuard& operator=(nullptr_t) noexcept /// Null-pointer assignment operator (calls Forget() to clear the functor)
	{
		Forget();
		return *this;
	}
	operator bool() const /// Convenience conversion operator that calls IsBound()
	{
		return IsBound();
	}
	bool IsBound() noexcept /// Returns whether functor has been bound to this FunctionGuard
	{
		return m_fn;
	}
	void Execute() /// Executes and clears the functor (if bound)
	{
		if(m_fn)
		{
			m_fn.value()();
			Forget();
		}
	}
	void Forget() noexcept /// Clear the functor (without calling it)
	{
		m_fn.reset();
	}

private:
	std::optional<tFn> m_fn; // The function to call when this scope guard is destroyed
};

/// Create a function guard (directly stores the concretely-typed functor in the FunctionGuard)
template <typename tFn>
FunctionGuard<tFn> MakeFnGuard(tFn in_fn)
{
	return FunctionGuard<tFn>(std::move(in_fn));
}

/// Create a generic function guard (preferable when re-assigning new functor values to the same variable)
inline FunctionGuard<> MakeGenericFnGuard(std::function<void()> in_fn)
{
	return FunctionGuard<>(std::move(in_fn));
}

NAMESPACE_SQUID_END

///@} end of FunctionGuard group
