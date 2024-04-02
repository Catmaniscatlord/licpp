#include <uchar.h>

#include <cstring>
#include <format>
#include <iostream>
#include <memory>
#include <ncpp/Direct.hh>
#include <ncpp/NotCurses.hh>
#include <string_view>

#include "parser.hpp"

std::wstring
PromptInput(ncpp::NotCurses &ncurses, std::shared_ptr<ncpp::Plane> plane);

void PrintInput(std::shared_ptr<ncpp::Plane> plane,
				std::vector<token_t> input,
				const uint indent);

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
			{
				if (!(bpos % line_size))
					y--;
				bpos--;
			}
		}
		else if (ni.id == NCKEY_RIGHT)
		{
			if (bpos < buf.size())
			{
				bpos++;
				if (!(bpos % line_size))
					y++;
			}
		}
		else if (ni.id == NCKEY_UP)
		{
			if (bpos >= line_size)
			{
				bpos -= line_size;
				y--;
			}
		}
		else if (ni.id == NCKEY_DOWN)
		{
			if (bpos + line_size < buf.size())
			{
				bpos += line_size;
				y++;
			}
		}
		// Ignore Tabs for now
		// TODO: add tab support
		// note: thise will require rewritting the cursors positioning code.
		else if (ni.id == NCKEY_TAB)
		{
			continue;
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
		/* const uint num_lines{static_cast<uint>((buf.size() - 1) /
		 * line_size)}; */
		const uint cypos{static_cast<uint>(bpos / (line_size))};
		plane->erase();
		plane->printf(y - cypos, 0, "> ");
		auto tokens = ParseTokens(buf);

		PrintInput(plane, tokens.first, buf_indent);
		/* plane->cursor_move(y, buf_indent + cxpos); */
		ncurses.cursor_enable(y, buf_indent + cxpos);
		ncurses.render();
	}
	return buf;
}

void PrintInput(std::shared_ptr<ncpp::Plane> plane,
				std::vector<token_t> input,
				const uint indent)
{
	uint x, y;
	plane->get_cursor_yx(y, x);

	const uint dimx{plane->get_dim_x()};
	const uint line_size{dimx - indent};

	// begin index
	uint bi{0};
	const static auto kQUOTE_COLOR = 0x2AC3DEu;
	const static std::array kPARA_COLORS = {0x698CD6u, 0x68B3DEu, 0x9A7ECCu,
											0x25AAC2u, 0x80A856u, 0xCFA25Fu};
	// number of unmatched paranthesis positive is unmatched left negative is
	// unmatched right
	int para_count{0};
	for (token_t &tok : input)
	{
		auto str = tok.pname;
		/* std::wcerr << str; */
		/* continue; */
		if (str == L"(")
		{
			if (para_count >= 0)
				plane->set_fg_rgb(
					kPARA_COLORS[para_count % kPARA_COLORS.size()]);
			para_count++;
		}
		else if (str == L")")
		{
			if (para_count > 0)
			{
				para_count--;
				plane->set_fg_rgb(
					kPARA_COLORS[para_count % kPARA_COLORS.size()]);
			}
		}
		else if (str == L"\'")
			plane->set_fg_rgb(kQUOTE_COLOR);

		// Print the token
		// line overflow
		while (bi + str.size() >= line_size)
		{
			// first part of token
			plane->putstr(
				y, indent + bi,
				std::format(L"{}", str.substr(0, line_size - bi)).c_str());
			y++;
			str.remove_prefix(line_size - bi);
			bi = 0;
		}

		plane->putstr(y, indent + bi, std::format(L"{}", str).c_str());
		bi += str.size();

		plane->set_fg_default();
	}
}
