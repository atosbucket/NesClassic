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
 * Class that contains contextual information during IR generation.
 */

#pragma once

#include <libsolidity/interface/OptimiserSettings.h>

#include <libsolidity/codegen/MultiUseYulFunctionCollector.h>

#include <liblangutil/EVMVersion.h>

#include <string>
#include <memory>

namespace dev
{
namespace solidity
{

class VariableDeclaration;

/**
 * Class that contains contextual information during IR generation.
 */
class IRGenerationContext
{
public:
	IRGenerationContext(langutil::EVMVersion _evmVersion, OptimiserSettings _optimiserSettings):
		m_evmVersion(_evmVersion),
		m_optimiserSettings(std::move(_optimiserSettings)),
		m_functions(std::make_shared<MultiUseYulFunctionCollector>())
	{}

	std::shared_ptr<MultiUseYulFunctionCollector> functionCollector() const { return m_functions; }

	std::string addLocalVariable(VariableDeclaration const& _varDecl);

private:
	langutil::EVMVersion m_evmVersion;
	OptimiserSettings m_optimiserSettings;
	std::map<VariableDeclaration const*, std::string> m_localVariables;
	std::shared_ptr<MultiUseYulFunctionCollector> m_functions;
};

}
}
