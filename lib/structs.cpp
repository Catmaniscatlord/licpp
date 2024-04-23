#include "structs.hpp"

#include <cassert>
#include <compare>
#include <format>
#include <iostream>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>

inline const wchar_t *TokenTypeToString(const TOKEN_TYPE &tt)
{
	switch (tt)
	{
	case TOKEN_TYPE::LIST:
		return L"LIST";
	case TOKEN_TYPE::DELIM:
		return L"DELIM";
	case TOKEN_TYPE::SYMBOL:
		return L"SYMBOL";
	case TOKEN_TYPE::INT:
		return L"INT";
	case TOKEN_TYPE::LAMBDA:
		return L"LAMBDA";
	case TOKEN_TYPE::BOOL:
		return L"BOOL";
	default:
		return L"";
	}
}

std::strong_ordering
token_t::nested_check(const token_t &l, const token_t &r) const
{
	if (l.type != r.type)
		return std::strong_ordering::less;
	switch (l.type)
	{
	case TOKEN_TYPE::SYMBOL:
	case TOKEN_TYPE::DELIM:
		return l.pname <=> r.pname;
	case TOKEN_TYPE::LIST:
		if (!l.apval.empty() || !r.apval.empty())
		{
			if (l.apval.size() != r.apval.size())
				return l.apval.size() <=> r.apval.size();
			for (size_t i{0}; i < l.apval.size(); i++)
				if (nested_check(l.apval[i], r.apval[i]) != 0)
					return nested_check(l.apval[i], r.apval[i]);
		}
		return l.apval.size() <=> r.apval.size();
	case TOKEN_TYPE::INT:
		return l.val <=> r.val;
	case TOKEN_TYPE::BOOL:
		return l.is_true <=> r.is_true;
	case TOKEN_TYPE::LAMBDA:
		if (*l.expr == *r.expr)
		{
			if (!l.apval.empty() || !r.apval.empty())
			{
				if (l.apval.size() != r.apval.size())
					return l.apval.size() <=> r.apval.size();
				for (size_t i{0}; i < l.apval.size(); i++)
					if (nested_check(l.apval[i], r.apval[i]) != 0)
						return nested_check(l.apval[i], r.apval[i]);
			}
			return l.apval.size() <=> r.apval.size();
		}
		return (*l.expr <=> *r.expr);
	}
	return std::strong_ordering::less;
}

// This is so we have a string to print to the output, as opposed to the
// ostream<< which is meant for debugging
token_t::operator std::wstring() const
{
	std::wstringstream ss;
	ss << (quoted ? L"'" : L"");
	switch (type)
	{
	case TOKEN_TYPE::DELIM:
		break;
	case TOKEN_TYPE::LIST:
	{
		ss << L"(";
		for (auto &i : apval)
		{
			ss << static_cast<std::wstring>(i);
			if (&i != &apval.back())
				ss << L" ";
		}
		ss << L")";
	}
	break;
	case TOKEN_TYPE::SYMBOL:
		ss << *pname;
		break;
	case TOKEN_TYPE::INT:
		ss << val;
		break;
	case TOKEN_TYPE::BOOL:
		ss << (is_true ? L"T" : L"NIL");
		break;
	case TOKEN_TYPE::LAMBDA:
		ss << L"(";
		for (auto &i : apval)
		{
			ss << static_cast<std::wstring>(i);
			if (&i != &apval.back())
				ss << L" ";
		}
		ss << L")";
		ss << L" ";
		ss << static_cast<std::wstring>(*expr);
		break;
	}
	return ss.str();
};

std::wostream &token_t::recursive_out(
	std::wostream &os, const token_t &t, const std::wstring &pre)
{
	assert(t != token_t{});
	os << std::format(
		L"{}type: {:>6.6s}, quoted: {:5}, pname: {} ", pre,
		TokenTypeToString(t.type), t.quoted, (t.pname ? *t.pname : L""));
	switch (t.type)
	{
	case TOKEN_TYPE::DELIM:
		return os;
	case TOKEN_TYPE::BOOL:
		return os << std::format(L"bool: {}", t.is_true);
	case TOKEN_TYPE::INT:
		return os << std::format(L"val: {}", t.val);
	case TOKEN_TYPE::SYMBOL:
		os << std::format(L"\n{}apval:", pre);
		os << std::format(L"\n{}[", pre);
		for (auto &i : t.apval)
		{
			os << "\n";
			recursive_out(os, i, pre + L"\t");
		}
		os << std::format(L"\n{}]", pre);
		return os;
	case TOKEN_TYPE::LIST:
		os << std::format(L"\n{}apval:", pre);
		os << std::format(L"\n{}[", pre);
		for (auto &i : t.apval)
		{
			os << "\n";
			recursive_out(os, i, pre + L"\t");
		}
		os << std::format(L"\n{}]", pre);
		return os;
	case TOKEN_TYPE::LAMBDA:
		os << std::format(L"\n{}apval:", pre);
		os << std::format(L"\n{}[", pre);
		for (auto &i : t.apval)
		{
			os << "\n";
			recursive_out(os, i, pre + L"\t");
		}
		os << std::format(L"\n{}]", pre);
		os << std::format(L"\n{}expr:", pre);
		os << std::format(L"\n{}[\n", pre);
		recursive_out(os, *t.expr, pre + L"\t");
		os << std::format(L"\n{}]", pre);

		os << std::format(L"\n{}env:", pre);
		os << std::format(L"\n{}[\n", pre);
		env_t::formated_out(os, t.env, pre + L"\t");
		os << std::format(L"\n{}]\n", pre);
		return os;
	default:
		return os;
	}
}

std::optional<token_t> env_t::find(const token_t &token)
{
	assert(!token.pname->empty());
	if (!curr_env_.contains(*token.pname))
	{
		if (next_env_)
			return next_env_->find(token);
		else
			return {};
	}
	return curr_env_[*token.pname];
}

void env_t::formated_out(
	std::wostream &os, const std::shared_ptr<env_t> &t, const std::wstring &pre)
{
	if (t->next_env_)
	{
		if (t->next_env_->env_name_ == L"global")
		{
			os << std::format(L"{}-----------\n", pre);
			os << std::format(L"{}global\n", pre);
		}
		else
			formated_out(os, t->next_env_, pre);
	}

	os << std::format(L"{}name: {}\n", pre, t->env_name_);
	os << std::format(L"{}-----------\n", pre);
	for (auto &i : t->curr_env_)
	{
		os << std::format(L"{}name: {}\n", pre, i.first);
		token_t::recursive_out(os, i.second, pre + L"\t");
		os << "\n";
	}
	os << std::format(L"{}-----------\n", pre);
}

std::wostream &operator<<(std::wostream &os, const env_t &t)
{
	if (t.next_env_)
	{
		os << "-----------\n";
		if (t.next_env_->env_name_ == L"global")
			os << "global"
			   << "\n";
		else
			os << t.next_env_ << "\n";
	}
	os << "-----------\n";

	os << "name: " << t.env_name_ << "\n";
	for (auto &i : t.curr_env_)
	{
		os << "name: " << i.first << "\n";
		token_t::recursive_out(os, i.second, L"\t");
		os << "\n";
	}
	os << "-----------\n";
	return os;
}
