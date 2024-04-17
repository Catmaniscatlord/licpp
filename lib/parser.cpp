#include "parser.hpp"

#include <cstddef>
#include <ranges>
#include <stack>
#include <string_view>
#include <vector>

#include "structs.hpp"

std::pair<std::vector<token_t>, ParserError>
ParseTokens(std::wstring_view input)
{
	// Used to keep track where in the input we are
	// so we can throw errors reasonable error messages on parsing
	uint input_pos{};
	ParserError err{};
	std::vector<token_t> tokens;
	// Store a stack containing the position in the array to the list token, and
	// the first token in the list
	std::stack<std::pair<size_t, size_t>> list_stack;
	// This stores all List tokens, with the position of the beginning and the
	// end of their lists
	std::vector<std::tuple<size_t, size_t, size_t>> lists;

	bool quoted;

	while (!input.empty())
		// White space characters
		if (input.starts_with(L" "))
		{
			auto n = input.find_first_not_of(L" ");

			if (n == input.npos)
				n = input.size();

			tokens.emplace_back(token_t{
				.type = TOKEN_TYPE::DELIM,
				.pname = input | std::ranges::views::take(n),
			});
			input.remove_prefix(n);
			input_pos += n;
		}
		// non white space item
		else
		{
			// Check if expression is quoted
			if (input.starts_with(L"\'"))
			{
				quoted = true;
				tokens.emplace_back(token_t{
					.type = TOKEN_TYPE::DELIM,
					.pname = input | std::ranges::views::take(1),
				});
				input.remove_prefix(1);
				input_pos++;

				if (input.starts_with(L" "))
					err = ParserError(
						ParserError::Exception::QUOTED_SPACE,
						{input_pos - 1, input.find_first_not_of(L" ")});
			}
			else
				quoted = false;

			// (
			if (input.starts_with(L"("))
			{
				// Start the list
				tokens.emplace_back(token_t{
					.quoted = quoted,
					.type = TOKEN_TYPE::LIST,
				});
				// we do size here because the '(' delimiter token will live at
				// that spot
				list_stack.emplace(tokens.size() - 1, tokens.size());

				// (
				tokens.emplace_back(token_t{
					.type = TOKEN_TYPE::DELIM,
					.pname = input | std::ranges::views::take(1),
				});
				input.remove_prefix(1);
				input_pos++;
			}
			// )
			else if (input.starts_with(L")"))
			{
				// )
				tokens.emplace_back(token_t{
					.type = TOKEN_TYPE::DELIM,
					.pname = input | std::ranges::views::take(1),
				});
				input.remove_prefix(1);
				input_pos++;

				if (!list_stack.empty())
				{
					auto list = list_stack.top();
					lists.emplace_back(list.first, list.second, tokens.size());
					list_stack.pop();
				}
			}
			else
			{
				// Currently double quotes arent supported
				if (quoted and input.starts_with(L"\'"))
					err = ParserError(ParserError::Exception::DOUBLE_QUOTE,
									  {input_pos - 1, input_pos});

				// Next character can not be ' or ( or ) or a space
				// Next token must be a symbol, a number or a boolean
				auto i = input.find_first_of(L"()\' ");
				std::wstring_view str;

				// Get the symbols string
				if (i == input.npos)
				{
					// No more delimiting characters found
					str = input;
					input.remove_prefix(input.size());
					input_pos = input.size() - 1;
				}
				else
				{
					str = input | std::ranges::views::take(i);
					input.remove_prefix(i);
					input_pos += i;
				}

				// Now convert the value to either a symbol an int or a boolean
				if (str == L"T")
				{
					tokens.emplace_back(token_t{.quoted = quoted,
												.is_true = true,
												.type = TOKEN_TYPE::BOOL,
												.pname = str});
				}
				else if (str == L"NIL")
				{
					tokens.emplace_back(token_t{.quoted = quoted,
												.is_true = false,
												.type = TOKEN_TYPE::BOOL,
												.pname = str});
				}
				try
				{
					size_t pos{};
					int x{std::stoi(std::wstring{str}, &pos)};
					if (pos != str.size())
						throw;
					tokens.emplace_back(token_t{.quoted = quoted,
												.val = x,
												.type = TOKEN_TYPE::INT,
												.pname = str});
				}
				catch (...)
				{
					// TODO: tell the user when its an overflow
					//  not convertable to an integer
					tokens.emplace_back(token_t{.quoted = quoted,
												.type = TOKEN_TYPE::SYMBOL,
												.pname = str});
				};
			}
		}

	for (auto &i : lists)
	{
		auto j = std::get<0>(i);
		tokens[j].apval = std::ranges::subrange(
			std::next(tokens.begin(), std::get<1>(i)),
			std::next(tokens.begin(), std::get<2>(i)));
	}

	return {tokens, err};
}
