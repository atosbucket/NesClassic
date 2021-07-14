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
// SPDX-License-Identifier: GPL-3.0
/**
 * @author Christian <c@ethdev.com>
 * @date 2017
 * Converts a parsed assembly into its textual form.
 */

#include <libyul/AsmPrinter.h>
#include <libyul/AST.h>
#include <libyul/Exceptions.h>
#include <libyul/Dialect.h>

#include <libsolutil/CommonData.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <range/v3/view/transform.hpp>

#include <memory>
#include <functional>

using namespace std;
using namespace solidity;
using namespace solidity::util;
using namespace solidity::yul;

//@TODO source locations

string AsmPrinter::operator()(Literal const& _literal) const
{
	switch (_literal.kind)
	{
	case LiteralKind::Number:
		yulAssert(isValidDecimal(_literal.value.str()) || isValidHex(_literal.value.str()), "Invalid number literal");
		return _literal.value.str() + appendTypeName(_literal.type);
	case LiteralKind::Boolean:
		yulAssert(_literal.value == "true"_yulstring || _literal.value == "false"_yulstring, "Invalid bool literal.");
		return ((_literal.value == "true"_yulstring) ? "true" : "false") + appendTypeName(_literal.type, true);
	case LiteralKind::String:
		break;
	}

	return escapeAndQuoteString(_literal.value.str()) + appendTypeName(_literal.type);
}

string AsmPrinter::operator()(Identifier const& _identifier) const
{
	yulAssert(!_identifier.name.empty(), "Invalid identifier.");
	return _identifier.name.str();
}

string AsmPrinter::operator()(ExpressionStatement const& _statement) const
{
	return std::visit(*this, _statement.expression);
}

string AsmPrinter::operator()(Assignment const& _assignment) const
{
	yulAssert(_assignment.variableNames.size() >= 1, "");
	string variables = (*this)(_assignment.variableNames.front());
	for (size_t i = 1; i < _assignment.variableNames.size(); ++i)
		variables += ", " + (*this)(_assignment.variableNames[i]);
	return variables + " := " + std::visit(*this, *_assignment.value);
}

string AsmPrinter::operator()(VariableDeclaration const& _variableDeclaration) const
{
	string out = "let ";
	out += boost::algorithm::join(
		_variableDeclaration.variables | ranges::views::transform(
			[this](TypedName argument) { return formatTypedName(argument); }
		),
		", "
	);
	if (_variableDeclaration.value)
	{
		out += " := ";
		out += std::visit(*this, *_variableDeclaration.value);
	}
	return out;
}

string AsmPrinter::operator()(FunctionDefinition const& _functionDefinition) const
{
	yulAssert(!_functionDefinition.name.empty(), "Invalid function name.");
	string out = "function " + _functionDefinition.name.str() + "(";
	out += boost::algorithm::join(
		_functionDefinition.parameters | ranges::views::transform(
			[this](TypedName argument) { return formatTypedName(argument); }
		),
		", "
	);
	out += ")";
	if (!_functionDefinition.returnVariables.empty())
	{
		out += " -> ";
		out += boost::algorithm::join(
			_functionDefinition.returnVariables | ranges::views::transform(
				[this](TypedName argument) { return formatTypedName(argument); }
			),
			", "
		);
	}

	return out + "\n" + (*this)(_functionDefinition.body);
}

string AsmPrinter::operator()(FunctionCall const& _functionCall) const
{
	return
		(*this)(_functionCall.functionName) + "(" +
		boost::algorithm::join(
			_functionCall.arguments | ranges::views::transform([&](auto&& _node) { return std::visit(*this, _node); }),
			", " ) +
		")";
}

string AsmPrinter::operator()(If const& _if) const
{
	yulAssert(_if.condition, "Invalid if condition.");
	string body = (*this)(_if.body);
	char delim = '\n';
	if (body.find('\n') == string::npos)
		delim = ' ';
	return "if " + std::visit(*this, *_if.condition) + delim + (*this)(_if.body);
}

string AsmPrinter::operator()(Switch const& _switch) const
{
	yulAssert(_switch.expression, "Invalid expression pointer.");
	string out = "switch " + std::visit(*this, *_switch.expression);
	for (auto const& _case: _switch.cases)
	{
		if (!_case.value)
			out += "\ndefault ";
		else
			out += "\ncase " + (*this)(*_case.value) + " ";
		out += (*this)(_case.body);
	}
	return out;
}

string AsmPrinter::operator()(ForLoop const& _forLoop) const
{
	yulAssert(_forLoop.condition, "Invalid for loop condition.");
	string pre = (*this)(_forLoop.pre);
	string condition = std::visit(*this, *_forLoop.condition);
	string post = (*this)(_forLoop.post);
	char delim = '\n';
	if (
		pre.size() + condition.size() + post.size() < 60 &&
		pre.find('\n') == string::npos &&
		post.find('\n') == string::npos
	)
		delim = ' ';
	return
		("for " + move(pre) + delim + move(condition) + delim + move(post) + "\n") +
		(*this)(_forLoop.body);
}

string AsmPrinter::operator()(Break const&) const
{
	return "break";
}

string AsmPrinter::operator()(Continue const&) const
{
	return "continue";
}

string AsmPrinter::operator()(Leave const&) const
{
	return "leave";
}

string AsmPrinter::operator()(Block const& _block) const
{
	if (_block.statements.empty())
		return "{ }";
	string body = boost::algorithm::join(
		_block.statements | ranges::views::transform([&](auto&& _node) { return std::visit(*this, _node); }),
		"\n"
	);
	if (body.size() < 30 && body.find('\n') == string::npos)
		return "{ " + body + " }";
	else
	{
		boost::replace_all(body, "\n", "\n    ");
		return "{\n    " + body + "\n}";
	}
}

string AsmPrinter::formatTypedName(TypedName _variable) const
{
	yulAssert(!_variable.name.empty(), "Invalid variable name.");
	return _variable.name.str() + appendTypeName(_variable.type);
}

string AsmPrinter::appendTypeName(YulString _type, bool _isBoolLiteral) const
{
	if (m_dialect && !_type.empty())
	{
		if (!_isBoolLiteral && _type == m_dialect->defaultType)
			_type = {};
		else if (_isBoolLiteral && _type == m_dialect->boolType && !m_dialect->defaultType.empty())
			// Special case: If we have a bool type but empty default type, do not remove the type.
			_type = {};
	}
	if (_type.empty())
		return {};
	else
		return ":" + _type.str();
}
