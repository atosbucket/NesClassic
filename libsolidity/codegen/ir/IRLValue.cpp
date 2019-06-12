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
/**
 * Generator for code that handles LValues.
 */

#include <libsolidity/codegen/ir/IRLValue.h>

#include <libsolidity/codegen/ir/IRGenerationContext.h>
#include <libsolidity/codegen/YulUtilFunctions.h>
#include <libsolidity/codegen/CompilerUtils.h>
#include <libsolidity/ast/AST.h>

#include <libdevcore/Whiskers.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;

IRLocalVariable::IRLocalVariable(
	IRGenerationContext& _context,
	VariableDeclaration const& _varDecl
):
	IRLValue(_context, _varDecl.annotation().type),
	m_variableName(_context.localVariableName(_varDecl))
{
}

string IRLocalVariable::storeValue(string const& _value, Type const& _type) const
{
	solAssert(_type == *m_type, "Storing different types - not necessarily a problem.");
	return m_variableName + " := " + _value + "\n";
}

string IRLocalVariable::setToZero() const
{
	return storeValue(m_context.utils().zeroValueFunction(*m_type) + "()", *m_type);
}

IRStorageItem::IRStorageItem(
	IRGenerationContext& _context,
	VariableDeclaration const& _varDecl
)
:IRStorageItem(
	_context,
	*_varDecl.annotation().type,
	_context.storageLocationOfVariable(_varDecl)
)
{ }

IRStorageItem::IRStorageItem(
	IRGenerationContext& _context,
	Type const& _type,
	std::pair<u256, unsigned> slot_offset
)
:	IRLValue(_context, &_type),
	m_slot(toCompactHexWithPrefix(slot_offset.first)),
	m_offset(slot_offset.second)
{
}

IRStorageItem::IRStorageItem(
	IRGenerationContext& _context,
	string _slot,
	boost::variant<string, unsigned> _offset,
	Type const& _type
):
	IRLValue(_context, &_type),
	m_slot(move(_slot)),
	m_offset(std::move(_offset))
{
	solAssert(!m_offset.empty(), "");
	solAssert(!m_slot.empty(), "");
}

string IRStorageItem::retrieveValue() const
{
	if (!m_type->isValueType())
		return m_slot;
	solUnimplementedAssert(m_type->category() != Type::Category::Function, "");
	if (m_offset.type() == typeid(string))
		return
			m_context.utils().dynamicReadFromStorage(*m_type, false) +
			"(" +
			m_slot +
			", " +
			boost::get<string>(m_offset) +
			")";
	else if (m_offset.type() == typeid(unsigned))
		return
			m_context.utils().readFromStorage(*m_type, boost::get<unsigned>(m_offset), false) +
			"(" +
			m_slot +
			")";

	solAssert(false, "");
}

string IRStorageItem::storeValue(string const& _value, Type const& _sourceType) const
{
	if (m_type->isValueType())
		solAssert(_sourceType == *m_type, "Different type, but might not be an error.");

	boost::optional<unsigned> offset;

	if (m_offset.type() == typeid(unsigned))
		offset = get<unsigned>(m_offset);

	return
		m_context.utils().updateStorageValueFunction(*m_type, offset) +
		"(" +
		m_slot +
		(m_offset.type() == typeid(string) ?
			(", " + get<string>(m_offset)) :
			""
		) +
		", " +
		_value +
		")\n";
}

string IRStorageItem::setToZero() const
{
	solUnimplemented("Delete for storage location not yet implemented");
}
