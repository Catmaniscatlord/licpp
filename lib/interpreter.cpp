#include "interpreter.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <ranges>
#include <variant>

value_t value_t::cdr()
{
	assert(std::holds_alternative<List>(apval));
	List list{std::get<List>(apval)};
	// TODO: throw if begin()+1 == end()
	return {.type = VALUE_TYPE::LIST,
			.apval = List{std::next(list.begin(), 1), list.end()}};
};

const value_t* value_t::car() const
{
	assert(std::holds_alternative<List>(apval));
	return &std::get<List>(apval).front();
};

value_t* value_t::car()
{
	assert(std::holds_alternative<List>(apval));
	return &std::get<List>(apval).front();
};

value_t eval(value_t& value, value_t& env)
{
	switch (value.type)
	{
		case VALUE_TYPE::INT:
		case VALUE_TYPE::SYMBOL:
		case VALUE_TYPE::BOOL:
			return value;
		case VALUE_TYPE::LIST:
		{
			if (value.car()->type == VALUE_TYPE::SYMBOL)
			{
				auto sym = value.cdr().car();
				// quote
				if (sym->pname == "quote")
					return *value.cdr().car();
				// if (conditional) (expr) (expr)
				else if (sym->pname == "if")
				{
					assert(value.cdr().car() != nullptr);
					assert(value.cdr().cdr().car() != nullptr);
					assert(value.cdr().cdr().cdr().car() != nullptr);
					auto conditional = (eval(*value.cdr().car(), env).apval);
					assert(std::holds_alternative<bool>(conditional));
					return std::get<bool>(conditional)
							   ? eval(*value.cdr().cdr().car(), env)
							   : eval(*value.cdr().cdr().cdr().car(), env);
				}
				// define name (expr)
				else if (sym->pname == "define")
				{
					assert(value.cdr().car()->type = VALUE_TYPE::SYMBOL);
					assert(value.cdr().cdr().car() != nullptr);
					value_t n_var;
					n_var.type = VALUE_TYPE::SYMBOL;
					n_var.pname = value.cdr().car()->pname;
					n_var.apval = eval(*value.cdr().cdr().car(), env).apval;
					std::get<List>(env.apval).push_back(n_var);
					return n_var;
				}
				// set! name (expr)
				else if (sym->pname == "set!")
				{
					assert(value.cdr().car()->type = VALUE_TYPE::SYMBOL);
					assert(value.cdr().cdr().car() != nullptr);
					// Find the variable
					auto var = std::ranges::find_if(
						std::get<List>(env.apval),
						[&value](value_t x)
						{
							if (x.type != VALUE_TYPE::SYMBOL)
								return false;
							return x.pname == value.cdr().car()->pname;
						});
					// TODO: Throw if variable not found
					var->apval = eval(*value.cdr().cdr().car(), env).apval;
					return *var;
				}
				// Defun
				// (defun name (params) (body))
				else if (sym->pname == "defun")
				{
					assert(value.cdr().car()->type = VALUE_TYPE::SYMBOL);
					assert(value.cdr().cdr().car() != nullptr);
					assert(value.cdr().cdr().cdr().car() != nullptr);

					// (define name)
					value_t n_func;
					n_func.type = VALUE_TYPE::SYMBOL;
					n_func.pname = value.cdr().car()->pname;
					// Add the name to the environment defun is called from
					std::get<List>(env.apval).push_back(n_func);

					n_func.apval = List{eval(
						// lambda (params) (body)
						{.type = VALUE_TYPE::LIST,
						 .apval = List{value_t{.type = VALUE_TYPE::SYMBOL,
											   .pname = "lambda"},
									   *value.cdr().cdr().car(),
									   *value.cdr().cdr().cdr().car()}},
						env)};
					return n_func;	// name
				}
				// (lambda (params) (body))
				else if (sym->pname == "lambda")
				{
					value_t env_copy = env;
					return {.type = VALUE_TYPE::LIST,
							.apval = List{
								value_t{.type = VALUE_TYPE::SYMBOL,
										.pname = "closure"},
								*value.cdr().car(), *value.cdr().cdr().car(),
								env_copy}};	 // (closure (params) (body) env)
				}
				else
				{
					// (apply (car value) (evargs (cdr value) env) env)
					auto args = eval(
						{.type = VALUE_TYPE::LIST,
						 .apval = List{value_t{.type = VALUE_TYPE::SYMBOL,
											   .pname = "evargs"},
									   eval(value.cdr(), env), env}},
						env);
					return apply(eval(*value.car(), env), args, env);
				}
			}
		}
	}

	return {};
};

value_t apply(const value_t& f, value_t& args, value_t& env)
{
	if (f.pname == "car")
		return *args.car();
	auto L = eval(f, env);
	if (f.car()->pname == "closure")
	{
		List LE;
		value_t tmp;
		for (std::pair<value_t&, value_t&> i :
			 std::views::zip(std::get<List>(L.cdr().car()->cdr().car()->apval),
							 std::get<List>(args.apval)))
		{
			tmp.type = VALUE_TYPE::SYMBOL;
			tmp.pname = i.first.pname;
			tmp.apval = i.second.apval;
			LE.emplace_back(std::move(tmp));
		}
		value_t copy = *L.cdr().cdr().car();
		std::get<List>(copy.apval)
			.insert(std::get<List>(copy.apval).begin(), LE.begin(), LE.end());
		return eval(*L.cdr().car()->cdr().cdr().car(), copy);
	}

	List LE;
	value_t tmp;
	for (std::pair<value_t&, value_t&> i : std::views::zip(
			 std::get<List>(L.cdr().car()->apval), std::get<List>(args.apval)))
	{
		tmp.type = VALUE_TYPE::SYMBOL;
		tmp.pname = i.first.pname;
		tmp.apval = i.second.apval;
		LE.emplace_back(std::move(tmp));
	}
	value_t copy = env;
	std::get<List>(copy.apval)
		.insert(std::get<List>(copy.apval).begin(), LE.begin(), LE.end());

	return eval(*L.cdr().car()->cdr().cdr().car(), copy);
};
