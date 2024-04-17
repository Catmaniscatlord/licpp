#pragma once
#include <memory>
#include <vector>

#include "structs.hpp"

// A list of possible errors that can occur during evaluation
class EvalError
{
public:
	enum class Exception
	{
		SYMBOL_NOT_FOUND,
		NOT_A_FUNCTION,
		NOTREACHABLE,
		NONE,
	};

	Exception err_{Exception::NONE};
	token_t token_;

	EvalError() = default;

	EvalError(Exception e, token_t token) : err_{e}, token_{token} {};

	token_t get_token()
	{
		return token_;
	};

	const wchar_t* what() const
	{
		switch (err_)
		{
			case Exception::NONE:
				return L"No Error";
				break;
			case Exception::NOT_A_FUNCTION:
				return L"Attempted to evaluate a non-function";
				break;
			case Exception::NOTREACHABLE:
				return L"This code should not be reachable";
				break;
			case Exception::SYMBOL_NOT_FOUND:
				return L"Could not find the symbol";
				break;
		}
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
	token_t proc(token_t& token, token_t& args);

	std::vector<EvalError> get_error()
	{
		return err_;
	};

	void clear_error()
	{
		err_.clear();
	}
};
