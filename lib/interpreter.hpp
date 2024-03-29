
#include <forward_list>
#include <memory>
#include <string>
#include <variant>
#include <vector>

/*
 * We have 4 different types of data in Lisp
 * A cons-cell, a pname, An applied value (list or int), and an expr
 *
 * We can break this down into 3 types of data
 * A symbol containing a pname,
 * then either an apval, or an expr
 * or a list that points to other data types
 * and an integer
 */

/*
 * INT apvalue
 * BOOL apvalue
 * LIST apvalue
 * SYMBOL pname
 * QUOTE list that doesnt get evaluated
 */

enum class VALUE_TYPE
{
	INT,
	BOOL,
	LIST,
	SYMBOL,
};

struct value_t;

using List = std::vector<value_t>;

struct value_t
{
	VALUE_TYPE type;
	std::variant<int, bool, List> apval;
	std::string pname{};

	value_t cdr();
	value_t* car();
	const value_t* car() const;
};

value_t eval(const value_t& value, value_t& env);

value_t apply(const value_t& f, value_t& args, value_t& env);
