#pragma once

#include <span>
#include <string_view>

enum class TOKEN_TYPE
{
	DELIM,
	LIST,
	SYMBOL,
	INT,
	BOOL
};

struct token_t
{
	bool quoted{false};
	int val{};
	bool is_true{false};
	TOKEN_TYPE type;
	std::wstring_view pname{};
	std::span<token_t, std::dynamic_extent> apval{};
	std::span<token_t, std::dynamic_extent> expr{};
};
