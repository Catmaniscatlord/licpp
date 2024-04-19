#include <cstdio>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <ncpp/NotCurses.hh>
#include <ranges>

#include "interpreter.hpp"
#include "parser.hpp"

std::wstring
PromptInput(ncpp::NotCurses &ncurses, std::shared_ptr<ncpp::Plane> plane);

void PrintInput(std::shared_ptr<ncpp::Plane> plane,
				std::vector<token_t> input,
				const uint indent);

void PrintWelcome(std::shared_ptr<ncpp::Plane> plane);

int main(void)
{
	// setup basic options
	notcurses_options opts{
		.termtype = nullptr,
		.loglevel{},
		.margin_t{},
		.margin_r{},
		.margin_b{},
		.margin_l{},
		.flags = NCOPTION_CLI_MODE | NCOPTION_SUPPRESS_BANNERS};

	ncpp::NotCurses ncurses{opts};
	std::shared_ptr<ncpp::Plane> command_plane(ncurses.get_stdplane());

	std::wofstream output(
		"debug.txt", std::ios_base::trunc | std::ios_base::out);

	std::wcout.rdbuf(output.rdbuf());

	uint y, x;
	command_plane->get_cursor_yx(y, x);
	auto c{command_plane->content(0, 0, y, x)};
	command_plane->putstr(c);
	ncurses.refresh({}, {});
	ncurses.render();
	PrintWelcome(command_plane);

	for (int i = 0; i < 5; i++)
	{
		std::wstring command = PromptInput(ncurses, command_plane);

		std::vector<token_t> tokens = ParseEvalTokens(command).first;

		Interpreter *interp{Interpreter::getInstance()};
		auto res{interp->eval(tokens[0])};

		for (auto i : interp->get_error())
		{
			std::wcout << i.what() << std::endl;
			auto j{i.get_token()};
			std::wcout << j.pname << " " << j.val;
			std::wcout << std::endl;
		}

		{
			auto i{res};
			std::wcout << "type: ";
			switch (i.type)
			{
				case TOKEN_TYPE::DELIM:
					std::wcout << "DELIM";
					break;
				case TOKEN_TYPE::LIST:
					std::wcout << "LIST :";
					for (auto &j :
						 i.apval | std::ranges::views::filter(
									   [](token_t t)
									   { return t.type != TOKEN_TYPE::DELIM; }))
						std::wcout << j.pname;
					std::wcout << std::endl;
					break;
				case TOKEN_TYPE::SYMBOL:
					std::wcout << "SYMBOL";
					break;
				case TOKEN_TYPE::INT:
					std::wcout << "INT";
					std::wcout << "\t value: " << i.val;
					break;
				case TOKEN_TYPE::BOOL:
					std::wcout << "BOOL";
					std::wcout << i.pname;
					break;
				case TOKEN_TYPE::LAMBDA:
					std::wcout << "LAMBDA";
					break;
			}
			std::wcout << "\tpname: ";
			std::wcout << i.pname;
			std::wcout << std::endl;
			output.flush();
		}
		std::wcout << std::endl << std::endl << "=============" << std::endl;
	}
	output.close();
};

void PrintWelcome(std::shared_ptr<ncpp::Plane> plane)
{
	auto msg{
		"        __     _                      \n"
		"       / /    (_)_____ ____   ____    \n"
		"      / /    / // ___// __ \\ / __ \\ \n"
		"     / /___ / // /__ / /_/ // /_/ /   \n"
		"    /_____//_/ \\___// .___// .___/   \n"
		"                   /_/    /_/         \n"
		"======================================\n"
		" A terminal based interpreter for lisp\n"
		"======================================\n\n\n"};

	plane->cursor_move(-1, 0);
	plane->putstr(msg);
}

std::wstring
PromptInput(ncpp::NotCurses &ncurses, std::shared_ptr<ncpp::Plane> plane)
{
	uint x, y;
	plane->get_cursor_yx(y, x);
	const uint dimx = plane->get_dim_x();
	plane->putstr(y, 0, "> ");
	const uint buf_indent = 2;
	ncurses.cursor_enable(y, 2);
	ncurses.render();
	// size of "> "
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
		{
			plane->putstr("\n");
			ncurses.refresh(y, x);
			return buf;
		}
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
		const uint cypos{static_cast<uint>(bpos / (line_size))};
		ncplane_erase_region(plane->to_ncplane(), y - cypos + 1, 0, INT_MAX, 0);
		plane->printf(y - cypos, 0, "> ");
		auto tokens = ParsePrintTokens(buf);

		PrintInput(plane, tokens.first, buf_indent);
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
