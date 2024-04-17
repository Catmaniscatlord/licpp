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
				break;
			case Exception::QUOTED_SPACE:
				return L"Trying to quote a space, \"\' \"";
				break;
			case Exception::DOUBLE_QUOTE:
				return L"Double quotes are un supported";
				break;
			case Exception::NONE:
				return L"No Error";
				break;
		}
	};
};

// We return the error here, this function is only called once and is non
// recursive
std::pair<std::vector<token_t>, ParserError>
ParseTokens(std::wstring_view string);
