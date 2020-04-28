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

n	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <tools/solidityUpgrade/Upgrade070.h>
#include <tools/solidityUpgrade/SourceTransform.h>

using namespace solidity::frontend;
using namespace solidity::tools;

void DotSyntax::endVisit(FunctionCall const& _functionCall)
{
	TypePointer type = _functionCall.annotation().type;
	if (auto const funcType = dynamic_cast<FunctionType const*>(type))
	{
		if (funcType->valueSet())
			m_changes.emplace_back(
				UpgradeChange::Level::Safe,
				_functionCall.location(),
				SourceTransform::valueUpdate(_functionCall.location())
			);

		if (funcType->gasSet())
			m_changes.emplace_back(
				UpgradeChange::Level::Safe,
				_functionCall.location(),
				SourceTransform::gasUpdate(_functionCall.location())
			);
	}
}
