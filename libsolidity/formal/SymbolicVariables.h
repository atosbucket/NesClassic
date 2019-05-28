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

#include <libsolidity/formal/SolverInterface.h>
#include <libsolidity/formal/SSAVariable.h>
#include <libsolidity/ast/Types.h>
#include <memory>

namespace dev
{
namespace solidity
{
namespace smt
{

class Type;

/**
 * This abstract class represents the symbolic version of a program variable.
 */
class SymbolicVariable
{
public:
	SymbolicVariable(
		solidity::TypePointer _type,
		std::string _uniqueName,
		SolverInterface& _interface
	);
	SymbolicVariable(
		SortPointer _sort,
		std::string _uniqueName,
		SolverInterface& _interface
	);

	virtual ~SymbolicVariable() = default;

	Expression currentValue() const;
	std::string currentName() const;
	virtual Expression valueAtIndex(int _index) const;
	virtual Expression increaseIndex();
	virtual Expression operator()(std::vector<Expression> /*_arguments*/) const
	{
		solAssert(false, "Function application to non-function.");
	}

	unsigned index() const { return m_ssa->index(); }
	unsigned& index() { return m_ssa->index(); }

	solidity::TypePointer const& type() const { return m_type; }

protected:
	std::string uniqueSymbol(unsigned _index) const;

	/// SMT sort.
	SortPointer m_sort;
	/// Solidity type, used for size and range in number types.
	solidity::TypePointer m_type;
	std::string m_uniqueName;
	SolverInterface& m_interface;
	std::unique_ptr<SSAVariable> m_ssa;
};

/**
 * Specialization of SymbolicVariable for Bool
 */
class SymbolicBoolVariable: public SymbolicVariable
{
public:
	SymbolicBoolVariable(
		solidity::TypePointer _type,
		std::string _uniqueName,
		SolverInterface& _interface
	);
};

/**
 * Specialization of SymbolicVariable for Integers
 */
class SymbolicIntVariable: public SymbolicVariable
{
public:
	SymbolicIntVariable(
		solidity::TypePointer _type,
		std::string _uniqueName,
		SolverInterface& _interface
	);
};

/**
 * Specialization of SymbolicVariable for Address
 */
class SymbolicAddressVariable: public SymbolicIntVariable
{
public:
	SymbolicAddressVariable(
		std::string _uniqueName,
		SolverInterface& _interface
	);
};

/**
 * Specialization of SymbolicVariable for FixedBytes
 */
class SymbolicFixedBytesVariable: public SymbolicIntVariable
{
public:
	SymbolicFixedBytesVariable(
		unsigned _numBytes,
		std::string _uniqueName,
		SolverInterface& _interface
	);
};

/**
 * Specialization of SymbolicVariable for FunctionType
 */
class SymbolicFunctionVariable: public SymbolicVariable
{
public:
	SymbolicFunctionVariable(
		solidity::TypePointer _type,
		std::string _uniqueName,
		SolverInterface& _interface
	);

	Expression increaseIndex();
	Expression operator()(std::vector<Expression> _arguments) const;

private:
	/// Creates a new function declaration.
	void resetDeclaration();

	/// Stores the current function declaration.
	Expression m_declaration;
};

/**
 * Specialization of SymbolicVariable for Mapping
 */
class SymbolicMappingVariable: public SymbolicVariable
{
public:
	SymbolicMappingVariable(
		solidity::TypePointer _type,
		std::string _uniqueName,
		SolverInterface& _interface
	);
};

/**
 * Specialization of SymbolicVariable for Array
 */
class SymbolicArrayVariable: public SymbolicVariable
{
public:
	SymbolicArrayVariable(
		solidity::TypePointer _type,
		std::string _uniqueName,
		SolverInterface& _interface
	);
};

/**
 * Specialization of SymbolicVariable for Enum
 */
class SymbolicEnumVariable: public SymbolicVariable
{
public:
	SymbolicEnumVariable(
		solidity::TypePointer _type,
		std::string _uniqueName,
		SolverInterface& _interface
	);
};

/**
 * Specialization of SymbolicVariable for Tuple
 */
class SymbolicTupleVariable: public SymbolicVariable
{
public:
	SymbolicTupleVariable(
		solidity::TypePointer _type,
		std::string _uniqueName,
		SolverInterface& _interface
	);

	std::vector<std::shared_ptr<SymbolicVariable>> const& components()
	{
		return m_components;
	}

	void setComponents(std::vector<std::shared_ptr<SymbolicVariable>> _components);

private:
	std::vector<std::shared_ptr<SymbolicVariable>> m_components;
};

}
}
}
