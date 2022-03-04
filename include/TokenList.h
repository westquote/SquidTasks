#pragma once

/// @defgroup Tokens Token List
/// @brief Data structure for tracking decentralized state across multiple tasks.
/// @{
/// 
/// Token objects can be created using @ref TokenList::MakeToken(), returning a shared pointer to a new Token. This
/// new Token can then be added to the TokenList using @ref TokenList::AddToken(). @ref TokenList::TakeToken() 
/// can be used to make + add a new token with a single function call.
/// 
/// Because TokenList uses weak pointers to track its elements, Token objects are logically removed from the list once
/// they are destroyed. As such, it is usually unnecessary to explicitly call @ref TokenList::RemoveToken() to remove a
/// Token from the list. Instead, it is idiomatic to consider the Token to be a sort of "scope guard" that will remove
/// itself from all TokenList objects when it leaves scope.
/// 
/// The TokenList class is included as part of Squid::Tasks to provide a simple mechanism for robustly sharing aribtrary
/// state between multiple tasks. Consider this example of a poison damage-over-time system:
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
/// 
/// class Character : public Actor
/// {
/// public:
/// 	bool IsPoisoned() const
/// 	{
/// 		return m_poisonTokens; // Whether there are any live poison tokens
/// 	}
/// 
/// 	void OnPoisoned(float in_dps, float in_duration)
/// 	{
/// 		m_taskMgr.RunManaged(ManagePoisonInstance(in_dps, in_duration));
/// 	}
/// 
/// private:
/// 	TokenList<float> m_poisonTokens; // Token list indicating live poison damage
/// 
/// 	Task<> ManagePoisonInstance(float in_dps, float in_duration)
/// 	{
/// 		// Take a poison token and hold it for N seconds
/// 		auto poisonToken = m_poisonTokens.TakeToken(__FUNCTION__, in_dps);
/// 		co_await WaitSeconds(in_duration);
/// 	}
/// 
/// 	Task<> ManageCharacter() // Called once per frame
/// 	{
/// 		while(true)
/// 		{
/// 			float poisonDps = m_poisonTokens.GetMax(); // Get highest DPS poison instance
/// 			DealDamage(poisonDps * GetDT()); // Deal the actual poison damage
/// 			co_await Suspend();
/// 		}
/// 	}
/// };
/// 
/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/// 
/// As the above example shows, this mechanism is well-suited for coroutines, as they can hold a Token across 
/// multiple frames. Also note that Token objects can optionally hold data. The TokenList class has query functions
/// (e.g. GetMin()/GetMax()) that can be used to aggregate the data from the set of live tokens. This is used above
/// to quickly find the highest DPS poison instance.

#include <algorithm>
#include <numeric>
#include <vector>
#include <string>

//--- User configuration header ---//
#include "TasksConfig.h"

NAMESPACE_SQUID_BEGIN

template <typename T = void>
class TokenList;

/// @brief Handle to a TokenList element that stores a debug name
/// @details In most circumstances, name should be set to \ref __FUNCTION__ at the point of creation.
struct Token
{
	Token(std::string in_name)
		: name(std::move(in_name))
	{
	}
	std::string name; // Used for debug only
};

/// @brief Handle to a TokenList element that stores both a debug name and associated data
/// @details In most circumstances, name should be set to \c __FUNCTION__ at the point of creation.
template <typename tData>
struct DataToken
{
	DataToken(std::string in_name, tData in_data)
		: name(std::move(in_name))
		, data(std::move(in_data))
	{
	}
	std::string name; // Used for debug only
	tData data;
};

/// Create a token with the specified debug name
inline std::shared_ptr<Token> MakeToken(std::string in_name)
{
	return std::make_shared<Token>(std::move(in_name));
}

/// Create a token with the specified debug name and associated data
template <typename tData>
std::shared_ptr<DataToken<tData>> MakeToken(std::string in_name, tData in_data)
{
	return std::make_shared<DataToken<tData>>(std::move(in_name), std::move(in_data));
}

/// @brief Container for tracking decentralized state across multiple tasks. (See \ref Tokens for more info...)
/// @tparam T Type of data to associate with each Token in this container
template <typename T>
class TokenList
{
public:
	/// Type of Token tracked by this container
	using Token = typename std::conditional_t<std::is_void<T>::value, Token, DataToken<T>>;

	/// Create a token with the specified debug name
	template <typename U = T, typename std::enable_if_t<std::is_void<U>::value>* = nullptr>
	static std::shared_ptr<Token> MakeToken(std::string in_name)
	{
		return std::make_shared<Token>(std::move(in_name));
	}

	/// Create a token with the specified debug name and associated data
	template <typename U = T, typename std::enable_if_t<!std::is_void<U>::value>* = nullptr>
	static std::shared_ptr<Token> MakeToken(std::string in_name, U in_data)
	{
		return std::make_shared<Token>(std::move(in_name), std::move(in_data));
	}

	/// Create and add a token with the specified debug name
	template <typename U = T, typename std::enable_if_t<std::is_void<U>::value>* = nullptr>
	SQUID_NODISCARD std::shared_ptr<Token> TakeToken(std::string in_name)
	{
		return AddTokenInternal(MakeToken(std::move(in_name)));
	}

	/// Create and add a token with the specified debug name and associated data
	template <typename U = T, typename std::enable_if_t<!std::is_void<U>::value>* = nullptr>
	SQUID_NODISCARD std::shared_ptr<Token> TakeToken(std::string in_name, U in_data)
	{
		return AddTokenInternal(MakeToken(std::move(in_name), std::move(in_data)));
	}

	/// Add an existing token to this container
	std::shared_ptr<Token> AddToken(std::shared_ptr<Token> in_token)
	{
		SQUID_RUNTIME_CHECK(in_token, "Cannot add null token");
		auto foundIter = std::find_if(m_tokens.begin(), m_tokens.end(), [&in_token](const std::weak_ptr<Token> in_iterToken){ 
			return in_iterToken.lock() == in_token; 
		});

		if(foundIter == m_tokens.end()) // Prevent duplicate tokens
		{
			return AddTokenInternal(in_token);
		}
		return in_token;
	}

	/// Explicitly remove a token from this container
	void RemoveToken(std::shared_ptr<Token> in_token)
	{
		// Find and remove the token
		if(m_tokens.size())
		{
			m_tokens.erase(std::remove_if(m_tokens.begin(), m_tokens.end(), [&in_token](const std::weak_ptr<Token>& in_otherToken) {
				return !in_otherToken.owner_before(in_token) && !in_token.owner_before(in_otherToken);
			}), m_tokens.end());
		}
	}

	/// Convenience conversion operator that calls HasTokens()
	operator bool() const
	{
		return HasTokens();
	}

	/// Returns whether this container holds any live tokens
	bool HasTokens() const
	{
		// Return true when holding any unexpired tokens
		for(auto i = (int32_t)(m_tokens.size() - 1); i >= 0; --i)
		{
			const auto& token = m_tokens[i];
			if(!token.expired())
			{
				return true;
			}
			m_tokens.pop_back(); // Because the token is expired, we can safely remove it from the back
		}
		return false;
	}

	/// Returns an array of all live token data
	std::vector<T> GetTokenData() const
	{
		std::vector<T> tokenData;
		for(const auto& tokenWeak : m_tokens)
		{
			if(auto token = tokenWeak.lock())
			{
				tokenData.push_back(token->data);
			}
		}
		return tokenData;
	}

	/// @name Data Queries
	/// Methods for querying and aggregating the data from the set of live tokens.
	/// @{
	
	/// Returns associated data from the least-recently-added live token
	std::optional<T> GetLeastRecent() const
	{
		Sanitize();
		return m_tokens.size() ? m_tokens.front().lock()->data : std::optional<T>{};
	}

	/// Returns associated data from the most-recently-added live token
	std::optional<T> GetMostRecent() const
	{
		Sanitize();
		return m_tokens.size() ? m_tokens.back().lock()->data : std::optional<T>{};
	}

	/// Returns smallest associated data from the set of live tokens
	std::optional<T> GetMin() const
	{
		std::optional<T> ret;
		SanitizeAndProcessData([&ret](const T& in_data) {
			if(!ret || in_data < ret.value())
			{
				ret = in_data;
			}
		});
		return ret;
	}

	/// Returns largest associated data from the set of live tokens
	std::optional<T> GetMax() const
	{
		std::optional<T> ret;
		SanitizeAndProcessData([&ret](const T& in_data) {
			if(!ret || in_data > ret.value())
			{
				ret = in_data;
			}
		});
		return ret;
	}

	/// Returns arithmetic mean of all associated data from the set of live tokens
	std::optional<double> GetMean() const
	{
		std::optional<double> ret;
		std::optional<double> total;
		SanitizeAndProcessData([&total](const T& in_data) {
			total = total.value_or(0.0) + (double)in_data;
		});
		if(total)
		{
			ret = total.value() / m_tokens.size();
		}
		return ret;
	}

	/// Returns whether the set of live tokens contains at least one token associated with the specified data
	template <typename U = T, typename std::enable_if_t<!std::is_void<U>::value>* = nullptr>
	bool Contains(const U& in_searchData) const
	{
		bool containsData = false;
		SanitizeAndProcessData([&in_searchData, &containsData](const T& in_data) {
			if(in_searchData == in_data)
			{
				containsData = true;
			}
		});
		return containsData;
	}
	///@} end of Data Queries

	/// Returns a debug string containing a list of the debug names of all live tokens
	std::string GetDebugString() const 
	{
		std::vector<std::string> tokenStrings;
		std::string debugStr;
		for(const auto& token : m_tokens) 
		{
			if(!token.expired())
			{
				if(debugStr.size() > 0)
				{
					debugStr += "\n";
				}
				debugStr += token.lock()->name;
			}
		}
		if(debugStr.size() == 0)
		{
			debugStr = "[no tokens]";
		}
		return debugStr;
	}

private:
	// Shared internal implementation for adding tokens
	std::shared_ptr<Token> AddTokenInternal(std::shared_ptr<Token> in_token)
	{
		Sanitize();
		m_tokens.push_back(in_token);
		return in_token;
	}

	// Sanitation
	void Sanitize() const
	{
		// Remove all invalid tokens
		if(m_tokens.size())
		{
			m_tokens.erase(std::remove_if(m_tokens.begin(), m_tokens.end(), [](const std::weak_ptr<Token>& in_token) {
				return in_token.expired();
			}), m_tokens.end());
		}
	}
	template <typename tFn>
	void SanitizeAndProcessData(tFn in_dataFn) const
	{
		// Remove all invalid tokens while applying a processing function on each valid token
		if(m_tokens.size())
		{
			m_tokens.erase(std::remove_if(m_tokens.begin(), m_tokens.end(), [&in_dataFn](const std::weak_ptr<Token>& in_token) {
				if(in_token.expired())
				{
					return true;
				}
				if(auto token = in_token.lock())
				{
					in_dataFn(token->data);
				}
				return false;
			}), m_tokens.end());
		}
	}

	// Token data
	mutable std::vector<std::weak_ptr<Token>> m_tokens; // Mutable so we can remove expired tokens while converting bool
};

NAMESPACE_SQUID_END

///@} end of Tokens group
