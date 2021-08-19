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
 * Class that can answer questions about values of variables and their relations.
 */

#include <libyul/optimiser/KnowledgeBase.h>

#include <libyul/AST.h>
#include <libyul/Utilities.h>
#include <libyul/optimiser/SimplificationRules.h>
#include <libyul/optimiser/DataFlowAnalyzer.h>

#include <libsolutil/CommonData.h>

#include <variant>

using namespace std;
using namespace solidity;
using namespace solidity::yul;

bool KnowledgeBase::knownToBeDifferent(YulString _a, YulString _b)
{
	// Try to use the simplification rules together with the
	// current values to turn `sub(_a, _b)` into a nonzero constant.
	// If that fails, try `eq(_a, _b)`.

	if (optional<u256> difference = differenceIfKnownConstant(_a, _b))
		return difference != 0;

	Expression expr2 = simplify(FunctionCall{{}, {{}, "eq"_yulstring}, util::make_vector<Expression>(Identifier{{}, _a}, Identifier{{}, _b})});
	if (holds_alternative<Literal>(expr2))
		return valueOfLiteral(std::get<Literal>(expr2)) == 0;

	return false;
}

optional<u256> KnowledgeBase::differenceIfKnownConstant(YulString _a, YulString _b)
{
	// Try to use the simplification rules together with the
	// current values to turn `sub(_a, _b)` into a constant.

	Expression expr1 = simplify(FunctionCall{{}, {{}, "sub"_yulstring}, util::make_vector<Expression>(Identifier{{}, _a}, Identifier{{}, _b})});
	if (Literal const* value = get_if<Literal>(&expr1))
		return valueOfLiteral(*value);

	return {};
}

bool KnowledgeBase::knownToBeDifferentByAtLeast32(YulString _a, YulString _b)
{
	// Try to use the simplification rules together with the
	// current values to turn `sub(_a, _b)` into a constant whose absolute value is at least 32.

	if (optional<u256> difference = differenceIfKnownConstant(_a, _b))
		return difference >= 32 && difference <= u256(0) - 32;

	return false;
}

bool KnowledgeBase::knownToBeZero(YulString _a)
{
	return valueIfKnownConstant(_a) == u256{};
}

optional<u256> KnowledgeBase::valueIfKnownConstant(YulString _a)
{
	if (m_variableValues.count(_a))
		if (Literal const* literal = get_if<Literal>(m_variableValues.at(_a).value))
			return valueOfLiteral(*literal);
	return {};
}

Expression KnowledgeBase::simplify(Expression _expression)
{
	m_counter = 0;
	return simplifyRecursively(move(_expression));
}

Expression KnowledgeBase::simplifyRecursively(Expression _expression)
{
	if (m_counter++ > 100)
		return _expression;

	if (holds_alternative<FunctionCall>(_expression))
		for (Expression& arg: std::get<FunctionCall>(_expression).arguments)
			arg = simplifyRecursively(arg);

	if (auto match = SimplificationRules::findFirstMatch(_expression, m_dialect, m_variableValues))
		return simplifyRecursively(match->action().toExpression(debugDataOf(_expression)));

	return _expression;
}
