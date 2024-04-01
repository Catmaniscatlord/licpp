#include <uchar.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <ncpp/Direct.hh>
#include <ncpp/NotCurses.hh>
#include <ranges>
#include <string_view>

std::wstring
PromptInput(ncpp::NotCurses &ncurses, std::shared_ptr<ncpp::Plane> plane);

void PrintInput(
	std::shared_ptr<ncpp::Plane> plane, std::wstring_view buf, uint indent);

int main(void)
{
	// setup basic options
	notcurses_options opts{.termtype = nullptr,
						   .loglevel{},
						   .margin_t{},
						   .margin_r{},
						   .margin_b{},
						   .margin_l{},
						   .flags = NCOPTION_CLI_MODE};

	ncpp::NotCurses ncurses{opts};
	std::shared_ptr<ncpp::Plane> command_plane(ncurses.get_stdplane());

	PromptInput(ncurses, command_plane);
	ncurses.render();
};

std::wstring
PromptInput(ncpp::NotCurses &ncurses, std::shared_ptr<ncpp::Plane> plane)
{
	uint x, y;
	plane->get_cursor_yx(y, x);
	const uint dimx = plane->get_dim_x();
	// size of "> "
	const uint buf_indent = 2;
	const uint line_size = dimx - buf_indent;

	std::wstring buf;
	size_t bpos = 0;  // cursor position in the buffer
	ncinput ni;

	// While user is inputting characters
	// false if there is an error in getting input
	while (ncurses.get(true, &ni) != (uint32_t)-1)
	{
		if (ni.evtype == NCTYPE_RELEASE)
			continue;
		if (ni.id == NCKEY_EOF || (ncinput_ctrl_p(&ni) && ni.id == 'D'))
			return {};
		else if (ni.id == NCKEY_ENTER)
			// TODO: Check if the inputted lisp command has all paranthesis
			// closed.
			//  then act accordingly
			return buf;
		// TODO: ctrl+action, to affect over words
		else if (ni.id == NCKEY_BACKSPACE)
		{
			if (bpos > 0 && bpos <= buf.size())
			{
				buf.erase(bpos - 1, 1);
				bpos--;
			}
		}
		else if (ni.id == 'U' && ncinput_ctrl_p(&ni))
		{
			buf.erase(0, bpos);
			bpos = 0;
		}
		else if (ni.id == NCKEY_LEFT)
		{
			if (bpos > 0)
				bpos--;
		}
		else if (ni.id == NCKEY_RIGHT)
		{
			if (bpos < buf.size())
				bpos++;
		}
		else if (ni.id == NCKEY_UP)
		{
			if (bpos > line_size)
				bpos -= line_size;
		}
		else if (ni.id == NCKEY_DOWN)
		{
			if (bpos + line_size < buf.size())
				bpos += line_size;
		}

		else if (nckey_synthesized_p(ni.id))
		{
			continue;
		}
		else
		{
			buf.insert(bpos, 1, ni.id);
			bpos++;
		}

		const uint cxpos{static_cast<uint>(bpos % line_size)};
		const uint cypos{static_cast<uint>(bpos / (line_size))};
		plane->erase();
		plane->printf(y - cypos, 0, "> ");
		// Messy logic stuff so we can fill in the line
		// TODO: refactor this

		PrintInput(plane, buf, 2);
		ncurses.cursor_enable(y, buf_indent + cxpos);
		ncurses.render();
	}
	return buf;
}

void PrintInput(std::shared_ptr<ncpp::Plane> plane,
				std::wstring_view input,
				const uint indent)
{
	uint x, y;
	plane->get_cursor_yx(y, x);

	const uint dimx{plane->get_dim_x()};
	const uint line_size{dimx - indent};

	auto get_token = [&]() -> std::wstring_view
	{
		std::wstring_view ret;

		if (input.empty())
			return ret;

		// take delimeter
		// '(', ')', ''', or ' '
		if (input.starts_with(L'(') || input.starts_with(L')') ||
			input.starts_with(L' ') || input.starts_with(L'\''))
		{
			ret = input | std::ranges::views::take(1);
			input.remove_prefix(1);
			return ret;
		}
		// take up to delimeter
		else
		{
			auto n = input.find_first_of(L"()\' ");
			if (n == input.npos)
			{
				// No more delimiting characters found
				ret = input;
				input.remove_prefix(input.size());
			}
			else
			{
				ret = input | std::ranges::views::take(n);
				input.remove_prefix(n);
			}
		}

		return ret;
	};

	// begin index
	uint bi{0};
	auto token{get_token()};
	const static auto kQUOTE_COLOR = 0xC35817u;
	const static std::array kPARA_COLORS = {0xEF476Fu, 0xFFD166u, 0x06D6A0u,
											0X118AB2u, 0X073B4Cu};
	// number of unmatched paranthesis positive is unmatched left negative is
	// unmatched right
	int para_count{0};
	while (!token.empty())
	{
		if (token == L"(")
		{
			if (para_count >= 0)
				plane->set_fg_rgb(
					kPARA_COLORS[para_count % kPARA_COLORS.size()]);
			para_count++;
		}
		else if (token == L")")
		{
			if (para_count > 0)
			{
				para_count--;
				plane->set_fg_rgb(
					kPARA_COLORS[para_count % kPARA_COLORS.size()]);
			}
		}
		else if (token == L"\'")
			plane->set_fg_rgb(kQUOTE_COLOR);

		// Print the token
		// line overflow
		if (bi + token.size() >= line_size)
		{
			while (bi + token.size() >= line_size)
			{
				// first part of token
				plane->printf(y, indent + bi, "%.*ls",
							  static_cast<int>(line_size - bi), token.data());
				y++;
				token.remove_prefix(line_size - bi);
				bi = 0;
			}
			// second part of token
			plane->printf(y, indent, "%.*ls", static_cast<int>(token.size()),
						  token.data());
			bi += token.size();
		}
		else
		{
			plane->printf(y, indent + bi, "%.*ls",
						  static_cast<int>(token.size()), token.data());
			bi += token.size();
		}

		// Next token
		token = get_token();
		plane->set_fg_default();
	}
}
