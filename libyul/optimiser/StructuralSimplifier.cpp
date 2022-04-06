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
#include <libyul/optimiser/StructuralSimplifier.h>
#include <libyul/AST.h>
#include <libyul/Utilities.h>
#include <libsolutil/CommonData.h>
#include <libsolutil/Visitor.h>

using namespace std;
using namespace solidity;
using namespace solidity::yul;

using OptionalStatements = std::optional<vector<Statement>>;

namespace
{

OptionalStatements replaceConstArgSwitch(Switch& _switchStmt, u256 const& _constExprVal)
{
	Block* matchingCaseBlock = nullptr;
	Case* defaultCase = nullptr;

	for (auto& _case: _switchStmt.cases)
	{
		if (_case.value && valueOfLiteral(*_case.value) == _constExprVal)
		{
			matchingCaseBlock = &_case.body;
			break;
		}
		else if (!_case.value)
			defaultCase = &_case;
	}

	if (!matchingCaseBlock && defaultCase)
		matchingCaseBlock = &defaultCase->body;

	if (matchingCaseBlock)
		return util::make_vector<Statement>(std::move(*matchingCaseBlock));
	else
		return optional<vector<Statement>>{vector<Statement>{}};
}

optional<u256> hasLiteralValue(Expression const& _expression)
{
	if (holds_alternative<Literal>(_expression))
		return valueOfLiteral(std::get<Literal>(_expression));
	else
		return std::optional<u256>();
}

bool expressionAlwaysTrue(Expression const& _expression)
{
	if (std::optional<u256> value = hasLiteralValue(_expression))
		return *value != 0;
	else
		return false;
}

bool expressionAlwaysFalse(Expression const& _expression)
{
	if (std::optional<u256> value = hasLiteralValue(_expression))
		return *value == 0;
	else
		return false;
}

}

void StructuralSimplifier::run(OptimiserStepContext&, Block& _ast)
{
	StructuralSimplifier{}(_ast);
}

void StructuralSimplifier::operator()(Block& _block)
{
	simplify(_block.statements);
}

void StructuralSimplifier::simplify(std::vector<yul::Statement>& _statements)
{
	// Explicit local variables ifLambda, switchLambda, forLoopLambda are created to avoid MSVC C++17 Debug test crash
	// (Run-Time Check Failure #2 - Stack around the variable '....' was corrupted).
	// As soon as the issue is fixed, this workaround can be removed.
	auto ifLambda = [&](If& _ifStmt) -> OptionalStatements
	{
		if (expressionAlwaysTrue(*_ifStmt.condition))
			return {std::move(_ifStmt.body.statements)};
		else if (expressionAlwaysFalse(*_ifStmt.condition))
			return {vector<Statement>{}};
		return {};
	};
	auto switchLambda = [&](Switch& _switchStmt) -> OptionalStatements
	{
		if (std::optional<u256> const constExprVal = hasLiteralValue(*_switchStmt.expression))
			return replaceConstArgSwitch(_switchStmt, constExprVal.value());
		return {};
	};
	auto forLoopLambda = [&](ForLoop& _forLoop) -> OptionalStatements
	{
		if (expressionAlwaysFalse(*_forLoop.condition))
			return {std::move(_forLoop.pre.statements)};
		return {};
	};

	util::GenericVisitor visitor{util::VisitorFallback<OptionalStatements>{}, ifLambda, switchLambda, forLoopLambda};

	util::iterateReplacing(
		_statements,
		[&](Statement& _stmt) -> OptionalStatements
		{
			OptionalStatements result = std::visit(visitor, _stmt);
			if (result)
				simplify(*result);
			else
				visit(_stmt);
			return result;
		}
	);
}
