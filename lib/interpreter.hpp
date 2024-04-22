#pragma once
#include <memory>
#include <span>
#include <vector>

#include "structs.hpp"

// A list of possible errors that can occur during evaluation
class EvalError
{
public:
	enum class Exception
	{
		INVALID_NUMBER_OF_ARGS,
		INVALID_ARG_TYPES,
		NOT_A_FUNCTION,
		UNDEFINED,
		REDEFINITION,
		NOTREACHABLE,
		OVERFLOW,
		DIVIDE_BY_ZERO,
		EVAL_EMPTY_LIST,
		MATH_ERR,
		NONE,
	};

	Exception err_{Exception::NONE};
	token_t token_;
	std::shared_ptr<env_t> env_;
	const std::wstring err_msg_;

	EvalError() = default;

	EvalError(Exception e, token_t token)
		: err_{e}, token_{token}, err_msg_{L""} {};

	EvalError(Exception e, token_t token, std::shared_ptr<env_t> env)
		: err_{e}, token_{token}, env_{env}, err_msg_{L""} {};

	EvalError(Exception e, std::wstring err_msg, token_t token)
		: err_{e}, token_{token}, err_msg_{err_msg} {};

	EvalError(Exception e,
			  std::wstring err_msg,
			  token_t token,
			  std::shared_ptr<env_t> env)
		: err_{e}, token_{token}, env_{env}, err_msg_{err_msg} {};

	auto get_env()
	{
		return env_;
	};

	auto get_token()
	{
		return token_;
	};

	const wchar_t* what() const
	{
		switch (err_)
		{
		case Exception::NONE:
			return L"No Error";
		case Exception::OVERFLOW:
			return L"int overflow";
		case Exception::DIVIDE_BY_ZERO:
			return L"division by 0";
		case Exception::MATH_ERR:
			return L"arithmatic with a non INT type";
		case Exception::NOT_A_FUNCTION:
			return L"Attempted to evaluate a non-function";
		case Exception::NOTREACHABLE:
			return L"This code should not be reachable";
		case Exception::UNDEFINED:
			return L"Symbol is undefined in the environment";
		case Exception::REDEFINITION:
			return L"Token is already defined";
		case Exception::EVAL_EMPTY_LIST:
			return L"You can't evaluate an empty list silly goose";
		case Exception::INVALID_NUMBER_OF_ARGS:
		case Exception::INVALID_ARG_TYPES:
			return err_msg_.c_str();
		}
		return L"tf did you do?";
	};
};

// Singelton for the interpreter, can be called from anywhere, stores its
// envirionment
class Interpreter
{
private:
	// The process of evaluating an expression in  LISP is inheriently recursive
	// so we share the err member across the whole class.
	// if there is an error the user has to explicitly ask for its value
	static std::vector<EvalError> err_;
	static std::shared_ptr<env_t> env_;
	static Interpreter* pinstance_;
	static std::mutex mutex_;

protected:
	Interpreter(){};

public:
	Interpreter(Interpreter& other) = delete;
	void operator=(Interpreter& other) = delete;

	static Interpreter* getInstance();

	token_t eval(token_t& token, std::shared_ptr<env_t> env = env_);

	std::vector<EvalError> get_error()
	{
		return err_;
	};

	void clear_error()
	{
		err_.clear();
	}

	auto get_env()
	{
		return env_;
	}

private:
	// Default lisp functions
	std::optional<token_t> default_functions(
		token_t& token,
		token_t& func,
		std::span<token_t> args,
		std::shared_ptr<env_t> env);

	std::optional<token_t> special_functions(
		token_t& token,
		token_t& func,
		std::span<token_t> args,
		std::shared_ptr<env_t> env);
};
