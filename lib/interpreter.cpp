#include "interpreter.hpp"

#include <memory>

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
		return token;

	switch (token.type)
	{
		case TOKEN_TYPE::SYMBOL:
		{
			// This little section is for error checking on if the envrionment
			// contained the value or not
			auto ret =
				env_->find(token)
					// returns the value if it exists, or >>>
					.or_else(
						[token]()
						{
							err_.emplace_back(
								EvalError::Exception::SYMBOL_NOT_FOUND, token);
							// >>> This if it has no value
							return std::optional<token_t>{};
						})
					// If we found the value we return it, otherwise we return a
					// default value
					.value_or(token_t{});
			return ret;
		}
		case TOKEN_TYPE::INT:
		case TOKEN_TYPE::BOOL:
			return token;
		case TOKEN_TYPE::LIST:
		{
			token_t func;
			if (token.apval.back().type == TOKEN_TYPE::LIST)
				func = eval(token.apval.back());
			else
				func = token.apval.back();

			switch (func.type)
			{
				case TOKEN_TYPE::SYMBOL:
					// if set set! defun lambda
					// If its not one of those, we check if its in the
					// environment as a lambda, If it is, set that to func and
					// continue, If not, call proc()
				case TOKEN_TYPE::LAMBDA:

				case TOKEN_TYPE::DELIM:
				case TOKEN_TYPE::LIST:
				case TOKEN_TYPE::INT:
				case TOKEN_TYPE::BOOL:
					err_.emplace_back(
						EvalError::Exception::NOT_A_FUNCTION, func);
					return {};
					break;
			}
		}
		case TOKEN_TYPE::LAMBDA:
		case TOKEN_TYPE::DELIM:
			// This should never be reached.
			err_.emplace_back(EvalError::Exception::NOTREACHABLE, token);
			return {};
	}

	return token;
};
