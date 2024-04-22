#pragma once

#include <string_view>
#include <vector>

#include "structs.hpp"

class ParserError
{
public:
	enum class Exception
	{
		NONE,
		NO_INPUT,
		DOUBLE_QUOTE,
		QUOTED_SPACE,
		UNMATCHED_PARANTHESIS
	};

	Exception err{Exception::NONE};
	std::pair<uint, uint> error_range_;

	ParserError() = default;

	ParserError(Exception e, std::pair<uint, uint> range)
		: err(e), error_range_(range){};

	const wchar_t* what() const
	{
		switch (err)
		{
		case Exception::UNMATCHED_PARANTHESIS:
			return L"unmatched paranthesis";
		case Exception::QUOTED_SPACE:
			return L"Trying to quote a space, \"\' \"";
		case Exception::DOUBLE_QUOTE:
			return L"Double quotes are un supported";
		case Exception::NO_INPUT:
			return L"No input given";
		case Exception::NONE:
			return L"No Error";
		default:
			return L"This shouldn't have happened";
		}
	};
};

// Returns a vector of tokens that is meant to be printed,
// Does not contain lists. This is essentially token tagging
std::pair<std::vector<parse_token_t>, ParserError>
ParsePrintTokens(std::wstring_view string);

// Returns a list of tokens to be evaluated.
std::pair<std::vector<token_t>, ParserError>
ParseEvalTokens(std::wstring_view string);
