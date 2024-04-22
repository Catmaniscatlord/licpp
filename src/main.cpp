#include <cstdio>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <ncpp/NotCurses.hh>
#include <string_view>

#include "interpreter.hpp"
#include "parser.hpp"

std::wstring
PromptInput(ncpp::NotCurses &ncurses, std::shared_ptr<ncpp::Plane> plane);

void PrintInput(std::shared_ptr<ncpp::Plane> plane,
				std::vector<parse_token_t> input,
				const uint indent);

void PrintWelcome(std::shared_ptr<ncpp::Plane> plane);

int main(void)
{
	Interpreter *interp{Interpreter::getInstance()};

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

	std::wcerr.rdbuf(output.rdbuf());

	uint y, x;
	command_plane->get_cursor_yx(y, x);
	auto c{command_plane->content(0, 0, y, x)};
	command_plane->putstr(c);
	ncurses.refresh({}, {});
	ncurses.render();
	PrintWelcome(command_plane);

	while (true)
	{
		std::wstring command{PromptInput(ncurses, command_plane)};

		auto parse_res{ParseEvalTokens(command)};
		if (parse_res.second.err != ParserError::Exception::NONE)
		{
			command_plane->set_fg_rgb(0xFF0000);
			command_plane->putstr("PARSE ERROR: ");
			command_plane->set_fg_default();
			command_plane->putstr(parse_res.second.what());
			command_plane->putstr(L"\n");
			ncurses.render();
			ncurses.refresh({}, {});
			continue;
		}

		std::vector<token_t> tokens{parse_res.first};

		for (auto &i : tokens)
		{
			interp->clear_error();
			auto res{interp->eval(i)};

			if (interp->get_error().empty())
			{
				/* command_plane->putstr(L"\n"); */
				command_plane->putstr(static_cast<std::wstring>(res).c_str());
				command_plane->putstr(L"\n");
				ncurses.render();
				ncurses.refresh({}, {});
			}
			else
			{
				command_plane->set_fg_rgb(0xFF0000);
				command_plane->putstr("EVAL ERROR: ");
				command_plane->set_fg_default();
				auto error{interp->get_error().front()};
				command_plane->putstr(error.what());
				command_plane->putstr(L"\n");

				command_plane->set_fg_rgb(0xAF0000);
				command_plane->putstr("TOKEN : ");
				command_plane->set_fg_default();
				// This casting is really verbose and annoying, there has to be
				// a better way to do this
				command_plane->putstr(
					static_cast<std::wstring>(error.get_token()).c_str());
				command_plane->putstr(L"\n");
				ncurses.render();
				ncurses.refresh({}, {});
				break;
			}
		}
	}

	ncurses.stop();
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
	static std::vector<std::wstring> cmd_hist{};

	auto cmd_hist_pos{cmd_hist.size()};
	bool edited{false};

	uint x, y;
	plane->get_cursor_yx(y, x);
	const uint dimx = plane->get_dim_x();
	plane->putstr(y, 0, "> ");
	const uint buf_indent = 2;
	ncurses.cursor_enable(y, 2);
	ncurses.render();
	ncurses.refresh({}, {});
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
			cmd_hist.push_back(buf);
			plane->putstr("\n");
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
			if (!edited && cmd_hist_pos > 0)
			{
				cmd_hist_pos--;
				buf = cmd_hist[cmd_hist_pos];
			}
			else if (bpos >= line_size)
			{
				bpos -= line_size;
				y--;
			}
		}
		else if (ni.id == NCKEY_DOWN)
		{
			if (!edited && cmd_hist_pos < cmd_hist.size() - 1)
			{
				cmd_hist_pos++;
				buf = cmd_hist[cmd_hist_pos];
			}
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
			edited = true;
			buf.insert(bpos, 1, ni.id);
			bpos++;
		}

		const uint cxpos{static_cast<uint>(bpos % line_size)};
		const uint cypos{static_cast<uint>(bpos / (line_size))};
		ncplane_erase_region(plane->to_ncplane(), y - cypos, -1, INT_MAX, 0);
		plane->printf(y - cypos, 0, "> ");
		auto tokens = ParsePrintTokens(buf);

		PrintInput(plane, tokens.first, buf_indent);
		ncurses.cursor_enable(y, buf_indent + cxpos);
		ncurses.render();
	}
	return buf;
}

void PrintInput(std::shared_ptr<ncpp::Plane> plane,
				std::vector<parse_token_t> input,
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
	for (auto &tok : input)
	{
		auto str{tok.pname};
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
