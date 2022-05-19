// WARNING: This is an internal implementation header, which must be included from a specific location/namespace
//          That is the reason that this header does not contain a #pragma once, nor namespace guards

// Helper struct representing a transition event to a new FSM state
struct TransitionEvent
{
	Task<> newTask;
	StateId newStateId;
};

// Base class for defining links between states
class LinkBase
{
public:
	virtual ~LinkBase() = default;
	virtual std::optional<TransitionEvent> EvaluateLink(const tOnStateTransitionFn& in_onTransitionFn) const = 0;
};

// Type-safe link handle
class LinkHandle
{
	bool IsOnCompleteLink() const
	{
		return m_linkType == eType::OnComplete;
	}
	bool HasCondition() const
	{
		return m_isConditionalLink;
	}

protected:
	// Link-type enum
	enum class eType
	{
		Normal,
		OnComplete,
	};

	// Friends
	template<class, class> friend class StateHandle;
	friend class NAMESPACE_SQUID::TaskFSM;

	// Constructors (friend-only)
	LinkHandle() = delete;
	LinkHandle(std::shared_ptr<LinkBase> in_link, eType in_linkType, bool in_isConditional)
		: m_link(std::move(in_link))
		, m_linkType(in_linkType)
		, m_isConditionalLink(in_isConditional)
	{
	}
	std::optional<TransitionEvent> EvaluateLink(const tOnStateTransitionFn& in_onTransitionFn) const
	{
		return m_link->EvaluateLink(in_onTransitionFn);
	}

private:
	std::shared_ptr<LinkBase> m_link; // The underlying link
	eType m_linkType; // Whether the link is normal or OnComplete
	bool m_isConditionalLink; // Whether the link has an associated condition predicate
};

// Internal FSM state object
template<class tStateInput, class tStateConstructorFn>
struct State
{
	State(tStateConstructorFn in_stateCtorFn, StateId in_stateId, std::string in_debugName)
		: stateCtorFn(in_stateCtorFn)
		, stateId(in_stateId)
		, debugName(in_debugName)
	{
	}

	tStateConstructorFn stateCtorFn;
	StateId stateId;
	std::string debugName;
};

// Internal FSM state object (exit state specialization)
template<>
struct State<void, void>
{
	State(StateId in_stateId, std::string in_debugName)
		: stateId(in_stateId)
		, debugName(in_debugName)
	{
	}

	StateId stateId;
	std::string debugName;
};

// Internal link definition object
template<class ReturnT, class tStateConstructorFn, class tPredicateFn>
class Link : public LinkBase
{
public:
	Link(std::shared_ptr<State<ReturnT, tStateConstructorFn>> in_targetState, tPredicateFn in_predicate)
	: m_targetState(std::move(in_targetState))
	, m_predicate(in_predicate)
	{
	}

private:
	virtual std::optional<TransitionEvent> EvaluateLink(const tOnStateTransitionFn& in_onTransitionFn) const final
	{
		std::optional<TransitionEvent> result;
		if(std::optional<ReturnT> payload = m_predicate())
		{
			if(in_onTransitionFn)
			{
				in_onTransitionFn();
			}
			result = TransitionEvent{ m_targetState->stateCtorFn(payload.value()), m_targetState->stateId };
		}
		return result;
	}

	std::shared_ptr<State<ReturnT, tStateConstructorFn>> m_targetState;
	tPredicateFn m_predicate;
};

// Internal link definition object (no-payload specialization)
template<class tStateConstructorFn, class tPredicateFn>
class Link<void, tStateConstructorFn, tPredicateFn> : public LinkBase
{
public:
	Link(std::shared_ptr<State<void, tStateConstructorFn>> in_targetState, tPredicateFn in_predicate)
		: m_targetState(std::move(in_targetState))
		, m_predicate(in_predicate)
	{
	}

private:
	virtual std::optional<TransitionEvent> EvaluateLink(const tOnStateTransitionFn& in_onTransitionFn) const final
	{
		std::optional<TransitionEvent> result;
		if(m_predicate())
		{
			if(in_onTransitionFn)
			{
				in_onTransitionFn();
			}
			result = TransitionEvent{ m_targetState->stateCtorFn(), m_targetState->stateId };
		}
		return result;
	}

	std::shared_ptr<State<void, tStateConstructorFn>> m_targetState;
	tPredicateFn m_predicate;
};

// Internal link definition object (exit-state specialization)
template<class tPredicateFn>
class Link<void, void, tPredicateFn> : public LinkBase
{
public:
	Link(std::shared_ptr<State<void, void>> in_targetState, tPredicateFn in_predicate)
		: m_targetState(std::move(in_targetState))
		, m_predicate(in_predicate)
	{
	}

private:
	virtual std::optional<TransitionEvent> EvaluateLink(const tOnStateTransitionFn& in_onTransitionFn) const final
	{
		std::optional<TransitionEvent> result;
		if(m_predicate())
		{
			if(in_onTransitionFn)
			{
				in_onTransitionFn();
			}
			result = TransitionEvent{ Task<>(), m_targetState->stateId };
		}
		return result;
	}

	std::shared_ptr<State<void, void>> m_targetState;
	tPredicateFn m_predicate;
};

// Specialized type traits that deduce the first argument type of an arbitrary callable type
template <typename tRet, typename tArg>
static tArg get_first_arg_type(std::function<tRet(tArg)> f); // Return type is first argument type

template <typename tRet>
static void get_first_arg_type(std::function<tRet()> f); // Return type is void (function has no arguments)

template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())> // Generic callable objects (use operator())
{
};

template <typename tRet, typename... tArgs> // Function
struct function_traits<tRet(tArgs...)>
{
	using tFunction = std::function<tRet(tArgs...)>;
	using tArg = decltype(get_first_arg_type(tFunction()));
};

template <typename tRet, typename... tArgs> // Function ptr
struct function_traits<tRet(*)(tArgs...)>
{
	using tFunction = std::function<tRet(tArgs...)>;
	using tArg = decltype(get_first_arg_type(tFunction()));
};

template <typename tClass, typename tRet, typename... tArgs> // Member function ptr (const)
struct function_traits<tRet(tClass::*)(tArgs...) const>
{
	using tFunction = std::function<tRet(tArgs...)>;
	using tArg = decltype(get_first_arg_type(tFunction()));
};

template <typename tClass, typename tRet, typename... tArgs> // Member function ptr
struct function_traits<tRet(tClass::*)(tArgs...)>
{
	using tFunction = std::function<tRet(tArgs...)>;
	using tArg = decltype(get_first_arg_type(tFunction()));
};
