#include "structs.hpp"

#include <cassert>
#include <format>

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
	if (!l.apval.empty() || !r.apval.empty())
	{
		if (l.apval.size() != r.apval.size())
			return l.apval.size() <=> r.apval.size();
		for (size_t i{0}; i < l.apval.size(); i++)
			if (nested_check(l.apval[i], r.apval[i]) != 0)
				return nested_check(l.apval[i], r.apval[i]);
	}
	return (l.is_true == r.is_true && l.type == r.type && l.pname == r.pname
				? (l.val <=> r.val)
				: std::strong_ordering::less);
}

std::wostream &token_t::recursive_out(
	std::wostream &os, const token_t &t, const std::wstring &pre)
{
	assert(t != token_t{});
	os << std::format(L"{}type: {:>6.6s}, quoted: {:5}, pname: {} ", pre,
					  TokenTypeToString(t.type), t.quoted, *t.pname);
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