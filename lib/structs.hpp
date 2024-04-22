#pragma once

#include <compare>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

enum class TOKEN_TYPE : uint8_t
{
	DELIM,
	LIST,
	SYMBOL,
	INT,
	BOOL,
	LAMBDA,
};

const wchar_t *TokenTypeToString(const TOKEN_TYPE &tt);

struct env_t;

// we template out the token type into
// parse tokens
struct parse_token_t
{
	bool quoted{false};
	bool is_true{false};
	TOKEN_TYPE type;
	std::wstring_view pname;
};

class token_t
{
public:
	int val{};
	bool quoted{false};
	bool is_true{false};
	TOKEN_TYPE type;
	std::shared_ptr<std::wstring> pname{std::make_shared<std::wstring>(L"")};

	// stores a list if its a list, if its a lambda or a function, this stores
	// the args
	std::vector<token_t> apval{};
	// This is a pointer as we have to pass a single token to be evaluated, this
	// has to be a list
	std::shared_ptr<token_t> expr{};
	std::shared_ptr<env_t> env{};

	operator std::wstring() const;

private:
	std::strong_ordering nested_check(const token_t &l, const token_t &r) const;

public:
	bool operator==(const token_t &other) const = default;

	// space ship operator nyooom nyooom
	auto operator<=>(const token_t &other) const
	{
		return nested_check(*this, other);
	};

	friend std::wostream &operator<<(std::wostream &os, const token_t &t)
	{
		return recursive_out(os, t, L"");
	}

	static std::wostream &
	recursive_out(std::wostream &os, const token_t &t, const std::wstring &pre);
};

struct env_t
{
	std::wstring env_name_{};
	std::unordered_map<std::wstring, token_t> curr_env_{};
	std::shared_ptr<env_t> next_env_{};

	std::optional<token_t> find(const token_t &token);

	friend std::wostream &operator<<(std::wostream &os, const env_t &t);

	static void formated_out(std::wostream &os,
							 const std::shared_ptr<env_t> &t,
							 const std::wstring &pre);

	friend std::wostream &
	operator<<(std::wostream &os, const std::shared_ptr<env_t> &t)
	{
		return os << *t;
	}
};
