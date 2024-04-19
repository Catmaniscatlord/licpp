#pragma once

#include <compare>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <vector>

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
	std::vector<token_t> apval{};
	// This is a pointer as we have to pass a single token to be evaluated, this
	// has to be a list
	std::shared_ptr<token_t> expr{};
	std::shared_ptr<env_t> env{};

private:
	std::strong_ordering nested_check(const token_t &l, const token_t &r) const
	{
		if (!l.apval.empty() || !r.apval.empty())
		{
			if (l.apval.size() != r.apval.size())
				return l.apval.size() <=> r.apval.size();
			for (size_t i{0}; i < l.apval.size(); i++)
				if (nested_check(l.apval[i], r.apval[i]) != 0)
					return nested_check(l.apval[i], r.apval[i]);
		}
		return (l.is_true == r.is_true && l.type == r.type && l.pname == r.pname
					? (l.val <=> r.val)
					: std::strong_ordering::less);
	};

public:
	bool operator==(const token_t &other) const = default;

	// space ship operator nyooom nyooom
	auto operator<=>(const token_t &other) const
	{
		return nested_check(*this, other);
	};
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

	std::optional<token_t> find(const token_t &token)
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
