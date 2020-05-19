/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <libsolidity/formal/Sorts.h>

#include <libsolidity/ast/Types.h>
#include <libsolidity/interface/ReadFile.h>
#include <liblangutil/Exceptions.h>
#include <libsolutil/Common.h>
#include <libsolutil/Exceptions.h>

#include <boost/noncopyable.hpp>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace solidity::frontend::smt
{

struct SMTSolverChoice
{
	bool cvc4 = false;
	bool z3 = false;

	static constexpr SMTSolverChoice All() { return {true, true}; }
	static constexpr SMTSolverChoice CVC4() { return {true, false}; }
	static constexpr SMTSolverChoice Z3() { return {false, true}; }
	static constexpr SMTSolverChoice None() { return {false, false}; }

	bool none() { return !some(); }
	bool some() { return cvc4 || z3; }
	bool all() { return cvc4 && z3; }
};

enum class CheckResult
{
	SATISFIABLE, UNSATISFIABLE, UNKNOWN, CONFLICTING, ERROR
};

// Forward declaration.
SortPointer smtSort(Type const& _type);

/// C++ representation of an SMTLIB2 expression.
class Expression
{
	friend class SolverInterface;
public:
	explicit Expression(bool _v): Expression(_v ? "true" : "false", Kind::Bool) {}
	explicit Expression(frontend::TypePointer _type): Expression(_type->toString(true), {}, std::make_shared<SortSort>(smtSort(*_type))) {}
	explicit Expression(std::shared_ptr<SortSort> _sort): Expression("", {}, _sort) {}
	Expression(size_t _number): Expression(std::to_string(_number), Kind::Int) {}
	Expression(u256 const& _number): Expression(_number.str(), Kind::Int) {}
	Expression(s256 const& _number): Expression(_number.str(), Kind::Int) {}
	Expression(bigint const& _number): Expression(_number.str(), Kind::Int) {}

	Expression(Expression const&) = default;
	Expression(Expression&&) = default;
	Expression& operator=(Expression const&) = default;
	Expression& operator=(Expression&&) = default;

	bool hasCorrectArity() const
	{
		if (name == "tuple_constructor")
		{
			auto tupleSort = std::dynamic_pointer_cast<TupleSort>(sort);
			solAssert(tupleSort, "");
			return arguments.size() == tupleSort->components.size();
		}

		static std::map<std::string, unsigned> const operatorsArity{
			{"ite", 3},
			{"not", 1},
			{"and", 2},
			{"or", 2},
			{"implies", 2},
			{"=", 2},
			{"<", 2},
			{"<=", 2},
			{">", 2},
			{">=", 2},
			{"+", 2},
			{"-", 2},
			{"*", 2},
			{"/", 2},
			{"mod", 2},
			{"select", 2},
			{"store", 3},
			{"const_array", 2},
			{"tuple_get", 2}
		};
		return operatorsArity.count(name) && operatorsArity.at(name) == arguments.size();
	}

	static Expression ite(Expression _condition, Expression _trueValue, Expression _falseValue)
	{
		solAssert(*_trueValue.sort == *_falseValue.sort, "");
		SortPointer sort = _trueValue.sort;
		return Expression("ite", std::vector<Expression>{
			std::move(_condition), std::move(_trueValue), std::move(_falseValue)
		}, std::move(sort));
	}

	static Expression implies(Expression _a, Expression _b)
	{
		return Expression(
			"implies",
			std::move(_a),
			std::move(_b),
			Kind::Bool
		);
	}

	/// select is the SMT representation of an array index access.
	static Expression select(Expression _array, Expression _index)
	{
		solAssert(_array.sort->kind == Kind::Array, "");
		std::shared_ptr<ArraySort> arraySort = std::dynamic_pointer_cast<ArraySort>(_array.sort);
		solAssert(arraySort, "");
		solAssert(_index.sort, "");
		solAssert(*arraySort->domain == *_index.sort, "");
		return Expression(
			"select",
			std::vector<Expression>{std::move(_array), std::move(_index)},
			arraySort->range
		);
	}

	/// store is the SMT representation of an assignment to array index.
	/// The function is pure and returns the modified array.
	static Expression store(Expression _array, Expression _index, Expression _element)
	{
		auto arraySort = std::dynamic_pointer_cast<ArraySort>(_array.sort);
		solAssert(arraySort, "");
		solAssert(_index.sort, "");
		solAssert(_element.sort, "");
		solAssert(*arraySort->domain == *_index.sort, "");
		solAssert(*arraySort->range == *_element.sort, "");
		return Expression(
			"store",
			std::vector<Expression>{std::move(_array), std::move(_index), std::move(_element)},
			arraySort
		);
	}

	static Expression const_array(Expression _sort, Expression _value)
	{
		solAssert(_sort.sort->kind == Kind::Sort, "");
		auto sortSort = std::dynamic_pointer_cast<SortSort>(_sort.sort);
		auto arraySort = std::dynamic_pointer_cast<ArraySort>(sortSort->inner);
		solAssert(sortSort && arraySort, "");
		solAssert(_value.sort, "");
		solAssert(*arraySort->range == *_value.sort, "");
		return Expression(
			"const_array",
			std::vector<Expression>{std::move(_sort), std::move(_value)},
			arraySort
		);
	}

	static Expression tuple_get(Expression _tuple, size_t _index)
	{
		solAssert(_tuple.sort->kind == Kind::Tuple, "");
		std::shared_ptr<TupleSort> tupleSort = std::dynamic_pointer_cast<TupleSort>(_tuple.sort);
		solAssert(tupleSort, "");
		solAssert(_index < tupleSort->components.size(), "");
		return Expression(
			"tuple_get",
			std::vector<Expression>{std::move(_tuple), Expression(_index)},
			tupleSort->components.at(_index)
		);
	}

	static Expression tuple_constructor(Expression _tuple, std::vector<Expression> _arguments)
	{
		solAssert(_tuple.sort->kind == Kind::Sort, "");
		auto sortSort = std::dynamic_pointer_cast<SortSort>(_tuple.sort);
		auto tupleSort = std::dynamic_pointer_cast<TupleSort>(sortSort->inner);
		solAssert(tupleSort, "");
		solAssert(_arguments.size() == tupleSort->components.size(), "");
		return Expression(
			"tuple_constructor",
			std::move(_arguments),
			tupleSort
		);
	}

	friend Expression operator!(Expression _a)
	{
		return Expression("not", std::move(_a), Kind::Bool);
	}
	friend Expression operator&&(Expression _a, Expression _b)
	{
		return Expression("and", std::move(_a), std::move(_b), Kind::Bool);
	}
	friend Expression operator||(Expression _a, Expression _b)
	{
		return Expression("or", std::move(_a), std::move(_b), Kind::Bool);
	}
	friend Expression operator==(Expression _a, Expression _b)
	{
		return Expression("=", std::move(_a), std::move(_b), Kind::Bool);
	}
	friend Expression operator!=(Expression _a, Expression _b)
	{
		return !(std::move(_a) == std::move(_b));
	}
	friend Expression operator<(Expression _a, Expression _b)
	{
		return Expression("<", std::move(_a), std::move(_b), Kind::Bool);
	}
	friend Expression operator<=(Expression _a, Expression _b)
	{
		return Expression("<=", std::move(_a), std::move(_b), Kind::Bool);
	}
	friend Expression operator>(Expression _a, Expression _b)
	{
		return Expression(">", std::move(_a), std::move(_b), Kind::Bool);
	}
	friend Expression operator>=(Expression _a, Expression _b)
	{
		return Expression(">=", std::move(_a), std::move(_b), Kind::Bool);
	}
	friend Expression operator+(Expression _a, Expression _b)
	{
		return Expression("+", std::move(_a), std::move(_b), Kind::Int);
	}
	friend Expression operator-(Expression _a, Expression _b)
	{
		return Expression("-", std::move(_a), std::move(_b), Kind::Int);
	}
	friend Expression operator*(Expression _a, Expression _b)
	{
		return Expression("*", std::move(_a), std::move(_b), Kind::Int);
	}
	friend Expression operator/(Expression _a, Expression _b)
	{
		return Expression("/", std::move(_a), std::move(_b), Kind::Int);
	}
	friend Expression operator%(Expression _a, Expression _b)
	{
		return Expression("mod", std::move(_a), std::move(_b), Kind::Int);
	}
	Expression operator()(std::vector<Expression> _arguments) const
	{
		solAssert(
			sort->kind == Kind::Function,
			"Attempted function application to non-function."
		);
		auto fSort = dynamic_cast<FunctionSort const*>(sort.get());
		solAssert(fSort, "");
		return Expression(name, std::move(_arguments), fSort->codomain);
	}

	std::string name;
	std::vector<Expression> arguments;
	SortPointer sort;

private:
	/// Manual constructors, should only be used by SolverInterface and this class itself.
	Expression(std::string _name, std::vector<Expression> _arguments, SortPointer _sort):
		name(std::move(_name)), arguments(std::move(_arguments)), sort(std::move(_sort)) {}
	Expression(std::string _name, std::vector<Expression> _arguments, Kind _kind):
		Expression(std::move(_name), std::move(_arguments), std::make_shared<Sort>(_kind)) {}

	explicit Expression(std::string _name, Kind _kind):
		Expression(std::move(_name), std::vector<Expression>{}, _kind) {}
	Expression(std::string _name, Expression _arg, Kind _kind):
		Expression(std::move(_name), std::vector<Expression>{std::move(_arg)}, _kind) {}
	Expression(std::string _name, Expression _arg1, Expression _arg2, Kind _kind):
		Expression(std::move(_name), std::vector<Expression>{std::move(_arg1), std::move(_arg2)}, _kind) {}
};

DEV_SIMPLE_EXCEPTION(SolverError);

class SolverInterface
{
public:
	virtual ~SolverInterface() = default;
	virtual void reset() = 0;

	virtual void push() = 0;
	virtual void pop() = 0;

	virtual void declareVariable(std::string const& _name, SortPointer const& _sort) = 0;
	Expression newVariable(std::string _name, SortPointer const& _sort)
	{
		// Subclasses should do something here
		solAssert(_sort, "");
		declareVariable(_name, _sort);
		return Expression(std::move(_name), {}, _sort);
	}

	virtual void addAssertion(Expression const& _expr) = 0;

	/// Checks for satisfiability, evaluates the expressions if a model
	/// is available. Throws SMTSolverError on error.
	virtual std::pair<CheckResult, std::vector<std::string>>
	check(std::vector<Expression> const& _expressionsToEvaluate) = 0;

	/// @returns a list of queries that the system was not able to respond to.
	virtual std::vector<std::string> unhandledQueries() { return {}; }

	/// @returns how many SMT solvers this interface has.
	virtual unsigned solvers() { return 1; }
};

}
