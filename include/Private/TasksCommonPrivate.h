#pragma once

//--- User configuration header ---//
#include "../TasksConfig.h"

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

// Exception macros (to support environments with exceptions disabled)
#if SQUID_USE_EXCEPTIONS && (defined(__cpp_exceptions) || defined(__EXCEPTIONS))
#include <stdexcept>
#define SQUID_THROW(exception, errStr) throw exception;
#define SQUID_RUNTIME_ERROR(errStr) throw std::runtime_error(errStr);
#define SQUID_RUNTIME_CHECK(condition, errStr) if(!(condition)) throw std::runtime_error(errStr);
#else
#include <cassert>
#define SQUID_THROW(exception, errStr) assert(false && errStr);
#define SQUID_RUNTIME_ERROR(errStr) assert(false && errStr);
#define SQUID_RUNTIME_CHECK(condition, errStr) assert((condition) && errStr);
#endif //__cpp_exceptions

// Time Interface
NAMESPACE_SQUID_BEGIN
#if SQUID_ENABLE_DOUBLE_PRECISION_TIME
using tTaskTime = double;
#else
using tTaskTime = float; // Defines time units for use with the Task system
#endif //SQUID_ENABLE_DOUBLE_PRECISION_TIME
NAMESPACE_SQUID_END

// Coroutine de-optimization macros [DEPRECATED]
#ifdef _MSC_VER
#if _MSC_VER >= 1920
// Newer versions of Visual Studio (>= VS2019) compile coroutines correctly
#define COROUTINE_OPTIMIZE_OFF
#define COROUTINE_OPTIMIZE_ON
#else
// Older versions of Visual Studio had code generation bugs when optimizing coroutines (they would compile, but have incorrect runtime results)
#define COROUTINE_OPTIMIZE_OFF __pragma(optimize("", off))
#define COROUTINE_OPTIMIZE_ON  __pragma(optimize("", on))
#endif // _MSC_VER >= 1920
#else
// The Clang compiler has sometimes crashed when optimizing/inlining certain coroutines, so this macro can be used to disable inlining
#define COROUTINE_OPTIMIZE_OFF _Pragma("clang optimize off")
#define COROUTINE_OPTIMIZE_ON  _Pragma("clang optimize on")
#endif

// False type for use in static_assert() [static_assert(false, ...) -> static_assert(static_false<T>, ...)]
template<typename T>
struct static_false : std::false_type
{
};

// Determine C++ Language Version
#if defined(_MSVC_LANG)
    #define CPP_LANGUAGE_VERSION _MSVC_LANG
#elif defined(__cplusplus)
    #define CPP_LANGUAGE_VERSION __cplusplus
#else
    #define CPP_LANGUAGE_VERSION 0L
#endif

#if CPP_LANGUAGE_VERSION > 201703L // C++20 or higher
    #define HAS_CXX17 1
    #define HAS_CXX20 1
#elif CPP_LANGUAGE_VERSION > 201402L // C++17 or higher
    #define HAS_CXX17 1
    #define HAS_CXX20 0
#elif CPP_LANGUAGE_VERSION > 201103L // C++14 or higher
    #define HAS_CXX17 0
    #define HAS_CXX20 0
#else // C++11 or lower
	#error Squid::Tasks requires C++14 or higher
	#define HAS_CXX17 0
	#define HAS_CXX20 0
#endif
#undef CPP_LANGUAGE_VERSION

// C++20 Compatibility (std::coroutine)
#if HAS_CXX20 || (defined(_MSVC_LANG) && defined(__cpp_lib_coroutine)) // Standard coroutines
#include <coroutine>
#define SQUID_EXPERIMENTAL_COROUTINES 0
#else // Experimental coroutines
#if defined(__clang__) && defined(_STL_COMPILER_PREPROCESSOR)
// HACK: Some distributions of clang don't have a <experimental/coroutine> header. We only need a few symbols, so just define them ourselves
namespace std {
	namespace experimental {
		inline namespace coroutines_v1 {

			template <typename R, typename...> struct coroutine_traits {
				using promise_type = typename R::promise_type;
			};

			template <typename Promise = void> struct coroutine_handle;

			template <> struct coroutine_handle<void> {
				static coroutine_handle from_address(void* addr) noexcept {
					coroutine_handle me;
					me.ptr = addr;
					return me;
				}
				void operator()() { resume(); }
				void* address() const { return ptr; }
				void resume() const { __builtin_coro_resume(ptr); }
				void destroy() const { __builtin_coro_destroy(ptr); }
				bool done() const { return __builtin_coro_done(ptr); }
				coroutine_handle& operator=(decltype(nullptr)) {
					ptr = nullptr;
					return *this;
				}
				coroutine_handle(decltype(nullptr)) : ptr(nullptr) {}
				coroutine_handle() : ptr(nullptr) {}
				//  void reset() { ptr = nullptr; } // add to P0057?
				explicit operator bool() const { return ptr; }

			protected:
				void* ptr;
			};

			template <typename Promise> struct coroutine_handle : coroutine_handle<> {
				using coroutine_handle<>::operator=;

				static coroutine_handle from_address(void* addr) noexcept {
					coroutine_handle me;
					me.ptr = addr;
					return me;
				}

				Promise& promise() const {
					return *reinterpret_cast<Promise*>(
						__builtin_coro_promise(ptr, alignof(Promise), false));
				}
				static coroutine_handle from_promise(Promise& promise) {
					coroutine_handle p;
					p.ptr = __builtin_coro_promise(&promise, alignof(Promise), true);
					return p;
				}
			};

			template <typename _PromiseT>
			bool operator==(coroutine_handle<_PromiseT> const& _Left,
				coroutine_handle<_PromiseT> const& _Right) noexcept
			{
				return _Left.address() == _Right.address();
			}

			template <typename _PromiseT>
			bool operator!=(coroutine_handle<_PromiseT> const& _Left,
				coroutine_handle<_PromiseT> const& _Right) noexcept
			{
				return !(_Left == _Right);
			}

			struct suspend_always {
				bool await_ready() noexcept { return false; }
				void await_suspend(coroutine_handle<>) noexcept {}
				void await_resume() noexcept {}
			};
			struct suspend_never {
				bool await_ready() noexcept { return true; }
				void await_suspend(coroutine_handle<>) noexcept {}
				void await_resume() noexcept {}
			};

		}
	}
}
#else
#include <experimental/coroutine>
#endif
namespace std // Alias experimental coroutine symbols into std namespace
{
	template <class _Promise = void>
	using coroutine_handle = experimental::coroutine_handle<_Promise>;
	using suspend_never = experimental::suspend_never;
	using suspend_always = experimental::suspend_always;
};
#define SQUID_EXPERIMENTAL_COROUTINES 1
#endif

// Determine whether our tasks need the member function "unhandled_exception()" defined or not
#if defined(_MSC_VER)
	// MSVC's rules for exceptions differ between standard + experimental coroutines
	#if SQUID_EXPERIMENTAL_COROUTINES
		// If exceptions are enabled, we must define unhandled_exception()
		#if defined(__cpp_exceptions) && __cpp_exceptions == 199711
			#define SQUID_NEEDS_UNHANDLED_EXCEPTION 1
		#else
			#define SQUID_NEEDS_UNHANDLED_EXCEPTION 0
		#endif
	#else
		// If we're using VS16.11 or newer -- or older than 16.10, we have one set of rules for standard coroutines
		#if _MSC_FULL_VER >= 192930133L || _MSC_VER < 1429L
			#define SQUID_NEEDS_UNHANDLED_EXCEPTION 1
		#else
			#if defined(__cpp_exceptions) && __cpp_exceptions == 199711
				#define SQUID_NEEDS_UNHANDLED_EXCEPTION 1
			#else
				// 16.10 has a bug with their standard coroutine implementation that creates a set of contradicting requirements
				// https://developercommunity.visualstudio.com/t/coroutine-uses-promise_type::unhandled_e/1374530
				#error Visual Studio 16.10 has a compiler bug that prevents all coroutines from compiling when exceptions are disabled and using standard C++20 coroutines or /await:strict. Please either upgrade your version of Visual Studio, or use the experimental /await flag, or enable exceptions.
			#endif
		#endif
	#endif
#else
// Clang always requires unhandled_exception() to be defined
#define SQUID_NEEDS_UNHANDLED_EXCEPTION 1
#endif

// C++17 Compatibility ([[nodiscard]])
#if !defined(SQUID_NODISCARD) && defined(__has_cpp_attribute)
	#if __has_cpp_attribute(nodiscard)
	#define SQUID_NODISCARD [[nodiscard]]
	#endif
#endif
#ifndef SQUID_NODISCARD
#define SQUID_NODISCARD
#endif

#undef HAS_CXX17
#undef HAS_CXX20

// Include UE core headers
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Async/Future.h"
