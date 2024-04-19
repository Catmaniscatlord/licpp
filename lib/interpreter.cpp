#include "interpreter.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <numeric>
#include <ranges>

#include "structs.hpp"

std::vector<EvalError> Interpreter::err_{};
std::shared_ptr<env_t> Interpreter::env_{new env_t};

// singleton stuff
Interpreter* Interpreter::pinstance_{nullptr};
std::mutex Interpreter::mutex_;

Interpreter* Interpreter::getInstance()
{
	std::lock_guard<std::mutex> lock(mutex_);

	if (pinstance_ == nullptr)
		pinstance_ = new Interpreter();

	return pinstance_;
}

token_t Interpreter::eval(token_t& token, std::shared_ptr<env_t> env)
{
	if (token.quoted)
	{
		token.quoted = false;
		return token;
	}

	switch (token.type)
	{
		case TOKEN_TYPE::SYMBOL:
		{
			// This little section is for error checking on if the envrionment
			// contained the value or not
			auto ret = env_->find(token)
						   // returns the value if it exists, or >>>
						   .or_else(
							   [token]()
							   {
								   err_.emplace_back(
									   EvalError::Exception::UNDEFINED, token);
								   // >>> This if it has no value
								   return std::optional<token_t>{};
							   })
						   // If we found the value we return it, otherwise we
						   // return a default value
						   .value_or(token_t{});
			return ret;
		}
		case TOKEN_TYPE::INT:
		case TOKEN_TYPE::BOOL:
			return token;
		case TOKEN_TYPE::LIST:
		{
			if (token.apval.empty())
			{
				err_.emplace_back(EvalError::Exception::EVAL_EMPTY_LIST, token);
				return {};
			}

			token_t func;
			if (token.apval.front().type == TOKEN_TYPE::LIST)
				func = eval(token.apval.front(), env);
			else
				func = token.apval.front();

			switch (func.type)
			{
				case TOKEN_TYPE::SYMBOL:
				{
					if (env->find(func).has_value())
					{
						if (env->find(func).value().type == TOKEN_TYPE::LAMBDA)
						{
							auto lambda = env->find(func).value();
							// This takes the args in the list and applies them
							// to the lambdas args
							auto env_args = std::ranges::views::zip_transform(
								[this, env](token_t l, token_t r) -> token_t
								{
									token_t new_t{eval(r, env)};
									new_t.pname = l.pname;
									return new_t;
								},
								// the arguments
								lambda.apval,
								// the variables to be evaluated
								token.apval | std::ranges::views::drop(1));

							// create the new environment with the functions
							// arguments and a chain to the functions
							// environment
							std::shared_ptr<env_t> new_env{new env_t{
								.curr_env_{env_args.begin(), env_args.end()},
								.next_env_{func.env}}};
							// evaluate the expression body within the given
							// envrionment
							return eval(*lambda.expr, new_env);
						}
						else
						{
							err_.emplace_back(
								EvalError::Exception::NOT_A_FUNCTION,
								env->find(func).value());
							return {};
						}
					}
					else
					{
						return default_functions(
								   token, func,
								   token.apval | std::ranges::views::drop(1),
								   env)
							.or_else(
								[token]() -> std::optional<token_t>
								{
									err_.emplace_back(
										EvalError::Exception::UNDEFINED, token);
									return {};
								})
							.value_or(token_t{});
					}
				}
				case TOKEN_TYPE::LAMBDA:
				{
					auto lambda = env->find(func).value();
					// This takes the args in the list and applies them
					// to the lambdas args
					auto env_args = std::ranges::views::zip_transform(
						[this, env](token_t l, token_t r) -> token_t
						{
							token_t new_t{eval(r, env)};
							new_t.pname = l.pname;
							return new_t;
						},
						// the arguments
						lambda.apval,
						// the variables to be evaluated
						token.apval | std::ranges::views::drop(1));

					// create the new environment with the functions
					// arguments and a chain to the functions
					// environment
					std::shared_ptr<env_t> new_env{
						new env_t{.curr_env_{env_args.begin(), env_args.end()},
								  .next_env_{func.env}}};
					// evaluate the expression body within the given
					// envrionment
					return eval(*lambda.expr, new_env);
				}
				case TOKEN_TYPE::LIST:
				case TOKEN_TYPE::INT:
				case TOKEN_TYPE::BOOL:
				case TOKEN_TYPE::DELIM:
					err_.emplace_back(
						EvalError::Exception::NOT_A_FUNCTION, func);
					return {};
					break;
			}
			return {};
		}
		case TOKEN_TYPE::LAMBDA:
		case TOKEN_TYPE::DELIM:
			// This should never be reached.
			err_.emplace_back(EvalError::Exception::NOTREACHABLE, token);
			return {};
	}

	return token;
};

std::optional<token_t> Interpreter::default_functions(
	token_t& token,
	token_t& func,
	std::span<token_t> raw_args,
	std::shared_ptr<env_t> env)
{
	assert(func.type == TOKEN_TYPE::SYMBOL);

	auto ret{special_functions(token, func, raw_args, env)};
	if (ret.has_value())
		ret.value();

	// for the rest of these the args are evaluated
	auto argsv = std::ranges::views::transform(
		raw_args, [this, env](token_t& t) -> token_t { return eval(t, env); });
	std::vector<token_t> args{argsv.begin(), argsv.end()};

	if (func.pname == L"mapcar")
	{
		// mapcar takes at least 2 args, mapcar func args args args ...
		if (args.size() < 2)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"mapcar takes 2 or more args", token);
			return {};
		}
		else if ((args[0].type != TOKEN_TYPE::SYMBOL &&
				  args[0].type != TOKEN_TYPE::LAMBDA) ||
				 // The rest of the arguments are a list
				 [args]()
				 {
					 // for each argument in the args (thats not the function)
					 for (const token_t& i : args | std::ranges::views::drop(1))
						 if (i.type != TOKEN_TYPE::LIST)
							 return true;
					 return false;
				 }())
		{
			err_.emplace_back(
				EvalError::Exception::INVALID_ARG_TYPES,
				L"mapcar takes arg types: lamba/symbol list list ...", token);
			return token_t{};
		}

		// behold this functional bullshit
		auto actual_args = args | std::ranges::views::drop(1);
		std::vector<token_t> results{};

		// shortest of the argument lists
		size_t shortest_list_size{
			std::ranges::min_element(
				actual_args, [](token_t& a, token_t& b)
				{ return a.apval.size() < b.apval.size(); })
				->apval.size()};

		// effectively a transposed join
		// {1 2 3} {4 5} {6 7 8 9}
		// ->
		// {1 2 3}
		// {4 5}
		// {6 7 8 9}
		// ->
		// {1 4 6} {2 5 7}
		// then throw the function in front and eval that hoe
		for (size_t i{0}; i < shortest_list_size; i++)
		{
			token_t eval_token{.type = TOKEN_TYPE::LIST};

			auto arg_list =
				actual_args |
				std::ranges::views::transform(
					[i](token_t& t) -> token_t& { return t.apval[i]; });

			eval_token.apval.assign(arg_list.begin(), arg_list.end());
			eval_token.apval.insert(eval_token.apval.begin(), args[0]);

			results.push_back(eval(eval_token, env));
		}

		return token_t{.type = TOKEN_TYPE::LIST, .apval{std::move(results)}};
	}
	else if (func.pname == L"car")
	{
		if (args.size() != 1)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"car takes 1 arg", token);
			return token_t{};
		}
		else if (args[0].type != TOKEN_TYPE::LIST)
		{
			err_.emplace_back(EvalError::Exception::INVALID_ARG_TYPES,
							  L"car takes arg types: list", token);
			return token_t{};
		}
		return args[0].apval.front();
	}
	else if (func.pname == L"cdr")
	{
		if (args.size() != 1)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"cdr takes 1 arg", token);
			return token_t{};
		}
		else if (args[0].type != TOKEN_TYPE::LIST)
		{
			err_.emplace_back(EvalError::Exception::INVALID_ARG_TYPES,
							  L"cdr takes arg types: list", token);
			return token_t{};
		}
		auto ret{args[0]};
		ret.apval.erase(ret.apval.begin());
		return ret;
	}
	else if (func.pname == L"cons")
	{
		if (args.size() != 1)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"cdr takes 2 args", token);
			return token_t{};
		}
		else if (args[1].type != TOKEN_TYPE::LIST)
		{
			err_.emplace_back(EvalError::Exception::INVALID_ARG_TYPES,
							  L"cons takes arg types: any list", token);
			return token_t{};
		}

		token_t ret{args[1]};
		ret.apval.insert(ret.apval.begin(), eval(args[0], env));

		return ret;
	}
	else if (func.pname == L"sqrt")
	{
		if (args.size() != 1)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"sqrt takes 1 arg", token);
			return token_t{};
		}
		if (args[0].type != TOKEN_TYPE::INT)
		{
			err_.emplace_back(EvalError::Exception::INVALID_ARG_TYPES,
							  L"sqrt takes arg types: int", token);
			return token_t{};
		}

		return token_t{
			.val = static_cast<int>(sqrt(args[0].val)),
			.type = TOKEN_TYPE::INT,
		};
	}
	else if (func.pname == L"pow")
		// TODO: Error checking
		return token_t{
			.val = static_cast<int>(pow(args[0].val, args[1].val)),
			.type = TOKEN_TYPE::INT,
		};
	else if (func.pname == L"+")
		return token_t{
			.val = std::accumulate(
				args.begin(), args.end(), 0,
				[&token](int total, token_t& x)
				{
					if (x.type != TOKEN_TYPE::INT)
						err_.emplace_back(
							EvalError::Exception::MATH_ERR, token);
					return total + x.val;
				}),
			.type = TOKEN_TYPE::INT,
		};
	else if (func.pname == L"-")
		return token_t{
			.val = std::accumulate(
				std::next(args.begin()), args.end(), args.begin()->val,
				[&token](int total, token_t& x)
				{
					if (x.type != TOKEN_TYPE::INT)
						err_.emplace_back(
							EvalError::Exception::MATH_ERR, token);
					return total - x.val;
				}),
			.type = TOKEN_TYPE::INT,
		};
	else if (func.pname == L"*")
		return token_t{
			.val = std::accumulate(
				std::next(args.begin()), args.end(), 1,
				[&token](int total, token_t& x)
				{
					if (x.type != TOKEN_TYPE::INT)
						err_.emplace_back(
							EvalError::Exception::MATH_ERR, token);
					return total * x.val;
				}),
			.type = TOKEN_TYPE::INT,
		};
	else if (func.pname == L"/")
		return token_t{
			.val = std::accumulate(
				std::next(args.begin()), args.end(), args.begin()->val,
				[&token](int total, token_t& x)
				{
					if (x.type != TOKEN_TYPE::INT)
						err_.emplace_back(
							EvalError::Exception::MATH_ERR, token);
					if (x.val == 0)
					{
						err_.emplace_back(
							EvalError::Exception::DIVIDE_BY_ZERO, x);
						return 0;
					}
					return total / x.val;
				}),
			.type = TOKEN_TYPE::INT,
		};
	else if (func.pname == L"/")
		return token_t{
			.val = std::accumulate(
				std::next(args.begin()), args.end(), args.begin()->val,
				[&token](int total, token_t& x)
				{
					if (x.type != TOKEN_TYPE::INT)
						err_.emplace_back(
							EvalError::Exception::MATH_ERR, token);
					if (x.val == 0)
					{
						err_.emplace_back(
							EvalError::Exception::DIVIDE_BY_ZERO, x);
						return 0;
					}
					return total / x.val;
				}),
			.type = TOKEN_TYPE::INT,
		};
	else if (func.pname == L"=")
	{
		if (args.size() < 2)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"= takes 2 or more args", token);
			return token_t{};
		}
		return token_t{
			.is_true =
				[&args, token]()
			{
				for (size_t i{0}; i < args.size() - 1; i++)
					if (args[i] > args[i + 1])
						return false;
				return true;
			}(),
			.type = TOKEN_TYPE::BOOL,
		};
	}
	else if (func.pname == L">")
	{
		if (args.size() < 2)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"> takes 2 or more args", token);
			return token_t{};
		}
		return token_t{
			.is_true =
				[&args, &token]()
			{
				for (size_t i{0}; i < args.size() - 1; i++)
				{
					if (args[i].type != TOKEN_TYPE::INT &&
						args[i + 1].type != TOKEN_TYPE::INT)
					{
						err_.emplace_back(
							EvalError::Exception::INVALID_ARG_TYPES,
							L"> takes arg types: int int int...", token);
						return false;
					}
					if (args[i] > args[i + 1])
						return false;
				}
				return true;
			}(),
			.type = TOKEN_TYPE::BOOL,
		};
	}
	else if (func.pname == L"<")
	{
		if (args.size() < 2)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"< takes 2 or more args", token);
			return token_t{};
		}
		return token_t{
			.is_true =
				[&args, &token]()
			{
				for (size_t i{0}; i < args.size() - 1; i++)
				{
					if (args[i].type != TOKEN_TYPE::INT &&
						args[i + 1].type != TOKEN_TYPE::INT)
					{
						err_.emplace_back(
							EvalError::Exception::INVALID_ARG_TYPES,
							L"< takes arg types: int int int...", token);
						return false;
					}
					if (args[i] < args[i + 1])
						return false;
				}
				return true;
			}(),
			.type = TOKEN_TYPE::BOOL,
		};
	}
	else if (func.pname == L"and")
	{
		if (args.size() < 2)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"and takes 2 or more args", token);
			return token_t{};
		}
		return token_t{
			.is_true =
				[&args, &token]()
			{
				for (size_t i{0}; i < args.size() - 1; i++)
				{
					if (args[i].type != TOKEN_TYPE::BOOL &&
						args[i + 1].type != TOKEN_TYPE::BOOL)
					{
						err_.emplace_back(
							EvalError::Exception::INVALID_ARG_TYPES,
							L"and takes arg types: bool bool bool...", token);
						return false;
					}
					if (!(args[i].is_true && args[i + 1].is_true))
						return false;
				}
				return true;
			}(),
			.type = TOKEN_TYPE::BOOL,
		};
	}
	else if (func.pname == L"or")
	{
		if (args.size() < 2)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"or takes 2 or more args", token);
			return token_t{};
		}
		return token_t{
			.is_true =
				[&args, &token]()
			{
				for (auto& i : args)
				{
					if (i.type != TOKEN_TYPE::BOOL)
					{
						err_.emplace_back(
							EvalError::Exception::INVALID_ARG_TYPES,
							L"or takes arg types: bool bool bool...", token);
						return false;
					}
					if (i.is_true)
						return true;
				}
				return false;
			}(),
			.type = TOKEN_TYPE::BOOL,
		};
	}
	else if (func.pname == L"not")
	{
		if (args.size() != 1)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"not takes 1 arg", token);
			return token_t{};
		}
		if (args[0].type != TOKEN_TYPE::BOOL)
		{
			err_.emplace_back(EvalError::Exception::INVALID_ARG_TYPES,
							  L"not takes arg types: bool", token);
			return token_t{};
		}
		return token_t{
			.is_true = !args[0].is_true,
			.type = TOKEN_TYPE::BOOL,
		};
	}
	// And Finally if it couldn't find it...
	return std::optional<token_t>{};
};

std::optional<token_t> Interpreter::special_functions(
	token_t& token,
	token_t& func,
	std::span<token_t> args,
	std::shared_ptr<env_t> env)
{
	assert(func.type == TOKEN_TYPE::SYMBOL);
	if (func.pname == L"if")
	{
		// if takes 3 arguments if test conseq alt
		if (args.size() != 3)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"if takes 3 args", func);
			return token_t{};
		}

		// get the test value and make sure its a boolean
		auto test{eval(args[0], env)};

		if (test.type != TOKEN_TYPE::BOOL)
		{
			err_.emplace_back(EvalError::Exception::INVALID_ARG_TYPES,
							  L"if takes arg types: Bool any any", token);
			return token_t{};
		}

		// if test is true eval with conseq, else eval with alt
		return eval(test.is_true ? args[1] : args[2], env);
	}
	else if (func.pname == L"define")
	{
		// define takes 2 arguments define name value
		if (args.size() != 2)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"define takes 2 args", token);
			return token_t{};
		}
		else if (args[0].type != TOKEN_TYPE::SYMBOL)
		{
			err_.emplace_back(EvalError::Exception::INVALID_ARG_TYPES,
							  L"Define takes arg types: symbol any", token);
			return token_t{};
		}
		// check that the symbol isn't already defined
		else if (env_->find(args[0]).has_value())
		{
			err_.emplace_back(EvalError::Exception::REDEFINITION, token);
			return token_t{};
		}

		token_t new_token{eval(args[1], env)};
		new_token.pname = args[0].pname;
		// define it in the global environment
		env_->curr_env_.insert(new_token);
		return token_t{};
	}
	else if (func.pname == L"set!")
	{
		// set! takes 2 arguments define name value
		if (args.size() != 2)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"set! takes 2 args", token);
			return token_t{};
		}
		else if (args[0].type != TOKEN_TYPE::SYMBOL)
		{
			err_.emplace_back(EvalError::Exception::INVALID_ARG_TYPES,
							  L"set takes arg types: symbol any", token);
			return token_t{};
		}
		// check that the symbol is already defined
		else if (!env_->find(args[0]).has_value())
		{
			err_.emplace_back(EvalError::Exception::UNDEFINED, args[0]);
			return token_t{};
		}
		token_t new_token = eval(args[1], env);
		new_token.pname = args[0].pname;

		// remove the old token
		env_->curr_env_.erase(args[0]);
		// put in the new token
		env_->curr_env_.insert(new_token);

		return token_t{};
	}
	else if (func.pname == L"defun")
	// equivalent to (define name (lambda (args) (expr)))
	{
		// Lambdas only take 3 args, defun name (args) (body)
		if (args.size() != 3)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"defun takes 3 args", token);
			return token_t{};
		}
		else if (args[0].type != TOKEN_TYPE::SYMBOL ||
				 args[1].type != TOKEN_TYPE::LIST ||
				 args[2].type != TOKEN_TYPE::LIST)
		{
			err_.emplace_back(
				EvalError::Exception::INVALID_ARG_TYPES,
				L"defun takes arg types: symbol list list", token);
			return token_t{};
		}
		// check that the function isn't already defined
		else if (env_->find(args[0]).has_value())
		{
			err_.emplace_back(EvalError::Exception::REDEFINITION, token);
			return token_t{};
		}

		// Create the function, ap val is the argument list,
		// expr is the body object, the environment is locked to
		// the current environment
		// define it in the global environment
		env_->curr_env_.emplace(
			token_t{.type = TOKEN_TYPE::LAMBDA,
					.pname{args[0].pname},
					.apval{args[1].apval},
					.expr{std::make_shared<token_t>(args[2])},
					.env{env}});
		return token_t{};
	}
	else if (func.pname == L"lambda")
	{
		// Lambdas only take 2 args, lambda (args) (body)
		if (args.size() != 2)
		{
			err_.emplace_back(EvalError::Exception::INVALID_NUMBER_OF_ARGS,
							  L"lambda takes 2 args", token);
			return token_t{};
		}
		else if (args[0].type != TOKEN_TYPE::LIST ||
				 args[1].type != TOKEN_TYPE::LIST)
		{
			err_.emplace_back(EvalError::Exception::INVALID_ARG_TYPES,
							  L"lambda takes ar types: list list", token);
			return token_t{};
		}
		// Create the lambda, ap val is the argument list, expr
		// is the body object, the environment is locked to the
		// current environment
		return token_t{.type = TOKEN_TYPE::LAMBDA,
					   .apval{args[0].apval},
					   .expr{std::make_shared<token_t>(args[1])},
					   .env{env}};
	}

	return std::optional<token_t>{};
}
