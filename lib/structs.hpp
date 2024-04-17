#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_set>

enum class TOKEN_TYPE : uint8_t
{
	DELIM,
	LIST,
	SYMBOL,
	INT,
	BOOL,
	LAMBDA
};

struct env_t;

struct token_t
{
	int val{};
	bool quoted{false};
	bool is_true{false};
	TOKEN_TYPE type;
	std::wstring_view pname{};
	// stores a list if its a list, if its a lambda or a function, this stores
	// the args
	std::span<token_t> apval{};
	std::span<token_t> expr{};
	std::shared_ptr<env_t> env{};
};

struct token_hash
{
	std::size_t operator()(const token_t &t) const noexcept
	{
		return std::hash<std::wstring_view>()(t.pname);
	};
};

struct env_t
{
	std::unordered_set<token_t, token_hash> curr_env_{};
	std::shared_ptr<env_t> next_env_{};

	std::optional<token_t> find(token_t &token)
	{
		if (!curr_env_.contains(token))
		{
			if (next_env_.get() != nullptr)
				return next_env_->find(token);
			else
				return {};
		}
		return *curr_env_.find(token);
	}
};
