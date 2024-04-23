#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <ncpp/NotCurses.hh>
#include <string_view>

#include "interpreter.hpp"
#include "parser.hpp"

// Colors stolen from tokyo night
const static auto kDEFAULT_COLOR = 0x7982A9u;
const static auto kINFO_COLOR = 0xFFDB69u;
const static auto kERROR_COLOR = 0xDB4B4Bu;
const static auto kINT_COLOR = 0xFF9E64u;
const static auto kSYMBOL_COLOR = 0x7DCFFFu;
const static auto kBOOL_COLOR = 0xFF9E64u;
const static auto kQUOTE_COLOR = 0x2AC3DEu;
const static std::array kPARA_COLORS = {0x698CD6u, 0x68B3DEu, 0x9A7ECCu,
										0x25AAC2u, 0x80A856u, 0xCFA25Fu};

// A global which stores the command history of the users,
// this allows you to re-run previous commands, or correct typos
static std::vector<std::wstring> cmd_hist{};

std::wstring
PromptInput(ncpp::NotCurses &ncurses, std::shared_ptr<ncpp::Plane> plane);

void PrintParseTokens(std::shared_ptr<ncpp::Plane> plane,
					  std::vector<parse_token_t> input,
					  const uint indent = 0);

void PrintWelcome(std::shared_ptr<ncpp::Plane> plane);

int main(void)
{
	// Create the not curses instance, for use in handeling user input/output.
	// It makes things pretty
	notcurses_options opts{
		.termtype = nullptr,
		.loglevel{},
		.margin_t{},
		.margin_r{},
		.margin_b{},
		.margin_l{},
		.flags = NCOPTION_NO_ALTERNATE_SCREEN | NCOPTION_SCROLLING |
				 NCOPTION_SUPPRESS_BANNERS};

	ncpp::NotCurses ncurses{opts};
	std::shared_ptr<ncpp::Plane> command_plane(ncurses.get_stdplane());
	command_plane->set_fg_rgb(kDEFAULT_COLOR);

	// change where wcout goes, to save interpreters output to a file
	std::wofstream output(
		"output.txt", std::ios_base::trunc | std::ios_base::out);

	// store the previous buffer to be swapped back too, stops a memory leak
	auto cout_buf = std::wcout.rdbuf(output.rdbuf());

	// grab the singelton interpreter
	Interpreter *interp{Interpreter::getInstance()};

	// Print hte welcom screen
	PrintWelcome(command_plane);
	ncurses.refresh({}, {});
	ncurses.render();

	//
	// Main command loop
	while (true)
	{
		// Grab the useres input
		std::wstring command{PromptInput(ncurses, command_plane)};

		// Parse the input and check if there were parsing errors
		auto parse_res{ParseEvalTokens(command)};
		if (parse_res.second.err != ParserError::Exception::NONE)
		{
			command_plane->set_fg_rgb(kERROR_COLOR);
			command_plane->putstr("PARSE ERROR: ");
			command_plane->set_fg_rgb(kDEFAULT_COLOR);
			command_plane->putstr(parse_res.second.what());
			command_plane->putstr(L"\n");
			ncurses.render();
			ncurses.refresh({}, {});
			continue;
		}

		// Gets the tokens supplied from the parser
		std::vector<token_t> tokens{parse_res.first};

		// Evaluates each token that the user supplied
		for (auto &i : tokens)
		{
			// clear any previous errors before we evaluate
			interp->clear_error();
			auto res{interp->eval(i)};

			// check that there were no evaluation errors
			if (interp->get_error().empty())
			{
				// cast the result to a wstring, for printing purposes
				auto str_res{static_cast<std::wstring>(res)};

				// Output the result to the output file
				std::wcout << str_res << std::endl;

				// Render the output to a the REPL plane
				// We parse the string in as parse tokens so we can reuse our
				// PrintParseTokens function for syntax highlighting
				auto parsed_for_output = ParsePrintTokens(str_res);
				assert(parsed_for_output.second.err ==
					   ParserError::Exception::NONE);
				command_plane->set_fg_rgb(0xA9B1D6);
				command_plane->putstr(L"res> ");
				command_plane->set_fg_rgb(kDEFAULT_COLOR);

				PrintParseTokens(command_plane, parsed_for_output.first, 5);
				command_plane->putstr(L"\n");
				ncurses.render();
				ncurses.refresh({}, {});
			}
			else
			{
				// Error handeling
				// the quit function is handeled as an error
				if (interp->get_error().front().err_ ==
					EvalError::Exception::QUIT)
				{
					goto exit;
				}
				command_plane->set_fg_rgb(kERROR_COLOR);
				command_plane->putstr("EVAL ERROR: ");
				command_plane->set_fg_rgb(kDEFAULT_COLOR);
				auto error{interp->get_error().front()};
				command_plane->putstr(error.what());
				command_plane->putstr(L"\n");

				command_plane->set_fg_rgb(kINFO_COLOR);
				command_plane->putstr("TOKEN : ");
				command_plane->set_fg_rgb(kDEFAULT_COLOR);
				// Similar to outputting on a nonerror, but without syntax
				// highlighting
				command_plane->putstr(
					static_cast<std::wstring>(error.get_token()).c_str());
				command_plane->putstr(L"\n");
				ncurses.render();
				ncurses.refresh({}, {});
				break;
			}
		}
	}

exit:
	std::wcout.rdbuf(cout_buf);
	return EXIT_SUCCESS;
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
	plane->putstr(msg);
}

std::wstring
PromptInput(ncpp::NotCurses &ncurses, std::shared_ptr<ncpp::Plane> plane)
{
	// The current position in the command history is at the bottom
	auto cmd_hist_pos{cmd_hist.size()};
	// WE can only go through the command history if we havent made any changes
	// to the command
	bool edited{false};

	// Initial set up of creating the user prompt
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
		// crtl u deletes all input before the cursor
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
			// command history navigation
			if (!edited && cmd_hist_pos > 0)
			{
				cmd_hist_pos--;
				buf = cmd_hist[cmd_hist_pos];
			}
			// prompt navigation
			else if (bpos >= line_size)
			{
				bpos -= line_size;
				y--;
			}
		}
		else if (ni.id == NCKEY_DOWN)
		{
			// command history navigation
			if (!edited && cmd_hist_pos < cmd_hist.size() - 1)
			{
				cmd_hist_pos++;
				buf = cmd_hist[cmd_hist_pos];
			}
			// prompt navigation
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
		// other special inputs
		else if (nckey_synthesized_p(ni.id))
		{
			continue;
		}
		else
		{
			// normal input
			edited = true;
			buf.insert(bpos, 1, ni.id);
			bpos++;
		}

		// reprints the input prompt at the correct location
		// handles multi-line commands
		const uint cxpos{static_cast<uint>(bpos % line_size)};
		const uint cypos{static_cast<uint>(bpos / (line_size))};
		ncplane_erase_region(plane->to_ncplane(), y - cypos, -1, INT_MAX, 0);
		plane->printf(y - cypos, 0, "> ");

		// Render the current input with syntax highlighting
		auto tokens = ParsePrintTokens(buf);
		PrintParseTokens(plane, tokens.first, buf_indent);
		ncurses.cursor_enable(y, buf_indent + cxpos);
		ncurses.render();
	}
	return buf;
}

void PrintParseTokens(std::shared_ptr<ncpp::Plane> plane,
					  std::vector<parse_token_t> input,
					  const uint indent)

{
	uint x, y;
	plane->get_cursor_yx(y, x);
	const uint dimx{plane->get_dim_x()};
	const uint line_size{dimx - indent};

	// begin index
	uint bi{0};
	// number of unmatched paranthesis, positive is unmatched left (, negative
	// is unmatched right )
	int para_count{0};
	for (auto &tok : input)
	{
		auto str{tok.pname};
		// syntax highlighting based on the tokens type
		switch (tok.type)
		{
		case TOKEN_TYPE::DELIM:
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
			break;
		case TOKEN_TYPE::SYMBOL:
			plane->set_fg_rgb(kSYMBOL_COLOR);
			break;
		case TOKEN_TYPE::INT:
			plane->set_fg_rgb(kINT_COLOR);
			break;
		case TOKEN_TYPE::BOOL:
			plane->set_fg_rgb(kBOOL_COLOR);
			break;
		case TOKEN_TYPE::LIST:
		case TOKEN_TYPE::LAMBDA:
			assert(false);
		default:
			plane->set_fg_rgb(kDEFAULT_COLOR);
		}

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

		plane->set_fg_rgb(kDEFAULT_COLOR);
	}
}
